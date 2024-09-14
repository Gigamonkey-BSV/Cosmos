#ifndef COSMOS_WALLET_KEYS_SECRET
#define COSMOS_WALLET_KEYS_SECRET

#include <Cosmos/database/write.hpp>
#include <Cosmos/wallet/keys/derivation.hpp>

namespace Cosmos {

    struct keychain : tool::base_rb_map<pubkey, secret, keychain> {
        using tool::base_rb_map<pubkey, secret, keychain>::base_rb_map;

        explicit keychain (const JSON &);
        operator JSON () const;

        Bitcoin::secret derive (const derivation &) const;

        using tool::base_rb_map<pubkey, secret, keychain>::insert;

        // please don't confuse people by putting in a different pubkey from secret key.
        keychain insert (const secret &x) const {
            return keychain {tool::base_rb_map<pubkey, secret, keychain>::insert (x.to_public (), x)};
        }

        bool valid () const {
            return tool::base_rb_map<pubkey, secret, keychain>::valid () && this->size () > 0;
        }
    };

    keychain inline read_keychain_from_file (const std::string &filename) {
        return keychain (read_from_file (filename));
    }

    Bitcoin::secret inline keychain::derive (const derivation &d) const {
        return Bitcoin::secret (HD::BIP_32::secret {(*this)[d.Parent]}.derive (d.Path));
    }
}

#endif
