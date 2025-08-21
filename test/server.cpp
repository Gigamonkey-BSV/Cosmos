#include "../source/server/method.hpp"
#include "../source/server/server.hpp"
#include "../source/server/key.hpp"
#include "../source/server/generate.hpp"
#include "../source/server/invert_hash.hpp"
#include <data/net/JSON.hpp>
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

bool is_error (const net::HTTP::response &r) {
    return read_error (r).valid ();
}

net::HTTP::response make_request (server &x, const net::HTTP::request &r) {
    return data::synced (&server::operator (), &x, r);
}

TEST (ServerTest, TestShutdown) {
    auto test_server = get_test_server ();
    EXPECT_FALSE (is_error (make_request (test_server, make_shutdown_request ())));
    EXPECT_TRUE (Shutdown);
}

net::HTTP::request make_generate_random_key_request (const std::string &, key_type = key_type::unset);
net::HTTP::request make_set_entropy_request (const string &entropy);
bool is_bool_response (bool expected, const net::HTTP::response &);

TEST (ServerTest, TestEntropy) {
    auto test_server = get_test_server ();

    // fail to generate a random key.
    ASSERT_TRUE (is_error (make_request (test_server,  make_generate_random_key_request ("X"))));

    // initialize random number generator.
    auto initialize_response = make_request (test_server, make_set_entropy_request ("abcd"));
    ASSERT_FALSE (is_error (initialize_response));
    ASSERT_TRUE (is_bool_response (true, initialize_response));

    // suceed at generating a random key
    EXPECT_FALSE (is_error (make_request (test_server, make_generate_random_key_request ("X"))));
}

server prepare (server, const string &entropy = "abcdwxyz");

template <size_t x> net::HTTP::request request_get_invert_hash (const data::crypto::digest<x> &d, digest_format format = digest_format::HEX);
template <size_t x> net::HTTP::request request_post_invert_hash (Cosmos::hash_function, const std::string &data);
template <size_t x> maybe<data::crypto::digest<x>> read_digest (const net::HTTP::response &);

TEST (ServerTest, TestInvertHash) {
    auto test_server = prepare (get_test_server ());
    std::string data_A = "Hi, this string will be hashed.";
    data::crypto::digest256 digest_A = data::crypto::SHA2_256 (data_A);

    // attempt to retrieve data before it has been entered.
    ASSERT_TRUE (is_error (make_request (test_server, request_get_invert_hash (digest_A))));

    EXPECT_TRUE (is_bool_response (true, make_request (test_server,
        request_post_invert_hash<32> (Cosmos::hash_function::SHA2_256, data_A))));

    EXPECT_EQ (digest_A, read_digest<32> (make_request (test_server, request_get_invert_hash (digest_A))));

}

net::HTTP::request make_retrieve_key_request (const std::string &key_name);

// TODO these are incomplete.
net::HTTP::request make_to_private_get_request (const key_expression &k) {
    return net::HTTP::request (net::HTTP::request::make {}.method (net::HTTP::method::get).path ("/to_private"));
}

net::HTTP::request make_to_private_post_request (const key_expression &k, const key_expression &) {
    return net::HTTP::request (net::HTTP::request::make {}.method (net::HTTP::method::post).path ("/to_private"));
}

key_expression read_key_expression (const net::HTTP::response &);

TEST (ServerTest, TestKey) {
    auto test_server = prepare (get_test_server ());

    // attempt to retrieve a key that hasn't been entered yet.
    EXPECT_TRUE (is_error (make_request (test_server, make_retrieve_key_request ("X"))));

    // generate keys of different types and retrieve them.
    EXPECT_TRUE (is_error (make_request (test_server, make_generate_random_key_request ("X", key_type::secp256k1))));
    EXPECT_TRUE (is_error (make_request (test_server, make_generate_random_key_request ("Y", key_type::WIF))));
    EXPECT_TRUE (is_error (make_request (test_server, make_generate_random_key_request ("Z", key_type::xpriv))));

    key_expression secret_X;
    key_expression secret_Y;
    key_expression secret_Z;

    EXPECT_NO_THROW (secret_X = read_key_expression (make_request (test_server, make_retrieve_key_request ("X"))));
    EXPECT_NO_THROW (secret_Y = read_key_expression (make_request (test_server, make_retrieve_key_request ("Y"))));
    EXPECT_NO_THROW (secret_Z = read_key_expression (make_request (test_server, make_retrieve_key_request ("Z"))));

    EXPECT_TRUE (data::valid (secret_X));
    EXPECT_TRUE (data::valid (secret_Y));
    EXPECT_TRUE (data::valid (secret_Z));

    // Test that to_private won't work if an entry hasn't been made yet.
    key_expression pubkey_X = key_expression {secp256k1::secret (secret_X).to_public ()};
    key_expression pubkey_Y = key_expression {Bitcoin::secret (secret_Y).to_public ()};
    key_expression pubkey_Z = key_expression {HD::BIP_32::secret (secret_Z).to_public ()};

    EXPECT_TRUE (is_error (make_request (test_server, make_to_private_get_request (pubkey_X))));

    // add entries for to_private
    EXPECT_FALSE (is_bool_response (true, make_request (test_server, make_to_private_post_request (pubkey_X, secret_X))));
    EXPECT_FALSE (is_bool_response (true, make_request (test_server, make_to_private_post_request (pubkey_Y, secret_Y))));
    EXPECT_FALSE (is_bool_response (true, make_request (test_server, make_to_private_post_request (pubkey_Z, secret_Z))));

    // TODO retrieve these.
}

