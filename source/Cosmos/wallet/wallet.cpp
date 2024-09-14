#include <Cosmos/wallet/wallet.hpp>

namespace Cosmos {

    spend::spent spend::operator () (redeem r,
        keychain k, addresses addrs, account acc,
        list<Bitcoin::output> to,
        satoshis_per_byte fees,
        uint32 lock) const {

        using namespace Gigamonkey;

        Bitcoin::satoshi value_to_spend = data::fold
            ([] (Bitcoin::satoshi val, const Bitcoin::output &o) -> Bitcoin::satoshi {
                return val + o.Value;
            }, Bitcoin::satoshi {0}, to);

        Bitcoin::satoshi value_available = acc.value ();

        // do we have enough funds to spend everything we want to spend?
        if (value_available < value_to_spend) throw exception {3} << "insufficient funds: " << value_available << " < " << value_to_spend;

        list<redeemer> inputs;
        account_diff diff;

        // select funds to be spent and organize them into redeemers and keep
        // track of outputs that will be removed from the wallet.
        for (const entry<Bitcoin::outpoint, redeemable> &e : Select (acc, value_to_spend, fees, Random)) {
            inputs <<= redeemer {
                for_each ([&k] (const derivation &x) -> sigop {
                    return sigop {k.derive (x)};
                }, e.Value.Derivation),
                Bitcoin::prevout {e.Key, e.Value.Prevout},
                e.Value.ExpectedScriptSize,
                Bitcoin::input::Finalized,
                e.Value.UnlockScriptSoFar};
            diff.Remove = diff.Remove.insert (e.Key);
        }

        // transform selected into inputs
        redeemable_transaction design_before_change {1, inputs, to, lock};

        // how much do we need to make in change?
        satoshis_per_byte fee_rate_before_change {design_before_change.fee (), design_before_change.expected_size ()};

        // what change needs to be accounted for in order to make this tx?
        Bitcoin::satoshi change_amount
            {floor (double (int64 (fee_rate_before_change.Satoshis)) - double (fees) * fee_rate_before_change.Bytes)};

        // make change outputs.
        change ch = Change (addrs.Sequences[addrs.Change], change_amount, fees, Random);

        auto change_outputs = ch.outputs ();

        // randomly order the new outputs.
        cross<size_t> outputs_ordering = random_ordering (to.size () + change_outputs.size (), Random);

        // shuffle outputs and construct tx.
        redeemable_transaction design {1, inputs, shuffle (change_outputs + to, outputs_ordering), lock};

        // Is the fee for this transaction sufficient?
        if (design.fee_rate () < fees) throw exception {3} << "failed to generate tx with sufficient fees";

        // redeem transaction.
        extended_transaction complete = design.redeem (r);

        if (!complete.valid ()) throw exception {3} << "invalid tx generated";

        // add new change outputs to account.
        diff.TXID = complete.id ();
        for (int i = 0; i < change_outputs.size (); i++) {
            size_t change_index = outputs_ordering[i];
            diff.Insert = diff.Insert.insert (outputs_ordering[i++], ch.Change[change_index]);
        }

        // return new wallet.
        return spent {{{complete, diff}}, addrs.update (addrs.Change, ch.Last)};
    }
}

