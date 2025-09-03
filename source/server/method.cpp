#include "method.hpp"
#include <regex>

using JSON = data::JSON;

net::HTTP::response error_response (unsigned int status, method m, problem tt, const std::string &detail) {
    std::stringstream meth_string;
    meth_string << m;
    std::stringstream problem_type;
    problem_type << tt;

    JSON err {
        {"method", meth_string.str ()},
        {"status", status},
        {"title", problem_type.str ()}};

    if (detail != "") err["detail"] = detail;

    return net::HTTP::response (status, {{"content-type", "application/problem+json"}}, bytes (data::string (err.dump ())));
}

net::HTTP::response help_response (method m) {
    std::stringstream ss;
    help (ss, m);
    return net::HTTP::response (200, {{"content-type", "text/plain"}}, bytes (data::string (ss.str ())));
}

std::string regex_replace (const std::string &x, const std::regex &r, const std::string &n) {
    std::stringstream ss;
    std::regex_replace (std::ostreambuf_iterator<char> (ss), x.begin (), x.end (), r, n);
    return ss.str ();
}

std::string sanitize (const std::string &in) {
    return regex_replace (data::to_lower (in), std::regex {"_|-"}, "");
}

method read_method (const UTF8 &p) {
    UTF8 sanitized = sanitize (p);

    if (sanitized == "help") return method::HELP;
    if (sanitized == "version") return method::VERSION;
    if (sanitized == "shutdown") return method::SHUTDOWN;
    if (sanitized == "addentropy") return method::ADD_ENTROPY;
    if (sanitized == "inverthash") return method::INVERT_HASH;
    if (sanitized == "toprivate") return method::TO_PRIVATE;
    if (sanitized == "createwallet") return method::CREATE_WALLET;
    if (sanitized == "listwallets") return method::LIST_WALLETS;
    if (sanitized == "details") return method::DETAILS;
    if (sanitized == "value") return method::VALUE;
    if (sanitized == "key") return method::KEY;
    if (sanitized == "keysequnce") return method::KEY_SEQUENCE;
    if (sanitized == "generate") return method::GENERATE;
    if (sanitized == "restore") return method::RESTORE;
    if (sanitized == "update") return method::UPDATE;
    if (sanitized == "request") return method::REQUEST;
    if (sanitized == "accept") return method::ACCEPT;
    if (sanitized == "pay") return method::PAY;
    if (sanitized == "sign") return method::SIGN;
    if (sanitized == "import") return method::IMPORT;
    if (sanitized == "send") return method::SPEND;
    if (sanitized == "boost") return method::BOOST;
    if (sanitized == "split") return method::SPLIT;
    if (sanitized == "taxes") return method::TAXES;
    if (sanitized == "encryptprivatekeys") return method::ENCRYPT_KEY;
    if (sanitized == "decryptprivatekeys") return method::DECRYPT_KEY;

    return method::UNSET;
}

std::ostream &operator << (std::ostream &o, method m) {
    switch (m) {
        case method::UNSET: return o << "unset";
        case method::HELP: return o << "help";                // print help messages
        case method::VERSION: return o << "version";          // print a version message
        case method::SHUTDOWN: return o << "shutdown";
        case method::ADD_ENTROPY: return o << "add_entropy";    // add entropy to the random number generator.
        case method::INVERT_HASH: return o << "invert_hash";
        case method::TO_PRIVATE: return o << "to_private";
        case method::CREATE_WALLET: return o << "create_wallet";
        case method::LIST_WALLETS: return o << "list_wallets";
        case method::VALUE: return o << "value";              // the value in the wallet.
        case method::DETAILS: return o << "details";
        case method::KEY: return o << "key";
        case method::KEY_SEQUENCE: return o << "key_sequence";
        case method::GENERATE: return o << "generate";        // generate a wallet
        case method::RESTORE: return o << "restore";          // restore a wallet
        case method::UPDATE: return o << "update";            // depricated: check if txs in pending have been mined.
        case method::SEND: return o << "send";                // (depricated)
        case method::SPEND: return o << "spend";              // check pending txs for having been mined. (depricated)
        case method::REQUEST: return o << "request";          // request a payment
        case method::ACCEPT: return o << "accept";            // accept a payment
        case method::PAY: return o << "pay";                  // make a payment.
        case method::SIGN: return o << "sign";                // sign an unsigned transaction
        case method::IMPORT: return o << "import";            // import a utxo with private key
        case method::BOOST: return o << "boost";              // boost some content
        case method::SPLIT: return o << "split";              // split your wallet into tiny pieces for privacy.
        case method::TAXES: return o << "taxes";              // calculate income and capital gain for a given year.
        case method::ENCRYPT_KEY: return o << "encrypt_key";
        case method::DECRYPT_KEY: return o << "decrypt_key";
        default: throw data::exception {} << "unknown method";
    }
}
