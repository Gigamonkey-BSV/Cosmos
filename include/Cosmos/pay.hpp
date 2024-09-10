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

        static entry<string, invoice> read_invoice (const JSON &);
        static JSON write_invoice (const entry<string, invoice> &);

        struct request {
            invoice Invoice;
            derivation Derivation;

            request (const invoice &inv, const derivation &d) : Invoice {inv}, Derivation {d} {};
        };

        struct payment {
            invoice Invoice;
            BEEF Transfer;
            list<account_diff> Diff;

            payment (const invoice &in, const BEEF &tr, const list<account_diff> &d) : Invoice {in}, Transfer {tr}, Diff {d} {}
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

    using payment_request = entry<string, payments::invoice>;

    struct payments::new_request {
        string ID;          // address, pubkey, or xpub
        invoice Invoice;
        payments Payments;  // new payments
        pubkeys Pubkeys;    // new pubkeys
    };

    enum class payment_type {
        pubkey,
        address,
        xpub
    };

    payments::new_request request_payment (payment_type t, const payments &p, const pubkeys &k, const payments::invoice &x);

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
    entry<string, payments::invoice> inline payments::read_invoice (const JSON &j) {
        if (!j.is_object ()) throw exception {} << "invalid invoice format";
        for (const auto &[id, inv] : j.items ()) return {id, payments::invoice (inv)};
        throw exception {} << "invalid invoice format";
    }

    JSON inline payments::write_invoice (const entry<string, invoice> &e) {
        JSON::object_t o;
        o[e.Key] = e.Value;
        return o;
    }
}

#endif
