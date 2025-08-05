
#include "db.hpp"

#include <Cosmos/database/SQLite/SQLite.hpp>

ptr<database> load_DB (const db_options &db_opts) {
    if (!db_opts.is<SQLite_options> ()) throw exception {} << "Only SQLite is supported";

    return Cosmos::SQLite::load (db_opts.get<SQLite_options> ().Path);
}
