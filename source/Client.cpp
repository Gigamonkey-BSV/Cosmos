
#include "Cosmos.hpp"
#include <Cosmos/REST/method.hpp>
#include "server/split.hpp"
#include "server/spend.hpp"
#include "server/import.hpp"

#include "old.hpp"

#include <Cosmos/boost/miner_options.hpp>
#include <Cosmos/Diophant.hpp>

#include <data/net/URL.hpp>

#include <data/io/random.hpp>
#include <data/io/arg_parser.hpp>
#include <data/io/log.hpp>
#include <data/io/main.hpp>

using error = data::io::error;

error run (const args::parsed &p);

namespace data::random {
    bytes Personalization {string {"Cosmos wallet client v1alpha"}};
}

using namespace Cosmos;

namespace data {
    void signal_handler (int signal) noexcept {
        exit (0);
    }

    error main (std::span<const char *const> rr) {

        // random number generator.
        data::random::init ({.secure = false});

        try {

            return run (args::parsed {static_cast<int> (rr.size ()), rr.data ()});

        } catch (const schema::mismatch &) {
            DATA_LOG (normal) << "mismatch (we will provide more information later)";
            return error {error::code::programmer_action, "failed to generate error upon failure to read user input"};
        } catch (const JSON::exception &x) {
            std::cout << "error reading JSON" << std::endl;
            return error {error::code::user_action, std::string {x.what ()}};
        } catch (const net::HTTP::exception &x) {
            std::cout << "Problem with http: " << std::endl;
            std::cout << "\trequest: " << x.Request << std::endl;
            std::cout << "\tresponse: " << x.Response << std::endl;
            return error {error::code::try_again, std::string {x.what ()}};
        }

        return {};
    }
}

net::HTTP::request read_command (const data::io::args::parsed &);

