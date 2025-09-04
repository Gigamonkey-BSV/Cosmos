#include "../source/server/method.hpp"
#include "../source/server/server.hpp"
#include "../source/server/key.hpp"
#include "../source/server/generate.hpp"
#include "../source/server/invert_hash.hpp"
#include <data/net/JSON.hpp>
#include <data/net/error.hpp>
#include "gtest/gtest.h"

std::atomic<bool> Shutdown {false};

constexpr const int arg_count = 1;
constexpr const char *const arg_values[arg_count] = {"--sqlite_in_memory"};

server get_test_server () {
    Shutdown = false;
    return server {options {arg_parser {arg_count, arg_values}}};
}

net::HTTP::request make_shutdown_request ();
maybe<net::error> read_error (const net::HTTP::response &);

bool is_error (const net::HTTP::response &r) {
    return bool (read_error (r));
}

net::HTTP::response make_request (server &x, const net::HTTP::request &r) {
    return data::synced (&server::operator (), &x, r);
}

TEST (Server, Shutdown) {
    auto test_server = get_test_server ();
    EXPECT_FALSE (is_error (make_request (test_server, make_shutdown_request ())));
    EXPECT_TRUE (Shutdown);
}

net::HTTP::request make_add_entropy_request (const string &entropy);
bool is_ok_response (const net::HTTP::response &r);
bool is_bool_response (bool expected, const net::HTTP::response &);
bool is_string_response (const net::HTTP::response &);
bool is_data_response (const bytes &expected, const net::HTTP::response &);

net::HTTP::request make_list_wallets_request ();
bool is_JSON_response (const JSON &, const net::HTTP::response &r);

net::HTTP::request inline make_create_wallet_request (const std::string &wallet_name) {
    string target = string::write ("/create_wallet/", wallet_name);
    std::cout << "target is " << target << std::endl;
    return net::HTTP::request::make ().method (net::HTTP::method::post).target (target).host ("localhost");
}

template <size_t x> maybe<data::digest<x>> read_digest (const net::HTTP::response &);

TEST (Server, InvertHash) {
    auto test_server = get_test_server ();
    string data_A = "Hi, this string will be hashed.";
    string data_B = "Hi, this string will not have the same hash.";

    data::digest256 SHA256_A = data::crypto::SHA2_256 (data_A);
    data::digest256 SHA256_B = data::crypto::SHA2_256 (data_B);

    data::digest160 Hash160_A = data::crypto::Bitcoin_160 (data_A);
    data::digest160 Hash160_B = data::crypto::Bitcoin_160 (data_B);

    // attempt to retrieve data before it has been entered.
    ASSERT_TRUE (is_error (make_request (test_server, get_invert_hash_request (SHA256_A))));
    ASSERT_TRUE (is_error (make_request (test_server, get_invert_hash_request (Hash160_A))));

    // attempt to input the wrong hash
    ASSERT_TRUE (is_error (make_request (test_server,
        put_invert_hash_request (Cosmos::hash_function::SHA2_256, bytes (data_B), SHA256_A))));

    EXPECT_TRUE (is_ok_response (make_request (test_server,
        put_invert_hash_request (Cosmos::hash_function::SHA2_256, bytes (data_A), SHA256_A))));

    ASSERT_TRUE (is_data_response (bytes (data_A), make_request (test_server, get_invert_hash_request (SHA256_A))));

    // this should still fail
    ASSERT_TRUE (is_error (make_request (test_server, get_invert_hash_request (Hash160_A))));

    // attempt to overwrite data that already exists.
    // this should succeed because it's a put.
    EXPECT_TRUE (is_ok_response (make_request (test_server,
        put_invert_hash_request (Cosmos::hash_function::SHA2_256, bytes (data_A)))));

    // provide another hash for the same data.
    EXPECT_TRUE (is_ok_response (make_request (test_server,
        put_invert_hash_request (Cosmos::hash_function::Hash160, bytes (data_A)))));

    EXPECT_TRUE (is_data_response (bytes (data_A), make_request (test_server, get_invert_hash_request (SHA256_A))));

}

TEST (Server, ToPrivate) {
    auto test_server = get_test_server ();

    // generate three kinds of keys.

    // try to retrieve key before it was entered.

    // enter private key into database.
}

TEST (Server, CreateWallet) {
    auto test_server = get_test_server ();

    // list_wallets should return an empty list.
    EXPECT_TRUE (is_JSON_response (JSON::array_t (), make_request (test_server, make_list_wallets_request ())));

    // Make a wallet.
    ASSERT_TRUE (is_ok_response (make_request (test_server, make_create_wallet_request ("Wally"))));

    // list wallets should now be a non-empty list.
    EXPECT_TRUE (is_JSON_response (JSON::array_t {"Wally"}, make_request (test_server, make_list_wallets_request ())));

    // try to make the wallet again and fail.
    ASSERT_TRUE (is_error (make_request (test_server, make_create_wallet_request ("Wally"))));

}

