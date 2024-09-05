
#include <Cosmos/wallet/keys/secret.hpp>

namespace Cosmos {

    keychain::keychain (const JSON &j) {
        data::map<pubkey, secret> db {};

        if (j == JSON (nullptr)) return;

        if (j != JSON (nullptr)) for (const auto &[key, value] : j.items ())
            db = db.insert (key, secret {std::string (value)});

        *this = keychain {db};
    }

    keychain::operator JSON () const {
        JSON::object_t db {};
        for (const data::entry<pubkey, secret> &e : Keys)
            db [e.Key] = e.Value;
        return db;
    }
}

