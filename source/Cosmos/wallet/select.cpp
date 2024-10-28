
#include <Cosmos/wallet/select.hpp>

namespace Cosmos {

    namespace {

        struct drop_down {
            // can go faster by using std::map
            std::map<Bitcoin::outpoint, redeemable> Result;

            Bitcoin::satoshi SpentValue;

            uint64 InputsExpectedSize;

            struct removable {
                double Weight;
                Bitcoin::outpoint Point;
            };

            // randomly remove outputs until we have an acceptable subset.
            void reduce (
                Bitcoin::satoshi value_to_spend,
                satoshis_per_byte fees,
                double optimal_outputs_per_spend,
                double min_change_value,
                double min_change_fraction,
                data::crypto::random &r) {

                while (true) {

                    // go through all prevouts and figure out which ones can be removed.
                    list<removable> rmv {};

                    for (const auto &[key, value] : Result) {
                        uint64 removed_inputs_expected_size = InputsExpectedSize - value.expected_input_size ();

                        double output_value = double (value.Prevout.Value);

                        double removed_spent_value = double (SpentValue) - output_value;
                        double removed_val_with_fee = double (value_to_spend) + double (fees) * double (removed_inputs_expected_size);

                        if (removed_spent_value <= removed_val_with_fee + double (min_change_value) ||
                            removed_spent_value <= removed_val_with_fee * (min_change_fraction + 1)) continue;

                        double optimal_value_per_output = removed_val_with_fee / optimal_outputs_per_spend;

                        double weight = output_value > optimal_value_per_output ?
                            output_value / optimal_value_per_output :
                            optimal_value_per_output / output_value;

                        rmv <<= {weight, key};

                    }

                    // if we cannot remove any then we are done.
                    if (size (rmv) == 0) return;

                    // randomly select an output to remove based on the weights.
                    cross<double> weights (size (rmv));
                    auto wi = weights.begin ();
                    for (const auto &[weight, _] : rmv) {
                        *wi = weight;
                        wi++;
                    }

                    // select a prevout to be removed.
                    uint32 selected_removable_index = crypto::select_index_by_weight (weights, r);
                    auto remove_key = rmv[selected_removable_index].Point;

                    auto removed = Result.find (remove_key);

                    InputsExpectedSize -= removed->second.expected_input_size ();
                    SpentValue -= removed->second.Prevout.Value;

                    Result.erase (removed);

                }
            }

            drop_down (
                const account &acc,
                Bitcoin::satoshi value_to_spend,
                satoshis_per_byte fees,
                uint32 optimal_outputs_per_spend,
                Bitcoin::satoshi min_change_value,
                double min_change_fraction,
                data::crypto::random &r): Result {}, SpentValue {acc.value ()}, InputsExpectedSize {0} {

                // are enough funds available to make the payment?
                if (SpentValue <= value_to_spend) throw exception {3} << "not enough funds to make payment.";

                // generate expected size of the inputs to the tx.
                for (const auto &[key, value] : acc) {
                    Result[key] = value;
                    InputsExpectedSize += value.expected_input_size ();
                }

                // in these cases, we cannot satisfy MinChangeFraction or MinChangeValue with the funds
                // available in the wallet, so we continue with everything selected.
                if (SpentValue > value_to_spend + min_change_value &&
                    double (SpentValue) > double (value_to_spend) * (min_change_fraction + 1))
                    reduce (value_to_spend, fees, double (optimal_outputs_per_spend),
                        double (min_change_value), min_change_fraction, r);
            }
        };

    }

    // select outputs from a wallet sufficient for the given value.
    selected select_down::operator ()
        (const account &acc, Bitcoin::satoshi value_to_spend, satoshis_per_byte fees, data::crypto::random &r) const {

        // we go through a process of selecting inputs to spend out of those in the wallet.
        // We do this by selecting all inputs and then removing outputs one by one until we can
        // no longer remove any and still satisfy MinChangeFraction and MinChangeValue.

        // We do it this way because it's easier to find outputs that we don't want to spend
        // than outputs that we do want to spend.

        drop_down dropped {acc, value_to_spend, fees, OptimalOutputsPerSpend, MinChangeValue,
            MinChangeFraction == MaxChangeFraction ? MinChangeFraction :
                std::uniform_real_distribution<double> {MinChangeFraction} (r), r};

        // double check that the selection is good
        double spend_val_with_fee = double (value_to_spend) + double (fees) * double (dropped.InputsExpectedSize);
        if (spend_val_with_fee > dropped.SpentValue) throw exception {3} <<
            "could not satisfy input selection requirements because " << spend_val_with_fee << " > " << dropped.SpentValue;

        list<entry<Bitcoin::outpoint, redeemable>> selected_outputs;
        for (const auto &[key, value] : dropped.Result) selected_outputs <<= data::entry<Bitcoin::outpoint, redeemable> {key, value};

        // shuffle before returning.
        return shuffle (selected_outputs, r);
    }

    selected select_up_biggest::operator ()
        (const account &acc, Bitcoin::satoshi value_to_spend, satoshis_per_byte fees, data::crypto::random &r) const {

        throw exception {} << "unimplemented method select_up_biggest::operator ()";
    }

    selected select_up_random::operator ()
        (const account &acc, Bitcoin::satoshi value_to_spend, satoshis_per_byte fees, data::crypto::random &r) const {

        throw exception {} << "unimplemented method select_up_random::operator ()";
    }

    selected select_up_and_down::operator ()
        (const account &acc, Bitcoin::satoshi value_to_spend, satoshis_per_byte fees, data::crypto::random &r) const {

        throw exception {} << "unimplemented method select_up_and_down::operator ()";
    }
}
