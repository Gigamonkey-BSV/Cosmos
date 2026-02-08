#ifndef SERVER_GENERATE
#define SERVER_GENERATE

#include <data/maybe.hpp>
#include <data/bytes.hpp>
#include <gigamonkey/schema/bip_44.hpp>
#include <data/io/arg_parser.hpp>
#include <Cosmos/Diophant.hpp>

namespace HD = Gigamonkey::HD;
namespace args = data::io::args;

namespace Cosmos {

    enum class mnemonic_style {
        none,
        BIP_39,
        Electrum_SV
    };

    enum class wallet_type {
        invalid,
        address,
        HD_sequence,
        BIP_44,
        // same as bip 44 except that we have a new derivation path for
        // extended pubkeys for payments.
        BIP_44_plus,
        // same as bip 44 plus except that the master chain code is generated
        // from another key that we can use for other purposes.
        experimental
    };

    // BIP 44 uses a coin type parameter in the derivation path and CentBee does not.
    enum class derivation_style {
        unset,
        BIP_44,
        CentBee
    };

    enum class restore_wallet_style {
        unset,
        Money_Button,
        RelayX,
        Simply_Cash,
        Electrum_SV,

        // centbee wallets do not use a standard bip44 derivation path and
        // is the only wallet type to use a password, which is called the PIN.
        CentBee
    };

    struct coin_type : maybe<uint32> {
        enum value {
            none,
            Bitcoin,
            Bitcoin_Cash,
            Bitcoin_SV,
        };

        coin_type (uint32 u): maybe<uint32> {HD::BIP_32::harden (u)} {}
        coin_type (): maybe<uint32> {} {}
        coin_type (value);

        bool operator == (const coin_type &ct) const {
            return static_cast<const maybe<uint32> &> (*this) == static_cast<const maybe<uint32> &> (ct);
        }
    };

    std::ostream &operator << (std::ostream &, wallet_type);
    std::istream &operator >> (std::istream &, wallet_type &);

    std::ostream &operator << (std::ostream &, mnemonic_style);
    std::istream &operator >> (std::istream &, mnemonic_style &);

    std::ostream &operator << (std::ostream &, derivation_style);
    std::istream &operator >> (std::istream &, derivation_style &);

    std::ostream &operator << (std::ostream &, restore_wallet_style);
    std::istream &operator >> (std::istream &, restore_wallet_style &);

    std::ostream &operator << (std::ostream &, coin_type);
    std::istream &operator >> (std::istream &, coin_type &);

    std::string write_derivation (list<uint32> d);

    enum class generate_error {
        valid,
        wallet_already_exists,
        words_vs_mnemonic_style,
        centbee_vs_coin_type,
        neither_style_nor_coin_type,
        mnemonic_vs_number_of_words,
        invalid_number_of_words,
        zero_accounts
    };

    // set of options provided in a http request that are
    // used to generate a new wallet.
    struct generate_request_options {
        Diophant::symbol Name {};

        constexpr static const Cosmos::wallet_type DefaultWalletType {Cosmos::wallet_type::BIP_44};

        maybe<Cosmos::wallet_type> WalletType {};

        Cosmos::restore_wallet_style RestoreWalletStyle {Cosmos::restore_wallet_style::unset};

        maybe<Cosmos::coin_type> CoinTypeDerivationParameter {};
        Cosmos::derivation_style DerivationStyle {Cosmos::derivation_style::unset};

        constexpr static const Cosmos::mnemonic_style DefaultMnemonicStyle {Cosmos::mnemonic_style::none};

        maybe<Cosmos::mnemonic_style> MnemonicStyle {};
        uint32 NumberOfWords {0};
        maybe<std::string> Password;
        uint32 Accounts {1};

        generate_request_options &name (const std::string &);
        generate_request_options &wallet_type (Cosmos::wallet_type);
        generate_request_options &mnemonic_style (Cosmos::mnemonic_style);
        generate_request_options &number_of_words (uint32);
        generate_request_options &coin_type (uint32);
        generate_request_options &coin_type_none ();
        generate_request_options &restore_wallet_style (Cosmos::restore_wallet_style);

        Diophant::symbol name () const {
            return Name;
        }

        uint32 accounts () const {
            return Accounts;
        }

        uint32 number_of_words () const {
            return NumberOfWords;
        }

        Cosmos::coin_type coin_type () const {
            if (CoinTypeDerivationParameter) return *CoinTypeDerivationParameter;
            if (DerivationStyle == Cosmos::derivation_style::CentBee) return Cosmos::coin_type {};
            // get coin type from wallet style.
            throw data::exception {"invalid parameters"};
        }

        Cosmos::mnemonic_style mnemonic_style () const {
            if (MnemonicStyle) return *MnemonicStyle;
            else return DefaultMnemonicStyle;
        }

        Cosmos::wallet_type wallet_type () const {
            if ((!WalletType || *WalletType == Cosmos::wallet_type::invalid) &&
                RestoreWalletStyle != Cosmos::restore_wallet_style::unset)
                return DefaultWalletType;
            return *WalletType;
        }

        // read from a generate request.
        generate_request_options (
            Diophant::symbol wallet_name, map<UTF8, UTF8> query,
            const maybe<net::HTTP::content> &content_type,
            const data::bytes &body);

        generate_request_options (const args::parsed &p);

        generate_error check () const;

        generate_request_options () {}
    };

    // return the mnemonic if the generate function succeeded
    generate_error generate (database &DB, const std::string &words, const generate_request_options &);

    net::HTTP::response handle_generate (database &DB,
        Diophant::symbol wallet_name, map<UTF8, UTF8> query,
        const maybe<net::HTTP::content> &content_type,
        const data::bytes &body);

    generate_request_options inline &generate_request_options::name (const std::string &name) {
        Name = name;
        return *this;
    }

    generate_request_options inline &generate_request_options::wallet_type (Cosmos::wallet_type wx) {
        WalletType = wx;
        return *this;
    }

    generate_request_options inline &generate_request_options::mnemonic_style (Cosmos::mnemonic_style mnx) {
        MnemonicStyle = mnx;
        return *this;
    }

    generate_request_options inline &generate_request_options::number_of_words (uint32 n) {
        NumberOfWords = n;
        return *this;
    }

    generate_request_options inline &generate_request_options::coin_type (uint32 u) {
        CoinTypeDerivationParameter = u;
        DerivationStyle = Cosmos::derivation_style::BIP_44;
        return *this;
    }

    generate_request_options inline &generate_request_options::coin_type_none () {
        CoinTypeDerivationParameter = {Cosmos::coin_type {}};
        DerivationStyle = Cosmos::derivation_style::CentBee;
        return *this;
    }
}

#endif

