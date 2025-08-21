#ifndef SERVER_INVERT_HASH
#define SERVER_INVERT_HASH

#include <data/maybe.hpp>
#include <data/bytes.hpp>
#include <data/net/HTTP.hpp>
#include "server.hpp"

enum class digest_format {
    unset,
    HEX,
    BASE58_CHECK,
    BASE64
};

std::ostream &operator << (std::ostream &, digest_format);

net::HTTP::response invert_hash (server &p,
    net::HTTP::method http_method, map<UTF8, UTF8> query,
    const maybe<net::HTTP::content> &content_type,
    const data::bytes &body);

#endif
