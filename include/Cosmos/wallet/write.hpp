#ifndef COSMOS_WALLET_WRITE
#define COSMOS_WALLET_WRITE

#include <gigamonkey/timechain.hpp>
#include <data/net/JSON.hpp>
#include <Cosmos/network.hpp>

namespace Cosmos {
    using namespace data;
    namespace Bitcoin = Gigamonkey::Bitcoin;

    std::string write (const Bitcoin::txid &);
    Bitcoin::txid read_txid (const string &);
    std::string write (const Bitcoin::outpoint &);
    Bitcoin::outpoint read_outpoint (const string &);

    JSON write (const Bitcoin::output &j);
    Bitcoin::output read_output (const JSON &j);

    void write_to_file (const JSON &, const std::string &filename);

    JSON read_from_file (const std::string &filename);
}

#endif
