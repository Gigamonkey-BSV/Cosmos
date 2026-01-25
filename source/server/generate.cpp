
#include "method.hpp"
#include "generate.hpp"

#include <gigamonkey/schema/bip_39.hpp>
#include <data/tools/map_schema.hpp>
#include <data/crypto/random.hpp>

namespace schema = data::schema;

coin_type::coin_type (coin_type::value v): maybe<uint32> {} {
    switch (v) {
        case Bitcoin: {
            *this = HD::BIP_44::coin_type_Bitcoin;
            return;
        }
        case Bitcoin_Cash: {
            *this = HD::BIP_44::coin_type_Bitcoin_Cash;
            return;
        }
        case Bitcoin_SV: {
            *this = HD::BIP_44::coin_type_Bitcoin_SV;
        }
    }
}

std::istream &operator >> (std::istream &i, coin_type &x) {
    std::string word;
    i >> word;
    if (!i) return i;
    std::string sanitized = sanitize (word);
    if (sanitized == "none") x = coin_type {};
    else if (sanitized == "bitcoin") x = coin_type {coin_type::Bitcoin};
    else if (sanitized == "bitcoincash") x = coin_type {coin_type::Bitcoin_Cash};
    else if (sanitized == "bitcoinsv") x = coin_type {coin_type::Bitcoin_SV};
    else if (!parse_uint32 (word, *x)) i.setstate (std::ios::failbit);
    return i;
}

std::ostream &operator << (std::ostream &o, coin_type x) {
    if (!bool (x)) return o << "none";

    uint32 v = *x;
    if (v == HD::BIP_44::coin_type_Bitcoin) return o << "Bitcoin";
    if (v == HD::BIP_44::coin_type_Bitcoin_Cash) return o << "BitcoinCash";
    if (v == HD::BIP_44::coin_type_Bitcoin_SV) return o << "BitcoinSV";
    return o << v;
}

generate_request_options::operator net::HTTP::request () const {
    std::stringstream query_stream;
    query_stream << "style=" << WalletStyle;
    if (!bool (CoinTypeDerivationParameter)) query_stream << "&coin_type=none";
    else query_stream << "&coin_type=" << *CoinTypeDerivationParameter;
    if (MnemonicStyle != mnemonic_style::none)
        query_stream << "&mnemonic=" << MnemonicStyle << "&number_of_words=" << NumberOfWords;
    return net::HTTP::request::make {}.path (
        string::write ("/generate/", Name)).query (query_stream.str ()).host ("localhost").method (net::HTTP::method::post);
}

std::ostream &operator << (std::ostream &o, wallet_style x) {
    switch (x) {
        case wallet_style::BIP_44: return o << "BIP44";
        case wallet_style::BIP_44_plus: return o << "BIP44plus";
        case wallet_style::experimental: return o << "experimental";
        default: throw 0;
    }
}

std::istream &operator >> (std::istream &i, wallet_style &x) {
    std::string word;
    i >> word;
    if (!i) return i;
    std::string sanitized = sanitize (word);
    if (sanitized == "bip44") x = wallet_style::BIP_44;
    else if (sanitized == "bip44plus") x = wallet_style::BIP_44_plus;
    // experimental means me generate two secp256k1 keys and use one as the chain code.
    // this allows us to use bip 32 and hd stuff and future protocols.
    else if (sanitized == "experimental") x = wallet_style::experimental;
    else i.setstate (std::ios::failbit);
    return i;
}

std::ostream &operator << (std::ostream &o, mnemonic_style x) {
    switch (x) {
        case mnemonic_style::none: return o << "none";
        case mnemonic_style::BIP_39: return o << "BIP39";
        case mnemonic_style::Electrum_SV: return o << "ElectrumSV";
        default: throw 0;
    }
}

std::istream &operator >> (std::istream &i, mnemonic_style &x) {
    std::string word;
    i >> word;
    if (!i) return i;
    std::string sanitized = sanitize (word);
    if (sanitized == "electrumsv") x = mnemonic_style::Electrum_SV;
    else if (sanitized == "bip39") x = mnemonic_style::BIP_39;
    else i.setstate (std::ios::failbit);
    return i;
}

std::istream &operator >> (std::istream &i, derivation_style &x) {
    std::string word;
    i >> word;
    if (!i) return i;
    std::string sanitized = sanitize (word);
    if (sanitized == "bip44") x = derivation_style::BIP_44;
    else if (sanitized == "centbee") x = derivation_style::CentBee;
    else i.setstate (std::ios::failbit);
    return i;
}

std::ostream &operator << (std::ostream &o, derivation_style x) {
    switch (x) {
        case derivation_style::CentBee: return o << "CentBee";
        case derivation_style::BIP_44: return o << "BIP44";
        default: return o << "unknown";
    }
}

