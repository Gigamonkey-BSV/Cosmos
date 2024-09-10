#ifndef COSMOS_OPTIONS
#define COSMOS_OPTIONS

#include <gigamonkey/satoshi.hpp>

namespace Cosmos {

    // TODO we want to load these options from a file at some point.
    struct options {
        constexpr static double DefaultMeanSatsPerOutput {1234567};

        Bitcoin::satoshi MaxSatsPerOutput {5000000};

        Bitcoin::satoshi MinSatsPerOutput {123456};

        double MeanSatsPerOutput {DefaultMeanSatsPerOutput};

        satoshis_per_byte FeeRate {50, 1000};

        uint32 MaxLookAhead {10};
    };
}

#endif
