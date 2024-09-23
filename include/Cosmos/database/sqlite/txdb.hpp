#ifndef COSMOS_DATABASE_MEMORY_TXDB
#define COSMOS_DATABASE_MEMORY_TXDB

#include <Cosmos/database/txdb.hpp>

namespace Cosmos {

    // see <Cosmos/database/json/txdb.hpp> for an example
    // that saves to and reads from a JSON object in a file.
    // it depends on <Cosmos/database/memory/txdb.hpp>
    // which in turn depends on <gigamonkey/SPV.hpp>

    struct SQLite_TXDB final : local_TXDB {
        const data::entry<data::N, Bitcoin::header> *latest () final override;

        const Bitcoin::header *header (const data::N &n) final override;
        const data::entry<data::N, Bitcoin::header> *header (const digest256 &n) override;

        tx transaction (const Bitcoin::TXID &t) final override;
        Merkle::dual dual_tree (const digest256 &d) const;

        const data::entry<N, Bitcoin::header> *insert (const data::N &height, const Bitcoin::header &h) final override;
        bool insert (const Merkle::proof &p) final override;
        void insert (const Bitcoin::transaction &) final override;

        // all unconfirmed txs in the database.
        set<Bitcoin::TXID> unconfirmed () final override;

        // only txs in unconfirmed can be removed.
        void remove (const Bitcoin::TXID &) final override;

        ordered_list<event> by_address (const Bitcoin::address &) final override;
        ordered_list<event> by_script_hash (const digest256 &) final override;
        event redeeming (const Bitcoin::outpoint &) final override;

        void add_address (const Bitcoin::address &, const Bitcoin::outpoint &) final override;
        void add_script (const digest256 &, const Bitcoin::outpoint &) final override;
        void set_redeem (const Bitcoin::outpoint &, const inpoint &) final override;

        // put some constructors here

    private:
        // anything else needed should go here.
    };
}

#endif

