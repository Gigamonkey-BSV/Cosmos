
#include <Cosmos/wallet/keys/derivation.hpp>
#include <Cosmos/database/write.hpp>

namespace Cosmos {

    derivation::operator JSON () const {
        JSON::object_t d;
        d["parent"] = Parent;
        d["path"] = write_path (Path);
        return d;
    }

    derivation::derivation (const JSON &j) {
        Parent = std::string (j["parent"]);
        Path = read_path (j["path"]);
    }

}
