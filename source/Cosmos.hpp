#ifndef COSMOS
#define COSMOS

#include <data/io/arg_parser.hpp>
#include <data/io/error.hpp>
#include <Cosmos/types.hpp>

using namespace data;

using arg_parser = io::arg_parser;
using error = io::error;

error run (const arg_parser &);

enum class method {
    UNSET,
    HELP,     // print help messages
    VERSION,  // print a version message
    GENERATE, // generate a wallet
    RESTORE,  // restore a wallet
    VALUE,    // the value in the wallet.
    UPDATE,   // check pending txs for having been mined.
    REQUEST,  // request a payment
    ACCEPT,   // accept a payment
    PAY,      // make a payment.
    SIGN,     // sign an unsigned transaction
    IMPORT,   // import a utxo with private key
    SEND,     // (depricated) send bitcoin to an address.
    BOOST,    // boost some content
    SPLIT,    // split your wallet into tiny pieces for privacy.
    TAXES,    // calculate income and capital gain for a given year.
    ENCRYPT_PRIVATE_KEYS,
    DECRYPT_PRIVATE_KEYS
};

void version ();

void help (method meth = method::UNSET);

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

method read_method (const io::arg_parser &, uint32 index = 1);

std::string sanitize (const std::string &in);

#endif
