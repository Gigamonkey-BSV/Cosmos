#include <Cosmos/database/txdb.hpp>
#include <gigamonkey/merkle/BUMP.hpp>
#include <filesystem>
#include <fstream>

namespace Cosmos {

    bool local_TXDB::import_transaction (const Bitcoin::transaction &tx, const Merkle::path &p, const Bitcoin::header &h) {

        auto hid = h.hash ();
        if (!this->header (hid)) return false;

        auto txid = tx.id ();
        if (!SPV::proof::valid (txid, p, h.MerkleRoot)) return false;

        auto db_entry = this->transaction (txid);

        if (!db_entry.Confirmation.valid ()) this->insert (Merkle::proof {Merkle::branch {txid, p}, h.MerkleRoot});

        if (this->transaction (txid).Transaction == nullptr) {
            this->insert (tx);

            uint32 i = 0;
            for (const Bitcoin::input in : tx.Inputs)
                this->set_redeem (in.Reference, inpoint {txid, i++});

            i = 0;
            for (const Bitcoin::output out : tx.Outputs) {
                auto script_hash = this->add_script (out.Script);
                auto op = Bitcoin::outpoint {txid, i};
                this->add_output (script_hash, op);

                pay_to_address p2a {out.Script};
                if (p2a.valid ())
                    this->add_address (Bitcoin::address {Bitcoin::address {Bitcoin::address::main, p2a.Address}}, script_hash);

                i++;
            }
        }

        return true;
    }

    namespace {
        const ptr<const entry<N, Bitcoin::header>> import_header (local_TXDB &local, network &net, const N &n) {
            auto block = net.WhatsOnChain.block ();
            auto header = synced (&whatsonchain::blocks::get_header_by_height, &block, n);
            return local.insert (header.Height, header.Header);
        }

        const ptr<const entry<N, Bitcoin::header>> import_header (local_TXDB &local, network &net, const digest256 &d) {
            auto block = net.WhatsOnChain.block ();
            auto header = synced (&whatsonchain::blocks::get_header_by_hash, &block, d);
            return local.insert (header.Height, header.Header);
        }
    }

    awaitable<bool> cached_remote_TXDB::import_transaction (const Bitcoin::TXID &txid) {

        maybe<bytes> tx = co_await Net.get_transaction (txid);
        if (!bool (tx)) co_return false;

        auto proof = co_await Net.WhatsOnChain.transaction ().get_merkle_proof (txid);

        if (!bool (proof)) {
            Local.insert (Bitcoin::transaction {*tx});
            co_return true;
        }

        if (!proof->Proof.valid ()) co_return false;
        ptr<const entry<N, Bitcoin::header>> h = Local.header (proof->BlockHash);
        if (!bool (h)) h = import_header (Local, Net, proof->BlockHash);
        if (!bool (h)) co_return false;
        co_return Local.import_transaction (Bitcoin::transaction {*tx}, Merkle::path (proof->Proof.Branch), h->Value);
    }

    events cached_remote_TXDB::by_address (const Bitcoin::address &a) {
        auto x = Local.by_address (a);
        if (!data::empty (x) && x.valid ()) return x;

        auto address = Net.WhatsOnChain.address ();
        list<Bitcoin::TXID> ids = synced (&whatsonchain::addresses::get_history, &address, a);

        int i = 0;
        for (const Bitcoin::TXID &txid : ids) {
            if (!synced (&cached_remote_TXDB::import_transaction, this, txid))
                // we just print the error because we may be in the middle of an
                // operation and then what do we do? This would require the user
                // to fix it up.
                std::cout << "error importing txid " << txid << std::endl;
            i++;
        }

        return Local.by_address (a);
    }

    events cached_remote_TXDB::by_script_hash (const digest256 &z) {
        auto x = Local.by_script_hash (z);
        if (data::empty (x)) return x;

        auto scripts = Net.WhatsOnChain.script ();
        auto ids = synced (&whatsonchain::scripts::get_history, &scripts, z);
        for (const Bitcoin::TXID &txid : ids) {
            if (!synced (&cached_remote_TXDB::import_transaction, this, txid))
                std::cout << "error importing txid " << txid << std::endl;
        }
        return Local.by_script_hash (z);
    }

    // we don't check online for a reedeming tx because if this was in the
    // UTXO set we'd have to keep checking every time. Instead we assume
    // that we have the redeeming tx for any output we owned.
    event cached_remote_TXDB::redeeming (const Bitcoin::outpoint &o) {
        return Local.redeeming (o);
    }

    SPV::database::tx cached_remote_TXDB::transaction (const Bitcoin::TXID &xd) {
        auto p = Local.transaction (xd);
        if (p.valid () && p.confirmed ()) return p;
        if (!synced (&cached_remote_TXDB::import_transaction, this, xd))
            std::cout << "error importing txid " << xd << std::endl;
        return Local.transaction (xd);
    }

    ptr<const entry<N, Bitcoin::header>> cached_remote_TXDB::header (const N &n) {
        const auto h = Local.header (n);
        if (bool (h)) return h;
        return import_header (Local, Net, n);
    }

    ptr<const entry<N, Bitcoin::header>> cached_remote_TXDB::header (const digest256 &d) {
        const auto e = Local.header (d);
        if (bool (e)) return e;
        return import_header (Local, Net, d);
    }

    namespace {
        awaitable<broadcast_single_result> broadcast_tx (cached_remote_TXDB &txdb, const extended_transaction &tx) {
            auto success = co_await txdb.Net.broadcast (tx);
            if (bool (success)) txdb.Local.insert (Bitcoin::transaction (tx));
            co_return success;
        }

