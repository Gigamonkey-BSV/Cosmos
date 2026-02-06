#include <data/io/main.hpp>
#include <data/io/random.hpp>
#include <data/io/arg_parser.hpp>
#include <data/io/log.hpp>

#include "Cosmos.hpp"
#include <Cosmos/REST/REST.hpp>
#include "server/split.hpp"
#include "server/spend.hpp"
#include "server/import.hpp"

#include <Cosmos/boost/miner_options.hpp>
#include <Cosmos/Diophant.hpp>

#include <data/net/URL.hpp>

using error = data::io::error;

error run (const args::parsed &p);

using error = data::io::error;

namespace data::random {
    bytes Personalization {string {"Cosmos wallet client v1alpha"}};
}

using namespace Cosmos;

namespace data {
    void signal_handler (int signal) noexcept {
        exit (0);
    }

    error main (std::span<const char *const> rr) {

        data::random::init ({.secure = false});

        try {

            return run (args::parsed {static_cast<int> (rr.size ()), rr.data ()});

        } catch (const net::HTTP::exception &x) {
            std::cout << "Problem with http: " << std::endl;
            std::cout << "\trequest: " << x.Request << std::endl;
            std::cout << "\tresponse: " << x.Response << std::endl;
            return error {error::code::try_again, std::string {x.what ()}};
        } catch (const exception &ex) {

            std::cout << "Error: " << ex.what () << std::endl;

            return error::code {ex.Code};
        }

        return {};
    }
}

net::HTTP::request read_command (const data::io::args::parsed &);

using authority = data::net::authority;

authority read_authority (const args::parsed &p) {
    maybe<authority> auth;
    p.get ("authority", auth);

    maybe<net::domain_name> dom;
    p.get ("domain", dom);

    maybe<net::IP::address> addr;
    p.get ("ip_address", addr);

    maybe<uint16> port;
    p.get ("port", port);

    if (dom) {
        authority auth_from_dom;
        auth_from_dom = authority {*dom};
        if (auth && *auth != auth_from_dom)
            throw data::exception {3} << "authority from option 'authority' and option 'domain' disagree";
        else auth = auth_from_dom;
    }

    if (addr) {
        authority auth_from_ip_port;
        if (port) auth_from_ip_port = authority {*addr, *port};
        else auth_from_ip_port = authority {*addr};
        if (auth && *auth != auth_from_ip_port)
            throw data::exception {3} << "authority from option 'authority' and options 'ip_address' and 'port' disagree";
        else auth = auth_from_ip_port;
    }

    return *auth;

}

net::HTTP::response call (const authority &a, const net::HTTP::request &req) {
    return data::synced ([&] () -> awaitable<net::HTTP::response> {
        auto stream = co_await net::HTTP::connect (net::HTTP::version_1_1, a);
        auto res = co_await stream->request (req);
        stream->close ();
        co_return res;
    });
}

error process_unexpected_response (const net::HTTP::response &r) {
    if (auto err = read_error_response (r))
        return error {error::user_action, string::write ("received error response: ", *err)};

    // TODO look at the response code and try to define the error more narrowly.

    return error {error::programmer_action, "Cannot read server response"};
}

