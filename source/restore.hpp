#include "interface.hpp"
#include "Cosmos.hpp"

void command_restore (const arg_parser &p) {
    using namespace Cosmos;

    // somehow we need to get this key to be valid.
    HD::BIP_32::pubkey pk;

    // we may have a secret key provided.
    maybe<HD::BIP_32::secret> sk;

    // enables restoring from 12 words or from the entropy string.
    bool use_bip_39 = false;

    // if the user simply tells us what kind of wallet this is,
    // that makes things a lot easier.
    maybe<string> wallet_type_string;
    p.get ("wallet_type", wallet_type_string);

    enum class restore_wallet_type {
        unset,
        RelayX,
        Simply_Cash,
        Electrum_SV,

        // centbee wallets do not use a standard bip44 derivation path and
        // is the only wallet type to use a password, which is called the PIN.
        CentBee
    } wallet_type = bool (wallet_type_string) ?
        [] (const std::string &in) -> restore_wallet_type {
            std::string wallet_type_sanitized = sanitize (in);
            std::cout << "wallet type is " << wallet_type_sanitized << std::endl;
            if (wallet_type_sanitized == "relayx") return restore_wallet_type::RelayX;
            if (wallet_type_sanitized == "simplycash") return restore_wallet_type::Simply_Cash;
            if (wallet_type_sanitized == "electrumsv") return restore_wallet_type::Electrum_SV;
            if (wallet_type_sanitized == "centbee") return restore_wallet_type::CentBee;
            throw exception {} << "could not read wallet type";
        } (*wallet_type_string) : restore_wallet_type::unset;

    // let's check for a key.
    maybe<string> master_human;
    p.get (3, "key", master_human);
    if (bool (master_human)) {
        HD::BIP_32::secret maybe_secret {*master_human};
        if (maybe_secret.valid ()) {
            sk = maybe_secret;
            pk = sk->to_public ();
        } else pk = HD::BIP_32::pubkey {*master_human};
    } else {
        std::cout << "attempting to restore from words " << std::endl;
        use_bip_39 = true;
        // next we will try to derive these words.
        maybe<string> bip_39_words;

        // words can be derived from entropy.
        maybe<string> bip_39_entropy_hex;
        p.get ("entropy", bip_39_entropy_hex);
        if (bool (bip_39_entropy_hex)) {
            maybe<bytes> bip_39_entropy = encoding::hex::read (*bip_39_entropy_hex);
            if (! bool (bip_39_entropy)) throw exception {} << "could not read entropy";
            bip_39_words = HD::BIP_39::generate (*bip_39_entropy);
        } else {
            p.get ("words", bip_39_words);
            if (!bool (bip_39_words)) throw exception {} << "need to supply --words";
        }

        if (! HD::BIP_39::valid (*bip_39_words)) throw exception {} << "words are not valid";

        // there may be a password.
        maybe<string> bip_39_password;
        p.get ("password", bip_39_password);

        if (!bool (bip_39_password)) {
            if (wallet_type == restore_wallet_type::CentBee)
                std::cout << "NOTE: password is required with CentBee but was not provided." << std::endl;
            bip_39_password = "";
        } else if (wallet_type != restore_wallet_type::CentBee && wallet_type != restore_wallet_type::unset)
            std::cout << "NOTE: password was provided but is not used in the specified wallet type" << std::endl;

        auto seed = HD::BIP_39::read (*bip_39_words, *bip_39_password);

        // try to generate key.
        sk = HD::BIP_32::secret::from_seed (seed, HD::BIP_32::main);
        pk = sk->to_public ();

    }

    if (!pk.valid ()) throw exception {} << "could not read key";

    std::cout << "read key " << pk << std::endl;

    // in addition, the user may tell us what type of HD key we are using.
    maybe<string> key_type_string;
    p.get ("key_type", key_type_string);

    enum class master_key_type {
        // a single sequnce of keys. This could be from any wallet but
        // is not a complete wallet.
        HD_sequence,

        // In this case, there will be receive and change derivations.
        BIP44_account,

        // in this case, we also need a coin type or wallet type.
        BIP44_master,

        // CentBee uses a non-standard derivation path.
        CentBee_master
    } key_type = bool (key_type_string) ?
        [] (const std::string &in) -> master_key_type {
            std::string key_type_sanitized = sanitize (in);
            std::cout << "key type is " << key_type_sanitized << std::endl;
            if (key_type_sanitized == "hdsequence") return master_key_type::HD_sequence;
            if (key_type_sanitized == "bip44account") return master_key_type::BIP44_account;
            if (key_type_sanitized == "bip44master") return master_key_type::BIP44_master;
            throw exception {} << "could not read key type";
        } (*key_type_string) : use_bip_39 && bool (sk) ?
            (wallet_type == restore_wallet_type::CentBee ? master_key_type::CentBee_master : master_key_type::BIP44_master ):
            master_key_type::HD_sequence;

    if (wallet_type == restore_wallet_type::CentBee && key_type != master_key_type::CentBee_master)
        throw exception {} << "wallet type cent bee must go along with key type cent bee. ";

    // look for contradictions between key type and wallet type.
    if (wallet_type != restore_wallet_type::unset && key_type != master_key_type::BIP44_master)
        throw exception {} << "incompatible wallet type and key type ";

    if (!bool (sk) && key_type == master_key_type::BIP44_master)
        throw exception {} << "need private key for BIP44 master key.";

    // coin type is a parameter of the standard BIP 44 derivation path that we
    // may need to determine to recover coins.
    maybe<uint32> coin_type;

    {
        // try to read coin type as an option from the command line.
        maybe<uint32> coin_type_option;

        maybe<string> coin_type_string;
        p.get ("coin_type", coin_type_string);

        if (bool (coin_type_string)) coin_type_option = [] (const string &cx) -> uint32 {

            // try to read as a number.
            if (encoding::decimal::valid (cx)) {
                uint32 coin_type_number;
                std::stringstream ss {cx};
                ss >> coin_type_number;
                return coin_type_number;
            }

            string coin_type_sanitized = sanitize (cx);
            std::cout << "coin type is " << coin_type_sanitized << std::endl;
            if (coin_type_sanitized == "bitcoin") return HD::BIP_44::coin_type_Bitcoin;
            if (coin_type_sanitized == "bitcoincash") return HD::BIP_44::coin_type_Bitcoin_Cash;
            if (coin_type_sanitized == "bitcoinsv") return HD::BIP_44::coin_type_Bitcoin_SV;

            throw exception {} << "could not read coin type";
        } (*coin_type_string);

        // try to infer coin type from the other information we have.
        maybe<uint32> coin_type_wallet;
        if (wallet_type != restore_wallet_type::unset) coin_type_wallet = [] (restore_wallet_type w) -> uint32 {
            switch (w) {
                case (restore_wallet_type::RelayX) : return HD::BIP_44::relay_x_coin_type;
                case (restore_wallet_type::Simply_Cash) : return HD::BIP_44::simply_cash_coin_type;
                case (restore_wallet_type::Electrum_SV) : return HD::BIP_44::simply_cash_coin_type;
                case (restore_wallet_type::CentBee) : return HD::BIP_44::centbee_coin_type;
                default: return HD::BIP_44::coin_type_Bitcoin;
            }
        } (wallet_type);

        // coin types as derived in these different ways must match.
        if (bool (coin_type_option)) {
            if (bool (coin_type_wallet) && *coin_type_option != *coin_type_wallet)
                throw exception {} << "coin type as determined from wallet type does not match that provided by user." <<
                    "Using user provided coin type.";
            coin_type = coin_type_option;
        } else if (bool (coin_type_wallet)) coin_type = coin_type_wallet;

    }

    enum class derivation_style {
        BIP_44,
        CentBee
    };

    maybe<derivation_style> style;

    if (wallet_type == restore_wallet_type::CentBee) style = derivation_style::CentBee;
    else if (wallet_type != restore_wallet_type::unset || key_type == master_key_type::BIP44_master) style = derivation_style::BIP_44;

    // right now we only use 0. However, this should be a command line option later on.
    maybe<uint32> bip_44_account;
    if (wallet_type != restore_wallet_type::unset || key_type == master_key_type::BIP44_master) bip_44_account = 0;

    // at this point we have enough informaiton to generate a key and derivation paths to generate addresses
    // to check.

    // the list of derivations we will look at to restore the wallet.
    wallet w;
    // TODO: warn if a wallet already exists that will be overwritten.

    if (key_type == master_key_type::HD_sequence) {
        std::cout << "\tfirst address: " << pk.derive (HD::BIP_32::path {0}).address () << std::endl;

        w = wallet {pubkeys {{pk, {pk, {}}}}, addresses {{{"receive", {pk, {}}}}, "receive", "receive"}, account {}};
    } else if (key_type == master_key_type::BIP44_account) {
        std::cout
            << "this is a bip 44 account"
            << "\n\tfirst receive address: " << pk.derive (HD::BIP_32::path {0, 0}).address ()
            << "\n\tfirst change address: " << pk.derive (HD::BIP_32::path {1, 0}).address () << std::endl;

        w = wallet {pubkeys {{pk, {pk, {}}}}, addresses {{{"receive", {pk, {0}}}, {"change", {pk, {1}}}}, "receive", "change"}, account {}};
    } else if (key_type == master_key_type::BIP44_master) {
        maybe<uint32> coin_type_option;

        maybe<string> coin_type_string;
        p.get ("coin_type", coin_type_string);

        if (bool (coin_type_string)) coin_type_option = [] (const string &cx) -> uint32 {

            // try to read as a number.
            if (encoding::decimal::valid (cx)) {
                uint32 coin_type_number;
                std::stringstream ss {cx};
                ss >> coin_type_number;
                return coin_type_number;
            }

            string coin_type_sanitized = sanitize (cx);
            std::cout << "coin type is " << coin_type_sanitized << std::endl;
            if (coin_type_sanitized == "bitcoin") return HD::BIP_44::coin_type_Bitcoin;
            if (coin_type_sanitized == "bitcoincash") return HD::BIP_44::coin_type_Bitcoin_Cash;
            if (coin_type_sanitized == "bitcoinsv") return HD::BIP_44::coin_type_Bitcoin_SV;

            throw exception {} << "could not read coin type";
        } (*coin_type_string);

        maybe<uint32> coin_type_wallet;
        if (wallet_type != restore_wallet_type::unset) coin_type_wallet = [] (restore_wallet_type w) -> uint32 {
            switch (w) {
                case (restore_wallet_type::RelayX) : return HD::BIP_44::relay_x_coin_type;
                case (restore_wallet_type::Simply_Cash) : return HD::BIP_44::simply_cash_coin_type;
                case (restore_wallet_type::Electrum_SV) : return HD::BIP_44::simply_cash_coin_type;
                case (restore_wallet_type::CentBee) : return HD::BIP_44::centbee_coin_type;
                default: return HD::BIP_44::coin_type_Bitcoin;
            }
        } (wallet_type);

        // coin type as derived in these different ways must match.
        if (bool (coin_type_option)) {
            if (bool (coin_type_wallet) && *coin_type_option != *coin_type_wallet)
                std::cout << "NOTE: coin type as determined from wallet type does not match that provided by user." <<
                    "Using user provided coin type." << std::endl;
            coin_type = coin_type_option;
        } else if (bool (coin_type_wallet)) coin_type = coin_type_wallet;

        if (coin_type) {
            auto construct_wallet = []
            (const HD::BIP_32::secret &master, list<uint32> to_account_master, list<uint32> to_receive, list<uint32> to_change) -> wallet {
                HD::BIP_32::pubkey account_master = master.derive (to_account_master).to_public ();
                return wallet {pubkeys {{account_master, {master.to_public (), to_account_master}}},
                    addresses {{{"receive", {account_master, to_receive}}, {"change", {account_master, to_change}}}, "receive", "change"},
                    account {}};
            };

            w = key_type == master_key_type::CentBee_master ?
                construct_wallet (*sk, {HD::BIP_44::purpose}, {0, 0, 0}, {0, 0, 1}) :
                construct_wallet (*sk, {HD::BIP_44::purpose, *coin_type, HD::BIP_32::harden (0)}, {0}, {1});

            HD::BIP_32::pubkey first_receive = w.Addresses.last ("receive").derive ();
            HD::BIP_32::pubkey first_change = w.Addresses.last ("change").derive ();

            std::cout
                << "\nthis is a bip 44 master, coin type " << *coin_type
                << "\n\tfirst receive address: " << first_receive.address ()
                << "\n\tfirst change address: " << first_change.address () << std::endl;

        } else {
            HD::BIP_32::path bitcoin_to_account_master {HD::BIP_44::purpose, HD::BIP_44::coin_type_Bitcoin, HD::BIP_32::harden (0)};
            HD::BIP_32::path bitcoin_cash_to_account_master {HD::BIP_44::purpose, HD::BIP_44::coin_type_Bitcoin_Cash, HD::BIP_32::harden (0)};
            HD::BIP_32::path bitcoin_sv_to_account_master {HD::BIP_44::purpose, HD::BIP_44::coin_type_Bitcoin_SV, HD::BIP_32::harden (0)};
            HD::BIP_32::path centbee_to_account_master {HD::BIP_44::purpose};
            HD::BIP_32::pubkey bitcoin_account_master = sk->derive (bitcoin_to_account_master).to_public ();
            HD::BIP_32::pubkey bitcoin_cash_account_master = sk->derive (bitcoin_cash_to_account_master).to_public ();
            HD::BIP_32::pubkey bitcoin_sv_account_master = sk->derive (bitcoin_sv_to_account_master).to_public ();
            HD::BIP_32::pubkey centbee_account_master = sk->derive (centbee_to_account_master).to_public ();

            // there is a problem because the receive and change address sequences will be set incorrectly.
            // I'm not sure how to fix this right now.
            w = wallet {
                pubkeys {
                    {bitcoin_account_master, {pk, bitcoin_to_account_master}},
                    {bitcoin_cash_account_master, {pk, bitcoin_cash_to_account_master}},
                    {bitcoin_sv_account_master, {pk, bitcoin_sv_to_account_master}},
                    {centbee_account_master, {pk, centbee_to_account_master}}},
                addresses {{
                    {"receiveBitcoin", {bitcoin_account_master, {0}}},
                    {"changeBitcoin", {bitcoin_account_master, {1}}},
                    {"receiveBitcoinCash", {bitcoin_cash_account_master, {0}}},
                    {"changeBitcoinCash", {bitcoin_cash_account_master, {1}}},
                    {"receiveBitcoinSV", {bitcoin_sv_account_master, {0}}},
                    {"changeBitcoinSV", {bitcoin_sv_account_master, {1}}},
                    {"receiveCentBee", {centbee_account_master, {0}}},
                    {"changeCentBee", {centbee_account_master, {1}}}}, "receive", "change"},
                account {}};

            std::cout
                << "coin type could not be determined."
                << "\nassuming this is a bip 44 master, coin type Bitcoin "
                << "\n\tfirst receive address: "
                << w.Addresses.last ("receiveBitcoin").derive ().address ()
                << "\n\tfirst change address: "
                << w.Addresses.last ("changeBitcoin").derive ().address ()
                << "\nassuming this is a bip 44 master, coin type Bitcoin Cash"
                << "\n\tfirst receive address: "
                << w.Addresses.last ("receiveBitcoinCash").derive ().address ()
                << "\n\tfirst change address: "
                << w.Addresses.last ("changeBitcoin").derive ().address ()
                << "\nassuming this is a bip 44 master, coin type Bitcoin SV"
                << "\n\tfirst receive address: "
                << w.Addresses.last ("receiveBitcoinSV").derive ().address ()
                << "\n\tfirst change address: "
                << w.Addresses.last ("changeBitcoin").derive ().address ()
                << std::endl;

        }
    }

    maybe<uint32> max_look_ahead;
    p.get (4, "max_look_ahead", max_look_ahead);
    if (!bool (max_look_ahead)) max_look_ahead = options::DefaultMaxLookAhead;

    std::cout << "max look ahead is " << *max_look_ahead << std::endl;

    restore rr {*max_look_ahead};

    std::cout << "loading wallet" << std::endl;
    wait_for_enter ();

    Interface e {};

    auto restore_from_pubkey = [&max_look_ahead, &w] (Cosmos::Interface::writable u) {
        ordered_list<ray> history {};
        for (const auto &[name, sequence] : w.Addresses.Sequences) {
            std::cout << "checking address sequence " << name << std::endl;
            auto restored = restore {*max_look_ahead, false} (*u.txdb (), sequence);
            w.Account = fold ([] (const account a, const account_diff &d) -> account {
                return a << d;
            }, w.Account, restored.Account);
            history = history + restored.History;
            w.Addresses = w.Addresses.update (name, restored.Last);
            std::cout << "done checking address sequence " << name << std::endl;
        }

        u.set_wallet (w);

        // There is a potential problem here because if a history already
        // exists we will throw an error if we try to put histories together
        // out of order. The restore command really only works with a new wallet.
        auto &h = *u.history ();
        h <<= history;
    };

    auto restore_from_privkey = [&sk, &restore_from_pubkey] (Cosmos::Interface::writable u) {
        restore_from_pubkey (u);
        u.set_keys (u.get ().keys ()->insert (*sk));
    };

    if (bool (sk)) {
        read_wallet_options (e, p);
        e.update<void> (restore_from_privkey);
    } else {
        read_watch_wallet_options (e, p);
        e.update<void> (restore_from_pubkey);
    }

    std::cout << "Wallet restred. Total funds: " << e.wallet ()->value () << std::endl;
}
