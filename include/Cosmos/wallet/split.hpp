#ifndef COSMOS_WALLET_SPLIT
#define COSMOS_WALLET_SPLIT

#include <gigamonkey/timechain.hpp>
#include <data/crypto/random.hpp>
#include <Cosmos/wallet/wallet.hpp>

namespace Cosmos {

    struct split {
        Bitcoin::satoshi MaxSatsPerOutput {5000000};
        Bitcoin::satoshi MinSatsPerOutput {123456};
        double MeanSatsPerOutput {123456};

        spent operator () (redeem, data::crypto::random &r, wallet w, const string &pubkey_name,
            list<entry<Bitcoin::outpoint, redeemable>> selected, double fee_rate) const;

        struct result_outputs {
            // list of new outputs
            list<redeemable> Outputs;
            uint32 Last;
        };

        result_outputs operator () (data::crypto::random &r, hd_pubkey_sequence key,
            Bitcoin::satoshi value, double fee_rate) const;
    };
}

#endif
