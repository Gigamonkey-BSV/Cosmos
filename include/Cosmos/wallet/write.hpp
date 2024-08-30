#ifndef COSMOS_WALLET_WRITE
#define COSMOS_WALLET_WRITE

#include <gigamonkey/timechain.hpp>
#include <gigamonkey/schema/hd.hpp>
#include <data/net/JSON.hpp>
#include <Cosmos/network.hpp>

namespace Cosmos {
    using namespace data;
    namespace Bitcoin = Gigamonkey::Bitcoin;
    namespace SPV = Gigamonkey::SPV;
    namespace HD = Gigamonkey::HD;

    std::string write (const Bitcoin::TXID &);
    Bitcoin::TXID read_txid (string_view);
    std::string write (const Bitcoin::outpoint &);
    Bitcoin::outpoint read_outpoint (const string &);
    std::string write (const N &);
    N read_N (const string &);

    std::string write (const Bitcoin::header &h);
    Bitcoin::header read_header (const string &j);

    JSON write (const Bitcoin::output &j);
    Bitcoin::output read_output (const JSON &j);

    void write_to_file (const JSON &, const std::string &filename);
    JSON read_from_file (const std::string &filename);

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

    Bitcoin::TXID inline read_txid (string_view x) {
        Bitcoin::TXID t {std::string {x}};
        if (!t.valid ()) throw exception {} << "invalid txid " << x;
        return t;
    }

    Bitcoin::output inline read_output (const JSON &j) {
        return Bitcoin::output {int64 (j["value"]), *encoding::hex::read (std::string (j["script"]))};
    }
}

#endif
