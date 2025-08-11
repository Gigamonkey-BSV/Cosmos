#ifndef SERVER_COMMAND
#define SERVER_COMMAND

#include <data/io/error.hpp>
#include <Cosmos/types.hpp>

#include "problem.hpp"

using error = data::io::error;
using UTF8 = data::UTF8;

enum class method {
    UNSET,
    HELP,             // print help messages
    VERSION,          // print a version message
    SHUTDOWN,
    ADD_ENTROPY,      // add entropy to the random number generator.
    LIST_WALLETS,
    MAKE_WALLET,
    KEY,
    TO_PRIVATE,
    INVERT_HASH,
    KEY_SEQUENCE,
    NEXT_ADDRESS,     // next address
    NEXT_XPUB,        // next xpub (if supported)
    GENERATE,         // generate a wallet
    ACCEPT,           // accept a payment
    RESTORE,          // restore a wallet
    UPDATE,           // depricated: check if txs in pending have been mined.
    VALUE,            // the value in the wallet.
    DETAILS,
    SEND,             // (depricated)
    SPEND,            //
    REQUEST,          // request a payment
    PAY,              // make a payment.
    SIGN,             // sign an unsigned transaction
    IMPORT,           // import a utxo with private key
    BOOST,            // boost some content
    SPLIT,            // split your wallet into tiny pieces for privacy.
    TAXES,            // calculate income and capital gain for a given year.
    ENCRYPT_KEY,
    DECRYPT_KEY
};

struct command {
    method Method;
    list<UTF8> Path;
    map<UTF8, UTF8> Query;

    struct body {
        net::HTTP::content::type ContentType;
        bytes Body;
    };

    maybe<body> Body;
};

method read_method (const UTF8 &);

std::ostream &operator << (std::ostream &, method);

std::ostream &version (std::ostream &);

std::ostream &help (std::ostream &, method m = method::UNSET);

net::HTTP::response error_response (unsigned int status, method m, problem, const std::string & = "");

net::HTTP::response help_response (method = method::UNSET);

std::string sanitize (const std::string &in);

#endif