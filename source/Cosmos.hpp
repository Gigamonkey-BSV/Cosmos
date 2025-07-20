#ifndef COSMOS
#define COSMOS

#include <data/io/arg_parser.hpp>
#include <data/io/error.hpp>
#include <Cosmos/types.hpp>

using namespace data;

using arg_parser = io::arg_parser;
using error = io::error;
using filepath = Cosmos::filepath;

enum meth {
    UNSET,
    HELP,             // print help messages
    VERSION,          // print a version message
    SHUTDOWN,
    ADD_ENTROPY,      // add entropy to the random number generator.
    LIST_WALLETS,
    MAKE_WALLET,
    ADD_KEY,
    TO_PRIVATE,
    INVERT_HASH,
    ADD_KEY_SEQUENCE,
    NEXT_ADDRESS,     // next address
    NEXT_XPUB,        // next xpub (if supported)
    GENERATE,         // generate a wallet
    RESTORE,          // restore a wallet
    UPDATE,           // depricated: check if txs in pending have been mined.
    VALUE,            // the value in the wallet.
    DETAILS,
    SEND,             // (depricated)
    SPEND,            //
    REQUEST,          // request a payment
    ACCEPT,           // accept a payment
    PAY,              // make a payment.
    SIGN,             // sign an unsigned transaction
    IMPORT,           // import a utxo with private key
    BOOST,            // boost some content
    SPLIT,            // split your wallet into tiny pieces for privacy.
    TAXES,            // calculate income and capital gain for a given year.
    ENCRYPT_KEY,
    DECRYPT_KEY
};

meth read_method (const UTF8 &);

std::ostream &operator << (std::ostream &, meth);

std::ostream &version (std::ostream &);

std::ostream &help (std::ostream &, meth m = UNSET);

net::HTTP::request request_generate (const arg_parser &);
net::HTTP::request request_update (const arg_parser &);
net::HTTP::request request_restore (const arg_parser &);
net::HTTP::request request_value (const arg_parser &);
net::HTTP::request request_request (const arg_parser &);
net::HTTP::request request_accept (const arg_parser &);
net::HTTP::request request_pay (const arg_parser &);
net::HTTP::request request_sign (const arg_parser &);
net::HTTP::request request_spend (const arg_parser &);
net::HTTP::request request_import (const arg_parser &);
net::HTTP::request request_boost (const arg_parser &);
net::HTTP::request request_split (const arg_parser &);
net::HTTP::request request_taxes (const arg_parser &);
net::HTTP::request request_encrypt_private_keys (const arg_parser &);
net::HTTP::request request_decrypt_private_keys (const arg_parser &);

void command_generate (const arg_parser &); // offline
void command_update (const arg_parser &);
void command_restore (const arg_parser &);
void command_value (const arg_parser &);    // offline
void command_request (const arg_parser &);  // offline
void command_accept (const arg_parser &);  // offline
void command_pay (const arg_parser &);      // offline
void command_sign (const arg_parser &);     // offline
void command_send (const arg_parser &);     // depricated
void command_import (const arg_parser &);
void command_boost (const arg_parser &);    // offline
void command_split (const arg_parser &);
void command_taxes (const arg_parser &);    // offline
void command_encrypt_private_keys (const arg_parser &);    // offline
void command_decrypt_private_keys (const arg_parser &);    // offline

// TODO offline methods function without an internet connection.

meth read_method (const io::arg_parser &, uint32 index = 1);

std::string sanitize (const std::string &in);

#endif
