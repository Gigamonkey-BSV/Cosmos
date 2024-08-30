
#include <Cosmos/wallet/split.hpp>
#include <data/math/probability/triangular_distribution.hpp>

namespace Cosmos {

    split::result_outputs split::operator () (data::crypto::random &r, address_sequence key,
        Bitcoin::satoshi split_value, double fee_rate) const {

        if (MaxSatsPerOutput < MinSatsPerOutput) throw exception {} << "MaxSatsPerOutput must not be less than MinSatsPerOutput";
        if (MeanSatsPerOutput > double (MaxSatsPerOutput)) throw exception {} << "MeanSatsPerOutput must not be greater than MaxSatsPerOutput";
        if (MeanSatsPerOutput < double (MinSatsPerOutput)) throw exception {} << "MeanSatsPerOutput must not be less than MixSatsPerOutput";

        uint32 output_size = 34;
        uint32 num_outputs = ceil (double (int64 (split_value)) / double (MeanSatsPerOutput + fee_rate * output_size));

        int64 remainder = int64 (split_value) - ceil (fee_rate * (Bitcoin::var_int::size (num_outputs) +     // num outputs
            num_outputs * output_size));

        if (remainder < MinSatsPerOutput) throw exception {} << "too few sats to split!";

        list<redeemable> outputs {};

        double min_sats_per_output = double (MinSatsPerOutput);
        for (int i = 0; i < num_outputs; i++) {
            double max_sats_per_output = std::min (double (remainder) - min_sats_per_output * (num_outputs - i), double (MaxSatsPerOutput));
            double mean_sats_per_output = double (num_outputs - i) / double (remainder);
            double mode_sats_per_output = mean_sats_per_output * 3 - max_sats_per_output - min_sats_per_output;

            int64 random_value = int64 (math::triangular_distribution<double>
                {min_sats_per_output, mode_sats_per_output, max_sats_per_output} (r) + .5);

            remainder -= random_value;

            entry<Bitcoin::address, signing> last_address = key.last ();

            outputs = outputs << redeemable {
                Bitcoin::output {Bitcoin::satoshi {random_value}, pay_to_address::script (last_address.Key.digest ())},
                last_address.Value.Derivation,
                last_address.Value.ExpectedScriptSize,
                last_address.Value.UnlockScriptSoFar};

            key = key.next ();
        }

        return result_outputs {shuffle (outputs, r), key.Last};
    }

    split::result split::operator () (redeem ree, data::crypto::random &rand, account a, keychain k, address_sequence x,
        list<entry<Bitcoin::outpoint, redeemable>> selected, double fee_rate) const {
        using namespace Gigamonkey;

        uint32 expected_tx_size_other_than_outputs;
        {
            uint32 num_inputs = data::size (selected);
            uint32 redeem_script_size = Bitcoin::signature::MaxSize + 33 + 2;
            expected_tx_size_other_than_outputs =
                4 +                                            // version
                4 +                                            // locktime
                Bitcoin::var_int::size (num_inputs) +          // num inputs
                num_inputs * (40 + redeem_script_size +        // input size
                    Bitcoin::var_int::size (redeem_script_size));
        }

        Bitcoin::satoshi split_value;

        {
            for (const auto &[key, value] : selected) split_value += value.Prevout.Value;
            split_value -= ceil (expected_tx_size_other_than_outputs * fee_rate);
        }

        if (split_value < MinSatsPerOutput) throw exception {5} << "too few sats to split!";

        // generate new outputs
        result_outputs split_outputs = operator () (rand, x, split_value, fee_rate);

        list<Bitcoin::output> outputs;

        for (const auto &o : split_outputs.Outputs) outputs <<= o.Prevout;

        extended_transaction completed = redeemable_transaction {1,
            for_each ([k] (const entry<Bitcoin::outpoint, redeemable> &e) -> redeemer {
                return redeemer {
                    list<sigop> {sigop {k.derive (first (e.Value.Derivation))}},
                    Bitcoin::prevout {e.Key, e.Value.Prevout},
                    e.Value.ExpectedScriptSize};
            }, selected), outputs, 0}.redeem (ree);

        if (!completed.valid ()) throw exception {6} << "invalid tx generated";

        // construct the final result.
        result r {completed, a, x.Last};

        Bitcoin::TXID id = r.Transaction.id ();

        // update account information.
        for (const auto &e : selected) r.Account.Account.erase (e.Key);

        uint32 i = 0;
        for (const auto &o : split_outputs.Outputs) r.Account.Account[Bitcoin::outpoint {id, i++}] = o;

        return r;

    }

}

