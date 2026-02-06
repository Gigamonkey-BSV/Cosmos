#include "server.hpp"
#include "../Cosmos.hpp"
#include "invert_hash.hpp"
#include "key.hpp"
#include "to_private.hpp"
#include "import.hpp"

#include <Diophant/parse.hpp>
#include <Diophant/symbol.hpp>

#include <gigamonkey/schema/random.hpp>
#include <gigamonkey/schema/bip_39.hpp>
#include <gigamonkey/schema/electrum_sv.hpp>
#include <gigamonkey/pay/BEEF.hpp>

#include <data/tools/schema.hpp>
#include <data/container.hpp>

#include <Cosmos/Diophant.hpp>
#include <Cosmos/REST/generate.hpp>
#include <Cosmos/REST/restore.hpp>

namespace schema = data::schema;
using BEEF = Gigamonkey::BEEF;

// for methods that operate on the database 
// without a particular wallet. 
net::HTTP::response process_method (
    server &,
    net::HTTP::method, Cosmos::method,
    map<UTF8, UTF8> query,
    const maybe<net::HTTP::content> &,
    const data::bytes &);

// for methods that operate on a specific wallet. 
net::HTTP::response process_wallet_method (
    server &p, net::HTTP::method http_method, Cosmos::method m,
    Diophant::symbol wallet_name,
    map<UTF8, UTF8> query,
    const maybe<net::HTTP::content> &content,
    const data::bytes &body);

extern std::atomic<bool> Shutdown;

// remove empty strings from the begining and end if they exist.
// When we use '/' as a delimiter, we will always get an empty
// string at the start, since the path always starts with '/' if
// it exists. A user may end a path with '/' as well.
list<UTF8> normalize_path (list<UTF8> path) {
    if (size (path) == 0) return path;
    if (first (path) == "") path = rest (path);
    if (size (path) == 0) return path;
    auto r = reverse (path);
    if (first (path) == "") r = rest (r);
    return reverse (r);
}

using namespace Cosmos;

awaitable<net::HTTP::response> server::operator () (const net::HTTP::request &req) {

    if (UserEntropy != nullptr) *UserEntropy << req;

    DATA_LOG (normal) << "Respond to request " << req;
    list<UTF8> path = normalize_path (req.Target.path ().read ('/'));

    // if no API method was called, serve the GUI. 
    if (size (path) == 0) co_return HTML_JS_UI_response ();

    // The favicon doesn't work right now, not sure why.
    if (size (path) == 1 && path[0] == "favicon.ico") co_return favicon ();

    method m = read_method (path[0]);

    if (m == method::UNSET) co_return error_response (400, m, problem::unknown_method, path[0]);

    if (m == method::VERSION) {
        if (req.Method != net::HTTP::method::get)
            co_return error_response (405, method::VERSION, problem::invalid_method, "use get");

        co_return version_response ();
    }

    if (m == method::HELP) {
        if (req.Method != net::HTTP::method::get)
            co_return error_response (405, method::HELP, problem::invalid_method, "use get with method help");

        if (path.size () == 1) co_return help_response ();
        else {
            method help_with_method = read_method (path[1]);
            if (help_with_method == method::UNSET)
                co_return error_response (400, method::HELP, problem::unknown_method, path[1]);
            co_return help_response (help_with_method);
        }
    }

    if (m == method::SHUTDOWN) {
        if (req.Method != net::HTTP::method::put)
            co_return error_response (405, method::SHUTDOWN, problem::invalid_method, "use put with method shutdown");

        Shutdown = true;
        co_return ok_response ();
    }

    if (m == method::ADD_ENTROPY) {
        if (req.Method != net::HTTP::method::post)
            co_return error_response (405, method::ADD_ENTROPY, problem::invalid_method,
                "use post with method add_entropy");

        // request was already proccessed into the user entropy.

        co_return ok_response ();
    }

    if (m == method::LIST_WALLETS) {
        if (req.Method != net::HTTP::method::get)
            co_return error_response (405, m, problem::invalid_method, "use get");
        JSON::array_t names;
        for (const std::string &name : DB.list_wallet_names ())
            names.push_back (name);

        co_return JSON_response (names);
    }

    // we checked the methods that don't take any parameters. 
    // now we get the parameters from the query. 
    map<UTF8, UTF8> query;
    maybe<dispatch<UTF8, UTF8>> qm = req.Target.query_map ();
    if (bool (qm)) {
        map<UTF8, list<UTF8>> q = data::dispatch_to_map (*qm);
        
        for (const auto &[k, v] : q) if (v.size () != 1) 
          co_return error_response (400, m, problem::invalid_parameter, "duplicate query parameters");
        else query = query.insert (k, v[0]);
    }

    UTF8 fragment {};
    maybe<UTF8> fm = req.Target.fragment ();
    if (bool (fm)) fragment = *fm;

    if (fragment != "") co_return error_response (400, m, problem::invalid_target, "We don't use the fragment");

    try {

        if (path.size () < 2)
            co_return process_method (*this, req.Method, m, query, req.content_type (), req.Body);

        // make sure the wallet name is a valid string.
        Diophant::symbol wallet_name {path[1]};
        if (!data::valid (wallet_name)) co_return error_response (400, m, problem::invalid_wallet_name);

        co_return process_wallet_method (*this, req.Method, m, wallet_name, query, req.content_type (), req.Body);

    } catch (const data::random::source::fail &) {
        co_return error_response (500, m, problem::need_entropy);
    } catch (const Diophant::parse_error &w) {
        co_return error_response (400, m,
            problem::invalid_expression,
            // this is a pretty unhelpful message. However, hopefully we won't throw this in the future.
            string::write ("an invalid Diophant expression was generated: ", w.what ()));
    } catch (schema::missing_key mk) {
        co_return error_response (400, m, problem::missing_parameter, string::write ("missing parameter ", mk.Key));
    } catch (schema::invalid_entry iv) {
        co_return error_response (400, m, problem::invalid_parameter, string::write ("invalid parameter ", iv.Key));
    } catch (schema::unknown_key uk) {
        co_return error_response (400, m, problem::invalid_query, string::write ("unknown parameter ", uk.Key));
    } catch (schema::incomplete_match uk) {
        co_return error_response (400, m, problem::unexpected_parameter,
            string::write ("unexpected parameter ", uk.Key));
    } catch (data::method::unimplemented un) {
        co_return error_response (501, m, problem::unimplemented, un.what ());
    }
}

