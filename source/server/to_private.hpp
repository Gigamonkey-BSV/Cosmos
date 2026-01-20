#ifndef SERVER_TO_PRIVATE
#define SERVER_TO_PRIVATE

#include <data/maybe.hpp>
#include <data/bytes.hpp>
#include <data/net/HTTP.hpp>
#include "server.hpp"

string escaped (const string &);

net::HTTP::request inline make_to_private_get_request (const key_expression &k) {
    return net::HTTP::request (
        net::HTTP::request::make {}.method (
            net::HTTP::method::get
        ).path (
            "/to_private"
        ).query_map ({{"key", k}}).host ("localhost"));
}

net::HTTP::request inline make_to_private_put_request (const key_expression &k, const key_expression &b) {
    return net::HTTP::request (
        net::HTTP::request::make {}.method (
            net::HTTP::method::put
        ).path (
            "/to_private"
        ).query_map ({{"key", k}}).body (b).host ("localhost"));
}

net::HTTP::response handle_to_private (
    server &p, net::HTTP::method http_method, map<UTF8, UTF8> query,
    const maybe<net::HTTP::content> &content_type, const data::bytes &body);

#endif
