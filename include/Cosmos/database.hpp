#ifndef COSMOS_DATABASE
#define COSMOS_DATABASE

#include <Cosmos/database/txdb.hpp>
#include <Cosmos/database/price_data.hpp>
#include <Cosmos/history.hpp>
#include <Cosmos/options.hpp>

#include <Diophant/symbol.hpp>
#include <Diophant/machine.hpp>

template <typename X> using slice = data::slice<X>;

namespace Cosmos {

    struct database;

    Diophant::expression inline evaluate (const database &, Diophant::machine, const Diophant::expression &) {
        throw 0;
    }

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

        // TODO there needs to be a time limit for these things.
        virtual void set_wallet_unused (const std::string &wallet_name, const std::string &key_name) = 0;
        virtual void set_wallet_used (const std::string &wallet_name, const std::string &key_name) = 0;
        virtual list<std::string> get_wallet_unused (const std::string &wallet_name) = 0; 

        struct derivation {
            key_derivation Derivation;
            std::string KeyName;
            uint32 Index;
            derivation (const key_derivation &d, const std::string key_name, uint32 index = 0):
                Derivation {d}, KeyName {key_name}, Index {index} {}
            
            key_expression increment () {
                throw data::method::unimplemented {"derivation::increment"};
            }
        };

        virtual bool set_wallet_derivation (const std::string &wallet_name, const std::string &deriv_name, const derivation &) = 0;

        virtual list<derivation> get_wallet_derivations (const std::string &wallet_name) = 0;
        virtual maybe<derivation> get_wallet_derivation (const std::string &wallet_name, const std::string &deriv_name) = 0;

        virtual Cosmos::account get_wallet_account (const std::string &wallet_name) = 0;

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

