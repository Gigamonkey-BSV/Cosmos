#include "server.hpp"
#include "../Cosmos.hpp"
#include "invert_hash.hpp"
#include "key.hpp"
#include "to_private.hpp"
#include "generate.hpp"
#include "restore.hpp"
#include "import.hpp"

#include <Diophant/parse.hpp>
#include <Diophant/symbol.hpp>

#include <gigamonkey/schema/random.hpp>
#include <gigamonkey/pay/BEEF.hpp>

#include <data/tools/schema.hpp>
#include <data/container.hpp>

#include <Cosmos/Diophant.hpp>

namespace schema = data::schema;
using BEEF = Gigamonkey::BEEF;

// for methods that operate on the database 
// without a particular wallet. 
net::HTTP::response process_method (
    server &,
    net::HTTP::method, method,
    map<UTF8, UTF8> query,
    const maybe<net::HTTP::content> &,
    const data::bytes &);

// for methods that operate on a specific wallet. 
net::HTTP::response process_wallet_method (
    server &p, net::HTTP::method http_method, method m,
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

        request_next_options opts (wallet_name, query);

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
