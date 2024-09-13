
#include <regex>

#include <data/io/exception.hpp>
#include <data/crypto/NIST_DRBG.hpp>

#include <data/io/wait_for_enter.hpp>

#include <gigamonkey/timestamp.hpp>
#include <gigamonkey/schema/bip_44.hpp>
#include <gigamonkey/schema/bip_39.hpp>
#include <gigamonkey/script/pattern/pay_to_address.hpp>

#include <Cosmos/network.hpp>
#include <Cosmos/interface.hpp>
#include <Cosmos/wallet/split.hpp>
#include <Cosmos/wallet/restore.hpp>
#include <Cosmos/database/price_data.hpp>
#include <Cosmos/options.hpp>
#include <Cosmos/tax.hpp>
#include <Cosmos/boost/miner_options.hpp>

using namespace data;

Cosmos::error run (const arg_parser &);

enum class method {
    UNSET,
    HELP,     // print help messages
    VERSION,  // print a version message
    GENERATE, // generate a wallet
    RESTORE,  // restore a wallet
    VALUE,    // the value in the wallet.
    UPDATE,   // check pending txs for having been mined.
    REQUEST,  // request a payment
    RECEIVE,  // receive a payment
    PAY,      // make a payment.
    SIGN,     // sign an unsigned transaction
    IMPORT,   // import a utxo with private key
    SEND,     // (depricated) send bitcoin to an address.
    BOOST,    // boost some content
    SPLIT,    // split your wallet into tiny pieces for privacy.
    TAXES     // calculate income and capital gain for a given year.
};

int main (int arg_count, char **arg_values) {

    auto err = run (arg_parser {arg_count, arg_values});

    if (err.Message) std::cout << "Error: " << static_cast<std::string> (*err.Message) << std::endl;
    else if (err.Code) std::cout << "Error: unknown." << std::endl;

    return err.Code;
}

void version ();

void help (method meth = method::UNSET);

void command_generate (const arg_parser &); // offline
void command_update (const arg_parser &);
void command_restore (const arg_parser &);
void command_value (const arg_parser &);    // offline
void command_request (const arg_parser &);  // offline
void command_receive (const arg_parser &);  // offline
void command_pay (const arg_parser &);      // offline
void command_sign (const arg_parser &);     // offline
void command_send (const arg_parser &);     // depricated
void command_import (const arg_parser &);
void command_boost (const arg_parser &);    // offline
void command_split (const arg_parser &);
void command_taxes (const arg_parser &);    // offline

// TODO offline methods function without an internet connection.

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

                case method::VALUE: {
                    command_value (p);
                    break;
                }

                case method::RESTORE: {
                    command_restore (p);
                    break;
                }

                case method::UPDATE: {
                    command_update (p);
                    break;
                }

                case method::REQUEST: {
                    command_request (p);
                    break;
                }

                case method::RECEIVE: {
                    command_receive (p);
                    break;
                }

                case method::PAY: {
                    command_pay (p);
                    break;
                }

                case method::SIGN: {
                    command_sign (p);
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
    if (*m == "restore") return method::RESTORE;
    if (*m == "value") return method::VALUE;
    if (*m == "request") return method::REQUEST;
    if (*m == "receive") return method::RECEIVE;
    if (*m == "pay") return method::PAY;
    if (*m == "sign") return method::SIGN;
    if (*m == "import") return method::IMPORT;
    if (*m == "send") return method::SEND;
    if (*m == "boost") return method::BOOST;
    if (*m == "split") return method::SPLIT;
    if (*m == "taxes") return method::TAXES;

    return method::UNSET;
}

