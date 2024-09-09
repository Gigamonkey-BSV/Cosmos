#ifndef COSMOS_PAY
#define COSMOS_PAY

#include <Cosmos/wallet/account.hpp>
#include <Cosmos/wallet/keys/pubkeys.hpp>
#include <gigamonkey/pay/BEEF.hpp>

namespace Cosmos {
    using BEEF = Gigamonkey::BEEF;

    // this type represents in-progress payments, both from and to.
    struct payments {

        struct invoice : JSON {
            Bitcoin::timestamp created ();
            maybe<Bitcoin::timestamp> expires ();
            maybe<Bitcoin::satoshi> amount ();
            maybe<string> memo ();

            invoice (const JSON &j) : JSON {j} {};
            invoice (Bitcoin::timestamp created) : JSON {} {
                (*this)["created"] = std::string (string (created));
            }

            invoice &set_expiration (Bitcoin::timestamp);
            invoice &set_amount (Bitcoin::satoshi);
            invoice &set_memo (const string &);
        };

        struct request {
            invoice Invoice;
            derivation Derivation;

            request (const invoice &inv, const derivation &d) : Invoice {inv}, Derivation {d} {};
        };

        struct payment {
            request Request;
            BEEF Transfer;
            account_diff Diff;

            payment (const request &req, const BEEF &tr, const account_diff &d) : Request {req}, Transfer {tr}, Diff {d} {}
        };

        // payment requests that have been made but not fulfilled.
        map<string, request> Requests;

        // payments that have been sent but not accepted.
        map<string, payment> Proposals;

        explicit operator JSON () const;
        explicit payments (const JSON &);

        payments (map<string, request> r, map<string, payment> p) : Requests {r}, Proposals {p} {}

        struct new_request;

    };

    struct payments::new_request {
        string ID;          // address, pubkey, or xpub
        invoice Invoice;
        payments Payments;  // new payments
        pubkeys Pubkeys;    // new pubkeys
    };

    payments::new_request inline request_payment (const payments &p, const pubkeys &k, const payments::invoice &x) {
        auto addr = k.last (k.Receive);
        return payments::new_request {
            addr.Key, x,
            payments {p.Requests.insert (addr.Key, payments::request {x, addr.Value.Derivation[0]}), p.Proposals},
            k.next (k.Receive)
        };
    }

    maybe<Bitcoin::timestamp> inline payments::invoice::expires () {
        if (this->contains ("expires")) return Bitcoin::timestamp {std::string ((*this)["expires"])};
        return {};
    }

    maybe<Bitcoin::satoshi> inline payments::invoice::amount () {
        if (this->contains ("amount")) return read_satoshi ((*this)["amount"]);
        return {};
    }

    maybe<string> inline payments::invoice::memo () {
        if (this->contains ("memo")) return std::string ((*this)["memo"]);
        return {};
    }

    payments::invoice inline &payments::invoice::set_expiration (Bitcoin::timestamp x) {
        (*this)["expires"] = std::string (string (x));
        return *this;
    }

    payments::invoice inline &payments::invoice::set_amount (Bitcoin::satoshi sats) {
        (*this)["amount"] = write (sats);
        return *this;
    }

    payments::invoice inline &payments::invoice::set_memo (const string &x) {
        (*this)["memo"] = std::string (x);
        return *this;
    }
}

#endif