net::HTTP::response process_method (
    server &p, net::HTTP::method http_method, method m,
    map<UTF8, UTF8> query,
    const maybe<net::HTTP::content> &content_type,
    const data::bytes &body) {

    if (m == method::INVERT_HASH) return handle_invert_hash (p, http_method, query, content_type, body);

    if (m == method::TO_PRIVATE) return handle_to_private (p, http_method, query, content_type, body);

    return error_response (500, m, problem::invalid_wallet_name, "wallet method called without wallet name");
}

net::HTTP::response process_wallet_method (
    server &p, net::HTTP::method http_method, method m,
    Diophant::symbol wallet_name,
    map<UTF8, UTF8> query,
    const maybe<net::HTTP::content> &content_type,
    const data::bytes &body) {

    // make sure the wallet name is a valid string.
    if (!wallet_name.valid ()) return error_response (400, m, problem::invalid_wallet_name, "wallet name argument must be alpha alnum+");

    // Associate a secret key with a name. The key could be anything; private, public, or symmetric.
    if (m == method::KEY) return handle_key (p, wallet_name, http_method, query, content_type, body);

    if (m == method::CREATE_WALLET) {
        if (http_method != net::HTTP::method::post)
            return error_response (405, m, problem::invalid_method, "use post");

        bool created = p.DB.make_wallet (wallet_name);
        if (created) return ok_response ();
        else return error_response (500, m, problem::failed, "could not create wallet");
    }

    if (m == method::KEY_SEQUENCE) {

        auto [name, post_key_sequence] = schema::validate<> (query,
            schema::map::key<Diophant::symbol> ("name") &&
            *(schema::map::key<key_expression> ("key") &&
                schema::map::key<key_derivation> ("derivation") &&
                *schema::map::key<uint32> ("index")));

        // make sure the name is a valid symbol name
        if (!data::valid (name))
            return error_response (400, m, problem::invalid_parameter,
                "key_name parameter must be alpha alnum+");

        if (post_key_sequence) {
            if (http_method != net::HTTP::method::post)
                return error_response (405, m, problem::invalid_method, "use POST");

            auto [key, derivation, index_param] = *post_key_sequence;
            uint32 index = bool (index_param) ? *index_param : 0;

            if (!key.valid ())
                return error_response (400, m, problem::invalid_parameter,
                    "name parameter must be alpha alnum+");

            if (!data::valid (derivation))
                return error_response (400, m, problem::invalid_parameter,
                    "name parameter must be alpha alnum+");

            if (!p.DB.set_wallet_sequence (wallet_name, name,
                Cosmos::key_sequence {key, derivation}, index))
                return error_response (400, m, problem::invalid_parameter,
                    string::write ("wallet ", wallet_name, " does not exist."));

            return ok_response ();

        }

        if (http_method != net::HTTP::method::get)
            return error_response (405, m, problem::invalid_method, "use GET");

        auto seq = p.DB.get_wallet_sequence (wallet_name, name);
        if (!seq) return error_response (404, m, problem::failed,
            string::write ("Could not find sequence ", wallet_name, ".", name));

        return string_response (std::string (*seq));
    }

    if (m == method::VALUE) {
        if (http_method != net::HTTP::method::get)
            return error_response (405, m, problem::invalid_method, "use get");

        return value_response (p.DB.get_wallet_account (wallet_name).value ());
    }

    if (m == method::DETAILS) {
        if (http_method != net::HTTP::method::get)
            return error_response (405, m, problem::invalid_method, "use get");

        return JSON_response (p.DB.get_wallet_account (wallet_name).details ());
    }

    if (m == method::GENERATE) {

        if (http_method != net::HTTP::method::post)
            return error_response (405, m, problem::invalid_method, "use post");

        return handle_generate (p, wallet_name, query, content_type, body);
    }

    if (m == method::NEXT) {
        if (http_method != net::HTTP::method::post)
            return error_response (405, m, problem::invalid_method, "use post");

        if (!data::contains (p.DB.list_wallet_names (), static_cast<const std::string &> (wallet_name)))
            return error_response (400, m, problem::invalid_parameter,
                string::write ("cannot find wallet named ", wallet_name));

        next_request_options opts (wallet_name, query);

        // in the future we will look for a default sequence name, but for now we don't.
        if (!opts.Sequence || !opts.Sequence->valid ())
            return error_response (400, m, problem::invalid_parameter, "sequence");
        
        // do we have a sequence by this name? 
        Cosmos::key_source sequence;

        {
            maybe<Cosmos::key_source> seq = p.DB.get_wallet_sequence (wallet_name, *opts.Sequence);
            if (!bool (seq)) return error_response (400, m, problem::invalid_query, string::write ("no key named ", *opts.Sequence));
            sequence = *seq;
        }

        // generate a new address (which could be an xpub).
        Cosmos::key_expression next_key {*sequence};

        if (!next_key.valid ())
            return error_response (500, method::NEXT, problem::failed);

        // make an unused reference to put in the database for later.
        p.DB.set_wallet_unused (wallet_name, database::unused {next_key, sequence.Sequence.Key});

        p.DB.set_wallet_sequence (wallet_name, *opts.Sequence, sequence.Sequence, sequence.Index + 1);

        return string_response (std::string (next_key));
    }

    if (m == method::IMPORT) {
        if (http_method != net::HTTP::method::put)
            return error_response (405, m, problem::invalid_method, "use put");

        return handle_import (p, wallet_name, query, content_type, body);

    }

    if (m == method::SPEND) {
        if (http_method != net::HTTP::method::post)
            return error_response (405, m, problem::invalid_method, "use post");

        auto [to, value, fee_rate, min_change_value, unit,
            map_redeem_proportion, min_reedeem_proportion,
            max_value_per_tx, min_value_per_tx, mean_value_per_tx,
            max_value_per_output, min_value_per_output, mean_value_per_output] = schema::validate<> (query,
            schema::map::key<std::string> ("to") &&
            schema::map::key<int64> ("value") &&
            schema::map::key<satoshis_per_byte> ("fee_rate", p.SpendOptions.FeeRate) &&
            schema::map::key<Bitcoin::satoshi> ("min_change_value", p.SpendOptions.MinChangeSats) &&
            schema::map::key<std::string> ("unit", "Bitcoin") && // must be Bitcoin for now.
            schema::map::key<double> ("max_redeem_proportion", p.SpendOptions.MaxRedeemProportion) &&
            schema::map::key<double> ("min_redeem_proportion", p.SpendOptions.MinRedeemProportion) &&
            schema::map::key<Bitcoin::satoshi> ("max_value_per_tx", p.SpendOptions.MaxSatsPerTx) &&
            schema::map::key<Bitcoin::satoshi> ("min_value_per_tx", p.SpendOptions.MinSatsPerTx) &&
            schema::map::key<double> ("mean_value_per_tx", p.SpendOptions.MeanSatsPerTx) &&
            schema::map::key<Bitcoin::satoshi> ("max_value_per_output", p.SpendOptions.MaxSatsPerOutput) &&
            schema::map::key<Bitcoin::satoshi> ("min_value_per_output", p.SpendOptions.MinSatsPerOutput) &&
            schema::map::key<double> ("mean_value_per_output", p.SpendOptions.MeanSatsPerOutput));

        if (unit != "Bitcoin" && unit != "BSV")
            if (http_method != net::HTTP::method::put)
                return error_response (405, m, problem::invalid_query,
                    "We do not support units other than Bitcoin SV for now.");

        // now we try to read who we are sending to.

        Bitcoin::address pay_to_address;
        HD::BIP_32::pubkey pay_to_xpub;

        const UTF8 *pay_to_param = query.contains ("pay_to");
        if (bool (pay_to_param)) {
            pay_to_address = Bitcoin::address {*pay_to_param};
            pay_to_xpub = HD::BIP_32::pubkey {*pay_to_param};
        } else return error_response (400, m, problem::invalid_query, "required parameter 'pay_to' not present");
/*
        if (pay_to_address.valid ()) {

        Cosmos::spend_options spend_options = p.SpendOptions;
        maybe<net::HTTP::response> error = read_spend_options (spend_options, query);
        if (bool (error)) return *error;
            return error_response (501, m, problem::unimplemented);
        } else if (pay_to_xpub.valid ()) {
            return error_response (501, m, problem::unimplemented);
        } else return error_response (400, m, problem::invalid_query, "invalid parameter 'pay_to'");*/
    }

    if (m == method::RESTORE) {
        if (http_method != net::HTTP::method::put)
            return error_response (405, m, problem::invalid_method, "use put");

        return handle_restore (p, wallet_name, query, content_type, body);
    }

    return error_response (501, m, problem::unimplemented);

    if (m == method::BOOST) {
        if (http_method != net::HTTP::method::post)
            return error_response (405, m, problem::invalid_method, "use post");

        return error_response (501, m, problem::unimplemented);
    }

    if (m == method::SPLIT) {
        if (http_method != net::HTTP::method::post)
            return error_response (405, m, problem::invalid_method, "use post");

        return error_response (501, m, problem::unimplemented);
    }

    if (m == method::TAXES) {
        if (http_method != net::HTTP::method::get)
            return error_response (405, m, problem::invalid_method, "use get");

        return error_response (501, m, problem::unimplemented);
    }

    if (m == method::ENCRYPT_KEY) {
        if (http_method != net::HTTP::method::post)
            return error_response (405, m, problem::invalid_method, "use post");

        return error_response (501, m, problem::unimplemented);
    }

    if (m == method::DECRYPT_KEY) {
        if (http_method != net::HTTP::method::post)
            return error_response (405, m, problem::invalid_method, "use post");

        return error_response (501, m, problem::unimplemented);
    }

    return error_response (501, m, problem::unimplemented);
}

