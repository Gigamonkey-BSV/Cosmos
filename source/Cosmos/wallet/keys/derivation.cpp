
#include <Cosmos/wallet/keys/derivation.hpp>
#include <Cosmos/database/write.hpp>

namespace Cosmos {

    derivation::operator JSON () const {
        JSON::object_t d;
        d["key"] = Key;
        d["path"] = write_path (Path);
        return d;
    }

    derivation::derivation (const JSON &j) {
        Key = std::string (j["key"]);
        Path = read_path (j["path"]);
    }

}
