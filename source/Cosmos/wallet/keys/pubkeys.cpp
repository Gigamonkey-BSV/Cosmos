
#include <Cosmos/wallet/keys/pubkeys.hpp>

namespace Cosmos {

    pubkeys::pubkeys (const JSON &j) {
        if (j == JSON (nullptr)) return;

        if (!j.is_object ()) throw exception {} << "invalid JSON pubkeys format";

        data::map<pubkey, derivation> db {};

        for (const auto &[key, value] : j.items ())
            db = db.insert (pubkey {key}, derivation {value});

        *this = db;
    }

    addresses::addresses (const JSON &j) {
        if (j == JSON (nullptr)) return;

        if (!j.is_object () || !j.contains ("sequences") || !j.contains ("receive") || !j.contains ("change"))
            throw exception {} << "invalid JSON addresses format";

        data::map<string, address_sequence> x {};

        for (const auto &[key, value] : j["sequences"].items ())
            x = x.insert (key, address_sequence {value});

        *this = addresses {x, j["receive"], j["change"]};
    }

    pubkeys::operator JSON () const {
        JSON::object_t db {};
        for (const auto &[key, val] : *this)
            db [key] = JSON (val);
        return db;
    }

    addresses::operator JSON () const {

        JSON::object_t x {};
        for (const auto &[key, val] : Sequences)
            x [key] = JSON (val);

        JSON::object_t j;
        j["sequences"] = x;
        j["receive"] = Receive;
        j["change"] = Change;
        return j;
    }
}