net::HTTP::response favicon () {
    // favicon.ico embedded as byte array
    static const bytes favicon_ico {
        0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x10, 0x10, 0x00, 0x00, 0x01, 0x00, 0x04, 0x00, 0x28, 0x01,
        0x00, 0x00, 0x16, 0x00, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x10,
        0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x01, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC4,
        0x0E, 0x00, 0x00, 0x12, 0x0B, 0x00, 0x00, 0x12, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x10, 0x00, 0x18, 0x00, 0x00, 0x00, 0x34, 0x34, 0x34, 0x00,
        0x00, 0x00, 0x00, 0xFF, 0x22, 0x8B, 0x22, 0x8B, 0x22, 0x8B, 0x22, 0x8B, 0x22, 0x8B, 0x22, 0x8B,
        0x22, 0x8B, 0x22, 0x8B, 0x22, 0x8B, 0x22, 0x8B, 0x22, 0x8B, 0x22, 0x8B, 0x22, 0x8B, 0x22, 0x8B,
        0x22, 0x8B, 0x22, 0x8B, 0x22, 0x8B, 0x22, 0x8B, 0x00, 0x00, 0x00, 0x00
    };

    return net::HTTP::response (200,
        {{"content-type", "image/x-icon"}},
        favicon_ico);
}

