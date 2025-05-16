#ifndef COSMOS_DATABASE_MEMORY_DATABASE
#define COSMOS_DATABASE_MEMORY_DATABASE

#include <Cosmos/database.hpp>

namespace Cosmos {

    // A local_TXDB that extends the in-memory implementation of the SPV database.
    struct memory_local_TXDB : public local_TXDB, public SPV::database::memory {
        memory_local_TXDB ();

        using SPV::database::memory::insert;

        events by_address (const Bitcoin::address &) final override;
        events by_script_hash (const digest256 &) final override;
        event redeeming (const Bitcoin::outpoint &) final override;

        void add_address (const Bitcoin::address &, const digest256 &) final override;
        digest256 add_script (const data::bytes &) final override;
        void add_output (const digest256 &, const Bitcoin::outpoint &) final override;
        void set_redeem (const Bitcoin::outpoint &, const inpoint &) final override;

        // NOTE: if a tx is ever dropped from the mempool (which shouldn't really happen)
        // Some information about it will not be dropped from these indices. Too bad but
        // it's not worth fixing. The in-memory database is just scaffolding.
        std::map<Bitcoin::address, list<digest256>> AddressIndex {};
        std::map<digest256, list<Bitcoin::outpoint>> ScriptIndex {};
        std::map<Bitcoin::outpoint, inpoint> RedeemIndex {};
        std::map<Bitcoin::timestamp, double> Price;

        virtual ~memory_local_TXDB () {}
    };

    inline memory_local_TXDB::memory_local_TXDB () :
        local_TXDB {}, SPV::database::memory {}, AddressIndex {}, ScriptIndex {}, RedeemIndex {} {}

    void inline memory_local_TXDB::set_redeem (const Bitcoin::outpoint &op, const inpoint &ip) {
        RedeemIndex[op] = ip;
    }
}

#endif
