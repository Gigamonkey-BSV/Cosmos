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

    namespace {
        bool import_header (local_txdb &local , network &net, const N &n) {
            auto header = net.WhatsOnChain.block ().get_header (n);
            if (!header.valid ()) return false;
            return local.insert (header.Height, header.Header);
        }

        bool import_header (local_txdb &local, network &net, const digest256 &d) {
            auto header = net.WhatsOnChain.block ().get_header (d);
            if (!header.valid ()) return false;
            return local.insert (header.Height, header.Header);
        }
    }

    bool cached_remote_txdb::import_transaction (const Bitcoin::TXID &txid) {

        auto tx = Net.get_transaction (txid);
        if (tx.size () == 0) return false;
        auto proof = Net.WhatsOnChain.transaction ().get_merkle_proof (txid);

        if (!proof.Proof.valid ()) return false;

        Bitcoin::transaction decoded {tx};
        auto h = Local.header (proof.BlockHash);
        if (bool (h)) return Local.import_transaction (decoded, Merkle::path (proof.Proof.Branch), h->Value);
        else return import_header (Local, Net, proof.BlockHash);
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

    SPV::database::confirmed cached_remote_txdb::tx (const Bitcoin::TXID &xd) {
        auto p = Local.tx (xd);
        if (p.valid ()) return p;
        import_transaction (xd);
        return Local.tx (xd);
    }

    const Bitcoin::header *cached_remote_txdb::header (const N &n) const {
        const auto *h = Local.header (n);
        if (bool (h)) return h;
        if (!import_header (Local, Net, n)) return nullptr;
        return Local.header (n);
    }

    const entry<N, Bitcoin::header> *cached_remote_txdb::header (const digest256 &d) const {
        const auto *e = Local.header (d);
        if (bool (e)) return e;
        if (!import_header (Local, Net, d)) return nullptr;
        return Local.header (d);
    }

    namespace {
        broadcast_single_result broadcast_tx (cached_remote_txdb &txdb, const extended_transaction &tx) {
            auto err = txdb.Net.broadcast (tx);
            if (!bool (err)) txdb.Local.insert (Bitcoin::transaction (tx));
            return err;
        }

        broadcast_multiple_result broadcast_txs (cached_remote_txdb &txdb, const list<extended_transaction> &txs) {
            auto err = txdb.Net.broadcast (txs);
            if (!bool (err)) for (const auto &tx : txs) txdb.Local.insert (Bitcoin::transaction (tx));
            return err;
        }

        broadcast_tree_result broadcast_map (cached_remote_txdb &txdb, SPV::proof::map map) {
            broadcast_tree_result so_far {};
            // go down the tree and process leaves first.
            for (const auto &[txid, pn] : map) if (pn->Proof.is<SPV::proof::confirmation> ()) {
                    // leaves have confirmations so they don't need to be broadcast and they
                    // just go into the database.
                    const auto &conf = pn->Proof.get<SPV::proof::confirmation> ();
                    if (!txdb.Local.import_transaction (pn->Transaction, conf.Path, conf.Header)) {
                        so_far.Sub = so_far.Sub.insert (txid, broadcast_error::invalid_transaction);
                        return so_far;
                    }
                } else {
                    // try to broadcast all sub transactions.
                    auto m = pn->Proof.get<SPV::proof::map> ();
                    auto successes = broadcast_map (txdb, m);
                    so_far.Sub = so_far.Sub + successes.Sub;
                    if (!bool (successes)) {
                        so_far.Error = successes.Error;
                        return so_far;
                    } else {
                        auto success = broadcast_tx (txdb, SPV::extended_transaction (pn->Transaction, m));
                        so_far.Sub = so_far.Sub.insert (txid, success);
                        if (bool (success)) txdb.insert (pn->Transaction);
                        else {
                            so_far.Error = success.Error;
                            return so_far;
                        }
                    }
                }

            //
            return so_far;
        }
    }

    broadcast_tree_result broadcast (cached_remote_txdb &txdb, SPV::proof p) {
        if (!p.valid ()) return broadcast_error::invalid_transaction;
        broadcast_tree_result success_map = broadcast_map (txdb, p.Proof);
        for (const auto &[_, x] : success_map.Sub) if (!bool (x))
            return {{x.Error, x.Details}, success_map.Sub};
        return {broadcast_txs (txdb, SPV::extended_transactions (p.Payment, p.Proof)), success_map.Sub};
    }
}
