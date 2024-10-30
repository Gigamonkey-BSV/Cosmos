
#include <Cosmos/database/write.hpp>

namespace Cosmos {

    JSON write_path (HD::BIP_32::path p) {
        JSON::array_t a;
        a.resize (p.size ());
        int index = 0;
        for (const uint32 &u : p) a[index++] = u;
        return a;
    }

    HD::BIP_32::path read_path (const JSON &j) {
        if (!j.is_array ()) throw exception {} << "invalid JSON path format";
        HD::BIP_32::path p;
        for (const JSON &jj : j) {
            if (!jj.is_number ()) throw exception {} << " could not read path " << j;
            p <<= uint32 (jj);
        }
        return p;
    }

    Bitcoin::outpoint read_outpoint (const string &x) {
        list<string_view> z = data::split (x, ":");
        if (z.size () != 2) throw exception {} << "invalid outpoint format: " << x;
        Bitcoin::outpoint o;
        o.Digest = read_TXID (z[0]);
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

}
