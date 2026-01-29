#ifndef COSMOS_NETWORK_BROADCAST
#define COSMOS_NETWORK_BROADCAST

#include <gigamonkey/pay/MAPI.hpp>
#include <gigamonkey/pay/ARC.hpp>
#include <gigamonkey/SPV.hpp>
#include <data/net/error.hpp>
#include <Cosmos/types.hpp>

namespace Cosmos {
    namespace MAPI = Gigamonkey::MAPI;
    namespace ARC = Gigamonkey::ARC;
    using extended_transaction = Gigamonkey::extended::transaction;
    using satoshis_per_byte = Gigamonkey::satoshis_per_byte;

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

    std::ostream &operator << (std::ostream &, broadcast_result);

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

    // Since we might end up trying to broadcast multiple txs at once
    // all coming from a single proof, we have a way of collecting all
    // results that come up for all broadcasts so that we can handle
    // the tx appropriately in the database.
    struct broadcast_tree_result : broadcast_multiple_result {
        using broadcast_multiple_result::broadcast_multiple_result;
        map<Bitcoin::TXID, broadcast_single_result> Sub;
        broadcast_tree_result (const broadcast_multiple_result &r, map<Bitcoin::TXID, broadcast_single_result> nodes = {}):
        broadcast_multiple_result {r}, Sub {nodes} {}
    };


}

#endif
