#include "restore.hpp"
#include <data/tools/map_schema.hpp>

namespace schema = data::schema;

template <typename X>
X inline set_with_default (const maybe<X> &opt, const X &def) {
    return bool (opt) ? *opt : def;
}

net::HTTP::response handle_restore (server &p,
    Diophant::symbol wallet_name, map<UTF8, UTF8> query,
    const maybe<net::HTTP::content> &content_type,
    const data::bytes &body) {

    // todo: if words fail, we need an option to try to fix errors.
    auto [
        derivation_options,
        key_options,
        accounts_param,
        max_lookup_param] = schema::validate<> (query,
        (schema::key<restore_wallet_type> ("wallet_type") | (
            *schema::key<wallet_style> ("wallet_style") &
            *schema::key<derivation_style> ("derivation_style") &
            *schema::key<coin_type> ("coin_type"))) &
        (schema::key<std::string> ("key") & *schema::key<std::string> ("key_type") |
            (*schema::key<std::string> ("password") &
                (schema::key<bytes> ("entropy") |
                    schema::key<std::string> ("mnemonic") &
                    *schema::key<std::string> ("mnemonic_style")))) &
        *schema::key<uint32> ("accounts") &
        *schema::key<uint32> ("max_lookup"));

    uint32 total_accounts = set_with_default (accounts_param, uint32 {1});
    uint32 max_lookup = set_with_default (max_lookup_param, uint32 {25});

    // somehow we need to get this key to be valid.
    HD::BIP_32::pubkey pk;

    // we may have a secret key provided.
    maybe<HD::BIP_32::secret> sk;

    // we need to figure out what kind of key it is.
    master_key_type KeyType {master_key_type::invalid};

    throw data::method::unimplemented {"handle_restore"};

    {
        bool derive_from_mnemonic = key_options.index () == 1;
        wallet_style WalletStyle {wallet_style::invalid};
        coin_type CoinType {};

        switch (derivation_options.index ()) {
            case 0: {
                restore_wallet_type restore_type = std::get<0> (derivation_options);
            } break;
            case 1: {
                auto [wallet_style_option, derivation_style_option, coin_type_option] = std::get<1> (derivation_options);

                // we need either derivation_style_option or coin_type_option
                // and if we have both, they need to be consistent.

            } break;
            default: throw data::exception {} << "Should not be able to get here";
        }

        if (derive_from_mnemonic) {
            auto [password_option, mnemonic_options] = std::get<1> (key_options);
            std::string password;

            bytes entropy;

            switch (mnemonic_options.index ()) {
                case 0: {
                    auto entropy_option = std::get<0> (mnemonic_options);
                } break;
                case 1: {
                    auto [mnemonic_option, style_option] = std::get<1> (mnemonic_options);

                    // TODO suppose the mnemonic doesn't work. Need options for trying to fix it.
                }
            }

            // now we have entropy so we derive key from entropy.

            // derive pubkey from secret key.

        } else {
            auto [key_option, type_option] = std::get<0> (key_options);

            // TODO
        }
    }

    // TODO generate the wallet

    // TODO restore the wallet

}
