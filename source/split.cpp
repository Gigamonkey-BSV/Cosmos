#include "interface.hpp"
#include "Cosmos.hpp"

namespace Cosmos {

    // we organize by script hash because we want to
    // redeem all identical scripts in the same tx.
    using splitable = map<digest256, list<entry<Bitcoin::outpoint, redeemable>>>;

    // collect splitable scripts by addres.
    splitable get_split_address (Interface::writable u, splitable x, const Bitcoin::address &addr) {
        const auto w = u.get ().wallet ();
        auto TXDB = u.txdb ();
        if (!bool (TXDB) | !bool (w)) throw exception {} << "could not load wallet";

        // is this address in our wallet?
        ordered_list<ray> outpoints = TXDB->by_address (addr);

        list<entry<Bitcoin::outpoint, redeemable>> outputs;

        if (outputs.size () == 0) return x;

        // this step doesn't entirely make sense because if we could
        // redeem any output with this address we could redeem every one.
        for (const ray &r : outpoints)
            if (const auto *en = w->Account.contains (r.Point); bool (en)) {
                outputs <<= entry<Bitcoin::outpoint, redeemable> {r.Point, *en};
            } else throw exception {} << "WARNING: output " << r.Point << " for address " << addr <<
                " was not found in our account even though we believe we own this address.";

        return x.insert (Gigamonkey::SHA2_256 (pay_to_address::script (addr.digest ())), outputs);

    };

    struct top_splitable {
        digest256 ScriptHash;
        Bitcoin::satoshi Value;
        list<entry<Bitcoin::outpoint, redeemable>> Outputs;

        bool operator < (const top_splitable &t) const {
            return Value > t.Value;
        }

        bool operator <= (const top_splitable &t) const {
            return Value >= t.Value;
        }
    };

    priority_queue<top_splitable> get_top_splitable (splitable x) {
        priority_queue<top_splitable> top;
        for (const auto &e : x) {
                top = top.insert (top_splitable {e.Key,
                fold ([] (const Bitcoin::satoshi val, const entry<Bitcoin::outpoint, redeemable> &e) {
                    return val + e.Value.Prevout.Value;
            }, Bitcoin::satoshi {0}, e.Value), e.Value});
        }
        return top;
    }
}

