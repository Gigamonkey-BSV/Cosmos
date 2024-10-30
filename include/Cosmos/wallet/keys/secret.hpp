#ifndef COSMOS_WALLET_KEYS_SECRET
#define COSMOS_WALLET_KEYS_SECRET

#include <Cosmos/database/write.hpp>
#include <Cosmos/wallet/keys/derivation.hpp>

namespace Cosmos {

    struct keychain : base_map<pubkey, secret, keychain> {
        using base_map<pubkey, secret, keychain>::base_map;

        explicit keychain (const JSON &);
        operator JSON () const;

        Bitcoin::secret derive (const derivation &) const;

        using base_map<pubkey, secret, keychain>::insert;

        // please don't confuse people by putting in a different pubkey from secret key.
        keychain insert (const secret &x) const {
            return keychain {base_map<pubkey, secret, keychain>::insert (x.to_public (), x)};
        }

        bool valid () const {
            return base_map<pubkey, secret, keychain>::valid () && this->size () > 0;
        }
    };

    Bitcoin::secret inline keychain::derive (const derivation &d) const {
        return Bitcoin::secret (HD::BIP_32::secret {(*this)[d.Parent]}.derive (d.Path));
    }
}

#endif
