#ifndef SERVER_SPEND
#define SERVER_SPEND

#include "server.hpp"
#include <Cosmos/options.hpp>

net::HTTP::response handle_spend (
    server &p, net::HTTP::method http_method, dispatch<UTF8, UTF8> query,
    const maybe<net::HTTP::content> &content_type, const data::bytes &body);

#endif

