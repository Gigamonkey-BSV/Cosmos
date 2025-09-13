#include "server.hpp"
#include "method.hpp"
#include "invert_hash.hpp"
#include "key.hpp"
#include "to_private.hpp"
#include "generate.hpp"

#include <Diophant/parse.hpp>
#include <Diophant/symbol.hpp>

#include <gigamonkey/schema/random.hpp>

#include <data/tools/map_schema.hpp>

void server::add_entropy (const bytes &b) {
    if (!FixedEntropy) FixedEntropy = std::make_shared<data::fixed_entropy> (b);
    else {
        FixedEntropy->Entropy = b;
        FixedEntropy->Position = 0;
    }

    if (!Entropy) Entropy = std::make_shared<data::entropy_sum> (FixedEntropy, std::make_shared<Gigamonkey::bitcoind_entropy> ());
}

server::server (const options &o) {
    SpendOptions = o.spend_options ();
    DB = load_DB (o.db_options ());
}

data::entropy &server::get_secure_random () {

    if (!SecureRandom) {

        if (!Entropy) throw data::entropy::fail {};

        SecureRandom = std::make_shared<crypto::NIST::DRBG> (crypto::NIST::DRBG::Hash, *Entropy, std::numeric_limits<uint32>::max ());
    }

    return *SecureRandom.get ();

}

data::entropy &server::get_casual_random () {

    if (!CasualRandom) {
        uint64 seed;
        get_secure_random () >> seed;

        CasualRandom = std::make_shared<data::linear_combination_random> (256,
            std::static_pointer_cast<data::entropy> (std::make_shared<data::std_random<std::default_random_engine>> (seed)),
            std::static_pointer_cast<data::entropy> (SecureRandom));
    }

    return *CasualRandom.get ();

}

net::HTTP::response version_response () {
    std::stringstream ss;
    version (ss);
    auto res = net::HTTP::response (200, {{"content-type", "text/plain"}}, bytes (data::string (ss.str ())));
    return res;
}

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

awaitable<net::HTTP::response> server::operator () (const net::HTTP::request &req) {

    std::cout << "Responding to request " << req << std::endl;
    list<UTF8> path = req.Target.path ().read ('/');

    // if no API method was called, serve the GUI. 
    if (size (path) == 0) co_return HTML_JS_UI_response ();

    // the first part of the path is always "" as long as we use "/" as a delimiter 
    // because a path always begins with "/". 
    path = rest (path);

    if (size (path) == 0 || path[0] == "") co_return HTML_JS_UI_response ();

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

        maybe<net::HTTP::content> content_type = req.content_type ();
        if (!bool (content_type) || (
            *content_type != net::HTTP::content::type::application_octet_stream &&
            *content_type != net::HTTP::content::type::text_plain))
            co_return error_response (400, method::ADD_ENTROPY, problem::invalid_content_type, "expected content-type:application/octet-stream");

        add_entropy (req.Body);

        co_return ok_response ();
    }

    if (m == method::LIST_WALLETS) {
        if (req.Method != net::HTTP::method::get)
            co_return error_response (405, m, problem::invalid_method, "use get");
        JSON::array_t names;
        for (const std::string &name : DB->list_wallet_names ())
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

        if (path.size () < 2) co_return process_method (*this, req.Method, m, query, req.content_type (), req.Body);

        // make sure the wallet name is a valid string.
        Diophant::symbol wallet_name {path[1]};
        if (!data::valid (wallet_name)) co_return error_response (400, m, problem::invalid_wallet_name);

        co_return process_wallet_method (*this, req.Method, m, wallet_name, query, req.content_type (), req.Body);

    } catch (const data::entropy::fail &) {
        co_return error_response (500, m, problem::need_entropy);
    } catch (const Diophant::parse_error &w) {
        co_return error_response (400, m,
            problem::invalid_expression,
            // this is a pretty unhelpful message. However, hopefully we won't throw this in the future.
            string::write ("an invalid Diophant expression was generated: ", w.what ()));
    } catch (data::schema::missing_key mk) {
        co_return error_response (400, m, problem::missing_parameter, string::write ("missing parameter ", mk.Key));
    } catch (data::schema::invalid_value iv) {
        co_return error_response (400, m, problem::invalid_parameter, string::write ("invalid parameter ", iv.Key));
    } catch (data::schema::unknown_key uk) {
        co_return error_response (400, m, problem::invalid_query, string::write ("unknown parameter ", uk.Key));
    }
}

