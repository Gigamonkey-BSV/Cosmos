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

        Cosmos::keychain *keys ();
        Cosmos::pubkeychain *pubkeys ();
        Cosmos::txdb *txdb ();
        SPV::database *spvdb ();
        Cosmos::account *account ();
        Cosmos::price_data *price_data ();

        Cosmos::watch_wallet *watch_wallet ();
        Cosmos::wallet *wallet ();

        crypto::random *random ();

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

    void inline generate_new_address (pubkeychain &p) {
        std::cout << "new address: " << Bitcoin::address {Bitcoin::address::main, p.last (p.Receive).Pubkey.address_hash ()} << std::endl;
        p = p.next (p.Receive);
    }

    void inline read_wallet_options (context &e, const arg_parser &p) {
        read_both_chains_options (e, p);
        read_account_and_txdb_options (e, p);
    }

    void inline read_watch_wallet_options (context &e, const arg_parser &p) {
        read_pubkeychain_options (e, p);
        read_account_and_txdb_options (e, p);
    }
}

#endif
