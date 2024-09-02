
#include <Cosmos/wallet/keys/chain.hpp>

namespace Cosmos {

    pubkeychain::pubkeychain (const JSON &j) {
        if (j == JSON (nullptr)) return;

        data::map<pubkey, derivation> db {};

        for (const auto &[key, value] : j["pubkeys"].items ())
            db = db.insert (pubkey {key}, derivation {value});

        data::map<string, address_sequence> x {};

        for (const auto &[key, value] : j["sequences"].items ())
            x = x.insert (key, address_sequence {value});

        *this = pubkeychain {db, x, j["receive"], j["change"]};
    }

    pubkeychain::operator JSON () const {
        JSON::object_t db {};
        for (const data::entry<pubkey, derivation> &e : Derivations)
            db [e.Key] = JSON (e.Value);

        JSON::object_t x {};
        for (const data::entry<string, address_sequence> &e : Sequences)
            db [e.Key] = JSON (e.Value);

        JSON::object_t j;
        j["pubkeys"] = db;
        j["sequences"] = x;
        j["receive"] = Receive;
        j["change"] = Change;
        return j;
    }
}
