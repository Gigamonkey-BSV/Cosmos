#ifndef COSMOS_PAY
#define COSMOS_PAY

#include <Cosmos/wallet/account.hpp>
//#include <Cosmos/wallet/keys/pubkeys.hpp>
#include <gigamonkey/pay/BEEF.hpp>

namespace Cosmos {
    using BEEF = Gigamonkey::BEEF;

    // this type represents in-progress payments, both from and to.
    struct payments {

        // TODO include signature
        struct request {
            Bitcoin::timestamp Created;
            maybe<Bitcoin::timestamp> Expires;
            maybe<Bitcoin::satoshi> Amount;
            maybe<string> Memo;

            explicit request (const JSON &j);
            explicit operator JSON () const;

            request (Bitcoin::timestamp created);
            request ();
        };

        using payment_request = entry<string, request>;

        static payment_request read_payment_request (const JSON &);
        static JSON write_payment_request (const payment_request &);

        // Given a payment request, we need to keep track of
        // how we will redeem the outputs to us once the
        // payment is received.
        struct redeemable {
            payment_request Request;

            signing Redeem;

            redeemable (const payment_request &inv, const signing &d) : Request {inv}, Redeem {d} {};
        };

        struct offer {
            payment_request Request;
            BEEF Transfer;
            list<account_diff> Diff;

            offer (const payment_request &in, const BEEF &tr, const list<account_diff> &d) : Request {in}, Transfer {tr}, Diff {d} {}
        };

        // payment requests that have been made but not fulfilled.
        map<string, redeemable> Requests;

        // payments that have been sent but not accepted.
        map<string, offer> Proposals;

        explicit operator JSON () const;
        explicit payments (const JSON &);

        payments (map<string, redeemable> r, map<string, offer> p) : Requests {r}, Proposals {p} {}

        struct new_request;

        enum class type {
            pubkey,
            address,
            xpub
        };

        static new_request request_payment (type t, const payments &p, const request &x);
    };

    struct payments::new_request {
        payments::payment_request Request;
        payments Payments;      // new payments
    };

    payments::payment_request inline payments::read_payment_request (const JSON &j) {
        if (!j.is_object ()) throw exception {} << "invalid payment request format A";
        for (const auto &[id, inv] : j.items ()) return {id, request (inv)};
        throw exception {} << "invalid payment request format B";
    }

    JSON inline payments::write_payment_request (const payments::payment_request &e) {
        JSON::object_t o;
        o[std::string (e.Key)] = JSON (e.Value);
        return o;
    }

    inline payments::request::request () : request {Bitcoin::timestamp::now ()} {}
    inline payments::request::request (Bitcoin::timestamp created) : Created {created}, Expires {}, Amount {}, Memo {} {}

}

#endif
