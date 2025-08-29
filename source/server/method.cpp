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
    UTF8 m = sanitize (p);

    if (m == "help") return method::HELP;
    if (m == "version") return method::VERSION;
    if (m == "shutdown") return method::SHUTDOWN;
    if (m == "addentropy") return method::ADD_ENTROPY;
    if (m == "key") return method::KEY;
    if (m == "keysequnce") return method::KEY_SEQUENCE;
    if (m == "toprivate") return method::TO_PRIVATE;
    if (m == "inverthash") return method::INVERT_HASH;
    if (m == "makewallet") return method::MAKE_WALLET;
    if (m == "listwallets") return method::LIST_WALLETS;
    if (m == "generate") return method::GENERATE;
    if (m == "restore") return method::RESTORE;
    if (m == "details") return method::DETAILS;
    if (m == "value") return method::VALUE;
    if (m == "update") return method::UPDATE;
    if (m == "request") return method::REQUEST;
    if (m == "accept") return method::ACCEPT;
    if (m == "pay") return method::PAY;
    if (m == "sign") return method::SIGN;
    if (m == "import") return method::IMPORT;
    if (m == "send") return method::SPEND;
    if (m == "boost") return method::BOOST;
    if (m == "split") return method::SPLIT;
    if (m == "taxes") return method::TAXES;
    if (m == "encryptprivatekeys") return method::ENCRYPT_KEY;
    if (m == "decryptprivatekeys") return method::DECRYPT_KEY;

    return method::UNSET;
}

std::ostream &operator << (std::ostream &o, method m) {
    switch (m) {
        case method::UNSET: return o << "unset";
        case method::HELP: return o << "help";              // print help messages
        case method::VERSION: return o << "version";        // print a version message
        case method::SHUTDOWN: return o << "shutdown";
        case method::ADD_ENTROPY: return o << "add_entropy";    // add entropy to the random number generator.
        case method::MAKE_WALLET: return o << "make_wallet";
        case method::KEY: return o << "key";
        case method::KEY_SEQUENCE: return o << "key_sequence";
        case method::TO_PRIVATE: return o << "to_private";
        case method::GENERATE: return o << "generate";       // generate a wallet
        case method::RESTORE: return o << "restore";         // restore a wallet
        case method::UPDATE: return o << "update";           // depricated: check if txs in pending have been mined.
        case method::VALUE: return o << "value";             // the value in the wallet.
        case method::DETAILS: return o << "details";
        case method::SEND: return o << "send";              // (depricated)
        case method::SPEND: return o << "spend";            // check pending txs for having been mined. (depricated)
        case method::REQUEST: return o << "request";        // request a payment
        case method::ACCEPT: return o << "accept";         // accept a payment
        case method::PAY: return o << "pay";               // make a payment.
        case method::SIGN: return o << "sign";             // sign an unsigned transaction
        case method::IMPORT: return o << "import";         // import a utxo with private key
        case method::BOOST: return o << "boost";          // boost some content
        case method::SPLIT: return o << "split";           // split your wallet into tiny pieces for privacy.
        case method::TAXES: return o << "taxes";           // calculate income and capital gain for a given year.
        case method::ENCRYPT_KEY: return o << "encrypt_key";
        case method::DECRYPT_KEY: return o << "decrypt_key";
        default: throw data::exception {} << "unknown method";
    }
}
