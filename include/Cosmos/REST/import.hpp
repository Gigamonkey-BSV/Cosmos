#ifndef SERVER_REST_IMPORT
#define SERVER_REST_IMPORT

#include <data/maybe.hpp>
#include <data/bytes.hpp>
#include <data/io/arg_parser.hpp>
#include <gigamonkey/pay/BEEF.hpp>
#include <Cosmos/Diophant.hpp>
#include <Cosmos/network.hpp>

namespace schema = data::schema;

namespace args = data::io::args;

using BEEF = Gigamonkey::BEEF;

namespace Cosmos {

    struct import_request_options {
        Diophant::symbol Name;

        using tx = either<Bitcoin::TxID, Bitcoin::transaction, BEEF>;
        list<tx> Txs;
        list<Bitcoin::outpoint> Outpoints;
        list<Bitcoin::WIF> Keys;

        import_request_options () {}

        // read from HTTP request
        import_request_options (
            const Diophant::symbol &wallet_name,
            const dispatch<UTF8, UTF8> query,
            const maybe<net::HTTP::content> &content_type,
            const data::bytes &body);

        bool valid () const;

        import_request_options (const args::parsed &p);
    };
}

#endif

