#include <Cosmos/network/whatsonchain.hpp>

namespace Cosmos {

    std::string inline whatsonchain::write (const Bitcoin::TXID &txid) {
        std::stringstream ss;
        ss << txid;
        return ss.str ().substr (2);
    }

    Bitcoin::TXID inline whatsonchain::read_txid (const JSON &j) {
        return Bitcoin::TXID {std::string {"0x"} + std::string (j)};
    }

    bool whatsonchain::transactions::broadcast (const bytes &tx) {

        auto request = API.REST.POST ("/v1/bsv/main/tx/raw",
            {{net::HTTP::header::content_type, "application/JSON"}},
            JSON {{"tx_hex", encoding::hex::write (tx)}}.dump ());

        auto response = API (request);
            std::cout << "response of broadcasting to whatsonchain: " << response << std::endl;

        if (response.Status >= 500)
            throw net::HTTP::exception {request, response, string {"problem reading txid."}};

        if (response.Status != 200 ||
            response.Headers[net::HTTP::header::content_type] == "text/plain") {
            return false;
        }

        if (response.Body == "") return false;

        return true;
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

    list<whatsonchain::UTXO> whatsonchain::addresses::get_unspent (const Bitcoin::address &addr) {

        std::stringstream ss;
        ss << "/v1/bsv/main/address/" << static_cast<const std::string &> (addr) << "/unspent";
        auto request = API.REST.GET (ss.str ());
        auto response = API (request);

        if (response.Status != net::HTTP::status::ok) {
            std::stringstream z;
            z << "status = \"" << response.Status << "\"; content_type = " <<
                response.Headers[net::HTTP::header::content_type] << "; body = \"" << response.Body << "\"";
            throw net::HTTP::exception {request, response, z.str ()};
        }

        JSON info = JSON::parse (response.Body);

        if (!info.is_array ()) throw response;

        list<UTXO> UTXOs;

        for (const JSON &item : info) {

            UTXO u (item);
            if (!u.valid ()) throw response;

            UTXOs = UTXOs << u;

        }

        return UTXOs;
    }

    list<whatsonchain::UTXO> whatsonchain::scripts::get_unspent (const digest256 &script_hash) {

        string path = string {"/v1/bsv/main/script/"} + write (script_hash) + "/unspent";

        auto request = API.REST.GET (path);
        auto response = API (request);

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

        return UTXOs;
    }

    list<Bitcoin::TXID> whatsonchain::addresses::get_history (const Bitcoin::address& addr) {

        std::stringstream call;
        call << "/v1/bsv/main/address/" + addr + "/history";

        auto request = API.REST.GET (call.str ());
        auto response = API (request);

        if (response.Status != net::HTTP::status::ok)
            throw net::HTTP::exception {request, response, "response status is not ok"};
        /*
        if (response.Headers[networking::HTTP::header::content_type] != "application/JSON")
            throw networking::HTTP::exception{request, response, "response header content_type does not indicate application/JSON"};
        */
        list<Bitcoin::TXID> txids;

        try {

            JSON txids_JSON = JSON::parse (response.Body);

            for (const JSON &item : txids_JSON) txids = txids << read_txid (item ["tx_hash"]);
        } catch (const JSON::exception &exception) {
            throw net::HTTP::exception {request, response, string {"problem reading JSON: "} + string {exception.what ()}};
        }

        return txids;
    }

    list<Bitcoin::TXID> whatsonchain::scripts::get_history (const digest256& script_hash) {
        std::stringstream call;
        call << "/v1/bsv/main/script/" << write (script_hash) << "/history";

        auto request = API.REST.GET (call.str ());
        auto response = API (request);

        if (response.Status != net::HTTP::status::ok)
            throw net::HTTP::exception {request, response, "response status is not ok"};
        /*
        if (response.Headers[networking::HTTP::header::content_type] != "application/JSON")
            throw networking::HTTP::exception{request, response, "response header content_type does not indicate application/JSON"};
        */
        list<Bitcoin::TXID> txids;

        try {

            JSON txids_JSON = JSON::parse (response.Body);

            for (const JSON &item : txids_JSON) txids = txids << read_txid (item ["tx_hash"]);
        } catch (const JSON::exception &exception) {
            throw net::HTTP::exception {request, response, string {"problem reading JSON: "} + string {exception.what ()}};
        }

        return txids;
    }

    bytes whatsonchain::transactions::get_raw (const Bitcoin::TXID &txid) {

        string call = string {"/v1/bsv/main/tx/"} + write (txid) + string {"/hex"};

        auto request = API.REST.GET (call);
        auto response = API (request);

        if (response.Status == 404) return {};

        if (response.Status != net::HTTP::status::ok)
            throw net::HTTP::exception {request, response, "response status is not ok"};

        maybe<bytes> tx = encoding::hex::read (response.Body);

        if (!bool (tx)) return {};
        return *tx;
    }

    JSON whatsonchain::transactions::tx_data (const Bitcoin::TXID &txid) {

        string call = string {"/v1/bsv/main/tx/hash/"} + write (txid);

        auto request = API.REST.GET (call);
        auto response = API (request);

        if (response.Status == 404) return {};

        if (response.Status != net::HTTP::status::ok)
            throw net::HTTP::exception {request, response, "response status is not ok"};

        return JSON::parse (response.Body);
    }

    whatsonchain::merkle_proof whatsonchain::transactions::get_merkle_proof (const Bitcoin::TXID &txid) {
        string call = string {"/v1/bsv/main/tx/"} + write (txid) + "/proof";

        auto request = API.REST.GET (call);
        auto response = API (request);

        if (response.Status == 404) return {};

        if (response.Status != net::HTTP::status::ok) {
            throw net::HTTP::exception {request, response, "response status is not ok"};
        }

        JSON proof = JSON::parse (response.Body)[0];

        whatsonchain::merkle_proof p;
        p.BlockHash = read_txid (proof["blockHash"]);
        p.Proof.Branch.Leaf.Digest = read_txid (proof["hash"]);
        p.Proof.Root = read_txid (proof["merkleRoot"]);

        for (auto i = proof["branches"].rbegin (); i < proof["branches"].rend (); i++) {
            const auto &b = *i;
            p.Proof.Branch.Digests <<= read_txid (b["hash"]);
            p.Proof.Branch.Leaf.Index <<= 1;
            if (std::string (b["pos"]) == "L") p.Proof.Branch.Leaf.Index++;
        }

        return p;

    }

    whatsonchain::header whatsonchain::blocks::get_header (const digest256 &hash) {

        string call = string {"/v1/bsv/main/block/"} + write (hash) + "/header";

        auto request = API.REST.GET (call);
        auto response = API (request);

        if (response.Status == 404) return {};
        if (response.Status == 400) return {};

        if (response.Status != net::HTTP::status::ok)
            throw net::HTTP::exception {request, response, "response status is not ok"};

        JSON h = JSON::parse (response.Body);

        std::string bits = std::string (h["bits"]);

        uint32_big j;
        boost::algorithm::unhex (bits.begin (), bits.end (), j.begin ());
        Bitcoin::target t {uint32_little {j}};

        return header {N {uint32 (h["height"])}, Bitcoin::header {
            int32 (h["version"]), read_txid (h["previousblockhash"]),
            read_txid (h["merkleroot"]), Bitcoin::timestamp {uint32 (h["time"])},
            t, uint32 (h["nonce"])}};

    }

    whatsonchain::header whatsonchain::blocks::get_header (const N &n) {

        string call = string {"/v1/bsv/main/block/"} + encoding::decimal::write (n) + "/header";

        auto request = API.REST.GET (call);
        auto response = API (request);

        if (response.Status == 404) return {};
        if (response.Status == 400) return {};

        if (response.Status != net::HTTP::status::ok)
            throw net::HTTP::exception {request, response, "response status is not ok"};

        JSON h = JSON::parse (response.Body);

        std::string bits = std::string (h["bits"]);

        uint32_big j;
        boost::algorithm::unhex (bits.begin (), bits.end (), j.begin ());
        Bitcoin::target t {uint32_little {j}};

        return header {N {uint32 (h["height"])}, Bitcoin::header {
            int32 (h["version"]), read_txid (h["previousblockhash"]),
            read_txid (h["merkleroot"]), Bitcoin::timestamp {uint32 (h["time"])},
            t, uint32 (h["nonce"])}};

    }
}
