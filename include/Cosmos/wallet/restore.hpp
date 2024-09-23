#ifndef COSMOS_WALLET_RESTORE
#define COSMOS_WALLET_RESTORE

#include <Cosmos/wallet/wallet.hpp>

namespace Cosmos {

    struct restore {
        uint32 MaxLookAhead;
        // whether to check for derived addresses as well.
        bool CheckSubKeys;

        // result of a restored wallet
        struct restored {
            events History;
            list<account_diff> Account;

            // last key used (+1)
            uint32 Last;
        };

        restored operator () (TXDB &, address_sequence pubkey);

    };


}

#endif
