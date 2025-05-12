#ifndef COSMOS_DATABASE_TXDB
#define COSMOS_DATABASE_TXDB

#include <gigamonkey/SPV.hpp>
#include <Cosmos/database/write.hpp>
#include <Cosmos/network.hpp>

namespace Cosmos {
    using namespace data;
    namespace Bitcoin = Gigamonkey::Bitcoin;
    namespace Merkle = Gigamonkey::Merkle;
    namespace SPV = Gigamonkey::SPV;

    using extended_transaction = Gigamonkey::extended::transaction;

    // a representation of time that includes unconfirmed and infinite
    // times. We use timestamp {0} for unconfirmed and bool for
    // positive and negative infinity. Unconfirmed is after every
    // Bitcoin::timestamp and incomparable with other unconfirmed.
    struct when : either<bool, Bitcoin::timestamp> {
        when (Bitcoin::timestamp t);
        explicit when (const JSON &j);
        explicit operator JSON () const;

        std::partial_ordering operator <=> (const when &w) const;

        bool operator == (const when &w) const;

        static when unconfirmed ();
        static when negative_infinity ();
        static when infinity ();

    private:
        using either<bool, Bitcoin::timestamp>::either;
    };

    std::ostream &operator << (std::ostream &o, const when &r);

    struct vertex {
        extended_transaction Transaction;
        entry<Bitcoin::TXID, SPV::proof::tree> Proof;

        vertex ();
        vertex (const extended_transaction &tx, const entry<Bitcoin::TXID, SPV::proof::tree> &pr);

        bool operator == (const vertex &tx) const;
        std::partial_ordering operator <=> (const vertex &tx) const;

        bool valid () const;
        operator bool () const;

        // whether a Merkle proof is included.
        bool confirmed () const;

        // check the proof if it exists and run all the scripts.
        bool validate (SPV::database &) const;

        Cosmos::when when () const;
    };

    enum class direction {
        in,
        out
    };

    // a tx with a complete merkle proof and an indication of a specific input or output.
    struct event : ptr<vertex> {

        Bitcoin::index Index;

        direction Direction;

        // output or input.
        bytes put () const;

        // outpoint or inpoint.
        Bitcoin::outpoint point () const {
            return {id (), Index};
        }

        // value moved.
        Bitcoin::satoshi value () const;

        event ();
        event (const ptr<vertex> &p, Bitcoin::index i, direction d);

        std::partial_ordering operator <=> (const event &e) const;
        bool operator == (const event &e) const;

        bool valid () const {
            return static_cast<const ptr<vertex> &> (*this) != nullptr;
        }

        const Bitcoin::TXID &id () const {
            return (*this)->Proof.Key;
        }

    };

    using events = ordered_list<event>;

    // a database of transactions.
    // we add to the SVP database by enabling
    // the retrievable of transactions as needed
    // from the network. Thus we can work entirely
    // with complete SPV proofs.
    struct TXDB : public virtual SPV::database {

        ptr<vertex> operator [] (const Bitcoin::TXID &id);

        // all events for a given address.
        virtual events by_address (const Bitcoin::address &) = 0;
        // using SHA2_256;
        virtual events by_script_hash (const digest256 &) = 0;
        virtual event redeeming (const Bitcoin::outpoint &) = 0;

        Bitcoin::output output (const Bitcoin::outpoint &p) {
            auto tx = this->transaction (p.Digest);
            if (!tx.valid ()) return {};
            return tx.Transaction->Outputs[p.Index];
        }

        Bitcoin::satoshi value (const Bitcoin::outpoint &p) {
            return output (p).Value;
        }

        virtual ~TXDB () {}

    };

    struct  local_TXDB : public virtual SPV::writable, public TXDB {
        // Check proof before entering it into the database.
        bool import_transaction (const Bitcoin::transaction &, const Merkle::path &, const Bitcoin::header &h);
        virtual void add_address (const Bitcoin::address &, const digest256 &script_hash) = 0;

    private:
        virtual digest256 add_script (const data::bytes &) = 0;
        // associate a script with a given hash with an output.
        virtual void add_output (const digest256 &, const Bitcoin::outpoint &) = 0;
        virtual void set_redeem (const Bitcoin::outpoint &, const inpoint &) = 0;
    public:
        virtual ~local_TXDB () {}
    };

