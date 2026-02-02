
#include <regex>

#include <data/io/exception.hpp>
#include <data/crypto/NIST_DRBG.hpp>

#include <data/io/wait_for_enter.hpp>

#include <gigamonkey/timestamp.hpp>
#include <gigamonkey/script/pattern/pay_to_address.hpp>

#include <Cosmos/network.hpp>
#include <Cosmos/wallet/split.hpp>
#include <Cosmos/wallet/restore.hpp>
#include <Cosmos/database/price_data.hpp>
#include <Cosmos/options.hpp>
#include <Cosmos/tax.hpp>
#include <Cosmos/boost/miner_options.hpp>

#include "Cosmos.hpp"
//#include "interface.hpp"


std::string regex_replace (const std::string &x, const std::regex &r, const std::string &n) {
    std::stringstream ss;
    std::regex_replace (std::ostreambuf_iterator<char> (ss), x.begin (), x.end (), r, n);
    return ss.str ();
}

std::string sanitize (const std::string &in) {
    return regex_replace (data::to_lower (in), std::regex {"_|-"}, "");
}

std::ostream &version (std::ostream &o) {
    return o << "Cosmos Wallet version 0.0.2 alpha";
}

std::ostream &help (std::ostream &o, method meth) {
    switch (meth) {
        default :
            return version (o) << "\n" << "input should be <method> <args>... where method is "
                "\n\tgenerate   -- create a new wallet."
                "\n\tupdate     -- get Merkle proofs for txs that were pending last time the program ran."
                "\n\tvalue      -- print the total value in the wallet."
                "\n\trequest    -- generate a payment_request."
                "\n\tpay        -- create a transaction based on a payment request."
                "\n\treceive    -- accept a new transaction for a payment request."
                "\n\tsign       -- sign an unsigned transaction."
                "\n\timport     -- add a utxo to this wallet."
                "\n\tsend       -- send to an address or script. (depricated)"
                "\n\tboost      -- boost content."
                "\n\tsplit      -- split an output into many pieces"
                "\n\trestore    -- restore a wallet from words, a key, or many other options."
                "\nuse help \"method\" for information on a specific method";
        case method::GENERATE :
            return o << "Generate a new wallet in terms of 24 words (BIP 39) or as an extended private key."
                "\narguments for method generate:"
                "\n\t(--name=)<wallet name>"
                "\n\t(--words) (use BIP 39)"
                "\n\t(--no_words) (don't use BIP 39)"
                "\n\t(--accounts=<uint32> (=10)) (how many accounts to pre-generate)"
                // TODO: as in method restore, a string should be accepted here which
                // could have "bitcoin" "bitcoin_cash" or "bitcoinSV" as its values,
                // ignoring case, spaces, and '_'.
                "\n\t(--coin_type=<uint32> (=0)) (value of BIP 44 coin_type)";
        case method::VALUE :
            return o << "Print the value in a wallet. No parameters.";
        case method::REQUEST :
            return o << "Generate a new payment request."
                "\narguments for method request:"
                "\n\t(--name=)<wallet name>"
                "\n\t(--payment_type=\"pubkey\"|\"address\"|\"xpub\") (= \"address\")"
                "\n\t(--expires=<number of minutes before expiration>)"
                "\n\t(--memo=\"<explanation of the nature of the payment>\")"
                "\n\t(--amount=<expected amount of payment>)";
        case method::PAY :
            return o << "Respond to a payment request by creating a payment."
                "\narguments for method pay:"
                "\n\t(--name=)<wallet name>"
                "\n\t(--request=)<payment request>"
                "\n\t(--address=<address to pay to>)"
                "\n\t(--amount=<amount to pay>)"
                "\n\t(--memo=<what is the payment about>)"
                "\n\t(--output=<output in hex>)"
                "\n\t(--min_sats_per_output=<float>) (= " << Cosmos::spend_options::DefaultMinSatsPerOutput << ")"
                "\n\t(--max_sats_per_output=<float>) (= " << Cosmos::spend_options::DefaultMaxSatsPerOutput << ")"
                "\n\t(--mean_sats_per_output=<float>) (= " << Cosmos::spend_options::DefaultMeanSatsPerOutput << ") ";
        case method::ACCEPT :
            return o << "Accept a payment."
                "\narguments for method accept:"
                "\n\t(--payment=)<payment tx in BEEF or SPV envelope>";
        case method::SIGN :
            return o << "arguments for method sign not yet available.";
        case method::IMPORT :
            return o << "arguments for method import not yet available.";
        case method::SEND :
            return o << "This method is DEPRICATED";
        case method::SPEND :
            return o << "Spend coins";
        case method::BOOST :
            return o << "arguments for method boost not yet available.";
        case method::SPLIT :
            return o << "Split outputs in your wallet into many tiny outputs with small values over a triangular distribution. "
                "\narguments for method split:"
                "\n\t(--name=)<wallet name>"
                "\n\t(--address=)<address | xpub | script hash>"
                "\n\t(--max_look_ahead=)<integer> (= 10) ; (only used if parameter 'address' is provided as an xpub"
                "\n\t(--min_sats_per_output=<float>) (= " << Cosmos::spend_options::DefaultMinSatsPerOutput << ")"
                "\n\t(--max_sats_per_output=<float>) (= " << Cosmos::spend_options::DefaultMaxSatsPerOutput << ")"
                "\n\t(--mean_sats_per_output=<float>) (= " << Cosmos::spend_options::DefaultMeanSatsPerOutput << ") ";
        case method::RESTORE :
            o << "arguments for method restore:"
                "\n\t(--name=)<wallet name>"
                "\n\t(--key=)<xpub | xpriv>"
                "\n\t(--max_look_ahead=)<integer> (= 10)"
                "\n\t(--words=<string>)"
                "\n\t(--key_type=\"HD_sequence\"|\"BIP44_account\"|\"BIP44_master\") (= \"HD_sequence\")"
                "\n\t(--coin_type=\"Bitcoin\"|\"BitcoinCash\"|\"BitcoinSV\"|<integer>)"
                "\n\t(--wallet_type=\"RelayX\"|\"ElectrumSV\"|\"SimplyCash\"|\"CentBee\"|<string>)"
                "\n\t(--entropy=<string>)";
        case method::ENCRYPT_KEY:
            o << "Encrypt the private key file so that it can only be accessed with a password. No parameters.";
        case method::DECRYPT_KEY :
            return o << "Decrypt the private key file again. No parameters.";
    }

}

