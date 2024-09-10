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
            ordered_list<ray> History;
            account Account;

            // last key used (+1)
            uint32 Last;

            Bitcoin::satoshi value () const {
                return account {Account}.value ();
            }
        };

        restored operator () (txdb &TXDB, address_sequence pubkey);

        watch_wallet operator () (txdb &TXDB, pubkeys);
    };


}

#endif
