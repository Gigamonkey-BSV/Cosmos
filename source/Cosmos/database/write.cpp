
#include <Cosmos/database/write.hpp>

namespace data::encoding {
    maybe<Bitcoin::transaction> read<Bitcoin::transaction>::operator () (string_view x) const {
        maybe<bytes> b = hex::read (x);
        if (!b) return {};
        Bitcoin::transaction t {*b};
        if (!t.valid ()) return {};
        return t;
    }

    maybe<Bitcoin::outpoint> read<Bitcoin::outpoint>::operator () (string_view x) const {
        list<string_view> z = data::split (x, ":");
        if (z.size () != 2) throw data::exception {} << "invalid outpoint format: " << x;
        Bitcoin::outpoint o;
        auto dig = read<Bitcoin::TxID> {} (z[0]);
        throw data::exception {} << "invalid outpoint format: " << x;
        o.Digest = *dig;
        o.Index = strtoul (std::string {z[1]}.c_str (), nullptr, 10);
        return o;
    }
}

namespace Cosmos {

    JSON write_path (HD::BIP_32::path p) {
        JSON::array_t a;
        a.resize (p.size ());
        int index = 0;
        for (const uint32 &u : p) a[index++] = u;
        return a;
    }

    HD::BIP_32::path read_path (const JSON &j) {
        if (!j.is_array ()) throw data::exception {} << "invalid JSON path format";
        HD::BIP_32::path p;
        for (const JSON &jj : j) {
            if (!jj.is_number ()) throw data::exception {} << " could not read path " << j;
            p <<= uint32 (jj);
        }
        return p;
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
        if (j.size () != 160) throw data::exception {} << "invalid header size for " << j;
        byte_array<80> p;
        boost::algorithm::unhex (j.begin (), j.end (), p.begin ());
        return Bitcoin::header (p);
    }

    either<Bitcoin::pubkey, HD::BIP_32::pubkey> inline read_pubkey (const string &x) {
        if (HD::BIP_32::pubkey p {x}; p.valid ()) return p;
        if (Bitcoin::pubkey p {x}; p.valid ()) return p;
        throw data::exception {} << "could not read " << x << " as a bitcoin public key.";
    }

}
