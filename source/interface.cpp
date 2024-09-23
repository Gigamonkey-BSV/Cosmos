
#include <data/io/exception.hpp>
#include <data/crypto/NIST_DRBG.hpp>
#include <gigamonkey/schema/bip_44.hpp>
#include <gigamonkey/schema/bip_39.hpp>
#include <Cosmos/network.hpp>
#include <Cosmos/wallet/split.hpp>
#include <sv/random.h>
#include "interface.hpp"

namespace Cosmos {

    void update_pending_transactions (Interface::writable u) {
        auto txdb = u.txdb ();
        auto w = u.get ().wallet ();
        auto *h = u.history ();
        if (!bool (txdb) || !bool (w) || !bool (h)) throw exception {"could not connect to network and database"};

        // all txs that have been updated with merkle proofs.
        list<ptr<vertex>> mined;
        for (const Bitcoin::TXID &txid : txdb->unconfirmed ()) if (txdb->import_transaction (txid))
            mined <<= (*txdb)[txid];

        // history will be updated automatically next time the program loads.
        // we could take care of it right now by writing history to JSON and reading it back, but we don't.

        // TODO look for completed payments and put them in history.
    }

    broadcast_tree_result Interface::writable::broadcast (list<std::pair<Bitcoin::transaction, account_diff>> payment) {

        auto w = I.wallet ();
        if (!bool (w)) throw exception {1} << "could not load wallet";
        Cosmos::wallet next_wallet = *w;

        // this will throw an exception if any of the diffs are incompatible.
        for (const auto &[_, diff] : payment) next_wallet.Account <<= diff;

        // we assume that the proof exists and can be generated.
        auto success = txdb ()->broadcast (
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

    void read_random_options (Interface &e, const arg_parser &p) {
        // no options set up for random yet.
    }

    maybe<std::string> &Interface::keychain_filepath () {
        if (!bool (KeychainFilepath) && bool (Name)) {
            std::stringstream ss;
            ss << *Name << ".keychain.json";
            KeychainFilepath = ss.str ();
        }

        return KeychainFilepath;
    }

    maybe<std::string> &Interface::pubkeys_filepath () {
        if (!bool (PubkeychainFilepath) && bool (Name)) {
            std::stringstream ss;
            ss << *Name << ".pubkeys.json";
            PubkeychainFilepath = ss.str ();
        }

        return PubkeychainFilepath;
    }

    maybe<std::string> &Interface::txdb_filepath () {
        if (!bool (TXDBFilepath) && bool (Name)) {
            std::stringstream ss;
            ss << "txdb.json";
            TXDBFilepath = ss.str ();
        }

        return TXDBFilepath;
    }

    maybe<std::string> &Interface::account_filepath () {
        if (!bool (AccountFilepath) && bool (Name)) {
            std::stringstream ss;
            ss << *Name << ".account.json";
            AccountFilepath = ss.str ();
        }

        return AccountFilepath;
    }

    maybe<std::string> &Interface::addresses_filepath () {
        if (!bool (AddressesFilepath) && bool (Name)) {
            std::stringstream ss;
            ss << *Name << ".addresses.json";
            AddressesFilepath = ss.str ();
        }

        return AddressesFilepath;
    }

    maybe<std::string> &Interface::price_data_filepath () {
        if (!bool (PriceDataFilepath) && bool (Name)) {
            std::stringstream ss;
            ss << "price_data.json";
            PriceDataFilepath = ss.str ();
        }

        return PriceDataFilepath;
    }

    maybe<std::string> &Interface::events_filepath () {
        if (!bool (HistoryFilepath) && bool (Name)) {
            std::stringstream ss;
            ss << *Name << ".events.json";
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
            if (bool (txf))
                LocalTXDB = std::static_pointer_cast<Cosmos::local_TXDB>
                    (std::make_shared<JSON_local_TXDB> (read_JSON_local_TXDB_from_file (*txf)));
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
                Account = std::make_shared<Cosmos::account> (read_account_from_file (*af));
            }
        }

        return Account.get ();
    }

    addresses *Interface::get_addresses () {

        if (!bool (Addresses)) {
            auto df = addresses_filepath ();
            if (bool (df)) {
                Addresses = std::make_shared<Cosmos::addresses> (read_addresses_from_file (*df));
            }
        }

        return Addresses.get ();
    }

    pubkeys *Interface::get_pubkeys () {

        if (!bool (Pubkeys)) {
            auto pf = pubkeys_filepath ();
            if (bool (pf)) Pubkeys = std::make_shared<Cosmos::pubkeys> (read_pubkeys_from_file (*pf));
        }

        return Pubkeys.get ();
    }

    keychain *Interface::get_keys () {

        if (!bool (Keys)) {
            auto kf = keychain_filepath ();
            if (bool (kf)) Keys = std::make_shared<keychain> (read_keychain_from_file (*kf));
        }

        return Keys.get ();
    }

    price_data *Interface::get_price_data () {
        if (PriceData == nullptr) {
            if (LocalPriceData == nullptr) {
                auto pdf = price_data_filepath ();
                if (!bool (pdf)) return nullptr;
                LocalPriceData = std::static_pointer_cast<Cosmos::local_price_data>
                    (std::make_shared<Cosmos::JSON_price_data> (read_from_file (*pdf)));
            }

            auto n = net ();
            if (!bool (n)) return LocalPriceData.get ();

            PriceData = std::make_shared<Cosmos::cached_remote_price_data> (*n, *LocalPriceData);
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
            if (bool (hf)) Events = std::make_shared<Cosmos::history> (read_from_file (*hf), *get_txdb ());
        }

        return Events.get ();
    }

    payments *Interface::get_payments () {
        if (!bool (Payments)) {
            auto pf = payments_filepath ();
            if (bool (pf)) Payments = std::make_shared<Cosmos::payments> (read_from_file (*pf));
        }

        return Payments.get ();
    }

    crypto::random *Interface::secure_random () {

        if (!SecureRandom) {

            Entropy = std::make_shared<crypto::user_entropy> (
                "We need some entropy for this operation. Please type random characters.",
                "Thank you for your entropy so far. That was not enough. Please give us more random characters.",
                "Sufficient entropy provided.", std::cout, std::cin);

            SecureRandom = std::make_shared<crypto::NIST::DRBG> (crypto::NIST::DRBG::Hash, *Entropy, std::numeric_limits<uint32>::max ());
        }

        return SecureRandom.get ();

    }

    crypto::random *Interface::casual_random () {

        if (!CasualRandom) {
            if (!SecureRandom) secure_random ();

            uint64 seed;
            Satoshi::GetStrongRandBytes ((byte *) &seed, 8);

            CasualRandom = std::make_shared<crypto::linear_combination_random> (256,
                std::static_pointer_cast<crypto::random> (std::make_shared<crypto::std_random<std::default_random_engine>> (seed)),
                std::static_pointer_cast<crypto::random> (SecureRandom));
        }

        return CasualRandom.get ();

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
            write_to_file (JSON (dynamic_cast<JSON_local_TXDB &> (*LocalTXDB)), *tf);

        if (bool (af) && bool (Account)) write_to_file (JSON (*Account), *af);

        if (bool (df) && bool (Addresses)) write_to_file (JSON (*Addresses), *df);

        if (bool (hf) && bool (Events)) write_to_file (JSON (*Events), *hf);

        if (bool (pf) && bool (Pubkeys)) write_to_file (JSON (*Pubkeys), *pf);

        if (bool (yf) && bool (Payments)) write_to_file (JSON (*Payments), *yf);

        if (bool (kf) && bool (Keys)) write_to_file (JSON (*Keys), *kf);

        if (bool (pdf) && bool (LocalPriceData))
            write_to_file (JSON (dynamic_cast<JSON_price_data &> (*LocalPriceData)), *pdf);

    }

}
