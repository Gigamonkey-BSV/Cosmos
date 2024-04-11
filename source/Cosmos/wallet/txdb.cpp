#include <Cosmos/wallet/txdb.hpp>
#include <Cosmos/wallet/write.hpp>
#include <gigamonkey/script/pattern/pay_to_address.hpp>
#include <filesystem>
#include <fstream>

namespace Cosmos {

    JSON write (const ptr<SPV::database::memory::entry> &e) {
        JSON::object_t o;
        o["height"] = write (e->Header.Key);
        o["header"] = write (e->Header.Value);
        o["tree"] = e->dual_tree ().serialize ();
        return o;
    }

    ptr<SPV::database::memory::entry> read_db_entry (const JSON &j) {
        return std::make_shared<SPV::database::memory::entry> (
            read_N (std::string (j["height"])),
            read_header (std::string (j["header"])),
            Merkle::dual::deserialize (std::string (j["tree"])).Paths);
    }

    local_txdb::operator JSON () const {
        JSON::object_t o;

        JSON::object_t height;
        for (const auto &[key, value] : this->ByHeight) height[write (key)] = write (value);

        JSON::object_t txs;
        for (const auto &[key, value] : this->Transactions) txs[write (key)] = encoding::hex::write (value->write ());

        JSON::object_t addresses;
        for (const auto &[key, value] : this->AddressIndex) {
            JSON::array_t outpoints;
            for (const auto &out : value) outpoints.push_back (write (out));
            addresses[std::string (key)] = outpoints;
        }

        JSON::object_t scripts;
        for (const auto &[key, value] : this->ScriptIndex) {
            JSON::array_t outpoints;
            for (const auto &out : value) outpoints.push_back (write (out));
            scripts[write (key)] = outpoints;
        }

        JSON::object_t redeems;
        for (const auto &[key, value] : this->RedeemIndex) redeems[write (key)] = write (value);

        o["height"] = height;
        o["transactions"] = txs;
        o["addresses"] = addresses;
        o["scripts"] = scripts;
        o["redeems"] = redeems;

        return o;
    }

    local_txdb::local_txdb (const JSON &j) {

        for (const auto &[key, value] : j["by_height"].items ()) {
            ptr<SPV::database::memory::entry> e = read_db_entry (value);
            this->ByHeight[read_N (key)] = e;
            this->ByHash[e->Header.Value.hash ()] = e;
            this->ByRoot[e->Header.Value.MerkleRoot] = e;
            for (const auto &m : e->Paths) this->ByTXID[m.Key] = e;
        }

        N last_height = 0;
        ptr<SPV::database::memory::entry> last_entry {};
        for (const auto &[key, value] : this->ByHeight) {
            if (last_height + 1 == key) value->Last = last_entry;
            last_height = key;
            last_entry = value;
        }

        for (const auto &[key, value] : j["transactions"].items ())
            this->Transactions[read_txid (key)] =
                std::make_shared<const Bitcoin::transaction> (*encoding::hex::read (std::string (value)));

        for (const auto &[key, value] : j["addresses"].items ()) {
            list<Bitcoin::outpoint> outpoints;
            for (const auto &k : value) outpoints <<= read_outpoint (std::string (k));
            this->AddressIndex[Bitcoin::address (std::string (key))] = outpoints;
        }

        for (const auto &[key, value] : j["scripts"].items ()) {
            list<Bitcoin::outpoint> outpoints;
            for (const auto &k : value) outpoints <<= read_outpoint (std::string (k));
            this->ScriptIndex[read_txid (key)] = outpoints;
        }

        for (const auto &[key, value] : j["redeems"].items ())
            this->RedeemIndex[read_outpoint (key)] = inpoint {read_outpoint (value)};

    }

    bool local_txdb::import_transaction (const Bitcoin::transaction &tx, const Merkle::path &p, const Bitcoin::header &h) {

        if (!SPV::proof::valid (bytes (tx), p, h)) return false;

        auto txid = tx.id ();
        auto hid = h.hash ();

        if (!ByHash.contains (hid)) return false;

        if (!Transactions.contains (txid)) {

            Transactions[txid] = ptr<const Bitcoin::transaction> {new Bitcoin::transaction {tx}};

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
        for (const Bitcoin::outpoint &o : zz->second) n = n.insert (ray {(*this) [o.Digest], o});
        return n;
    }

    ordered_list<ray> local_txdb::by_script_hash (const digest256 &x) {
        auto zz = ScriptIndex.find (x);
        if (zz == ScriptIndex.end ()) return {};
        ordered_list<ray> n;
        for (const Bitcoin::outpoint &o : zz->second) n = n.insert (ray {(*this) [o.Digest], o});
        return n;
    }

    ptr<ray> local_txdb::redeeming (const Bitcoin::outpoint &o) {
        auto zz = RedeemIndex.find (o);
        if (zz == RedeemIndex.end ()) return {};
        auto p = (*this) [o.Digest];
        if (!p) return {};
        return ptr<ray> {new ray {(*this) [zz->second.Digest], zz->second, p.Transaction->Outputs[o.Index].Value}};
    }

    void cached_remote_txdb::import_transaction (const Bitcoin::txid &txid) {
        auto tx = Net.get_transaction (txid);
        if (tx.size () == 0) throw exception {} << "transaction " << txid << " does not exist.";
        auto proof = Net.WhatsOnChain.transaction ().get_merkle_proof (txid);

        if (!proof.Proof.valid ())
            throw exception {} << "could not get Merkle proof for " << txid;

        Bitcoin::transaction decoded {tx};
        auto h = Local.ByHash.find (proof.BlockHash);
        if (h != Local.ByHash.end ()) Local.import_transaction (decoded, Merkle::path (proof.Proof.Branch), h->second->Header.Value);
        else {
            auto header = Net.WhatsOnChain.block ().get_header (proof.BlockHash);
            if (!header.valid ()) return;

            Local.insert (header.Height, header.Header);
            Local.import_transaction (decoded, Merkle::path (proof.Proof.Branch), header.Header);
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

    vertex cached_remote_txdb::operator [] (const Bitcoin::txid &id) {
        auto p = Local [id];
        if (!p.valid ()) return p;
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
