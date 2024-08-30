#ifndef COSMOS_CONTEXT
#define COSMOS_CONTEXT

#include <data/crypto/random.hpp>
#include <data/io/arg_parser.hpp>

#include <Cosmos/wallet/wallet.hpp>

using arg_parser = data::io::arg_parser;

namespace Cosmos {

    struct context {

        maybe<std::string> &wallet_name ();
        maybe<std::string> &pubkeychain_filename ();
        maybe<std::string> &keychain_filename ();
        maybe<std::string> &txdb_filename ();
        maybe<std::string> &account_filename ();
        maybe<std::string> &price_data_filename ();

        network *net ();
        crypto::random *random ();

        const Cosmos::keychain *keys () const;
        const Cosmos::pubkeychain *pubkeys () const;
        const Cosmos::txdb *txdb () const;
        const SPV::database *spvdb () const;
        const Cosmos::account *account () const;
        const Cosmos::price_data *price_data () const;

        const Cosmos::watch_wallet *watch_wallet () const;
        const Cosmos::wallet *wallet () const;

        struct writable_database {

            Cosmos::keychain *keys ();
            Cosmos::pubkeychain *pubkeys ();
            Cosmos::txdb *txdb ();
            SPV::database *spvdb ();
            Cosmos::account *account ();
            Cosmos::price_data *price_data ();

            Cosmos::watch_wallet *watch_wallet ();
            Cosmos::wallet *wallet ();

            context &Context;
        };

        writable_database update () {
            Written = true;
            return writable_database {*this};
        }

        ~context ();

    private:

        maybe<std::string> Name {};
        maybe<std::string> KeychainFilename {};
        maybe<std::string> PubkeychainFilename {};
        maybe<std::string> TXDBFilename {};
        maybe<std::string> AccountFilename {};
        maybe<std::string> PriceDataFilename {};

        network *Net {nullptr};

        Cosmos::keychain *Keys {nullptr};
        Cosmos::pubkeychain *Pubkeys {nullptr};
        Cosmos::local_txdb *LocalTXDB {nullptr};
        Cosmos::cached_remote_txdb *RemoteTXDB {nullptr};
        Cosmos::price_data *PriceData {nullptr};
        Cosmos::account *Account {nullptr};
        Cosmos::watch_wallet *WatchWallet {nullptr};
        Cosmos::wallet *Wallet {nullptr};

        ptr<crypto::user_entropy> Entropy;
        crypto::random *Random {nullptr};

        bool Written {false};

        Cosmos::keychain *keys ();
        Cosmos::pubkeychain *pubkeys ();
        Cosmos::txdb *txdb ();
        SPV::database *spvdb ();
        Cosmos::account *account ();
        Cosmos::price_data *price_data ();

        Cosmos::watch_wallet *watch_wallet ();
        Cosmos::wallet *wallet ();

        friend struct writable_database;
    };

    void generate_wallet (const string &private_filename, const string &public_filename, uint32 account = 0);
    void display_value (watch_wallet &w);
    void generate_new_address (pubkeychain &p);
    void generate_new_address (pubkeychain &p);
    void send_to (network &n, wallet &w, crypto::random &rand, list<Bitcoin::output> o);
    void restore_wallet (context &e);

    void read_both_chains_options (context &, const arg_parser &p);
    void read_pubkeychain_options (context &, const arg_parser &p);
    void read_account_and_txdb_options (context &, const arg_parser &p);
    void read_random_options (context &, const arg_parser &p);

    void read_wallet_options (context &e, const arg_parser &p);
    void read_watch_wallet_options (context &e, const arg_parser &p);

    struct error {
        int Code;
        maybe<std::string> Message;
        error () : Code {0}, Message {} {}
        error (int code) : Code {code}, Message {} {}
        error (int code, const string &err): Code {code}, Message {err} {}
        error (const string &err): Code {1}, Message {err} {}
    };

    maybe<std::string> inline &context::wallet_name () {
        return Name;
    }

    void inline display_value (watch_wallet &w) {
        std::cout << w.value () << std::endl;
    }

    void generate_new_xpub (pubkeychain &p);

    void inline read_wallet_options (context &e, const arg_parser &p) {
        read_both_chains_options (e, p);
        read_account_and_txdb_options (e, p);
    }

    void inline read_watch_wallet_options (context &e, const arg_parser &p) {
        read_pubkeychain_options (e, p);
        read_account_and_txdb_options (e, p);
    }

    const Cosmos::keychain inline *context::keys () const {
        return const_cast<context *> (this)->keys ();
    }

    const Cosmos::pubkeychain inline *context::pubkeys () const {
        return const_cast<context *> (this)->pubkeys ();
    }

    const Cosmos::txdb inline *context::txdb () const {
        return const_cast<context *> (this)->txdb ();
    }

    const SPV::database inline *context::spvdb () const {
        return const_cast<context *> (this)->spvdb ();
    }

    const Cosmos::account inline *context::account () const {
        return const_cast<context *> (this)->account ();
    }

    const Cosmos::price_data inline *context::price_data () const {
        return const_cast<context *> (this)->price_data ();
    }

    const Cosmos::watch_wallet inline *context::watch_wallet () const {
        return const_cast<context *> (this)->watch_wallet ();
    }

    const Cosmos::wallet inline *context::wallet () const {
        return const_cast<context *> (this)->wallet ();
    }

    Cosmos::keychain inline *context::writable_database::keys () {
        return Context.keys ();
    }

    Cosmos::pubkeychain inline *context::writable_database::pubkeys () {
        return Context.pubkeys ();
    }

    Cosmos::txdb inline *context::writable_database::txdb () {
        return Context.txdb ();
    }

    SPV::database inline *context::writable_database::spvdb () {
        return Context.spvdb ();
    }

    Cosmos::account inline *context::writable_database::account () {
        return Context.account ();
    }

    Cosmos::price_data inline *context::writable_database::price_data () {
        return Context.price_data ();
    }

    Cosmos::watch_wallet inline *context::writable_database::watch_wallet () {
        return Context.watch_wallet ();
    }

    Cosmos::wallet inline *context::writable_database::wallet () {
        return Context.wallet ();
    }

}

#endif
