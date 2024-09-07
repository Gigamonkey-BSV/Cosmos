
#include <Cosmos/wallet/keys/chain.hpp>

namespace Cosmos {

    pubkeychain::pubkeychain (const JSON &j) {
        if (j == JSON (nullptr)) return;

        if (!j.is_object () || !j.contains ("pubkeys") || !j.contains ("sequences") || !j.contains ("receive") || !j.contains ("change"))
            throw exception {} << "invalid JSON format";

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
            x [e.Key] = JSON (e.Value);

        JSON::object_t j;
        j["pubkeys"] = db;
        j["sequences"] = x;
        j["receive"] = Receive;
        j["change"] = Change;
        return j;
    }
}
