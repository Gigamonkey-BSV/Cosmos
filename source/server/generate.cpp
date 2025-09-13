
#include "method.hpp"
#include "generate.hpp"

#include <gigamonkey/schema/bip_39.hpp>
#include <data/tools/map_schema.hpp>

generate_request_options::operator net::HTTP::request () const {
    std::stringstream query_stream;
    query_stream << "?wallet_style=" << WalletStyle;
    if (!bool (CoinTypeDerivationParameter)) query_stream << "&coin_type=none";
    else query_stream << "&coin_type=" << *CoinTypeDerivationParameter;
    if (MnemonicStyle != mnemonic_style::none)
        query_stream << "&mnemonic_style=" << MnemonicStyle << "&number_of_words=" << NumberOfWords;
    return net::HTTP::request::make {}.path (
        string::write ("/generate/", Name)).query (query_stream.str ()).host ("localhost").method (net::HTTP::method::post);
}

std::ostream &operator << (std::ostream &o, wallet_style x) {
    switch (x) {
        case wallet_style::BIP_44: return o << "BIP 44";
        case wallet_style::BIP_44_plus: return o << "BIP 44 plus";
        case wallet_style::experimental: return o << "experimental";
        default: throw 0;
    }
}

std::ostream &operator << (std::ostream &o, mnemonic_style x) {
    switch (x) {
        case mnemonic_style::none: return o << "none";
        case mnemonic_style::BIP_39: return o << "BIP 39";
        case mnemonic_style::Electrum_SV: return o << "Electrum SV";
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

net::HTTP::response handle_generate (server &p,
    Diophant::symbol wallet_name, map<UTF8, UTF8> query,
    const maybe<net::HTTP::content> &content_type,
    const data::bytes &body) {

    auto validated = data::schema::validate<> (query,
        data::schema::key<wallet_style> ("wallet_style") &
        data::schema::key<std::string> ("coin_type") &
        *data::schema::key<uint32> ("number_of_words") &
        *data::schema::key<mnemonic_style> ("mnemonic_style"));

    // parameter wallet_style, required
    wallet_style WalletStyle = std::get<0> (validated).Value;

    // coin_type is required and it should be either a uint32 or "none".
    maybe<uint32> coin_type;

    {
        std::string coin_type_param = std::get<1> (validated).Value;
        if (coin_type_param != "none" && !parse_uint32 (coin_type_param, *coin_type))
            return error_response (400, method::GENERATE, problem::invalid_parameter, "'coin_type'");
    }

    mnemonic_style MnemonicStyle {mnemonic_style::none}; // parameter mnemonic_style
    uint32 number_of_words = 24;                         // parameter number_of_words

    auto numw = std::get<2> (validated);
    auto mn = std::get<3> (validated);

    if (bool (numw) || bool (mn)) {
        if (!(bool (numw) && bool (mn)))
            return error_response (400, method::GENERATE, problem::invalid_query,
                "if number_of_words or mnemonic_style is present, the other must also be");

        number_of_words = numw->Value;
        MnemonicStyle = mn->Value;
    }

    if (number_of_words != 12 && number_of_words != 24)
        return error_response (400, method::GENERATE, problem::invalid_parameter,
            "'number_of_words' should be either 12 or 24");

    if (MnemonicStyle == mnemonic_style::Electrum_SV || WalletStyle == wallet_style::experimental)
        return error_response (501, method::GENERATE, problem::unimplemented);

    // we generate the wallet the same way regardless of whether the user wants words.
    bytes wallet_entropy {};
    wallet_entropy.resize (number_of_words * 4 / 3);

    p.get_secure_random () >> wallet_entropy;
    std::string wallet_words = HD::BIP_39::generate (wallet_entropy);

    auto master = HD::BIP_32::secret::from_seed (HD::BIP_39::read (wallet_words));

    key_expression master_key_expr {master};
    std::string master_key_name = "Master";

    HD::BIP_32::pubkey account;
    string account_derivation_string;

    if (bool (coin_type)) {
        account = HD::BIP_44::from_root (master, *coin_type, 0).to_public ();
        account_derivation_string = string::write ("(", master_key_expr, ") / `44 / `", std::to_string (*coin_type), " / `0");
    } else {
        account = HD::BIP_44::from_root (master, *coin_type, 0).to_public ();
        account_derivation_string = string::write ("(", master_key_expr, ") / `44 / `0");
    }

    key_expression account_key_expr {account};
    key_expression account_derivation {account_derivation_string};

    std::string account_name = "Account0";

    p.DB->set_key (wallet_name, master_key_name, master_key_expr);
    p.DB->set_key (wallet_name, account_name, account_key_expr);
    p.DB->set_to_private (account_name, account_derivation);

    p.DB->set_wallet_derivation (wallet_name, "receive",
        database::derivation {key_derivation {"@ name index -> name / 0 / index"}, account_name});

    p.DB->set_wallet_derivation (wallet_name, "change",
        database::derivation {key_derivation {"@ name index -> name / 1 / index"}, account_name});

    if (WalletStyle == wallet_style::BIP_44_plus)
        p.DB->set_wallet_derivation (wallet_name, "receivex",
            database::derivation {key_derivation {"@ name index -> name / 0 / `index"}, account_name});

    if (MnemonicStyle != mnemonic_style::none) return string_response (wallet_words);
    return ok_response ();

}
