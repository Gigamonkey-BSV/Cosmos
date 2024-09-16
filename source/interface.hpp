#ifndef INTERFACE
#define INTERFACE

#include <data/tools/store.hpp>
#include <data/crypto/random.hpp>
#include <data/io/arg_parser.hpp>

#include <Cosmos/wallet/wallet.hpp>
#include <Cosmos/wallet/split.hpp>
#include <Cosmos/database/json/price_data.hpp>
#include <Cosmos/database/json/txdb.hpp>
#include <Cosmos/history.hpp>
#include <Cosmos/boost/random.hpp>

using arg_parser = data::io::arg_parser;

namespace Cosmos {

    // kind of a dumb name but I don't know what else to call it right now.
    // Interface is the connection to both the database and the outside world.
    struct Interface {

        maybe<std::string> &wallet_name ();

        maybe<std::string> &txdb_filepath ();
        maybe<std::string> &price_data_filepath ();
        maybe<std::string> &keychain_filepath ();
        maybe<std::string> &pubkeys_filepath ();
        maybe<std::string> &addresses_filepath ();
        maybe<std::string> &account_filepath ();
        maybe<std::string> &history_filepath ();
        maybe<std::string> &payments_filepath ();

        network *net ();
        crypto::random *random ();// depricated
        crypto::random *secure_random ();
        crypto::random *casual_random ();

        const Cosmos::local_txdb *local_txdb () const;
        const maybe<Cosmos::cached_remote_txdb> txdb () const;
        const SPV::database *spvdb () const;
        const ptr<Cosmos::price_data> price_data () const;

        const Cosmos::events *history () const;
        const Cosmos::addresses *addresses () const;
        const Cosmos::payments *payments () const;

        const Cosmos::keychain *keys () const;
        const Cosmos::pubkeys *pubkeys () const;
        const Cosmos::account *account () const;

        const maybe<Cosmos::wallet> wallet () const;

        Interface () {}

        // We use this to change the database.
        struct writable;

        // if this function returns normally, any changes
        // to the database will be saved. If an exception
        // is thrown, the database will remain unchanged
        // when the program is closed (although the in-memory
        // database will be changed).
        template <typename X> X update (function<X (writable w)> f);

        // We use this to change the database.
        struct writable {

            Cosmos::local_txdb *local_txdb ();
            maybe<Cosmos::cached_remote_txdb> txdb ();
            SPV::database *spvdb ();
            ptr<Cosmos::price_data> price_data ();

            Cosmos::events *history ();
            crypto::random *random ();

            void set_keys (const Cosmos::keychain &);
            void set_pubkeys (const Cosmos::pubkeys &);
            void set_account (const Cosmos::account &);
            void set_addresses (const Cosmos::addresses &);

            void set_wallet (const Cosmos::wallet &);
            void set_payments (const Cosmos::payments &);

            broadcast_error broadcast (const spend::spent &);

            // make a transaction with a bunch of default options already set
            spend::spent make_tx (list<Bitcoin::output> o) {
                return spend {
                    select_output_parameters {4, 5000, .23},
                    // TODO replace this with split parameters.
                    split_change_parameters {}, *I.random ()}
                    (Gigamonkey::redeem_p2pkh_and_p2pk, *I.keys (), *I.wallet (), o);
            }

            const Cosmos::Interface &get () {
                return I;
            }

        private:
            Cosmos::Interface &I;

            writable (Cosmos::Interface &i) : I {i} {}
            friend struct Cosmos::Interface;
        };

        ~Interface ();

    private:

        maybe<std::string> Name {};
        maybe<std::string> KeychainFilepath {};
        maybe<std::string> PubkeychainFilepath {};
        maybe<std::string> TXDBFilepath {};
        maybe<std::string> AccountFilepath {};
        maybe<std::string> AddressesFilepath {};
        maybe<std::string> PriceDataFilepath {};
        maybe<std::string> HistoryFilepath {};
        maybe<std::string> PaymentsFilepath {};

        ptr<network> Net {nullptr};
        ptr<Cosmos::keychain> Keys {nullptr};
        ptr<Cosmos::pubkeys> Pubkeys {nullptr};
        ptr<Cosmos::local_txdb> LocalTXDB {nullptr};
        ptr<Cosmos::local_price_data> LocalPriceData {nullptr};
        ptr<Cosmos::events> Events {nullptr};
        ptr<Cosmos::account> Account {nullptr};
        ptr<Cosmos::addresses> Addresses {nullptr};
        ptr<Cosmos::payments> Payments {nullptr};

        ptr<crypto::user_entropy> Entropy;
        ptr<crypto::NIST::DRBG> SecureRandom {nullptr};
        ptr<crypto::linear_combination_random> CasualRandom {nullptr};

        // if this is set to true, then everything will be
        // saved to disk on destruction of the Interface.
        bool Written {false};

