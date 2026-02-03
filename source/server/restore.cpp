#include "restore.hpp"
#include "../method.hpp"
#include <data/tools/schema.hpp>

#include <gigamonkey/wif.hpp>
#include <gigamonkey/schema/electrum_sv.hpp>
#include <gigamonkey/schema/bip_39.hpp>

namespace schema = data::schema;

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

template <typename X>
X inline set_with_default (const maybe<X> &opt, const X &def) {
    return bool (opt) ? *opt : def;
}

net::HTTP::response handle_restore (server &p,
    Diophant::symbol wallet_name, map<UTF8, UTF8> query,
    const maybe<net::HTTP::content> &content_type,
    const data::bytes &body) {

    // TODO: if words fail, we need an option to try to fix errors.
    // TODO: need option for gussing coin type.
    auto [
        derivation_options,
        key_options,
        accounts_param,
        max_lookup_param] = schema::validate<> (query,
        (schema::map::key<restore_wallet_type> ("wallet_type") || (
            *schema::map::key<wallet_style> ("wallet_style") &&
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
        *schema::map::key<uint32> ("max_lookup"));

    uint32 total_accounts = set_with_default (accounts_param, uint32 {1});
    uint32 max_lookup = set_with_default (max_lookup_param, uint32 {25});

    // somehow we need to get this key to be valid.
    HD::BIP_32::pubkey pk;

    // we may have a secret key provided.
    maybe<HD::BIP_32::secret> sk;

    // we need to figure out what kind of key it is.
    master_key_type KeyType {master_key_type::invalid};

    wallet_style WalletStyle {wallet_style::invalid};

    maybe<coin_type> CoinType {};
    bool guess_coin_type {false};

    {

        switch (derivation_options.index ()) {
            case 0: {
                restore_wallet_type restore_type = std::get<0> (derivation_options);
                WalletStyle = wallet_style::BIP_44;
                switch (restore_type) {
                    case restore_wallet_type::Money_Button: {
                        CoinType = HD::BIP_44::moneybutton_coin_type;
                    } break;
                    case restore_wallet_type::RelayX: {
                        CoinType = HD::BIP_44::relay_x_coin_type;
                    } break;
                    case restore_wallet_type::Simply_Cash: {
                        CoinType = HD::BIP_44::simply_cash_coin_type;
                    } break;
                    case restore_wallet_type::Electrum_SV: {
                        CoinType = HD::BIP_44::electrum_sv_coin_type;
                    } break;
                    case restore_wallet_type::CentBee: {
                        CoinType = coin_type {};
                    } break;
                    default: throw data::method::unimplemented {"wallet types for GENERATE method"};
                }
            } break;
            case 1: {
                // we need either derivation_style_option or coin_type_option
                // and if we have both, they need to be consistent.
                auto [wallet_style_option, derivation_style_option, coin_type_option] = std::get<1> (derivation_options);

                if (coin_type_option) {
                    // coin type was provided
                    if (coin_type_option->index () == 0)
                        CoinType = std::get<0> (*coin_type_option);
                    else guess_coin_type = std::get<1> (*coin_type_option);
                }

                if (bool (derivation_style_option)) {
                    if (*derivation_style_option == derivation_style::BIP_44) {
                        if (!bool (CoinType) || !bool (*CoinType))
                            return error_response (400, method::RESTORE, problem::invalid_parameter,
                                "If derivation_style is BIP_44, then coin_type must be povided and not be 'none'");
                    } else {
                        if (!bool (CoinType)) {
                            if (guess_coin_type)
                                return error_response (400, method::RESTORE, problem::invalid_query,
                                    "Do not need to guess coin type since we have derivation_style=centbee");
                            else CoinType = coin_type {};
                        } if (bool (CoinType) && bool (*CoinType))
                            return error_response (400, method::RESTORE, problem::invalid_parameter,
                                "If derivation_style is CentBee, then coin_type, if provided, must be 'none'");
                    }
                } else if (!bool (CoinType))
                    return error_response (400, method::RESTORE, problem::invalid_parameter,
                        "Either derivation_style or coin_type must be provided");

                if (bool (wallet_style_option)) WalletStyle = *wallet_style_option;

            } break;
            default: throw data::exception {} << "Should not be able to get here";
        }

        // if we do not know coin type at this point, then guess coin type must be set.
        if (!CoinType && !guess_coin_type)
            return error_response (400, method::RESTORE, problem::invalid_query,
                "Please provide a coin_type parameter or set guess_coin_type to true");

        bool derive_from_mnemonic = key_options.index () == 1;
        if (derive_from_mnemonic) {
            auto [password_option, mnemonic_or_entropy, mnemonic_style_option] = std::get<1> (key_options);

            mnemonic_style MnemonicStyle = set_with_default (mnemonic_style_option, mnemonic_style::BIP_39);

            // now we need some way to calculate the words.
            data::UTF8 words;

            switch (mnemonic_or_entropy.index ()) {
                case 0: {
                    words = std::get<0> (mnemonic_or_entropy);
                } break;
                case 1: {
                    auto entropy = std::get<1> (mnemonic_or_entropy);

                    if (MnemonicStyle == mnemonic_style::BIP_39) {
                        words = HD::BIP_39::generate (entropy);
                    } else {
                        words = encoding::UTF8::encode (HD::Electrum_SV::generate (entropy));
                    }
                }
            }

            UTF8 password {};
            bool guess_CentBee_PIN {false};
            if (bool (password_option)) {
                switch (password_option->index ()) {
                    case 0: {
                        password = std::get<0> (*password_option);
                    } break;
                    case 1: {
                        uint16 centbee_PIN = std::get<1> (*password_option);

                        // must be in the correct range.
                        if (centbee_PIN > 9999)
                            return error_response (400, method::RESTORE, problem::invalid_query, "Invalid CentBee PIN range");

                        password = std::to_string (centbee_PIN);
                    } break;
                    case 2: {
                        guess_CentBee_PIN = std::get<2> (*password_option);
                        if (!guess_CentBee_PIN)
                            return error_response (400, method::RESTORE, problem::invalid_query,
                                "Please set guess_centbee_pin to true and we will attempt to figure it out.");

                    }

                    if (password_option->index () != 1) {
                        if (KeyType == master_key_type::invalid) KeyType = master_key_type::BIP44_master;
                        else if (KeyType != master_key_type::BIP44_master)
                            return error_response (400, method::RESTORE, problem::invalid_query,
                                "CentBee option implies that key_type must be BIP_44_master");

                        if (WalletStyle == wallet_style::invalid) WalletStyle = wallet_style::BIP_44;
                        else if (WalletStyle != wallet_style::BIP_44)
                            return error_response (400, method::RESTORE, problem::invalid_query,
                                "CentBee option implies that wallet_style must be BIP_44");
                    }
                }
            }
            // In some cases, we can guess the wallet style from other options.
            // TODO support single address style.
            switch (WalletStyle) {
                case wallet_style::invalid: {
                    // assume bip 44 in this case.
                    if (derive_from_mnemonic) {
                        WalletStyle == wallet_style::BIP_44;
                        break;
                    } else return error_response (400, method::RESTORE, problem::missing_parameter,
                        "Need to set parameter style");
                }
                case wallet_style::single_address:
                case wallet_style::HD_sequence:
                    return error_response (400, method::RESTORE, problem::invalid_query,
                        "derivation from mnemonic is incompatible with single address or hd sequence wallet styles.");
            }

            // now we try to generate seed from words.
            HD::seed seed;

            if (guess_CentBee_PIN) throw data::method::unimplemented {"method::RESTORE: guess centbee pin"};
            else if (MnemonicStyle == mnemonic_style::BIP_39) {
                if (!HD::BIP_39::valid (words))
                    return error_response (400, method::RESTORE, problem::invalid_query,
                        "Invalid mnemonic provided.");

                seed = HD::BIP_39::read (words, password);
            } else {
                if (!HD::Electrum_SV::valid (words))
                    return error_response (400, method::RESTORE, problem::invalid_query,
                        "Invalid mnemonic provided.");

                seed = HD::Electrum_SV::read (words, password);
            }

            // Now we generate keys from seed.
            sk = HD::BIP_32::secret::from_seed (seed);

            // derive pubkey from secret key.
            pk = sk->to_public ();

        } else {
            auto [key_option, type_option] = std::get<0> (key_options);

            if (Bitcoin::address perhaps_address {key_option}; perhaps_address.valid ()) {
                if (bool (type_option)) {
                    if (*type_option != master_key_type::single_address)
                        return error_response (400, method::RESTORE, problem::invalid_query,
                            "Restoring a bitcoin address is only compatible with key type single_address");
                } else KeyType = master_key_type::single_address;

                throw data::method::unimplemented {"method::GENERATE, single address restore"};
            } else if (Bitcoin::pubkey perhaps_pubkey {key_option}; perhaps_pubkey.valid ()) {
                if (bool (type_option)) {
                    if (*type_option != master_key_type::single_address)
                        return error_response (400, method::RESTORE, problem::invalid_query,
                            "Restoring a bitcoin pubkey is only compatible with key type single_address");
                } else KeyType = master_key_type::single_address;

                throw data::method::unimplemented {"method::GENERATE, single address restore"};
            } else if (Bitcoin::secret perhaps_WIF {key_option}; perhaps_WIF.valid ()) {
                if (bool (type_option)) {
                    if (*type_option != master_key_type::single_address)
                        return error_response (400, method::RESTORE, problem::invalid_query,
                            "Restoring a bitcoin WIF is only compatible with key type single_address");
                } else KeyType = master_key_type::single_address;

                throw data::method::unimplemented {"method::GENERATE, single address restore"};
            } else if (HD::BIP_32::pubkey perhaps_HD_pubkey {key_option}; perhaps_HD_pubkey.valid ()) {
                if (bool (type_option)) {
                    if (*type_option == master_key_type::single_address)
                        return error_response (400, method::RESTORE, problem::invalid_query,
                            "HD pubkey is incompatible with key type single_address");
                }

                pk = perhaps_HD_pubkey;
            } else if (HD::BIP_32::secret perhaps_secret {key_option}; perhaps_secret.valid ()) {
                if (bool (type_option)) {
                    if (*type_option == master_key_type::single_address)
                        return error_response (400, method::RESTORE, problem::invalid_query,
                            "HD secret is incompatible with key type single_address");
                }

                sk = perhaps_secret;
                pk = perhaps_secret.to_public ();
            }

            // TODO we have to make sure that key type, if provided, matches
            // what was provided in derivation options.

            // at this point, it is possible that WalletStyle is not set. Can we set it?
            switch (WalletStyle) {
                case wallet_style::invalid: {
                    switch (KeyType) {

                    }
                } break;
                case wallet_style::single_address:
                    if (KeyType != master_key_type::single_address)
                        return error_response (400, method::RESTORE, problem::invalid_query,
                            "derivation from mnemonic is incompatible with single address or hd sequence wallet styles.");
                case wallet_style::HD_sequence:
                    return error_response (400, method::RESTORE, problem::invalid_query,
                        "derivation from mnemonic is incompatible with single address or hd sequence wallet styles.");
            }

            throw data::method::unimplemented {"what to do if mnemonic isn't provided in method::RESTORE"};
        }
    }

    if (!bool (sk)) {
        throw data::method::unimplemented {"method RESTORE with no private key"};
    }

    // NOTE: we do not necessarily want to make a whole new wallet every time we restore.
    if (!p.DB.make_wallet (wallet_name))
        return error_response (500, method::RESTORE, problem::failed,
            string::write ("wallet ", wallet_name, " already exists"));

    // set master key
    switch (KeyType) {
        case master_key_type::BIP44_master: {

        } break;
        default: throw data::method::unimplemented {"method RESTORE: key types other than bip 44 master"};
    }

    if (!bool (CoinType)) throw data::method::unimplemented {"method RESTORE: guess coin type"};

    coin_type CoinTypeFinal = *CoinType;

    // if coin type is none, then we make a centbee-style account.
    list<uint32> root_derivation = bool (CoinTypeFinal) ?
        list<uint32> {HD::BIP_44::purpose, *CoinTypeFinal}:
        list<uint32> {HD::BIP_44::purpose};

    string account_name {"account"};

    // TODO set this;
    key_expression master_key_expr;

    // set a sequence for the accounts.
    p.DB.set_wallet_sequence (wallet_name, account_name,
        key_sequence {
            key_expression {string::write ("(", master_key_expr, ") ", write_derivation (root_derivation))},
            key_derivation {string::write ("@ key index -> key / harden (index)")}},
        total_accounts);

    if (WalletStyle == wallet_style::BIP_44_plus || WalletStyle == wallet_style::experimental)
        throw data::method::unimplemented {"restore extended bip 44 types"};

    // generate all the accounts.
    for (uint32 account_number = 0; account_number < total_accounts; account_number++) {

        list<uint32> account_derivation = root_derivation << HD::BIP_32::harden (account_number);

        list<uint32> receive_derivation = account_derivation << 0;
        list<uint32> change_derivation = account_derivation << 1;

        key_expression receive_pubkey = key_expression {sk->derive (receive_derivation).to_public ()};
        key_expression change_pubkey = key_expression {sk->derive (change_derivation).to_public ()};

        p.DB.set_to_private (receive_pubkey,
            key_expression {string::write ("(", master_key_expr, ") ",
                write_derivation (receive_derivation))});

        p.DB.set_to_private (change_pubkey,
            key_expression {string::write ("(", master_key_expr, ") ",
                write_derivation (change_derivation))});

        string receive_name = string::write ("receive_", account_number);
        string change_name = string::write ("change_", account_number);

        // note that each of these returns a regular pubkey rather than an xpub.
        p.DB.set_wallet_sequence (wallet_name, receive_name,
            key_sequence {
                receive_pubkey,
                key_derivation {"@ key index -> pubkey (key / index)"}}, 0);

        p.DB.set_wallet_sequence (wallet_name, change_name,
            key_sequence {
                change_pubkey,
                key_derivation {"@ key index -> pubkey (key / index)"}}, 0);

    }

    // TODO restore the wallet
    throw data::method::unimplemented {"method RESTORE"};

}

restore_request_options::restore_request_options (const args::parsed &) {

    throw data::method::unimplemented {"method RESTORE"};
}
