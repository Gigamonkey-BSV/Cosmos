
#include <data/io/exception.hpp>
#include <data/crypto/NIST_DRBG.hpp>
#include <gigamonkey/schema/bip_44.hpp>
#include <gigamonkey/schema/bip_39.hpp>
#include <Cosmos/network.hpp>
#include <Cosmos/interface.hpp>
#include <Cosmos/wallet/split.hpp>

namespace Cosmos {

    void update_pending_transactions (Interface::writable u) {
        auto txdb = u.txdb ();
        if (!bool (txdb)) throw exception {"could not connect to network and database"};
        set<Bitcoin::TXID> pending = txdb->Local.pending ();
        for (const Bitcoin::TXID &txid : pending) txdb->import_transaction (txid);
    }
/*
    pubkeys generate_new_xpub (const pubkeys &p) {
        const auto *v = p.Sequences.contains (p.Receive);
        if (v == nullptr) throw exception {} << "pubkey map has no receive sequence";
        address_sequence receive_sequence = p.Sequences[p.Receive];
        HD::BIP_32::pubkey next_pubkey = receive_sequence.Key.derive (receive_sequence.Path << receive_sequence.Last);
        Bitcoin::address::decoded next_address = next_pubkey.address ();
        std::cout << "next xpub is " << next_pubkey << " and its address is " << next_address;
        return p.next (p.Receive);
    }*/

    void generate_wallet::operator () (Interface::writable u) {
        const auto w = u.get ().wallet ();
        if (!bool (w)) throw exception {} << "could not read wallet.";

        if (UseBIP_39) std::cout << "We will generate a new BIP 39 wallet for you." << std::endl;
        else std::cout << "We will generate a new BIP 44 wallet for you." << std::endl;
        std::cout << "We will pre-generate " << Accounts << " accounts in this wallet." << std::endl;
        std::cout << "Coin type is " << CoinType << std::endl;
        std::cout << "Type random characters to use as entropy for generating this wallet. "
            "Press enter when you think you have enough."
            "Around 200 characters ought to be enough as long as they are random enough." << std::endl;

        std::string user_input {};

        while (true) {
            char x = std::cin.get ();
            if (x == '\n') break;
            user_input.push_back (x);
        }

        HD::BIP_32::secret master {};

        if (UseBIP_39) {
            digest256 bits = crypto::SHA2_256 (user_input);

            std::string words = HD::BIP_39::generate (bits);

            std::cout << "your words are\n\n\t" << words << "\n\nRemember, these words can be used to generate "
                "all your keys, but at scale that is not enough to restore your funds. You need to keep the transactions"
                " into your wallet along with Merkle proofs as well." << std::endl;

            // TODO use passphrase option.
            master = HD::BIP_32::secret::from_seed (HD::BIP_39::read (words));
        } else {
            digest512 bits = crypto::SHA2_512 (user_input);
            master.ChainCode.resize (32);
            std::copy (bits.begin (), bits.begin () + 32, master.Secret.Value.begin ());
            std::copy (bits.begin () + 32, bits.end (), master.ChainCode.begin ());
        }

        HD::BIP_32::pubkey master_pubkey = master.to_public ();
        keychain keys = keychain {}.insert (master_pubkey, master);

        pubkeys pub {{}, {}, "receive_0", "change_0"};

        for (int account = 0; account < Accounts; account++) {
            list<uint32> path {
                HD::BIP_44::purpose,
                HD::BIP_32::harden (CoinType),
                HD::BIP_32::harden (account)};

            HD::BIP_32::pubkey account_master_pubkey = master.derive (path).to_public ();

            std::cout << "\tmaster pubkey for account " << account << " is " << account_master_pubkey << std::endl;

            pub.Derivations = pub.Derivations.insert (account_master_pubkey, derivation {master_pubkey, path});

            std::string receive_name = std::string {"receive_"} + std::to_string (account);
            std::string change_name = std::string {"change_"} + std::to_string (account);

            pub.Sequences = pub.Sequences.insert (receive_name, address_sequence {account_master_pubkey, {HD::BIP_44::receive_index}});
            pub.Sequences = pub.Sequences.insert (change_name, address_sequence {account_master_pubkey, {HD::BIP_44::change_index}});
        }

        u.set_keys (keys);
        u.set_pubkeys (pub);
    }