        Cosmos::keychain *get_keys ();
        Cosmos::pubkeys *get_pubkeys ();
        maybe<Cosmos::cached_remote_txdb> get_txdb ();
        Cosmos::local_txdb *get_local_txdb ();
        Cosmos::account *get_account ();
        ptr<Cosmos::price_data> get_price_data ();
        events *get_history ();
        Cosmos::addresses *get_addresses ();
        Cosmos::payments *get_payments ();

        maybe<Cosmos::wallet> get_wallet ();

        friend struct writable;
    };

    void display_value (const wallet &w);

    pubkeys generate_new_xpub (const pubkeys &p);

    void update_pending_transactions (Interface::writable);

    void restore_wallet (Interface &e);

    void read_both_chains_options (Interface &, const arg_parser &p);
    void read_pubkeys_options (Interface &, const arg_parser &p);
    void read_account_and_txdb_options (Interface &, const arg_parser &p);
    void read_random_options (Interface &, const arg_parser &p);

    void read_wallet_options (Interface &e, const arg_parser &p);
    void read_watch_wallet_options (Interface &e, const arg_parser &p);

    template <typename X> X Interface::update (function<X (writable w)> f) {
        store<X> x {f, writable {*this}};
        Written = true;
        return x.retrieve ();
    }

    maybe<std::string> inline &Interface::wallet_name () {
        return Name;
    }

    void inline display_value (const wallet &w) {
        std::cout << w.value () << std::endl;
    }

    void inline read_wallet_options (Interface &e, const arg_parser &p) {
        read_both_chains_options (e, p);
        read_account_and_txdb_options (e, p);
    }

    void inline read_watch_wallet_options (Interface &e, const arg_parser &p) {
        read_pubkeys_options (e, p);
        read_account_and_txdb_options (e, p);
    }

    maybe<Cosmos::cached_remote_txdb> inline Interface::writable::txdb () {
        return I.get_txdb ();
    }

    local_txdb inline *Interface::writable::local_txdb () {
        return I.get_local_txdb ();
    }

    events inline *Interface::writable::history () {
        return I.get_history ();
    }

    crypto::random inline *Interface::writable::random () {
        return I.random ();
    }

    ptr<Cosmos::price_data> inline Interface::writable::price_data () {
        return I.get_price_data ();
    }

    const Cosmos::local_txdb inline *Interface::local_txdb () const {
        return const_cast<Interface *> (this)->get_local_txdb ();
    }

    const Cosmos::keychain inline *Interface::keys () const {
        return const_cast<Interface *> (this)->get_keys ();
    }

    const Cosmos::pubkeys inline *Interface::pubkeys () const {
        return const_cast<Interface *> (this)->get_pubkeys ();
    }

    const maybe<Cosmos::cached_remote_txdb> inline Interface::txdb () const {
        return const_cast<Interface *> (this)->get_txdb ();
    }

    const Cosmos::account inline *Interface::account () const {
        return const_cast<Interface *> (this)->get_account ();
    }

    const Cosmos::addresses inline *Interface::addresses () const {
        return const_cast<Interface *> (this)->get_addresses ();
    }

    const ptr<Cosmos::price_data> inline Interface::price_data () const {
        return const_cast<Interface *> (this)->get_price_data ();
    }

    const maybe<Cosmos::wallet> inline Interface::wallet () const {
        return const_cast<Interface *> (this)->get_wallet ();
    }

    const events inline *Interface::history () const {
        return const_cast<Interface *> (this)->get_history ();
    }

    const Cosmos::payments inline *Interface::payments () const {
        return const_cast<Interface *> (this)->get_payments ();
    }

    void inline Interface::writable::set_keys (const Cosmos::keychain &kk) {
        if (I.Keys) *I.Keys = kk;
        else I.Keys = std::make_shared<Cosmos::keychain> (kk);
    }

    void inline Interface::writable::set_pubkeys (const Cosmos::pubkeys &pk) {
        if (I.Pubkeys) *I.Pubkeys = pk;
        else I.Pubkeys = std::make_shared<Cosmos::pubkeys> (pk);
    }

    void inline Interface::writable::set_account (const Cosmos::account &a) {
        if (I.Account) *I.Account = a;
        else I.Account = std::make_shared<Cosmos::account> (a);
    }

    void inline Interface::writable::set_addresses (const Cosmos::addresses &a) {
        if (I.Addresses) *I.Addresses = a;
        else I.Addresses = std::make_shared<Cosmos::addresses> (a);
    }

    void inline Interface::writable::set_wallet (const Cosmos::wallet &ww) {
        set_account (ww.Account);
        set_pubkeys (ww.Pubkeys);
        set_addresses (ww.Addresses);
    }

    void inline Interface::writable::set_payments (const Cosmos::payments &pk) {
        if (I.Payments) *I.Payments = pk;
        else I.Payments = std::make_shared<Cosmos::payments> (pk);
    }

    crypto::random inline *Interface::random () {
        return secure_random ();
    }

}

#endif