net::HTTP::response process_method (
    server &p, net::HTTP::method http_method, method m,
    map<UTF8, UTF8> query,
    const maybe<net::HTTP::content> &content_type,
    const data::bytes &body) {

    if (m == method::INVERT_HASH) return handle_invert_hash (p, http_method, query, content_type, body);

    if (m == method::TO_PRIVATE) handle_to_private (p, http_method, query, content_type, body);

    return error_response (500, m, problem::invalid_wallet_name, "wallet method called without wallet name");
}

bool parse_uint32 (const std::string &str, uint32_t &result) {
    try {
        size_t idx = 0;
        unsigned long val = std::stoul (str, &idx, 10);  // base 10

        // Extra characters after number
        if (idx != str.size ()) return false;

        // Out of range for uint32_t
        if (val > std::numeric_limits<uint32_t>::max ())
            return false;

        result = static_cast<uint32_t> (val);
        return true;

    } catch (const std::invalid_argument &) {
        return false;  // Not a number
    } catch (const std::out_of_range &) {
        return false;  // Too large for unsigned long
    }
}

maybe<net::HTTP::response> read_spend_options (Cosmos::spend_options &, map<UTF8, UTF8> query) {
    // TODO
    return {};
}

net::HTTP::response process_wallet_method (
    server &p, net::HTTP::method http_method, method m,
    Diophant::symbol wallet_name,
    map<UTF8, UTF8> query,
    const maybe<net::HTTP::content> &content_type,
    const data::bytes &body) {

    // make sure the wallet name is a valid string.
    if (!wallet_name.valid ()) return error_response (400, m, problem::invalid_wallet_name, "name argument must be alpha alnum+");

    // Associate a secret key with a name. The key could be anything; private, public, or symmetric.
    if (m == method::KEY) return handle_key (p, wallet_name, http_method, query, content_type, body);

    if (m == method::CREATE_WALLET) {
        if (http_method != net::HTTP::method::post)
            return error_response (405, m, problem::invalid_method, "use post");

        bool created = p.DB->make_wallet (wallet_name);
        if (created) return ok_response ();
        else return error_response (500, m, problem::failed, "could not create wallet");
    }

    // TODO there's something screwed up here.
    if (m == method::KEY_SEQUENCE) {
        if (http_method != net::HTTP::method::post)
            return error_response (405, m, problem::invalid_method, "use post");

        const UTF8 *key_name_param = query.contains ("key_name");
        if (!bool (key_name_param))
            return error_response (400, m, problem::invalid_query, "required parameter 'key_name' not present");

        Diophant::symbol key_name {*key_name_param};

        // make sure the name is a valid symbol name
        if (!data::valid (key_name)) return error_response (400, m, problem::invalid_parameter, "key_name parameter must be alpha alnum+");

        const UTF8 *name_param = query.contains ("name");
        if (!bool (name_param))
            return error_response (400, m, problem::invalid_query, "required parameter 'name' not present");

        Diophant::symbol name {*name_param};

        // make sure the name is a valid symbol name
        if (!data::valid (name)) return error_response (400, m, problem::invalid_parameter, "name parameter must be alpha alnum+");

        uint32 index = 0;
        const UTF8 *index_param = query.contains ("index");
        if (bool (index_param)) {
            if (!parse_uint32 (*index_param, index))
                return error_response (400, m, problem::invalid_parameter, "'index'");
        }

        key_derivation key_deriv {data::string (body)};

        if (p.DB->set_wallet_derivation (wallet_name, name, database::derivation {key_deriv, key_name, index})) return ok_response ();
        return error_response (500, m, problem::failed, "could not set wallet derivation");
    }

    if (m == method::VALUE) {
        if (http_method != net::HTTP::method::get)
            return error_response (405, m, problem::invalid_method, "use get");

        return value_response (p.DB->get_wallet_account (wallet_name).value ());
    }

    if (m == method::DETAILS) {
        if (http_method != net::HTTP::method::get)
            return error_response (405, m, problem::invalid_method, "use get");

        return JSON_response (p.DB->get_wallet_account (wallet_name).details ());
    }

    if (m == method::GENERATE) {

        if (http_method != net::HTTP::method::post)
            return error_response (405, m, problem::invalid_method, "use post");

        return handle_generate (p, wallet_name, query, content_type, body);
    }

    if (m == method::NEXT_ADDRESS || m == method::NEXT_XPUB) {
        if (http_method != net::HTTP::method::post)
            return error_response (405, m, problem::invalid_method, "use post");
        
        // look at the receive key sequence.
        Diophant::symbol sequence_name;

        {
            const UTF8 *sequence_name_param = query.contains ("sequence_name");
            if (bool (sequence_name_param)) sequence_name = *sequence_name_param;
            else sequence_name = m == method::NEXT_ADDRESS ? std::string {"receive"} : std::string {"receivex"};
        }

        if (!sequence_name.valid ()) return error_response (400, m, problem::invalid_parameter, "sequence_name");
        
        // do we have a sequence by this name? 
        maybe<database::derivation> seq = p.DB->get_wallet_derivation (wallet_name, sequence_name);
        if (!bool (seq)) return error_response (400, m, problem::invalid_query);

        // generate a new key. 
        key_expression next_expression = seq->increment ();
        Cosmos::key_expression next_key {Cosmos::evaluate (*p.DB, p.Machine, next_expression)};

        if (m == method::NEXT_ADDRESS) {
            Bitcoin::net net {Bitcoin::net::Main}; // just assume Main if we don't know.
            Bitcoin::secret next_secret (next_key);
            Bitcoin::pubkey next_pubkey {};

            // hot wallet
            if (next_secret.valid ()) {
                p.DB->set_key (wallet_name, next_secret.encode (), next_expression);
                Bitcoin::pubkey next_pubkey = next_secret.to_public ();
                p.DB->set_to_private (string (next_pubkey), next_expression);
                net = next_secret.Network;
            // cold wallet
            } else if (next_pubkey = Bitcoin::pubkey (next_key); next_pubkey.valid ()) {
                p.DB->set_key (wallet_name, string (next_pubkey), next_expression);
            } else return error_response (400, m, problem::failed);

            digest160 next_address_hash = next_pubkey.address_hash ();
            p.DB->set_invert_hash (next_address_hash, Cosmos::hash_function::Hash160, next_pubkey);

            Bitcoin::address next_address {net, next_address_hash};

            p.DB->set_wallet_unused (wallet_name, next_address);

            return JSON_response (JSON (std::string (next_address)));
        } else { // else it's NEXT_XPUB
            return error_response (501, m, problem::unimplemented);
        }
    }

    if (m == method::IMPORT) {
        if (http_method != net::HTTP::method::put)
            return error_response (405, m, problem::invalid_method, "use put");
        // we need to find addresses and xpubs that we need to check. 

        list<std::string> unused = p.DB->get_wallet_unused (wallet_name);

        list<digest160> unused_addresses;
        list<HD::BIP_32::pubkey> unused_pubkeys;

        for (const std::string &u : unused) {
            if (Bitcoin::address a {u}; a.valid ()) {

            } else if (HD::BIP_32::pubkey pp {u}; pp.valid ()) {
                
            } 
        }

        // first we need a function that recognizes output patterns. 
        return error_response (501, m, problem::unimplemented);
    }

    if (m == method::SPEND) {
        if (http_method != net::HTTP::method::post)
            return error_response (405, m, problem::invalid_method, "use post");

        Bitcoin::address pay_to_address;
        HD::BIP_32::pubkey pay_to_xpub;

        const UTF8 *pay_to_param = query.contains ("pay_to");
        if (bool (pay_to_param)) {
            pay_to_address = Bitcoin::address {*pay_to_param};
            pay_to_xpub = HD::BIP_32::pubkey {*pay_to_param};
        } else return error_response (400, m, problem::invalid_query, "required parameter 'pay_to' not present");

        if (pay_to_address.valid ()) {

        Cosmos::spend_options spend_options = p.SpendOptions;
        maybe<net::HTTP::response> error = read_spend_options (spend_options, query);
        if (bool (error)) return *error;
            return error_response (501, m, problem::unimplemented);
        } else if (pay_to_xpub.valid ()) {
            return error_response (501, m, problem::unimplemented);
        } else return error_response (400, m, problem::invalid_query, "invalid parameter 'pay_to'");
    }

    return error_response (501, m, problem::unimplemented);

    if (m == method::RESTORE) {
        if (http_method != net::HTTP::method::put)
            return error_response (405, m, problem::invalid_method, "use put");

        return error_response (501, m, problem::unimplemented);
    }

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
