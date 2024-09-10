#ifndef COSMOS_WALLET_KEYS_SEQUENCE
#define COSMOS_WALLET_KEYS_SEQUENCE

#include <Cosmos/wallet/keys/derivation.hpp>

namespace Cosmos {
    namespace HD = Gigamonkey::HD;

    // Like derivation, derived_pubkey includes a pubkey and a path.
    // However, address_sequence is guaranteed to include only a non-hardened
    // path, and therefore an xpub can always be derived from it.
    struct derived_pubkey {
        // the master pubkey
        HD::BIP_32::pubkey Key;
        // derivation from this pubkey to the address. Must be non-hardened
        HD::BIP_32::path Path;

        // the derived pubkey
        HD::BIP_32::pubkey derive () const {
            return Key.derive (Path);
        }

        bool valid () const;
    };

    struct address_sequence {
        HD::BIP_32::pubkey Key;
        // derivation from this pubkey to the address. Must be non-hardened
        HD::BIP_32::path Path;
        // last element of the derivation.
        uint32 Last;

        address_sequence () : Key {}, Path {}, Last {0} {}
        address_sequence (const HD::BIP_32::pubkey &s, HD::BIP_32::path p, uint32 l = 0) :
            Key {s}, Path {p}, Last {l} {}

        derived_pubkey last () const;
        address_sequence next () const;

        address_sequence sub () const;

        explicit address_sequence (const JSON &);
        explicit operator JSON () const;

        bool valid () const {
            if (!Key.valid ()) return false;
            for (uint32 u : Path) if (HD::BIP_32::hardened (u)) return false;
            return true;
        }

        bool operator == (const address_sequence &x) const;
    };

    std::ostream inline &operator << (std::ostream &o, const address_sequence &a) {
        return o << "{" << a.Key << ", " << a.Path << ", " << a.Last << "}";
    }

    address_sequence inline address_sequence::next () const {
        return address_sequence {this->Key, this->Path, Last + 1};
    }

    bool inline address_sequence::operator == (const address_sequence &x) const {
        return Key == x.Key && Path == x.Path && Last == x.Last;
    }

    derived_pubkey inline address_sequence::last () const {
        return {this->Key, Path << Last};
    }
}

#endif