error call_server (method m, const args::parsed &p) {
    auto a = read_authority (p);

    Cosmos::REST REST {a};

    switch (m) {

        // TODO URL parameter
        case method::SHUTDOWN: {
            auto validated = args::validate (p, no_params (method::SHUTDOWN));
            auto res = call (a, REST.request_shutdown ());
            if (read_ok_response (res)) return {};
            return process_unexpected_response (res);
        }

        case method::ADD_ENTROPY: {
            auto [_a, _b, entropy_string] = std::get<1> (
                args::validate (p,
                    args::command {
                        set<std::string> {},
                        prefix (method::ADD_ENTROPY) + schema::list::value<std::string> (),
                        -call_options ()}));
            auto res = call (a, REST.request_add_entropy (entropy_string));
            if (read_ok_response (res)) return {};
            return process_unexpected_response (res);
        }

        case method::LIST_WALLETS: {
            auto validated = args::validate (p,
                    args::command {
                        set<std::string> {},
                        prefix (method::LIST_WALLETS),
                        -call_options ()});
            auto res = call (a, REST.request_list_wallets ());
            auto names = read_JSON_response (res);
            if (!names) return process_unexpected_response (res);
            std::cout << *names << std::endl;
            return {};
        }

        case method::GENERATE: {
            auto res = call (a, REST.request (generate_request_options {p}));
            if (read_ok_response (res)) return {};
            auto words = read_string_response (res);
            if (words) {
                std::cout << *words << std::endl;
                return {};
            }
            return process_unexpected_response (res);
        }

        case method::VALUE: {
            auto [wallet_name, _] = std::get<2> (
                args::validate (p,
                    args::command {
                        set<std::string> {},
                        prefix (method::VALUE),
                        schema::map::key<Diophant::symbol> ("name") && -call_options ()}));

            auto res = call (a, REST.request_value (wallet_name));
            auto val = read_string_response (res);
            if (!val) return process_unexpected_response (res);
            std::cout << *val << std::endl;
            return {};
        }

        case method::NEXT: {
            auto res = call (a, REST.request (next_request_options {p}));
            auto addr = read_string_response (res);
            if (!addr) return process_unexpected_response (res);
            std::cout << *addr << std::endl;
            return {};
        }

        case method::IMPORT: {
            auto res = call (a, request_import (p));
            auto result = read_JSON_response (res);
            if (!result) return process_unexpected_response (res);
            std::cout << *result << std::endl;
            return {};
        }

        case method::RESTORE: {
            auto res = call (a, restore_request_options (p).request ());
            auto result = read_JSON_response (res);
            if (!result) return process_unexpected_response (res);
            std::cout << *result << std::endl;
            return {};
        }

        case method::SPEND: {
            auto res = call (a, spend_request_options (p).request ());
            auto txids = read_string_response (res);
            if (!txids) return process_unexpected_response (res);
            std::cout << *txids << std::endl;
            return {};
        }

        case method::SPLIT: {
            auto res = call (a, split_request_options (p).request ());
            auto result = read_string_response (res);
            if (!result) return process_unexpected_response (res);
            std::cout << *result << std::endl;
            return {};
        }

        case method::BOOST: {
            auto res = call (a, BoostPOW::script_options::read (p).request ());
            auto result = read_string_response (res);
            if (!result ) return process_unexpected_response (res);
            std::cout << *result << std::endl;
            return {};
        }
/*
        case UPDATE: {
            command_update (p);
            break;
        }

        case REQUEST: {
            command_request (p);
            break;
        }

        case ACCEPT: {
            command_accept (p);
            break;
        }

        case PAY: {
            command_pay (p);
            break;
        }

        case SIGN: {
            command_sign (p);
            break;
        }

        case TAXES: {
            command_taxes (p);
            break;
        }

        case ENCRYPT_KEY: {
            command_encrypt_private_keys (p);
            break;
        }

        case DECRYPT_KEY: {
            command_decrypt_private_keys (p);
            break;
        }*/

        default: {
            throw data::exception {} << "Error: could not read user's command.";
        }
    }
}

boost::asio::io_context IO;

error try_with_method (method m, const args::parsed &p) {
    switch (m) {
        case method::VERSION: {

            args::validate (p, no_params (method::VERSION));

            DATA_LOG (normal) << version ();
        } return {};

        case method::HELP: {

            auto [_a, _b, help_with] = std::get<1> (
                args::validate (p,
                    args::command {set<std::string> {},
                    prefix (method::HELP) + *schema::list::value<method> (),
                    schema::map::empty ()}));

            if (help_with) DATA_LOG (normal) << help (*help_with);
            else DATA_LOG (normal) << help ();

        } return {};

        default: {
            return call_server (m, p);
        }
    }
}

error run (const args::parsed &p) {

    try {

        // try to read a method from the input.
        maybe<method> Method = p.Arguments.size () > 1 ?
            data::encoding::read<method> {} (p.Arguments[1]) : maybe<method> {};

        // if we can't, look for --help or --version.
        if (!Method) {

            if (p.has ("version"))
                DATA_LOG (normal) << version ();

            else if (p.has ("help"))
                DATA_LOG (normal) << help ();

            else {
                DATA_LOG (normal) << "Error: could not read user's command.";
                DATA_LOG (normal) << help ();
            }
        }

        else return try_with_method (*Method, p);

    } catch (const schema::mismatch &) {
        DATA_LOG (normal) << "mismatch (we will provide more information later)";
        return error {error::code::user_action};
    } catch (const net::HTTP::exception &x) {
        DATA_LOG (normal) << "Problem with http: ";
        DATA_LOG (normal) << "\trequest: " << x.Request;
        DATA_LOG (normal) << "\tresponse: " << x.Response;
        return error {error::code::try_again, std::string {x.what ()}};
    }

    return {};
}


std::ostream &version (std::ostream &o) {
    return o << "Cosmos Wallet version 0.0.2 alpha";
}

