#include <Cosmos/wallet/spend.hpp>
#include <data/shuffle.hpp>

namespace Cosmos {

    namespace incomplete = Gigamonkey::Bitcoin::incomplete;

    spend::spent spend::operator () (
        redeem red, account acc,
        key_source addresses,
        // payee's outputs
        list<Bitcoin::output> to,
        satoshis_per_byte fees) const {

        // TODO if we could we ought to estimate the size of the tx and
        // the fees required to spend it and include that in value_to_spend.
        Bitcoin::satoshi value_to_spend = data::fold
            ([] (Bitcoin::satoshi val, const Bitcoin::output &o) -> Bitcoin::satoshi {
                return val + o.Value;
            }, Bitcoin::satoshi {0}, to);

        Bitcoin::satoshi value_available = acc.value ();

        // do we have enough funds to spend everything we want to spend?
        // It is possible that we would have more than value_to_spend
        // but then not enough for fees.
        if (value_available < value_to_spend)
            throw data::exception {} << "insufficient funds: " << value_available << " < " << value_to_spend;

        // we increase the value to spend so as to redeem some extra.
        double wallet_value_proportion = double (value_available) / double (value_to_spend);

        double max_redeem_proportion = wallet_value_proportion > (((MaxRedeemProportion - 1) * 2) + 1) ?
            MaxRedeemProportion: ((wallet_value_proportion - 1) / 2) + 1;

        double min_redeem_proportion = max_redeem_proportion > (((MinRedeemProportion - 1) * 2) + 1) ?
            MinRedeemProportion: ((max_redeem_proportion - 1) / 2) + 1;

        Bitcoin::satoshi value_to_redeem =
            std::ceil (value_to_spend * std::uniform_real_distribution<double> {MinRedeemProportion, MaxRedeemProportion} (Random));

        // construct map of inputs removed from the account.
        map<Bitcoin::index, Bitcoin::outpoint> removed;

        // list of inputs
        list<nosig::input> inputs;

        // construct list of inputs and map of removed elements.
        size_t input_index = 0;
        // select outputs to redeem.
        for (const auto &[op, re]: Select (acc, value_to_redeem, fees, Random)) {
            inputs <<= nosig::input {
                Bitcoin::prevout {op, re.Prevout},
                red (re.Prevout, {}, re.UnlockScriptSoFar)
            };
            removed = removed.insert (input_index++, op);
        }

        nosig::transaction design_before_change {1, inputs, to, 0};

        // how much do we need to make in change?
        satoshis_per_byte fee_rate_before_change {design_before_change.fee (), design_before_change.expected_size ()};

        // what change needs to be accounted for in order to make this tx?
        Bitcoin::satoshi change_amount
            {floor (double (int64 (fee_rate_before_change.Satoshis)) - double (fees) * fee_rate_before_change.Bytes)};

        // make change outputs.
        change ch = Change (change_amount, fees, addresses, Random);

        auto change_outputs = ch.outputs ();

        // randomly order the new outputs.
        data::cross<size_t> outputs_ordering = random_ordering (to.size () + change_outputs.size (), Random);

        nosig::transaction final_design {1, inputs, shuffle (change_outputs + to, outputs_ordering), 0};

        // Is the fee for this transaction sufficient?
        if (final_design.fee_rate () < fees)
            throw data::exception {3} << "failed to generate tx with sufficient fees";

        std::cout << " transaction design is complete. It has " << final_design.Inputs.size () << " inputs spending " << final_design.spent () << ", " <<
            final_design.Outputs.size () << " outputs sending " << final_design.sent () << " with fees " << final_design.fee  () << std::endl;

        // the list of new entries to be added to account.
        map<Bitcoin::index, redeemable> inserted;

        // add new change outputs to account.
        for (int i = 0; i < change_outputs.size (); i++) {
            size_t change_index = outputs_ordering[i];
            inserted = inserted.insert (change_index, ch.Change[i]);
        }

        // return new wallet.
        return spent {{{final_design, inserted, removed}}, key_source {ch.Last, addresses.Sequence}};
    }
}

