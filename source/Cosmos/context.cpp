
#include <data/io/exception.hpp>
#include <data/crypto/NIST_DRBG.hpp>
#include <gigamonkey/schema/bip_44.hpp>
#include <gigamonkey/script/pattern/pay_to_address.hpp>
#include <Cosmos/network.hpp>
#include <Cosmos/context.hpp>
#include <Cosmos/wallet/split.hpp>

namespace Cosmos {

    void generate_wallet (const string &private_filename, const string &public_filename, uint32 account) {
        std::cout << "Type random characters and we will generate a wallet for you. Press enter when you think you have enough." << std::endl;

        std::string user_input {};

        while (true) {
            char x = std::cin.get ();
            if (x == '\n') break;
            user_input.push_back (x);
        }

        digest512 bits = crypto::SHA2_512 (user_input);

        secp256k1::secret x;

        HD::chain_code chain_code (32);

        std::copy (bits.begin (), bits.begin () + 32, x.Value.begin ());
        std::copy (bits.begin () + 32, bits.end (), chain_code.begin ());

        list<uint32> path {
            HD::BIP_44::purpose,
            HD::BIP_44::coin_type_Bitcoin,
            HD::BIP_32::harden (account)};

        HD::BIP_32::secret master {x, chain_code, HD::BIP_32::main};
        HD::BIP_32::pubkey master_pubkey = master.to_public ();

        keychain key {"master", secret {master}};
        pubkeychain pub {"receive", pubkey {master_pubkey, derivation {"master", path << HD::BIP_44::receive_index}}};
        pubkey change {master_pubkey, derivation {"master", path << HD::BIP_44::change_index}};
        pub = pub.insert ("change", change);

        write_to_file (JSON (key), private_filename);
        write_to_file (JSON (pub), public_filename);
    }

    void send_to (network &n, wallet &w, crypto::random &rand, list<Bitcoin::output> o) {
        spent z = spend (w,
            select_output_parameters {4, 5000, .23},
            make_change_parameters {10, 100, 1000000, .4},
            Gigamonkey::redeem_p2pkh_and_p2pk, rand, o);

        std::cout << "broadcasting tx " << Bitcoin::transaction::id (z.Transaction) << std::endl;

        auto err = n.broadcast (z.Transaction);

        if (bool (err)) throw exception {3} << "tx broadcast failed;\n\ttx: " << encoding::hex::write (z.Transaction) << "\n\terror: " << err;

        w = z.Wallet;
    }

    void read_both_chains_options (context &e, const arg_parser &p) {
        auto &name = e.wallet_name ();
        p.get (2, "name", name);
        if (bool (name)) return;

        auto &keychain_filename = e.keychain_filename ();
        auto &pubkeychain_filename = e.pubkeychain_filename ();
        p.get ("keychain_filename", keychain_filename);
        p.get ("pubkeychain_filename", pubkeychain_filename);
        if (!bool (keychain_filename)) throw data::exception {1} << "could not read filename of keychain";
        if (!bool (pubkeychain_filename)) throw data::exception {1} << "could not read filename of pubkeychain";
    }

    void read_pubkeychain_options (context &e, const arg_parser &p) {
        auto &name = e.wallet_name ();
        p.get (2, "name", name);
        if (bool (name)) return;

        auto &filename = e.pubkeychain_filename ();
        p.get ("filename", filename);
        if (bool (filename)) return;
        p.get ("pubkeychain_filename", filename);
        if (bool (filename)) return;

        throw data::exception {1} << "could not read filename of pubkeychain";
    }

    void read_account_and_txdb_options (context &e, const arg_parser &p) {
        auto &name = e.wallet_name ();
        p.get (2, "name", name);
        if (bool (name)) return;

        auto &txdb_filename = e.txdb_filename ();
        auto &account_filename = e.account_filename ();
        p.get ("txdb_filename", txdb_filename);
        p.get ("account_filename", account_filename);
        if (!bool (txdb_filename)) std::cout << "local tx database not provided; using remote only";
        if (!bool (account_filename)) throw data::exception {1} << "could not read filename of account";
    }

    void read_random_options (context &e, const arg_parser &p) {
        // no options set up for random yet.
    }

    maybe<std::string> &context::keychain_filename () {
        if (!bool (KeychainFilename) && bool (Name)) {
            std::stringstream ss;
            ss << *Name << ".keychain";
            KeychainFilename = ss.str ();
        }

        return KeychainFilename;
    }

    maybe<std::string> &context::pubkeychain_filename () {
        if (!bool (PubkeychainFilename) && bool (Name)) {
            std::stringstream ss;
            ss << *Name << ".pubkeychain";
            PubkeychainFilename = ss.str ();
        }

        return PubkeychainFilename;
    }