    broadcast_error Interface::writable::broadcast (const spend::spent &x) {

        auto ww = I.watch_wallet ();
        if (!bool (ww)) throw exception {1} << "could not load wallet";
        Cosmos::watch_wallet next_wallet = *ww;
        // if a broadcast fails, the keys will still be updated. Hopefully not more than max_look_ahead!
        next_wallet.Pubkeys = x.Pubkeys;

        broadcast_error err;
        for (const auto &[extx, diff] : x.Transactions) {
            std::cout << "broadcasting tx " << diff.TXID;
            wait_for_enter ();
            auto err = txdb ()->broadcast ({diff.TXID, extx});
            if (bool (err)) {
                std::cout << "tx broadcast failed;\n\ttx: " << extx << "\n\terror: " << err << std::endl;
                break;
            }

            next_wallet.Account <<= diff;
        }

        // save new wallet.
        set_watch_wallet (next_wallet);
        return err;

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

    maybe<std::string> &Interface::price_data_filepath () {
        if (!bool (PriceDataFilepath) && bool (Name)) {
            std::stringstream ss;
            ss << "price_data.json";
            PriceDataFilepath = ss.str ();
        }

        return PriceDataFilepath;
    }

    maybe<std::string> &Interface::history_filepath () {
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

    local_txdb *Interface::get_local_txdb () {

        if (!bool (LocalTXDB)) {
            auto txf = txdb_filepath ();
            if (bool (txf))
                LocalTXDB = std::static_pointer_cast<Cosmos::local_txdb>
                    (std::make_shared<JSON_local_txdb> (read_JSON_local_txdb_from_file (*txf)));
            else return nullptr;
        }

        return LocalTXDB.get ();
    }

    maybe<cached_remote_txdb> Interface::get_txdb () {

        auto n = net ();
        auto l = get_local_txdb ();
        if (!bool (n) || !bool (l)) return {};

        return {cached_remote_txdb {*n, *l}};
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

    ptr<price_data> Interface::get_price_data () {
        if (LocalPriceData == nullptr) {
            auto pdf = price_data_filepath ();
            if (!bool (pdf)) return nullptr;
            LocalPriceData = std::static_pointer_cast<Cosmos::local_price_data>
                (std::make_shared<Cosmos::JSON_price_data> (read_from_file (*pdf)));
        }

        auto n = net ();
        if (!bool (n)) return std::static_pointer_cast<Cosmos::price_data> (LocalPriceData);
        return std::static_pointer_cast<Cosmos::price_data> (std::make_shared<Cosmos::cached_remote_price_data> (*n, *LocalPriceData));
    }

    maybe<watch_wallet> Interface::get_watch_wallet () {

        auto acc = account ();
        auto pkc = pubkeys ();

        if (!bool (acc) || !bool (pkc)) return {};

        return {Cosmos::watch_wallet {*pkc, *acc}};
    }

    maybe<wallet> Interface::get_wallet () {

        auto watch = watch_wallet ();
        auto k = keys ();

        if (!bool (watch) || !bool (k)) return {};

        return {Cosmos::wallet {*k, watch->Pubkeys, watch->Account}};
    }

    events *Interface::get_history () {
        if (!bool (Events)) {
            auto hf = history_filepath ();
            if (bool (hf)) Events = std::make_shared<events> (read_from_file (*hf));
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

    crypto::random *Interface::random () {

        if (!Random) {

            Entropy = std::make_shared<crypto::user_entropy> (
                "We need some entropy for this operation. Please type random characters.",
                "Sufficient entropy provided.", std::cout, std::cin);

            Random = std::make_shared<crypto::NIST::DRBG> (crypto::NIST::DRBG::Hash, *Entropy);
        }

        return Random.get ();

    }

    Interface::~Interface () {
        if (!Written) return;

        auto tf = txdb_filepath ();
        auto af = account_filepath ();
        auto hf = history_filepath ();
        auto pf = pubkeys_filepath ();
        auto kf = keychain_filepath ();
        auto pdf = price_data_filepath ();
        auto yf = payments_filepath ();

        if (bool (tf) && bool (LocalTXDB))
            write_to_file (JSON (dynamic_cast<JSON_local_txdb &> (*LocalTXDB)), *tf);

        if (bool (af) && bool (Account))
            write_to_file (JSON (*Account), *af);

        if (bool (hf) && bool (Events))
            write_to_file (JSON (*Events), *hf);

        if (bool (pf) && bool (Pubkeys))
            write_to_file (JSON (*Pubkeys), *pf);

        if (bool (yf) && bool (Payments))
            write_to_file (JSON (*Payments), *yf);

        if (bool (kf) && bool (Keys))
            write_to_file (JSON (*Keys), *kf);

        if (bool (pdf) && bool (LocalPriceData))
            write_to_file (JSON (dynamic_cast<JSON_price_data &> (*LocalPriceData)), *pdf);

    }

}
