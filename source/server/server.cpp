#include "server.hpp"
#include "method.hpp"

#include <Diophant/parse.hpp>
#include <Diophant/symbol.hpp>

#include <gigamonkey/schema/random.hpp>

#include <gigamonkey/schema/bip_39.hpp>

void server::add_entropy (const bytes &b) {
    if (!FixedEntropy) FixedEntropy = std::make_shared<crypto::fixed_entropy> (b);
    else {
        FixedEntropy->Entropy = b;
        FixedEntropy->Position = 0;
    }

    if (!Entropy) Entropy = std::make_shared<crypto::entropy_sum> (FixedEntropy, std::make_shared<Gigamonkey::bitcoind_entropy> ());
}

server::server (const options &o) {
    SpendOptions = o.spend_options ();
    DB = load_DB (o.db_options ());
}

crypto::entropy &server::get_secure_random () {

    if (!SecureRandom) {

        if (!Entropy) throw data::crypto::entropy::fail {};

        SecureRandom = std::make_shared<crypto::NIST::DRBG> (crypto::NIST::DRBG::Hash, *Entropy, std::numeric_limits<uint32>::max ());
    }

    return *SecureRandom.get ();

}

crypto::entropy &server::get_casual_random () {

    if (!CasualRandom) {
        uint64 seed;
        get_secure_random () >> seed;

        CasualRandom = std::make_shared<crypto::linear_combination_random> (256,
            std::static_pointer_cast<crypto::entropy> (std::make_shared<crypto::std_random<std::default_random_engine>> (seed)),
            std::static_pointer_cast<crypto::entropy> (SecureRandom));
    }

    return *CasualRandom.get ();

}

net::HTTP::response version_response () {
    std::stringstream ss;
    version (ss);
    auto res = net::HTTP::response (200, {{"content-type", "text/plain"}}, bytes (data::string (ss.str ())));
    return res;
}

namespace secp256k1 = Gigamonkey::secp256k1;
namespace HD = Gigamonkey::HD;

using key_expression = Cosmos::key_expression;
using key_derivation = Cosmos::key_derivation;

maybe<bool> read_bool (const std::string &utf8) {
    std::string sanitized = sanitize (utf8);
    if (sanitized == "true") return true;
    else if (sanitized == "false") return false;
    else return {};
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
            co_return error_response (405, method::HELP, problem::invalid_method, "use get");

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
            co_return error_response (405, method::SHUTDOWN, problem::invalid_method, "use put");

        Shutdown = true;
        co_return ok_response ();
    }

    if (m == method::ADD_ENTROPY) {
        if (req.Method != net::HTTP::method::post)
            co_return error_response (405, method::ADD_ENTROPY, problem::invalid_method, "use post");

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

    } catch (const crypto::entropy::fail &) {
        co_return error_response (500, m, problem::need_entropy);
    } catch (const Diophant::parse_error &) {
        co_return error_response (400, m, problem::invalid_expression);
    }
}

net::HTTP::response invert_hash (server &p, 
    net::HTTP::method http_method, map<UTF8, UTF8> query,
    const maybe<net::HTTP::content> &content_type,
    const data::bytes &body); 

