#ifndef SERVER_KEY
#define SERVER_KEY

#include <data/maybe.hpp>
#include <data/bytes.hpp>
#include <data/net/HTTP.hpp>
#include "server.hpp"

net::HTTP::response handle_key (server &p,
    const Diophant::symbol &wallet_name,
    net::HTTP::method http_method, map<UTF8, UTF8> query,
    const maybe<net::HTTP::content> &content_type,
    const data::bytes &body);

// if key_type is set, then we generate a random key.
enum class key_type {
    unset,
    secp256k1,
    WIF,
    xpriv
};

std::ostream &operator << (std::ostream &, key_type);

net::HTTP::request get_key_request (const std::string &wallet_name, const std::string &key_name);
net::HTTP::request post_key_request (const std::string &wallet_name, const std::string &key_name, key_type);
net::HTTP::request post_key_request (const std::string &wallet_name, const std::string &key_name, const key_expression &expr);

struct key_request_options {
    const std::string WalletName {};
    const std::string KeyName {};
    maybe<::key_type> KeyType {};

    maybe<net::HTTP::method> HTTPMethod {};
    maybe<key_expression> Body {};
    Bitcoin::net Net {Bitcoin::net::Main};

    key_request_options (const std::string &wallet_name, const std::string &key_name);

    key_request_options &get ();
    key_request_options &post ();

    key_request_options &key_type (::key_type);
    key_request_options &body (const key_expression &k);
    key_request_options &net (Bitcoin::net);

    operator net::HTTP::request () const;
};

net::HTTP::request inline get_key_request (const std::string &wallet_name, const std::string &key_name) {
    return key_request_options {wallet_name, key_name}.get ();
}

net::HTTP::request inline post_key_request (const std::string &wallet_name, const std::string &key_name, key_type kt) {
    return key_request_options {wallet_name, key_name}.post ().key_type (kt);
}

net::HTTP::request inline post_key_request (const std::string &wallet_name, const std::string &key_name, const key_expression &expr) {
    return key_request_options {wallet_name, key_name}.post ().body (expr);
}

inline key_request_options::key_request_options (const std::string &wallet_name, const std::string &name):
    WalletName {wallet_name}, KeyName {name} {}

key_request_options inline &key_request_options::get () {
    HTTPMethod = net::HTTP::method::get;
    return *this;
}

key_request_options inline &key_request_options::post () {
    HTTPMethod = net::HTTP::method::post;
    return *this;
}

key_request_options inline &key_request_options::key_type (::key_type kk) {
    KeyType = kk;
    return *this;
}

key_request_options inline &key_request_options::body (const key_expression &k) {
    Body = k;
    return *this;
}

#endif