std::istream &operator >> (std::istream &i, restore_wallet_type &x) {
    std::string word;
    i >> word;
    if (!i) return i;
    std::string sanitized = sanitize (word);
    if (sanitized == "moneybutton") x = restore_wallet_type::Money_Button;
    else if (sanitized == "relayx") x = restore_wallet_type::RelayX;
    else if (sanitized == "simplycash") x = restore_wallet_type::Simply_Cash;
    else if (sanitized == "electrumsv") x = restore_wallet_type::Electrum_SV;
    else if (sanitized == "centbee") x = restore_wallet_type::CentBee;
    else i.setstate (std::ios::failbit);
    return i;
}

std::ostream &operator << (std::ostream &o, restore_wallet_type x) {
    switch (x) {
        case restore_wallet_type::Money_Button: return o << "MoneyButton";
        case restore_wallet_type::RelayX: return o << "RelayX";
        case restore_wallet_type::Simply_Cash: return o << "SimplyCash";
        case restore_wallet_type::Electrum_SV: return o << "ElectrumSV";
        case restore_wallet_type::CentBee: return o << "CentBee";
        default: return o << "unknown";
    }
}

std::string write_derivation (list<uint32> d) {
    std::stringstream ss;
    for (const uint32 &u : d) {
        ss << " / ";
        if (HD::BIP_32::hardened (u)) ss << "`" << HD::BIP_32::soften (u);
        else ss << u;
    }
    return ss.str ();
}

// read from a generate request.
generate_request_options::generate_request_options (
    Diophant::symbol wallet_name,
    map<UTF8, UTF8> query,
    const maybe<net::HTTP::content> &content_type,
    const data::bytes &body) {

    Name = wallet_name;

    // either we have a use an existing wallet style
    // that has options built in or we specify certain
    // things about the derivation path. In particular,
    // there are differences of opinion in what should
    // be used as the coin_type parameter in a BIP44
    // derivation path.
    auto derivation_path_schema = schema::key<restore_wallet_type> ("format") ||
        (schema::key<::wallet_style> ("style") &&
        *schema::key<::derivation_style> ("derivation_style") &&
        *schema::key<::coin_type> ("coin_type"));

    // we may not use a mnemonic at all but if we do
    // there are some options that apply. In particular
    // ElectrumSV uses a special set of words.
    auto mnemonic_schema = schema::key<::mnemonic_style> ("mnemonic") &&
        *schema::key<uint32> ("number_of_words") &&
        *schema::key<std::string> ("password");

    auto generate_schema = *mnemonic_schema && *schema::key<uint32> ("accounts") && derivation_path_schema;

    auto [mnemonic_options, accounts_param, wallet_generation] =
        schema::validate<> (query, generate_schema);

    switch (wallet_generation.index ()) {
        case 0: {
            WalletStyle = wallet_style::BIP_44;
            restore_wallet_type WalletType = std::get<0> (wallet_generation);
            switch (WalletType) {
                case restore_wallet_type::Money_Button: {
                    CoinTypeDerivationParameter = HD::BIP_44::moneybutton_coin_type;
                } break;
                case restore_wallet_type::RelayX: {
                    CoinTypeDerivationParameter = HD::BIP_44::relay_x_coin_type;
                } break;
                case restore_wallet_type::Simply_Cash: {
                    CoinTypeDerivationParameter = HD::BIP_44::simply_cash_coin_type;
                } break;
                case restore_wallet_type::Electrum_SV: {
                    CoinTypeDerivationParameter = HD::BIP_44::electrum_sv_coin_type;
                } break;
                case restore_wallet_type::CentBee: {
                    CoinTypeDerivationParameter = ::coin_type {};
                } break;
                default: throw data::method::unimplemented {"wallet types for GENERATE method"};
            };
        } break;
        case 1: {
            maybe<derivation_style> dstyle;
            std::tie (WalletStyle, dstyle, CoinTypeDerivationParameter) = std::get<1> (wallet_generation);

            if (bool (dstyle)) DerivationStyle = *dstyle;
        };
    }

    if (mnemonic_options) {
        maybe<uint32> num_words;
        std::tie (MnemonicStyle, num_words, Password) = *mnemonic_options;

        if (bool (num_words)) NumberOfWords = *num_words;
    }

    if (MnemonicStyle == mnemonic_style::Electrum_SV)
        throw data::method::unimplemented {"Electrum SV mnemonic style"};

    if (WalletStyle == wallet_style::experimental)
        throw data::method::unimplemented {"experimental wallet style"};

    if (bool (Password))
        throw data::method::unimplemented {"wallet with password"};

    if (accounts_param) Accounts = *accounts_param;
}

generate_error generate_request_options::check () const {
    if (DerivationStyle != ::derivation_style::unset) {
        if (DerivationStyle == ::derivation_style::BIP_44) {
            if (!bool (CoinTypeDerivationParameter) || !bool (*CoinTypeDerivationParameter))
                return generate_error::words_vs_mnemonic_style;
        } else if (bool (CoinTypeDerivationParameter) && bool (*CoinTypeDerivationParameter))
            return generate_error::centbee_vs_coin_type;
    } else if (!CoinTypeDerivationParameter)
        return generate_error::neither_style_nor_coin_type;

    if (MnemonicStyle == mnemonic_style::none && NumberOfWords != 0)
        return generate_error::mnemonic_vs_number_of_words;

    if (NumberOfWords != 0 && NumberOfWords != 12 && NumberOfWords != 24)
        return generate_error::invalid_number_of_words;

    if (Accounts < 1) return generate_error::zero_accounts;

    return generate_error::valid;
}

