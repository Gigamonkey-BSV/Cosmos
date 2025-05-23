
#include <data/io/exception.hpp>
#include <data/crypto/NIST_DRBG.hpp>
#include <gigamonkey/schema/bip_44.hpp>
#include <gigamonkey/schema/bip_39.hpp>
#include <Cosmos/network.hpp>
#include <Cosmos/wallet/split.hpp>
#include "interface.hpp"

namespace Cosmos {

    spend_options read_tx_options (network &n, const arg_parser &p, bool online) {

        spend_options o {};

        maybe<double> max_sats_per_output;
        maybe<double> min_sats_per_output;
        maybe<double> mean_sats_per_output;
        maybe<double> fee_rate;

        p.get ("min_sats_per_output", min_sats_per_output);
        p.get ("max_sats_per_output", max_sats_per_output);
        p.get ("mean_sats_per_output", mean_sats_per_output);
        p.get ("fee_rate", fee_rate);

        if (bool (max_sats_per_output)) o.MaxSatsPerOutput = std::ceil (*max_sats_per_output);
        if (bool (min_sats_per_output)) o.MinSatsPerOutput = std::ceil (*min_sats_per_output);
        if (bool (mean_sats_per_output)) o.MeanSatsPerOutput = *mean_sats_per_output;

        maybe<satoshis_per_byte> sats_per_byte;
        if (bool (fee_rate)) *sats_per_byte = {Bitcoin::satoshi {int64 (ceil (*fee_rate * 1000))}, 1000};

        std::cout << "Checking network health" << std::endl;
        if (online) {
            auto health_response = synced (&ARC::client::health, n.TAAL);
            std::cout << "Get TAAL health: " << health_response << std::endl;
            if (bool (health_response)) {
                auto health = health_response.health ();
                if (!health.healthy ())
                    throw exception {} << "According to TALL, network is not healthy because " << health.reason ();
            }

            satoshis_per_byte network_fee_rate;
            auto policy_response = synced (&ARC::client::policy, n.TAAL);
            std::cout << "Get TAAL policy: " << policy_response << std::endl;
            if (bool (policy_response) && !bool (sats_per_byte)) {
                *sats_per_byte = policy_response.policy ().mining_fee ();
                // double it just to be safe.
                sats_per_byte->Satoshis *= 2;
            }
            std::cout << " new fee rate " << *sats_per_byte << std::endl;
        }

        if (bool (sats_per_byte)) o.FeeRate = *sats_per_byte;

        return o;

    }

    spend::spent Interface::writable::make_tx (list<Bitcoin::output> send_to, const spend_options &opts) {
        auto acc = I.account ();

        if (!bool (acc)) throw exception {} << "could not load wallet";

        // remove all pending payments from the account so that we don't
        // accidentally invalidate them with this payment.
        auto *p = I.payments ();
        if (!bool (p)) throw exception {} << "could not load payments";
        Cosmos::account pruned_account = *acc;
        for (const auto &[_, offer] : p->Proposals) for (const auto diff : offer.Diff) pruned_account <<= diff;

        return spend {
            select_down {4, 5000, .5, 5},
            split_change_parameters {opts}, *get_casual_random ()}
            (redeem_p2pkh_and_p2pk, *acc, opts.FeeRate);
    }

    void update_pending_transactions (Interface::writable u) {
        std::cout << "Updating wallet with network" << std::endl;
        auto txdb = u.txdb ();
        auto w = u.get ().wallet ();
        auto *p = u.get ().payments ();
        if (!bool (txdb) || !bool (w) || !bool (p)) throw exception {"could not connect to network and database"};

        // all txs that have been updated with merkle proofs.
        list<Bitcoin::TXID> mined;
        auto unconfirmed = txdb->unconfirmed ();

        std::cout << " found " << unconfirmed.size () << " unconfirmed txs." << std::endl;
        for (const Bitcoin::TXID &txid : unconfirmed) if ((*txdb)[txid]->confirmed ()) mined <<= txid;
        std::cout << " of these " << mined.size () << " were mined since the last time the program was run." << std::endl;

        // update the unconfirmed txs in history.
        auto *h = u.history ();

        // look for payments that have been made which have been accepted by the network.
        Cosmos::account pruned_account = w->Account;
        map<string, payments::offer> new_proposals {};
        for (const auto &proposal : p->Proposals) {
            bool broadcast = true;
            list<Bitcoin::TXID> ids;
            for (const auto diff : proposal.Value.Diff)
                if (synced (&cached_remote_TXDB::import_transaction, txdb, diff.TXID)) {
                    // catching an error means that we have already accounted for this tx in our account.
                    try {
                        pruned_account <<= diff;
                    } catch (account::cannot_apply_diff) {
                        continue;
                    }

                    ids <<= diff.TXID;

                    auto v = (*txdb)[diff.TXID];

                    events e;
                    for (const auto &[i, _]: diff.Remove) e <<= event {v, i, direction::in};
                    for (const auto &[i, _]: diff.Insert) e <<= event {v, i, direction::out};
                    *h <<= e;

                } else broadcast = false;
            h->Payments = h->Payments << history::payment {proposal.Key, proposal.Value.Request.Value, ids};
            // if the payment was not entirely broadcast we keep it to try again later.
            if (!broadcast) new_proposals = new_proposals.insert (proposal);
        }

        u.set_payments (Cosmos::payments {p->Requests, new_proposals});
        std::cout << " done updating." << std::endl;

    }

