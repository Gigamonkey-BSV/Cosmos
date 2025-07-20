#ifndef COSMOS_DATABASE
#define COSMOS_DATABASE

#include <Cosmos/database/txdb.hpp>
#include <Cosmos/database/price_data.hpp>
#include <Cosmos/history.hpp>
#include <Cosmos/options.hpp>

namespace Cosmos {

    struct database;

    void setup_BIP_44_wallet (const HD::BIP_32::secret &master, list<uint32> accounts = {1});

    enum class hash_function : byte {
        SHA1 = 0,
        SHA2_256 = 1,
        RIPEMD_160 = 2, 
        Hash256 = 3, 
        Hash160 = 4
    };

    struct database : local_TXDB, local_price_data {

        virtual bool make_wallet (const std::string &name) = 0;
        virtual data::list<std::string> list_wallet_names () = 0;

        virtual bool set_invert_hash (slice<const byte>, hash_function, slice<const byte>) = 0;
        virtual maybe<tuple<hash_function, bytes>> get_invert_hash (slice<const byte>) = 0;

        virtual bool set_key (const std::string &key_name, const key_expression &k) = 0;

        // set the private key for a given public key.
        virtual bool set_to_private (const std::string &key_name, const key_expression &k) = 0;
        virtual key_expression get_to_private (const std::string &key_name, const key_expression &k) = 0;

        virtual key_expression get_key (const std::string &key_name) = 0;

        // set the private key for a given public key.
        virtual key_expression get_private (const std::string &key_name) = 0;

        struct derivation {
            key_derivation Derivation;
            std::string KeyName;
            uint32 Index;
            derivation (const key_derivation &d, const std::string key_name, uint32 index = 0):
                Derivation {d}, KeyName {key_name}, Index {index} {}
        };

        virtual bool set_derivation (const std::string &wallet_name, const std::string &deriv_name, const derivation &) = 0;

        virtual list<derivation> get_wallet_derivations (const std::string &wallet_name) = 0;

        virtual Cosmos::account get_wallet_account (const std::string &wallet_name) = 0;

        struct readable;

        virtual ptr<readable> get_wallet (const std::string &name) = 0;

        // We use this to change the database.
        struct writable;

        struct readable {

            virtual list<derivation> derivations () = 0;

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
            virtual bool set_derivation (const std::string &name, const derivation &) = 0;

            // Do we need this?
            virtual Cosmos::history *history () = 0;

            virtual void set_account (const Cosmos::account &) = 0;

            virtual void set_payments (const Cosmos::payments &) = 0;

        };

    };

}

#endif

