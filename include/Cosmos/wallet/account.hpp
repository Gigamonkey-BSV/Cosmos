#ifndef COSMOS_WALLET_ACCOUNT
#define COSMOS_WALLET_ACCOUNT

#include <Cosmos/wallet/keys.hpp>
#include <Cosmos/wallet/txdb.hpp>

namespace Cosmos {
    struct redeemable : signing {

        // the output to be redeemeed.
        Bitcoin::output Prevout;

        redeemable (): signing {}, Prevout {} {}
        redeemable (const Bitcoin::output &p, list<derivation> d, uint64 ez, const bytes &script_code = {}) :
            signing {d, ez, script_code}, Prevout {p} {}

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

    struct events {

        struct event {
            Bitcoin::txid TXID;
            Bitcoin::timestamp When;

            Bitcoin::satoshi Received;
            Bitcoin::satoshi Spent;
            Bitcoin::satoshi Moved;

            list<ray> Events;
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

        // all events muust be later than Latest.
        events &operator <<= (ordered_list<ray> e);
    };

    // result of a restored wallet
    struct restored {
        ordered_list<ray> History;
        std::map<Bitcoin::outpoint, redeemable> Account;

        // last key used (+1)
        uint32 Last;

        Bitcoin::satoshi value () const {
            return account {Account}.value ();
        }
    };

    restored restore (txdb &TXDB, hd_pubkey pubkey, uint32 max_look_ahead);

    account inline read_account_from_file (const std::string &filename) {
        return account (read_from_file (filename));
    }

}

#endif
