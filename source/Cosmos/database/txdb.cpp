#include <Cosmos/database/txdb.hpp>
#include <gigamonkey/merkle/BUMP.hpp>
#include <filesystem>
#include <fstream>

namespace Cosmos {

    bool local_txdb::import_transaction (const Bitcoin::transaction &tx, const Merkle::path &p, const Bitcoin::header &h) {

        auto hid = h.hash ();
        if (!this->header (hid)) return false;

        auto txid = tx.id ();
        if (!SPV::proof::valid (txid, p, h.MerkleRoot)) return false;

        auto db_entry = this->tx (txid);

        if (!db_entry.Confirmation.valid ()) this->insert (Merkle::proof {Merkle::branch {txid, p}, h.MerkleRoot});
        if (this->tx (txid).Transaction == nullptr) {
            this->insert (tx);

            uint32 i = 0;
            for (const Bitcoin::input in : tx.Inputs)
                this->set_redeem (in.Reference, inpoint {txid, i++});

            i = 0;
            for (const Bitcoin::output out : tx.Outputs) {
                auto op = Bitcoin::outpoint {txid, i};
                this->add_script (Gigamonkey::SHA2_256 (out.Script), op);

                pay_to_address p2a {out.Script};
                if (p2a.valid ())
                    this->add_address (Bitcoin::address {Bitcoin::address {Bitcoin::address::main, p2a.Address}}, op);

                i++;
            }
        }

        return true;
    }

    bool cached_remote_txdb::import_transaction (const Bitcoin::TXID &txid) {

        auto tx = Net.get_transaction (txid);
        if (tx.size () == 0) return false;
        auto proof = Net.WhatsOnChain.transaction ().get_merkle_proof (txid);

        if (!proof.Proof.valid ()) return false;

        Bitcoin::transaction decoded {tx};
        auto h = Local.header (proof.BlockHash);
        if (bool (h)) return Local.import_transaction (decoded, Merkle::path (proof.Proof.Branch), h->Value);
        else {
            auto header = Net.WhatsOnChain.block ().get_header (proof.BlockHash);
            if (!header.valid ()) return false;

            Local.insert (header.Height, header.Header);
            return Local.import_transaction (decoded, Merkle::path (proof.Proof.Branch), header.Header);
        }
    }

    ordered_list<ray> cached_remote_txdb::by_address (const Bitcoin::address &a) {
        auto x = Local.by_address (a);
        if (!data::empty (x) && x.valid ()) return x;

        auto ids = Net.WhatsOnChain.address ().get_history (a);

        int i = 0;
        for (const Bitcoin::TXID &txid : ids) {
            import_transaction (txid);
            i++;
        }

        return Local.by_address (a);
    }

    ordered_list<ray> cached_remote_txdb::by_script_hash (const digest256 &z) {
        auto x = Local.by_script_hash (z);
        if (data::empty (x)) return x;

        auto ids = Net.WhatsOnChain.script ().get_history (z);

        for (const Bitcoin::TXID &txid : ids) import_transaction (txid);

        return Local.by_script_hash (z);
    }

    // we don't check online for a reedeming tx because if this was in the
    // UTXO set we'd have to keep checking every time. Instead we assume
    // that we have the redeeming tx for any output we owned.
    ptr<ray> cached_remote_txdb::redeeming (const Bitcoin::outpoint &o) {
        return Local.redeeming (o);
    }

    vertex cached_remote_txdb::operator [] (const Bitcoin::TXID &id) {
        auto p = Local [id];
        if (p.valid ()) return p;
        import_transaction (id);
        return Local [id];
    }
}
