
#include "db.hpp"
#include "method.hpp"

#include <Cosmos/database/SQLite/SQLite.hpp>

ptr<database> load_DB (const db_options &db_opts) {
    if (!db_opts.is<SQLite_options> ()) throw data::exception {} << "Only SQLite is supported";

    return Cosmos::SQLite::load (db_opts.get<SQLite_options> ().Path);
}

std::istream &Cosmos::operator >> (std::istream &i, Cosmos::hash_function &f) {
    std::string word;
    i >> word;
    if (!i) return i;
    std::string sanitized = sanitize (word);
    if (sanitized == "sha1") f = hash_function::SHA1;
    else if (sanitized == "md5") f = hash_function::MD5;
    else if (sanitized == "sha2256") f = hash_function::SHA2_256;
    else if (sanitized == "sha2512") f = hash_function::SHA2_512;
    else if (sanitized == "sha3256") f = hash_function::SHA3_256;
    else if (sanitized == "sha3512") f = hash_function::SHA3_512;
    else if (sanitized == "ripmd160") f = hash_function::RIPEMD160;
    else if (sanitized == "hash256") f = hash_function::Hash256;
    else if (sanitized == "hash160") f = hash_function::Hash160;
    else i.setstate (std::ios::failbit);
    return i;
}
