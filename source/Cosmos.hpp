#ifndef COSMOS
#define COSMOS

#include <data/io/arg_parser.hpp>
#include <data/io/error.hpp>
#include <Cosmos/types.hpp>
#include "method.hpp"

std::string version ();

std::string help (method m = method::UNSET);

net::HTTP::request inline request_shutdown (const UTF8 &host = "localhost") {
    return net::HTTP::request::make {}.method (
        net::HTTP::method::put
    ).path (
        "/shutdown"
    ).host (host);
}


net::HTTP::request inline request_add_entropy (const string &entropy, const UTF8 &host = "localhost") {
    return net::HTTP::request::make {}.method (
        net::HTTP::method::post
    ).path (
        "/add_entropy"
    ).body (bytes (entropy)).host (host);
}

net::HTTP::request inline request_list_wallets (const UTF8 &host = "localhost") {
    return net::HTTP::request::make ().path ("/list_wallets").method (net::HTTP::method::get).host (host);
}

net::HTTP::request request_value (const UTF8 &host = "localhost");

namespace args = data::io::args;

net::HTTP::request request_generate (const args::parsed &);
net::HTTP::request request_update (const args::parsed &);
net::HTTP::request request_restore (const args::parsed &);
net::HTTP::request request_value (const args::parsed &);
net::HTTP::request request_request (const args::parsed &);
net::HTTP::request request_accept (const args::parsed &);
net::HTTP::request request_pay (const args::parsed &);
net::HTTP::request request_sign (const args::parsed &);
net::HTTP::request request_spend (const args::parsed &);
net::HTTP::request request_import (const args::parsed &);
net::HTTP::request request_split (const args::parsed &);
net::HTTP::request request_taxes (const args::parsed &);
net::HTTP::request request_encrypt_private_keys (const args::parsed &);
net::HTTP::request request_decrypt_private_keys (const args::parsed &);

void command_generate (const args::parsed &); // offline
void command_update (const args::parsed &);
void command_restore (const args::parsed &);
void command_value (const args::parsed &);    // offline
void command_request (const args::parsed &);  // offline
void command_accept (const args::parsed &);   // offline
void command_pay (const args::parsed &);      // offline
void command_sign (const args::parsed &);     // offline
void command_send (const args::parsed &);     // depricated
void command_import (const args::parsed &);
void command_boost (const args::parsed &);    // offline
void command_split (const args::parsed &);
void command_taxes (const args::parsed &);    // offline
void command_encrypt_private_keys (const args::parsed &);    // offline
void command_decrypt_private_keys (const args::parsed &);    // offline

#endif
