
#include <regex>

#include <data/io/exception.hpp>
#include <data/crypto/NIST_DRBG.hpp>

#include <data/io/wait_for_enter.hpp>

#include <gigamonkey/schema/bip_44.hpp>
#include <gigamonkey/schema/bip_39.hpp>
#include <gigamonkey/script/pattern/pay_to_address.hpp>

#include <Cosmos/network.hpp>
#include <Cosmos/context.hpp>
#include <Cosmos/wallet/split.hpp>
#include <Cosmos/wallet/restore.hpp>

using namespace data;

Cosmos::error run (const arg_parser &);

enum class method {
    UNSET,
    HELP,
    VERSION,
    GENERATE,
    RECEIVE,
    VALUE,
    IMPORT,
    SEND,
    BOOST,
    RESTORE,
    SPLIT,
    TAXES
};

int main (int arg_count, char **arg_values) {

    auto err = run (arg_parser {arg_count, arg_values});

    if (err.Message) std::cout << "Error: " << static_cast<std::string> (*err.Message) << std::endl;
    else if (err.Code) std::cout << "Error: unknown." << std::endl;

    return err.Code;
}

void version ();

void help (method meth = method::UNSET);

void command_generate (const arg_parser &);
void command_receive (const arg_parser &);
void command_value (const arg_parser &);
void command_send (const arg_parser &);
void command_import (const arg_parser &);
void command_boost (const arg_parser &);
void command_restore (const arg_parser &);
void command_split (const arg_parser &);
void command_taxes (const arg_parser &);

method read_method (const io::arg_parser &, uint32 index = 1);

Cosmos::error run (const io::arg_parser &p) {

    try {

        if (p.has ("version")) version ();

        else if (p.has ("help")) help ();

        else {

            method cmd = read_method (p);

            switch (cmd) {
                case method::VERSION: {
                    version ();
                    break;
                }

                case method::HELP: {
                    help (read_method (p, 2));
                    break;
                }

                case method::GENERATE: {
                    command_generate (p);
                    break;
                }

                case method::RECEIVE: {
                    command_receive (p);
                    break;
                }

                case method::VALUE: {
                    command_value (p);
                    break;
                }

                case method::SEND: {
                    command_send (p);
                    break;
                }

                case method::IMPORT: {
                    command_import (p);
                    break;
                }

                case method::SPLIT: {
                    command_split (p);
                    break;
                }

                case method::BOOST: {
                    command_boost (p);
                    break;
                }

                case method::RESTORE: {
                    command_restore (p);
                    break;
                }

                case method::TAXES: {
                    command_taxes (p);
                    break;
                }

                default: {
                    std::cout << "Error: could not read user's command." << std::endl;
                    help ();
                }
            }
        }

    } catch (const net::HTTP::exception &x) {
        std::cout << "Problem with http: " << std::endl;
        std::cout << "\trequest: " << x.Request << std::endl;
        std::cout << "\tresponse: " << x.Response << std::endl;
        return Cosmos::error {1, std::string {x.what ()}};
    } catch (const data::exception &x) {
        return Cosmos::error {x.Code, std::string {x.what ()}};
    } catch (const std::exception &x) {
        return Cosmos::error {1, std::string {x.what ()}};
    } catch (...) {
        return Cosmos::error {1};
    }

    return {};
}

std::string regex_replace (const std::string &x, const std::regex &r, const std::string &n) {
    std::stringstream ss;
    std::regex_replace (std::ostreambuf_iterator<char> (ss), x.begin (), x.end (), r, n);
    return ss.str ();
}

std::string inline sanitize (const std::string &in) {
    return regex_replace (data::to_lower (in), std::regex {"_|-"}, "");
}

void command_generate (const arg_parser &p) {
    Cosmos::context e {};
    Cosmos::read_both_chains_options (e, p);
    return Cosmos::generate_wallet (*e.keychain_filename (), *e.pubkeychain_filename ());
}

void command_value (const arg_parser &p) {
    Cosmos::context e {};
    Cosmos::read_account_and_txdb_options (e, p);
    return Cosmos::display_value (*e.update ().watch_wallet ());
}

void command_receive (const arg_parser &p) {
    Cosmos::context e {};
    Cosmos::read_pubkeychain_options (e, p);
    return Cosmos::generate_new_xpub (*e.update ().pubkeys ());
}

