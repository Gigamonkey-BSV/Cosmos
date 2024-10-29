#ifndef COSMOS_DATABASE_WRITE
#define COSMOS_DATABASE_WRITE

#include <data/net/JSON.hpp>
#include <data/crypto/encrypted.hpp>

#include <gigamonkey/timechain.hpp>
#include <gigamonkey/schema/hd.hpp>

#include <Cosmos/network.hpp>

// provide standard ways of converting certain types into strings and back.
namespace Cosmos {
    using namespace data;
    namespace Bitcoin = Gigamonkey::Bitcoin;
    namespace SPV = Gigamonkey::SPV;
    namespace HD = Gigamonkey::HD;

    std::string write (const Bitcoin::TXID &);
    Bitcoin::TXID read_TXID (string_view);
    std::string write (const Bitcoin::outpoint &);
    Bitcoin::outpoint read_outpoint (const string &);
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

    JSON write_path (HD::BIP_32::path p);

    HD::BIP_32::path read_path (const JSON &j);

    void write_to_file (const std::string &, const std::string &filename);

    void inline write_to_file (const JSON &j, const std::string &filename) {
        write_to_file (j.dump (' ', 2), filename);
    }

    struct file {
        JSON Payload;
        // If included, then the file is encrypted.
        maybe<crypto::symmetric_key<32>> Key;
        file (JSON &&j): Payload {j}, Key {} {}
        file (JSON &&j, const crypto::symmetric_key<32> &k): Payload {j}, Key {k} {}
    };

    void write_to_file (const file &, const std::string &filename);
    file read_from_file (const std::string &filename);

    std::string inline write (const N &n) {
        return encoding::decimal::write (n);
    }

    N inline read_N (const string &x) {
        return N {x};
    }

    std::string inline write (const Bitcoin::TXID &txid) {
        return encoding::hexidecimal::write (txid);
    }

    std::string inline write (const Bitcoin::outpoint &o) {
        std::stringstream ss;
        ss << write (o.Digest) << ":" << o.Index;
        return ss.str ();
    }

    Bitcoin::TXID inline read_TXID (string_view x) {
        Bitcoin::TXID t {std::string {x}};
        if (!t.valid ()) throw exception {} << "invalid txid " << x;
        return t;
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
        return Bitcoin::input {read_outpoint (j["reference"]), *encoding::hex::read (std::string (j["script"])), uint32 (j["sequence"])};
    }
}

#endif
