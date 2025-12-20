#include <Cosmos/wallet/split.hpp>

namespace Cosmos {

    constexpr const uint32 output_size = 34;

    split::result_outputs split::construct_outputs (data::entropy &r, const key_source &k,
        Bitcoin::satoshi split_value, double fee_rate) const {

        list<redeemable> outputs {};

        int64 remaining_split_value = split_value;

        key_source key = k;

        while (true) {
            // how many outputs will we have by the next iteration?
            uint32 outputs_size_next = outputs.size () + 1;

            // how many fees will we have accumulated by the next iteration?
            int64 expected_fees_next = ceil (fee_rate * (Bitcoin::var_int::size (outputs_size_next) + (outputs_size_next) * output_size));

            // remaining number of satoshies, after the cost of all the expected outputs is taken into account,
            int64 expected_remainder = remaining_split_value - expected_fees_next;

            // this will only happen if not enough sats are provided initially.
            if (expected_remainder < MinSatsPerOutput)
                throw data::exception {} << "too few sats to split!";

            // round up.
            int64 random_value = int64 (LogTriangular (r) + .5);

            // if the remaining sats will be too few, just make a final output using all that's left.
            bool we_are_done = expected_remainder - random_value < MinSatsPerOutput;

            int64 output_value = we_are_done ?
                expected_remainder:
                random_value;

            entry<Bitcoin::address, signing> last_address =
                make_pay_to_address (*key);

            outputs = outputs << redeemable {
                Bitcoin::output {
                    Bitcoin::satoshi {output_value},
                    pay_to_address::script (last_address.Key.digest ())},
                last_address.Value};

            if (we_are_done) return result_outputs {outputs, key.Index};

            remaining_split_value -= output_value;

            key = key.next ();
        }
    }
/*
    split::result split::operator () (redeem ree, data::entropy &rand,
        keychain k, pubkeys p, address_sequence x,
        list<entry<Bitcoin::outpoint, redeemable>> selected, double fee_rate) const {
        using namespace Gigamonkey;
        std::cout << "splitting tx with mean size " << MeanSatsPerOutput << std::endl;
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
        split_outputs.Outputs = shuffle (shuffle (split_outputs.Outputs, rand));

        extended_transaction completed = redeemable_transaction {1,
            data::for_each ([k, p] (const data::entry<Bitcoin::outpoint, redeemable> &e) -> redeemer {

                auto d = first (e.Value.Derivation);
                Bitcoin::secret sec = find_secret (k, p, d);
                if (!sec.valid ()) throw exception {} << "could not find secret key for " << d;

                return redeemer {
                    data::list<sigop> {sigop {sec}},
                    Bitcoin::prevout {e.Key, e.Value.Prevout},
                    e.Value.ExpectedScriptSize};

            }, selected), for_each ([] (const auto &x) -> Bitcoin::output {
                return x.Prevout;
            }, split_outputs.Outputs), 0}.redeem (ree);

        if (!completed.valid ()) throw exception {6} << "invalid tx generated";

        account_diff diff;
        diff.TXID = completed.id ();

        // update account information.
        {
            Bitcoin::index input_index = 0;
            for (const auto &e : selected) diff.Remove = diff.Remove.insert (input_index++, e.Key);
        } {
            Bitcoin::index output_index = 0;
            for (const auto &o : split_outputs.Outputs) diff.Insert = diff.Insert.insert (output_index++, o);
        }

        // construct the final result.
        return result {{{completed, diff}}, split_outputs.Last};

    }

    change split_change_parameters::operator () (
        address_sequence x,
        Bitcoin::satoshi val,
        satoshis_per_byte fees,
        data::crypto::random &rand) const {

        if (val < MinimumCreateValue) return change {{}, x.Last};

        if (val < this->MinSatsPerOutput) {
            entry<Bitcoin::address, signing> derived = pay_to_address_signing (x.last ());
            return change {{redeemable {
                    Bitcoin::output {
                        Bitcoin::satoshi {int64 (val) - std::ceil (double (pay_to_address::redeem_expected_size (true)) * double (fees))},
                        pay_to_address::script (derived.Key.digest ())},
                    derived.Value}},
                x.Last + 1};
        }

        result_outputs z = (*this) (rand, x, val, double (fees));
        return change {cross<redeemable> (z.Outputs), z.Last};
    }*/

}

