
#include <Cosmos/network.hpp>
#include <mutex>
#include <iomanip>

std::mutex Mutex;

namespace Cosmos {
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

    broadcast_single_result network::broadcast (const extended_transaction &tx) {

        std::cout << "attempting to broadcast tx " << tx.id () << "\n\t" << bytes (tx) << std::endl;
        wait_for_enter ();

        ARC::submit_response response;
        try {
            response = TAAL.submit (tx);
        } catch (net::HTTP::exception ex) {
            std::cout << "Could not connect: " << ex.what () << std::endl;
            return broadcast_error::network_connection_fail;
        }

        std::cout << "response status: " << response.Status << std::endl;

        if (response.Status == 401) broadcast_error::inauthenticated;

        std::cout << "response body: " << response.Body << std::endl;

        if (response.Status == 200) return response.status ();

        if (response.Status == 465 || response.Status == 473) return {broadcast_error::insufficient_fee, *response.body ()};
        if (response.Status >= 460) return {broadcast_error::invalid_transaction, *response.body ()};
        return {broadcast_error::unknown, *response.body ()};
    }

    broadcast_multiple_result network::broadcast (list<extended_transaction> txs) {

        std::cout << "attempting to broadcast " << std::endl;
        for (const auto &tx: txs) std::cout << "\t" << tx.id () << "\n\t" << bytes (tx) << std::endl;
        wait_for_enter ();

        ARC::submit_txs_response response;
        try {
            response = TAAL.submit_txs (txs);
        } catch (net::HTTP::exception ex) {
            std::cout << "Could not connect: " << ex.what () << std::endl;
            return broadcast_error::network_connection_fail;
        }

        std::cout << "response status: " << response.Status << std::endl;

        if (response.Status == 401) return broadcast_error::inauthenticated;

        std::cout << "response body: " << response.Body << std::endl;

        if (response.Status == 200) return response.status ();

        if (response.Status == 465 || response.Status == 473) return {broadcast_error::insufficient_fee, *response.body ()};
        if (response.Status >= 460) return {broadcast_error::invalid_transaction, *response.body ()};
        return {broadcast_error::unknown, *response.body ()};

    }

    bytes network::get_transaction (const Bitcoin::TXID &txid) {
        static map<Bitcoin::TXID, bytes> cache;

        auto known = cache.contains (txid);
        if (known) return *known;

        bytes tx = WhatsOnChain.transaction ().get_raw (txid);

        if (tx != bytes {}) cache = cache.insert (txid, tx);

        return tx;
    }

    // transactions by txid
    map<Bitcoin::TXID, bytes> Transaction;

    // script histories by script hash
    map<digest256, list<Bitcoin::TXID>> History;

    satoshis_per_byte network::mining_fee () {
        std::lock_guard<std::mutex> lock (Mutex);
        auto z = Gorilla.get_fee_quote ();
        if (!z.valid ()) throw exception {} << "invalid fee quote response received: " << string (JSON (z));
        auto j = JSON (z);

        return z.Fees["standard"].MiningFee;
    }

    double network::price (const Bitcoin::timestamp &tm) {

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

            auto response = CoinGecko (request);
            if (response.Status == net::HTTP::status::ok) {
                JSON info = JSON::parse (response.Body);
                return info["market_data"]["current_price"]["usd"];
            }

            net::asio::io_context io {};
            net::asio::steady_timer {io, net::asio::chrono::seconds (30)}.wait ();

        }
    }

    std::ostream &operator << (std::ostream &o, broadcast_error e) {
        switch (e.Error) {
            case (broadcast_error::none) : return o << "none";
            case (broadcast_error::unknown) : return o << "unknown";
            case (broadcast_error::network_connection_fail) : return o << "could not connect to the network";
            case (broadcast_error::insufficient_fee) : return o << "insufficient fee";
            case (broadcast_error::invalid_transaction) : return o << "invalid transaction";
            default: return o << "invalid error";
        }
        return o;
    }
}
