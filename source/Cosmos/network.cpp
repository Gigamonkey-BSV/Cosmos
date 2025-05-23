
#include <Cosmos/network.hpp>
#include <data/io/wait_for_enter.hpp>
#include <mutex>
#include <iomanip>

// we use this mutex to ensure that we don't call the map at the same time.
std::mutex Mutex;

namespace Cosmos {

    std::ostream &operator << (std::ostream &o, monetary_unit u) {
        switch (u) {
            case (USD): return o << "USD";
            default: return o << "(invalid unit)";
        }
    }
/*
    broadcast_error network::broadcast (const bytes &tx) {
        std::lock_guard<std::mutex> lock (Mutex);
        std::cout << "broadcasting tx " << std::endl;

        bool broadcast_whatsonchain;
        bool broadcast_gorilla;
        bool broadcast_pow_co;

        try {
            broadcast_whatsonchain = WhatsOnChain.transaction ().broadcast (tx);
        } catch (net::HTTP::exception ex) {
            std::cout << "exception caught broadcasting whatsonchain." << ex.what () << std::endl;
            broadcast_whatsonchain = false;
        }

        try {
            auto broadcast_result = Gorilla.submit_transaction ({tx});
            std::cout << "GorallaPool broadcast result: " << JSON (broadcast_result) << std::endl;
            broadcast_gorilla = broadcast_result.ReturnResult == MAPI::success;
            if (!broadcast_gorilla) std::cout << "Gorilla broadcast description: " << broadcast_result.ResultDescription << std::endl;
        } catch (net::HTTP::exception ex) {
            std::cout << "exception caught broadcasting gorilla: " << ex.what () << "; response code = " << ex.Response.Status << std::endl;
            broadcast_gorilla = false;
        }

        std::cout << "broadcast results: Whatsonchain = " << std::boolalpha <<
            broadcast_whatsonchain << "; gorilla = "<< broadcast_gorilla << "; pow_co = " << broadcast_pow_co << std::endl;

        // we don't count whatsonchain because that one seems to return false positives a lot.
        return broadcast_gorilla || broadcast_pow_co ? broadcast_error::none : broadcast_error::unknown;
    }*/

    awaitable<broadcast_single_result> network::broadcast (const extended_transaction &tx) {

        std::cout << "attempting to broadcast tx " << tx.id () << std::endl;
        wait_for_enter ();

        ARC::submit_response response;
        try {
            response = co_await TAAL.submit (tx);
        } catch (net::HTTP::exception ex) {
            std::cout << "Could not connect: " << ex.what () << std::endl;
            co_return broadcast_result::ERROR_NETWORK_CONNECTION_FAIL;
        }

        std::cout << "response status: " << response.Status << std::endl;

        if (response.Status == 401) co_return broadcast_result::ERROR_INAUTHENTICATED;

        std::cout << "response body: " << response.Body << std::endl;

        if (response.Status == 200) co_return response.status ();

        if (response.Status == 465 || response.Status == 473)
            co_return broadcast_single_result {broadcast_result::ERROR_INSUFFICIENT_FEE, *response.body ()};

        if (response.Status >= 460)
            co_return broadcast_single_result {broadcast_result::ERROR_INVALID, *response.body ()};

        co_return broadcast_single_result {broadcast_result::ERROR_UNKNOWN, *response.body ()};
    }

    awaitable<broadcast_multiple_result> network::broadcast (list<extended_transaction> txs) {

        std::cout << "attempting to broadcast " << std::endl;
        for (const auto &tx: txs) std::cout << "\t" << tx.id () << std::endl;
        wait_for_enter ();

        ARC::submit_txs_response response;
        try {
            response = co_await TAAL.submit_txs (txs);
        } catch (net::HTTP::exception ex) {
            std::cout << "Could not connect: " << ex.what () << std::endl;
            co_return broadcast_result::ERROR_NETWORK_CONNECTION_FAIL;
        }

        std::cout << "response status: " << response.Status << std::endl;

        if (response.Status == 401) co_return broadcast_result::ERROR_INAUTHENTICATED;

        std::cout << "response body: " << response.Body << std::endl;

        if (response.Status == 200) co_return response.status ();

        if (response.Status == 465 || response.Status == 473)
            co_return broadcast_multiple_result {broadcast_result::ERROR_INSUFFICIENT_FEE, *response.body ()};

        if (response.Status >= 460)
            co_return broadcast_multiple_result {broadcast_result::ERROR_INVALID, *response.body ()};

        co_return broadcast_multiple_result {broadcast_result::ERROR_UNKNOWN, *response.body ()};

    }

    awaitable<maybe<bytes>> network::get_transaction (const Bitcoin::TXID &txid) {
        static map<Bitcoin::TXID, bytes> cache;

        auto known = cache.contains (txid);
        if (known) co_return maybe<bytes> {*known};

        bytes tx = co_await WhatsOnChain.transaction ().get_raw (txid);

        if (tx == bytes {})
            co_return maybe<bytes> {};

        cache = cache.insert (txid, tx);

        co_return tx;
    }

    // transactions by txid
    map<Bitcoin::TXID, bytes> Transaction;

    // script histories by script hash
    map<digest256, list<Bitcoin::TXID>> History;

    awaitable<satoshis_per_byte> network::mining_fee () {
        std::lock_guard<std::mutex> lock (Mutex);
        auto z = co_await Gorilla.get_fee_quote ();

        if (!z.valid ())
            throw exception {} << "invalid fee quote response received: " << string (JSON (z));

        auto j = JSON (z);

        co_return z.Fees["standard"].MiningFee;
    }

    awaitable<double> network::price (monetary_unit, const Bitcoin::timestamp &tm) {

        std::tm time (tm);

        std::stringstream ss;
        ss << time.tm_mday << "-" << (time.tm_mon + 1) << "-" << (time.tm_year + 1900);

        string date = ss.str ();

        auto request = CoinGecko.REST.GET ("/api/v3/coins/bitcoin-cash-sv/history", {
            entry<data::UTF8, data::UTF8> {"date", date },
            entry<data::UTF8, data::UTF8> {"localization", "false" }
        });

        // the rate limitation for this call is hard to understand.
        // If it doesn't work we wait 30 seconds.
        while (true) {

            auto response = co_await CoinGecko (request);
            if (response.Status == net::HTTP::status::ok) {
                JSON info = JSON::parse (response.Body);
                co_return info["market_data"]["current_price"]["usd"];
            }

            net::asio::io_context io {};
            net::asio::steady_timer {io, net::asio::chrono::seconds (30)}.wait ();

        }
    }

    std::ostream &operator << (std::ostream &o, broadcast_result e) {

        switch (e.Error) {
            case (broadcast_result::SUCCESS) : return o << "none";
            case (broadcast_result::ERROR_UNKNOWN) : return o << "unknown";
            case (broadcast_result::ERROR_NETWORK_CONNECTION_FAIL) : return o << "could not connect to the network";
            case (broadcast_result::ERROR_INSUFFICIENT_FEE) : return o << "insufficient fee";
            case (broadcast_result::ERROR_INVALID) : return o << "invalid transaction";
            default: return o << "invalid error";
        }

        return o;
    }
}
