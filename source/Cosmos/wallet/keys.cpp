
#include <Cosmos/wallet/keys.hpp>

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
        d["name"] = Name;
        d["path"] = write_path (Path);
        return d;
    }

    derivation::derivation (const JSON &j) {
        Name = std::string (j["name"]);
        read_path (Path, j["path"]);
    }

    derived_pubkey::derived_pubkey::derived_pubkey (const JSON &j):
        Pubkey {std::string (j["pubkey"])}, Derivation {j["derivation"]} {}

    derived_pubkey::operator JSON () const {
        JSON::object_t j;
        j["pubkey"] = string (Pubkey);
        j["derivation"] = JSON (Derivation);
        return j;
    }

    hd_pubkey_sequence::hd_pubkey_sequence (const JSON &j) :
        hd_pubkey {HD::BIP_32::pubkey {std::string (j["pubkey"])}, derivation {j["path"]}}, Last {j["last"]} {}

    hd_pubkey_sequence::operator JSON () const {
        JSON::object_t j;
        j["pubkey"] = string (Pubkey);
        j["last"] = Last;
        j["derivation"] = JSON (Derivation);
        return j;
    }

    bool pubkey::valid () const {
        if (std::holds_alternative<derived_pubkey> (*this)) return std::get<derived_pubkey> (*this).Pubkey.valid ();
        return std::get<hd_pubkey_sequence> (*this).Pubkey.valid ();
    }

    bool secret::valid () const {
        if (std::holds_alternative<Bitcoin::secret> (*this)) return std::get<Bitcoin::secret> (*this).valid ();
        return std::get<HD::BIP_32::secret> (*this).valid ();
    }

    secret::operator string () const {
        if (std::holds_alternative<Bitcoin::secret> (*this)) return std::string (std::get<Bitcoin::secret> (*this));
        return string (std::get<HD::BIP_32::secret> (*this));
    }

    secret::secret (const string &x) : either<Bitcoin::secret, HD::BIP_32::secret> {Bitcoin::secret {x}} {
        if (!std::get<Bitcoin::secret> (*this).valid ())
        *this = HD::BIP_32::secret {x};
    }

    pubkey::pubkey () : either<derived_pubkey, hd_pubkey_sequence> {derived_pubkey {}} {}

    pubkey::pubkey (const JSON &j): pubkey {} {
        if (j.contains ("last")) *this = hd_pubkey_sequence {j};
        else *this = derived_pubkey {j};
    }

    pubkey::operator JSON () const {
        if (std::holds_alternative<derived_pubkey> (*this)) return JSON (std::get<derived_pubkey> (*this));
        return JSON (std::get<hd_pubkey_sequence> (*this));
    }

    pubkey pubkey::next () const {
        if (std::holds_alternative<hd_pubkey_sequence> (*this)) return pubkey {std::get<hd_pubkey_sequence> (*this).next ()};
        return *this;
    }

    derived_pubkey pubkey::last () const {
        if (std::holds_alternative<hd_pubkey_sequence> (*this)) return std::get<hd_pubkey_sequence> (*this).last ();
        return std::get<derived_pubkey> (*this);
    }

    Bitcoin::secret secret::derive (HD::BIP_32::path p) const {
        if (std::holds_alternative<Bitcoin::secret> (*this)) {
            if (data::size (p) != 0) throw exception {} << "attempt to derive key from non-HD key.";

            return std::get<Bitcoin::secret> (*this);
        }

        return Bitcoin::secret (std::get<HD::BIP_32::secret> (*this).derive (p));
    }

    Bitcoin::pubkey pubkey::derive (HD::BIP_32::path p) const {
        if (std::holds_alternative<derived_pubkey> (*this)) {
            if (data::size (p) != 0) throw exception {} << "attempt to derive key from non-HD key.";

            return std::get<derived_pubkey> (*this).Pubkey;
        }

        return Bitcoin::pubkey (std::get<hd_pubkey_sequence> (*this).Pubkey.derive (p).Pubkey);
    }

    pubkeychain::pubkeychain (const JSON &j) {
        std::cout << "reading keychain" << std::endl;
        data::map<string, pubkey> db {};

        if (j == JSON (nullptr)) return;

        for (const auto &[key, value] : j[0].items ())
            db = db.insert (key, pubkey {value});

        *this = pubkeychain {db, std::string (j[1]), std::string (j[2])};
    }

    pubkeychain::operator JSON () const {
        JSON::object_t db {};
        for (const data::entry<string, pubkey> &e : Map)
            db [e.Key] = JSON (e.Value);

        return JSON::array_t {db, Receive, Change};
    }

    keychain::keychain (const JSON &j) {
        data::map<string, secret> db {};

        if (j == JSON (nullptr)) throw exception {} << "could not read pubkeychain";

        if (j != JSON (nullptr)) for (const auto &[key, value] : j[0].items ())
            db = db.insert (key, secret {std::string (value)});

        *this = keychain {db, std::string (j[1])};
    }

    keychain::operator JSON () const {
        JSON::object_t db {};
        for (const data::entry<string, secret> &e : Map)
            db [e.Key] = string (e.Value);
        return JSON::array_t {db, Master};
    }
}
