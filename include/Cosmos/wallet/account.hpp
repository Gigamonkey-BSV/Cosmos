#ifndef COSMOS_WALLET_ACCOUNT
#define COSMOS_WALLET_ACCOUNT

#include <Cosmos/wallet/keys.hpp>
#include <Cosmos/database/txdb.hpp>

#include <gigamonkey/script/pattern/pay_to_address.hpp>
#include <gigamonkey/script/pattern/pay_to_pubkey.hpp>

namespace Cosmos {

    using pay_to_address = Gigamonkey::pay_to_address;
    using pay_to_pubkey = Gigamonkey::pay_to_pubkey;

    entry<Bitcoin::address, signing> inline pay_to_address_signing (const key_expression &d);

    entry<Bitcoin::pubkey, signing> inline pay_to_pubkey_signing (const key_expression &d);

    // information to redeem an output in the utxo set.
    struct redeemable : signing {

        // the output to be redeemeed.
        Bitcoin::output Prevout;

        redeemable (): signing {}, Prevout {} {}
        redeemable (const Bitcoin::output &p, list<key_expression> d, uint64 ez, const bytes &script_code = {}) :
        signing {d, ez, script_code}, Prevout {p} {}
        redeemable (const Bitcoin::output &p, const signing &x) : signing {x}, Prevout {p} {}

        explicit operator JSON () const;
        redeemable (const JSON &);
    };

    std::ostream &operator << (std::ostream &o, const redeemable &r);

    size_t estimated_size (const redeemable &);

    // the effect of a single transaction on an account.
    struct account_diff {
        Bitcoin::TXID TXID {};
        map<Bitcoin::index, redeemable> Insert {};
        // outpoints that need to be removed and the index of the inpoint that removes them.
        map<Bitcoin::index, Bitcoin::outpoint> Remove {};

        account_diff () {}
        account_diff (const Bitcoin::TXID &txid, map<Bitcoin::index, redeemable> ins, map<Bitcoin::index, Bitcoin::outpoint> ree) :
            TXID {txid}, Insert {ins}, Remove {ree} {}

    };

    struct account : base_map<Bitcoin::outpoint, redeemable, account> {
        using base_map<Bitcoin::outpoint, redeemable, account>::base_map;

        // apply a diff to an account. Throw exception if the diff contains
        // outpoints to be removed that are not in the account.
        static account apply (const account_diff &);

        explicit account (const JSON &);
        explicit operator JSON () const;
        Bitcoin::satoshi value () const {
            Bitcoin::satoshi v {0};
            for (const auto &[_, value] : *this) v += value.Prevout.Value;
            return v;
        }

        account operator + (const account b) const {
            account a = *this;
            for (const auto &[key, value] : b) a = a.insert (key, value);
            return a;
        }

        struct cannot_apply_diff : std::logic_error {
            cannot_apply_diff () : std::logic_error {"Attempt to remove nonexistent outpoints from account"} {}
        };

        account operator << (const account_diff &d) const;

        account &operator += (const account b) {
            return *this = *this + b;
        }

        account &operator <<= (const account_diff &d) {
            return *this = *this << d;
        }

    };
/*
    entry<Bitcoin::address, signing> inline pay_to_address_signing (const key_expression &d) {
        return {d.derive ().address ().encode (),
            signing {{derivation {d.Parent, d.Path}}, pay_to_address::redeem_expected_size ()}};
    }

    entry<Bitcoin::pubkey, signing> inline pay_to_pubkey_signing (const key_expression &d) {
        return {Bitcoin::pubkey {d.derive ().Pubkey},
            signing {{derivation {d.Parent, d.Path}}, pay_to_pubkey::redeem_expected_size ()}};
    }*/

    std::ostream inline &operator << (std::ostream &o, const redeemable &r) {
        return o << "redeemable {" << r.Prevout << ", keys: " << r.Keys << "}";
    }

}

#endif
