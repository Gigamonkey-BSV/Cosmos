#ifndef SERVER_DB
#define SERVER_DB

#include <Cosmos/database.hpp>

namespace net = data::net;

struct SQLite_options {
    maybe<filepath> Path {}; // missing for in-memory
};

// depricated. We changed things too much to be
// able to use the old JSON database anymore.
struct JSON_DB_options {
    std::string Path; // should be a directory
};

// we were talking about using mongodb at one point 
// but that is not supported at all right now. 
struct MongoDB_options {
    net::URL URL;
    std::string UserName;
    std::string Password;
}; // not yet supported.

using db_options = either<JSON_DB_options, SQLite_options, MongoDB_options>;

using database = Cosmos::database;

ptr<database> load_DB (const db_options &o);

#endif