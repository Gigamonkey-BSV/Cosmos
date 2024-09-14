#ifndef COSMOS
#define COSMOS

#include <Cosmos/types.hpp>

using namespace data;

struct error {
    int Code;
    maybe<std::string> Message;
    error () : Code {0}, Message {} {}
    error (int code) : Code {code}, Message {} {}
    error (int code, const string &err): Code {code}, Message {err} {}
    error (const string &err): Code {1}, Message {err} {}
};

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
    RECEIVE,  // receive a payment
    PAY,      // make a payment.
    SIGN,     // sign an unsigned transaction
    IMPORT,   // import a utxo with private key
    SEND,     // (depricated) send bitcoin to an address.
    BOOST,    // boost some content
    SPLIT,    // split your wallet into tiny pieces for privacy.
    TAXES     // calculate income and capital gain for a given year.
};

void version ();

void help (method meth = method::UNSET);

void command_generate (const arg_parser &); // offline
void command_update (const arg_parser &);
void command_restore (const arg_parser &);
void command_value (const arg_parser &);    // offline
void command_request (const arg_parser &);  // offline
void command_receive (const arg_parser &);  // offline
void command_pay (const arg_parser &);      // offline
void command_sign (const arg_parser &);     // offline
void command_send (const arg_parser &);     // depricated
void command_import (const arg_parser &);
void command_boost (const arg_parser &);    // offline
void command_split (const arg_parser &);
void command_taxes (const arg_parser &);    // offline

// TODO offline methods function without an internet connection.

method read_method (const io::arg_parser &, uint32 index = 1);

std::string inline sanitize (const std::string &in) {
    return regex_replace (data::to_lower (in), std::regex {"_|-"}, "");
}

#endif
