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
        set<Bitcoin::outpoint> Remove {};

        account_diff () {}
    };

    struct account {
        std::map<Bitcoin::outpoint, redeemable> Account;

        // apply a diff to an account. Throw exception if the diff contains
        // outpoints to be removed that are not in the account.
        static std::map<Bitcoin::outpoint, redeemable> apply (const account_diff &);

        account () : Account {} {}
        account (std::map<Bitcoin::outpoint, redeemable> a) : Account {a} {}
        explicit account (const JSON &);
        explicit operator JSON () const;
        Bitcoin::satoshi value () const {
            Bitcoin::satoshi v {0};
            for (const auto &[key, value] : Account) v += value.Prevout.Value;
            return v;
        }

        account &operator += (const account b) {
            for (const auto &[key, value] : b.Account) Account[key] = value;
            return *this;
        }

        account &operator <<= (const account_diff &d);

    };

    // This is how we build an account out of individual events.
    struct events {

        // an event corresponds to a single transaction and everything
        // that happens in it which is relevant to our wallet.
        struct event {
            Bitcoin::TXID TXID {};
            Bitcoin::timestamp When {};

            Bitcoin::satoshi Received {};
            Bitcoin::satoshi Spent {};
            Bitcoin::satoshi Moved {};

            ordered_list<ray> Events {};

            bool operator <=> (const event &) const;
            event () {}
        };

        // timestamp of the latest event incorporated into this account.
        Bitcoin::timestamp Latest;
        std::map<Bitcoin::outpoint, Bitcoin::output> Account;
        list<event> Events;

        Bitcoin::satoshi Value;
        Bitcoin::satoshi Spent;
        Bitcoin::satoshi Received;

        events () : Latest {0}, Account {}, Events {}, Value {0}, Spent {0}, Received {0} {}
        events (Bitcoin::timestamp l, std::map<Bitcoin::outpoint, Bitcoin::output> a, list<event> e, Bitcoin::satoshi v):
            Latest {l}, Account {a}, Events {e}, Value {v} {}

        // all events must be later than Latest.
        events &operator <<= (ordered_list<ray> e);

        explicit operator JSON () const;
        explicit events (const JSON &j);

        struct history {
            std::map<Bitcoin::outpoint, Bitcoin::output> Account;
            ordered_list<event> Events;
        };

        // get all events within a given range.
        history get_history (
            math::signed_limit<Bitcoin::timestamp> from = math::signed_limit<Bitcoin::timestamp>::negative_infinity (),
            math::signed_limit<Bitcoin::timestamp> to = math::signed_limit<Bitcoin::timestamp>::infinity ()) const;
    };

    account inline read_account_from_file (const std::string &filename) {
        return account (read_from_file (filename));
    }

}

#endif