using JSON = data::JSON;

net::HTTP::request make_list_wallets_request ();

net::HTTP::request make_next_address_request (const std::string &wallet_name, const std::string &sequence_name = "receive");

net::HTTP::request make_next_xpub_request (const std::string &wallet_name);

bool is_JSON_response (const JSON &, const net::HTTP::response &r);
maybe<std::string> read_string (const net::HTTP::response &r);

TEST (ServerTest, TestGenerate) {
    auto test_server = prepare (get_test_server ());

    // list_wallets should return an empty list.
    EXPECT_TRUE (is_JSON_response (JSON::array_t (), make_request (test_server, make_list_wallets_request ())));

    // generate a wallet of each type
    // we should simply get a bool response when we don't request the restoration words.
    EXPECT_TRUE (is_bool_response (true, make_request (test_server,
        generate_options {}.name ("A").wallet_style (wallet_style::BIP_44))));
    EXPECT_TRUE (is_bool_response (true, make_request (test_server,
        generate_options {}.name ("B").wallet_style (wallet_style::BIP_44_plus))));
    EXPECT_TRUE (is_bool_response (true, make_request (test_server,
        generate_options {}.name ("C").wallet_style (wallet_style::experimental))));

    maybe<std::string> maybe_words_D = read_string (make_request (test_server,
        generate_options {}.name ("D").wallet_style (wallet_style::BIP_44).mnemonic_style (mnemonic_style::BIP_39)));
    EXPECT_TRUE (bool (maybe_words_D));

    maybe<std::string> maybe_words_E = read_string (make_request (test_server,
        generate_options {}.name ("E").wallet_style (wallet_style::BIP_44_plus).mnemonic_style (mnemonic_style::BIP_39)));
    EXPECT_TRUE (bool (maybe_words_E));

    maybe<std::string> maybe_words_F = read_string (make_request (test_server,
        generate_options {}.name ("F").wallet_style (wallet_style::experimental).mnemonic_style (mnemonic_style::BIP_39)));
    EXPECT_TRUE (bool (maybe_words_F));

    // for each wallet, generate a new address of receive and change

    maybe<std::string> next_receive_A = read_string (make_request (test_server, make_next_address_request ("A", "receive")));
    maybe<std::string> next_change_A = read_string (make_request (test_server, make_next_address_request ("A", "change")));

    maybe<std::string> next_receive_B = read_string (make_request (test_server, make_next_address_request ("B", "receive")));
    maybe<std::string> next_change_B = read_string (make_request (test_server, make_next_address_request ("B", "change")));

    maybe<std::string> next_receive_C = read_string (make_request (test_server, make_next_address_request ("C", "receive")));
    maybe<std::string> next_change_C = read_string (make_request (test_server, make_next_address_request ("C", "change")));

    maybe<std::string> next_receive_D = read_string (make_request (test_server, make_next_address_request ("D", "receive")));
    maybe<std::string> next_change_D = read_string (make_request (test_server, make_next_address_request ("D", "change")));

    maybe<std::string> next_receive_E = read_string (make_request (test_server, make_next_address_request ("E", "receive")));
    maybe<std::string> next_change_E = read_string (make_request (test_server, make_next_address_request ("E", "change")));

    maybe<std::string> next_receive_F = read_string (make_request (test_server, make_next_address_request ("F", "receive")));
    maybe<std::string> next_change_F = read_string (make_request (test_server, make_next_address_request ("F", "change")));

    // TODO ensure that we can regenerate these from the words.

    // generate a new xpub where appropriate

    EXPECT_TRUE (is_error (make_request (test_server, make_next_xpub_request ("A"))));

    maybe<std::string> next_xpub_B = read_string (make_request (test_server, make_next_xpub_request ("B")));

    maybe<std::string> next_xpub_C = read_string (make_request (test_server, make_next_xpub_request ("C")));

    EXPECT_TRUE (is_error (make_request (test_server, make_next_xpub_request ("D"))));

    maybe<std::string> next_xpub_E = read_string (make_request (test_server, make_next_xpub_request ("E")));

    maybe<std::string> next_xpub_F = read_string (make_request (test_server, make_next_xpub_request ("F")));

    // TODO Generate the signing key where appropriate.

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

net::HTTP::request make_list_wallets_request () {
    return net::HTTP::request (net::HTTP::request::make ().path ("/list_wallets"));
}

net::HTTP::request make_next_address_request (const std::string &wallet_name, const std::string &sequence_name) {
    return net::HTTP::request (net::HTTP::request::make ().method (net::HTTP::method::post).path (string::write ("/next_address/", wallet_name)));
}

net::HTTP::request make_next_xpub_request (const std::string &wallet_name) {
    return net::HTTP::request (net::HTTP::request::make ().method (net::HTTP::method::post).path (string::write ("/next_xpub/", wallet_name)));
}

maybe<JSON> read_JSON_response (const net::HTTP::response &r) {
    try {
        return JSON::parse (string (r.Body));
    } catch (...) {
        return {};
    }
}

bool is_JSON_response (const JSON &expected, const net::HTTP::response &r) {
    maybe<JSON> result = read_JSON_response (r);
    if (!bool (result)) return false;
    return *result = expected;
}

maybe<std::string> read_string (const net::HTTP::response &r) {
    if (!r.content_type () == net::HTTP::content::type::text_plain) return {};
    return string (r.Body);
}

net::HTTP::request make_shutdown_request () {
    return net::HTTP::request (net::HTTP::request::make {}.method (net::HTTP::method::get).path ("/shutdown"));
}

net::error read_error (const net::HTTP::response &r) {
    if (auto ct = r.content_type (); !bool (ct) || *ct != "application/problem+json") return {JSON ()};
    return {JSON::parse (r.Body)};
}

maybe<bool> read_bool_response (const net::HTTP::response &r) {
    auto j = read_JSON_response (r);
    if (!bool (j)) return {};
    if (!j->is_boolean ()) return {};
    return bool (*j);
}

bool is_bool_response (bool expected, const net::HTTP::response &r) {
    maybe<bool> result = read_bool_response (r);
    if (!bool (result)) return false;
    return *result = expected;
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
        data::string::write ("method=random&type=", k, "&name=", x)));
}

