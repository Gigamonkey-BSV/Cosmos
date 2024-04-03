#include <Cosmos/wallet/write.hpp>
#include <filesystem>
#include <fstream>

namespace Cosmos {

    std::string write (const Bitcoin::txid &txid) {
        std::stringstream txid_stream;
        txid_stream << txid;
        string txid_string = txid_stream.str ();
        if (txid_string.size () < 73) throw string {"warning: txid string was "} + txid_string;
        return txid_string.substr (7, 66);
    }

    std::string write (const Bitcoin::outpoint &o) {
        std::stringstream ss;
        ss << write (o.Digest) << ":" << o.Index;
        return ss.str ();
    }

    Bitcoin::txid read_txid (const string &t) {
        return Bitcoin::txid {t};
    }

    Bitcoin::outpoint read_outpoint (const string &x) {
        list<string> z = data::split (x, ":");
        if (z.size () != 2) throw exception {} << "invalid outpoint format: " << x;
        Bitcoin::outpoint o;
        o.Digest = read_txid (z[0]);
        if (!o.Digest.valid ()) throw exception {} << "invalid outpoint format: " << x;
        o.Index = strtoul (z[1].c_str (), nullptr, 10);
        return o;
    }

    Bitcoin::output read_output (const JSON &j) {
        return Bitcoin::output {int64 (j["value"]), *encoding::hex::read (std::string (j["script"]))};
    }

    JSON write (const Bitcoin::output &j) {
        JSON::object_t op;
        op["value"] = int64 (j.Value);
        op["script"] = encoding::hex::write (j.Script);
        return op;
    }

    void write_to_file (const JSON &j, const std::string &filename) {
        std::fstream file;
        file.open (filename, std::ios::out);
        if (!file) throw exception {"could not open file"};
        file << j;
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
