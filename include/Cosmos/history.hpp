#ifndef COSMOS_HISTORY
#define COSMOS_HISTORY

#include <Cosmos/wallet/account.hpp>
#include <Cosmos/pay.hpp>

namespace Cosmos {

    // This is how we build an account out of individual events.
    struct history {

        // All the events for a single transaction.
        struct tx {
            Bitcoin::TXID TXID {};
            when When {};

            Bitcoin::satoshi Received {};
            Bitcoin::satoshi Spent {};
            Bitcoin::satoshi Moved {};

            events Events {};

            bool operator == (const tx &) const;
            std::partial_ordering operator <=> (const tx &) const;
            tx () {}
        };

        // account of the wallet after going through all the events.
        std::map<Bitcoin::outpoint, Bitcoin::output> Account;

        // events from latest to earliest.
        stack<tx> Events;

        struct payment : payments::payment_request {
            list<Bitcoin::TXID> Transactions;
            payment (const string &id, const payments::request &r, list<Bitcoin::TXID> txs):
                payments::payment_request {id, r}, Transactions {txs} {}
        };

        // list of completed payments and txids associated with them.
        stack<payment> Payments;

        // the latest known timestamp before unconfirmed events.
        when latest_known () const;

        Bitcoin::satoshi Value;
        Bitcoin::satoshi Spent;
        Bitcoin::satoshi Received;

        history () : Account {}, Events {}, Value {0}, Spent {0}, Received {0} {}
        history (std::map<Bitcoin::outpoint, Bitcoin::output> a, stack<tx> e,
            Bitcoin::satoshi v, Bitcoin::satoshi spent, Bitcoin::satoshi received):
            Account {a}, Events {e}, Value {v}, Spent {spent}, Received {received} {}

        // all events must be later than Latest.
        history &operator <<= (events e);

        explicit operator JSON () const;
        explicit history (const JSON &j, TXDB &);

        // a starting point and events following it, enough information
        // to generate a new account at the end of the range.
        struct episode {
            std::map<Bitcoin::outpoint, Bitcoin::output> Account;
            ordered_list<tx> History;
        };

        // get all events within a given range.
        episode get (when from = when::negative_infinity (), when to = when::infinity ()) const;

        // through the history and try to find confirmations for all unconfirmed txs.
        history update (TXDB &) const;
    };
}

#endif
