#ifndef COSMOS_WALLET_KEYS_SEQUENCE
#define COSMOS_WALLET_KEYS_SEQUENCE

#include <Cosmos/wallet/keys/derivation.hpp>
#include <gigamonkey/script/pattern/pay_to_address.hpp>

namespace Cosmos {
    namespace HD = Gigamonkey::HD;
    using pay_to_address = Gigamonkey::pay_to_address;

    // information required to make signatures to redeem an output.
    struct signing {

        // derivation paths to required keys.
        list<derivation> Derivation;

        // expected size of the completed input script.
        uint64 ExpectedScriptSize;

        // we may need a partially completed script.
        bytes UnlockScriptSoFar;

        signing () : Derivation {}, ExpectedScriptSize {}, UnlockScriptSoFar {} {}
        signing (list<derivation> d, uint64 ez, const bytes &script_code = {}) :
            Derivation {d}, ExpectedScriptSize {ez}, UnlockScriptSoFar {script_code} {}

        uint64 expected_input_size () const {
            return ExpectedScriptSize + Bitcoin::var_int::size (ExpectedScriptSize) + 40;
        }
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

        address_sequence next () const;

        // TODO: this should be an xpub rather than an address.
        entry<Bitcoin::address, signing> last () const;

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

    entry<Bitcoin::address, signing> inline address_sequence::last () const {
        auto path = Path << Last;
        return entry<Bitcoin::address, signing> {
            Bitcoin::address::decoded {Bitcoin::address::main, this->Key.derive (path).address ().Digest}.encode (),
            signing {{derivation {Key, path}}, pay_to_address::redeem_expected_size ()}};
    }

    bool inline address_sequence::operator == (const address_sequence &x) const {
        return Key == x.Key && Path == x.Path && Last == x.Last;
    }
}

#endif