void command_import (const arg_parser &p) {
    using namespace Cosmos;
    Cosmos::context e {};
    Cosmos::read_wallet_options (e, p);

    maybe<std::string> txid_hex;
    p.get (3, "txid", txid_hex);
    if (!bool (txid_hex)) throw exception {2} << "could not read txid of output to import.";
    digest256 txid {*txid_hex};
    if (!txid.valid ()) throw exception {2} << "could not read txid of output to import.";

    maybe<uint32> index;
    p.get (4, "index", index);
    if (!bool (index)) throw exception {2} << "could not read index of output to import.";

    maybe<std::string> wif;
    p.get (5, "wif", wif);
    if (!bool (wif)) throw exception {2} << "could not read key for redeeming output.";
    Bitcoin::secret key {*wif};
    if (!key.valid ()) throw exception {2} << "could not read key for redeeming output.";

    maybe<std::string> script_code_hex;
    p.get (6, "script_code", script_code_hex);
    maybe<bytes> script_code;
    if (bool (script_code_hex)) script_code = *encoding::hex::read (*script_code_hex);

    Bitcoin::outpoint outpoint {txid, *index};

    auto &w = *e.update ().wallet ();
    w.import_output (
        Bitcoin::prevout {outpoint, e.update ().txdb ()->output (outpoint)},
        key, Gigamonkey::pay_to_address::redeem_expected_size (key.Compressed),
        bool (script_code) ? *script_code : bytes {});
}

void command_send (const arg_parser &p) {
    using namespace Cosmos;
    Cosmos::context e {};
    Cosmos::read_wallet_options (e, p);
    Cosmos::read_random_options (e, p);

    maybe<std::string> address_string;
    p.get (3, "address", address_string);
    if (!bool (address_string))
        throw exception {2} << "could not read address.";

    HD::BIP_32::pubkey xpub {*address_string};
    Bitcoin::address address {*address_string};
    if (!address.valid () && !xpub.valid ())
        throw exception {2} << "could not read address.";

    maybe<int64> value;
    p.get (4, "value", value);
    if (!bool (value))
        throw exception {2} << "could not read value to send.";

    if (address.valid ()) Cosmos::send_to (*e.net (), *e.update ().wallet (), *e.random (),
        {Bitcoin::output {Bitcoin::satoshi {*value}, pay_to_address::script (address.decode ().Digest)}});

    auto &rand = *e.random ();
    Cosmos::send_to (*e.net (), *e.update ().wallet (), rand,
        for_each ([] (const redeemable &m) -> Bitcoin::output {
            return m.Prevout;
        }, Cosmos::split {} (rand, address_sequence {xpub, {}, 0}, Bitcoin::satoshi {*value}, .001).Outputs));
}

void command_boost (const arg_parser &p) {
    throw exception {} << "command_boost is unimplemented";
}
/*
void command_boost (const arg_parser &p) {
    Cosmos::context e {};
    Cosmos::read_wallet_options (e, p);
    Cosmos::read_random_options (e, p);

    maybe<int64> value;
    p.get (3, "value", value);
    if (!bool (value))
        throw exception {2} << "could not read value to boost.";

    auto script = Boost::output_script (BoostPOW::script_options::read (p.Parser, 4));

    Cosmos::send_to (*e.net (), *e.wallet (), *e.random (),
        {Bitcoin::output {Bitcoin::satoshi {*value}, script.write ()}});
}*/

void command_split (const arg_parser &p) {
    using namespace Cosmos;
    Cosmos::context e {};
    Cosmos::read_wallet_options (e, p);
    Cosmos::read_random_options (e, p);

    auto &w = *e.update ().wallet ();
    auto &TXDB = *e.update ().txdb ();

    maybe<string> address_string;
    p.get (3, "address", address_string);
    if (!bool (address_string)) throw exception {1} << "could not read address to split";

    Bitcoin::address addr {*address_string};
    if (!addr.valid ()) throw exception {5} << "invalid address";

    // is this address in our wallet?
    ordered_list<Cosmos::ray> outpoints = TXDB.by_address (addr);
    list<entry<Bitcoin::outpoint, Cosmos::redeemable>> outputs;

    for (const Cosmos::ray &r : outpoints) {
        auto e = w.Account.Account.find (r.Point);
        if (e != w.Account.Account.end ()) outputs <<= entry<Bitcoin::outpoint, Cosmos::redeemable> {r.Point, e->second};
    }

    if (data::size (outputs) == 0) throw exception {1} << "We do not know how to spend this address";

    maybe<double> max_sats_per_output =  50000000;
    maybe<double> mean_sats_per_output = 12345678;
    maybe<double> min_sats_per_output = 1234567;

    p.get ("min_sats", min_sats_per_output);
    p.get ("max_sats", max_sats_per_output);
    p.get ("mean_sats", mean_sats_per_output);

    auto spent = Cosmos::split {int64 (*max_sats_per_output), int64 (*min_sats_per_output), *mean_sats_per_output}
        (Gigamonkey::redeem_p2pkh_and_p2pk, *e.random (), w, outputs, 50. / 1000.);

    e.net ()->broadcast (bytes (spent.Transaction));
    w = spent.Wallet;

}

