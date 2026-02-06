#ifndef SERVER_COMMAND
#define SERVER_COMMAND

#include <Cosmos/types.hpp>

namespace Cosmos {

    enum class method {
        UNSET,
        HELP,             // print help messages
        VERSION,          // print a version message
        SHUTDOWN,
        ADD_ENTROPY,      // add entropy to the random number generator.
        LIST_WALLETS,
        CREATE_WALLET,
        KEY,
        TO_PRIVATE,
        INVERT_HASH,
        KEY_SEQUENCE,
        NEXT,             // next address or xpub if supported.
        IMPORT,           // import a utxo with private key
        GENERATE,         // generate a wallet
        RESTORE,          // restore a wallet
        ACCEPT,           // accept a payment
        UPDATE,           // depricated: check if txs in pending have been mined.
        VALUE,            // the value in the wallet.
        DETAILS,
        SEND,             // (depricated)
        SPEND,            //
        REQUEST,          // request a payment
        PAY,              // make a payment.
        SIGN,             // sign an unsigned transaction
        BOOST,            // boost some content
        SPLIT,            // split your wallet into tiny pieces for privacy.
        TAXES,            // calculate income and capital gain for a given year.
        ENCRYPT_KEY,
        DECRYPT_KEY
    };

    method read_method (const UTF8 &);

    std::ostream &operator << (std::ostream &, method);
    std::istream &operator >> (std::istream &, method &);

    std::string sanitize (const std::string &in);
}

#endif
