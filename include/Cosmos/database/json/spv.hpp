#ifndef COSMOS_DATABASE_JSON_SPV
#define COSMOS_DATABASE_JSON_SPV

#include <gigamonkey/SPV.hpp>
#include <Cosmos/types.hpp>

namespace Cosmos {
    namespace SPV = Gigamonkey::SPV;
    struct JSON_SPV_database : SPV::database::memory {

        JSON_SPV_database () : SPV::database::memory {} {}
        explicit JSON_SPV_database (const JSON &);
        explicit operator JSON () const;
    };
}

#endif
