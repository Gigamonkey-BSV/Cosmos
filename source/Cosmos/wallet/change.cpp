#include <Cosmos/wallet/change.hpp>
#include <gigamonkey/script/pattern/pay_to_pubkey.hpp>

#include <math.h>

namespace Cosmos {

    // construct a set of change outputs.
    change make_change_parameters::operator ()
        (Bitcoin::satoshi val, satoshis_per_byte fees, key_source x, data::entropy &r) const {

        list<redeemable> cx {};

        if (val >= MinimumCreateValue) {

            double v = double (val);

            while (v > MinimumCreateValue) {
                Bitcoin::satoshi next_value;
                if (v <= MinimumSplitValue) next_value = floor (v);
                next_value = ExpectedSplitFraction * v * 2 > MaximumSplitValue ?
                    std::ceil (std::uniform_real_distribution<double> (MinimumCreateValue, MaximumSplitValue) (r)):
                    std::ceil (std::uniform_real_distribution<double> (MinimumCreateValue, ExpectedSplitFraction * v * 2) (r));

                uint64 expected_size = pay_to_address::redeem_expected_size (true);

                entry<Bitcoin::address, signing> derived = make_pay_to_address (*x);
                ++x;

                cx <<= redeemable {
                    Bitcoin::output {next_value, pay_to_address::script (derived.Key.digest ())},
                        derived.Value};

                v -= double (expected_size) * double (fees) + double (next_value);
            }
        }

        return change {data::cross<redeemable> (cx), x.Index};

    }
}
