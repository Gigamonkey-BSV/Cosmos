#include <Cosmos/network/whatsonchain.hpp>

namespace Cosmos {

    std::string inline whatsonchain::write (const Bitcoin::TXID &txid) {
        std::stringstream ss;
        ss << txid;
        return ss.str ().substr (2);
    }

    Bitcoin::TXID inline whatsonchain::read_TXID (const JSON &j) {
        return Bitcoin::TXID {std::string {"0x"} + std::string (j)};
    }

    awaitable<bool> whatsonchain::transactions::broadcast (const bytes &tx) {

        net::HTTP::request req = net::HTTP::request (API.REST (net::HTTP::request::make {}.method (net::HTTP::method::post).
            path ("/v1/bsv/main/tx/raw").body (JSON {{"tx_hex", encoding::hex::write (tx)}})));

        auto response = co_await API (req);

        std::cout << "response of broadcasting to whatsonchain: " << response << std::endl;

        if (response.Status >= 500)
            throw net::HTTP::exception {req, response, string {"problem reading txid."}};

        if (response.Status != 200 || (bool (response.content_type ()) && *response.content_type () == "text/plain"))
            co_return false;

        if (response.Body == "") co_return false;

        co_return true;
    }

    whatsonchain::UTXO::UTXO () : Outpoint {}, Value {}, Height {} {}

    whatsonchain::UTXO::UTXO (const JSON &item) : UTXO {} {

        digest256 tx_hash {string {"0x"} + string (item.at ("tx_hash"))};
        if (!tx_hash.valid ()) return;

        Outpoint = Bitcoin::outpoint {tx_hash, uint32 (item.at ("tx_pos"))};
        Value = Bitcoin::satoshi {int64 (item.at ("value"))};
        Height = uint32 (item.at ("height"));

    }

    whatsonchain::UTXO::operator JSON () const {
        std::stringstream ss;
        ss << Outpoint.Digest;
        return JSON {
            {"tx_hash", ss.str ().substr (9, 64)},
            {"tx_pos", Outpoint.Index},
            {"value", int64 (Value)},
            {"height", Height}};
    }

    awaitable<list<whatsonchain::UTXO>> whatsonchain::addresses::get_unspent (const Bitcoin::address &addr) {

        auto request = API.REST.GET
            ((std::stringstream {} << "/v1/bsv/main/address/" << static_cast<const std::string &> (addr) << "/unspent").str ());
        auto response = co_await API (request);

        if (response.Status != net::HTTP::status::ok) {
            std::stringstream z;
            z << "status = \"" << response.Status << "\"; ";
            if (response.content_type ()) z << "content-type = " << *response.content_type () << "; ";
            z << "body = \"" << response.Body << "\"";
            throw net::HTTP::exception {request, response, z.str ()};
        }

        JSON info = JSON::parse (response.Body);

        if (!info.is_array ()) throw net::HTTP::exception {request, response, "expect array"};

        list<UTXO> UTXOs;

        for (const JSON &item : info) {

            UTXO u (item);
            if (!u.valid ()) throw response;

            UTXOs = UTXOs << u;

        }

        co_return UTXOs;
    }

    awaitable<list<whatsonchain::UTXO>> whatsonchain::scripts::get_unspent (const digest256 &script_hash) {

        auto request = API.REST.GET ((std::stringstream {} << "/v1/bsv/main/script/" << write (script_hash) << "/unspent").str ());
        auto response = co_await API (request);

        if (response.Status != net::HTTP::status::ok)
            throw net::HTTP::exception {request, response, string {"response status is not ok. body is: "} + response.Body};
        /*
        if (response.Headers[networking::HTTP::header::content_type] != "application/JSON")
            throw networking::HTTP::exception{request, response, "response header content_type does not indicate application/JSON"};
        */
        list<UTXO> UTXOs;

        try {

            JSON unspent_UTXOs_JSON = JSON::parse (response.Body);

            for (const JSON &item : unspent_UTXOs_JSON) {

                UTXO u (item);
                if (!u.valid ()) throw response;

                UTXOs <<= u;

            }
        } catch (const JSON::exception &exception) {
            throw net::HTTP::exception {request, response, string {"problem reading JSON: "} + string {exception.what()}};
        }

        co_return UTXOs;
    }

    awaitable<list<Bitcoin::TXID>> whatsonchain::addresses::get_history (const Bitcoin::address& addr) {

        auto request = API.REST.GET ((std::stringstream {} << "/v1/bsv/main/address/" + addr + "/history").str ());
        auto response = co_await API (request);

        if (response.Status != net::HTTP::status::ok)
            throw net::HTTP::exception {request, response, "response status is not ok"};
        /*
        if (response.Headers[networking::HTTP::header::content_type] != "application/JSON")
            throw networking::HTTP::exception{request, response, "response header content_type does not indicate application/JSON"};
        */
        list<Bitcoin::TXID> txids;

        try {

            JSON txids_JSON = JSON::parse (response.Body);

            for (const JSON &item : txids_JSON) txids = txids << read_TXID (item ["tx_hash"]);
        } catch (const JSON::exception &exception) {
            throw net::HTTP::exception {request, response, string {"problem reading JSON: "} + string {exception.what ()}};
        }

        co_return txids;
    }

