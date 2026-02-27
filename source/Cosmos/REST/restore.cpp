
#include <Cosmos/REST/REST.hpp>
#include <data/tools/schema.hpp>

#include <gigamonkey/wif.hpp>
#include <gigamonkey/schema/electrum_sv.hpp>
#include <gigamonkey/schema/bip_39.hpp>

namespace schema = data::schema;

namespace Cosmos {

    std::ostream &operator << (std::ostream &, master_key_type) {
        throw data::method::unimplemented {" << master_key_type"};
    }

    std::istream &operator >> (std::istream &i, master_key_type &x) {
        std::string word;
        i >> word;
        if (!i) return i;
        std::string sanitized = command::sanitize (word);
        if (sanitized == "single_address") x = master_key_type::single_address;
        else if (sanitized == "hdsequence") x = master_key_type::HD_sequence;
        else if (sanitized == "bip44account") x = master_key_type::BIP44_account;
        else if (sanitized == "bip44master") x = master_key_type::BIP44_master;
        else i.setstate (std::ios::failbit);
        return i;
    }

    restore_request_options::restore_request_options (const args::parsed &p) {
        std::cout << "validate restore options " << p.Options << std::endl;
        // we must have either a key and optional key type, entropy, or words.
        auto [flags, name, opts] = args::validate (p, args::command {
            set<std::string> {}, schema::list::value<Diophant::symbol> (), schema () && command::call_options ()});

        this->Name = name;

        auto [
            derivation_options,
            key_options,
            accounts_param,
            max_lookup_param, _] = opts;

        switch (derivation_options.index ()) {
            case 0: {
                this->RestoreWalletStyle = std::get<0> (derivation_options);
            } break;
            case 1: {
                auto [wallet_type_option, derivation_style_option, coin_type_options] = std::get<1> (derivation_options);

                if (wallet_type_option)
                    this->WalletType = *wallet_type_option;

                if (derivation_style_option)
                    this->DerivationStyle = *derivation_style_option;

                if (coin_type_options) {
                    // coin type was provided
                    if (coin_type_options->index () == 0)
                        this->CoinTypeDerivationParameter = std::get<0> (*coin_type_options);
                    else this->GuessCoinType = std::get<1> (*coin_type_options);
                }
            } break;
        }

        switch (key_options.index ()) {
            case 0: {
                auto [master_key, master_key_type] = std::get<0> (key_options);
                this->Key = master_key;
                if (master_key_type) this->MasterKeyType = *master_key_type;
            } break;
            case 1: {
                auto [mnemonic_or_entropy, password_option, mnemonic_style_option] = std::get<1> (key_options);
                switch (mnemonic_or_entropy.index ()) {
                    case 0: {
                        this->Mnemonic = std::get<0> (mnemonic_or_entropy);
                    } break;
                    case 1: {
                        this->Entropy = std::get<1> (mnemonic_or_entropy);
                    } break;
                }

                if (password_option) {
                    switch (password_option->index ()) {
                        case 0: this->Password = std::get<0> (*password_option);
                        case 1: this->CentBeePIN = std::get<1> (*password_option);
                        case 2: this->GuessCentBeePIN = std::get<2> (*password_option);
                    }

                }

                if (mnemonic_style_option) this->MnemonicStyle;
            } break;
        }

        if (accounts_param) this->Accounts = *accounts_param;

        if (max_lookup_param) this->MaxLookAhead = *max_lookup_param;


    }
}
