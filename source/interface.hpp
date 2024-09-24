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
        maybe<std::string> &events_filepath ();
        maybe<std::string> &payments_filepath ();

        network *net ();
        crypto::random *random ();// depricated
        crypto::random *secure_random ();
        crypto::random *casual_random ();

        const Cosmos::local_TXDB *local_txdb () const;
        const Cosmos::cached_remote_TXDB *txdb () const;
        const SPV::database *spvdb () const;
        const Cosmos::price_data *price_data () const;

        const Cosmos::history *history () const;
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

            Cosmos::local_TXDB *local_txdb ();
            Cosmos::cached_remote_TXDB *txdb ();
            SPV::database *spvdb ();
            Cosmos::price_data *price_data ();

            Cosmos::history *history ();
            crypto::random *random ();

            void set_keys (const Cosmos::keychain &);
            void set_pubkeys (const Cosmos::pubkeys &);
            void set_account (const Cosmos::account &);
            void set_addresses (const Cosmos::addresses &);

            void set_wallet (const Cosmos::wallet &);
            void set_payments (const Cosmos::payments &);

            // broadcast a series of transactions with account diffs
            // to update in the wallet. Optionally, an SPV::proof::map
            // may be provided. In this case, all antedecent transactions
            // will be entered into the database and broadcast if appropriate.
            broadcast_tree_result broadcast (list<std::pair<Bitcoin::transaction, account_diff>>);

            // make a transaction with a bunch of default options already set
            spend::spent make_tx (list<Bitcoin::output> o);

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
        ptr<Cosmos::local_TXDB> LocalTXDB {nullptr};
        ptr<Cosmos::cached_remote_TXDB> TXDB {nullptr};
        ptr<Cosmos::local_price_data> LocalPriceData {nullptr};
        ptr<Cosmos::cached_remote_price_data> PriceData {nullptr};
        ptr<Cosmos::history> Events {nullptr};
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
        Cosmos::cached_remote_TXDB *get_txdb ();
        Cosmos::local_TXDB *get_local_txdb ();
        Cosmos::account *get_account ();
        Cosmos::price_data *get_price_data ();
        Cosmos::history *get_history ();
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

    cached_remote_TXDB inline *Interface::writable::txdb () {
        return I.get_txdb ();
    }

    local_TXDB inline *Interface::writable::local_txdb () {
        return I.get_local_txdb ();
    }

    history inline *Interface::writable::history () {
        return I.get_history ();
    }

    crypto::random inline *Interface::writable::random () {
        return I.random ();
    }

    price_data inline *Interface::writable::price_data () {
        return I.get_price_data ();
    }

    const local_TXDB inline *Interface::local_txdb () const {
        return const_cast<Interface *> (this)->get_local_txdb ();
    }

    const keychain inline *Interface::keys () const {
        return const_cast<Interface *> (this)->get_keys ();
    }

    const pubkeys inline *Interface::pubkeys () const {
        return const_cast<Interface *> (this)->get_pubkeys ();
    }

    const cached_remote_TXDB inline *Interface::txdb () const {
        return const_cast<Interface *> (this)->get_txdb ();
    }

    const account inline *Interface::account () const {
        return const_cast<Interface *> (this)->get_account ();
    }

    const addresses inline *Interface::addresses () const {
        return const_cast<Interface *> (this)->get_addresses ();
    }

    const price_data inline *Interface::price_data () const {
        return const_cast<Interface *> (this)->get_price_data ();
    }

    const maybe<wallet> inline Interface::wallet () const {
        return const_cast<Interface *> (this)->get_wallet ();
    }

    const history inline *Interface::history () const {
        return const_cast<Interface *> (this)->get_history ();
    }

    const payments inline *Interface::payments () const {
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