    broadcast_tree_result Interface::writable::broadcast (list<std::pair<Bitcoin::transaction, account_diff>> payment) {

        auto w = I.wallet ();
        if (!bool (w)) throw exception {1} << "could not load wallet";
        Cosmos::wallet next_wallet = *w;

        // this will throw an exception if any of the diffs are incompatible.
        for (const auto &[_, diff] : payment) next_wallet.Account <<= diff;

        // we assume that the proof exists and can be generated.
        auto success = synced (&cached_remote_TXDB::broadcast, txdb (),
            *SPV::generate_proof (*txdb (),
                for_each ([] (const auto p) -> Bitcoin::transaction {
                    return p.first;
                }, payment)));

        // update wallet.
        if (bool (success)) set_wallet (next_wallet);

        return success;
    }

    void read_both_chains_options (Interface &e, const arg_parser &p) {
        auto &name = e.wallet_name ();
        p.get (2, "name", name);
        if (bool (name)) return;

        auto &keychain_filepath = e.keychain_filepath ();
        auto &pubkeys_filepath = e.pubkeys_filepath ();
        p.get ("keychain_filepath", keychain_filepath);
        p.get ("pubkeys_filepath", pubkeys_filepath);
        if (!bool (keychain_filepath)) throw data::exception {1} << "could not read filepath of keychain";
        if (!bool (pubkeys_filepath)) throw data::exception {1} << "could not read filepath of pubkeys";
    }

    void read_pubkeys_options (Interface &e, const arg_parser &p) {
        auto &name = e.wallet_name ();
        p.get (2, "name", name);
        if (bool (name)) return;

        auto &filepath = e.pubkeys_filepath ();
        p.get ("filepath", filepath);
        if (bool (filepath)) return;
        p.get ("pubkeys_filepath", filepath);
        if (bool (filepath)) return;

        throw data::exception {1} << "could not read filepath of pubkeys";
    }

    void read_account_and_txdb_options (Interface &e, const arg_parser &p) {
        auto &name = e.wallet_name ();
        p.get (2, "name", name);
        if (bool (name)) return;

        auto &txdb_filepath = e.txdb_filepath ();
        auto &account_filepath = e.account_filepath ();
        p.get ("txdb_filepath", txdb_filepath);
        p.get ("account_filepath", account_filepath);
        if (!bool (txdb_filepath)) std::cout << "local tx database not provided; using remote only";
        if (!bool (account_filepath)) throw data::exception {1} << "could not read filepath of account";
    }

    void read_random_options (const arg_parser &p) {
        // no options set up for random yet.
    }

    maybe<filepath> &Interface::keychain_filepath () {
        if (!bool (KeychainFilepath) && bool (Name)) {
            std::stringstream ss;
            ss << *Name << ".keychain.json";
            KeychainFilepath = ss.str ();
        }

        return KeychainFilepath;
    }

    maybe<filepath> &Interface::pubkeys_filepath () {
        if (!bool (PubkeychainFilepath) && bool (Name)) {
            std::stringstream ss;
            ss << *Name << ".pubkeys.json";
            PubkeychainFilepath = ss.str ();
        }

        return PubkeychainFilepath;
    }

    maybe<filepath> &Interface::txdb_filepath () {
        if (!bool (TXDBFilepath) && bool (Name)) {
            std::stringstream ss;
            ss << "txdb.json";
            TXDBFilepath = ss.str ();
        }

        return TXDBFilepath;
    }

    maybe<filepath> &Interface::account_filepath () {
        if (!bool (AccountFilepath) && bool (Name)) {
            std::stringstream ss;
            ss << *Name << ".account.json";
            AccountFilepath = ss.str ();
        }

        return AccountFilepath;
    }

    maybe<filepath> &Interface::addresses_filepath () {
        if (!bool (AddressesFilepath) && bool (Name)) {
            std::stringstream ss;
            ss << *Name << ".addresses.json";
            AddressesFilepath = ss.str ();
        }

        return AddressesFilepath;
    }

    maybe<filepath> &Interface::price_data_filepath () {
        if (!bool (PriceDataFilepath) && bool (Name)) {
            std::stringstream ss;
            ss << "price_data.json";
            PriceDataFilepath = ss.str ();
        }

        return PriceDataFilepath;
    }

    maybe<filepath> &Interface::events_filepath () {
        if (!bool (HistoryFilepath) && bool (Name)) {
            std::stringstream ss;
            ss << *Name << ".history.json";
            HistoryFilepath = ss.str ();
        }

        return HistoryFilepath;
    }

