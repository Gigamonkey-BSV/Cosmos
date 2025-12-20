#ifndef COSMOS_OPTIONS
#define COSMOS_OPTIONS

#include <gigamonkey/satoshi.hpp>

namespace Cosmos {

    // TODO we want to load these options from a file at some point.
    struct spend_options {
        constexpr static int64 DefaultMaxSatsPerTx {20};
        constexpr static int64 DefaultMinSatsPerTx {1};
        constexpr static double DefaultMeanSatsPerTx {4};

        constexpr static int64 DefaultMaxSatsPerOutput {54321000};
        constexpr static int64 DefaultMinSatsPerOutput {1234567};
        constexpr static double DefaultMeanSatsPerOutput {12345678};

        // TODO replace this with a constexpr satoshis_per_byte
        constexpr static uint64 DefaultFeeRate[2] {10, 1000};
        constexpr static int64 DefaultMinChangeSats {100};

        constexpr static double DefaultMinRedeemProportion {1.3};
        constexpr static double DefaultMaxRedeemProportion {2.5};

        // NOTE: this is not really a spend option and should be
        // moved somewhere else.
        constexpr static uint32 DefaultMaxLookAhead {10};

        static satoshis_per_byte default_fee_rate () {
            static satoshis_per_byte dfr {Bitcoin::satoshi {static_cast<int64> (DefaultFeeRate[0])}, DefaultFeeRate[1]};
            return dfr;
        }

        Bitcoin::satoshi MaxSatsPerTx {DefaultMaxSatsPerTx};

        Bitcoin::satoshi MinSatsPerTx {DefaultMinSatsPerTx};

        double MeanSatsPerTx {DefaultMeanSatsPerTx};

        Bitcoin::satoshi MaxSatsPerOutput {DefaultMaxSatsPerOutput};

        Bitcoin::satoshi MinSatsPerOutput {DefaultMinSatsPerOutput};

        double MeanSatsPerOutput {DefaultMeanSatsPerOutput};

        satoshis_per_byte FeeRate {default_fee_rate ()};

        Bitcoin::satoshi MinChangeSats {DefaultMinChangeSats};

        double MinRedeemProportion {DefaultMinRedeemProportion};
        double MaxRedeemProportion {DefaultMaxRedeemProportion};

        uint32 MaxLookAhead {DefaultMaxLookAhead};
    };
}

#endif
