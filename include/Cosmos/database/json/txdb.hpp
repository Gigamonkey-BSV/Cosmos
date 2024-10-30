#ifndef COSMOS_DATABASE_JSON_TXDB
#define COSMOS_DATABASE_JSON_TXDB

#include <Cosmos/database/memory/txdb.hpp>

namespace Cosmos {
    struct JSON_local_TXDB final : public memory_local_TXDB {

        JSON_local_TXDB () : memory_local_TXDB {} {}
        explicit JSON_local_TXDB (const JSON &);
        explicit operator JSON () const;
    };
}

#endif
