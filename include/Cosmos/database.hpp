#ifndef COSMOS_DATABASE
#define COSMOS_DATABASE

#include <Cosmos/database/txdb.hpp>
#include <Cosmos/database/price_data.hpp>
#include <Cosmos/history.hpp>
#include <Cosmos/wallet/wallet.hpp>

namespace Cosmos {

    struct database : local_TXDB, local_price_data {

        struct readable_wallet;

        virtual ptr<readable_wallet> get_wallet (const std::string &name) = 0;

        // We use this to change the database.
        struct writable_wallet;

        struct readable_wallet {

            virtual const Cosmos::keychain *keys (const std::string &name) = 0;
            virtual const Cosmos::pubkeys *pubkeys (const std::string &name) = 0;
            virtual const Cosmos::addresses *addresses (const std::string &name) = 0;
            virtual const Cosmos::account *account (const std::string &name) = 0;

            virtual const Cosmos::history *load_history (const std::string &name) = 0;

            virtual const Cosmos::payments *payments (const std::string &name) = 0;

            const maybe<Cosmos::wallet> wallet (const std::string &name);

            // if this function returns normally, any changes
            // to the database will be saved. If an exception
            // is thrown, the database will remain unchanged
            // when the program is closed (although the in-memory
            // database will be changed).
            template <typename X> X update (function<X (writable w)> f);
        };

        struct writable_wallet {
            Cosmos::history *history ();

            virtual void set_keys (const Cosmos::keychain &) = 0;
            virtual void set_pubkeys (const Cosmos::pubkeys &) = 0;
            virtual void set_account (const Cosmos::account &) = 0;
            virtual void set_addresses (const Cosmos::addresses &) = 0;

            virtual void set_wallet (const Cosmos::wallet &) = 0;
            virtual void set_payments (const Cosmos::payments &) = 0;
        };

    };

}

#endif

