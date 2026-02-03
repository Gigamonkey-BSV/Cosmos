#ifndef SERVER_SPEND
#define SERVER_SPEND

#include "server.hpp"
#include <Cosmos/options.hpp>

struct spend_request_options : Cosmos::spend_options {

    spend_request_options (const args::parsed &) {
        throw data::method::unimplemented {"parsed -> spend_request_options"};
    }

    spend_request_options (map<UTF8, UTF8> query,
        const maybe<net::HTTP::content> &content_type, const data::bytes &body);

    net::HTTP::request request (const UTF8 & = "localhost") const {
        throw data::method::unimplemented {"spend_request_options -> HTTP request"};
    }

};

net::HTTP::response handle_spend (
    server &p, net::HTTP::method http_method, map<UTF8, UTF8> query,
    const maybe<net::HTTP::content> &content_type, const data::bytes &body);

#endif

