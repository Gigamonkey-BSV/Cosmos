#ifndef COSMOS_WALLET_ACCOUNT
#define COSMOS_WALLET_ACCOUNT

#include <data/math/infinite.hpp>
#include <Cosmos/wallet/keys/sequence.hpp>
#include <Cosmos/wallet/txdb.hpp>

namespace Cosmos {
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

    std::ostream inline &operator << (std::ostream &o, const redeemable &r) {
        return o << "redeemable {" << r.Prevout << ", derivation: " << r.Derivation << "}";
    }

    size_t estimated_size (const redeemable &);

    struct account {
        std::map<Bitcoin::outpoint, redeemable> Account;

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

        events () : Latest {}, Account {}, Events {}, Value {0}, Spent {0}, Received {0} {}
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
