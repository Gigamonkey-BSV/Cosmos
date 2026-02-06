#ifndef COSMOS
#define COSMOS

#include <data/io/arg_parser.hpp>
#include <data/io/error.hpp>
#include <data/net/URL.hpp>
#include <Cosmos/REST/method.hpp>
#include <Cosmos/REST/problem.hpp>
#include <Cosmos/Diophant.hpp>

namespace args = data::io::args;
namespace schema = data::schema;

std::string version ();

std::string help (Cosmos::method m = Cosmos::method::UNSET);

std::ostream &version (std::ostream &o);

std::ostream &help (std::ostream &o, Cosmos::method meth = Cosmos::method::UNSET);

net::HTTP::response error_response (unsigned int status, Cosmos::method m, Cosmos::problem, const std::string & = "");

net::HTTP::response help_response (Cosmos::method = Cosmos::method::UNSET);

net::HTTP::response version_response ();

maybe<net::error> read_error_response (const net::HTTP::response &r);

bool read_ok_response (const net::HTTP::response &r);

maybe<string> read_string_response (const net::HTTP::response &r);

maybe<JSON> read_JSON_response (const net::HTTP::response &r);

std::string inline version () {
    std::stringstream ss;
    version (ss);
    return ss.str ();
}

std::string inline help (Cosmos::method m) {
    std::stringstream ss;
    help (m);
    return ss.str ();
}

net::HTTP::request request_request (const args::parsed &);
net::HTTP::request request_accept (const args::parsed &);
net::HTTP::request request_pay (const args::parsed &);
net::HTTP::request request_sign (const args::parsed &);
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
