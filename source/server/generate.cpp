
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

std::string write_derivation (list<uint32>) {
    throw data::method::unimplemented {"write_derivation"};
}

net::HTTP::response handle_generate (server &p,
    Diophant::symbol wallet_name, map<UTF8, UTF8> query,
    const maybe<net::HTTP::content> &content_type,
    const data::bytes &body) {

    auto validated = data::schema::validate<> (query,
        data::schema::key<wallet_style> ("wallet_style") &
        data::schema::key<std::string> ("coin_type") &
        *data::schema::key<uint32> ("number_of_words") &
        *data::schema::key<mnemonic_style> ("mnemonic_style") &
        *data::schema::key<uint32> ("accounts"));

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

    uint32 total_accounts = 1;
    auto acc = std::get<4> (validated);
    if (bool (acc)) total_accounts = acc->Value;

    // we generate the wallet the same way regardless of whether the user wants words.
    bytes wallet_entropy {};
    wallet_entropy.resize (number_of_words * 4 / 3);

    // generate master key.
    p.get_secure_random () >> wallet_entropy;
    std::string wallet_words = HD::BIP_39::generate (wallet_entropy);

    auto master = HD::BIP_32::secret::from_seed (HD::BIP_39::read (wallet_words));

    key_expression master_key_expr {master};

    p.DB->set_key (wallet_name, Diophant::symbol {"master"}, master_key_expr);

    // if coin type is none, then we make a centbee-style account.
    list<uint32> root_derivation = bool (coin_type) ?
        list<uint32> {HD::BIP_44::purpose, *coin_type}:
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

    if (MnemonicStyle != mnemonic_style::none) return string_response (wallet_words);
    return ok_response ();

}
