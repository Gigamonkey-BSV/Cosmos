#ifndef COSMOS_WALLET_KEYS_DERIVATION
#define COSMOS_WALLET_KEYS_DERIVATION

#include <gigamonkey/schema/hd.hpp>

namespace Cosmos {
    using namespace data;
    namespace HD = Gigamonkey::HD;
    namespace Bitcoin = Gigamonkey::Bitcoin;
    using digest160 = Gigamonkey::digest160;

    struct key : string {
        using string::string;
        key (const Bitcoin::pubkey &p) : string {string (p)} {}
        key (const HD::BIP_32::pubkey &p) : string {string (p)} {}
        key (const Bitcoin::secret &x) : string {string (x)} {}
        key (const HD::BIP_32::secret &x) : string {string (x)} {}
        digest160 address_hash () const;
    };

    struct pubkey : key {
        using key::key;
        bool valid () const {
            return HD::BIP_32::pubkey {*this}.valid () || Bitcoin::pubkey {*this}.valid ();
        }
    };

    struct secret : key {
        using key::key;
        pubkey to_public () const;
        bool valid () const;
    };

    // derivation path for a key. Includes a name and a path, which may be empty.
    struct derivation {
        // the key to derive from.
        pubkey Parent;

        // the derivation path from the key.
        // if the derivation includes hardened derivations, the private key
        // must be used.
        HD::BIP_32::path Path;

        derivation () : Parent {}, Path {} {}
        derivation (const pubkey &k, HD::BIP_32::path path): Parent {k}, Path {path} {}

        bool valid () const {
            return Parent.valid ();
        }

        explicit operator JSON () const;
        explicit derivation (const JSON &);

        bool operator == (const derivation &d) const {
            return Parent == d.Parent && Path == d.Path;
        }
    };

    std::ostream inline &operator << (std::ostream &o, const derivation &d) {
        return o << "derivation {" << d.Parent << ", " << d.Path << "}";
    }

    bool inline secret::valid () const {
        return Bitcoin::secret {*this}.valid () ? true : HD::BIP_32::secret {*this}.valid ();
    }
}

#endif
