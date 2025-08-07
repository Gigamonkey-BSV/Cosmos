#include "options.hpp"
#include <Cosmos/random.hpp>

using UTF8 = data::UTF8;

struct server {

    server (const options &o);

    awaitable<net::HTTP::response> operator () (const net::HTTP::request &);

    void add_entropy (const bytes &);

    crypto::entropy &get_secure_random ();
    crypto::entropy &get_casual_random ();

    Cosmos::spend_options SpendOptions;

    ptr<database> DB;

    struct make_wallet_options {};

    bool make_wallet (const make_wallet_options &);

    awaitable<Bitcoin::satoshi> value (const UTF8 &wallet_name);

    struct wallet_info {
        Bitcoin::satoshi Value;
        uint32 Outputs;
        Bitcoin::satoshi MaxOutput;

        operator JSON () const;
    };

    awaitable<wallet_info> details (const UTF8 &wallet_name);
/*
    map<Bitcoin::outpoint, Bitcoin::output> account (const instruction &);

    tuple<Bitcoin::TXID, wallet_info> spend (const instruction &);*/

private:
    ptr<crypto::fixed_entropy> FixedEntropy {nullptr};
    ptr<crypto::entropy> Entropy {nullptr};
    ptr<crypto::NIST::DRBG> SecureRandom {nullptr};
    ptr<crypto::linear_combination_random> CasualRandom {nullptr};

};

net::HTTP::response favicon ();
net::HTTP::response HTML_JS_UI_response ();
net::HTTP::response version_response ();

net::HTTP::response inline ok_response () {
    return net::HTTP::response (204);
}

net::HTTP::response inline JSON_response (const JSON &j) {
    return net::HTTP::response (200, {{"content-type", "application/json"}}, bytes (data::string (j.dump ())));
}

net::HTTP::response inline boolean_response (bool b) {
    return JSON_response (b);
}

net::HTTP::response inline value_response (Bitcoin::satoshi x) {
    return JSON_response (JSON (int64 (x)));
}