maybe<std::string> de_escape (string_view input) {

    string rt {};

    std::ostringstream decoded;

    for (std::size_t i = 0; i < input.size (); ++i) {
        if (input[i] == '\\') {
            if (i + 1 >= input.size ()) return {};
            i += 1;
        }

        decoded << input[i];
    }

    return {decoded.str ()};
}
/*
void command_value (const arg_parser &p) {
    Cosmos::Interface e {};
    Cosmos::read_account_and_txdb_options (e, p);
    //e.update<void> (Cosmos::update_pending_transactions);
    auto w = e.wallet ();
    if (!bool (w)) throw exception {} << "could not read wallet";
    return Cosmos::display_value (*w);
}

// find all pending transactions and check if merkle proofs are available.
void command_update (const arg_parser &p) {
    Cosmos::Interface e {};
    Cosmos::read_account_and_txdb_options (e, p);
    e.update<void> (Cosmos::update_pending_transactions);
}

// TODO get fee rate from network.
// TODO make sure we don't invalidate existing payments.
void command_pay (const arg_parser &p) {
    using namespace Cosmos;
    Interface e {};
    read_wallet_options (e, p);
    read_random_options (p);

    // first look for a payment request.
    maybe<std::string> payment_request_string;
    p.get (3, "request", payment_request_string);

    // if we cannot find one, maybe there's an address.
    maybe<std::string> address_string;
    p.get ("address", address_string);

    // if there is no amount specified, check for one.
    maybe<int64> amount_sats;
    p.get ("amount", amount_sats);

    maybe<string> memo_input;
    p.get ("memo", memo_input);

    maybe<string> output;
    p.get ("output", output);

    payments::payment_request *pr;
    if (bool (payment_request_string)) {

        std::cout << "payment request inputed as " << *payment_request_string << std::endl;
        std::string payment_request = *de_escape (*payment_request_string);
        std::cout << "de escaped as " << payment_request << std::endl;

        pr = new payments::payment_request {payments::read_payment_request (JSON::parse (payment_request))};

        if (bool (pr->Value.Amount) && bool (amount_sats) && *pr->Value.Amount != Bitcoin::satoshi {*amount_sats})
            throw exception {} << "WARNING: amount provided in payment request and as an option and do not agree";

        if (!bool (pr->Value.Amount)) {
            if (!bool (amount_sats)) throw exception {} << "no amount provided";
            else pr->Value.Amount = Bitcoin::satoshi {*amount_sats};
        }

        if (bool (pr->Value.Memo) && bool (memo_input) && *pr->Value.Memo != *memo_input)
            throw exception {} << "WARNING: memo provided in payment request and as an option and do not agree";

        if (!bool (pr->Value.Memo) && bool (memo_input)) pr->Value.Memo = *memo_input;

    } else {
        if (!bool (address_string)) throw exception {} << "no address provided";
        if (!bool (amount_sats)) throw exception {} << "no amount provided";
        pr = new payments::payment_request {*address_string, payments::request {}};
        pr->Value.Amount = *amount_sats;
        if (bool (memo_input)) pr->Value.Memo = *memo_input;
    }

    // TODO if other incomplete payments exist, be sure to subtract
    // them from the account so as not to create a double spend.
    Bitcoin::satoshi wallet_value = e.wallet ()->value ();

    std::cout << "This is a payment for " << *pr->Value.Amount << " sats; wallet value: " << wallet_value << std::endl;

    // TODO estimate fee to send this tx.
    if (wallet_value < pr->Value.Amount) throw exception {} << "Wallet does not have sufficient funds to make this payment";

    options opts = read_tx_options (e, p);
    e.update<void> (update_pending_transactions);

    // TODO encrypt payment request in OP_RETURN.
    BEEF beef = e.update<BEEF> ([pr, opts] (Interface::writable u) -> BEEF {

        Bitcoin::address addr {pr->Key};
        Bitcoin::pubkey pubkey {pr->Key};
        HD::BIP_32::pubkey xpub {pr->Key};

        spend::spent spent;
        if (addr.valid ()) {
            spent = u.make_tx ({Bitcoin::output {*pr->Value.Amount, pay_to_address::script (addr.digest ())}}, opts);
        } else if (pubkey.valid ()) {
            spent = u.make_tx ({Bitcoin::output {*pr->Value.Amount, pay_to_pubkey::script (pubkey)}}, opts);
        } else if (xpub.valid ()) {
            throw exception {} << "pay to xpub not yet implemented";
        } else throw exception {} << "could not read payment address " << pr->Key;
        std::cout << " generating SPV proof " << std::endl;
        maybe<SPV::proof> ppp = generate_proof (*u.local_txdb (),
            for_each ([] (const auto &e) -> Bitcoin::transaction {
                return Bitcoin::transaction (e.first);
            }, spent.Transactions));

        if (!bool (ppp)) throw exception {} << "failed to generate payment";

        std::cout << "SPV proof generated containing " << ppp->Payment.size () <<
            " transactions and " << ppp->Proof.size () << " antecedents" << std::endl;

        BEEF beef {*ppp};
        std::cout << "Beef produced containing " << beef.Transactions.size () <<
            " transactions and " << beef.BUMPs.size () << " proofs" << std::endl;

        wait_for_enter ("Press enter to continue.");

        // save to proposed payments.
        auto payments = *u.get ().payments ();
        u.set_payments (Cosmos::payments {payments.Requests, payments.Proposals.insert (pr->Key, payments::offer
            {*pr, beef, for_each ([] (const auto &e) -> account_diff {
                return e.second;
            }, spent.Transactions)})});

        return beef;
    });

    if (bool (output)) {
        // TODO make sure that the output doesn't already exist.
        write_to_file (encoding::base64::write (bytes (beef)), *output);
        std::cout << "offer written to " << *output << std::endl;
    } else std::cout << "please show this string to your seller and he will broadcast the payment if he accepts it:\n\t" <<
        encoding::base64::write (bytes (beef)) << std::endl;

    delete pr;
}

void command_sign (const arg_parser &p) {
    throw exception {} << "Commond sign not yet implemented";
}

// TODO get fee rate from options.
void command_send (const arg_parser &p) {
    std::cout << "WARNING: This command is depricated!" << std::endl;
    using namespace Cosmos;
    Cosmos::Interface e {};
    Cosmos::read_wallet_options (e, p);
    Cosmos::read_random_options (p);

    maybe<std::string> address_string;
    p.get (3, "address", address_string);
    if (!bool (address_string))
        throw exception {2} << "could not read address.";

    HD::BIP_32::pubkey xpub {*address_string};
    Bitcoin::address address {*address_string};

    maybe<int64> value;
    p.get (4, "value", value);
    if (!bool (value))
        throw exception {2} << "could not read value to send.";

    Bitcoin::satoshi spend_amount {*value};

    auto *rand = get_random ();
    auto *net = e.net ();

    if (rand == nullptr) throw exception {4} << "could not initialize random number generator.";
    if (net == nullptr) throw exception {5} << "could not connect to remote servers";

    e.update<void> (update_pending_transactions);
    if (address.valid ()) e.update<void> ([rand, net, &address, &spend_amount] (Cosmos::Interface::writable u) {
            spend::spent spent = u.make_tx ({Bitcoin::output {spend_amount, pay_to_address::script (address.decode ().Digest)}});
            u.set_addresses (spent.Addresses);
            for (const auto &[extx, diff] : spent.Transactions)
                if (auto success = u.broadcast ({{Bitcoin::transaction (extx), diff}}); !bool (success))
                    throw exception {} << "broadcast failed with error " << success;
        });
    else if (xpub.valid ()) e.update<void> ([rand, net, &xpub, &spend_amount] (Cosmos::Interface::writable u) {
            spend::spent spent = u.make_tx (for_each ([] (const redeemable &m) -> Bitcoin::output {
                    return m.Prevout;
                }, Cosmos::split {} (*rand, address_sequence {xpub, {}, 0}, spend_amount, .001).Outputs));
            u.set_addresses (spent.Addresses);
            for (const auto &[extx, diff] : spent.Transactions)
                if (auto success = u.broadcast ({{Bitcoin::transaction (extx), diff}}); !bool (success))
                    throw exception {} << "broadcast failed with error " << success;
        });
    else throw exception {2} << "Could not read address/xpub";
}

void command_boost (const arg_parser &p) {
    using namespace BoostPOW;

    Cosmos::Interface e {};
    Cosmos::read_wallet_options (e, p);
    Cosmos::read_random_options (p);

    data::maybe<int64> value;
    p.get (3, "value", value);
    if (!bool (value))
        throw exception {2} << "could not read value to boost.";

    Bitcoin::output op {Bitcoin::satoshi {*value}, Boost::output_script (script_options::read (p.Parser, 3)).write ()};
    e.update<void> (Cosmos::update_pending_transactions);
    e.update<void> ([net = e.net (), &op] (Cosmos::Interface::writable u) {
        Cosmos::spend::spent x = u.make_tx ({op});
        u.set_addresses (x.Addresses);
        for (const auto &[extx, diff] : x.Transactions)
            if (auto success = u.broadcast ({{Bitcoin::transaction (extx), diff}}); !bool (success))
                throw exception {} << "broadcast failed with error " << success;
    });
}

void command_taxes (const arg_parser &p) {
    using namespace Cosmos;

    Cosmos::when begin = Cosmos::when::negative_infinity ();
    Cosmos::when end = Cosmos::when::infinity ();

    // first need to get a time range.
    // TODO maybe it should be possible to make a narrower time range.
    maybe<uint32> tax_year;
    p.get ("tax_year", tax_year);
    if (!bool (tax_year)) throw exception {1} << "no tax year given";

    std::tm tm_begin = {0, 0, 0, 1, 0, *tax_year - 1900};
    std::tm tm_end = {0, 0, 0, 1, 0, *tax_year - 1900 + 1};

    begin = Bitcoin::timestamp {std::mktime (&tm_begin)};
    end = Bitcoin::timestamp {std::mktime (&tm_begin)};

    // TODO there are also issues related to time zone
    // and the tax year.

    Interface e {};
    read_watch_wallet_options (e, p);

    // then look in history for that time range.
    e.update<void> ([&begin, &end] (Cosmos::Interface::writable u) {
        auto *h = u.history ();
        if (h == nullptr) throw exception {} << "could not read wallet history";

        std::cout << "Tax implications: " << std::endl;
        std::cout << tax::calculate (*u.txdb (), u.price_data (), h->get (begin, end)) << std::endl;
    });
}

crypto::symmetric_key<32> throw_on_call () {
    throw exception {} << "keys are already encrypted";
}

void command_encrypt_private_keys (const arg_parser &p) {
    using namespace Cosmos;

    Cosmos::Interface e {};
    Cosmos::read_wallet_options (e, p);
    Cosmos::read_random_options (p);

    // check if file is already encrypted.
    maybe<std::string> keychain_filepath = e.keychain_filepath ();
    if (!bool (keychain_filepath)) throw exception {} << "Could not read keys filepath";

    file fi = read_from_file (*keychain_filepath, &throw_on_call);

    crypto::symmetric_key<32> new_key = get_user_key_from_password ();

    // load key file the normal way.
    auto kkk = e.keys ();

    e.Files.replace_part (*keychain_filepath, {new_key});
}

void command_decrypt_private_keys (const arg_parser &p) {
    using namespace Cosmos;

    Cosmos::Interface e {};
    Cosmos::read_wallet_options (e, p);
    Cosmos::read_random_options (p);

    // check if file is already decrypted.
    maybe<std::string> keychain_filepath = e.keychain_filepath ();
    if (!bool (keychain_filepath)) throw exception {} << "Could not read keys filepath";

    bool already_decrypted = true;

    try {
        file fi = read_from_file (*keychain_filepath, &throw_on_call);
    } catch (...) {
        already_decrypted = false;
    }

    if (already_decrypted) throw exception {} << "Keys are already decrypted.";

    // load key file the normal way.
    auto kkk = e.keys ();

    e.Files.replace_part (*keychain_filepath, maybe<crypto::symmetric_key<32>> {});

}*/