    // Since we might end up trying to broadcast multiple txs at once
    // all coming from a single proof, we have a way of collecting all
    // results that come up for all broadcasts so that we can handle
    // the tx appropriately in the database.
    struct broadcast_tree_result : broadcast_multiple_result {
        using broadcast_multiple_result::broadcast_multiple_result;
        map<Bitcoin::TXID, broadcast_single_result> Sub;
        broadcast_tree_result (const broadcast_multiple_result &r, map<Bitcoin::TXID, broadcast_single_result> nodes = {}):
            broadcast_multiple_result {r}, Sub {nodes} {}
    };


    // an implementation of TXDB that relies on
    // a local database and a network connection.
    struct cached_remote_TXDB final : public TXDB {
        network &Net;
        local_TXDB &Local;

        cached_remote_TXDB (network &n, local_TXDB &x): TXDB {}, Net {n}, Local {x} {}

        ptr<const entry<N, Bitcoin::header>> header (const N &) final override;

        ptr<const entry<N, Bitcoin::header>> latest () final override;

        // get by hash or merkle root (need both)
        ptr<const entry<N, Bitcoin::header>> header (const digest256 &) final override;

        // do we have a tx or merkle proof for a given tx?
        SPV::database::tx transaction (const Bitcoin::TXID &) final override;

        set<Bitcoin::TXID> unconfirmed () final override;

        events by_address (const Bitcoin::address &) final override;
        events by_script_hash (const digest256 &) final override;
        event redeeming (const Bitcoin::outpoint &) final override;

        awaitable<bool> import_transaction (const Bitcoin::TXID &);

        awaitable<broadcast_tree_result> broadcast (SPV::proof);
    };

    set<Bitcoin::TXID> inline cached_remote_TXDB::unconfirmed () {
        return Local.unconfirmed ();
    }

    ptr<const entry<N, Bitcoin::header>> inline cached_remote_TXDB::latest () {
        return Local.latest ();
    }

    inline vertex::vertex (): Transaction {}, Proof {Bitcoin::TXID {}, {}} {}

    inline vertex::vertex (const extended_transaction &tx, const entry<Bitcoin::TXID, SPV::proof::tree> &pr):
        Transaction {tx}, Proof {pr} {}

    bool inline vertex::valid () const {
        return Proof.valid ();
    }

    inline vertex::operator bool () const {
        return valid ();
    }

    std::partial_ordering inline vertex::operator <=> (const vertex &tx) const {
        if (!valid () || !tx.valid ()) throw exception {} << "invalid tx.";
        auto compare_tx = SPV::proof::ordering (Proof, tx.Proof);
    }

    // whether a Merkle proof is included.
    bool inline vertex::confirmed () const {
        return valid () && Proof.Value.is<SPV::confirmation> ();
    }

    Cosmos::when inline vertex::when () const {
        return Proof.Value.is<SPV::proof::map> () ? when::unconfirmed (): Proof.Value.get<SPV::confirmation> ().Header.Timestamp;
    }

    inline when::when (Bitcoin::timestamp t) : either<bool, Bitcoin::timestamp> {t} {}

    bool inline when::operator == (const when &w) const {
        return static_cast<either<bool, Bitcoin::timestamp>> (*this) == static_cast<either<bool, Bitcoin::timestamp>> (w);
    }

    when inline when::unconfirmed () {
        return when {Bitcoin::timestamp {0}};
    }

    when inline when::negative_infinity () {
        return when {false};
    }

    when inline when::infinity () {
        return when {true};
    }

    bytes inline event::put () const {
        return Direction == direction::in ? bytes ((*this)->Transaction.Inputs[Index]) :
            bytes ((*this)->Transaction.Outputs[Index]);
    }

    // value moved.
    Bitcoin::satoshi inline event::value () const {
        return Direction == direction::out ?
            (*this)->Transaction.Outputs[Index].Value:
            (*this)->Transaction.Inputs[Index].Prevout.Value;
    }

    inline event::event (): ptr<vertex> {} {}
    inline event::event (const ptr<vertex> &p, Bitcoin::index i, direction d): ptr<vertex> {p}, Index {i}, Direction {d} {}

    bool inline event::operator == (const event &e) const {
        return ((*this) <=> e) == std::partial_ordering::equivalent;
    }
}

#endif
