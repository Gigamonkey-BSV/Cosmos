#include <Cosmos/wallet/wallet.hpp>

namespace Cosmos {

    void wallet::import_output (const Bitcoin::prevout &p, const Bitcoin::secret &x, uint64 expected_size, const bytes &script_code) {
        Account.Account[p.Key] = redeemable {p.Value, list<derivation> {derivation {x.to_public (), {}}}, expected_size};
        Keys = Keys.insert (secret {x});
    }

    spent spend (wallet w, select s, make_change c, redeem r,
        data::crypto::random &rand,
        list<Bitcoin::output> to,
        satoshi_per_byte fees,
        uint32 lock) {

        using namespace Gigamonkey;

        Bitcoin::satoshi value_to_spend = data::fold
            ([] (Bitcoin::satoshi val, const Bitcoin::output &o) -> Bitcoin::satoshi {
                return val + o.Value;
            }, Bitcoin::satoshi {0}, to);

        Bitcoin::satoshi value_available;

        // do we have enough funds to spend everything we want to spend?
        if (value_available < value_to_spend) throw exception {3} << "insufficient funds: " << value_available << " < " << value_to_spend;

        // select funds
        selected to_spend = s (w.Account, value_to_spend, fees, rand);

        list<redeemer> inputs = for_each ([k = w.Keys] (const auto &e) -> redeemer {
            return redeemer {
                for_each ([&k] (const derivation &x) -> sigop {
                    return sigop {k.derive (x)};
                }, e.Value.Derivation),
                Bitcoin::prevout {e.Key, e.Value.Prevout},
                e.Value.ExpectedScriptSize,
                Bitcoin::input::Finalized,
                e.Value.UnlockScriptSoFar};
        }, to_spend.Selected);

        // transform selected into inputs
        redeemable_transaction design_before_change {1, inputs, to, lock};

        // how much do we need to make in change?
        satoshi_per_byte fee_rate_before_change {design_before_change.fee (), design_before_change.expected_size ()};

        // what change needs to be acconted for in order to make
        Bitcoin::satoshi change_amount
            {floor (double (int64 (fee_rate_before_change.Satoshis)) - double (fees) * fee_rate_before_change.Bytes)};

        // make change outputs.
        change ch = c (w.Pubkeys.Sequences[w.Pubkeys.Change], change_amount, fees, rand);

        auto change_outputs = ch.outputs ();

        // randomly order the new outputs.
        cross<size_t> outputs_ordering = random_ordering (to.size () + change_outputs.size (), rand);

        // shuffle outputs and construct tx.
        redeemable_transaction design {1, inputs, shuffle (change_outputs + to, outputs_ordering), lock};

        // Is the fee for this transaction sufficient?
        if (design.fee_rate () < fees) throw exception {3} << "failed to generate tx with sufficient fees";

        // redeem transaction.
        extended_transaction complete = design.redeem (r);

        if (!complete.valid ()) throw exception {3} << "invalid tx generated";

        auto txid = complete.id ();

        // add new change outputs to account.
        account new_account = to_spend.Rest;
        for (int i = 0; i < change_outputs.size (); i++) {
            size_t change_index = outputs_ordering[i];
            new_account.Account[Bitcoin::outpoint {txid, outputs_ordering[i++]}] = ch.Change[change_index];
        }

        // return new wallet.
        return spent {complete, {w.Keys, new_account, w.Pubkeys.update (w.Pubkeys.Change, ch.Last)}};
    }
}