void command_split (const arg_parser &p) {
    using namespace Cosmos;
    Interface e {};
    read_wallet_options (e, p);
    read_random_options (e, p);

    maybe<double> max_sats_per_output = double (options::DefaultMaxSatsPerOutput);
    maybe<double> mean_sats_per_output = double (options::DefaultMeanSatsPerOutput);
    maybe<double> min_sats_per_output = double (options::DefaultMinSatsPerOutput);
    maybe<double> fee_rate = double (options::default_fee_rate ());

    p.get ("min_sats", min_sats_per_output);
    p.get ("max_sats", max_sats_per_output);
    p.get ("mean_sats", mean_sats_per_output);
    p.get ("fee_rate", fee_rate);

    Cosmos::split split {int64 (*min_sats_per_output), int64 (*max_sats_per_output), *mean_sats_per_output};

    splitable x;

    // the user may provide an address or xpub to split. If it is provided, we look for
    // outputs in our wallet corresponding to that address or to addressed derived
    // sequentially from the pubkey.

    maybe<string> address_string;
    p.get (3, "address", address_string);

    if (bool (address_string)) {
        Bitcoin::address addr {*address_string};
        HD::BIP_32::pubkey pk {*address_string};
        if (!pk.valid () && !addr.valid ()) throw exception {6} << "could not read address string";

        if (addr.valid ())
            x = e.update<splitable> ([&addr] (Interface::writable u) {
                return get_split_address (u, splitable {}, addr);
            });
        else {

            // if we're using a pubkey we need a max_look_ahead.
            maybe<uint32> max_look_ahead;
            p.get (4, "max_look_ahead", max_look_ahead);
            if (!bool (max_look_ahead)) max_look_ahead = options {}.MaxLookAhead;

            x = e.update<splitable> ([&pk, &max_look_ahead] (Interface::writable u) {

                splitable z {};
                int32 since_last_unused = 0;
                address_sequence seq {pk, {}};

                while (since_last_unused < *max_look_ahead) {
                    size_t last_size = z.size ();
                    z = get_split_address (u, z, pay_to_address_signing (seq.last ()).Key);
                    seq = seq.next ();
                    if (z.size () == last_size) {
                        since_last_unused++;
                    } else since_last_unused = 0;
                }

                return z;
            });
        }
    } else {
        // in this case, we just look for all outputs in our account that we are able to split.
        x = e.update<splitable> ([&max_sats_per_output] (Interface::writable u) {

            const auto w = u.get ().wallet ();
            if (!bool (w)) throw exception {} << "could not load wallet";

            splitable z;

            for (const auto &[key, value]: w->Account)
                // find all outputs that are worth splitting
                if (value.Prevout.Value > *max_sats_per_output) {
                    z = z.insert (
                        Gigamonkey::SHA2_256 (value.Prevout.Script),
                        {entry<Bitcoin::outpoint, redeemable> {key, value}},
                        [] (list<entry<Bitcoin::outpoint, redeemable>> o, list<entry<Bitcoin::outpoint, redeemable>> n)
                            -> list<entry<Bitcoin::outpoint, redeemable>> {
                                return o + n;
                            });
                }

            // go through all the smaller outputs and check on whether the same script exists in our list
            // of big outputs since we don't want to leave any script partially redeemed and partially not.
            for (const auto &[key, value]: w->Account)
                // find all outputs that are worth splitting
                if (value.Prevout.Value <= *max_sats_per_output) {
                    digest256 script_hash = Gigamonkey::SHA2_256 (value.Prevout.Script);
                    if (z.contains (script_hash))
                        z = z.insert (script_hash,
                            {entry<Bitcoin::outpoint, redeemable> {key, value}},
                            [] (list<entry<Bitcoin::outpoint, redeemable>> o, list<entry<Bitcoin::outpoint, redeemable>> n)
                                -> list<entry<Bitcoin::outpoint, redeemable>> {
                                    return o + n;
                                });
                }

            return z;
        });
    }

    if (data::size (x) == 0) throw exception {1} << "No outputs to split";

    size_t num_outputs {0};
    Bitcoin::satoshi total_split_value {0};
    for (const auto &e : x) {
        num_outputs += e.Value.size ();
        for (const auto &v : e.Value) total_split_value += v.Value.Prevout.Value;
    }

    std::cout << "found " << x.size () << " scripts in " << num_outputs <<
        " outputs to split with a total value of " << total_split_value << std::endl;

    // TODO estimate fees here.

    auto top = get_top_splitable (x);

    e.update<void> ([&split, &top, fee_rate] (Cosmos::Interface::writable u) {

        int count = 0;
        std::cout << "top 10 splittable scripts: " << std::endl;
        {
            auto view_top = top;
            while (!view_top.empty ()) {
                const auto &t = view_top.first ();

                if (count++ >= 10) break;
                std::cout << "\t" << t.Value << " sats in script " << t.ScriptHash << " in " << std::endl;
                for (const auto &o : t.Outputs) std::cout << "\t\t" << o.Key << std::endl;

                view_top = view_top.rest ();
            }
        }

        // we had a problem earlier where I tried to dereference this pointer
        // which destroyed the ptr object. This is definitely kinda confusing,
        // since you can safely dereference other pointers returned by Interface.
        ptr<Cosmos::price_data> price_data = u.price_data ();
        auto &txdb = *u.txdb ();
        /*
        // commented out since we don't have a reliable source of price data right now.
        Bitcoin::timestamp now = Bitcoin::timestamp::now ();
        double current_price = *(price_data->get (now));
        tax::capital_gain Gain {};

        std::cout << "generating tax implications; current price " << current_price << std::endl;
        {
            auto tax_top = top;
            while (!tax_top.empty ()) {
                const auto &t = tax_top.first ();

                for (const auto &o : t.Outputs) {
                    std::cout << " A" << std::endl;
                    Bitcoin::satoshi amount = o.Value.Prevout.Value;
                    std::cout << " B" << std::endl;
                    Bitcoin::timestamp buy_time = txdb[o.Key.Digest].when ();
                    std::cout << " C" << std::endl;
                    double buy_price = *(price_data->get (buy_time));
                    std::cout << "price at " << buy_time << " was " << buy_price << std::endl;
                    if (buy_price > current_price) Gain.Loss += (buy_price - current_price) * int64 (amount);
                    // we assume no leap days or seconds.
                    else if (uint32 (now) - uint32 (buy_time) < 31536000)
                        Gain.ShortTerm += (current_price - buy_price) * int64 (amount);
                    else Gain.LongTerm += (current_price - buy_price) * int64 (amount);
                }

                tax_top = tax_top.rest ();
            }
        }

        std::cout << "tax implications should these splits be created now are \n" << std::endl;
        std::cout << Gain << std::endl;*/

        if (!get_user_yes_or_no ("Do you want to continue?")) throw exception {} << "program aborted";

        list<spend::spent> split_txs;
        wallet old = *u.get ().wallet ();

        {
            while (!top.empty ()) {
                const auto &t = top.first ();

                std::cout << " Splitting script with hash " << t.ScriptHash << " containing value " << t.Value << " over " <<
                    t.Outputs.size () << " output" << (t.Outputs.size () == 1 ? "" : "s") << "." << std::endl;
                spend::spent spent = split (Gigamonkey::redeem_p2pkh_and_p2pk, *u.random (), *u.get ().keys (), old, t.Outputs, *fee_rate);

                account new_account = old.Account;

                std::cout << " Produced transactions " << std::endl;

                // each split may give us several new transactions to work with.
                for (const auto &[extx, diff] : spent.Transactions) {
                    new_account <<= diff;
                    std::cout << "  " << extx.id () << "\n  of size " << extx.serialized_size () <<
                        " with " << extx.Outputs.size () << " outputs and " << extx.fee () << " in fees." << std::endl;
                }

                split_txs <<= spent;
                old = wallet {old.Pubkeys, spent.Addresses, new_account};

                top = top.rest ();
            }
        }

        int number_of_transactions {0};
        size_t total_size {0};
        Bitcoin::satoshi total_fee {0};

        for (const auto &x : split_txs) for (const auto &[tx, _] : x.Transactions) {
            number_of_transactions++;
            total_size += tx.serialized_size ();
            total_fee += tx.fee ();
        }

        std::cout << "Transactions have been generated!" << std::endl;
        std::cout << "  number of transactions: " << number_of_transactions << std::endl;
        std::cout << "  total size: " << total_size << std::endl;
        std::cout << "  total fees: " << total_fee << std::endl;
        // Put some data here like fees and so on.

        if (!get_user_yes_or_no ("Do you want broadcast these transactions?")) throw exception {} << "program aborted";

        std::cout << "broadcasting split transactions" << std::endl;

        for (const spend::spent &sp : split_txs) u.broadcast (sp);

    });
}