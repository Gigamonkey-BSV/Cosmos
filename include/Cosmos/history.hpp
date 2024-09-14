#ifndef COSMOS_HISTORY
#define COSMOS_HISTORY

#include <Cosmos/wallet/account.hpp>
#include <Cosmos/pay.hpp>

namespace Cosmos {

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
}

#endif
