#ifndef COSMOS_WALLET_SELECT
#define COSMOS_WALLET_SELECT

#include <gigamonkey/timechain.hpp>
#include <data/random.hpp>
#include <Cosmos/wallet/account.hpp>

namespace crypto = data::crypto;

namespace Cosmos {

    using selected = dispatch<Bitcoin::outpoint, redeemable>;

    // select outputs from a wallet sufficient for the given value, plus the tx cost of the outputs selected.
    using select = data::function<selected (const account &, Bitcoin::satoshi, satoshis_per_byte fees, data::random::entropy &)>;

    // default select function
    struct select_down {
        // how many outputs should be selected ideally per spend operation.
        uint32 OptimalOutputsPerSpend;

        // The minimum amount that can go in a change output.
        Bitcoin::satoshi MinChangeValue;

        // How much of the amount spent should be change?
        double MinChangeFraction;
        double MaxChangeFraction;

        // select outputs from a wallet sufficient for the given value.
        selected operator () (const account &, Bitcoin::satoshi, satoshis_per_byte fees, data::random::entropy &) const;
    };

    // select the biggest outputs until we have enough.
    struct select_up_biggest {

        // The minimum amount that can go in a change output.
        Bitcoin::satoshi MinChangeValue;

        // How much of the amount spent should be change?
        double MinChangeFraction;
        double MaxChangeFraction;

        // select outputs from a wallet sufficient for the given value.
        selected operator () (const account &, Bitcoin::satoshi, satoshis_per_byte fees, data::random::entropy &) const;
    };

    // select random outputs until we have enough.
    struct select_up_random {

        // The minimum amount that can go in a change output.
        Bitcoin::satoshi MinChangeValue;

        // How much of the amount spent should be change?
        double MinChangeFraction;
        double MaxChangeFraction;

        // select outputs from a wallet sufficient for the given value.
        selected operator () (const account &, Bitcoin::satoshi, satoshis_per_byte fees, data::random::entropy &) const;
    };

    // combine select_up_random with select_down.
    struct select_up_and_down {
        // how many outputs should be selected ideally per spend operation.
        uint32 OptimalOutputsPerSpend;

        // The minimum amount that can go in a change output.
        Bitcoin::satoshi MinChangeValue;

        // How much of the amount spent should be change?
        double MinChangeFraction;
        double MaxChangeFraction;

        // select outputs from a wallet sufficient for the given value.
        selected operator () (const account &, Bitcoin::satoshi, satoshis_per_byte fees, data::random::entropy &) const;
    };

}

#endif