generate_error generate (database &DB, const std::string &wallet_words, const generate_request_options &gen) {

    auto root = HD::BIP_32::secret::from_seed (HD::BIP_39::read (wallet_words));

    // if coin type is none, then we make a centbee-style account.
    list<uint32> root_derivation = bool (gen.coin_type ()) ?
        list<uint32> {HD::BIP_44::purpose, *gen.coin_type ()}:
        list<uint32> {HD::BIP_44::purpose};

    auto master = root.derive (root_derivation);

    // having read all the settings, we proceed to alter the database.
    if (!DB.make_wallet (gen.Name))
        return generate_error::wallet_already_exists;

    // set root key
    DB.set_key (gen.Name, Diophant::symbol {"root"}, key_expression {root});
    DB.set_key (gen.Name, Diophant::symbol {"master"}, key_expression {master});

    // generate all the accounts.
    for (uint32 account_number = 0; account_number < gen.Accounts; account_number++) {

        list<uint32> account_derivation = {HD::BIP_32::harden (account_number)};

        list<uint32> receive_derivation = account_derivation << 0;
        list<uint32> change_derivation = account_derivation << 1;

        key_expression receive_pubkey = key_expression {master.derive (receive_derivation).to_public ()};
        key_expression change_pubkey = key_expression {master.derive (change_derivation).to_public ()};

        DB.set_to_private (receive_pubkey,
            key_expression {string::write ("(", key_expression {master}, ") ",
                write_derivation (receive_derivation))});

        DB.set_to_private (change_pubkey,
            key_expression {string::write ("(", key_expression {master}, ") ",
                write_derivation (change_derivation))});

        string receive_name;
        string change_name;

        if (account_number == 0) {
            receive_name = "receive";
            change_name = "change";
        } else {
            receive_name = string::write ("receive_", account_number);
            change_name = string::write ("change_", account_number);
        }

        // note that each of these returns a regular pubkey rather than an xpub.
        DB.set_wallet_sequence (gen.Name, receive_name,
            key_sequence {
                receive_pubkey,
                key_derivation {"@ key index -> key / index"}}, 0);

        DB.set_wallet_sequence (gen.Name, change_name,
            key_sequence {
                change_pubkey,
                key_derivation {"@ key index -> key / index"}}, 0);

    }

    return generate_error::valid;

}

net::HTTP::response handle_generate (server &p,
    Diophant::symbol wallet_name, map<UTF8, UTF8> query,
    const maybe<net::HTTP::content> &content_type,
    const data::bytes &body) {

    generate_request_options gen {wallet_name, query, content_type, body};

    generate_error checked = gen.check ();

    if (checked != generate_error::valid)
        switch (checked) {
            case generate_error::words_vs_mnemonic_style:
                return error_response (400, method::GENERATE, problem::invalid_parameter,
                    "If derivation_style is BIP_44, then coin_type must be povided and not be 'none'");
            case generate_error::centbee_vs_coin_type:
                return error_response (400, method::GENERATE, problem::invalid_parameter,
                    "If derivation_style is CentBee, then coin_type, if provided, must be 'none'");
            case generate_error::neither_style_nor_coin_type:
                return error_response (400, method::GENERATE, problem::invalid_parameter,
                    "Either derivation_style or coin_type must be provided");
            case generate_error::mnemonic_vs_number_of_words:
                return error_response (400, method::GENERATE, problem::invalid_parameter,
                    "if mnemonic is none, then number_of_words must not be present");
            case generate_error::invalid_number_of_words:
                return error_response (400, method::GENERATE, problem::invalid_parameter,
                    string::write ("'number_of_words' should be either 12 or 24 and instead is ", gen.NumberOfWords));
            case generate_error::zero_accounts:
                return error_response (400, method::GENERATE, problem::invalid_parameter,
                    "cannot have zero accounts");
            default:
                return error_response (500, method::GENERATE, problem::failed);
        }


    // we generate the wallet the same way regardless of whether the user wants words.
    bytes wallet_entropy {};
    wallet_entropy.resize (gen.NumberOfWords * 4 / 3);

    // generate master key.
    data::crypto::random::get () >> wallet_entropy;
    std::string wallet_words = HD::BIP_39::generate (wallet_entropy);

    generate_error result = generate (p.DB, wallet_words, gen);

    if (result != generate_error::valid)
        switch (result) {
            case generate_error::wallet_already_exists:
                return error_response (500, method::GENERATE, problem::failed,
                    string::write ("wallet ", gen.Name, " already exists"));
            default:
                return error_response (500, method::GENERATE, problem::failed);
        }

    if (gen.MnemonicStyle != mnemonic_style::none)
        return string_response (wallet_words);

    return ok_response ();

}
