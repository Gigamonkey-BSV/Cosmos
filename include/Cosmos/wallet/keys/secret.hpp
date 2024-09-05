#ifndef COSMOS_WALLET_KEYS_SECRET
#define COSMOS_WALLET_KEYS_SECRET

#include <Cosmos/database/write.hpp>
#include <Cosmos/wallet/keys/derivation.hpp>

namespace Cosmos {

    struct keychain {
        map<pubkey, secret> Keys;

        keychain (): Keys {} {}

        explicit keychain (map<pubkey, secret> keys): Keys {keys} {}

        explicit keychain (const JSON &);
        operator JSON () const;

        Bitcoin::secret derive (const derivation &) const;

        keychain insert (const secret &x) const {
            return insert (x.to_public (), x);
        }

        // please don't confuse people by putting in a different pubkey from secret key.
        keychain insert (const pubkey &p, const secret &x) const {
            return keychain {Keys.insert (p, x, [] (const secret &o, const secret &n) {
                if (o != n) throw exception {} << "Different key in map under the same name!";
                return o;
            })};
        }

        bool valid () const {
            return data::size (Keys) > 0;
        }
    };

    keychain inline read_keychain_from_file (const std::string &filename) {
        return keychain (read_from_file (filename));
    }

    Bitcoin::secret inline keychain::derive (const derivation &d) const {
        return Bitcoin::secret (HD::BIP_32::secret {Keys[d.Key]}.derive (d.Path));
    }
}

#endif
