#ifndef COSMOS_WALLET_ACCOUNT
#define COSMOS_WALLET_ACCOUNT

#include <data/math/infinite.hpp>
#include <Cosmos/wallet/keys/redeemer.hpp>
#include <Cosmos/database/txdb.hpp>

namespace Cosmos {

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
            for (const auto &[key, value] : *this) v += value.Prevout.Value;
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

    account inline read_account_from_file (const std::string &filename) {
        return account (read_from_file (filename).Payload);
    }

}

#endif
