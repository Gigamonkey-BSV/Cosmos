#ifndef SERVER_SERVER
#define SERVER_SERVER

#include "options.hpp"
#include <Diophant/machine.hpp>
#include <Cosmos/network.hpp>
#include <Cosmos/random.hpp>

using UTF8 = data::UTF8;

namespace secp256k1 = Gigamonkey::secp256k1;
namespace HD = Gigamonkey::HD;

using key_expression = Cosmos::key_expression;
using key_derivation = Cosmos::key_derivation;
using key_sequence = Cosmos::key_sequence;

struct server {

    Cosmos::spend_options SpendOptions;

    Cosmos::network *Net;
    database &DB;

    Cosmos::random::user_entropy *UserEntropy;

    server (const Cosmos::spend_options &x, database &db, Cosmos::network *net, Cosmos::random::user_entropy *ue):
        SpendOptions {x}, Net {net}, DB {db}, UserEntropy {ue} {}

    // handle an HTTP request.
    awaitable<net::HTTP::response> operator () (const net::HTTP::request &);

    struct make_wallet_options {};

    bool make_wallet (const make_wallet_options &);

    awaitable<Bitcoin::satoshi> value (const UTF8 &wallet_name);

    struct wallet_info {
        Bitcoin::satoshi Value;
        uint32 Outputs;
        Bitcoin::satoshi MaxOutput;

        operator JSON () const;
    };

    awaitable<wallet_info> details (const UTF8 &wallet_name);
/*
    map<Bitcoin::outpoint, Bitcoin::output> account (const instruction &);

    tuple<Bitcoin::TXID, wallet_info> spend (const instruction &);*/

};

bool parse_uint32 (const std::string &str, uint32_t &result);

net::HTTP::response favicon ();
net::HTTP::response HTML_JS_UI_response ();
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

#endif