net::HTTP::response call (const net::HTTP::request &req) {
    DATA_LOG (debug) << "make call with request " << req;
    return data::synced ([&] () -> awaitable<net::HTTP::response> {
        auto stream = co_await net::HTTP::connect (net::HTTP::version_1_1, req.host ());
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

error process_ok_response (const net::HTTP::response &res) {
    if (read_ok_response (res)) return {};
    return process_unexpected_response (res);
}

error process_string_response (const net::HTTP::response &res) {
    auto x = read_string_response (res);
    if (!x) return process_unexpected_response (res);
    std::cout << *x << std::endl;
    return {};
}

error process_JSON_response (const net::HTTP::response &res) {
    auto j = read_JSON_response (res);
    if (!j) return process_unexpected_response (res);
    std::cout << *j << std::endl;
    return {};
}

error call_server (command::method m, const args::parsed &p) {
    DATA_LOG (normal) << "call server with method " << m;

    using namespace command;

    switch (m) {

        // TODO URL parameter
        case SHUTDOWN:
            return process_ok_response (call (make_request<SHUTDOWN> (p)));

        case ADD_ENTROPY:
            return process_ok_response (call (make_request<ADD_ENTROPY> (p)));

        case LIST_WALLETS:
            return process_JSON_response (call (make_request<LIST_WALLETS> (p)));

        case GENERATE: {
            // TODO we should process these errors by catching exceptions
            generate_request_options opts {p};

            switch (opts.check ()) {
                case generate_error::words_vs_mnemonic_style:
                    return error {error::code {3}, "'--words' and '--mnemonic_style=none' or '--no_words' and '--mnmonic_style=<not none>'"};
                case generate_error::centbee_vs_coin_type:
                    return error {error::code {3}, "'derivation_style' conflicts with 'coin_type'"};
                case generate_error::neither_style_nor_coin_type:
                    return error {error::code {3}, "must provide either derivation_style, coin_type, or style"};
                case generate_error::mnemonic_vs_number_of_words:
                    return error {error::code {3}, ""};
                case generate_error::invalid_number_of_words:
                    return error {error::code {3}, "invalid number of words"};
            }

            auto res = call (make_request<GENERATE> (p));

            if (read_ok_response (res)) return {};
            return process_string_response (res);
        }

        case RESTORE: {
            try {
                return process_JSON_response (call (make_request<RESTORE> (p)));
            } catch (const schema::missing_key &x) {
                if (x.Key == "entropy" || x.Key == "master_key" || x.Key == "mnemonic") {
                    std::cout << "We require a value for either 'mnemonic', 'entropy', or 'master_key' in order to restore a wallet." << std::endl;
                    return error {error::code {3}};
                }
                throw x;
            }
        }

        case VALUE: {
            auto res = call (make_request<VALUE> (p));
            auto val = read_string_response (res);
            if (!val) return process_unexpected_response (res);
            std::cout << *val << std::endl;
            return {};
        }

        case NEXT: return process_string_response (call (make_request<NEXT> (p)));

        case IMPORT: {
            auto res = call (make_request<IMPORT> (p));
            auto result = read_JSON_response (res);
            if (!result) return process_unexpected_response (res);
            std::cout << *result << std::endl;
            return {};
        }

        case SPEND: {
            auto res = call (make_request<SPEND> (p));
            auto txids = read_string_response (res);
            if (!txids) return process_unexpected_response (res);
            std::cout << *txids << std::endl;
            return {};
        }

        case SPLIT: {
            auto res = call (make_request<SPLIT> (p));
            auto result = read_string_response (res);
            if (!result) return process_unexpected_response (res);
            std::cout << *result << std::endl;
            return {};
        }

        case BOOST: {
            auto res = call (make_request<BOOST> (p));
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

        case IMPORT_DB:
            return process_JSON_response (call (make_request<command::IMPORT_DB> (p)));

        case command::EXPORT_DB:
            return process_JSON_response (call (make_request<EXPORT_DB> (p)));

        case IMPORT_WALLET:
            return process_JSON_response (call (make_request<IMPORT_WALLET> (p)));

        case EXPORT_WALLET:
            return process_JSON_response (call (make_request<EXPORT_WALLET> (p)));

        default:
            throw data::exception {} << "Error: could not read user's command.";
    }
}

boost::asio::io_context IO;

error try_with_method (command::method m, const args::parsed &p) {
    try {
        switch (m) {
            case command::VERSION: {

                args::validate (p, command::empty ());

                version (std::cout) << std::endl;
            } return {};

            case command::HELP: {

                maybe<command::method> help_with = std::get<2> (
                    args::validate (p,
                        args::command {set<std::string> {},
                        schema::list::value<std::string> () +
                            schema::list::value<command::method> () +
                            *schema::list::value<command::method> (),
                        schema::map::empty ()}).Arguments);

                if (help_with) help (std::cout, *help_with) << std::endl;
                else help (std::cout) << std::endl;

            } return {};

            default: {
                return call_server (m, p);
            }
        }
    } catch (const schema::invalid_entry &x) {
        return error {error::code {3},
            string::write ("invalid value ", x.Value, " for option ", x.Key)};
    } catch (const schema::missing_key &x) {
        return error {error::code {3},
            string::write ("missing expected option ", x.Key)};
    } catch (const schema::incomplete_match &x) {
        return error {error::code {3},
            string::write ("encountered key ", x.Key, " which should not be present because it conflicts with a different option")};
    } catch (const schema::unknown_key &x) {
        return error {error::code {3},
            string::write ("unknown option ", x.Key)};
    } catch (const schema::invalid_value_at &x) {
        return error {error::code {3},
            string::write ("invalid value ", p.Arguments[x.Position], " at position ", x.Position)};
    } catch (const schema::end_of_sequence &x) {
        return error {error::code {3},
            string::write ("encountered end of argument sequence at position ", x.Position, " but expected more arguments")};
    } catch (const schema::no_end_of_sequence &x) {
        return error {error::code {3},
            string::write ("expected end of argument sequence at position ", x.Position, " but found ", p.Arguments[x.Position])};
    }
}

error run (const args::parsed &p) {

    // try to read a method from the input.
    maybe<command::method> Method = p.Arguments.size () > 1 ?
        data::encoding::read<command::method> {} (p.Arguments[1]) : maybe<command::method> {};

    // if we can't, look for --help or --version.
    if (!Method) {

        if (p.has ("version"))
            std::cout << version () << std::endl;

        else if (p.has ("help"))
            std::cout << help () << std::endl;

        else {
            std::cout << "Error: could not read user's command." << std::endl;
            std::cout << help () << std::endl;
        }
    }

    else return try_with_method (*Method, p);

    return {};
}


std::ostream &version (std::ostream &o) {
    return o << "Cosmos Wallet version 0.0.2 alpha";
}

std::ostream &help (std::ostream &o, command::method meth) {
    switch (meth) {
        default :
            return version (o) << "\n" << "input should be <method> <args>... where method is "
            "\n\tgenerate   -- create a new wallet."
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
        case command::GENERATE :
            return o << "Generate a new wallet in terms of 24 words (BIP 39) or as an extended private key."
            "\narguments for method generate:"
            "\n\t<string> (wallet name)"
            "\n\t" R"((--wallet_type=)<"address" | "hd_sequence" | "bip44">)"
            "\n\t(--words) (use BIP 39)"
            "\n\t(--no_words) (don't use BIP 39)"
            "\n\t(--accounts=<uint32> (=10)) (how many accounts to pre-generate)"
            "\n\t" R"((--coin_type=<uint32|string> (=0)) (value of BIP 44 coin_type | "none" | "bitcoin" | "bitcoin_cash" | "bitcoin_sv"))"
            "\n\t" R"((--derivation_style=) ("bip44" | "centbee"))"
            "\n\t" R"((--mnemonic_style=) ("none" | "bip39" | "electrumsv"))"
            "\n\t" R"((--style=) ("moneybutton" | "relayx" | "centbee" | "electrumsv"))"
            "\n\t(--number_of_words=<uint32>)"
            "\n\t(--password=<string>)";
        case command::RESTORE :
            o << "arguments for method restore:"
            "\n\t<string> (wallet name)"
            "\n\t(--master_key=)<xpub | xpriv>"
            "\n\t(--max_look_ahead=)<integer> (= 10)"
            "\n\t(--mnemonic=<string>)"
            "\n\t(--master_key_type=\"HD_sequence\"|\"BIP44_account\"|\"BIP44_master\") (= \"HD_sequence\")"
            "\n\t" R"((--coin_type=<uint32|string> (=0)) (value of BIP 44 coin_type | "none" | "bitcoin" | "bitcoin_cash" | "bitcoin_sv"))"
            "\n\t" R"((--derivation_style=) ("bip44" | "centbee"))"
            "\n\t" R"((--mnemonic_style=) ("none" | "bip39" | "electrumsv"))"
            "\n\t" R"((--style=) ("moneybutton" | "relayx" | "centbee" | "electrumsv"))"
            "\n\t(--entropy=<string>)";
            "\n\t(--password=<string>)";
            "\n\t(--centbee_PIN=<four digits>)";
        case command::VALUE :
            return o << "Print the value in a wallet."
            "\narguments for method value:"
            "\n\t<string> (wallet name)";
        case command::NEXT :
            return o << "Generate the next receive address"
            "\narguments for method next:"
            "\n\t<string> (wallet name)"
            "\n\t" R"((--sequence=) (sequence name))";
        case command::IMPORT :
            return o << "arguments for method import not yet available."
                "\narguments for method next:"
                "\n\t<string> (wallet name)"
                "\n\t" R"(--key=<a private key>)"
                "\n\t" R"(--tx=<txid, hex tx, or BEEF>)"
                "\n\t" R"(--outpoint=<txid:uint32>)"
                "\n\t" R"(--index=<uint32>)"
                "\nAt least one of `key`, `tx`, or `outpoint` must be present. Multiple values of each of these is allowed."
                "\nIf `index` is present, then outpoint must not be present and only one tx is allowed."
                "\nIf no keys are given, we try to find the right keys in the database and it's an error not to find any."
                "\nIf no outpoints are given, we check every output of every transaction provided and it's an error if nothing works."
                "\nIf a txid is provided with no transaction, we search for it online.";
        case command::REQUEST :
            return o << "Generate a new payment request."
                "\narguments for method request:"
                "\n\t<string> (wallet name)"
                "\n\t(--payment_type=\"pubkey\"|\"address\"|\"xpub\") (= \"address\")"
                "\n\t(--expires=<number of minutes before expiration>)"
                "\n\t(--memo=\"<explanation of the nature of the payment>\")"
                "\n\t(--amount=<expected amount of payment>)";
        case command::PAY :
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
        case command::ACCEPT :
            return o << "Accept a payment."
            "\narguments for method accept:"
            "\n\t(--payment=)<payment tx in BEEF or SPV envelope>";
        case command::SIGN :
            return o << "arguments for method sign not yet available.";
        case command::SEND :
            return o << "This method is DEPRICATED";
        case command::SPEND :
            return o << "Spend coins";
        case command::BOOST :
            return o << "arguments for method boost not yet available.";
        case command::SPLIT :
            return o << "Split outputs in your wallet into many tiny outputs with small values over a triangular distribution. "
            "\narguments for method split:"
            "\n\t(--name=)<wallet name>"
            "\n\t(--address=)<address | xpub | script hash>"
            "\n\t(--max_look_ahead=)<integer> (= 10) ; (only used if parameter 'address' is provided as an xpub"
            "\n\t(--min_sats_per_output=<float>) (= " << Cosmos::spend_options::DefaultMinSatsPerOutput << ")"
            "\n\t(--max_sats_per_output=<float>) (= " << Cosmos::spend_options::DefaultMaxSatsPerOutput << ")"
            "\n\t(--mean_sats_per_output=<float>) (= " << Cosmos::spend_options::DefaultMeanSatsPerOutput << ") ";
        case command::ENCRYPT_KEY:
            o << "Encrypt the private key file so that it can only be accessed with a password. No parameters.";
        case command::DECRYPT_KEY :
            return o << "Decrypt the private key file again. No parameters.";
    }

}