maybe<string> read_string_response (const net::HTTP::response &r);

TEST (Server, Entropy) {
    auto test_server = get_test_server ();

    // fail to load a key because the wallet doesn't exist.
    ASSERT_TRUE (is_error (make_request (test_server,
        get_key_request ("Wally", "X"))));

    // fail to generate a random key because the wallet doesn't exist
    ASSERT_TRUE (is_error (make_request (test_server,
        post_key_request ("Wally", "X", key_type::secp256k1))));

    // Make a wallet.
    ASSERT_TRUE (is_ok_response (make_request (test_server, make_create_wallet_request ("Wally"))));

    // fail to generate a random key because the random number generator is not set up.
    ASSERT_TRUE (is_error (make_request (test_server,
        post_key_request ("Wally", "X", key_type::secp256k1))));

    // initialize random number generator.
    ASSERT_TRUE (is_ok_response (make_request (test_server,
        make_add_entropy_request ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"))));

    // succeed at generating a random key
    auto successfully_create_key = read_string_response (make_request (test_server,
        post_key_request ("Wally", "X", key_type::secp256k1)));

    ASSERT_TRUE (successfully_create_key);

    auto get_new_key = read_string_response (make_request (test_server, get_key_request ("Wally", "X")));

    // successfully retrieve the key.
    ASSERT_TRUE (get_new_key);

    EXPECT_EQ (*successfully_create_key, *get_new_key);

    // try to overwrite the key and fail.
    ASSERT_TRUE (is_error (make_request (test_server,
        post_key_request ("Wally", "X", key_type::secp256k1))));
}

// prepare a server with entropy and an initial wallet.
server prepare (server, const string &entropy = "entropyABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");

template <size_t x> net::HTTP::request request_get_invert_hash (const data::digest<x> &d, digest_format format = digest_format::HEX);
template <size_t x> net::HTTP::request request_post_invert_hash (Cosmos::hash_function, const std::string &data);

// TODO these are incomplete.
net::HTTP::request make_to_private_get_request (const key_expression &k) {
    return net::HTTP::request (net::HTTP::request::make {}.method (net::HTTP::method::get).path ("/to_private"));
}

net::HTTP::request make_to_private_post_request (const key_expression &k, const key_expression &) {
    return net::HTTP::request (net::HTTP::request::make {}.method (net::HTTP::method::post).path ("/to_private"));
}

key_expression read_key_expression (const net::HTTP::response &);

bool is_string_response (const net::HTTP::response &r) {
    return bool (read_string_response (r));
}

TEST (Server, Key) {
    auto test_server = prepare (get_test_server ());

    // attempt to retrieve a key that hasn't been entered yet.
    EXPECT_TRUE (is_error (make_request (test_server, get_key_request ("Wally", "X"))));

    // generate keys of different types and retrieve them.
    auto res_gen_X = read_string_response (make_request (test_server, post_key_request ("Wally", "X", key_type::secp256k1)));
    auto res_gen_Y = read_string_response (make_request (test_server, post_key_request ("Wally", "Y", key_type::WIF)));
    auto res_gen_Z = read_string_response (make_request (test_server, post_key_request ("Wally", "Z", key_type::xpriv)));

    EXPECT_TRUE (bool (res_gen_X));
    EXPECT_TRUE (bool (res_gen_Y));
    EXPECT_TRUE (bool (res_gen_Y));

    key_expression secret_X;
    key_expression secret_Y;
    key_expression secret_Z;
/*
    EXPECT_NO_THROW (secret_X = read_key_expression (make_request (test_server, get_key_request ("wally", "X"))));
    EXPECT_NO_THROW (secret_Y = read_key_expression (make_request (test_server, get_key_request ("Wally", "Y"))));
    EXPECT_NO_THROW (secret_Z = read_key_expression (make_request (test_server, get_key_request ("Wally", "Z"))));

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
    */
}

using JSON = data::JSON;

net::HTTP::request make_next_address_request (const std::string &wallet_name, const std::string &sequence_name = "receive");

net::HTTP::request make_next_xpub_request (const std::string &wallet_name);

maybe<std::string> read_string (const net::HTTP::response &r);

TEST (Server, Generate) {
    auto test_server = prepare (get_test_server ());

    // generate a wallet of each type
    // we should simply get a bool response when we don't request the restoration words.
    EXPECT_TRUE (is_bool_response (true, make_request (test_server,
        generate_request_options {"A"}.wallet_style (wallet_style::BIP_44))));
    EXPECT_TRUE (is_bool_response (true, make_request (test_server,
        generate_request_options {"B"}.wallet_style (wallet_style::BIP_44_plus))));
    EXPECT_TRUE (is_bool_response (true, make_request (test_server,
        generate_request_options {"C"}.wallet_style (wallet_style::experimental))));

    maybe<std::string> maybe_words_D = read_string (make_request (test_server,
        generate_request_options {"D"}.wallet_style (wallet_style::BIP_44).mnemonic_style (mnemonic_style::BIP_39)));
    EXPECT_TRUE (bool (maybe_words_D));

    maybe<std::string> maybe_words_E = read_string (make_request (test_server,
        generate_request_options {"E"}.wallet_style (wallet_style::BIP_44_plus).mnemonic_style (mnemonic_style::BIP_39)));
    EXPECT_TRUE (bool (maybe_words_E));

    maybe<std::string> maybe_words_F = read_string (make_request (test_server,
        generate_request_options {"F"}.wallet_style (wallet_style::experimental).mnemonic_style (mnemonic_style::BIP_39)));
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

TEST (Server, Wallet) {
    auto test_server = prepare (get_test_server ());
    // make a wallet.
    // Add key sequence to wallet
    // generate next keys of key sequences.
}

server prepare (server x, const string &entropy) {
    data::synced (&server::operator (), &x,
        make_add_entropy_request (entropy));
    data::synced (&server::operator (), &x, make_create_wallet_request ("Wally"));
    return x;
}

bool is_ok_response (const net::HTTP::response &r) {
    bool ok = !bool (r.content_type ()) && r.Body == bytes {} && r.Status == 204;
    if (ok) return true;
    // check if this is an error response instead.
    maybe<net::error> err = read_error (r);
    if (bool (err))
        std::cout << "Expected ok response but got error response " << *err << std::endl;
    return false;
}

maybe<string> read_string_response (const net::HTTP::response &r) {
    bool ok = bool (r.content_type ()) && *r.content_type () == "text/plain" && r.Status == 200;
    if (ok) return string (r.Body);
    // check if this is an error response instead.
    maybe<net::error> err = read_error (r);
    if (bool (err))
        std::cout << "Expected string response but got error response " << *err << std::endl;
    return {};
}

net::HTTP::request make_list_wallets_request () {
    return net::HTTP::request::make ().path ("/list_wallets").method (net::HTTP::method::get).host ("localhost");
}

net::HTTP::request make_next_address_request (const std::string &wallet_name, const std::string &sequence_name) {
    return net::HTTP::request::make ().method (net::HTTP::method::post).path (string::write ("/next_address/", wallet_name)).host ("localhost");
}

net::HTTP::request make_next_xpub_request (const std::string &wallet_name) {
    return net::HTTP::request::make ().method (net::HTTP::method::post).path (string::write ("/next_xpub/", wallet_name)).host ("localhost");
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
    return *result == expected;
}

maybe<std::string> read_string (const net::HTTP::response &r) {
    if (!r.content_type () == net::HTTP::content::type::text_plain) return {};
    return string (r.Body);
}

net::HTTP::request make_shutdown_request () {
    return net::HTTP::request::make {}.method (
        net::HTTP::method::put
    ).path (
        "/shutdown"
    ).host ("localhost");
}

maybe<net::error> read_error (const net::HTTP::response &r) {
    auto ct = r.content_type ();
    if (!bool (ct)) return {};

    if (*ct != "application/problem+json") return {};

    auto j = JSON::parse (string (r.Body));

    if (!net::error::valid (j)) return {};

    return j;
}

maybe<bool> read_bool_response (const net::HTTP::response &r) {
    auto j = read_JSON_response (r);
    if (!bool (j)) return {};
    if (!j->is_boolean ()) return {};
    return bool (*j);
}

maybe<bytes> read_data_response (const net::HTTP::response &r) {
    auto ct = r.content_type ();
    if (!bool (ct)) return {};

    if (*ct != "application/octet-stream") return {};

    return r.Body;
}

bool is_bool_response (bool expected, const net::HTTP::response &r) {
    maybe<bool> result = read_bool_response (r);
    if (!bool (result)) return false;
    return *result == expected;
}

bool is_data_response (const bytes &expected, const net::HTTP::response &r) {
    maybe<bytes> result = read_data_response (r);
    if (!bool (result)) return false;
    return *result == expected;
}

net::HTTP::request make_add_entropy_request (const string &entropy) {
    return net::HTTP::request::make {}.method (
            net::HTTP::method::post
        ).path (
            "/add_entropy"
        ).body (bytes (entropy)).host ("localhost");
}

template <size_t x> maybe<data::digest<x>> read_digest (const net::HTTP::response &r) {
    auto j = read_JSON_response (r);
    if (!bool (j) || !j->is_object () || !j->contains ("data")) return {};
    const JSON &jd = (*j)["data"];
    if (!jd.is_string ()) return {};
    data::digest<x> d;
    maybe<bytes> data = encoding::base64::read (std::string ((*j)["data"]));
    if (!bool (data) || data->size () != x) return {};
    std::copy (data->begin (), data->end (), d.begin ());
    return d;
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
