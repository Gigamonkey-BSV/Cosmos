
#include <Cosmos/wallet/select.hpp>

namespace Cosmos {

    // select outputs from a wallet sufficient for the given value.
    selected select_output_parameters::operator ()
        (const account &acc, Bitcoin::satoshi value_to_spend, satoshis_per_byte fees, data::crypto::random &r) const {

        // we go through a process of selecting inputs to spend out of those in the wallet.
        // We do this by selecting all inputs and then removing outputs one by one until we can
        // no longer remove any and still satisfy MinChangeFraction and MinChangeValue.

        // We do it this way because it's easier to find outputs that we don't want to spend
        // than outputs that we do want to spend.

        // select all funds, determine total value of account.
        Bitcoin::satoshi spendable_value = acc.value ();

        // are enough funds available to make the payment?
        if (spendable_value <= value_to_spend) throw exception {3} << "not enough funds to make payment.";

        // generate expected size of the inputs to the tx.
        uint64 inputs_expected_size;
        for (const auto &[key, value] : acc) inputs_expected_size += value.expected_input_size ();

        auto result = acc;
        auto spent_value = spendable_value;

        // in these cases, we cannot satisfy MinChangeFraction or MinChangeValue with the funds
        // available in the wallet, so we continue with everything selected and go to the end.
        if (spendable_value <= value_to_spend + MinChangeValue ||
            double (spendable_value) <= double (value_to_spend) * (MinChangeFraction + 1)) goto end;

        struct removable {
            double Weight;
            Bitcoin::outpoint Point;
        };

        while (true) {

            // go through all prevouts and figure out which ones can be removed.
            list<removable> rmv {};

            uint32 i = 0;
            for (const auto &[key, value] : result) {
                i++;
                uint64 removed_inputs_expected_size = inputs_expected_size - value.expected_input_size ();

                double output_value = double (value.Prevout.Value);

                double removed_spent_value = double (spent_value) - output_value;
                double removed_val_with_fee = double (value_to_spend) + double (fees) * double (removed_inputs_expected_size);

                if (removed_spent_value <= removed_val_with_fee + double (MinChangeValue) ||
                    removed_spent_value <= removed_val_with_fee * (MinChangeFraction + 1)) continue;

                double optimal_value_per_output = removed_val_with_fee / double (OptimalOutputsPerSpend);

                double weight = output_value > optimal_value_per_output ?
                    output_value / optimal_value_per_output :
                    optimal_value_per_output / output_value;

                rmv <<= {weight, key};

            }

            // if we cannot remove any then we are done.
            if (size (rmv) == 0) goto end;

            // randomly select an output to remove based on the weights.
            cross<double> weights (size (rmv));
            auto wi = weights.begin ();
            for (const removable &re : rmv) {
                *wi = re.Weight;
                wi++;
            }

            // select a prevout to be removed.
            uint32 selected_removable_index = crypto::select_index_by_weight (weights, r);
            auto remove_key = rmv[selected_removable_index].Point;

            auto removed = result[remove_key];

            inputs_expected_size -= removed.expected_input_size ();
            spent_value -= removed.Prevout.Value;

            result = result.remove (remove_key);

        }

        end:

        // double check that the selection is good
        double val_with_fee = double (value_to_spend) + double (fees) * double (inputs_expected_size);
        if (val_with_fee < spent_value) throw exception {3} << "could not satisfy input selection requirements.";

        list<entry<Bitcoin::outpoint, redeemable>> selected_outputs;
        for (const auto &[key, value] : result) selected_outputs <<= data::entry<Bitcoin::outpoint, redeemable> {key, value};

        // shuffle before returning.
        return shuffle (selected_outputs, r);
    }

}