struct top_receive_event {
    Cosmos::events::event Event;
    std::weak_ordering operator <=> (const top_receive_event &e) const {
        return Event.Received <=> e.Event.Received;
    }

    bool operator == (const top_receive_event &e) const {
        return Event.Received == e.Event.Received;
    }
};

std::ostream &operator << (std::ostream &o, const top_receive_event &e) {
    return o << "received {" << e.Event.Received << " sats in " << e.Event.TXID << "}";
}

struct top_move_event {
    Cosmos::events::event Event;
    std::weak_ordering operator <=> (const top_move_event &e) const {
        return Event.Moved <=> e.Event.Moved;
    }

    bool operator == (const top_move_event &e) const {
        return Event.Moved == e.Event.Moved;
    }
};

std::ostream &operator << (std::ostream &o, const top_move_event &e) {
    return o << "moved {" << e.Event.Moved << " sats in " << e.Event.TXID << "}";
}

struct top_events_event {
    Cosmos::events::event Event;
    std::weak_ordering operator <=> (const top_events_event &e) const {
        return data::size (Event.Events) <=> data::size (e.Event.Events);
    }

    bool operator == (const top_events_event &e) const {
        return data::size (Event.Events) == data::size (e.Event.Events);
    }
};

std::ostream &operator << (std::ostream &o, const top_events_event &e) {
    return o << "found " << data::size (e.Event.Events) << " events in " << e.Event.TXID << ". ";
}

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
    pubkeychain derivations;

    if (key_type == master_key_type::HD_sequence) {
        std::cout << "\tfirst address: " << pk.derive (HD::BIP_32::path {0}).address () << std::endl;

        derivations = pubkeychain {{{pk, {pk, {}}}}, {{"receive", {pk, {}}}}, "receive", "receive"};
    } else if (key_type == master_key_type::BIP44_account) {
        std::cout
            << "this is a bip 44 account"
            << "\n\tfirst receive address: " << pk.derive (HD::BIP_32::path {0, 0}).address ()
            << "\n\tfirst change address: " << pk.derive (HD::BIP_32::path {1, 0}).address () << std::endl;

        derivations = pubkeychain {{{pk, {pk, {}}}}, {{"receive", {pk, {0}}}, {"change", {pk, {1}}}}, "receive", "change"};
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

            auto construct_derivation = []
            (const HD::BIP_32::secret &master, list<uint32> to_account_master, list<uint32> to_receive, list<uint32> to_change) ->pubkeychain {
                HD::BIP_32::pubkey account_master = master.derive (to_account_master).to_public ();
                return pubkeychain {
                    {{account_master, {master.to_public (), to_account_master}}},
                    {{"receive", {account_master, to_receive}}, {"change", {account_master, to_change}}}, "receive", "change"};
            };

            derivations = key_type == master_key_type::CentBee_master ?
                construct_derivation (*sk, {HD::BIP_44::purpose}, {0, 0, 0}, {0, 0, 1}) :
                construct_derivation (*sk, {HD::BIP_44::purpose, *coin_type, HD::BIP_32::harden (0)}, {0}, {1});

            auto first_receive = derivations.last ("receive").Key;
            auto first_change = derivations.last ("change").Key;

            std::cout
                << "\nthis is a bip 44 master, coin type " << *coin_type
                << "\n\tfirst receive address: " << first_receive
                << "\n\tfirst change address: " << first_change;

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
            derivations = pubkeychain {{
                {bitcoin_account_master, {pk, bitcoin_to_account_master}},
                {bitcoin_cash_account_master, {pk, bitcoin_cash_to_account_master}},
                {bitcoin_sv_account_master, {pk, bitcoin_sv_to_account_master}},
                {centbee_account_master, {pk, centbee_to_account_master}}}, {
                {"receiveBitcoin", {bitcoin_account_master, {0}}},
                {"changeBitcoin", {bitcoin_account_master, {1}}},
                {"receiveBitcoinCash", {bitcoin_cash_account_master, {0}}},
                {"changeBitcoinCash", {bitcoin_cash_account_master, {1}}},
                {"receiveBitcoinSV", {bitcoin_sv_account_master, {0}}},
                {"changeBitcoinSV", {bitcoin_sv_account_master, {1}}},
                {"receiveCentBee", {centbee_account_master, {0}}},
                {"changeCentBee", {centbee_account_master, {1}}}}, "receive", "change"};

            std::cout
                << "coin type could not be determined."
                << "\nassuming this is a bip 44 master, coin type Bitcoin "
                << "\n\tfirst receive address: "
                << derivations.last ("receiveBitcoin").Key
                << "\n\tfirst change address: "
                << derivations.last ("changeBitcoin").Key
                << "\nassuming this is a bip 44 master, coin type Bitcoin Cash"
                << "\n\tfirst receive address: "
                << derivations.last ("receiveBitcoinCash").Key
                << "\n\tfirst change address: "
                << derivations.last ("changeBitcoin").Key
                << "\nassuming this is a bip 44 master, coin type Bitcoin SV"
                << "\n\tfirst receive address: "
                << derivations.last ("receiveBitcoinSV").Key
                << "\n\tfirst change address: "
                << derivations.last ("changeBitcoin").Key
                << std::endl;

        }
    }

    maybe<uint32> max_look_ahead;
    p.get (4, "max_look_ahead", max_look_ahead);
    if (!bool (max_look_ahead)) max_look_ahead = 25;

    std::cout << "max look ahead is " << *max_look_ahead << std::endl;

    restore rr {*max_look_ahead};

    std::cout << "loading wallet" << std::endl;
    wait_for_enter ();

    context e {};

    if (bool (sk)) read_wallet_options (e, p);
    else read_watch_wallet_options (e, p);
    auto *w = e.update ().watch_wallet ();
    if (!w) throw exception {} << "could not load wallet";
    w->Pubkeys = derivations;

    ordered_list<ray> history {};
    for (const data::entry<string, address_sequence> ed : derivations.Sequences) {
        std::cout << "checking address sequence " << ed.Key << std::endl;
        auto restored = restore {*max_look_ahead} (*e.update ().txdb (), ed.Value);
        w->Account += restored.Account;
        history = history + restored.History;
        w->Pubkeys = w->Pubkeys.update (ed.Key, restored.Last);
        std::cout << "done checking address sequence " << ed.Key << std::endl;
    }

    std::cout << "Wallet history: " << history << std::endl;

}

