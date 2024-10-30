
#ifndef COSMOS_FILES
#define COSMOS_FILES

#include <data/net/JSON.hpp>
#include <data/crypto/encrypted.hpp>
#include <data/tools/base_map.hpp>

// provide standard ways of converting certain types into strings and back.
namespace Cosmos {
    using namespace data;

    void write_to_file (const std::string &, const std::string &filename);

    void inline write_to_file (const JSON &j, const std::string &filename) {
        write_to_file (j.dump (' ', 2), filename);
    }

    struct file {
        JSON Payload;
        // If included, then the file is encrypted.
        maybe<crypto::symmetric_key<32>> Key;
        file (const JSON &j): Payload (j), Key {} {}
        file (const JSON &j, const crypto::symmetric_key<32> &k): Payload (j), Key {k} {}
    };

    crypto::symmetric_key<32> get_user_key_from_password ();

    void write_to_file (const file &, const std::string &filename);
    file read_from_file (const std::string &filename, crypto::symmetric_key<32> (*) () = &get_user_key_from_password);

    struct files : base_map<std::string, maybe<crypto::symmetric_key<32>>, files> {
        files write (const std::string &, const JSON &) const;
        tuple<JSON, files> load (const std::string &) const;
    };

}

#endif
