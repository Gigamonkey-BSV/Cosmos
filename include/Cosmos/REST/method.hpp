#ifndef SERVER_COMMAND
#define SERVER_COMMAND

#include <Cosmos/Diophant.hpp>
#include <data/net/REST.hpp>
#include <data/io/arg_parser.hpp>

namespace net = data::net;

namespace schema = data::schema;

namespace args = data::io::args;

namespace Cosmos::command {

    enum method {
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
        DECRYPT_KEY,

        // work with an earlier JSON format.
        IMPORT_DB,
        EXPORT_DB,
        IMPORT_WALLET,
        EXPORT_WALLET
    };

    method read_method (const UTF8 &);

    std::ostream &operator << (std::ostream &, method);
    std::istream &operator >> (std::istream &, method &);

    std::string sanitize (const std::string &in);

    // make an HTTP request from program input
    template <method> net::HTTP::request make_request (const args::parsed &);

    template <> net::HTTP::request make_request<SHUTDOWN> (const args::parsed &);
    template <> net::HTTP::request make_request<ADD_ENTROPY> (const args::parsed &);
    template <> net::HTTP::request make_request<LIST_WALLETS> (const args::parsed &);
    template <> net::HTTP::request make_request<NEXT> (const args::parsed &);
    template <> net::HTTP::request make_request<GENERATE> (const args::parsed &);
    template <> net::HTTP::request make_request<VALUE> (const args::parsed &);
    template <> net::HTTP::request make_request<RESTORE> (const args::parsed &);
    template <> net::HTTP::request make_request<IMPORT_DB> (const args::parsed &);
    template <> net::HTTP::request make_request<EXPORT_DB> (const args::parsed &);
    template <> net::HTTP::request make_request<IMPORT_WALLET> (const args::parsed &);
    template <> net::HTTP::request make_request<EXPORT_WALLET> (const args::parsed &);

    using authority = data::net::authority;

    authority read_authority (const args::parsed &p);

    // schema for an empty command
    auto inline empty () {
        return args::command (set<std::string> {},
            schema::list::value<std::string> () +
                schema::list::value<method> (),
            schema::map::empty ());
    }

    auto inline call_options () {
        return schema::map::key<net::IP::TCP::endpoint> ("endpoint") ||
        schema::map::key<uint16> ("port", 4567) && (
            schema::map::key<net::IP::address> ("ip_address") ||
            schema::map::key<net::authority> ("authority") ||
            schema::map::key<net::domain_name> ("domain", "localhost"));
    }

    auto inline empty_call () {
        return args::command (set<std::string> {},
            schema::list::value<std::string> () +
                schema::list::value<method> (),
            call_options ());
    }

    auto inline wallet_method () {
        return args::command (set<std::string> {},
            schema::list::value<std::string> () +
                schema::list::value<command::method> () +
                schema::list::value<Diophant::symbol> (),
            call_options ());
    }

    template <typename list_schema, typename map_schema>
    auto inline call (const list_schema &args, const map_schema &opts, set<std::string> flags = {}) {
        return args::command (flags, args, opts + call_options ());
    }
}

#endif
