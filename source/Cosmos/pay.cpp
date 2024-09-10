
#include <Cosmos/pay.hpp>

namespace Cosmos {

    payments::new_request request_payment (payment_type t, const payments &p, const pubkeys &k, const payments::invoice &x) {
        derived_pubkey d = k.last (k.Receive);
        switch (t) {
            case payment_type::address: {
                entry<Bitcoin::address, signing> addr = pay_to_address_signing (d);
                return payments::new_request {
                    addr.Key, x,
                    payments {p.Requests.insert (addr.Key, payments::request {x, addr.Value.Derivation[0]}), p.Proposals},
                    k.next (k.Receive)};
            }
            case payment_type::xpub: {
                string ww = string (d.Key);
                return payments::new_request {
                    ww, x,
                    payments {p.Requests.insert (ww, payments::request {x, derivation {d.Key, d.Path}}), p.Proposals},
                    k.next (k.Receive)};
            }
            case payment_type::pubkey: {
                entry<Bitcoin::pubkey, signing> ppk = pay_to_pubkey_signing (d);
                string ww = string (ppk.Key);
                return payments::new_request {
                    ww, x,
                    payments {p.Requests.insert (ww, payments::request {x, ppk.Value.Derivation[0]}), p.Proposals},
                    k.next (k.Receive)};
            }
            default : throw exception {} << "unknown payment type";
        }
    }

    account_diff read_account_diff (const JSON &j) {
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

    JSON write_request (const payments::request &r) {
        JSON::object_t o;
        o["invoice"] = r.Invoice;
        o["derivation"] = JSON (r.Derivation);
        return o;
    }

    payments::request inline read_request (const JSON &j) {
        return payments::request {j["invoice"], derivation (j["derivation"])};
    }

    payments::payment read_payment (const JSON &j) {
        return payments::payment {
            payments::invoice (j["invoice"]),
            BEEF (*encoding::base64::read (std::string (j["transfer"]))),
            read_account_diffs (j["diff"])};
    }

    JSON write_payment (const payments::payment &p) {
        JSON::object_t o;
        o["invoice"] = p.Invoice;
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

        for (const auto &[key, value] : requests->items ()) Requests = Requests.insert (string (key), read_request (value));

        for (const auto &[key, value] : proposals->items ()) Proposals = Proposals.insert (string (key), read_payment (value));

    }

    payments::operator JSON () const {

        JSON::object_t r;
        for (const auto &e : Requests) r[std::string (e.Key)] = write_request (e.Value);

        JSON::object_t p;
        for (const auto &e : Proposals) p[std::string (e.Key)] = write_payment (e.Value);

        JSON::object_t o;
        o["requests"] = r;
        o["proposals"] = p;
        return o;
    }
}