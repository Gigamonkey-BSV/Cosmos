#ifndef COSMOS_DATABASE_JSON_TXDB
#define COSMOS_DATABASE_JSON_TXDB

#include <Cosmos/database/memory/database.hpp>
#include <Cosmos/database.hpp>

namespace Cosmos {
    struct JSON_local_TXDB final : public memory_local_TXDB {

        JSON_local_TXDB () : memory_local_TXDB {} {}

        // note: these functions need to change because
        // this type now contains more information than
        // just the txdb.
        explicit JSON_local_TXDB (const JSON &);
        explicit operator JSON () const;
    };
}

#endif
