#ifndef COSMOS_DATABASE_WRITE
#define COSMOS_DATABASE_WRITE

#include <data/net/JSON.hpp>
#include <data/encoding/read.hpp>

#include <gigamonkey/timechain.hpp>
#include <gigamonkey/schema/hd.hpp>

#include <Cosmos/network.hpp>

namespace data::encoding {
    template <> struct read<Bitcoin::transaction> {
        maybe<Bitcoin::transaction> operator () (string_view x) const;
    };

    template <> struct read<Bitcoin::outpoint> {
        maybe<Bitcoin::outpoint> operator () (string_view x) const;
    };

    template <> struct read<Bitcoin::TxID> {
        maybe<Bitcoin::TxID> operator () (string_view x) const {
            Bitcoin::TxID t {std::string {x}};
            if (!t.valid ()) return {};
            return t;
        }
    };
}

// provide standard ways of converting certain types into strings and back.
namespace Cosmos {
    namespace Bitcoin = Gigamonkey::Bitcoin;
    namespace SPV = Gigamonkey::SPV;
    namespace HD = Gigamonkey::HD;

    std::string write (const Bitcoin::TxID &);
    Bitcoin::TxID read_TxID (string_view);
    std::string write (const Bitcoin::outpoint &);
    Bitcoin::outpoint read_outpoint (string_view);
    std::string write (const N &);
    N read_N (const string &);

    std::string write (const Bitcoin::header &h);
    Bitcoin::header read_header (const string &j);

    JSON write (const Bitcoin::satoshi &j);
    Bitcoin::satoshi read_satoshi (const JSON &j);

    JSON write (const Bitcoin::output &j);
    Bitcoin::output read_output (const JSON &j);

    JSON write (const Bitcoin::input &j);
    Bitcoin::input read_input (const JSON &j);

    JSON write (const Bitcoin::prevout &);
    Bitcoin::prevout read_prevout (const JSON &j);

    JSON write_path (HD::BIP_32::path p);
    HD::BIP_32::path read_path (const JSON &j);

    std::string inline write (const N &n) {
        return encoding::decimal::write (n);
    }

    N inline read_N (const string &x) {
        return N {x};
    }

    std::string inline write (const Bitcoin::TxID &txid) {
        return encoding::hexidecimal::write (txid);
    }

    std::string inline write (const Bitcoin::outpoint &o) {
        std::stringstream ss;
        ss << write (o.Digest) << ":" << o.Index;
        return ss.str ();
    }

    Bitcoin::TxID inline read_TxID (string_view x) {
        maybe<Bitcoin::TxID> t = data::encoding::read<Bitcoin::TxID> {} (x);
        if (!t) throw data::exception {} << "invalid txid " << x;
        return *t;
    }

    Bitcoin::outpoint inline read_outpoint (string_view x) {
        maybe<Bitcoin::outpoint> t = data::encoding::read<Bitcoin::outpoint> {} (x);
        if (!t) throw data::exception {} << "invalid outpoint " << x;
        return *t;
    }

    JSON inline write (const Bitcoin::satoshi &j) {
        return JSON (int64 (j));
    }

    Bitcoin::satoshi inline read_satoshi (const JSON &j) {
        return Bitcoin::satoshi (int64 (j));
    }

    Bitcoin::output inline read_output (const JSON &j) {
        return Bitcoin::output {read_satoshi (j["value"]), *encoding::hex::read (std::string (j["script"]))};
    }

    Bitcoin::input inline read_input (const JSON &j) {
        return Bitcoin::input {read_outpoint (std::string (j["reference"])),
            *encoding::hex::read (std::string (j["script"])), uint32 (j["sequence"])};
    }

    JSON inline write (const Bitcoin::prevout &p) {
        return JSON::object_t {{write (p.Key), write (p.Value)}};
    }
}

#endif
