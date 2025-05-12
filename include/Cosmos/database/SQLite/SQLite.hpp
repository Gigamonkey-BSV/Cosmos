#ifndef COSMOS_DATABASE_SQLITE_SQLITE
#define COSMOS_DATABASE_SQLITE_SQLITE

#include <Cosmos/database/txdb.hpp>
#include <Cosmos/database/json/txdb.hpp>

namespace Cosmos::SQLite {

    ptr<local_TXDB> load (const std::string &fzf);
    ptr<local_TXDB> load_and_update (const std::string &fzf, const JSON_local_TXDB *);

}

#endif
