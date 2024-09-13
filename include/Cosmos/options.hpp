#ifndef COSMOS_OPTIONS
#define COSMOS_OPTIONS

#include <gigamonkey/satoshi.hpp>

namespace Cosmos {

    // TODO we want to load these options from a file at some point.
    struct options {
        constexpr static int64 DefaultMaxSatsPerOutput {5000000};
        constexpr static int64 DefaultMinSatsPerOutput {123456};
        constexpr static double DefaultMeanSatsPerOutput {1234567};
        constexpr static uint64 DefaultFeeRate[2] {10, 1000};
        constexpr static uint32 DefaultMaxLookAhead {10};

        static satoshis_per_byte default_fee_rate () {
            static satoshis_per_byte dfr {Bitcoin::satoshi {static_cast<int64> (DefaultFeeRate[0])}, DefaultFeeRate[1]};
            return dfr;
        }

        Bitcoin::satoshi MaxSatsPerOutput {DefaultMaxSatsPerOutput};

        Bitcoin::satoshi MinSatsPerOutput {DefaultMinSatsPerOutput};

        double MeanSatsPerOutput {DefaultMeanSatsPerOutput};

        satoshis_per_byte FeeRate {default_fee_rate ()};

        uint32 MaxLookAhead {DefaultMaxLookAhead};
    };
}

#endif
