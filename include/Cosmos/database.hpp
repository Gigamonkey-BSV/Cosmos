#ifndef COSMOS_DATABASE
#define COSMOS_DATABASE

#include <Cosmos/database/txdb.hpp>
#include <Cosmos/database/price_data.hpp>
#include <Cosmos/history.hpp>
#include <Cosmos/options.hpp>

#include <Diophant/symbol.hpp>

template <typename X> using slice = data::slice<X>;

namespace Cosmos {

    struct database;

    void setup_BIP_44_wallet (const HD::BIP_32::secret &master, list<uint32> accounts = {1});

    enum class hash_function : byte {
        invalid = 0,
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
    std::istream &operator << (std::istream &, hash_function &);

    struct database : local_TXDB, local_price_data {

        virtual bool set_invert_hash (slice<const byte> digest, hash_function, slice<const byte> data) = 0;
        virtual maybe<tuple<hash_function, bytes>> get_invert_hash (slice<const byte>) = 0;

        virtual bool make_wallet (const std::string &name) = 0;
        virtual data::list<std::string> list_wallet_names () = 0;

        virtual bool set_key (const std::string &wallet_name, const Diophant::symbol &key_name, const key_expression &k) = 0;
        virtual key_expression get_key (const std::string &wallet_name, const Diophant::symbol &key_name) = 0;

        // set the private key for a given public key.
        virtual bool set_to_private (const key_expression &key_name, const key_expression &k) = 0;
        virtual key_expression get_to_private (const key_expression &key_name) = 0;

        // this one returns void because we can overwrite what's already there.
        virtual bool set_wallet_sequence (
            const std::string &wallet_name,
            const Diophant::symbol &sequence_name,
            const key_sequence &sequence,
            uint32 index) = 0;

        virtual maybe<key_source> get_wallet_sequence (const std::string &wallet_name, const std::string &key_name) = 0;

        // TODO there needs to be a time limit for these things.
        virtual void set_wallet_unused (const std::string &wallet_name, const std::string &key_name) = 0;
        virtual void set_wallet_used (const std::string &wallet_name, const std::string &key_name) = 0;
        virtual list<std::string> get_wallet_unused (const std::string &wallet_name) = 0; 

        virtual Cosmos::account get_wallet_account (const std::string &wallet_name) = 0;

    };

    bool inline supported (hash_function f) {
        return byte (f) > 0 && byte (f) < 10;
    }

}

#endif

