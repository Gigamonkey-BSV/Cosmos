#ifndef SERVER_SPLIT
#define SERVER_SPLIT

#include <data/net/HTTP.hpp>
#include "server.hpp"

struct split_request_options {
    // read from a generate request.
    split_request_options (
        Diophant::symbol wallet_name, map<UTF8, UTF8> query,
        const maybe<net::HTTP::content> &content_type,
        const data::bytes &body);

    split_request_options (const args::parsed &) {
        throw data::method::unimplemented {"split options from input args"};
    }

    net::HTTP::request request (const UTF8 & = "localhost") const {
        throw data::method::unimplemented {"make HTTP request from split options"};
    }
};

net::HTTP::response handle_split (
    server &p, net::HTTP::method http_method, map<UTF8, UTF8> query,
    const maybe<net::HTTP::content> &content_type, const data::bytes &body);

#endif

