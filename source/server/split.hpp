#ifndef SERVER_SPLIT
#define SERVER_SPLIT

#include <data/net/HTTP.hpp>
#include "server.hpp"


net::HTTP::response handle_split (
    server &p, net::HTTP::method http_method, dispatch<UTF8, UTF8> query,
    const maybe<net::HTTP::content> &content_type, const data::bytes &body);

#endif

