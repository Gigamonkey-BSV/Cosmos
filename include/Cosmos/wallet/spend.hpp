#ifndef COSMOS_WALLET_WALLET
#define COSMOS_WALLET_WALLET

#include <Cosmos/wallet/account.hpp>
#include <Cosmos/wallet/select.hpp>
#include <Cosmos/wallet/change.hpp>
#include <Cosmos/wallet/sign.hpp>
#include <chrono>

namespace Cosmos {

    struct spend {
        select Select;
        make_change Change;
        data::crypto::random &Random;

        struct tx {
            nosig::transaction Transaction;
            map<Bitcoin::index, redeemable> Insert;
            map<Bitcoin::index, Bitcoin::outpoint> Remove;
        };

        struct spent {
            list<tx> Transactions;

            key_source Addresses;

            bool valid () const {
                return data::size (Transactions) != 0;
            }

            spent () : Transactions {}, Addresses {} {}
            spent (list<tx> txs, key_source a) : Transactions {txs}, Addresses {a} {}
        };

        spent operator () (
            account, key_source addresses,
            // payee's outputs
            list<Bitcoin::output> to,
            satoshis_per_byte fees = {1, 100},
            uint32 lock = 0) const;
    };

}

#endif
