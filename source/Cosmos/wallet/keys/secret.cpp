
#include <Cosmos/wallet/keys/secret.hpp>

namespace Cosmos {

    pubkey secret::to_public () const {
        if (Bitcoin::secret x {*this}; x.valid ()) return x.to_public ();
        if (HD::BIP_32::secret x {*this}; x.valid ()) return x.to_public ();
        throw exception {"invalid secret key"};
    };

    keychain::keychain (const JSON &j) {
        data::map<pubkey, secret> db {};

        if (j == JSON (nullptr)) return;

        if (j != JSON (nullptr)) for (const auto &[key, value] : j.items ())
            db = db.insert (key, secret {std::string (value)});

        *this = keychain {db};
    }

    keychain::operator JSON () const {
        JSON::object_t db {};
        for (const auto &[key, val] : *this)
            db [key] = val;
        return db;
    }
}

