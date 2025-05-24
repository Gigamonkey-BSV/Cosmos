
#include <regex>

#include <data/io/exception.hpp>
#include <data/crypto/NIST_DRBG.hpp>

#include <data/io/wait_for_enter.hpp>

#include <gigamonkey/timestamp.hpp>
#include <gigamonkey/script/pattern/pay_to_address.hpp>

#include <Cosmos/network.hpp>
#include <Cosmos/wallet/split.hpp>
#include <Cosmos/wallet/restore.hpp>
#include <Cosmos/database/price_data.hpp>
#include <Cosmos/options.hpp>
#include <Cosmos/tax.hpp>
#include <Cosmos/boost/miner_options.hpp>

#include "Cosmos.hpp"
#include "interface.hpp"

error run (const io::arg_parser &p);

int main (int arg_count, char **arg_values) {

    auto err = run (arg_parser {arg_count, arg_values});

    if (err.Message) {
        if (err.Code) std::cout << "Error: ";
        std::cout << static_cast<std::string> (*err.Message) << std::endl;
    } else if (err.Code) std::cout << "Error: unknown." << std::endl;

    return err.Code;
}

error run (const io::arg_parser &p) {

    try {

        if (p.has ("version")) {
            version (std::cout);
            std::cout << std::endl;
        }

        else if (p.has ("help")) {
            help (std::cout);
            std::cout << std::endl;
        }

        else {

            metd cmd = read_method (p);

            switch (cmd) {
                case VERSION: {
                    version (std::cout);
                    std::cout << std::endl;
                    break;
                }

                case HELP: {
                    help (std::cout, read_method (p, 2)) << std::endl;
                    break;
                }

                case GENERATE: {
                    command_generate (p);
                    break;
                }

                case VALUE: {
                    command_value (p);
                    break;
                }

                case RESTORE: {
                    command_restore (p);
                    break;
                }

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

                case SEND: {
                    command_send (p);
                    break;
                }

                case IMPORT: {
                    command_import (p);
                    break;
                }

                case SPLIT: {
                    command_split (p);
                    break;
                }

                case BOOST: {
                    command_boost (p);
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
                }

                default: {
                    std::cout << "Error: could not read user's command." << std::endl;
                    help (std::cout);
                    std::cout << std::endl;
                }
            }
        }

    } catch (const net::HTTP::exception &x) {
        std::cout << "Problem with http: " << std::endl;
        std::cout << "\trequest: " << x.Request << std::endl;
        std::cout << "\tresponse: " << x.Response << std::endl;
        return error {1, std::string {x.what ()}};
    } catch (const data::exception &x) {
        return error {x.Code, std::string {x.what ()}};
    } catch (const std::exception &x) {
        return error {1, std::string {x.what ()}};
    } catch (...) {
        return error {1};
    }

    return {};
}
