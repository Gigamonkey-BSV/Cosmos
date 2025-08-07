#ifndef COSMOS_NETWORK_WHATSONCHAIN
#define COSMOS_NETWORK_WHATSONCHAIN

#include <Cosmos/types.hpp>
#include <data/net/JSON.hpp>
#include <data/net/HTTP_client.hpp>
#include <gigamonkey/merkle/proof.hpp>

namespace net = data::net;
using JSON = data::JSON;

namespace Cosmos {
    using N = data::N;

    struct whatsonchain : net::HTTP::client {
        struct UTXO {

            Bitcoin::outpoint Outpoint;
            Bitcoin::satoshi Value;
            uint32 Height;

            UTXO ();
            UTXO (const JSON &);
            bool valid () const;

            bool operator == (const UTXO &u) const {
                return Outpoint == u.Outpoint && Value == u.Value && Height == u.Height;
            }

            explicit operator JSON () const;

            friend std::ostream inline &operator << (std::ostream &o, const UTXO &u) {
                return o << "UTXO {" << u.Outpoint << ", " << u.Value << ", " << u.Height << "}";
            }

        };

        whatsonchain (ptr<net::HTTP::SSL> ssl) :
            net::HTTP::client {ssl,
                net::HTTP::REST {"https", "api.whatsonchain.com"},
                data::tools::rate_limiter {3, data::milliseconds {1000}}} {}

        whatsonchain (): net::HTTP::client {
            net::HTTP::REST {"https", "api.whatsonchain.com"},
            data::tools::rate_limiter {3, data::milliseconds {1000}}} {}

        static std::string write (const Bitcoin::TXID &);
        static Bitcoin::TXID read_TXID (const JSON &);

        struct addresses {
            struct balance {
                Bitcoin::satoshi Confirmed;
                Bitcoin::satoshi Unconfirmed;
            };

            awaitable<balance> get_balance (const Bitcoin::address &);

            // txids of all transactions that spend to or redeem from a given address.
            awaitable<list<Bitcoin::TXID>> get_history (const Bitcoin::address &);

            awaitable<list<UTXO>> get_unspent (const Bitcoin::address &address);

            whatsonchain &API;
        };

        addresses address ();

        struct merkle_proof {
            digest256 BlockHash;
            Merkle::proof Proof;
        };

        struct transactions {

            awaitable<bool> broadcast (const bytes& tx);

            awaitable<bytes> get_raw (const Bitcoin::TXID &);

            awaitable<JSON> tx_data (const Bitcoin::TXID &);

            awaitable<maybe<merkle_proof>> get_merkle_proof (const Bitcoin::TXID &);

            whatsonchain &API;
        };

        transactions transaction ();

        struct scripts {

            awaitable<list<UTXO>> get_unspent (const digest256 &script_hash);
            awaitable<list<Bitcoin::TXID>> get_history (const digest256 &script_hash);

            whatsonchain &API;

        };

        scripts script ();

        struct header {
            N Height;
            Bitcoin::header Header;
            header ();
            header (const N &n, const Bitcoin::header &h);
            bool valid () const;
        };

        struct blocks {
            // by block hash, not by merkle root.
            awaitable<header> get_header_by_hash (const digest256 &);

            // by height
            awaitable<header> get_header_by_height (const N &);

            whatsonchain &API;
        };

        blocks block ();

    };

    whatsonchain::addresses inline whatsonchain::address () {
        return addresses {*this};
    }

    whatsonchain::transactions inline whatsonchain::transaction () {
        return transactions {*this};
    }

    whatsonchain::scripts inline whatsonchain::script () {
        return scripts {*this};
    }

    whatsonchain::blocks inline whatsonchain::block () {
        return blocks {*this};
    }

    bool inline whatsonchain::UTXO::valid () const {
        return Value != 0;
    }

    inline whatsonchain::header::header () : Height {0}, Header {} {}

    inline whatsonchain::header::header (const N &n, const Bitcoin::header &h): Height {n}, Header {h} {}

    bool inline whatsonchain::header::valid () const {
        return Header.valid ();
    }
}

#endif

