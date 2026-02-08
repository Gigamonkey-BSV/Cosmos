
#include <Cosmos/REST/REST.hpp>
#include <data/tools/schema.hpp>

#include <gigamonkey/wif.hpp>
#include <gigamonkey/schema/electrum_sv.hpp>
#include <gigamonkey/schema/bip_39.hpp>

namespace schema = data::schema;

namespace Cosmos {
    decltype (
        (schema::map::key<restore_wallet_style> ("style") || (
            *schema::map::key<wallet_type> ("wallet_type") &&
            *schema::map::key<derivation_style> ("derivation_style") &&
            *(schema::map::key<coin_type> ("coin_type") ||
                schema::map::key<bool> ("guess_coin_type")))) &&
        (schema::map::key<UTF8> ("key") &&
            *schema::map::key<master_key_type> ("key_type") ||
            (*(schema::map::key<UTF8> ("password") ||
                schema::map::key<uint16> ("CentBee_PIN") ||
                schema::map::key<bool> ("guess_CentBee_PIN")) &&
                (schema::map::key<UTF8> ("mnemonic") ||
                    schema::map::key<bytes> ("entropy")) &&
                *schema::map::key<mnemonic_style> ("mnemonic_style"))) &&
        *schema::map::key<uint32> ("accounts") &&
        *schema::map::key<uint32> ("max_lookup")) RestoreOptionsSchema =
            (schema::map::key<restore_wallet_style> ("style") || (
                *schema::map::key<wallet_type> ("wallet_type") &&
                *schema::map::key<derivation_style> ("derivation_style") &&
                *(schema::map::key<coin_type> ("coin_type") ||
                    schema::map::key<bool> ("guess_coin_type")))) &&
            (schema::map::key<UTF8> ("key") &&
                *schema::map::key<master_key_type> ("key_type") ||
                (*(schema::map::key<UTF8> ("password") ||
                    schema::map::key<uint16> ("CentBee_PIN") ||
                    schema::map::key<bool> ("guess_CentBee_PIN")) &&
                    (schema::map::key<UTF8> ("mnemonic") ||
                        schema::map::key<bytes> ("entropy")) &&
                    *schema::map::key<mnemonic_style> ("mnemonic_style"))) &&
            *schema::map::key<uint32> ("accounts") &&
            *schema::map::key<uint32> ("max_lookup");

    std::ostream &operator << (std::ostream &, master_key_type) {
        throw data::method::unimplemented {" << master_key_type"};
    }

    std::istream &operator >> (std::istream &i, master_key_type &x) {
        std::string word;
        i >> word;
        if (!i) return i;
        std::string sanitized = sanitize (word);
        if (sanitized == "single_address") x = master_key_type::single_address;
        else if (sanitized == "hdsequence") x = master_key_type::HD_sequence;
        else if (sanitized == "bip44account") x = master_key_type::BIP44_account;
        else if (sanitized == "bip44master") x = master_key_type::BIP44_master;
        else i.setstate (std::ios::failbit);
        return i;
    }

    restore_request_options::restore_request_options (const args::parsed &p) {

        // we must have either a key and optional key type, entropy, or words.
        auto [flags, name, opts] = args::validate (p, args::command {
            set<std::string> {}, schema::list::value<Diophant::symbol> (), RestoreOptionsSchema});

        this->Name = name;



    }
}
