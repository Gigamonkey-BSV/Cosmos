#ifndef COSMOS_WALLET_CHANGE
#define COSMOS_WALLET_CHANGE

#include <gigamonkey/timechain.hpp>
#include <data/crypto/random.hpp>
#include <Cosmos/wallet/account.hpp>
#include <Cosmos/options.hpp>

namespace Cosmos {

    using address_source = Gigamonkey::address_source;

    struct change {
        data::cross<redeemable> Change;

        // last key used +1
        int32 Last;

        list<Bitcoin::output> outputs () const {
            list<Bitcoin::output> o;
            for (auto &p : Change) o <<= p.Prevout;
            return o;
        }
    };

    // construct a set of change outputs.
    using make_change = data::function<change (Bitcoin::satoshi, satoshis_per_byte fees, address_source &, data::crypto::entropy &)>;

    // default make_change function
    struct make_change_parameters {
        // the minimum value of a change output. Below this value, no output will be created
        Bitcoin::satoshi MinimumCreateValue {spend_options::DefaultMinChangeValue};

        // the minimum value of an output that will be created by splitting
        // a larger value.
        Bitcoin::satoshi MinimumSplitValue;

        // the maximum value of an output that will be created by splitting
        // a larger value off.
        Bitcoin::satoshi MaximumSplitValue;

        // the fraction of a value to split off into a separate output.
        double ExpectedSplitFraction;

        // construct a set of change outputs.
        change operator () (Bitcoin::satoshi, satoshis_per_byte fees, key_source, data::crypto::entropy &) const;
    };
}

#endif

