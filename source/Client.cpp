#include <data/io/main.hpp>
#include <data/io/random.hpp>
#include <data/io/arg_parser.hpp>
#include <data/io/log.hpp>

#include <Cosmos/types.hpp>
#include "Cosmos.hpp"
#include "server/generate.hpp"
#include "server/restore.hpp"
#include "server/split.hpp"
#include "server/spend.hpp"

#include <Cosmos/boost/miner_options.hpp>

error run (const args::parsed &p);

using error = data::io::error;

namespace data {
    void signal_handler (int signal) noexcept {
        exit (0);
    }

    error main (std::span<const char *const> rr) {

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

namespace schema = data::schema;

auto inline prefix (method m) {
    return schema::list::value<std::string> () + schema::list::equal<method> (m);
}

auto inline no_params (method m) {
    return args::command (set<std::string> {}, prefix (m), schema::map::empty ());
}

auto inline call_options () {
    return (schema::map::key<net::URL> ("url") || schema::map::key<net::URL> ("URL"));
}

net::HTTP::request make_HTTP_request (method m, const args::parsed &p) {
    switch (m) {

        // TODO URL parameter
        case method::SHUTDOWN: {
            auto validated = args::validate (p, no_params (method::SHUTDOWN));
            return request_shutdown ();
        }

        case method::ADD_ENTROPY: {
            auto [_a, _b, entropy_string] = std::get<1> (
                args::validate (p,
                    args::command {
                        set<std::string> {},
                        prefix (method::ADD_ENTROPY) + schema::list::value<std::string> (),
                        schema::map::empty ()}));
            return request_add_entropy (entropy_string);
        }

        case method::LIST_WALLETS: {
            auto validated = args::validate (p, no_params (method::LIST_WALLETS));
            return request_list_wallets ();
        }

        case method::GENERATE: return generate_request_options (p).request ();

        case method::VALUE: return request_value ();

        case method::IMPORT: return request_import (p);

        case method::RESTORE: return restore_request_options (p).request ();

        case method::SPEND: return spend_request_options (p).request ();

        case method::SPLIT: return split_request_options (p).request ();

        case method::BOOST: return BoostPOW::script_options::read (p).request ();
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
            auto req = make_HTTP_request (m, p);
            // TODO try to dial into the server
            return {};
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
