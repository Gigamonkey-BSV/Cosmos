#include "../source/server/method.hpp"
#include "../source/server/server.hpp"
#include "../source/server/key.hpp"
#include <data/net/error.hpp>
#include "gtest/gtest.h"

std::atomic<bool> Shutdown {false};

int arg_count = 0;
const char *const arg_values[] = {};

server get_test_server () {
    Shutdown = false;
    return server {options {arg_parser {arg_count, arg_values}}};
}

net::HTTP::request make_shutdown_request ();
net::error read_error (const net::HTTP::response &);

TEST (ServerTest, TestShutdown) {
    auto test_server = get_test_server ();
    EXPECT_FALSE (read_error (data::synced (&server::operator (), &test_server, make_shutdown_request ())).valid ());
    EXPECT_TRUE (Shutdown);
}

net::HTTP::request make_generate_random_key_request (const std::string &, key_type = key_type::unset);
net::HTTP::request make_set_entropy_request (const string &entropy);

TEST (ServerTest, TestEntropy) {
    auto test_server = get_test_server ();

    // fail to generate a random key.
    EXPECT_TRUE (read_error (data::synced (&server::operator (), &test_server, make_generate_random_key_request ("X"))).valid ());

    // initialize random number generator.
    EXPECT_FALSE (read_error (data::synced (&server::operator (), &test_server, make_set_entropy_request ("abcd"))).valid ());

    // suceed at generating a random key
    EXPECT_FALSE (read_error (data::synced (&server::operator (), &test_server, make_generate_random_key_request ("X"))).valid ());
}

server prepare (server, const string &entropy = "abcdwxyz");

TEST (ServerTest, TestKey) {
    auto test_server = prepare (get_test_server ());
    EXPECT_FALSE (read_error (data::synced (&server::operator (), &test_server, make_generate_random_key_request ("X"))).valid ());
}

TEST (ServerTest, TestInvertHash) {
    auto test_server = prepare (get_test_server ());
}

TEST (ServerTest, TestToPrivate) {
    auto test_server = prepare (get_test_server ());
}

TEST (ServerTest, TestWallet) {
    auto test_server = prepare (get_test_server ());
    // make a wallet.
    // Add key sequence to wallet
    // generate next keys of key sequences.
}

server prepare (server x, const string &entropy) {
    data::synced (&server::operator (), &x, make_set_entropy_request ("entropy"));
    return x;
}

net::HTTP::request make_shutdown_request () {
    return net::HTTP::request (net::HTTP::request::make {}.method (net::HTTP::method::get).path ("/shutdown"));
}

net::error read_error (const net::HTTP::response &r) {
    if (auto ct = r.content_type (); !bool (ct) || *ct != "application/problem+json") return {JSON ()};
    return {JSON::parse (r.Body)};
}

net::HTTP::request make_set_entropy_request (const string &entropy) {
    return net::HTTP::request (
        net::HTTP::request::make {}.method (
            net::HTTP::method::post
        ).path (
            "/set_entropy"
        ).body (bytes (entropy)));
}

net::HTTP::request make_generate_random_key_request (const std::string &x, key_type k) {
    return net::HTTP::request (net::HTTP::request::make ().method (net::HTTP::method::post).path ("/key").query (
        data::string::write ("method=random&type=", k, "&name=", k)));
}

// we will get linker errors if we don't provide these functions but we won't actually use them.
net::HTTP::response HTML_JS_UI_response () {
    return ok_response ();
}

std::ostream &version (std::ostream &o) {
    return o;
}

std::ostream &help (std::ostream &o, method meth) {
    return o;
}
