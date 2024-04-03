#include <Cosmos/wallet/txdb.hpp>
#include <Cosmos/wallet/write.hpp>
#include <gigamonkey/script/pattern/pay_to_address.hpp>
#include <filesystem>
#include <fstream>

namespace Cosmos {

    std::string inline write_header (const byte_array<80> &h) {
        return encoding::hex::write (h);
    }

    byte_array<80> inline read_header (const string &j) {
        if (j.size () != 160) throw exception {} << "invalid header size for " << j;
        byte_array<80> p;
        boost::algorithm::unhex (j.begin (), j.end (), p.begin ());
        return p;
    }

    processed::operator JSON () const {
        JSON::object_t o;
        o["tx"] = encoding::hex::write (Transaction);
        if (Proof != nullptr) {
            o["block_hash"] = write (Proof->BlockHash);
            JSON::array_t a;
            for (const digest256 &d : Proof->Path.Digests) a.push_back (write (d));
            o["path"] = a;
            o["index"] = Proof->Path.Index;
        }

        return o;
    }

    processed::processed (const JSON &j) {
        Transaction = *encoding::hex::read (std::string (j["tx"]));
        if (j.contains ("index")) {
            Merkle::path path;
            digest256 hash;
            path.Index = uint32 (j["index"]);
            for (const JSON &a : j["path"]) path.Digests <<= read_txid (std::string (a));
            hash = read_txid (std::string (j["block_hash"]));
            Proof = new merkle_proof {path, hash};
        }
    }

    local_txdb::local_txdb (const JSON &j): local_txdb {} {
        if (j == JSON (nullptr)) return;

        for (const auto &[key, value] : j["transactions"].items ())
            Transactions[read_txid (key)] = processed (value);

        for (const auto &[key, value] : j["headers"].items ())
            Headers[read_txid (key)] = read_header (value);

        for (const auto &[key, value] : j["redemptions"].items ())
            RedeemIndex[read_outpoint (std::string (key))] = inpoint {read_outpoint (std::string (value))};

        for (const auto &[key, value] : j["scripts"].items ()) {
            list <Bitcoin::outpoint> op;
            for (const JSON &j : value) op = append (op, read_outpoint (std::string (j)));
            ScriptIndex[read_txid (key)] = op;
        }

        for (const auto &[key, value] : j["addresses"].items ()) {
            list <Bitcoin::outpoint> op;
            for (const JSON &j : value) op = append (op, read_outpoint (std::string (j)));
            AddressIndex[Bitcoin::address (std::string (key))] = op;
        }
    }

    local_txdb::operator JSON () const {
        JSON::object_t txs {};
        for (const auto &[key, value] : Transactions)
            txs [write (key)] = JSON (value);

        JSON::object_t headers {};
        for (const auto &[key, value] : Headers)
            headers [write (key)] = write_header (value);

        JSON::object_t redemptions {};
        for (const auto &[key, value] : RedeemIndex)
            redemptions [write (key)] = write (value);

        JSON::object_t scripts {};
        for (const auto &[key, value] : ScriptIndex) {
            JSON::array_t vals;
            for (const Bitcoin::outpoint &b : value) vals.push_back (write (b));
            scripts [write (key)] = vals;
        }

        JSON::object_t addresses {};
        for (const auto &[key, value] : AddressIndex) {
            JSON::array_t vals;
            for (const Bitcoin::outpoint &b : value) vals.push_back (write (b));
            addresses [key] = vals;
        }

        return JSON::object_t {
            {"transactions", txs},
            {"headers", headers},
            {"redemptions", redemptions},
            {"scripts", scripts},
            {"addresses", addresses}};
    }

    ptr<vertex> local_txdb::operator [] (const Bitcoin::txid &id) {
        auto t = Transactions.find (id);
        if (t == Transactions.end ()) throw exception {} << "cannot retrieve tx " << id;
        auto h = Headers.find (t->second.Proof->BlockHash);
        if (h == Headers.end ()) throw exception {} << "cannot retrieve tx " << id;
        return ptr<vertex> {new vertex {t->second, h->second}};
    }

    bool processed::verify (const Bitcoin::txid &id, const byte_array<80> &h) const {
        return this->id () == id && Bitcoin::header::valid (h) &&
            Merkle::proof {Merkle::branch {id, Proof->Path}, Bitcoin::header::merkle_root (h)}.valid ();
    }

