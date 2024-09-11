
#include <Cosmos/pay.hpp>

namespace Cosmos {

    payments::request::request (const JSON &j) {
        if (!j.is_object ()) throw exception {} << "Invalid payments::request format";

        auto created = j.find ("created");
        auto expires = j.find ("expires");
        auto amount = j.find ("amount");
        auto memo = j.find ("memo");

        if (created == j.end ()) throw exception {} << "invalid payments::request format";
        Created = Bitcoin::timestamp {std::string (*created)};
        if (expires != j.end ()) Expires = Bitcoin::timestamp {std::string (*expires)};
        if (amount != j.end ()) Amount = read_satoshi (*amount);
        if (memo != j.end ()) Memo = std::string (*memo);
    }

    payments::request::operator JSON () const {
        JSON::object_t o;

        o["created"] = string (Created);
        if (bool (Expires)) o["expires"] = string (*Expires);
        if (bool (Amount)) o["amount"] = write (*Amount);
        if (bool (Memo)) o["memo"] = *Memo;

        return o;
    }

    payments::new_request payments::request_payment (payments::type t, const payments &p, const pubkeys &k, const payments::request &x) {
        derived_pubkey d = k.last (k.Receive);
        switch (t) {

            case payments::type::address: {
                entry<Bitcoin::address, signing> addr = pay_to_address_signing (d);
                payments::payment_request pr {addr.Key, x};
                return payments::new_request {pr,
                    payments {p.Requests.insert (addr.Key, payments::redeemable {pr, addr.Value.Derivation[0]}), p.Proposals},
                    k.next (k.Receive)};
            }

            case payments::type::xpub: {
                string ww = string (d.Parent);
                payments::payment_request pr {ww, x};
                return payments::new_request {pr,
                    payments {p.Requests.insert (ww, payments::redeemable {pr, derivation {d.Parent, d.Path}}), p.Proposals},
                    k.next (k.Receive)};
            }

            case payments::type::pubkey: {
                entry<Bitcoin::pubkey, signing> ppk = pay_to_pubkey_signing (d);
                string ww = string (ppk.Key);
                payments::payment_request pr {ww, x};
                return payments::new_request {pr,
                    payments {p.Requests.insert (ww, payments::redeemable {pr, ppk.Value.Derivation[0]}), p.Proposals},
                    k.next (k.Receive)};
            }

            default : throw exception {} << "unknown payment type";
        }
    }

    account_diff read_account_diff (const JSON &j) {
        if (!j.is_object ()) throw "invalid payments offer format";

        account_diff d;
        d.TXID = read_txid (std::string (j["txid"]));

        for (const auto &jj : j["insert"])
            d.Insert = d.Insert.insert (uint32 (jj[0]), redeemable {jj[1]});

        for (const auto &jj : j["remove"])
            d.Remove = d.Remove.insert (read_outpoint (jj));

        return d;
    }

    JSON write_account_diff (const account_diff &d) {
        JSON::array_t insert;
        insert.resize (d.Insert.size ());
        int index = 0;
        for (const auto &e : d.Insert) insert[index++] = JSON::array_t {uint32 (e.Key), JSON (e.Value)};

        JSON::array_t remove;
        remove.resize (d.Remove.size ());
        index = 0;
        for (const Bitcoin::outpoint &op: d.Remove) remove[index++] = write (op);

        JSON::object_t o;
        o["txid"] = write (d.TXID);
        o["insert"] = insert;
        o["remove"] = remove;
        return o;
    }

    JSON write_account_diffs (const list<account_diff> &d) {
        JSON::array_t a;
        a.resize (d.size ());
        int index = 0;
        for (const account_diff &dd : d) a[index++] = write_account_diff (dd);
        return a;
    }

    list<account_diff> read_account_diffs (const JSON &j) {
        if (!j.is_array ()) throw exception {} << "invalid JSON format for account diffs";
        list<account_diff> diffs;
        for (const auto &jj : j) diffs <<= read_account_diff (jj);
        return diffs;
    }

    JSON write_redeemable (const payments::redeemable &r) {
        JSON::object_t o;
        o["request"] = payments::write_payment_request (r.Request);
        o["derivation"] = JSON (r.Derivation);
        return o;
    }

    payments::redeemable inline read_redeemable (const JSON &j) {
        return payments::redeemable {payments::read_payment_request (j["request"]), derivation (j["derivation"])};
    }

    payments::offer read_offer (const JSON &j) {
        if (!j.is_object ()) throw "invalid payments offer format";
        return payments::offer {
            payments::read_payment_request (j["request"]),
            BEEF (*encoding::base64::read (std::string (j["transfer"]))),
            read_account_diffs (j["diff"])};
    }

    JSON write_offer (const payments::offer &p) {
        JSON::object_t o;
        o["request"] = payments::write_payment_request (p.Request);
        o["transfer"] = encoding::base64::write (bytes (p.Transfer));
        o["diff"] = write_account_diffs (p.Diff);
        return o;
    }

    payments::payments (const JSON &j) {
        if (j == nullptr) return;
        if (!j.is_object ()) throw exception {} << "invalid payments JSON format A";

        auto requests = j.find ("requests");
        auto proposals = j.find ("proposals");

        if (requests == j.end () || proposals == j.end ()) throw exception {} << "invalid payments JSON format B";

        if (!requests->is_object () || !proposals->is_object ()) throw exception {} << "invalid payments JSON format C";

        for (const auto &[key, value] : requests->items ()) Requests = Requests.insert (string (key), read_redeemable (value));

        for (const auto &[key, value] : proposals->items ()) Proposals = Proposals.insert (string (key), read_offer (value));

    }

    payments::operator JSON () const {

        JSON::object_t r;
        for (const auto &e : Requests) r[std::string (e.Key)] = write_redeemable (e.Value);

        JSON::object_t p;
        for (const auto &e : Proposals) p[std::string (e.Key)] = write_offer (e.Value);

        JSON::object_t o;
        o["requests"] = r;
        o["proposals"] = p;
        return o;
    }
}
