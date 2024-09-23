
#include <Cosmos/tax.hpp>

namespace Cosmos {

    // TODO not exactly accurate.
    const double one_year = 365 * 24 * 60 * 60;

    tax tax::calculate (TXDB &txs, price_data *pd, const history::episode &events) {

        if (events.History.size () == 0) return {{}, {}, events.Account};

        account current_account = events.Account;

        // potential income events.
        list<potential_income> income;

        // capital gain so far.
        capital_gain cg;

        for (const history::tx &e : events.History) {
            // at this point we know that n != n.end ();
            potential_income potential;
            potential.TXID = e.TXID;
            // we are assuming that we have a regular timestamp here
            // instead of unconfirmed or infinity. You'd think this would
            // be ok since people typically do taxes on past events.
            potential.Price = *pd->get (e.When.get<Bitcoin::timestamp> ());

            Bitcoin::satoshi current_moved {0};
            Bitcoin::satoshi current_income {0};

            for (const event &n : e.Events)
                if (n.Direction == direction::in) {
                    // this is either a potential income or a move event.
                    auto op = Bitcoin::input {n.put ()}.Reference;
                    if (auto x = current_account.find (op); x != current_account.end ()) {
                        // this is a move event, which means we have to calculate capital gain.
                        auto value = n.value ();
                        current_moved += value;
                        // when was the original output created?
                        Bitcoin::timestamp when = txs[op.Digest]->when ().get<Bitcoin::timestamp> ();
                        // At what price was it bought?
                        double buy_price = *pd->get (when);
                        // if the buy price is greater than the sell price, it's a capital loss.
                        if (potential.Price < buy_price) {
                            cg.Loss += (buy_price - potential.Price) * double (value);
                        // otherwise it's either a long or a short term capital gain.
                        } else (n->when ().get<Bitcoin::timestamp> () - when < one_year ? cg.ShortTerm : cg.LongTerm) +=
                            (potential.Price - buy_price) * double (value);
                    } else potential.Incoming <<= n;
                } else current_income += n.value ();

            history v {};
            v.Account = current_account;
            v <<= e.Events;

            if (current_income > current_moved)
                potential.Income = current_income - current_moved;

            if (potential.Income > 0) income <<= potential;

            current_account = v.Account;
        }

        return {cg, income, current_account};
    }
}