void help (method meth) {
    switch (meth) {
        default : {
            version ();
            std::cout << "input should be <method> <args>... where method is "
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
                "\nuse help \"method\" for information on a specific method"<< std::endl;
        } break;
        case method::GENERATE : {
            std::cout << "Generate a new wallet in terms of 24 words (BIP 39) or as an extended private key."
                "\narguments for method generate:"
                "\n\t(--name=)<wallet name>"
                "\n\t(--words) (use BIP 39)"
                "\n\t(--no_words) (don't use BIP 39)"
                "\n\t(--accounts=<uint32> (=10)) (how many accounts to pre-generate)"
                // TODO: as in method restore, a string should be accepted here which
                // could have "bitcoin" "bitcoin_cash" or "bitcoinSV" as its values,
                // ignoring case, spaces, and '_'.
                "\n\t(--coin_type=<uint32> (=0)) (value of BIP 44 coin_type)" << std::endl;
        } break;
        case method::VALUE : {
            std::cout << "Print the value in a wallet. No parameters." << std::endl;
        } break;
        case method::REQUEST : {
            std::cout << "Generate a new payment request."
                "\narguments for method request:"
                "\n\t(--name=)<wallet name>"
                "\n\t(--payment_type=\"pubkey\"|\"address\"|\"xpub\") (= \"address\")"
                "\n\t(--expires=<number of minutes before expiration>)"
                "\n\t(--memo=\"<explanation of the nature of the payment>\")"
                "\n\t(--amount=<expected amount of payment>)" << std::endl;
        } break;
        case method::RECEIVE : {
            std::cout << "arguments for method receive not yet available." << std::endl;
        } break;
        case method::PAY : {
            std::cout << "arguments for method pay not yet available." << std::endl;
        } break;
        case method::SIGN : {
            std::cout << "arguments for method sign not yet available." << std::endl;
        } break;
        case method::IMPORT : {
            std::cout << "arguments for method import not yet available." << std::endl;
        } break;
        case method::SEND : {
            std::cout << "This method is DEPRICATED" << std::endl;
        } break;
        case method::BOOST : {
            std::cout << "arguments for method boost not yet available." << std::endl;
        } break;
        case method::SPLIT : {
            std::cout << "Split outputs in your wallet into many tiny outputs with small values over a triangular distribution. "
                "\narguments for method split:"
                "\n\t(--name=)<wallet name>"
                "\n\t(--address=)<address | xpub>"
                "\n\t(--max_look_ahead=)<integer> (= 10) ; (only used if parameter 'address' is provided as an xpub"
                "\n\t(--min_sats=<float>) (= 123456)"
                "\n\t(--max_sats=<float>) (= 5000000)"
                "\n\t(--mean_sats=<float>) (= 1234567) " << std::endl;
        } break;
        case method::RESTORE : {
            std::cout << "arguments for method restore:"
                "\n\t(--name=)<wallet name>"
                "\n\t(--key=)<xpub | xpriv>"
                "\n\t(--max_look_ahead=)<integer> (= 10)"
                "\n\t(--words=<string>)"
                "\n\t(--key_type=\"HD_sequence\"|\"BIP44_account\"|\"BIP44_master\") (= \"HD_sequence\")"
                "\n\t(--coin_type=\"Bitcoin\"|\"BitcoinCash\"|\"BitcoinSV\"|<integer>)"
                "\n\t(--wallet_type=\"RelayX\"|\"ElectrumSV\"|\"SimplyCash\"|\"CentBee\"|<string>)"
                "\n\t(--entropy=<string>)" << std::endl;
        }
    }

}

void version () {
    std::cout << "Cosmos Wallet version 0.0.1 alpha" << std::endl;
}

// TODO encrypt file
void command_generate (const arg_parser &p) {
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
    std::cout << "private keys will be saved in " << *e.pubkeys_filepath () << "." << std::endl;
}

void command_value (const arg_parser &p) {
    Cosmos::Interface e {};
    Cosmos::read_account_and_txdb_options (e, p);
    e.update<void> (Cosmos::update_pending_transactions);
    if (!bool (e.watch_wallet ())) throw exception {} << "could not read wallet";
    return Cosmos::display_value (*e.watch_wallet ());
}

// find all pending transactions and check if merkle proofs are available.
void command_update (const arg_parser &p) {
    Cosmos::Interface e {};
    Cosmos::read_account_and_txdb_options (e, p);
    e.update<void> (Cosmos::update_pending_transactions);
}