net::HTTP::response handle_generate (server &p,
    Diophant::symbol wallet_name, map<UTF8, UTF8> query,
    const maybe<net::HTTP::content> &content_type,
    const data::bytes &body) {

    generate_request_options gen {wallet_name, query, content_type, body};

    generate_error checked = gen.check ();

    if (checked != generate_error::valid)
        switch (checked) {
            case generate_error::words_vs_mnemonic_style:
                return error_response (400, method::GENERATE, problem::invalid_parameter,
                    "If derivation_style is BIP_44, then coin_type must be povided and not be 'none'");
            case generate_error::centbee_vs_coin_type:
                return error_response (400, method::GENERATE, problem::invalid_parameter,
                    "If derivation_style is CentBee, then coin_type, if provided, must be 'none'");
            case generate_error::neither_style_nor_coin_type:
                return error_response (400, method::GENERATE, problem::invalid_parameter,
                    "Either derivation_style or coin_type must be provided");
            case generate_error::mnemonic_vs_number_of_words:
                return error_response (400, method::GENERATE, problem::invalid_parameter,
                    "if mnemonic is none, then number_of_words must not be present");
            case generate_error::invalid_number_of_words:
                return error_response (400, method::GENERATE, problem::invalid_parameter,
                    string::write ("'number_of_words' should be either 12 or 24 and instead is ", gen.number_of_words ()));
            case generate_error::zero_accounts:
                return error_response (400, method::GENERATE, problem::invalid_parameter,
                    "cannot have zero accounts");
            default:
                return error_response (500, method::GENERATE, problem::failed);
        }


    // we generate the wallet the same way regardless of whether the user wants words.
    bytes wallet_entropy {};
    wallet_entropy.resize (gen.number_of_words () * 4 / 3);

    // generate master key.
    data::crypto::random::get () >> wallet_entropy;
    std::string wallet_words = HD::BIP_39::generate (wallet_entropy);

    generate_error result = generate (p.DB, wallet_words, gen);

    if (result != generate_error::valid)
        switch (result) {
            case generate_error::wallet_already_exists:
                return error_response (500, method::GENERATE, problem::failed,
                    string::write ("wallet ", gen.name (), " already exists"));
            default:
                return error_response (500, method::GENERATE, problem::failed);
        }

    if (gen.mnemonic_style () != ::mnemonic_style::none)
        return string_response (wallet_words);

    return ok_response ();

}

