#ifndef COSMOS_DATABASE_JSON_TXDB
#define COSMOS_DATABASE_JSON_TXDB

#include <Cosmos/database/txdb.hpp>
#include <Cosmos/database/json/spv.hpp>

namespace Cosmos {

    struct JSON_local_txdb final : local_txdb {

        // get a block header by height.
        const Bitcoin::header *header (const N &) const final override;

        const entry<N, Bitcoin::header> *latest () const final override;

        // get by hash or merkle root (need both)
        const entry<N, Bitcoin::header> *header (const digest256 &) const final override;

        // do we have a tx or merkle proof for a given tx?
        SPV::database::confirmed tx (const Bitcoin::TXID &) const final override;

        bool insert (const N &height, const Bitcoin::header &h) final override;

        bool insert (const Merkle::proof &) final override;
        void insert (const Bitcoin::transaction &) final override;

        ordered_list<ray> by_address (const Bitcoin::address &) final override;
        ordered_list<ray> by_script_hash (const digest256 &) final override;
        ptr<ray> redeeming (const Bitcoin::outpoint &) final override;

        set<Bitcoin::TXID> pending () final override;
        void remove (const Bitcoin::TXID &) final override;

        void add_address (const Bitcoin::address &, const Bitcoin::outpoint &) final override;
        void add_script (const digest256 &, const Bitcoin::outpoint &) final override;
        void set_redeem (const Bitcoin::outpoint &, const inpoint &) final override;

        JSON_local_txdb ();
        explicit JSON_local_txdb (const JSON &);
        explicit operator JSON () const;

        JSON_SPV_database SPVDB;

        std::map<Bitcoin::address, list<Bitcoin::outpoint>> AddressIndex;
        std::map<digest256, list<Bitcoin::outpoint>> ScriptIndex;
        std::map<Bitcoin::outpoint, inpoint> RedeemIndex;
    };

    inline JSON_local_txdb::JSON_local_txdb () :
        local_txdb {}, SPVDB {}, AddressIndex {}, ScriptIndex {}, RedeemIndex {} {}

    JSON_local_txdb inline read_JSON_local_txdb_from_file (const std::string &filename) {
        return JSON_local_txdb (read_from_file (filename));
    }

    const Bitcoin::header inline *JSON_local_txdb::header (const N &n) const {
        return SPVDB.header (n);
    }

    const entry<N, Bitcoin::header> inline *JSON_local_txdb::latest () const {
        return SPVDB.latest ();
    }

    const entry<N, Bitcoin::header> inline *JSON_local_txdb::header (const digest256 &h) const {
        return SPVDB.header (h);
    }

    SPV::database::confirmed inline JSON_local_txdb::tx (const Bitcoin::TXID &id) const {
        return SPVDB.tx (id);
    }

    bool inline JSON_local_txdb::insert (const N &height, const Bitcoin::header &h) {
        return SPVDB.insert (height, h);
    }

    bool inline JSON_local_txdb::insert (const Merkle::proof &p) {
        return SPVDB.insert (p);
    }

    void inline JSON_local_txdb::insert (const Bitcoin::transaction &tx) {
        return SPVDB.insert (tx);
    }

    void inline JSON_local_txdb::set_redeem (const Bitcoin::outpoint &op, const inpoint &ip) {
        RedeemIndex[op] = ip;
    }

    set<Bitcoin::TXID> inline JSON_local_txdb::pending () {
        return SPVDB.pending ();
    }

    void inline JSON_local_txdb::remove (const Bitcoin::TXID &tx) {
        return SPVDB.remove (tx);
    }
}

#endif