void command_request (const arg_parser &p) {
    using namespace Cosmos;
    Cosmos::Interface e {};
    Cosmos::read_pubkeys_options (e, p);
    e.update<void> (Cosmos::update_pending_transactions);

    Bitcoin::timestamp now = Bitcoin::timestamp::now ();
    payments::request request {now};

    maybe<uint32> expiration_minutes;
    p.get ("expires", expiration_minutes);
    if (bool (expiration_minutes)) request.Expires = Bitcoin::timestamp {uint32 (now) + *expiration_minutes * 60};

    maybe<int64> satoshi_amount;
    p.get ("amount", satoshi_amount);
    if (bool (satoshi_amount)) request.Amount = Bitcoin::satoshi {*satoshi_amount};

    maybe<std::string> memo;
    p.get ("memo", memo);
    if (bool (memo)) request.Memo = *memo;

    maybe<std::string> payment_option_string;
    p.get ("payment_type", payment_option_string);

    payments::type payment_option = bool (payment_option_string) ?
        [] (const std::string &payment_option) -> payments::type {
            std::cout << "payment type is " << payment_option << std::endl;
            if (payment_option == "address") return payments::type::address;
            if (payment_option == "pubkey") return payments::type::pubkey;
            if (payment_option == "xpub") return payments::type::xpub;
            throw exception {} << "could not read payment type";
        } (sanitize (*payment_option_string)) : payments::type::address;

    if (payment_option == payments::type::xpub || payment_option == payments::type::pubkey)
        std::cout << "NOTE: the payment type you have chosen is not safe against a quantum attack. "
            "Please don't use it if you think your customer may have access to a quantum computer." << std::endl;

    e.update<void> ([&request, &payment_option] (Cosmos::Interface::writable u) {
        const auto *pub = u.get ().pubkeys ();
        const auto *pay = u.get ().payments ();
        if (pub == nullptr || pay == nullptr) throw exception {} << "could not read wallet";

        auto pr = payments::request_payment (payment_option, *pay, *pub, request);
        std::cout << "Show the following string to your customer to request payment. " << std::endl;
        std::cout << "\t" << payments::write_payment_request (pr.Request) << std::endl;
        u.set_pubkeys (pr.Pubkeys);
        u.set_payments (pr.Payments);
    });
}

