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

namespace Cosmos {

    std::ostream &operator << (std::ostream &, hash_function);
    std::istream &operator >> (std::istream &, hash_function &);

    std::ostream inline &operator << (std::ostream &o, hash_function f) {
        switch (f) {
            case hash_function::SHA1: return o << "SHA1";
            case hash_function::MD5: return o << "MD5";
            case hash_function::SHA2_256: return o << "SHA2_256";
            case hash_function::SHA2_512: return o << "SHA2_512";
            case hash_function::RIPEMD160: return o << "RIPEMD160";
            case hash_function::SHA3_256: return o << "SHA3_256";
            case hash_function::SHA3_512: return o << "SHA3_512";
            case hash_function::Hash256: return o << "Hash256";
            case hash_function::Hash160: return o << "Hash160";
            default: throw data::exception {} << "invalid hash function";
        }
    }
}

#endif
