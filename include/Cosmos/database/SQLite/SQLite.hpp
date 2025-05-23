#ifndef COSMOS_DATABASE_SQLITE_SQLITE
#define COSMOS_DATABASE_SQLITE_SQLITE

#include <Cosmos/database.hpp>
#include <Cosmos/database/json/txdb.hpp>

namespace Cosmos {
    using filepath = std::filesystem::path;
}

namespace Cosmos::SQLite {

    ptr<database> load (const data::maybe<filepath> &fzf);
    ptr<database> load_and_update (const data::maybe<filepath> &fzf, const JSON_local_TXDB *);

}

#endif
