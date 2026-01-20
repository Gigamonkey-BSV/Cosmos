#ifndef COSMOS_NETWORK
#define COSMOS_NETWORK

#include <gigamonkey/pay/MAPI.hpp>
#include <gigamonkey/pay/ARC.hpp>
#include <Cosmos/network/whatsonchain.hpp>
#include <data/io/log.hpp>
#include <ctime>

using extended_transaction = Gigamonkey::extended::transaction;
using satoshis_per_byte = Gigamonkey::satoshis_per_byte;

namespace Cosmos {
    namespace MAPI = Gigamonkey::MAPI;
    namespace ARC = Gigamonkey::ARC;

    struct broadcast_result {
        enum result {
            SUCCESS,
            ERROR_UNKNOWN,
            ERROR_NETWORK_CONNECTION_FAIL,
            ERROR_INAUTHENTICATED,
            ERROR_INSUFFICIENT_FEE,
            ERROR_INVALID
        };

        result Error;
        // an HTTP JSON error object if provided, as used in the ARC protocol.
        net::error Details;

        broadcast_result (result e, net::error deets = JSON (nullptr)): Error {e}, Details {deets} {}
        broadcast_result (): Error {SUCCESS}, Details (JSON (nullptr)) {}

        // broadcast_result is equivalent to true when the
        // operation succeeds.
        operator bool () const {
            return Error == SUCCESS;
        }

        bool error () const {
            return Error != SUCCESS;
        }

        bool success () const {
            return Error == SUCCESS;
        }
    };

    struct broadcast_single_result : broadcast_result {
        ARC::status Status {JSON (nullptr)};
        using broadcast_result::broadcast_result;
        broadcast_single_result (const ARC::status &stat): broadcast_result {}, Status (stat) {}
    };

    struct broadcast_multiple_result : broadcast_result {
        list<ARC::status> Status;
        using broadcast_result::broadcast_result;
        broadcast_multiple_result (list<ARC::status> stats): broadcast_result {}, Status (stats) {}
    };

    std::ostream &operator << (std::ostream &, broadcast_result);

    enum monetary_unit {
        UNKNOWN_MONETARY_UNIT,
        USD
    };

    std::ostream &operator << (std::ostream &, monetary_unit);

    struct network {
        net::asio::io_context IO;
        ptr<net::HTTP::SSL> SSL;
        whatsonchain WhatsOnChain;
        MAPI::client Gorilla;
        net::HTTP::client CoinGecko;
        ARC::client TAAL;

        network () : IO {}, SSL {std::make_shared<net::HTTP::SSL> (net::HTTP::SSL::tlsv12_client)},
            WhatsOnChain {SSL}, Gorilla {SSL, net::HTTP::REST {"https", "mapi.gorillapool.io"}},
            CoinGecko {SSL, net::HTTP::REST {"https", "api.coingecko.com"}, data::tools::rate_limiter {1, data::milliseconds {10}}},
            // TODO I don't know what to put for TAAL's rate limiter.
            TAAL {SSL, net::HTTP::REST {"https", "arc.taal.com"}, data::tools::rate_limiter {1, data::milliseconds {10}}} {
            SSL->set_default_verify_paths ();
            SSL->set_verify_mode (net::asio::ssl::verify_peer);
        }
        
        awaitable<maybe<bytes>> get_transaction (const Bitcoin::TXID &);
        
        awaitable<satoshis_per_byte> mining_fee ();
        
        // standard tx format and extended are allowed.
        awaitable<broadcast_single_result> broadcast (const extended_transaction &tx);
        awaitable<broadcast_multiple_result> broadcast (list<extended_transaction> tx);

        awaitable<double> price (monetary_unit, const Bitcoin::timestamp &);
        
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
                return double (data::synced (&network::mining_fee, &Net));
            } catch (std::exception &e) {
                DATA_LOG (warning) << "Warning! Exception caught while trying to get a fee quote: " << e.what ();
                return Default;
            }
        }
    };
    
}

#endif
