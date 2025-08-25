#ifndef SERVER_GENERATE
#define SERVER_GENERATE

#include <data/maybe.hpp>
#include <data/bytes.hpp>
#include <data/net/HTTP.hpp>
#include <gigamonkey/schema/bip_44.hpp>
#include "server.hpp"

enum class mnemonic_style {
    none,
    BIP_39,
    Electrum_SV
};

enum class wallet_style {
    BIP_44,
    // same as bip 44 except that we have a new derivation path for
    // extended pubkeys for payments.
    BIP_44_plus,
    // same as bip 44 plus except that the master chain code is generated
    // from another key that we can use for other purposes.
    experimental
};

struct generate_request_options {
    std::string Name;
    ::wallet_style WalletStyle {::wallet_style::BIP_44};
    maybe<uint32> CoinTypeDerivationParameter {HD::BIP_44::coin_type_Bitcoin};
    ::mnemonic_style MnemonicStyle {mnemonic_style::none};
    uint32 NumberOfWords {12};

    generate_request_options (const std::string &name): Name {name} {}

    generate_request_options &wallet_style (::wallet_style);
    generate_request_options &mnemonic_style (::mnemonic_style);
    generate_request_options &number_of_words (uint32);
    generate_request_options &coin_type (uint32);
    generate_request_options &coin_type_none ();

    // make a generate request
    operator net::HTTP::request () const;
};

std::ostream &operator << (std::ostream &, wallet_style);
std::ostream &operator << (std::ostream &, mnemonic_style);

net::HTTP::response handle_generate (server &p,
    Diophant::symbol wallet_name, map<UTF8, UTF8> query,
    const maybe<net::HTTP::content> &content_type,
    const data::bytes &body);

generate_request_options inline &generate_request_options::wallet_style (::wallet_style wx) {
    WalletStyle = wx;
    return *this;
}

generate_request_options inline &generate_request_options::mnemonic_style (::mnemonic_style mnx) {
    MnemonicStyle = mnx;
    return *this;
}

generate_request_options inline &generate_request_options::number_of_words (uint32 n) {
    NumberOfWords = n;
    return *this;
}

generate_request_options inline &generate_request_options::coin_type (uint32 u) {
    CoinTypeDerivationParameter = u;
    return *this;
}

generate_request_options inline &generate_request_options::coin_type_none () {
    CoinTypeDerivationParameter = {};
    return *this;
}

#endif

