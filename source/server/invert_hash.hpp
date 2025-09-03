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

template <size_t size>
net::HTTP::request get_invert_hash_request (
    const data::digest<size> &,
    digest_format ff = digest_format::BASE64,
    Cosmos::hash_function = Cosmos::hash_function::invalid);

net::HTTP::request put_invert_hash_request (Cosmos::hash_function, const bytes &data);

net::HTTP::request get_invert_hash_request (const Bitcoin::address &address);

net::HTTP::request put_invert_hash_request (const Bitcoin::pubkey &pub, const Bitcoin::address &address);

template <size_t size>
net::HTTP::request put_invert_hash_request (
    Cosmos::hash_function, const bytes &data,
    const data::digest<size> &,
    digest_format ff = digest_format::BASE64);

struct invert_hash_request_options {
    // either GET or PUT
    maybe<net::HTTP::method> Method {};
    Cosmos::hash_function Function {Cosmos::hash_function::invalid};

    // unused if digest format is not base58 check. The value is ignored in any case.
    byte Base58CheckVersion {0x00};

    maybe<either<bytes, std::string>> Digest;
    ::digest_format Format {::digest_format::unset};

    maybe<bytes> Data;

    invert_hash_request_options () {}

    invert_hash_request_options &get ();
    invert_hash_request_options &put ();

    invert_hash_request_options &digest (const bytes &, ::digest_format);
    invert_hash_request_options &digest (const std::string &, ::digest_format);
    invert_hash_request_options &function (Cosmos::hash_function);
    invert_hash_request_options &base58_check_version (byte);
    invert_hash_request_options &data (const bytes &);

    operator net::HTTP::request () const;
};

net::HTTP::response handle_invert_hash (server &p,
    net::HTTP::method http_method, map<UTF8, UTF8> query,
    const maybe<net::HTTP::content> &content_type,
    const data::bytes &body);

invert_hash_request_options inline &invert_hash_request_options::get () {
    Method = net::HTTP::method::get;
    return *this;
}

invert_hash_request_options inline &invert_hash_request_options::put () {
    Method = net::HTTP::method::put;
    return *this;
}

invert_hash_request_options inline &invert_hash_request_options::function (Cosmos::hash_function f) {
    Function = f;
    return *this;
}

invert_hash_request_options inline &invert_hash_request_options::data (const bytes &dd) {
    Data = dd;
    return *this;
}

invert_hash_request_options inline &invert_hash_request_options::digest (const std::string &dd, ::digest_format ff) {
    Digest = dd;
    Format = ff;
    return *this;
}

invert_hash_request_options inline &invert_hash_request_options::digest (const bytes &dd, ::digest_format ff) {
    Digest = dd;
    Format = ff;
    return *this;
}

template <size_t size>
net::HTTP::request inline get_invert_hash_request (
    const data::digest<size> &dig, digest_format ff, Cosmos::hash_function f) {

    auto req = invert_hash_request_options {}.get ().digest (dig, ff);
    if (f != Cosmos::hash_function::invalid) req.function (f);
    return req;
}

net::HTTP::request inline put_invert_hash_request (Cosmos::hash_function f, const bytes &data) {
    return invert_hash_request_options {}.put ().function (f).data (data);
}

template <size_t size>
net::HTTP::request inline put_invert_hash_request (
    Cosmos::hash_function f, const bytes &data,
    const data::digest<size> &dig,
    digest_format ff) {
    return invert_hash_request_options {}.put ().function (f).data (data).digest (dig, ff);
}

net::HTTP::request inline get_invert_hash_request (const Bitcoin::address &address) {
    return invert_hash_request_options {}.get ().function (
        Cosmos::hash_function::Hash160
    ).digest (address, digest_format::BASE58_CHECK);
}

net::HTTP::request inline put_invert_hash_request (const Bitcoin::pubkey &pub, const Bitcoin::address &address) {
    return invert_hash_request_options {}.put ().function (
        Cosmos::hash_function::Hash160
    ).digest (address, digest_format::BASE58_CHECK).data (pub);
}

#endif