void command_pay (const arg_parser &p) {
    using namespace Cosmos;
    Cosmos::Interface e {};
    Cosmos::read_wallet_options (e, p);

    // first look for a payment request.
    maybe<std::string> payment_request_string;
    p.get ("request", payment_request_string);

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
        pr = new payments::payment_request {payments::read_payment_request (*payment_request_string)};

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
    Bitcoin::satoshi value = e.watch_wallet ()->value ();

    std::cout << "This is a payment for " << *pr->Value.Amount << " sats; wallet value: " << value << std::endl;

    // TODO estimate fee to send this tx.
    if (value < pr->Value.Amount) throw exception {} << "Wallet does not have sufficient funds to make this payment";

    // TODO encrypt payment request in OP_RETURN.
    BEEF beef = e.update<BEEF> ([pr] (Interface::writable u) -> BEEF {

        Bitcoin::address addr {pr->Key};
        Bitcoin::pubkey pubkey {pr->Key};
        HD::BIP_32::pubkey xpub {pr->Key};

        spend::spent spent;
        if (addr.valid ()) {
            spent = u.make_tx ({Bitcoin::output {*pr->Value.Amount, pay_to_address::script (addr.digest ())}});
        } else if (pubkey.valid ()) {
            spent = u.make_tx ({Bitcoin::output {*pr->Value.Amount, pay_to_pubkey::script (pubkey)}});
        } else if (xpub.valid ()) {
            throw exception {} << "pay to xpub not yet implemented";
        } else throw exception {} << "could not read payment address " << pr->Key;

        maybe<SPV::proof> ppp = generate_proof (*u.local_txdb (),
            for_each ([] (const auto &e) -> Bitcoin::transaction {
                return Bitcoin::transaction (e.first);
            }, spent.Transactions));

        if (!bool (ppp)) throw exception {} << "failed to generate payment";

        BEEF beef {*ppp};

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

void command_receive (const arg_parser &p) {
    throw exception {} << "Commond receive not yet implemented";
}

void command_sign (const arg_parser &p) {
    throw exception {} << "Commond sign not yet implemented";
}

void command_import (const arg_parser &p) {
    using namespace Cosmos;
    Cosmos::Interface e {};
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
    e.update<void> (update_pending_transactions);

    e.update<void> ([&outpoint, &key, &script_code] (Cosmos::Interface::writable u) {
        const auto w = u.get ().wallet ();
        if (!bool (w)) throw exception {} << "could not load wallet.";

        auto txdb = u.txdb ();
        if (!bool (txdb)) throw exception {} << "could not read database";

        wallet wall = *w;
        wall.import_output (
            Bitcoin::prevout {outpoint, txdb->output (outpoint)},
            key, Gigamonkey::pay_to_address::redeem_expected_size (key.Compressed),
            bool (script_code) ? *script_code : bytes {});

        u.set_wallet (wall);
    });
}

// TODO get fee rate from options.
void command_send (const arg_parser &p) {
    std::cout << "WARNING: This command is depricated!" << std::endl;
    using namespace Cosmos;
    Cosmos::Interface e {};
    Cosmos::read_wallet_options (e, p);
    Cosmos::read_random_options (e, p);

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

    auto *rand = e.random ();
    auto *net = e.net ();

    if (rand == nullptr) throw exception {4} << "could not initialize random number generator.";
    if (net == nullptr) throw exception {5} << "could not connect to remote servers";

    e.update<void> (update_pending_transactions);

    if (address.valid ()) e.update<void> ([rand, net, &address, &spend_amount] (Cosmos::Interface::writable u) {
            u.broadcast (u.make_tx ({Bitcoin::output {spend_amount, pay_to_address::script (address.decode ().Digest)}}));
        });
    else if (xpub.valid ()) e.update<void> ([rand, net, &xpub, &spend_amount] (Cosmos::Interface::writable u) {
            u.broadcast (u.make_tx (for_each ([] (const redeemable &m) -> Bitcoin::output {
                    return m.Prevout;
                }, Cosmos::split {} (*rand, address_sequence {xpub, {}, 0}, spend_amount, .001).Outputs)));
        });
    else throw exception {2} << "Could not read address/xpub";
}

void command_boost (const arg_parser &p) {
    using namespace BoostPOW;

    Cosmos::Interface e {};
    Cosmos::read_wallet_options (e, p);
    Cosmos::read_random_options (e, p);

    maybe<int64> value;
    p.get (3, "value", value);
    if (!bool (value))
        throw exception {2} << "could not read value to boost.";

    Bitcoin::output op {Bitcoin::satoshi {*value}, Boost::output_script (script_options::read (p.Parser, 3)).write ()};

    e.update<void> (Cosmos::update_pending_transactions);
    e.update<void> ([net = e.net (), rand = e.random (), &op] (Cosmos::Interface::writable u) {
        u.broadcast (u.make_tx ({op}));
    });
}

namespace Cosmos {

    // we organize by script hash because we want to
    // redeem all identical scripts in the same tx.
    using splitable = map<digest256, list<entry<Bitcoin::outpoint, redeemable>>>;

    // collect splitable scripts by addres.
    splitable get_split_address (Interface::writable u, splitable x, const Bitcoin::address &addr) {
        const auto w = u.get ().wallet ();
        auto TXDB = u.txdb ();
        if (!bool (TXDB) | !bool (w)) throw exception {} << "could not load wallet";

        // is this address in our wallet?
        ordered_list<ray> outpoints = TXDB->by_address (addr);

        list<entry<Bitcoin::outpoint, redeemable>> outputs;

        if (outputs.size () == 0) return x;

        // this step doesn't entirely make sense because if we could
        // redeem any output with this address we could redeem every one.
        for (const ray &r : outpoints)
            if (auto en = w->Account.Account.find (r.Point); en != w->Account.Account.end ()) {
                outputs <<= entry <Bitcoin::outpoint, redeemable> {r.Point, en->second};
            } else throw exception {} << "WARNING: output " << r.Point << " for address " << addr <<
                " was not found in our account even though we believe we own this address.";

        return x.insert (Gigamonkey::SHA2_256 (pay_to_address::script (addr.digest ())), outputs);

    };

    struct top_splitable {
        digest256 ScriptHash;
        Bitcoin::satoshi Value;
        list<Bitcoin::outpoint> Outputs;

        bool operator < (const top_splitable &t) const {
            return Value > t.Value;
        }

        bool operator <= (const top_splitable &t) const {
            return Value >= t.Value;
        }
    };

    priority_queue<top_splitable> get_top_splitable (splitable x) {
        priority_queue<top_splitable> top;
        for (const auto &e : x) {
                top = top.insert (top_splitable {e.Key,
                fold ([] (const Bitcoin::satoshi val, const entry<Bitcoin::outpoint, redeemable> &e) {
                    return val + e.Value.Prevout.Value;
            }, Bitcoin::satoshi {0}, e.Value), for_each ([] (const auto &p) {
                return p.Key;
            }, e.Value)});
        }
        return top;
    }
}

void command_split (const arg_parser &p) {
    using namespace Cosmos;
    Interface e {};
    read_wallet_options (e, p);
    read_random_options (e, p);

    maybe<double> max_sats_per_output = double (options::DefaultMaxSatsPerOutput);
    maybe<double> mean_sats_per_output = double (options::DefaultMeanSatsPerOutput);
    maybe<double> min_sats_per_output = double (options::DefaultMinSatsPerOutput);
    maybe<double> fee_rate = double (options::default_fee_rate ());

    p.get ("min_sats", min_sats_per_output);
    p.get ("max_sats", max_sats_per_output);
    p.get ("mean_sats", mean_sats_per_output);
    p.get ("fee_rate", fee_rate);

    Cosmos::split split {int64 (*min_sats_per_output), int64 (*max_sats_per_output), *mean_sats_per_output};

    splitable x;

    // the user may provide an address or xpub to split. If it is provided, we look for
    // outputs in our wallet corresponding to that address or to addressed derived
    // sequentially from the pubkey.

    maybe<string> address_string;
    p.get (3, "address", address_string);

    if (bool (address_string)) {
        Bitcoin::address addr {*address_string};
        HD::BIP_32::pubkey pk {*address_string};
        if (!pk.valid () && !addr.valid ()) throw exception {6} << "could not read address string";

        if (addr.valid ())
            x = e.update<splitable> ([&addr] (Interface::writable u) {
                return get_split_address (u, splitable {}, addr);
            });
        else {

            // if we're using a pubkey we need a max_look_ahead.
            maybe<uint32> max_look_ahead;
            p.get (4, "max_look_ahead", max_look_ahead);
            if (!bool (max_look_ahead)) max_look_ahead = options {}.MaxLookAhead;

            x = e.update<splitable> ([&pk, &max_look_ahead] (Interface::writable u) {

                splitable z {};
                int32 since_last_unused = 0;
                address_sequence seq {pk, {}};

                while (since_last_unused < *max_look_ahead) {
                    size_t last_size = z.size ();
                    z = get_split_address (u, z, pay_to_address_signing (seq.last ()).Key);
                    seq = seq.next ();
                    if (z.size () == last_size) {
                        since_last_unused++;
                    } else since_last_unused = 0;
                }

                return z;
            });
        }
    } else {
        // in this case, we just look for all outputs in our account that we are able to split.
        x = e.update<splitable> ([&max_sats_per_output] (Interface::writable u) {

            const auto w = u.get ().wallet ();
            if (!bool (w)) throw exception {} << "could not load wallet";

            splitable z;

            for (const auto &[key, value]: w->Account.Account)
                // find all outputs that are worth splitting
                if (value.Prevout.Value > *max_sats_per_output) {
                    z = z.insert (
                        Gigamonkey::SHA2_256 (value.Prevout.Script),
                        {entry<Bitcoin::outpoint, redeemable> {key, value}},
                        [] (list<entry<Bitcoin::outpoint, redeemable>> o, list<entry<Bitcoin::outpoint, redeemable>> n)
                            -> list<entry<Bitcoin::outpoint, redeemable>> {
                                return o + n;
                            });
                }

            // go through all the smaller outputs and check on whether the same script exists in our list
            // of big outputs since we don't want to leave any script partially redeemed and partially not.
            for (const auto &[key, value]: w->Account.Account)
                // find all outputs that are worth splitting
                if (value.Prevout.Value <= *max_sats_per_output) {
                    digest256 script_hash = Gigamonkey::SHA2_256 (value.Prevout.Script);
                    if (z.contains (script_hash))
                        z = z.insert (script_hash,
                            {entry<Bitcoin::outpoint, redeemable> {key, value}},
                            [] (list<entry<Bitcoin::outpoint, redeemable>> o, list<entry<Bitcoin::outpoint, redeemable>> n)
                                -> list<entry<Bitcoin::outpoint, redeemable>> {
                                    return o + n;
                                });
                }

            return z;
        });
    }

    if (data::size (x) == 0) throw exception {1} << "No outputs to split";

    size_t num_outputs {0};
    Bitcoin::satoshi total_split_value {0};
    for (const auto &e : x) {
        num_outputs += e.Value.size ();
        for (const auto &v : e.Value) total_split_value += v.Value.Prevout.Value;
    }

    std::cout << "found " << x.size () << " scripts in " << num_outputs <<
        " outputs to split with a total value of " << total_split_value << std::endl;

    // TODO estimate fees here.

    auto top = get_top_splitable (x);

    int count = 0;
    std::cout << "top 10 splittable scripts: " << std::endl;
    for (const auto &t : top) {
        std::cout << "\t" << t.Value << " sats in script " << t.ScriptHash << " in " << std::endl;
        for (const auto &o : t.Outputs) std::cout << "\t\t" << o << std::endl;
        if (count++ >= 10) break;
    }

    if (!get_user_yes_or_no ("Do you want to continue?")) throw exception {} << "program aborted";

    e.update<void> ([rand = e.random (), &split, &x, fee_rate] (Cosmos::Interface::writable u) {

        // we will generate some txs and then add fake events, as if they had been broadcast,
        // to the history to determine tax implications before broadcasting them. If the program
        // halts before this function is complete, the txs will not be broadcast and the new
        // wallets will not be saved.
        events h = *u.history ();

        Bitcoin::timestamp now = Bitcoin::timestamp::now ();
        uint32 fake_block_index = 1;

        list<spend::spent> split_txs;
        wallet old = *u.get ().wallet ();
        for (const entry<digest256, list<entry<Bitcoin::outpoint, redeemable>>> &e : x) {
            spend::spent spent = split (Gigamonkey::redeem_p2pkh_and_p2pk, *rand, old, e.Value, *fee_rate);

            account new_account = old.Account;

            // each split may give us several new transactions to work with.
            for (const auto &[extx, diff] : spent.Transactions) {

                new_account <<= diff;

                stack<ray> new_events;
                uint32 index = 0;

                for (const Gigamonkey::extended::input &input : extx.Inputs)
                    new_events <<= ray {now, fake_block_index,
                        inpoint {diff.TXID, index++}, static_cast<Bitcoin::input> (input), input.Prevout.Value};

                h <<= ordered_list<ray> (reverse (new_events));
                fake_block_index++;
            }

            split_txs <<= spent;
            old = wallet {old.Keys, spent.Pubkeys, new_account};
        }

        std::cout << "Tax implications: " << std::endl;
        std::cout << tax::calculate (*u.txdb (), *u.price_data (), h.get_history (now)).CapitalGain << std::endl;

        if (!get_user_yes_or_no ("Do you want broadcast these transactions?")) throw exception {} << "program aborted";

        std::cout << "broadcasting split transactions" << std::endl;

        // TODO how do these get in my wallet?
        for (const spend::spent &x : split_txs) u.broadcast (x);

    });
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
    pubkeys derivations;

    if (key_type == master_key_type::HD_sequence) {
        std::cout << "\tfirst address: " << pk.derive (HD::BIP_32::path {0}).address () << std::endl;

        derivations = pubkeys {{{pk, {pk, {}}}}, {{"receive", {pk, {}}}}, "receive", "receive"};
    } else if (key_type == master_key_type::BIP44_account) {
        std::cout
            << "this is a bip 44 account"
            << "\n\tfirst receive address: " << pk.derive (HD::BIP_32::path {0, 0}).address ()
            << "\n\tfirst change address: " << pk.derive (HD::BIP_32::path {1, 0}).address () << std::endl;

        derivations = pubkeys {{{pk, {pk, {}}}}, {{"receive", {pk, {0}}}, {"change", {pk, {1}}}}, "receive", "change"};
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
            (const HD::BIP_32::secret &master, list<uint32> to_account_master, list<uint32> to_receive, list<uint32> to_change) ->pubkeys {
                HD::BIP_32::pubkey account_master = master.derive (to_account_master).to_public ();
                return pubkeys {
                    {{account_master, {master.to_public (), to_account_master}}},
                    {{"receive", {account_master, to_receive}}, {"change", {account_master, to_change}}}, "receive", "change"};
            };

            derivations = key_type == master_key_type::CentBee_master ?
                construct_derivation (*sk, {HD::BIP_44::purpose}, {0, 0, 0}, {0, 0, 1}) :
                construct_derivation (*sk, {HD::BIP_44::purpose, *coin_type, HD::BIP_32::harden (0)}, {0}, {1});

            HD::BIP_32::pubkey first_receive = derivations.last ("receive").derive ();
            HD::BIP_32::pubkey first_change = derivations.last ("change").derive ();

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
            derivations = pubkeys {{
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
                << derivations.last ("receiveBitcoin").derive ().address ()
                << "\n\tfirst change address: "
                << derivations.last ("changeBitcoin").derive ().address ()
                << "\nassuming this is a bip 44 master, coin type Bitcoin Cash"
                << "\n\tfirst receive address: "
                << derivations.last ("receiveBitcoinCash").derive ().address ()
                << "\n\tfirst change address: "
                << derivations.last ("changeBitcoin").derive ().address ()
                << "\nassuming this is a bip 44 master, coin type Bitcoin SV"
                << "\n\tfirst receive address: "
                << derivations.last ("receiveBitcoinSV").derive ().address ()
                << "\n\tfirst change address: "
                << derivations.last ("changeBitcoin").derive ().address ()
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

    auto restore_from_pubkey = [&max_look_ahead, &derivations] (Cosmos::Interface::writable u) {
        const auto &pp = u.get ().pubkeys ();
        if (!pp) throw exception {} << "could not load pubkeys";

        const auto *acp = u.get ().account ();
        if (!acp) throw exception {} << "could not load account";

        account acc = *acp;

        ordered_list<ray> history {};
        for (const data::entry<string, address_sequence> ed : derivations.Sequences) {
            std::cout << "checking address sequence " << ed.Key << std::endl;
            auto restored = restore {*max_look_ahead, false} (*u.txdb (), ed.Value);
            acc += restored.Account;
            history = history + restored.History;
            derivations = derivations.update (ed.Key, restored.Last);
            std::cout << "done checking address sequence " << ed.Key << std::endl;
        }

        u.set_account (acc);

        // TODO: we should have a way of merging the pubkeys. This would overwrite anything already in there!
        u.set_pubkeys (derivations);

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
        e.update<void> (restore_from_pubkey);
    } else {
        read_watch_wallet_options (e, p);
        e.update<void> (restore_from_privkey);
    }

    std::cout << "Wallet restred. Total funds: " << e.watch_wallet ()->value () << std::endl;
}

void command_taxes (const arg_parser &p) {
    using namespace Cosmos;

    math::signed_limit<Bitcoin::timestamp> begin = math::signed_limit<Bitcoin::timestamp>::negative_infinity ();
    math::signed_limit<Bitcoin::timestamp> end = math::signed_limit<Bitcoin::timestamp>::infinity ();

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
        std::cout << tax::calculate (*u.txdb (), *u.price_data (), h->get_history (begin, end)) << std::endl;
    });
}

