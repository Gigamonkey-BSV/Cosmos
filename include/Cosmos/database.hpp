#ifndef COSMOS_DATABASE
#define COSMOS_DATABASE

#include <Cosmos/database/txdb.hpp>
#include <Cosmos/database/price_data.hpp>
#include <Cosmos/history.hpp>
#include <Cosmos/options.hpp>
//#include <Cosmos/wallet/wallet.hpp>

namespace Cosmos {

    struct database : local_TXDB, local_price_data {

        // key can be any standard key format (in quotes) or a derivation from another key.
        virtual void set_key (const std::string &key_name, const key_expression &k) = 0;

        // set the private key for a given public key.
        virtual void to_private (const std::string &key_name, const key_expression &k) = 0;

        virtual bool make_wallet (const std::string &name) = 0;
        virtual data::list<std::string> get_wallet_names () = 0;

        void setup_BIP_44_wallet (const std::string &wallet_name, const HD::BIP_32::secret &master);

        void setup_BIP_44_account (const std::string &wallet_name, const HD::BIP_32::pubkey &account_master, uint32 account = 0);

        struct readable;

        virtual ptr<readable> get_wallet (const std::string &name) = 0;

        // We use this to change the database.
        struct writable;

        struct readable {

            virtual const Cosmos::account *account () = 0;

            virtual const Cosmos::history *load_history () = 0;

            virtual const Cosmos::payments *payments () = 0;

            virtual maybe<secp256k1::signature> sign (const Bitcoin::incomplete::transaction &tx, const key_expression &k) = 0;

            // if this function returns normally, any changes
            // to the database will be saved. If an exception
            // is thrown, the database will remain unchanged
            // when the program is closed (although the in-memory
            // database will be changed).
            template <typename X> X update (function<X (writable w)> f);
        };

        struct writable {

            // key can be any standard key format (in quotes) or a derivation from another key.
            virtual void set_key (const std::string &key_name, const key_expression &k) = 0;
            virtual void to_private (const std::string &key_name, const key_expression &k) = 0;
            virtual void set_key_source (const std::string &name, const key_source &k) = 0;

            // Do we need this?
            virtual Cosmos::history *history () = 0;

            virtual void set_account (const Cosmos::account &) = 0;

            virtual void set_payments (const Cosmos::payments &) = 0;

        };

    };

}

#endif

