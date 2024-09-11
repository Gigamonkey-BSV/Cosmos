#ifndef COSMOS_WALLET_KEYS_REDEEMER
#define COSMOS_WALLET_KEYS_REDEEMER

#include <Cosmos/wallet/keys/sequence.hpp>
#include <gigamonkey/script/pattern/pay_to_address.hpp>
#include <gigamonkey/script/pattern/pay_to_pubkey.hpp>

namespace Cosmos {
    using pay_to_address = Gigamonkey::pay_to_address;
    using pay_to_pubkey = Gigamonkey::pay_to_pubkey;

    // information required to make signatures to redeem an output.
    struct signing {

        // derivation paths to required keys. Need to find secret keys
        // corresponding to the given pubkeys in order to redeem it.
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

    entry<Bitcoin::address, signing> inline pay_to_address_signing (const derived_pubkey &d);

    entry<Bitcoin::pubkey, signing> inline pay_to_pubkey_signing (const derived_pubkey &d);

    // information to redeem an output in the utxo set.
    struct redeemable : signing {

        // the output to be redeemeed.
        Bitcoin::output Prevout;

        redeemable (): signing {}, Prevout {} {}
        redeemable (const Bitcoin::output &p, list<derivation> d, uint64 ez, const bytes &script_code = {}) :
            signing {d, ez, script_code}, Prevout {p} {}
        redeemable (const Bitcoin::output &p, const signing &x) : signing {x}, Prevout {p} {}

        explicit operator JSON () const;
        redeemable (const JSON &);

        bool operator == (const redeemable &d) const {
            return Prevout == d.Prevout && this->Derivation == d.Derivation && this->UnlockScriptSoFar == d.UnlockScriptSoFar;
        }
    };

    std::ostream &operator << (std::ostream &o, const redeemable &r);

    size_t estimated_size (const redeemable &);

    entry<Bitcoin::address, signing> inline pay_to_address_signing (const derived_pubkey &d) {
        return {d.derive ().address ().encode (),
            signing {{derivation {d.Parent, d.Path}}, pay_to_address::redeem_expected_size ()}};
    }

    entry<Bitcoin::pubkey, signing> inline pay_to_pubkey_signing (const derived_pubkey &d) {
        return {Bitcoin::pubkey {d.derive ().Pubkey},
            signing {{derivation {d.Parent, d.Path}}, pay_to_pubkey::redeem_expected_size ()}};
    }

    std::ostream inline &operator << (std::ostream &o, const redeemable &r) {
        return o << "redeemable {" << r.Prevout << ", derivation: " << r.Derivation << "}";
    }
}

#endif
