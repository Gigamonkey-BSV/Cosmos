#ifndef SERVER_REST_IMPORT
#define SERVER_REST_IMPORT

#include <data/maybe.hpp>
#include <data/bytes.hpp>
#include <data/io/arg_parser.hpp>
#include <gigamonkey/pay/BEEF.hpp>
#include <Cosmos/Diophant.hpp>

namespace schema = data::schema;

using BEEF = Gigamonkey::BEEF;

namespace Cosmos {

    struct import_request_options {
        Diophant::symbol Name;

        using tx = either<Bitcoin::TxID, Bitcoin::transaction, BEEF>;
        list<tx> Txs;
        list<Bitcoin::outpoint> Outpoints;
        list<Bitcoin::WIF> Keys;

        import_request_options () {}

        import_request_options (Diophant::symbol wallet_name, map<std::string, list<std::string>> args);

        bool valid () const;


        import_request_options (const args::parsed &) {
            throw data::method::unimplemented {"parsed -> import_request_options"};
        }

        import_request_options (Diophant::symbol wallet_name, map<UTF8, UTF8> query);
    };
}

#endif

