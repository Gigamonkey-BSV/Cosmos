#ifndef COSMOS
#define COSMOS

#include <data/io/arg_parser.hpp>
#include <data/io/error.hpp>
#include <data/net/URL.hpp>
#include <Cosmos/types.hpp>
#include "method.hpp"
#include "server/problem.hpp"
#include <Cosmos/Diophant.hpp>

namespace args = data::io::args;
namespace schema = data::schema;

std::string version ();

std::string help (method m = method::UNSET);

std::ostream &version (std::ostream &o);

std::ostream &help (std::ostream &o, method meth = method::UNSET);

net::HTTP::request request_shutdown (const UTF8 &host = "localhost");

net::HTTP::request request_add_entropy (const string &entropy, const UTF8 &host = "localhost");

net::HTTP::request request_list_wallets (const UTF8 &host = "localhost");

net::HTTP::request request_value (const Diophant::symbol &wallet_name, const UTF8 &host = "localhost");

auto inline prefix (method m) {
    return schema::list::value<std::string> () + schema::list::equal<method> (m);
}

auto inline no_params (method m) {
    return args::command (set<std::string> {}, prefix (m), schema::map::empty ());
}

auto inline call_options () {
    return +*schema::map::key<net::authority> ("authority") ||
        +*schema::map::key<net::domain_name> ("domain") ||
        +(*schema::map::key<net::IP::address> ("ip_address") && *schema::map::key<uint32> ("port"));
}

struct request_next_options {
    Diophant::symbol Name;
    maybe<Diophant::symbol> Sequence;
    request_next_options () {}
    request_next_options (const args::parsed &);
    request_next_options (Diophant::symbol wallet_name, map<UTF8, UTF8> query):
        Name {wallet_name}, Sequence {schema::validate<> (query,
            *schema::map::key<Diophant::symbol> ("sequence"))} {}

    net::HTTP::request request (const UTF8 &host = "localhost") const;

    request_next_options &name (const Diophant::symbol &name) {
        Name = name;
        return *this;
    }

    request_next_options &sequence (const Diophant::symbol &sequence) {
        Sequence = sequence;
        return *this;
    }
};

net::HTTP::response error_response (unsigned int status, method m, problem, const std::string & = "");

net::HTTP::response help_response (method = method::UNSET);

net::HTTP::response version_response ();

net::HTTP::response inline ok_response () {
    return net::HTTP::response (204);
}

net::HTTP::response inline JSON_response (const JSON &j) {
    return net::HTTP::response (200, {{"content-type", "application/json"}}, bytes (data::string (j.dump ())));
}

net::HTTP::response inline string_response (const string &str) {
    return net::HTTP::response (200, {{"content-type", "text/plain"}}, bytes (str));
}

net::HTTP::response inline data_response (const bytes &str) {
    return net::HTTP::response (200, {{"content-type", "application/octet-stream"}}, str);
}

net::HTTP::response inline boolean_response (bool b) {
    return JSON_response (b);
}

net::HTTP::response inline value_response (Bitcoin::satoshi x) {
    return JSON_response (JSON (int64 (x)));
}

maybe<net::error> read_error_response (const net::HTTP::response &r);

bool read_ok_response (const net::HTTP::response &r);

maybe<string> read_string_response (const net::HTTP::response &r);

maybe<JSON> read_JSON_response (const net::HTTP::response &r);

std::string inline version () {
    std::stringstream ss;
    version (ss);
    return ss.str ();
}

std::string inline help (method m) {
    std::stringstream ss;
    help (m);
    return ss.str ();
}

net::HTTP::request inline request_shutdown (const UTF8 &host) {
    return net::HTTP::request::make {}.method (
        net::HTTP::method::put
    ).path (
        "/shutdown"
    ).host (host);
}

net::HTTP::request inline request_add_entropy (const string &entropy, const UTF8 &host) {
    return net::HTTP::request::make {}.method (
        net::HTTP::method::post
    ).path (
        "/add_entropy"
    ).body (bytes (entropy)).host (host);
}

net::HTTP::request inline request_list_wallets (const UTF8 &host) {
    return net::HTTP::request::make ().path ("/list_wallets").method (net::HTTP::method::get).host (host);
}

net::HTTP::request inline request_value (const Diophant::symbol &wallet_name, const UTF8 &host) {
    return net::HTTP::request::make ().path (string::write ("/value/", wallet_name)).method (net::HTTP::method::get).host (host);
}

inline request_next_options::request_next_options (const args::parsed &p) {
    auto [name, sequence_name] = std::get<2> (
        args::validate (p,
            args::command {
                set<std::string> {},
                prefix (method::NEXT),
                schema::map::key<Diophant::symbol> ("name") &&
                *schema::map::key<Diophant::symbol> ("sequence")}));

    Name = name;
    Sequence = sequence_name;
}

net::HTTP::request inline request_next_options::request (const UTF8 &host) const {
    auto m = net::HTTP::request::make ().method (
        net::HTTP::method::post
    ).path (
        string::write ("/next/", Name)
    ).host (host);
    if (Sequence) return m.query_map ({{"sequence", UTF8 (*Sequence)}});
    return m;
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
