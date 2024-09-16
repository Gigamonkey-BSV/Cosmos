#include <gigamonkey/schema/bip_44.hpp>
#include <gigamonkey/schema/bip_39.hpp>

#include "interface.hpp"
#include "Cosmos.hpp"

namespace Cosmos {

    struct generate_wallet {
        bool UseBIP_39 {true};
        uint32 Accounts {10};
        uint32 CoinType {0};
        generate_wallet () {}
        generate_wallet (bool b39, uint32 accounts = 10, uint32 coin_type = 0) :
            UseBIP_39 {b39}, Accounts {accounts}, CoinType {coin_type} {}

        void operator () (Interface::writable u);
    };

    void generate_wallet::operator () (Interface::writable u) {
        const auto w = u.get ().wallet ();
        if (!bool (w)) throw exception {} << "could not read wallet.";

        if (UseBIP_39) std::cout << "We will generate a new BIP 39 wallet for you." << std::endl;
        else std::cout << "We will generate a new BIP 44 wallet for you." << std::endl;
        std::cout << "We will pre-generate " << Accounts << " accounts in this wallet." << std::endl;
        std::cout << "Coin type is " << CoinType << std::endl;
        std::cout << "Type random characters to use as entropy for generating this wallet. "
            "Press enter when you think you have enough."
            "Around 200 characters ought to be enough as long as they are random enough." << std::endl;

        std::string user_input {};

        while (true) {
            char x = std::cin.get ();
            if (x == '\n') break;
            user_input.push_back (x);
        }

        HD::BIP_32::secret master {};

        if (UseBIP_39) {
            digest256 bits = crypto::SHA2_256 (user_input);

            std::string words = HD::BIP_39::generate (bits);

            std::cout << "your words are\n\n\t" << words << "\n\nRemember, these words can be used to generate "
                "all your keys, but at scale that is not enough to restore your funds. You need to keep the transactions"
                " into your wallet along with Merkle proofs as well." << std::endl;

            // TODO use passphrase option.
            master = HD::BIP_32::secret::from_seed (HD::BIP_39::read (words));
        } else {
            digest512 bits = crypto::SHA2_512 (user_input);
            master.ChainCode.resize (32);
            std::copy (bits.begin (), bits.begin () + 32, master.Secret.Value.begin ());
            std::copy (bits.begin () + 32, bits.end (), master.ChainCode.begin ());
        }

        HD::BIP_32::pubkey master_pubkey = master.to_public ();
        keychain keys = keychain {}.insert (pubkey {master_pubkey}, master);

        pubkeys pub {};
        addresses addrs {{}, "receive_0", "change_0"};

        for (int account = 0; account < Accounts; account++) {
            list<uint32> path {
                HD::BIP_44::purpose,
                HD::BIP_32::harden (CoinType),
                HD::BIP_32::harden (account)};

            HD::BIP_32::pubkey account_master_pubkey = master.derive (path).to_public ();

            std::cout << "\tmaster pubkey for account " << account << " is " << account_master_pubkey << std::endl;

            pub = pub.insert (account_master_pubkey, derivation {master_pubkey, path});

            std::string receive_name = std::string {"receive_"} + std::to_string (account);
            std::string change_name = std::string {"change_"} + std::to_string (account);

            addrs.Sequences = addrs.Sequences.insert (receive_name, address_sequence {account_master_pubkey, {HD::BIP_44::receive_index}});
            addrs.Sequences = addrs.Sequences.insert (change_name, address_sequence {account_master_pubkey, {HD::BIP_44::change_index}});
        }

        u.set_keys (keys);
        u.set_pubkeys (pub);
        u.set_addresses (addrs);
    }
}

// TODO encrypt file
void command_generate (const arg_parser &p) {
    using namespace Cosmos;
    Cosmos::Interface e {};
    Cosmos::read_both_chains_options (e, p);

    Cosmos::generate_wallet gen {};

    if (p.has ("words")) gen.UseBIP_39 = true;
    if (p.has ("no_words")) gen.UseBIP_39 = false;

    maybe<uint32> accounts;
    p.get ("accounts", accounts);
    if (bool (accounts)) gen.Accounts = *accounts;

    maybe<uint32> coin_type;
    p.get ("coin_type", coin_type);
    if (bool (coin_type)) gen.CoinType = *coin_type;

    e.update<void> (gen);

    std::cout << "private keys will be saved in " << *e.keychain_filepath () << "." << std::endl;
    std::cout << "public keys will be saved in " << *e.pubkeys_filepath () << "." << std::endl;
}