    bool local_txdb::import_transaction (const processed &p, const byte_array<80> &h) {

        if (!vertex {p, h}.valid ()) return false;

        auto txid = p.id ();
        auto hid = Bitcoin::header::hash (h);

        if (!Headers.contains (hid)) Headers[hid] = h;
        if (!Transactions.contains (txid)) {

            Transactions[txid] = p;

            Bitcoin::transaction tx {p.Transaction};

            uint32 i = 0;
            for (const Bitcoin::input in : tx.Inputs) {
                RedeemIndex[in.Reference] = inpoint {txid, i};
                i++;
            }

            i = 0;
            for (const Bitcoin::output out : tx.Outputs) {
                auto op = Bitcoin::outpoint {txid, i};

                {
                    auto script_hash = Gigamonkey::SHA2_256 (out.Script);
                    auto v = ScriptIndex.find (script_hash);
                    if (v != ScriptIndex.end ()) v->second = data::append (v->second, op);
                    else ScriptIndex[script_hash] = list<Bitcoin::outpoint> {op};
                }

                Gigamonkey::pay_to_address ppp {out.Script};
                if (ppp.valid ()) {
                    Bitcoin::address addr {Bitcoin::address {Bitcoin::address::main, ppp.Address}};
                    auto v = AddressIndex.find (addr);
                    if (v != AddressIndex.end ()) v->second = data::append (v->second, op);
                    else AddressIndex[addr] = list<Bitcoin::outpoint> {op};
                }

                i++;
            }
        }

        return true;
    }

    ordered_list<ray> local_txdb::by_address (const Bitcoin::address &a) {
        auto zz = AddressIndex.find (a);
        if (zz == AddressIndex.end ()) return {};
        ordered_list<ray> n;
        for (const Bitcoin::outpoint &o : zz->second) n = n.insert (ray {*(*this) [o.Digest], o});
        return n;
    }

    ordered_list<ray> local_txdb::by_script_hash (const digest256 &x) {
        auto zz = ScriptIndex.find (x);
        if (zz == ScriptIndex.end ()) return {};
        ordered_list<ray> n;
        for (const Bitcoin::outpoint &o : zz->second) n = n.insert (ray {*(*this) [o.Digest], o});
        return n;
    }

    ptr<ray> local_txdb::redeeming (const Bitcoin::outpoint &o) {
        auto zz = RedeemIndex.find (o);
        if (zz == RedeemIndex.end ()) return {};
        auto p = (*this) [o.Digest];
        if (!p) return {};
        return ptr<ray> {new ray {*(*this) [zz->second.Digest], zz->second,
            Bitcoin::output {Bitcoin::transaction::output (p->Processed.Transaction, o.Index)}.Value}};
    }

    void cached_remote_txdb::import_transaction (const Bitcoin::txid &txid) {
        auto tx = Net.get_transaction (txid);
        if (tx.size () == 0) throw exception {} << "transaction " << txid << " does not exist.";
        auto proof = Net.WhatsOnChain.transaction ().get_merkle_proof (txid);

        if (!proof.Proof.valid ())
            throw exception {} << "could not get Merkle proof for " << txid;

        processed p {tx, Merkle::path (proof.Proof.Branch), proof.BlockHash};

        auto h = Local.Headers.find (proof.BlockHash);
        if (h != Local.Headers.end ()) Local.import_transaction (p, h->second);
        else {
            byte_array<80> header = Net.WhatsOnChain.block ().get_header (proof.BlockHash);
            if (!Bitcoin::header::valid (header)) return;

            Local.import_transaction (p, header);
        }
    }

    ordered_list<ray> cached_remote_txdb::by_address (const Bitcoin::address &a) {
        auto x = Local.by_address (a);
        if (!data::empty (x)) return x;

        auto ids = Net.WhatsOnChain.address ().get_history (a);

        int i = 0;
        for (const Bitcoin::txid &txid : ids) {
            import_transaction (txid);
            i++;
        }

        return Local.by_address (a);
    }

    ordered_list<ray> cached_remote_txdb::by_script_hash (const digest256 &z) {
        auto x = Local.by_script_hash (z);
        if (data::empty (x)) return x;

        auto ids = Net.WhatsOnChain.script ().get_history (z);

        for (const Bitcoin::txid &txid : ids) import_transaction (txid);

        return Local.by_script_hash (z);
    }

    // we don't check online for a reedeming tx because if this was in the
    // UTXO set we'd have to keep checking every time. Instead we assume
    // that we have the redeeming tx for any output we owned.
    ptr<ray> cached_remote_txdb::redeeming (const Bitcoin::outpoint &o) {
        return Local.redeeming (o);
    }

    ptr<vertex> cached_remote_txdb::operator [] (const Bitcoin::txid &id) {
        auto p = Local [id];
        if (p != nullptr) return p;
        import_transaction (id);
        return Local [id];
    }

    double price_data::operator [] (const Bitcoin::timestamp &t) {
        auto v = Price.find (t);
        if (v != Price.end ()) return v->second;
        double p = Net.price (t);
        Price[t] = p;
        return p;
    }

    price_data::price_data (network &n, const JSON &j): Net {n} {
        std::cout << "reading " << j.size () << " entries of price data " << std::endl;
        for (const JSON &x : j) Price[Bitcoin::timestamp {uint32 (x[0])}] = double (x[1]);
    }

    price_data::operator JSON () const {
        JSON::array_t data {};
        for (const auto &[key, value] : Price)
            data.push_back (JSON::array_t {uint32 (key), value});

        return data;
    }
}