    awaitable<list<Bitcoin::TXID>> whatsonchain::scripts::get_history (const digest256 &script_hash) {

        auto request = API.REST.GET ((std::stringstream {} << "/v1/bsv/main/script/" << write (script_hash) << "/history").str ());
        auto response = co_await API (request);

        if (response.Status != net::HTTP::status::ok)
            throw net::HTTP::exception {request, response, "response status is not ok"};
        /*
        if (response.Headers[networking::HTTP::header::content_type] != "application/JSON")
            throw networking::HTTP::exception{request, response, "response header content_type does not indicate application/JSON"};
        */
        list<Bitcoin::TXID> txids;

        try {

            JSON txids_JSON = JSON::parse (response.Body);

            for (const JSON &item : txids_JSON) txids = txids << read_TXID (item ["tx_hash"]);
        } catch (const JSON::exception &exception) {
            throw net::HTTP::exception {request, response, string {"problem reading JSON: "} + string {exception.what ()}};
        }

        co_return txids;
    }

    awaitable<bytes> whatsonchain::transactions::get_raw (const Bitcoin::TXID &txid) {

        auto request = API.REST.GET ((std::stringstream {} << "/v1/bsv/main/tx/" << write (txid) << "/hex").str ());
        auto response = co_await API (request);

        if (response.Status == 404) co_return bytes {};

        if (response.Status != net::HTTP::status::ok)
            throw net::HTTP::exception {request, response, "response status is not ok"};

        maybe<bytes> tx = encoding::hex::read (response.Body);

        if (!bool (tx)) co_return bytes {};
        co_return *tx;
    }

    awaitable<JSON> whatsonchain::transactions::tx_data (const Bitcoin::TXID &txid) {

        auto request = API.REST.GET ((std::stringstream {} << "/v1/bsv/main/tx/hash/" << write (txid)).str ());
        auto response = co_await API (request);

        if (response.Status == 404) co_return JSON {};

        if (response.Status != net::HTTP::status::ok)
            throw net::HTTP::exception {request, response, "response status is not ok"};

        co_return JSON::parse (response.Body);
    }

    awaitable<maybe<whatsonchain::merkle_proof>> whatsonchain::transactions::get_merkle_proof (const Bitcoin::TXID &txid) {
        using result = maybe<whatsonchain::merkle_proof>;

        auto request = API.REST.GET ((std::stringstream {} << "/v1/bsv/main/tx/" << write (txid) << "/proof").str ());
        auto response = co_await API (request);

        if (response.Status == 404) co_return result {};

        if (response.Status != net::HTTP::status::ok) {
            throw net::HTTP::exception {request, response, "response status is not ok"};
        }

        JSON proof = JSON::parse (response.Body)[0];

        if (proof == JSON (nullptr)) co_return result {};

        whatsonchain::merkle_proof p;
        p.BlockHash = read_TXID (proof["blockHash"]);
        p.Proof.Branch.Leaf.Digest = read_TXID (proof["hash"]);
        p.Proof.Root = read_TXID (proof["merkleRoot"]);

        for (auto i = proof["branches"].rbegin (); i < proof["branches"].rend (); i++) {
            const auto &b = *i;
            p.Proof.Branch.Digests <<= read_TXID (b["hash"]);
            p.Proof.Branch.Leaf.Index <<= 1;
            if (std::string (b["pos"]) == "L") p.Proof.Branch.Leaf.Index++;
        }

        co_return result {p};

    }

    awaitable<whatsonchain::header> whatsonchain::blocks::get_header_by_hash (const digest256 &hash) {

        auto request = API.REST.GET ((std::stringstream {} << "/v1/bsv/main/block/" << write (hash) << "/header").str ());
        auto response = co_await API (request);

        if (response.Status == 404) co_return header {};
        if (response.Status == 400) co_return header {};

        if (response.Status != net::HTTP::status::ok)
            throw net::HTTP::exception {request, response, "response status is not ok"};

        JSON h = JSON::parse (response.Body);

        std::string bits = std::string (h["bits"]);

        uint32_big j;
        boost::algorithm::unhex (bits.begin (), bits.end (), j.begin ());
        Bitcoin::target t {uint32_little {j}};

        co_return header {N {uint32 (h["height"])}, Bitcoin::header {
            int32 (h["version"]), read_TXID (h["previousblockhash"]),
            read_TXID (h["merkleroot"]), Bitcoin::timestamp {uint32 (h["time"])},
            t, uint32 (h["nonce"])}};

    }

    awaitable<whatsonchain::header> whatsonchain::blocks::get_header_by_height (const N &n) {

        auto request = API.REST.GET ((std::stringstream {} << "/v1/bsv/main/block/" << encoding::decimal::write (n) << "/header").str ());
        auto response = co_await API (request);

        if (response.Status == 404) co_return header {};
        if (response.Status == 400) co_return header {};

        if (response.Status != net::HTTP::status::ok)
            throw net::HTTP::exception {request, response, "response status is not ok"};

        JSON h = JSON::parse (response.Body);

        std::string bits = std::string (h["bits"]);

        uint32_big j;
        boost::algorithm::unhex (bits.begin (), bits.end (), j.begin ());
        Bitcoin::target t {uint32_little {j}};

        co_return header {N {uint32 (h["height"])}, Bitcoin::header {
            int32 (h["version"]), read_TXID (h["previousblockhash"]),
            read_TXID (h["merkleroot"]), Bitcoin::timestamp {uint32 (h["time"])},
            t, uint32 (h["nonce"])}};

    }
}
