#ifndef COSMOS_NETWORK
#define COSMOS_NETWORK

#include <gigamonkey/pay/MAPI.hpp>
#include <gigamonkey/pay/ARC.hpp>
#include <Cosmos/network/whatsonchain.hpp>
#include <ctime>

namespace Cosmos {
    namespace MAPI = Gigamonkey::MAPI;
    namespace ARC = Gigamonkey::ARC;
    using extended_transaction = Gigamonkey::extended::transaction;
    using satoshis_per_byte = Gigamonkey::satoshis_per_byte;

    struct broadcast_error {
        enum error {
            none,
            unknown,
            network_connection_fail,
            inauthenticated,
            insufficient_fee,
            invalid_transaction
        };

        error Error;
        // an HTTP JSON error object if provided.
        net::error Details;

        broadcast_error (error e, net::error deets = JSON (nullptr)): Error {e}, Details {deets} {}
        broadcast_error (): Error {none}, Details (JSON (nullptr)) {}

        // broadcast_error is equivalent to true when the
        // operation succeeds.
        operator bool () const {
            return Error == none;
        }

        bool error () const {
            return Error != none;
        }

        bool success () const {
            return Error == none;
        }
    };

    struct broadcast_single_result : broadcast_error {
        ARC::status Status {JSON (nullptr)};
        using broadcast_error::broadcast_error;
        broadcast_single_result (const ARC::status &stat): broadcast_error {}, Status (stat) {}
    };

    struct broadcast_multiple_result : broadcast_error {
        list<ARC::status> Status;
        using broadcast_error::broadcast_error;
        broadcast_multiple_result (list<ARC::status> stats): broadcast_error {}, Status (stats) {}
    };

    std::ostream &operator << (std::ostream &, broadcast_error);

    struct network {
        net::asio::io_context IO;
        ptr<net::HTTP::SSL> SSL;
        whatsonchain WhatsOnChain;
        MAPI::client Gorilla;
        net::HTTP::client_blocking CoinGecko;
        ARC::client TAAL;

        network () : IO {}, SSL {std::make_shared<net::HTTP::SSL> (net::HTTP::SSL::tlsv12_client)},
            WhatsOnChain {SSL}, Gorilla {SSL, net::HTTP::REST {"https", "mapi.gorillapool.io"}},
            CoinGecko {SSL, net::HTTP::REST {"https", "api.coingecko.com"}, tools::rate_limiter {1, 10}},
            // TODO I don't know what to put for TAAL's rate limiter.
            TAAL {SSL, net::HTTP::REST {"https", "tapi.taal.com/arc"}, tools::rate_limiter {1, 10}} {
            SSL->set_default_verify_paths ();
            SSL->set_verify_mode (net::asio::ssl::verify_peer);
        }
        
        bytes get_transaction (const Bitcoin::TXID &);
        
        satoshis_per_byte mining_fee ();
        
        // standard tx format and extended are allowed.
        broadcast_single_result broadcast (const extended_transaction &tx);
        broadcast_multiple_result broadcast (list<extended_transaction> tx);

        double price (const Bitcoin::timestamp &);
        
    };
    
    struct fees {
        virtual double get () = 0;
        virtual ~fees () {}
    };
    
    struct given_fees : fees {
        double FeeRate;
        given_fees (double f) : FeeRate{f} {}
        double get () final override {
            return FeeRate;
        }
    };
    
    struct network_fees : fees {
        network &Net;
        double Default;
        network_fees (network &n, double d = .05) : Net {n}, Default {d} {}
        double get () final override {
            try {
                return double (Net.mining_fee ());
            } catch (std::exception &e) {
                std::cout << "Warning! Exception caught while trying to get a fee quote: " << e.what () << std::endl;
                return Default;
            }
        }
    };
    
}

#endif