    maybe<std::string> &context::txdb_filename () {
        if (!bool (TXDBFilename) && bool (Name)) {
            std::stringstream ss;
            ss << *Name << ".txdb";
            TXDBFilename = ss.str ();
        }

        return TXDBFilename;
    }

    maybe<std::string> &context::account_filename () {
        if (!bool (AccountFilename) && bool (Name)) {
            std::stringstream ss;
            ss << *Name << ".account";
            AccountFilename = ss.str ();
        }

        return AccountFilename;
    }

    maybe<std::string> &context::price_data_filename () {
        if (!bool (PriceDataFilename) && bool (Name)) {
            std::stringstream ss;
            ss << *Name << ".prices";
            PriceDataFilename = ss.str ();
        }

        return PriceDataFilename;
    }

    network *context::net () {
        if (!bool (Net)) Net = new network ();

        return Net;
    }

    txdb *context::txdb () {
        if (bool (RemoteTXDB)) return RemoteTXDB;

        if (!bool (LocalTXDB)) {
            auto txf = txdb_filename ();
            if (bool (txf)) LocalTXDB = new local_txdb {read_local_txdb_from_file (*txf)};
            else return nullptr;
        }

        auto n = net ();
        if (bool (n)) {

            RemoteTXDB = new cached_remote_txdb {*n, *LocalTXDB};
            LocalTXDB = &RemoteTXDB->Local;
            return RemoteTXDB;

        } else return LocalTXDB;
    }

    account *context::account () {
        if (!bool (Account)) {
            auto af = account_filename ();
            if (bool (af)) {
                Account = new Cosmos::account {read_account_from_file (*af)};
            }
        }

        return Account;
    }

    pubkeychain *context::pubkeys () {
        if (!bool (Pubkeys)) {
            auto pf = pubkeychain_filename ();
            if (bool (pf)) Pubkeys = new pubkeychain {read_pubkeychain_from_file (*pf)};
        }

        return Pubkeys;
    }

    keychain *context::keys () {
        if (!bool (Keys)) {
            auto kf = keychain_filename ();
            if (bool (kf)) Keys = new keychain {read_keychain_from_file (*kf)};
        }

        return Keys;
    }

    price_data *context::price_data () {
        if (PriceData == nullptr) {
            auto pdf = price_data_filename ();
            auto n = net ();
            if (!bool (pdf) || !bool (n)) return nullptr;
            PriceData = new Cosmos::price_data {*net (), read_from_file (*pdf)};
        }

        return PriceData;
    }

    watch_wallet *context::watch_wallet () {
        if (bool (WatchWallet)) return WatchWallet;
        auto txs = txdb ();
        auto acc = account ();
        auto pkc = pubkeys ();

        if (!bool (txs) || !bool (acc) || !bool (pkc)) return nullptr;

        WatchWallet = new Cosmos::watch_wallet {*acc, *pkc};
        Account = &WatchWallet->Account;
        Pubkeys = &WatchWallet->Pubkeys;

        return WatchWallet;
    }

    wallet *context::wallet () {
        if (!bool (Wallet)) return Wallet;

        auto watch = watch_wallet ();
        auto k = keys ();
        if (!bool (watch) || !bool (k)) return nullptr;

        Wallet = new Cosmos::wallet {watch->Account, watch->Pubkeys, *k};
        Keys = &Wallet->Keys;
        WatchWallet = Wallet;

        return Wallet;
    }

    crypto::random *context::random () {

        if (!Random) {

            Entropy = std::make_shared<crypto::user_entropy> (
                "We need some entropy for this operation. Please type random characters.",
                "Sufficient entropy provided.", std::cout, std::cin);

            Random = new crypto::NIST::DRBG {crypto::NIST::DRBG::Hash, *Entropy};
        }

        return Random;

    }

    context::~context () {

        auto tf = txdb_filename ();
        auto af = account_filename ();
        auto pf = pubkeychain_filename ();
        auto kf = keychain_filename ();
        auto pdf = price_data_filename ();

        if (bool (tf) && bool (LocalTXDB)) write_to_file (JSON (*LocalTXDB), *tf);
        if (bool (af) && bool (Account)) write_to_file (JSON (*Account), *af);
        if (bool (pf) && bool (Pubkeys)) write_to_file (JSON (*Pubkeys), *kf);
        if (bool (kf) && bool (Keys)) write_to_file (JSON (*Keys), *kf);

        if (bool (pdf) && bool (PriceData)) {
            write_to_file (JSON (*PriceData), *pdf);
            delete PriceData;
        }

        if (Wallet != nullptr) delete Wallet;
        else {
            delete Keys;
            if (WatchWallet != nullptr) delete WatchWallet;
            else {
                delete Account;
                delete Pubkeys;
            }
        }

        if (RemoteTXDB != nullptr) delete RemoteTXDB;
        else delete LocalTXDB;

        delete Net;

        delete Random;
    }

}
