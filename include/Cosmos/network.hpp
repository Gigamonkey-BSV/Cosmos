#ifndef COSMOS_NETWORK
#define COSMOS_NETWORK

#include <gigamonkey/pay/MAPI.hpp>
#include <Cosmos/network/whatsonchain.hpp>
#include <ctime>

namespace Cosmos {
    namespace MAPI = Gigamonkey::MAPI;
    using satoshis_per_byte = Gigamonkey::satoshis_per_byte;

    struct broadcast_error {
        enum error {
            none,
            unknown,
            network_connection_fail,
            insufficient_fee,
            invalid_transaction
        };

        error Error;

        broadcast_error (error e): Error {e} {}
        broadcast_error (): Error {none} {}

        // broadcast_error is equivalent to true when the
        // operation succeeds.
        operator bool () {
            return Error == none;
        }
    };

    std::ostream &operator << (std::ostream &, broadcast_error);

    struct network {
        net::asio::io_context IO;
        ptr<net::HTTP::SSL> SSL;
        whatsonchain WhatsOnChain;
        MAPI::client Gorilla;
        net::HTTP::client_blocking CoinGecko;
        
        network () : IO {}, SSL {std::make_shared<net::HTTP::SSL> (net::HTTP::SSL::tlsv12_client)},
            WhatsOnChain {SSL}, Gorilla {net::HTTP::REST {"https", "mapi.gorillapool.io"}},
            CoinGecko {net::HTTP::REST {"https", "api.coingecko.com"}, tools::rate_limiter {1, 10}} {
            SSL->set_default_verify_paths ();
            SSL->set_verify_mode (net::asio::ssl::verify_peer);
        }
        
        bytes get_transaction (const Bitcoin::TXID &);
        
        satoshis_per_byte mining_fee ();
        
        broadcast_error broadcast (const bytes &tx);

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
