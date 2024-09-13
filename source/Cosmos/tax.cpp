
#include <Cosmos/tax.hpp>

namespace Cosmos {

    const double one_year = 365 * 24 * 60 * 60;

    tax tax::calculate (txdb &txs, ptr<price_data> pd, const events::history &history) {

        if (history.Events.size () == 0) return {{}, {}, history.Account};

        account current_account = history.Account;

        // potential income events.
        list<potential_income> income;

        // capital gain so far.
        capital_gain cg;

        for (const events::event &e : history.Events) {
            // at this point we know that n != n.end ();
            potential_income potential;
            potential.TXID = e.TXID;
            potential.Price = *pd->get (e.When);

            Bitcoin::satoshi current_moved {0};
            Bitcoin::satoshi current_income {0};

            for (const ray &n : e.Events)
                if (n.direction () == direction::in) {
                    // this is either a potential income or a move event.
                    auto op = Bitcoin::input {n.Put}.Reference;
                    if (auto x = current_account.find (op); x != current_account.end ()) {
                        // this is a move event, which means we have to calculate capital gain.
                        auto value = n.value ();
                        current_moved += value;
                        // when was the original output created?
                        Bitcoin::timestamp when = txs[op.Digest].when ();
                        // At what price was it bought?
                        double buy_price = *pd->get (when);
                        // if the buy price is greater than the sell price, it's a capital loss.
                        if (potential.Price < buy_price) {
                            cg.Loss += (buy_price - potential.Price) * double (value);
                        // otherwise it's either a long or a short term capital gain.
                        } else (n.When - when < one_year ? cg.ShortTerm : cg.LongTerm) +=
                            (potential.Price - buy_price) * double (value);
                    } else potential.Incoming <<= n;
                } else current_income += n.value ();

            events v {};
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
