#ifndef COSMOS_WALLET_SPLIT
#define COSMOS_WALLET_SPLIT

#include <gigamonkey/timechain.hpp>
#include <data/crypto/random.hpp>
#include <Cosmos/wallet/wallet.hpp>

namespace Cosmos {

    struct split {
        Bitcoin::satoshi MaxSatsPerOutput {5000000};
        Bitcoin::satoshi MinSatsPerOutput {12345};
        double MeanSatsPerOutput {123456};

        spent operator () (redeem, data::crypto::random &, wallet, list<entry<Bitcoin::outpoint, redeemable>> selected, double fee_rate) const;

        struct result {
            extended_transaction Transaction;
            account Account;
            uint32 Last;
        };

        result operator () (redeem, data::crypto::random &, account, keychain k, address_sequence,
            list<entry<Bitcoin::outpoint, redeemable>> selected, double fee_rate) const;

        struct result_outputs {
            // list of new outputs
            list<redeemable> Outputs;
            uint32 Last;
        };

        result_outputs operator () (data::crypto::random &r, address_sequence key,
            Bitcoin::satoshi value, double fee_rate) const;
    };

    spent inline split::operator () (redeem ree, data::crypto::random &rand, wallet w,
        list<entry<Bitcoin::outpoint, redeemable>> selected, double fee_rate) const {
        result rr = (*this) (ree, rand, w.Account, w.Keys, w.Pubkeys.Sequences[w.Pubkeys.Change], selected, fee_rate);
        return spent {rr.Transaction, wallet {w.Keys, w.Account, w.Pubkeys.update (w.Pubkeys.Change, rr.Last)}};
    }
}

#endif
