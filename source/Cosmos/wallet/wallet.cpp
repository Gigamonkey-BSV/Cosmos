#include <Cosmos/wallet/wallet.hpp>

namespace Cosmos {

    Bitcoin::secret find_secret (keychain k, pubkeys p, derivation d) {
        // first we check in pubkeys for the parent key.
        if (const auto *v = p.contains (d.Parent); bool (v))
        // if it is there, we expand the derivation to include the parent's derivation.
            d = derivation {v->Parent, v->Path + d.Path};
        // otherwise we check in private keys for the parent.
        if (const auto *v = k.contains (d.Parent); bool (v))
            return Bitcoin::secret (HD::BIP_32::secret {*v}.derive (d.Path));
        return Bitcoin::secret {};
    }

    spend::spent spend::operator () (redeem r,
        keychain k, wallet w,
        list<Bitcoin::output> to,
        satoshis_per_byte fees,
        uint32 lock) const {

        namespace G = Gigamonkey;

        // TODO if we could we ought to estimate the size of the tx and
        // the fees required to spend it and include that in value_to_spend.
        Bitcoin::satoshi value_to_spend = data::fold
            ([] (Bitcoin::satoshi val, const Bitcoin::output &o) -> Bitcoin::satoshi {
                return val + o.Value;
            }, Bitcoin::satoshi {0}, to);

        Bitcoin::satoshi value_available = w.Account.value ();

        // do we have enough funds to spend everything we want to spend?
        if (value_available < value_to_spend) throw exception {3} << "insufficient funds: " << value_available << " < " << value_to_spend;

        list<G::redeemer> inputs;
        account_diff diff;

        // select funds to be spent and organize them into redeemers and keep
        // track of outputs that will be removed from the wallet.
        Bitcoin::index input_index = 0;
        for (const auto &[op, re] : Select (w.Account, value_to_spend, fees, Random)) {
            inputs <<= G::redeemer {
                for_each ([&k, w] (const derivation &x) -> G::sigop {
                    Bitcoin::secret sec = find_secret (k, w.Pubkeys, x);
                    if (!sec.valid ()) throw exception {} << "could not find secret key for " << x;
                    return G::sigop {sec};
                }, re.Derivation),
                Bitcoin::prevout {op, re.Prevout},
                re.ExpectedScriptSize,
                Bitcoin::input::Finalized,
                re.UnlockScriptSoFar};
            diff.Remove = diff.Remove.insert (input_index++, op);
        }

        // transform selected into inputs
        G::redeemable_transaction design_before_change {1, inputs, to, lock};

        // how much do we need to make in change?
        satoshis_per_byte fee_rate_before_change {design_before_change.fee (), design_before_change.expected_size ()};

        // what change needs to be accounted for in order to make this tx?
        Bitcoin::satoshi change_amount
            {floor (double (int64 (fee_rate_before_change.Satoshis)) - double (fees) * fee_rate_before_change.Bytes)};

        // make change outputs.

        change ch = Change (w.Addresses.Sequences[w.Addresses.Change], change_amount, fees, Random);

        auto change_outputs = ch.outputs ();

        // randomly order the new outputs.
        cross<size_t> outputs_ordering = random_ordering (to.size () + change_outputs.size (), Random);

        // shuffle outputs and construct tx.
        G::redeemable_transaction design {1, inputs, shuffle (change_outputs + to, outputs_ordering), lock};

        // Is the fee for this transaction sufficient?
        if (design.fee_rate () < fees) throw exception {3} << "failed to generate tx with sufficient fees";
        std::cout << " transaction design is complete. It has " << design.Inputs.size () << " inputs spending " << design.spent () << ", " <<
            design.Outputs.size () << " outputs sending " << design.sent () << " with fees " << design.fee  () << std::endl;
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
        return spent {{{complete, diff}}, w.Addresses.update (w.Addresses.Change, ch.Last)};
    }
}