std::ostream &help (std::ostream &o, method meth) {
    switch (meth) {
        default :
            return version (o) << "\n" << "input should be <method> <args>... where method is "
            "\n\tgenerate   -- create a new wallet."
            "\n\tupdate     -- get Merkle proofs for txs that were pending last time the program ran."
            "\n\tvalue      -- print the total value in the wallet."
            "\n\trequest    -- generate a payment_request."
            "\n\tpay        -- create a transaction based on a payment request."
            "\n\treceive    -- accept a new transaction for a payment request."
            "\n\tsign       -- sign an unsigned transaction."
            "\n\timport     -- add a utxo to this wallet."
            "\n\tsend       -- send to an address or script. (depricated)"
            "\n\tboost      -- boost content."
            "\n\tsplit      -- split an output into many pieces"
            "\n\trestore    -- restore a wallet from words, a key, or many other options."
            "\nuse help \"method\" for information on a specific method";
        case method::GENERATE :
            return o << "Generate a new wallet in terms of 24 words (BIP 39) or as an extended private key."
            "\narguments for method generate:"
            "\n\t(--name=) (wallet name)"
            "\n\t" R"((--style=)<"address" | "hd_sequence" | "bip44">)"
            "\n\t(--words) (use BIP 39)"
            "\n\t(--no_words) (don't use BIP 39)"
            "\n\t(--accounts=<uint32> (=10)) (how many accounts to pre-generate)"
            "\n\t" R"((--coin_type=<uint32|string> (=0)) (value of BIP 44 coin_type | "none" | "bitcoin" | "bitcoin_cash" | "bitcoin_sv"))"
            "\n\t" R"((--derivation_style=) ("bip44" | "centbee"))"
            "\n\t" R"((--mnemonic_style=) ("none" | "bip39" | "electrumsv"))"
            "\n\t" R"((--format=) ("moneybutton" | "relayx" | "centbee" | "electrumsv"))"
            "\n\t(--number_of_words=<uint32>)"
            "\n\t(--password=<string>)";
        case method::VALUE :
            return o << "Print the value in a wallet. No parameters.";
        case method::NEXT :
            return o << "Generate the next receive address"
            "\narguments for method next:"
            "\n\t(--name=) (wallet name)"
            "\n\t" R"((--sequence=) (sequence name))";
        case method::REQUEST :
            return o << "Generate a new payment request."
                "\narguments for method request:"
                "\n\t(--name=)<wallet name>"
                "\n\t(--payment_type=\"pubkey\"|\"address\"|\"xpub\") (= \"address\")"
                "\n\t(--expires=<number of minutes before expiration>)"
                "\n\t(--memo=\"<explanation of the nature of the payment>\")"
                "\n\t(--amount=<expected amount of payment>)";
        case method::PAY :
            return o << "Respond to a payment request by creating a payment."
                "\narguments for method pay:"
                "\n\t(--name=)<wallet name>"
                "\n\t(--request=)<payment request>"
                "\n\t(--address=<address to pay to>)"
                "\n\t(--amount=<amount to pay>)"
                "\n\t(--memo=<what is the payment about>)"
                "\n\t(--output=<output in hex>)"
                "\n\t(--min_sats_per_output=<float>) (= " << Cosmos::spend_options::DefaultMinSatsPerOutput << ")"
                "\n\t(--max_sats_per_output=<float>) (= " << Cosmos::spend_options::DefaultMaxSatsPerOutput << ")"
                "\n\t(--mean_sats_per_output=<float>) (= " << Cosmos::spend_options::DefaultMeanSatsPerOutput << ") ";
        case method::ACCEPT :
            return o << "Accept a payment."
            "\narguments for method accept:"
            "\n\t(--payment=)<payment tx in BEEF or SPV envelope>";
        case method::SIGN :
            return o << "arguments for method sign not yet available.";
        case method::IMPORT :
            return o << "arguments for method import not yet available.";
        case method::SEND :
            return o << "This method is DEPRICATED";
        case method::SPEND :
            return o << "Spend coins";
        case method::BOOST :
            return o << "arguments for method boost not yet available.";
        case method::SPLIT :
            return o << "Split outputs in your wallet into many tiny outputs with small values over a triangular distribution. "
            "\narguments for method split:"
            "\n\t(--name=)<wallet name>"
            "\n\t(--address=)<address | xpub | script hash>"
            "\n\t(--max_look_ahead=)<integer> (= 10) ; (only used if parameter 'address' is provided as an xpub"
            "\n\t(--min_sats_per_output=<float>) (= " << Cosmos::spend_options::DefaultMinSatsPerOutput << ")"
            "\n\t(--max_sats_per_output=<float>) (= " << Cosmos::spend_options::DefaultMaxSatsPerOutput << ")"
            "\n\t(--mean_sats_per_output=<float>) (= " << Cosmos::spend_options::DefaultMeanSatsPerOutput << ") ";
        case method::RESTORE :
            o << "arguments for method restore:"
            "\n\t(--name=)<wallet name>"
            "\n\t(--key=)<xpub | xpriv>"
            "\n\t(--max_look_ahead=)<integer> (= 10)"
            "\n\t(--words=<string>)"
            "\n\t(--key_type=\"HD_sequence\"|\"BIP44_account\"|\"BIP44_master\") (= \"HD_sequence\")"
            "\n\t(--coin_type=\"Bitcoin\"|\"BitcoinCash\"|\"BitcoinSV\"|<integer>)"
            "\n\t(--wallet_type=\"RelayX\"|\"ElectrumSV\"|\"SimplyCash\"|\"CentBee\"|<string>)"
            "\n\t(--entropy=<string>)";
        case method::ENCRYPT_KEY:
            o << "Encrypt the private key file so that it can only be accessed with a password. No parameters.";
        case method::DECRYPT_KEY :
            return o << "Decrypt the private key file again. No parameters.";
    }

}

