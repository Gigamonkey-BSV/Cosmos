
#include "method.hpp"
#include "generate.hpp"

#include <gigamonkey/schema/bip_39.hpp>
#include <data/tools/map_schema.hpp>

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

std::ostream &operator << (std::ostream &o, mnemonic_style x) {
    switch (x) {
        case mnemonic_style::none: return o << "none";
        case mnemonic_style::BIP_39: return o << "BIP39";
        case mnemonic_style::Electrum_SV: return o << "ElectrumSV";
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

std::string write_derivation (list<uint32> d) {
    std::stringstream ss;
    for (const uint32 &u : d) {
        ss << " / ";
        if (HD::BIP_32::hardened (u)) ss << "`" << HD::BIP_32::soften (u);
        else ss << u;
    }
    return ss.str ();
}

net::HTTP::response handle_generate (server &p,
    Diophant::symbol wallet_name, map<UTF8, UTF8> query,
    const maybe<net::HTTP::content> &content_type,
    const data::bytes &body) {

    // either we have a use an existing wallet style
    // that has options built in or we specify certain
    // things about the derivation path. In particular,
    // there are differences of opinion in what should
    // be used as the coin_type parameter in a BIP44
    // derivation path.
    auto derivation_path_schema = schema::key<restore_wallet_type> ("format") |
        (schema::key<wallet_style> ("style") &
        *schema::key<derivation_style> ("derivation_style") &
        *schema::key<coin_type> ("coin_type"));

    // we may not use a mnemonic at all but if we do
    // there are some options that apply. In particular
    // ElectrumSV uses a special set of words.
    auto mnemonic_schema = schema::key<mnemonic_style> ("mnemonic") &
        *schema::key<uint32> ("number_of_words") &
        *schema::key<std::string> ("password");

    auto generate_schema = *mnemonic_schema & *schema::key<uint32> ("accounts") & derivation_path_schema;

    auto [mnemonic_options, accounts_param, wallet_generation] =
        schema::validate<> (query, generate_schema);

    wallet_style WalletStyle;
    coin_type CoinType;

    switch (wallet_generation.index ()) {
        case 0: {
            WalletStyle = wallet_style::BIP_44;
            restore_wallet_type WalletType = std::get<0> (wallet_generation);
            switch (WalletType) {
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
            };
        } break;
        case 1: {
            maybe<derivation_style> dstyle;
            maybe<coin_type> ct;
            std::tie (WalletStyle, dstyle, ct) = std::get<1> (wallet_generation);

            if (bool (dstyle)) {
                if (*dstyle == derivation_style::BIP_44) {
                    if (!bool (ct) || !bool (*ct))
                        return error_response (400, method::GENERATE, problem::invalid_parameter,
                            "If derivation_style is BIP_44, then coin_type must be povided and not be 'none'");
                } else if (bool (ct) && bool (*ct))
                    return error_response (400, method::GENERATE, problem::invalid_parameter,
                        "If derivation_style is CentBee, then coin_type, if provided, must be 'none'");
            } else if (!bool (ct))
                return error_response (400, method::GENERATE, problem::invalid_parameter,
                    "Either derivation_style or coin_type must be provided");

            if (bool (ct)) CoinType = *ct;
        };
    }

    mnemonic_style MnemonicStyle {mnemonic_style::none}; // parameter mnemonic_style
    uint32 number_of_words {24};                              // parameter number_of_words
    maybe<std::string> password;

    if (mnemonic_options) {
        maybe<uint32> num_words;
        std::tie (MnemonicStyle, num_words, password) = *mnemonic_options;

        if (MnemonicStyle == mnemonic_style::none && bool (num_words))
            return error_response (400, method::GENERATE, problem::invalid_parameter,
                "if mnemonic is none, then number_of_words must not be present");

        if (bool (num_words)) number_of_words = *num_words;
    }

    if (number_of_words != 12 && number_of_words != 24)
        return error_response (400, method::GENERATE, problem::invalid_parameter,
            string::write ("'number_of_words' should be either 12 or 24 and instead is ", number_of_words));

    if (MnemonicStyle == mnemonic_style::Electrum_SV || WalletStyle == wallet_style::experimental || bool (password))
        return error_response (501, method::GENERATE, problem::unimplemented);

    uint32 total_accounts = bool (accounts_param) ? *accounts_param : 1;

    // we generate the wallet the same way regardless of whether the user wants words.
    bytes wallet_entropy {};
    wallet_entropy.resize (number_of_words * 4 / 3);

    // generate master key.
    p.get_secure_random () >> wallet_entropy;
    std::string wallet_words = HD::BIP_39::generate (wallet_entropy);

    auto master = HD::BIP_32::secret::from_seed (HD::BIP_39::read (wallet_words));

    key_expression master_key_expr {master};

    if (!p.DB->make_wallet (wallet_name))
        return error_response (500, method::GENERATE, problem::failed,
            string::write ("wallet ", wallet_name, " already exists"));

    p.DB->set_key (wallet_name, Diophant::symbol {"master"}, master_key_expr);

    // if coin type is none, then we make a centbee-style account.
    list<uint32> root_derivation = bool (CoinType) ?
        list<uint32> {HD::BIP_44::purpose, *CoinType}:
        list<uint32> {HD::BIP_44::purpose};

    string account_name {"account"};

    // set a sequence for the accounts.
    p.DB->set_wallet_sequence (wallet_name, account_name,
        key_sequence {
            key_expression {string::write ("(", master_key_expr, ") ", write_derivation (root_derivation))},
            key_derivation {string::write ("@ key index -> key / harden (index)")}},
        total_accounts);

    if (WalletStyle == wallet_style::BIP_44_plus) {
        Diophant::symbol receive_x_symbol {"receivex"};
        p.DB->set_wallet_sequence (wallet_name, receive_x_symbol,
            key_sequence {
                key_expression {string::write ("(", master_key_expr, ") ", write_derivation (root_derivation))},
                key_derivation {"@ key index -> key / `index"}}, 0);
    }

    // generate all the accounts.
    for (uint32 account_number = 0; account_number < total_accounts; account_number++) {

        list<uint32> account_derivation = root_derivation << HD::BIP_32::harden (account_number);

        list<uint32> receive_derivation = account_derivation << 0;
        list<uint32> change_derivation = account_derivation << 1;

        key_expression receive_pubkey = key_expression {master.derive (receive_derivation).to_public ()};
        key_expression change_pubkey = key_expression {master.derive (change_derivation).to_public ()};

        p.DB->set_to_private (receive_pubkey,
            key_expression {string::write ("(", master_key_expr, ") ",
                write_derivation (receive_derivation))});

        p.DB->set_to_private (change_pubkey,
            key_expression {string::write ("(", master_key_expr, ") ",
                write_derivation (change_derivation))});

        string receive_name = string::write ("receive_", account_number);
        string change_name = string::write ("change_", account_number);

        // note that each of these returns a regular pubkey rather than an xpub.
        p.DB->set_wallet_sequence (wallet_name, receive_name,
            key_sequence {
                receive_pubkey,
                key_derivation {"@ key index -> pubkey (key / index)"}}, 0);

        p.DB->set_wallet_sequence (wallet_name, change_name,
            key_sequence {
                change_pubkey,
                key_derivation {"@ key index -> pubkey (key / index)"}}, 0);

    }

    if (MnemonicStyle != mnemonic_style::none)
        return string_response (wallet_words);

    return ok_response ();

}