template <typename X>
X inline set_with_default (const maybe<X> &opt, const X &def) {
    return bool (opt) ? *opt : def;
}

net::HTTP::response handle_restore (server &p,
    Diophant::symbol wallet_name, map<UTF8, UTF8> query,
    const maybe<net::HTTP::content> &content_type,
    const data::bytes &body) {

    // TODO: if words fail, we need an option to try to fix errors.
    // TODO: need option for gussing coin type.
    auto [
        derivation_options,
        key_options,
        accounts_param,
        max_lookup_param] = schema::validate<> (query,
        (schema::map::key<restore_wallet_type> ("wallet_type") || (
            *schema::map::key<wallet_style> ("wallet_style") &&
            *schema::map::key<derivation_style> ("derivation_style") &&
            *(schema::map::key<coin_type> ("coin_type") ||
                schema::map::key<bool> ("guess_coin_type")))) &&
        (schema::map::key<UTF8> ("key") &&
            *schema::map::key<master_key_type> ("key_type") ||
            (*(schema::map::key<UTF8> ("password") ||
                schema::map::key<uint16> ("CentBee_PIN") ||
                schema::map::key<bool> ("guess_CentBee_PIN")) &&
                (schema::map::key<UTF8> ("mnemonic") ||
                    schema::map::key<bytes> ("entropy")) &&
                    *schema::map::key<mnemonic_style> ("mnemonic_style"))) &&
        *schema::map::key<uint32> ("accounts") &&
        *schema::map::key<uint32> ("max_lookup"));

    uint32 total_accounts = set_with_default (accounts_param, uint32 {1});
    uint32 max_lookup = set_with_default (max_lookup_param, uint32 {25});

    // somehow we need to get this key to be valid.
    HD::BIP_32::pubkey pk;

    // we may have a secret key provided.
    maybe<HD::BIP_32::secret> sk;

    // we need to figure out what kind of key it is.
    master_key_type KeyType {master_key_type::invalid};

    wallet_style WalletStyle {wallet_style::invalid};

    maybe<coin_type> CoinType {};
    bool guess_coin_type {false};

    {

        switch (derivation_options.index ()) {
            case 0: {
                restore_wallet_type restore_type = std::get<0> (derivation_options);
                WalletStyle = wallet_style::BIP_44;
                switch (restore_type) {
                    case restore_wallet_type::Money_Button: {
                        CoinType = HD::BIP_44::moneybutton_coin_type;
                    } break;
                    case restore_wallet_type::RelayX: {
                        CoinType = HD::BIP_44::relay_x_coin_type;
                    } break;
                    case restore_wallet_type::Simply_Cash: {
                        CoinType = HD::BIP_44::simply_cash_coin_type;
                    } break;
                    case restore_wallet_type::Electrum_SV: {
                        CoinType = HD::BIP_44::electrum_sv_coin_type;
                    } break;
                    case restore_wallet_type::CentBee: {
                        CoinType = coin_type {};
                    } break;
                    default: throw data::method::unimplemented {"wallet types for GENERATE method"};
                }
            } break;
            case 1: {
                // we need either derivation_style_option or coin_type_option
                // and if we have both, they need to be consistent.
                auto [wallet_style_option, derivation_style_option, coin_type_option] = std::get<1> (derivation_options);

                if (coin_type_option) {
                    // coin type was provided
                    if (coin_type_option->index () == 0)
                        CoinType = std::get<0> (*coin_type_option);
                    else guess_coin_type = std::get<1> (*coin_type_option);
                }

                if (bool (derivation_style_option)) {
                    if (*derivation_style_option == derivation_style::BIP_44) {
                        if (!bool (CoinType) || !bool (*CoinType))
                            return error_response (400, method::RESTORE, problem::invalid_parameter,
                                "If derivation_style is BIP_44, then coin_type must be povided and not be 'none'");
                    } else {
                        if (!bool (CoinType)) {
                            if (guess_coin_type)
                                return error_response (400, method::RESTORE, problem::invalid_query,
                                    "Do not need to guess coin type since we have derivation_style=centbee");
                            else CoinType = coin_type {};
                        } if (bool (CoinType) && bool (*CoinType))
                            return error_response (400, method::RESTORE, problem::invalid_parameter,
                                "If derivation_style is CentBee, then coin_type, if provided, must be 'none'");
                    }
                } else if (!bool (CoinType))
                    return error_response (400, method::RESTORE, problem::invalid_parameter,
                        "Either derivation_style or coin_type must be provided");

                if (bool (wallet_style_option)) WalletStyle = *wallet_style_option;

            } break;
            default: throw data::exception {} << "Should not be able to get here";
        }

        // if we do not know coin type at this point, then guess coin type must be set.
        if (!CoinType && !guess_coin_type)
            return error_response (400, method::RESTORE, problem::invalid_query,
                "Please provide a coin_type parameter or set guess_coin_type to true");

        bool derive_from_mnemonic = key_options.index () == 1;
        if (derive_from_mnemonic) {
            auto [password_option, mnemonic_or_entropy, mnemonic_style_option] = std::get<1> (key_options);

            mnemonic_style MnemonicStyle = set_with_default (mnemonic_style_option, mnemonic_style::BIP_39);

            // now we need some way to calculate the words.
            data::UTF8 words;

            switch (mnemonic_or_entropy.index ()) {
                case 0: {
                    words = std::get<0> (mnemonic_or_entropy);
                } break;
                case 1: {
                    auto entropy = std::get<1> (mnemonic_or_entropy);

                    if (MnemonicStyle == mnemonic_style::BIP_39) {
                        words = HD::BIP_39::generate (entropy);
                    } else {
                        words = encoding::UTF8::encode (HD::Electrum_SV::generate (entropy));
                    }
                }
            }

            UTF8 password {};
            bool guess_CentBee_PIN {false};
            if (bool (password_option)) {
                switch (password_option->index ()) {
                    case 0: {
                        password = std::get<0> (*password_option);
                    } break;
                    case 1: {
                        uint16 centbee_PIN = std::get<1> (*password_option);

                        // must be in the correct range.
                        if (centbee_PIN > 9999)
                            return error_response (400, method::RESTORE, problem::invalid_query, "Invalid CentBee PIN range");

                        password = std::to_string (centbee_PIN);
                    } break;
                    case 2: {
                        guess_CentBee_PIN = std::get<2> (*password_option);
                        if (!guess_CentBee_PIN)
                            return error_response (400, method::RESTORE, problem::invalid_query,
                                "Please set guess_centbee_pin to true and we will attempt to figure it out.");

                    }

                    if (password_option->index () != 1) {
                        if (KeyType == master_key_type::invalid) KeyType = master_key_type::BIP44_master;
                        else if (KeyType != master_key_type::BIP44_master)
                            return error_response (400, method::RESTORE, problem::invalid_query,
                                "CentBee option implies that key_type must be BIP_44_master");

                        if (WalletStyle == wallet_style::invalid) WalletStyle = wallet_style::BIP_44;
                        else if (WalletStyle != wallet_style::BIP_44)
                            return error_response (400, method::RESTORE, problem::invalid_query,
                                "CentBee option implies that wallet_style must be BIP_44");
                    }
                }
            }
            // In some cases, we can guess the wallet style from other options.
            // TODO support single address style.
            switch (WalletStyle) {
                case wallet_style::invalid: {
                    // assume bip 44 in this case.
                    if (derive_from_mnemonic) {
                        WalletStyle == wallet_style::BIP_44;
                        break;
                    } else return error_response (400, method::RESTORE, problem::missing_parameter,
                        "Need to set parameter style");
                }
                case wallet_style::single_address:
                case wallet_style::HD_sequence:
                    return error_response (400, method::RESTORE, problem::invalid_query,
                        "derivation from mnemonic is incompatible with single address or hd sequence wallet styles.");
            }

            // now we try to generate seed from words.
            HD::seed seed;

            if (guess_CentBee_PIN) throw data::method::unimplemented {"method::RESTORE: guess centbee pin"};
            else if (MnemonicStyle == mnemonic_style::BIP_39) {
                if (!HD::BIP_39::valid (words))
                    return error_response (400, method::RESTORE, problem::invalid_query,
                        "Invalid mnemonic provided.");

                seed = HD::BIP_39::read (words, password);
            } else {
                if (!HD::Electrum_SV::valid (words))
                    return error_response (400, method::RESTORE, problem::invalid_query,
                        "Invalid mnemonic provided.");

                seed = HD::Electrum_SV::read (words, password);
            }

            // Now we generate keys from seed.
            sk = HD::BIP_32::secret::from_seed (seed);

            // derive pubkey from secret key.
            pk = sk->to_public ();

        } else {
            auto [key_option, type_option] = std::get<0> (key_options);

            if (Bitcoin::address perhaps_address {key_option}; perhaps_address.valid ()) {
                if (bool (type_option)) {
                    if (*type_option != master_key_type::single_address)
                        return error_response (400, method::RESTORE, problem::invalid_query,
                            "Restoring a bitcoin address is only compatible with key type single_address");
                } else KeyType = master_key_type::single_address;

                throw data::method::unimplemented {"method::GENERATE, single address restore"};
            } else if (Bitcoin::pubkey perhaps_pubkey {key_option}; perhaps_pubkey.valid ()) {
                if (bool (type_option)) {
                    if (*type_option != master_key_type::single_address)
                        return error_response (400, method::RESTORE, problem::invalid_query,
                            "Restoring a bitcoin pubkey is only compatible with key type single_address");
                } else KeyType = master_key_type::single_address;

                throw data::method::unimplemented {"method::GENERATE, single address restore"};
            } else if (Bitcoin::secret perhaps_WIF {key_option}; perhaps_WIF.valid ()) {
                if (bool (type_option)) {
                    if (*type_option != master_key_type::single_address)
                        return error_response (400, method::RESTORE, problem::invalid_query,
                            "Restoring a bitcoin WIF is only compatible with key type single_address");
                } else KeyType = master_key_type::single_address;

                throw data::method::unimplemented {"method::GENERATE, single address restore"};
            } else if (HD::BIP_32::pubkey perhaps_HD_pubkey {key_option}; perhaps_HD_pubkey.valid ()) {
                if (bool (type_option)) {
                    if (*type_option == master_key_type::single_address)
                        return error_response (400, method::RESTORE, problem::invalid_query,
                            "HD pubkey is incompatible with key type single_address");
                }

                pk = perhaps_HD_pubkey;
            } else if (HD::BIP_32::secret perhaps_secret {key_option}; perhaps_secret.valid ()) {
                if (bool (type_option)) {
                    if (*type_option == master_key_type::single_address)
                        return error_response (400, method::RESTORE, problem::invalid_query,
                            "HD secret is incompatible with key type single_address");
                }

                sk = perhaps_secret;
                pk = perhaps_secret.to_public ();
            }

            // TODO we have to make sure that key type, if provided, matches
            // what was provided in derivation options.

            // at this point, it is possible that WalletStyle is not set. Can we set it?
            switch (WalletStyle) {
                case wallet_style::invalid: {
                    switch (KeyType) {

                    }
                } break;
                case wallet_style::single_address:
                    if (KeyType != master_key_type::single_address)
                        return error_response (400, method::RESTORE, problem::invalid_query,
                            "derivation from mnemonic is incompatible with single address or hd sequence wallet styles.");
                case wallet_style::HD_sequence:
                    return error_response (400, method::RESTORE, problem::invalid_query,
                        "derivation from mnemonic is incompatible with single address or hd sequence wallet styles.");
            }

            throw data::method::unimplemented {"what to do if mnemonic isn't provided in method::RESTORE"};
        }
    }

    if (!bool (sk)) {
        throw data::method::unimplemented {"method RESTORE with no private key"};
    }

    // NOTE: we do not necessarily want to make a whole new wallet every time we restore.
    if (!p.DB.make_wallet (wallet_name))
        return error_response (500, method::RESTORE, problem::failed,
            string::write ("wallet ", wallet_name, " already exists"));

    // set master key
    switch (KeyType) {
        case master_key_type::BIP44_master: {

        } break;
        default: throw data::method::unimplemented {"method RESTORE: key types other than bip 44 master"};
    }

    if (!bool (CoinType)) throw data::method::unimplemented {"method RESTORE: guess coin type"};

    coin_type CoinTypeFinal = *CoinType;

    // if coin type is none, then we make a centbee-style account.
    list<uint32> root_derivation = bool (CoinTypeFinal) ?
        list<uint32> {HD::BIP_44::purpose, *CoinTypeFinal}:
        list<uint32> {HD::BIP_44::purpose};

    string account_name {"account"};

    // TODO set this;
    key_expression master_key_expr;

    // set a sequence for the accounts.
    p.DB.set_wallet_sequence (wallet_name, account_name,
        key_sequence {
            key_expression {string::write ("(", master_key_expr, ") ", write_derivation (root_derivation))},
            key_derivation {string::write ("@ key index -> key / harden (index)")}},
        total_accounts);

    if (WalletStyle == wallet_style::BIP_44_plus || WalletStyle == wallet_style::experimental)
        throw data::method::unimplemented {"restore extended bip 44 types"};

    // generate all the accounts.
    for (uint32 account_number = 0; account_number < total_accounts; account_number++) {

        list<uint32> account_derivation = root_derivation << HD::BIP_32::harden (account_number);

        list<uint32> receive_derivation = account_derivation << 0;
        list<uint32> change_derivation = account_derivation << 1;

        key_expression receive_pubkey = key_expression {sk->derive (receive_derivation).to_public ()};
        key_expression change_pubkey = key_expression {sk->derive (change_derivation).to_public ()};

        p.DB.set_to_private (receive_pubkey,
            key_expression {string::write ("(", master_key_expr, ") ",
                write_derivation (receive_derivation))});

        p.DB.set_to_private (change_pubkey,
            key_expression {string::write ("(", master_key_expr, ") ",
                write_derivation (change_derivation))});

        string receive_name = string::write ("receive_", account_number);
        string change_name = string::write ("change_", account_number);

        // note that each of these returns a regular pubkey rather than an xpub.
        p.DB.set_wallet_sequence (wallet_name, receive_name,
            key_sequence {
                receive_pubkey,
                key_derivation {"@ key index -> pubkey (key / index)"}}, 0);

        p.DB.set_wallet_sequence (wallet_name, change_name,
            key_sequence {
                change_pubkey,
                key_derivation {"@ key index -> pubkey (key / index)"}}, 0);

    }

    // TODO restore the wallet
    throw data::method::unimplemented {"method RESTORE"};

}