net::HTTP::response process_method (
    server &p, net::HTTP::method http_method, method m,
    map<UTF8, UTF8> query,
    const maybe<net::HTTP::content> &content_type,
    const data::bytes &body) {

    if (m == method::INVERT_HASH) return invert_hash (p, http_method, query, content_type, body); 

    // Associate a secret key with a name. The key could be anything; private, public, or symmetric. 
    if (m == method::KEY) {

        const UTF8 *key_name_param = query.contains ("name");
        if (!bool (key_name_param))
            return error_response (400, m, problem::missing_parameter, "required parameter 'name' not present");

        Diophant::symbol key_name {*key_name_param};

        // make sure the name is a valid symbol name
        if (!key_name.valid ()) 
            return error_response (400, m, problem::invalid_parameter, "parameter 'name' must be alpha alnum+");

        enum class generation_method {
            random,
            expression
        } Method;
            
        {
            const UTF8 *method_val = query.contains ("method");
            if (bool (method_val)) {
                std::string method_san = sanitize (*method_val);
                if (method_san == "random") Method = generation_method::random;
                else if (method_san == "expression") Method = generation_method::expression;
                else return error_response (400, method::KEY, problem::invalid_query, "invalid parameter 'type'");
            }

            if (bool (content_type)) {
                if (bool (method_val) && Method == generation_method::random)
                    return error_response (400, method::KEY, problem::invalid_query);
                Method = generation_method::expression; 
            } else {
                if (bool (method_val) && Method == generation_method::expression)
                    return error_response (400, method::KEY, problem::invalid_query);
                Method = generation_method::random;
            }
        }

        // after this point, Method is set as some definite value and is not garbage. 

        enum class key_type {
            unset,
            secp256k1,
            WIF,
            xpriv
        } KeyType {key_type::unset};

        const UTF8 *key_type = query.contains ("type");
        if (bool (key_type)) {
            if (Method == generation_method::expression) 
                return error_response (400, method::KEY, problem::invalid_query);
            std::string key_type_san = sanitize (*key_type);
            if (key_type_san == "secp256k1") KeyType = key_type::secp256k1;
            else if (key_type_san == "wif") KeyType = key_type::WIF;
            else if (key_type_san == "xpriv") KeyType = key_type::xpriv;
            else return error_response (400, method::KEY, problem::invalid_query, "invalid parameter 'type'");
        }

        if (Method == generation_method::expression && KeyType != key_type::unset)
            return error_response (400, method::KEY, problem::invalid_query);

        Bitcoin::net net = Bitcoin::net::Main;
        const UTF8 *net_type_param = query.contains ("net");
        if (bool (net_type_param)) {
            std::string net_type_san = sanitize (*net_type_param);
            if (net_type_san == "main") net = Bitcoin::net::Main;
            else if (net_type_san == "test") net = Bitcoin::net::Test;
            else return error_response (400, m, problem::invalid_query, "invalid parameter 'net'");
        }
        
        if (Method == generation_method::expression && bool (net_type_param))
            return error_response (400, method::KEY, problem::invalid_query);

        bool compressed = true;
        const UTF8 *compressed_param = query.contains ("compressed");
        if (bool (compressed_param)) {
            maybe<bool> maybe_compressed =  read_bool (*compressed_param);
            if (!maybe_compressed) return error_response (400, m, problem::invalid_query, "invalid parameter 'compressed'");
            compressed = *maybe_compressed;
        }
        
        if (bool (compressed_param) && (Method == generation_method::expression || 
            (KeyType != key_type::WIF && KeyType != key_type::xpriv)))
            return error_response (400, method::KEY, problem::invalid_query);

        if (http_method == net::HTTP::method::get) {
            if (KeyType != key_type::unset) 
                return error_response (400, method::KEY, problem::invalid_query);
            
            if (bool (net_type_param))
                return error_response (400, method::KEY, problem::invalid_query);
            
            if (bool (compressed_param))
                return error_response (400, method::KEY, problem::invalid_query);
            
            if (bool (content_type))
                return error_response (400, method::KEY, problem::invalid_query);
            
            return error_response (501, m, problem::unimplemented);

        } else if (http_method == net::HTTP::method::post) {

            key_expression key_expr;

            if (Method == generation_method::expression) {
                if (KeyType != key_type::unset)
                    return error_response (400, m, problem::invalid_query, "key type will be inferred from the expression provided");

                if (!bool (content_type) || *content_type != net::HTTP::content::type::text_plain)
                    return error_response (400, m, problem::invalid_content_type, "expected content-type:text/plain");

                key_expr = key_expression {data::string (body)};

            } else {
                if (KeyType != key_type::unset)
                    return error_response (400, m, problem::invalid_query, "need a key type to tell us what to generate");

                if (bool (content_type))
                    return error_response (400, m, problem::invalid_query, "no body when we generate random keys");

                crypto::entropy &random = p.get_secure_random ();
                secp256k1::secret key;
                random >> key.Value;

                switch (KeyType) {
                    case (key_type::WIF): {
                        key_expr = key_expression {Bitcoin::secret {net, key, compressed}};
                    } break;
                    case (key_type::xpriv): {
                        HD::chain_code x;
                        random >> x;
                        key_expr = key_expression (HD::BIP_32::secret {key, x, net});
                    } break;
                    default: {
                        key_expr = key_expression {key};
                    }
                }
            }

            if (p.DB->set_key (key_name, key_expr)) return ok_response ();
            return error_response (500, m, problem::failed, "could not create key");
        } else return error_response (405, m, problem::invalid_method, "use get or post");

    }

    if (m == method::TO_PRIVATE) {
        if (http_method == net::HTTP::method::post) {

            if (!bool (content_type) || *content_type != net::HTTP::content::type::text_plain)
                return error_response (400, m, problem::invalid_content_type, "expected content-type:text/plain");

            const UTF8 *key_name_param = query.contains ("name");
            if (!bool (key_name_param))
                return error_response (400, m, problem::missing_parameter, "required parameter 'name' not present");

            Diophant::symbol key_name {*key_name_param};

            if (!key_name.valid ())
                return error_response (400, m, problem::invalid_parameter, "invalid parameter 'name'");

            key_expression key_expr {data::string (body)};

            if (p.DB->set_to_private (key_name, key_expr)) return ok_response ();
            return error_response (500, m, problem::failed, "could not set private key");
        } else if (http_method != net::HTTP::method::get) {
            return error_response (501, m, problem::unimplemented, "GET TO_PRIVATE");
        } else return error_response (405, m, problem::invalid_method, "use post or get");
    }

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
    const maybe<net::HTTP::content> &content,
    const data::bytes &body) {

    // make sure the wallet name is a valid string.
    if (!wallet_name.valid ()) return error_response (400, m, problem::invalid_wallet_name, "name argument must be alpha alnum+");

    if (m == method::MAKE_WALLET) {
        if (http_method != net::HTTP::method::post)
            return error_response (405, m, problem::invalid_method, "use post");

        bool created = p.DB->make_wallet (wallet_name);
        if (created) return ok_response ();
        else return error_response (500, m, problem::failed, "could not create wallet");
    }

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

        // first we will determine whether we will return a mnemonic
        bool use_mnemonic = false;
        const UTF8 *use_mnemonic_param = query.contains ("mnemonic");
        if (bool (use_mnemonic_param)) {
            maybe<bool> maybe_mnemonic = read_bool (*use_mnemonic_param);
            if (!maybe_mnemonic) return error_response (400, m, problem::invalid_query, "invalid parameter 'mnemonic'");
            use_mnemonic = *maybe_mnemonic;
        }

        enum class mnemonic_style {
            none,
            BIP_39,
            Electrum_SV
        } MnemonicStyle {mnemonic_style::none};

        const UTF8 *mnemonic_style_param = query.contains ("mnemonic_style");
        if (bool (mnemonic_style_param)) {
            std::string sanitized = sanitize (*mnemonic_style_param);
            if (sanitized == "electrumsv") MnemonicStyle = mnemonic_style::Electrum_SV;
            else if (sanitized == "bip39") MnemonicStyle = mnemonic_style::BIP_39;
            else return error_response (400, m, problem::invalid_query, "invalid parameter 'mnemonic_style'");
        }

        uint32 number_of_words = 24;
        const UTF8 *number_of_words_param = query.contains ("number_of_words");
        if (bool (number_of_words_param)) {
            if (!parse_uint32 (*number_of_words_param, number_of_words))
                return error_response (400, m, problem::invalid_parameter, "'number_of_words' should be either 12 or 24");
        }

        if (number_of_words != 12 && number_of_words != 24)
            return error_response (400, m, problem::invalid_parameter, "'number_of_words' should be either 12 or 24");

        enum class wallet_style {
            BIP_44,
            BIP_44_plus,
            experimental
        } WalletStyle {wallet_style::BIP_44};

        const UTF8 *wallet_style_param = query.contains ("wallet_style");
        if (bool (wallet_style_param)) {
            std::string sanitized = sanitize (*wallet_style_param);
            if (sanitized == "bip44") WalletStyle = wallet_style::BIP_44;
            else if (sanitized == "bip44plus") WalletStyle = wallet_style::BIP_44_plus;
            // experimental means me generate two secp256k1 keys and use one as the chain code.
            // this allows us to use bip 32 and hd stuff and future protocols. 
            else if (sanitized == "experimental") WalletStyle = wallet_style::experimental;
            else return error_response (400, m, problem::invalid_query, "'wallet_style'");
        }

        uint32 coin_type = 0;
        const UTF8 *coin_type_param = query.contains ("coin_type");
        if (bool ()) {
            if (!parse_uint32 (*number_of_words_param, number_of_words))
                return error_response (400, m, problem::invalid_parameter, "'number_of_words'");
        }

        if (MnemonicStyle == mnemonic_style::Electrum_SV || WalletStyle == wallet_style::experimental)
            return error_response (501, m, problem::unimplemented);

        // we generate the wallet the same way regardless of whether the user wants words.
        bytes wallet_entropy {};
        wallet_entropy.resize (number_of_words * 4 / 3);

        p.get_secure_random () >> wallet_entropy;
        std::string wallet_words = HD::BIP_39::generate (wallet_entropy);

        auto master = HD::BIP_32::secret::from_seed (HD::BIP_39::read (wallet_words));
        auto account = HD::BIP_44::from_root (master, coin_type, 0).to_public ();

        key_expression master_key_expr {master};
        key_expression account_key_expr {account};
        key_expression account_derivation {static_cast<std::string> (master_key_expr) + " / `44 / `" + std::to_string (coin_type) + " / `0"};

        std::string master_key_name = static_cast<std::string> (wallet_name) + "_master";
        std::string account_name = static_cast<std::string> (wallet_name) + "_account_0";

        p.DB->set_key (master_key_name, master_key_expr);
        p.DB->set_key (account_name, account_key_expr);
        p.DB->set_to_private (account_name, account_derivation);

        p.DB->set_wallet_derivation (wallet_name, "receive",
            database::derivation {key_derivation {"@ name index -> name / 0 / index"}, account_name});

        p.DB->set_wallet_derivation (wallet_name, "change",
            database::derivation {key_derivation {"@ name index -> name / 1 / index"}, account_name});

        if (WalletStyle == wallet_style::BIP_44_plus)
            p.DB->set_wallet_derivation (wallet_name, "receivex",
                database::derivation {key_derivation {"@ name index -> name / 0 / `index"}, account_name});

        if (use_mnemonic) return JSON_response (JSON {wallet_words});
        return ok_response ();

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
                p.DB->set_key (next_secret.encode (), next_expression);
                Bitcoin::pubkey next_pubkey = next_secret.to_public ();
                p.DB->set_to_private (string (next_pubkey), next_expression);
                net = next_secret.Network;
            // cold wallet
            } else if (next_pubkey = Bitcoin::pubkey (next_key); next_pubkey.valid ()) {
                p.DB->set_key (string (next_pubkey), next_expression);
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
