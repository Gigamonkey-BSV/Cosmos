#include <Cosmos/database/write.hpp>
#include <filesystem>
#include <fstream>

namespace Cosmos {

    Bitcoin::outpoint read_outpoint (const string &x) {
        list<string_view> z = data::split (x, ":");
        if (z.size () != 2) throw exception {} << "invalid outpoint format: " << x;
        Bitcoin::outpoint o;
        o.Digest = read_txid (z[0]);
        o.Index = strtoul (std::string {z[1]}.c_str (), nullptr, 10);
        return o;
    }

    JSON write (const Bitcoin::output &j) {
        JSON::object_t op;
        op["value"] = write (j.Value);
        op["script"] = encoding::hex::write (j.Script);
        return op;
    }

    JSON write (const Bitcoin::input &o) {
        JSON::object_t ip;
        ip["reference"] = write (o.Reference);
        ip["script"] = encoding::hex::write (o.Script);
        ip["sequence"] = uint32 (o.Sequence);
        return ip;
    }

    std::string write (const Bitcoin::header &h) {
        return encoding::hex::write (h.write ());
    }

    Bitcoin::header read_header (const string &j) {
        if (j.size () != 160) throw exception {} << "invalid header size for " << j;
        byte_array<80> p;
        boost::algorithm::unhex (j.begin (), j.end (), p.begin ());
        return Bitcoin::header (p);
    }

    either<Bitcoin::pubkey, HD::BIP_32::pubkey> inline read_pubkey (const string &x) {
        if (HD::BIP_32::pubkey p {x}; p.valid ()) return p;
        if (Bitcoin::pubkey p {x}; p.valid ()) return p;
        throw exception {} << "could not read " << x << " as a bitcoin public key.";
    }

    void write_to_file (const JSON &j, const std::string &filename) {
        std::fstream file;
        file.open (filename, std::ios::out);
        if (!file) throw exception {"could not open file"};
        file << j.dump (0, ' ');
        file.close ();
    }

    JSON read_from_file (const std::string &filename) {

        std::filesystem::path p {filename};
        if (!std::filesystem::exists (p)) return JSON (nullptr);
        std::ifstream file;
        file.open (filename, std::ios::in);
        if (!file) throw exception {"could not open file"};

        return JSON::parse (file);
    }

}
