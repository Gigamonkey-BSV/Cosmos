
#include <Cosmos/wallet/keys/sequence.hpp>

namespace Cosmos {

    JSON write_path (HD::BIP_32::path p) {
        JSON::array_t a;
        for (const uint32 &u : p) a.push_back (u);
        return a;
    }

    void read_path (HD::BIP_32::path &p, const JSON &j) {
        for (const JSON &jj : j) p <<= uint32 (jj);
    }

    derivation::operator JSON () const {
        JSON::object_t d;
        d["key"] = Key;
        d["path"] = write_path (Path);
        return d;
    }

    derivation::derivation (const JSON &j) {
        Key = std::string (j["key"]);
        read_path (Path, j["path"]);
    }

    pubkey secret::to_public () const {
        if (Bitcoin::secret x {*this}; x.valid ()) return x.to_public ();
        if (HD::BIP_32::secret x {*this}; x.valid ()) return x.to_public ();
        throw exception {"invalid secret key"};
    };

    address_sequence::address_sequence (const JSON &j) {
        if (j == JSON {nullptr}) return;

        Key = HD::BIP_32::pubkey {std::string (j["key"])};
        read_path (Path, j);
        Last = j["last"];
    }

    address_sequence::operator JSON () const {
        JSON::object_t x {};
        x["key"] = Key.write ();
        x["path"] = write_path (Path);
        x["last"] = Last;
        return x;
    }

}
