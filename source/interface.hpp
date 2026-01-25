#ifndef INTERFACE
#define INTERFACE

#include <data/tools/store.hpp>
#include <data/crypto/random.hpp>
#include <data/io/arg_parser.hpp>

#include <Cosmos/wallet/split.hpp>
#include <Cosmos/database/json/price_data.hpp>
#include <Cosmos/database/json/txdb.hpp>
#include <Cosmos/history.hpp>
#include <Cosmos/random.hpp>
#include <Cosmos/files.hpp>

using arg_parser = data::io::arg_parser;

template <typename X> using function = std::function<X>;

namespace Cosmos {
    using filepath = std::filesystem::path;

    // kind of a dumb name but I don't know what else to call it right now.
    // Interface is the connection to both the database and the outside world.
    struct Interface {

        maybe<std::string> &wallet_name ();

        maybe<filepath> &txdb_filepath ();
        maybe<filepath> &price_data_filepath ();
        maybe<filepath> &keychain_filepath ();
        maybe<filepath> &pubkeys_filepath ();
        maybe<filepath> &addresses_filepath ();
        maybe<filepath> &account_filepath ();
        maybe<filepath> &events_filepath ();
        maybe<filepath> &payments_filepath ();

        network *net ();

        const Cosmos::local_TXDB *local_txdb () const;
        const Cosmos::cached_remote_TXDB *txdb () const;
        const SPV::database *spvdb () const;
        const Cosmos::price_data *price_data () const;

        const Cosmos::history *history () const;
        const Cosmos::payments *payments () const;

        const Cosmos::account *account () const;

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
        struct writable : database::writable {

            Cosmos::local_TXDB *local_txdb ();
            Cosmos::cached_remote_TXDB *txdb ();
            SPV::database *spvdb ();
            Cosmos::price_data *price_data ();

            void set_key (const std::string &key_name, const key_expression &k) final override;
            void to_private (const std::string &key_name, const key_expression &k) final override;

            Cosmos::history *history () final override;

            void set_account (const Cosmos::account &) final override;

            void set_payments (const Cosmos::payments &) final override;

            // broadcast a series of transactions with account diffs
            // to update in the wallet. Optionally, an SPV::proof::map
            // may be provided. In this case, all antedecent transactions
            // will be entered into the database and broadcast if appropriate.
            broadcast_tree_result broadcast (list<std::pair<Bitcoin::transaction, account_diff>>);

            // make a transaction with a bunch of default options already set
            spend::spent make_tx (list<Bitcoin::output> send_to, const spend_options & = {});

            const Cosmos::Interface &get () {
                return I;
            }

        private:
            Cosmos::Interface &I;

            writable (Cosmos::Interface &i) : I {i} {}
            friend struct Cosmos::Interface;
        };

        ~Interface ();

        files Files;

    private:

        maybe<std::string> Name {};
        maybe<filepath> KeychainFilepath {};
        maybe<filepath> PubkeychainFilepath {};
        maybe<filepath> TXDBFilepath {};
        maybe<filepath> AccountFilepath {};
        maybe<filepath> AddressesFilepath {};
        maybe<filepath> PriceDataFilepath {};
        maybe<filepath> HistoryFilepath {};
        maybe<filepath> PaymentsFilepath {};


        ptr<network> Net {nullptr};
        ptr<Cosmos::local_TXDB> LocalTXDB {nullptr};
        ptr<Cosmos::cached_remote_TXDB> TXDB {nullptr};
        ptr<Cosmos::local_price_data> LocalPriceData {nullptr};
        ptr<Cosmos::cached_remote_price_data> PriceData {nullptr};
        ptr<Cosmos::history> Events {nullptr};
        ptr<Cosmos::account> Account {nullptr};
        ptr<Cosmos::payments> Payments {nullptr};

        // if this is set to true, then everything will be
        // saved to disk on destruction of the Interface.
        bool Written {false};

        Cosmos::cached_remote_TXDB *get_txdb ();
        Cosmos::local_TXDB *get_local_txdb ();
        Cosmos::account *get_account ();
        Cosmos::price_data *get_price_data ();
        Cosmos::history *get_history ();
        Cosmos::payments *get_payments ();

        friend struct writable;
    };

    void display_value (const account &w);

    void update_pending_transactions (Interface::writable);

    void restore_wallet (Interface &e);

    void read_both_chains_options (Interface &, const arg_parser &p);
    void read_pubkeys_options (Interface &, const arg_parser &p);
    void read_account_and_txdb_options (Interface &, const arg_parser &p);
    void read_random_options (const arg_parser &p);

    spend_options read_tx_options (network &, const arg_parser &p, bool online = true);

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

    void inline display_value (const account &w) {
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

    price_data inline *Interface::writable::price_data () {
        return I.get_price_data ();
    }

    const local_TXDB inline *Interface::local_txdb () const {
        return const_cast<Interface *> (this)->get_local_txdb ();
    }

    const cached_remote_TXDB inline *Interface::txdb () const {
        return const_cast<Interface *> (this)->get_txdb ();
    }

    const account inline *Interface::account () const {
        return const_cast<Interface *> (this)->get_account ();
    }

    const price_data inline *Interface::price_data () const {
        return const_cast<Interface *> (this)->get_price_data ();
    }

    const history inline *Interface::history () const {
        return const_cast<Interface *> (this)->get_history ();
    }

    const payments inline *Interface::payments () const {
        return const_cast<Interface *> (this)->get_payments ();
    }

    void inline Interface::writable::set_account (const Cosmos::account &a) {
        if (I.Account) *I.Account = a;
        else I.Account = std::make_shared<Cosmos::account> (a);
    }

    void inline Interface::writable::set_payments (const Cosmos::payments &pk) {
        if (I.Payments) *I.Payments = pk;
        else I.Payments = std::make_shared<Cosmos::payments> (pk);
    }

}

#endif
