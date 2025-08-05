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
        SHA1 = 1,
        MD5 = 2,
        SHA2_256 = 3,
        SHA2_512 = 4, 
        RIPEMD160 = 5, 
        SHA3_256 = 6,
        SHA3_512 = 7, 
        Hash256 = 8, 
        Hash160 = 9
    };

    bool supported (hash_function);
    
    std::ostream &operator << (std::ostream &, hash_function);

    struct database : local_TXDB, local_price_data {

        virtual bool make_wallet (const std::string &name) = 0;
        virtual data::list<std::string> list_wallet_names () = 0;

        virtual bool set_invert_hash (slice<const byte>, hash_function, slice<const byte>) = 0;
        virtual maybe<tuple<hash_function, bytes>> get_invert_hash (slice<const byte>) = 0;

        virtual bool set_key (const std::string &key_name, const key_expression &k) = 0;
        virtual key_expression get_key (const std::string &key_name) = 0;

        // set the private key for a given public key.
        virtual bool set_to_private (const std::string &key_name, const key_expression &k) = 0;
        virtual key_expression get_to_private (const std::string &key_name) = 0;

        struct derivation {
            key_derivation Derivation;
            std::string KeyName;
            uint32 Index;
            derivation (const key_derivation &d, const std::string key_name, uint32 index = 0):
                Derivation {d}, KeyName {key_name}, Index {index} {}
            
            key_expression increment () {
                throw method::unimplemented {"derivation::increment"};
            }
        };

        virtual bool set_wallet_derivation (const std::string &wallet_name, const std::string &deriv_name, const derivation &) = 0;

        virtual list<derivation> get_wallet_derivations (const std::string &wallet_name) = 0;

        virtual maybe<derivation> get_wallet_derivation (const std::string &wallet_name, const std::string &deriv_name) = 0;

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

    bool inline supported (hash_function f) {
        return byte (f) > 0 && byte (f) < 10;
    }
    
    std::ostream inline &operator << (std::ostream &o, hash_function f) {
        switch (f) {
            case hash_function::SHA1: return o << "SHA1";
            case hash_function::MD5: return o << "MD5";
            case hash_function::SHA2_256: return o << "SHA2 256";
            case hash_function::SHA2_512: return o << "SHA2 512";
            case hash_function::RIPEMD160: return o << "RIPEMD160";
            case hash_function::SHA3_256: return o << "SHA3 256";
            case hash_function::SHA3_512: return o << "SHA3 512";
            case hash_function::Hash256: return o << "Hash256";
            case hash_function::Hash160: return o << "Hash160";
            default: return o << "invalid hash function";
        }
    }

}

#endif

