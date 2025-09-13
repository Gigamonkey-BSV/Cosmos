#ifndef SERVER_TO_PRIVATE
#define SERVER_TO_PRIVATE

#include <data/maybe.hpp>
#include <data/bytes.hpp>
#include <data/net/HTTP.hpp>
#include "server.hpp"

net::HTTP::response handle_to_private (
    server &p, net::HTTP::method http_method, map<UTF8, UTF8> query,
    const maybe<net::HTTP::content> &content_type, const data::bytes &body);

#endif
