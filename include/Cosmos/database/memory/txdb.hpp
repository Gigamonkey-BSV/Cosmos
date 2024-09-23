#ifndef COSMOS_DATABASE_MEMORY_TXDB
#define COSMOS_DATABASE_MEMORY_TXDB

#include <Cosmos/database/txdb.hpp>

namespace Cosmos {

    // A local_TXDB that extends the in-memory implementation of the SPV database.
    struct memory_local_TXDB : public local_TXDB, public SPV::database::memory {
        memory_local_TXDB ();

        using SPV::database::memory::insert;

        events by_address (const Bitcoin::address &) final override;
        events by_script_hash (const digest256 &) final override;
        event redeeming (const Bitcoin::outpoint &) final override;

        void add_address (const Bitcoin::address &, const Bitcoin::outpoint &) final override;
        void add_script (const digest256 &, const Bitcoin::outpoint &) final override;
        void set_redeem (const Bitcoin::outpoint &, const inpoint &) final override;

        std::map<Bitcoin::address, list<Bitcoin::outpoint>> AddressIndex {};
        std::map<digest256, list<Bitcoin::outpoint>> ScriptIndex {};
        std::map<Bitcoin::outpoint, inpoint> RedeemIndex {};

        virtual ~memory_local_TXDB () {}
    };

    inline memory_local_TXDB::memory_local_TXDB () :
        SPV::database::memory {}, local_TXDB {}, AddressIndex {}, ScriptIndex {}, RedeemIndex {} {}

    void inline memory_local_TXDB::set_redeem (const Bitcoin::outpoint &op, const inpoint &ip) {
        RedeemIndex[op] = ip;
    }
}

#endif
