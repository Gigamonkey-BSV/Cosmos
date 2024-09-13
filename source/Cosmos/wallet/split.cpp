
#include <Cosmos/wallet/split.hpp>

namespace Cosmos {

    const uint32 output_size = 34;

    // here e_x means exponential of x. Thus the variables are not all independent.
    double log_triangular_distribution_mean (double a, double b, double m, double e_a, double e_b, double e_m);

    double inline log_triangular_distribution_mean (double a, double b, double m, double e_a, double e_b) {
        return log_triangular_distribution_mean (a, b, m, e_a, e_b, exp (m));
    }

    double inline log_triangular_distribution_mean (double a, double b, double m) {
        return log_triangular_distribution_mean (a, b, m, exp (a), exp (b));
    }

    double inline log_triangular_distribution_mean (double a, double b, double m, double e_a, double e_b, double e_m) {
        return (((e_m * (a - m + 1) - e_a) / (a - m)) + ((e_m * (m - b - 1) + e_b) / (b - m))) * 2 / (b - a);
    }

    // when m -> a or m -> b we get a limit of 0 / 0 so we have to have a special case for those cases, which
    // represent the maximum and minimum allowed values of the mean.
    double min_log_triangular_distribution_mean (double a, double b, double e_a, double e_b);
    double max_log_triangular_distribution_mean (double a, double b, double e_a, double e_b);

    double inline min_log_triangular_distribution_mean (double a, double b) {
        return min_log_triangular_distribution_mean (a, b, exp (a), exp (b));
    }

    double inline max_log_triangular_distribution_mean (double a, double b) {
        return max_log_triangular_distribution_mean (a, b, exp (a), exp (b));
    }

    double inline min_log_triangular_distribution_mean (double a, double b, double e_a, double e_b) {
        return (e_a * (a - b - 1) + e_b) * 2 / ((b - a) * (b - a));
    }

    double inline max_log_triangular_distribution_mean (double a, double b, double e_a, double e_b) {
        return (e_b * (a - b + 1) - e_a) * 2 / ((a - b) * (b - a));
    }

    double find_triangle_mode (double a, double b, double mean, double e_a, double e_b) {
        double min = a;
        double max = b;
        double guess, m;

        while (true) {
            // it's possible to converge a lot faster than this but let's see if this works well enough.
            double m = (max - min) / 2 + min;
            double guess = log_triangular_distribution_mean (a, b, m, e_a, e_b);
            if (std::max (mean - guess, guess - mean) > 1) return m;
            (guess > mean ? max : min) = m;
        }
    }

    split::log_triangular_distribution::log_triangular_distribution (double min, double max, double mean) {

        if (max < min) throw exception {} <<
            "log triangular distribution: max (" << max << ") must not be less than min (" << min << ")";

        if (mean > max) throw exception {} <<
            "log triangular distribution: mean (" << mean << ") must not be greater than max (" << max << ")";

        if (mean < min) throw exception {} <<
            "log triangular distribution: mean (" << mean << ") must not be less than min (" << min << ")";

        double a = ln (double (min));
        double b = ln (double (max));

        double min_mean = min_log_triangular_distribution_mean (a, b, min, max);
        double max_mean = max_log_triangular_distribution_mean (a, b, min, max);

        if (mean < min_mean) throw exception {} <<
            "Minimum possible mean value for max " << max << " and min " << min << " is " << min_mean;

        if (mean > max_mean) throw exception {} <<
            "Maximum possible mean value for max " << max << " and min " << min << " is " << max_mean;

        Triangular = math::triangular_distribution<double> {a, find_triangle_mode (a, b, mean, min, max), b};
    }

    split::result_outputs split::operator () (data::crypto::random &r, address_sequence key,
        Bitcoin::satoshi split_value, double fee_rate) const {

        list<redeemable> outputs {};

        int64 remaining_split_value = split_value;

        while (true) {
            // how many outputs will we have by the next iteration?
            uint32 outputs_size_next = outputs.size () + 1;

            // how many fees will we have accumulated by the next iteration?
            int64 expected_fees_next = ceil (fee_rate * (Bitcoin::var_int::size (outputs_size_next) + (outputs_size_next) * output_size));

            // remaining number of satoshies, after the cost of all the expected outputs is taken into account,
            int64 expected_remainder = remaining_split_value - expected_fees_next;

            // this will only happen if not enough sats are provided initially.
            if (expected_remainder < MinSatsPerOutput) throw exception {} << "too few sats to split!";

            // round up.
            int64 random_value = int64 (LogTriangular (r) + .5);

            // if the remaining sats will be too few, just make a final output using all that's left.
            bool we_are_done = expected_remainder - random_value < MinSatsPerOutput;

            int64 output_value = we_are_done ? expected_remainder : random_value;

            entry<Bitcoin::address, signing> last_address = pay_to_address_signing (key.last ());

            outputs = outputs << redeemable {
                Bitcoin::output {Bitcoin::satoshi {output_value}, pay_to_address::script (last_address.Key.digest ())},
                last_address.Value.Derivation,
                last_address.Value.ExpectedScriptSize,
                last_address.Value.UnlockScriptSoFar};

            if (we_are_done) return result_outputs {shuffle (outputs, r), key.Last};

            remaining_split_value -= output_value;

            key = key.next ();
        }
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

        extended_transaction completed = redeemable_transaction {1,
            for_each ([k] (const entry<Bitcoin::outpoint, redeemable> &e) -> redeemer {
                return redeemer {
                    list<sigop> {sigop {k.derive (first (e.Value.Derivation))}},
                    Bitcoin::prevout {e.Key, e.Value.Prevout},
                    e.Value.ExpectedScriptSize};
            }, selected), for_each ([] (const auto &x) -> Bitcoin::output {
                return x.Prevout;
            }, split_outputs.Outputs), 0}.redeem (ree);

        if (!completed.valid ()) throw exception {6} << "invalid tx generated";

        account_diff diff;
        diff.TXID = completed.id ();

        // update account information.
        for (const auto &e : selected) diff.Remove = diff.Remove.insert (e.Key);

        uint32 i = 0;
        for (const auto &o : split_outputs.Outputs) diff.Insert = diff.Insert.insert (i++, o);

        // construct the final result.
        return result {{{completed, diff}}, x.Last};

    }

}

