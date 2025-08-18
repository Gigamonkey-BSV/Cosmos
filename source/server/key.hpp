#ifndef SERVER_KEY
#define SERVER_KEY

#include <data/maybe.hpp>
#include <data/bytes.hpp>
#include <data/net/HTTP.hpp>
#include "server.hpp"

enum class key_generation_method {
    random,
    expression
};

enum class key_type {
    unset,
    secp256k1,
    WIF,
    xpriv
};

std::ostream &operator << (std::ostream &, key_type);

net::HTTP::response key (server &p,
    net::HTTP::method http_method, map<UTF8, UTF8> query,
    const maybe<net::HTTP::content> &content_type,
    const data::bytes &body);

#endif

