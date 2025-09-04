
#include "method.hpp"
#include "generate.hpp"

#include <gigamonkey/schema/bip_39.hpp>

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

net::HTTP::response handle_generate (server &p,
    Diophant::symbol wallet_name, map<UTF8, UTF8> query,
    const maybe<net::HTTP::content> &content_type,
    const data::bytes &body) {

    // first we will determine whether we will return a mnemonic
    mnemonic_style MnemonicStyle {mnemonic_style::none};

    const UTF8 *mnemonic_style_param = query.contains ("mnemonic_style");
    if (bool (mnemonic_style_param)) {
        std::string sanitized = sanitize (*mnemonic_style_param);
        if (sanitized == "electrumsv") MnemonicStyle = mnemonic_style::Electrum_SV;
        else if (sanitized == "bip39") MnemonicStyle = mnemonic_style::BIP_39;
        else return error_response (400, method::GENERATE, problem::invalid_query, "invalid parameter 'mnemonic_style'");
    }

    uint32 number_of_words = 24;
    const UTF8 *number_of_words_param = query.contains ("number_of_words");
    if (bool (number_of_words_param)) {
        if (!parse_uint32 (*number_of_words_param, number_of_words))
            return error_response (400, method::GENERATE, problem::invalid_parameter, "'number_of_words' should be either 12 or 24");
    }

    if (number_of_words != 12 && number_of_words != 24)
        return error_response (400, method::GENERATE, problem::invalid_parameter, "'number_of_words' should be either 12 or 24");

    wallet_style WalletStyle {wallet_style::BIP_44};

    const UTF8 *wallet_style_param = query.contains ("wallet_style");
    if (bool (wallet_style_param)) {
        std::string sanitized = sanitize (*wallet_style_param);
        if (sanitized == "bip44") WalletStyle = wallet_style::BIP_44;
        else if (sanitized == "bip44plus") WalletStyle = wallet_style::BIP_44_plus;
        // experimental means me generate two secp256k1 keys and use one as the chain code.
        // this allows us to use bip 32 and hd stuff and future protocols.
        else if (sanitized == "experimental") WalletStyle = wallet_style::experimental;
        else return error_response (400, method::GENERATE, problem::invalid_query, "'wallet_style'");
    }

    if (MnemonicStyle == mnemonic_style::Electrum_SV || WalletStyle == wallet_style::experimental)
        return error_response (501, method::GENERATE, problem::unimplemented);;

    // coin_type is required and it should be either a uint32 or "none".
    maybe<uint32> coin_type;
    const UTF8 *coin_type_param = query.contains ("coin_type");
    if (bool (coin_type_param)) {
        if (*coin_type_param != "none" && !parse_uint32 (*coin_type_param, *coin_type))
            return error_response (400, method::GENERATE, problem::invalid_parameter, "'coin_type'");
    } else coin_type = 0;

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
