
#include <Cosmos/wallet/keys/sequence.hpp>

namespace Cosmos {

    JSON write_path (HD::BIP_32::path p) {
        JSON::array_t a;
        a.resize (p.size ());
        int index = 0;
        for (const uint32 &u : p) a[index++] = u;
        return a;
    }

    HD::BIP_32::path read_path (const JSON &j) {
        HD::BIP_32::path p;
        for (const JSON &jj : j) {
            if (!jj.is_number ()) throw exception {} << " could not read path " << j;
            p <<= uint32 (jj);
        }
        return p;
    }

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

    pubkey secret::to_public () const {
        if (Bitcoin::secret x {*this}; x.valid ()) return x.to_public ();
        if (HD::BIP_32::secret x {*this}; x.valid ()) return x.to_public ();
        throw exception {"invalid secret key"};
    };

    address_sequence::address_sequence (const JSON &j) {
        if (j == JSON (nullptr)) return;

        Key = HD::BIP_32::pubkey {std::string (j["key"])};
        Path = read_path (j["path"]);
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