        awaitable<broadcast_multiple_result> broadcast_txs (cached_remote_TXDB &txdb, const list<extended_transaction> &txs) {
            auto success = co_await txdb.Net.broadcast (txs);
            if (bool (success)) for (const auto &tx : txs) txdb.Local.insert (Bitcoin::transaction (tx));
            co_return success;
        }

        awaitable<broadcast_tree_result> broadcast_map (cached_remote_TXDB &txdb, SPV::proof::map map) {
            broadcast_tree_result so_far {};
            // go down the tree and process leaves first.
            for (const auto &[txid, pn] : map) if (pn->Proof.is<SPV::confirmation> ()) {
                    // leaves have confirmations so they don't need to be broadcast and they
                    // just go into the database.
                    const auto &conf = pn->Proof.get<SPV::confirmation> ();
                    if (!txdb.Local.import_transaction (pn->Transaction, conf.Path, conf.Header)) {
                        so_far.Sub = so_far.Sub.insert (txid, broadcast_result::ERROR_INVALID);
                        co_return so_far;
                    }
                } else {
                    // try to broadcast all sub transactions.
                    auto m = pn->Proof.get<SPV::proof::map> ();
                    auto successes = co_await broadcast_map (txdb, m);
                    so_far.Sub = so_far.Sub & successes.Sub;
                    if (bool (successes)) {
                        so_far.Error = successes.Error;
                        co_return so_far;
                    } else {
                        auto success = co_await broadcast_tx (txdb, SPV::extended_transaction (pn->Transaction, m));
                        so_far.Sub = so_far.Sub.insert (txid, success);
                        if (bool (success)) txdb.Local.insert (pn->Transaction);
                        else {
                            so_far.Error = success.Error;
                            co_return so_far;
                        }
                    }
                }

            //
            co_return so_far;
        }
    }

    awaitable<broadcast_tree_result> cached_remote_TXDB::broadcast (SPV::proof p) {
        if (!p.validate (*this)) co_return broadcast_result::ERROR_INVALID;

        broadcast_tree_result success_map = co_await broadcast_map (*this, p.Proof);

        // if there are any errors, return those.
        for (const auto &[_, x] : success_map.Sub) if (!bool (x))
            co_return broadcast_tree_result {{x.Error, x.Details}, success_map.Sub};

        co_return broadcast_tree_result {co_await broadcast_txs (*this, SPV::extended_transactions (p.Payment, p.Proof)), success_map.Sub};
    }

    std::ostream &operator << (std::ostream &o, const event &r) {
        o << "\n\t" << r.value () << " ";
        if (r.Direction == direction::in) o << "received in ";
        else o << "spent from ";
        return o << write (r.point ()) << std::endl;
    }

    std::ostream &operator << (std::ostream &o, const when &r) {
        if (r.is<bool> ()) {
            if (!r.get<bool> ()) o << "-";
            return o << "infinity";
        }

        if (r.get<Bitcoin::timestamp> () == Bitcoin::timestamp {0}) return o << "unconfirmed";
        return o << r.get<Bitcoin::timestamp> ();
    }

    std::partial_ordering when::operator <=> (const when &w) const {
        if (*this == w) return *this == unconfirmed () ?
            std::partial_ordering::unordered :
            std::partial_ordering::equivalent;

        // so we know that they are not equal.
        if (this->is<bool> ()) this->get<bool> () ?
            std::partial_ordering::greater :
            std::partial_ordering::less;

        if (w.is<bool> ()) return w.get<bool> () ?
            std::partial_ordering::less :
            std::partial_ordering::greater;

        // they must both be timestamps at this point.
        return *this == unconfirmed () ? std::partial_ordering::greater :
            this->get<Bitcoin::timestamp> () <=> w.get<Bitcoin::timestamp> ();
    }

    std::partial_ordering event::operator <=> (const event &e) const {
        if (!valid () || !e.valid ()) throw exception {} << "invalid tx.";

        auto compare_tx = SPV::proof::ordering ((*this)->Proof, e->Proof);

        if (compare_tx != std::partial_ordering::equivalent) return compare_tx;
        if (Direction != e.Direction) return Direction == direction::in ? std::partial_ordering::less : std::partial_ordering::greater;

        return Index <=> Index;
    }

    ptr<vertex> TXDB::operator [] (const Bitcoin::TXID &id) {
        SPV::database::tx tx = this->transaction (id);
        if (!tx.valid ()) return {};
        if (tx.confirmed ()) {
            auto ext = SPV::extend (*this, *tx.Transaction);
            if (!bool (ext)) return {};
            return std::make_shared<vertex> (*ext,
                entry<Bitcoin::TXID, SPV::proof::tree> {id, SPV::proof::tree (tx.Confirmation)});
        }

        maybe<SPV::proof> p = SPV::generate_proof (*this, {*tx.Transaction});
        if (!bool (p)) return {};

        return std::make_shared<vertex> (
            SPV::extended_transaction (p->Payment[0], p->Proof),
            entry<Bitcoin::TXID, SPV::proof::tree> {id, SPV::proof::tree (p->Proof)});
    }

    when when_from_JSON (const JSON &j) {
        if (j.is_string ()) {
            string str = std::string (j);
            if (str == "infinity") return when::infinity ();
            if (str == "-infinity") return when::negative_infinity ();
            if (str == "unconfirmed") return when::unconfirmed ();
        } else if (j.is_number_unsigned ()) return when {Bitcoin::timestamp {uint32 (j)}};
        throw exception {} << "could not read Bitcoin time from " << j;
    }

    when::when (const JSON &j): when {when_from_JSON (j)} {}

    when::operator JSON () const {
        if (*this == when::unconfirmed ()) return "unconfirmed";
        if (*this == when::infinity ()) return "infinity";
        if (*this == when::negative_infinity ()) return "-infinity";
        return uint32 (this->get<Bitcoin::timestamp> ());
    }
}