void command_taxes (const arg_parser &) {
    throw exception {} << "method command_taxes unimplemented";
}

method read_method (const arg_parser &p, uint32 index) {
    maybe<std::string> m;
    p.get (index, m);
    if (!bool (m)) return method::UNSET;

    std::transform (m->begin (), m->end (), m->begin (),
        [] (unsigned char c) {
            return std::tolower (c);
        });

    if (*m == "help") return method::HELP;
    if (*m == "version") return method::VERSION;
    if (*m == "generate") return method::GENERATE;
    if (*m == "receive") return method::RECEIVE;
    if (*m == "value") return method::VALUE;
    if (*m == "import") return method::IMPORT;
    if (*m == "send") return method::SEND;
    if (*m == "boost") return method::BOOST;
    if (*m == "restore") return method::RESTORE;

    return method::UNSET;
}

void help (method meth) {
    switch (meth) {
        default : {
            version ();
            std::cout << "input should be <method> <args>... where method is "
                "\n\tgenerate   -- create a new wallet."
                "\n\tvalue      -- print the total value in the wallet."
                "\n\treceive    -- generate a new xpub + address."
                "\n\timport     -- add a utxo to this wallet."
                "\n\tsend       -- send to an address or script."
                "\n\tboost      -- boost content."
                "\n\tsplit      -- split an output into many pieces"
                "\n\trestore    -- restore a wallet."
                "\nuse help \"method\" for information on a specific method"<< std::endl;
        } break;
        case method::GENERATE : {
            std::cout << "arguments for method generate not yet available." << std::endl;
        } break;
        case method::RECEIVE : {
            std::cout << "arguments for method receive not yet available." << std::endl;
        } break;
        case method::VALUE : {
            std::cout << "arguments for method value not yet available." << std::endl;
        } break;
        case method::IMPORT : {
            std::cout << "arguments for method import not yet available." << std::endl;
        } break;
        case method::SEND : {
            std::cout << "arguments for method send not yet available." << std::endl;
        } break;
        case method::BOOST : {
            std::cout << "arguments for method boost not yet available." << std::endl;
        } break;
        case method::RESTORE : {
            std::cout << "arguments for method restore:"
                "\n\t(--name=)<wallet name>"
                "\n\t(--key=)<public key>"
                "\n\t(--max_look_ahead=)<integer> (= 25)"
                "\n\t(--key_type=\"HD_sequence\"|\"BIP44_account\"|\"BIP44_master\") (= \"HD_sequence\")"
                "\n\t(--coin_type=\"Bitcoin\"|\"BitcoinCash\"|\"BitcoinSV\"|<integer>)"
                "\n\t(--wallet_type=\"RelayX\"|\"ElectrumSV\"|\"SimplyCash\"|\"CentBee\"|<string>)"
                "\n\t(--words=<string>)"
                "\n\t(--entropy=<string>)" << std::endl;
        } break;
        case method::SPLIT : {
            std::cout << "arguments for method split not yet available." << std::endl;
        } break;
    }

}

void version () {
    std::cout << "Cosmos Wallet version 0.0" << std::endl;
}

