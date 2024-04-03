#include <Cosmos/wallet/change.hpp>
#include <gigamonkey/script/pattern/pay_to_address.hpp>
#include <gigamonkey/script/pattern/pay_to_pubkey.hpp>

#include <math.h>

namespace Cosmos {

    // construct a set of change outputs.
    change make_change_parameters::operator ()
        (const pubkeychain &pk, Bitcoin::satoshi val, satoshi_per_byte fees, data::crypto::random &r) const {

        change cx {{}, pk};

        if (val < MinimumCreateValue) return cx;

        double v = double (val);

        while (v > MinimumCreateValue) {
            Bitcoin::satoshi next_value;
            if (v <= MinimumSplitValue) next_value = floor (v);
            else if (ExpectedSplitFraction * v * 2 > MaximumSplitValue) {
                next_value = std::ceil (std::uniform_real_distribution<double> (MinimumCreateValue, MaximumSplitValue) (r));
            } else {
                next_value = std::ceil (std::uniform_real_distribution<double> (MinimumCreateValue, ExpectedSplitFraction * v * 2) (r));
            }

            uint64 expected_size = pay_to_address::redeem_expected_size (true);

            auto derived = cx.Chain.last (cx.Chain.Change);

            cx.Change <<= redeemable {
                Bitcoin::output {next_value, pay_to_address::script (derived.Pubkey.address_hash ())},
                    {derived.Derivation}, expected_size};

            cx.Chain = cx.Chain.next (cx.Chain.Change);

            v -= double (expected_size) * double (fees) + double (next_value);
        }

        return cx;

    }
}