template <size_t x> net::HTTP::request request_get_invert_hash (const data::crypto::digest<x> &d, digest_format format) {
    return net::HTTP::request (net::HTTP::request::make ().method (net::HTTP::method::get).path ("/invert_hash").query (
        data::string::write ("digest_format=", format, "&digest=", encoding::hex::write (d))));
}

template <size_t x> net::HTTP::request request_post_invert_hash (Cosmos::hash_function f, const std::string &data) {
    return net::HTTP::request (net::HTTP::request::make ().method (net::HTTP::method::post).path ("/invert_hash").query (
        data::string::write ("function=", f)).body (data));
}

template <size_t x> maybe<data::crypto::digest<x>> read_digest (const net::HTTP::response &r) {
    auto j = read_JSON_response (r);
    if (!bool (j) || !j->is_object () || !j->contains ("data")) return {};
    const JSON &jd = (*j)["data"];
    if (!jd.is_string ()) return {};
    data::crypto::digest<x> d;
    maybe<bytes> data = encoding::base64::read (std::string ((*j)["data"]));
    if (!bool (data) || data->size () != x) return {};
    std::copy (data->begin (), data->end (), d.begin ());
    return d;
}

net::HTTP::request make_retrieve_key_request (const std::string &key_name) {
    return net::HTTP::request (net::HTTP::request::make ().method (net::HTTP::method::get).path ("/key").query (
        data::string::write ("name=", key_name)));
}

key_expression read_key_expression (const net::HTTP::response &r) {
    return key_expression {string (r.Body)};
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
