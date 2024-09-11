
#include <Cosmos/wallet/keys/sequence.hpp>
#include <Cosmos/database/write.hpp>

namespace Cosmos {

    address_sequence::address_sequence (const JSON &j) {
        if (j == JSON (nullptr)) return;

        Parent = HD::BIP_32::pubkey {std::string (j["parent"])};
        Path = read_path (j["path"]);
        Last = j["last"];
    }

    address_sequence::operator JSON () const {
        JSON::object_t x {};
        x["parent"] = Parent.write ();
        x["path"] = write_path (Path);
        x["last"] = Last;
        return x;
    }

}

