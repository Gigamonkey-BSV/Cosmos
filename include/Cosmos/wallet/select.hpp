#ifndef COSMOS_WALLET_SELECT
#define COSMOS_WALLET_SELECT

#include <gigamonkey/timechain.hpp>
#include <data/crypto/random.hpp>
#include <Cosmos/wallet/account.hpp>

namespace Cosmos {

    struct selected {
        list<entry<Bitcoin::outpoint, redeemable>> Selected;
        account Rest;
    };

    // select outputs from a wallet sufficient for the given value, plus the tx cost of the outputs selected.
    using select = data::function<selected (const account &, Bitcoin::satoshi, satoshis_per_byte fees, data::crypto::random &)>;

    // default select function
    struct select_output_parameters {
        // how many outputs should be selected ideally per spend operation.
        uint32 OptimalOutputsPerSpend;

        // The minimum amount that can go in a change output.
        Bitcoin::satoshi MinChangeValue;

        // How much of the amount spent should be change?
        double MinChangeFraction;

        // select outputs from a wallet sufficient for the given value.
        selected operator () (const account &, Bitcoin::satoshi, satoshis_per_byte fees, data::crypto::random &) const;
    };
}

#endif
