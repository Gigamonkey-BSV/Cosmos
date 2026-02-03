#ifndef SERVER_IMPORT
#define SERVER_IMPORT

#include <data/maybe.hpp>
#include <data/bytes.hpp>
#include <data/net/HTTP.hpp>
#include "server.hpp"
#include <Cosmos/network.hpp>

// * wif         -- wif or list of wifs.
//                  if nothing else is given, we search online for addresses.
// * txid        -- txid to import
// * index       -- index of output to import.
// * script_code -- if we are importing some weird output, there may be a script code.

bool import_output (Cosmos::database &db, Cosmos::network &net, const Cosmos::redeemable &);
bool import_output (Cosmos::database &db, Cosmos::network &net, const Bitcoin::WIF &);

net::HTTP::request inline request_import (const args::parsed &p, const UTF8 & = "localhost") {
    throw data::method::unimplemented {"request import"};
}

net::HTTP::response handle_import (server &p,
    const Diophant::symbol &wallet_name,
    map<UTF8, UTF8> query,
    const maybe<net::HTTP::content> &content_type,
    const data::bytes &body);

#endif

