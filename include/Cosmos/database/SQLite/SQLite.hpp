#ifndef COSMOS_DATABASE_SQLITE_SQLITE
#define COSMOS_DATABASE_SQLITE_SQLITE

#include <Cosmos/database.hpp>
#include <Cosmos/database/json/txdb.hpp>

namespace Cosmos {
    using filepath = std::filesystem::path;
}

namespace Cosmos::SQLite {

    ptr<controller> load (const data::maybe<filepath> &fzf);
    ptr<controller> load_and_update (const data::maybe<filepath> &fzf, const JSON_local_TXDB *);

}

#endif