    maybe<std::string> &Interface::payments_filepath () {
        if (!bool (PaymentsFilepath) && bool (Name)) {
            std::stringstream ss;
            ss << *Name << ".payments.json";
            PaymentsFilepath = ss.str ();
        }

        return PaymentsFilepath;
    }

    network *Interface::net () {
        if (!bool (Net)) Net = std::make_shared<network> ();

        return Net.get ();
    }

    local_TXDB *Interface::get_local_txdb () {
        if (!bool (LocalTXDB)) {
            auto txf = txdb_filepath ();
            if (bool (txf)) {
                auto [json, files] = Files.load (*txf);
                Files = files;
                LocalTXDB = std::static_pointer_cast<Cosmos::local_TXDB>
                (std::make_shared<JSON_local_TXDB> (json));
            }
            else return nullptr;
        }

        return LocalTXDB.get ();
    }

    cached_remote_TXDB *Interface::get_txdb () {
        if (!bool (TXDB)) {

            auto n = net ();
            auto l = get_local_txdb ();
            if (!bool (n) || !bool (l)) return nullptr;

            TXDB = std::make_shared<cached_remote_TXDB> (*n, *l);
        }

        return TXDB.get ();
    }

    account *Interface::get_account () {

        if (!bool (Account)) {
            auto af = account_filepath ();
            if (bool (af)) {
                auto [json, files] = Files.load (*af);
                Files = files;
                Account = std::make_shared<Cosmos::account> (json);
            }
        }

        return Account.get ();
    }

    addresses *Interface::get_addresses () {

        if (!bool (Addresses)) {
            auto df = addresses_filepath ();
            if (bool (df)) {
                auto [json, files] = Files.load (*df);
                Files = files;
                Addresses = std::make_shared<Cosmos::addresses> (json);
            }
        }

        return Addresses.get ();
    }

    price_data *Interface::get_price_data () {
        if (PriceData == nullptr) {
            if (LocalPriceData == nullptr) {
                auto pdf = price_data_filepath ();
                if (!bool (pdf)) return nullptr;

                auto [json, files] = Files.load (*pdf);
                Files = files;
                LocalPriceData = std::static_pointer_cast<Cosmos::local_price_data>
                (std::make_shared<Cosmos::JSON_price_data> (json));
            }

            // We do not have remote price data at this time.
            /*
            auto n = net ();
            if (!bool (n)) return LocalPriceData.get ();

            PriceData = std::make_shared<Cosmos::cached_remote_price_data> (*n, *LocalPriceData);
            */

            PriceData = std::make_shared<Cosmos::cached_remote_price_data> (std::make_shared<ask_for_price_data> (), *LocalPriceData);
        }

        return PriceData.get ();
    }

    maybe<wallet> Interface::get_wallet () {

        auto acc = account ();
        auto pkc = pubkeys ();
        auto add = addresses ();

        if (!bool (acc) || !bool (pkc) || !bool (add)) return {};

        return {Cosmos::wallet {*pkc, *add, *acc}};
    }

    history *Interface::get_history () {
        if (!bool (Events)) {
            auto hf = events_filepath ();
            if (bool (hf)) {
                auto [json, files] = Files.load (*hf);
                Files = files;
                Events = std::make_shared<Cosmos::history> (json, *get_txdb ());
            }
        }

        return Events.get ();
    }

    payments *Interface::get_payments () {
        if (!bool (Payments)) {
            auto pf = payments_filepath ();
            if (bool (pf)) {
                auto [json, files] = Files.load (*pf);
                Files = files;
                Payments = std::make_shared<Cosmos::payments> (json);
            }
        }

        return Payments.get ();
    }

    Interface::~Interface () {
        if (!Written) return;

        auto tf = txdb_filepath ();
        auto af = account_filepath ();
        auto df = addresses_filepath ();
        auto hf = events_filepath ();
        auto pf = pubkeys_filepath ();
        auto kf = keychain_filepath ();
        auto pdf = price_data_filepath ();
        auto yf = payments_filepath ();

        if (bool (tf) && bool (LocalTXDB))
            Files.write (*tf, JSON (dynamic_cast<JSON_local_TXDB &> (*LocalTXDB)));

        if (bool (af) && bool (Account)) Files.write (*af, JSON (*Account));

        if (bool (df) && bool (Addresses)) Files.write (*df, JSON (*Addresses));

        if (bool (hf) && bool (Events)) Files.write (*hf, JSON (*Events));

        if (bool (pf) && bool (Pubkeys)) Files.write (*pf, JSON (*Pubkeys));

        if (bool (yf) && bool (Payments)) Files.write (*yf, JSON (*Payments));

        if (bool (kf) && bool (Keys)) Files.write (*kf, JSON (*Keys));

        if (bool (pdf) && bool (LocalPriceData))
            Files.write (*pdf, JSON (dynamic_cast<JSON_price_data &> (*LocalPriceData)));

    }

}
