#ifndef SERVER_RESTORE
#define SERVER_RESTORE

#include "generate.hpp"
#include <data/math/number/modular.hpp>

namespace schema = data::schema;

namespace Cosmos {

    enum class master_key_type {
        invalid,
        single_address,
        // a single sequnce of keys. This could be from any wallet but
        // is not a complete wallet.
        HD_sequence,

        // In this case, there will be receive and change derivations.
        BIP44_account,

        // in this case, we also need a coin type or wallet type.
        BIP44_master,
    };

    std::ostream &operator << (std::ostream &, master_key_type);
    std::istream &operator >> (std::istream &, master_key_type &);

    struct restore_request_options : generate_request_options {

        static auto schema () {
                // options relating to the wallet configuration
            return (schema::map::key<Cosmos::restore_wallet_style> ("style") || (
                    *schema::map::key<Cosmos::wallet_type> ("wallet_type") &&
                    *schema::map::key<Cosmos::derivation_style> ("derivation_style") &&
                    *(schema::map::key<Cosmos::coin_type> ("coin_type") ||
                        schema::map::key<bool> ("guess_coin_type")))) &&
                // options relating to the master keys
                (schema::map::key<UTF8> ("master_key") &&
                    *schema::map::key<Cosmos::master_key_type> ("master_key_type") ||
                    (schema::map::key<std::string> ("mnemonic") ||
                        schema::map::key<bytes> ("entropy")) &&
                    *(schema::map::key<UTF8> ("password") ||
                        schema::map::key<data::math::number::modular<uint16 (10000)>> ("CentBee_PIN") ||
                        schema::map::key<bool> ("guess_CentBee_PIN")) &&
                    *schema::map::key<Cosmos::mnemonic_style> ("mnemonic_style")) &&
                *schema::map::key<uint32> ("accounts") &&
                *schema::map::key<uint32> ("max_lookup");
        }

        bool GuessCoinType {false};
        bool GuessCentBeePIN {false};
        maybe<uint16> CentBeePIN;
        maybe<Cosmos::master_key_type> MasterKeyType;
        maybe<std::string> Key;
        maybe<bytes> Entropy;
        maybe<data::string> Mnemonic;
        maybe<uint32> MaxLookAhead;

        restore_request_options () noexcept {};
        restore_request_options (
            Diophant::symbol wallet_name, map<UTF8, UTF8> query,
            const maybe<net::HTTP::content> &content_type,
            const data::bytes &body);

        restore_request_options (const args::parsed &);

        restore_request_options &master_key_type (const master_key_type);
        restore_request_options &key (const std::string &);
        restore_request_options &entropy (const data::string &);
        restore_request_options &mnemonic (const data::string &);
        restore_request_options &max_look_ahead (uint32);

        bool valid () const;
    };

    struct restore_result {};
}

#endif
