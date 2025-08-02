
#include <charconv>

#include <cstdlib>  // for std::getenv
#include <laserpants/dotenv/dotenv.h>

#include <gigamonkey/schema/random.hpp>
#include <gigamonkey/schema/bip_39.hpp>

#include <data/async.hpp>
#include <data/net/URL.hpp>
#include <data/net/JSON.hpp>
#include <data/net/HTTP_server.hpp>

#include <Cosmos/database.hpp>
#include <Cosmos/random.hpp>
#include <Cosmos/options.hpp>

#include <Cosmos/database/SQLite/SQLite.hpp>

#include <Diophant/parse.hpp>
#include <Diophant/parse/grammar.hpp>
#include <Diophant/symbol.hpp>

#include "Cosmos.hpp"

namespace Bitcoin = Gigamonkey::Bitcoin;
namespace secp256k1 = Gigamonkey::secp256k1;
namespace HD = Gigamonkey::HD;

struct options : io::arg_parser {
    options (io::arg_parser &&ap) : io::arg_parser {ap} {}

    // path to an env file containing program options. 
    maybe<filepath> env () const;

    // depricated. We changed things too much to be
    // able to use the old JSON database anymore.
    struct JSON_DB_options {
        std::string Path; // should be a directory
    };

    struct SQLite_options {
        maybe<filepath> Path {}; // missing for in-memory
    };

    // we were talking about using mongodb at one point 
    // but that is not supported at all right now. 
    struct MongoDB_options {
        net::URL URL;
        std::string UserName;
        std::string Password;
    }; // not yet supported.

    either<JSON_DB_options, SQLite_options, MongoDB_options> db_options () const;

    net::IP::TCP::endpoint endpoint () const;
    uint32 threads () const;
    bool offline () const;

    Cosmos::spend_options spend_options () const;
};

void run (const options &);

void signal_handler (int signal);

template <typename fun, typename ...args>
requires std::regular_invocable<fun, args...>
error catch_all (fun f, args... a) {
    try {
        std::invoke (std::forward<fun> (f), std::forward<args> (a)...);
    } catch (const method::unimplemented &x) {
        return error {7, x.what ()};
    } catch (const net::HTTP::exception &x) {
        return error {6, x.what ()};
    } catch (const JSON::exception &x) {
        return error {5, x.what ()};
    } catch (const CryptoPP::Exception &x) {
        return error {4, x.what ()};
    } catch (const exception &x) {
        return error {3, x.what ()};
    } catch (const std::exception &x) {
        return error {2, x.what ()};
    } catch (...) {
        return error {1};
    }

    return error {};
}

int main (int arg_count, char **arg_values) {
    std::signal (SIGINT, signal_handler);
    std::signal (SIGTERM, signal_handler);

    error err = catch_all (run, options {io::arg_parser {arg_count, arg_values}});

    if (bool (err)) {
        if (err.Message) std::cout << "Fail code " << err.Code << ": " << *err.Message << std::endl;
        else std::cout << "Fail code " << err.Code << "." << std::endl;
    }

    return err.Code;

}

boost::asio::io_context IO;

ptr<net::HTTP::server> Server;

std::atomic<bool> Shutdown {false};
std::vector<std::atomic<bool>> ThreadShutdown;

void shutdown () {
    if (Shutdown) return;
    std::cout << "\nShut down!" << std::endl;
    Shutdown = true;
    for (std::atomic<bool> &u : ThreadShutdown) u = true;
    if (Server != nullptr) Server->close ();
}

void signal_handler (int signal) {
    if (signal == SIGINT || signal == SIGTERM) shutdown ();
}

ptr<Cosmos::database> load_DB (const options &o);

struct processor {

    processor (const options &o);

    awaitable<net::HTTP::response> operator () (const net::HTTP::request &);

    void add_entropy (const bytes &);

    crypto::entropy &get_secure_random ();
    crypto::entropy &get_casual_random ();

    Cosmos::spend_options SpendOptions;

    ptr<Cosmos::database> DB;

    struct make_wallet_options {};

    bool make_wallet (const make_wallet_options &);

    awaitable<Bitcoin::satoshi> value (const UTF8 &wallet_name);

    struct wallet_info {
        Bitcoin::satoshi Value;
        uint32 Outputs;
        Bitcoin::satoshi MaxOutput;

        operator JSON () const;
    };

    awaitable<wallet_info> details (const UTF8 &wallet_name);
/*
    map<Bitcoin::outpoint, Bitcoin::output> account (const instruction &);

    tuple<Bitcoin::TXID, wallet_info> spend (const instruction &);*/

private:
    ptr<crypto::fixed_entropy> FixedEntropy {nullptr};
    ptr<crypto::entropy> Entropy {nullptr};
    ptr<crypto::NIST::DRBG> SecureRandom {nullptr};
    ptr<crypto::linear_combination_random> CasualRandom {nullptr};

};

void run (const options &program_options) {

    // path to an env file containing program options. 
    auto envpath = program_options.env ();

    // If the user provided a path, it is an error if it is not found. 
    // Otherwise we look in the default location but don't throw an
    // error if we don't find it. 
    if (bool (envpath)) {
        std::error_code ec;
        if (!std::filesystem::exists (*envpath, ec)) {
            throw exception {} << "file " << *envpath << " does not exist ";
        } else if (ec) {
            throw exception {} << "could not access file " << *envpath;
        }

        dotenv::init (envpath->c_str ());
    } else dotenv::init ();

    // for now, we require the endpoint to connect only to localhost.
    net::IP::TCP::endpoint endpoint = program_options.endpoint ();
    if (endpoint.address () != net::IP::address {"127.0.0.1"})
        throw exception {} << "This program is not ready to be connected to the Internet.  localhost is required";

    // I tried something with multiple threads but couldn't quite 
    // get it working. The problem was receiving the signal to 
    // shut down. That can come from any thread. 
    uint32 num_threads = program_options.threads ();
    if (num_threads < 1) throw exception {} << "We cannot run with zero threads. There is already one thread running to read this number.";
    if (num_threads > 1) throw exception {} << "We do not do multithreaded yet";
    uint32 extra_threads = num_threads - 1;

    std::cout << "loading server on " << endpoint << std::endl;
    Server = std::make_shared<net::HTTP::server>
        (IO.get_executor (), endpoint, processor {program_options});

    data::spawn (IO.get_executor (), [&] () -> awaitable<void> {
        while (co_await Server->accept ()) {}
        shutdown ();
    });

    // Logic for handling exceptions thrown by threads.
    std::exception_ptr stored_exception = nullptr;
    std::mutex exception_mutex;

    std::vector<std::thread> threads {};
    for (int i = 0; i < extra_threads; i++)
        threads.emplace_back ([&]() {
            int index = i;
            try {
                while (!ThreadShutdown[index]) {
                    if (stored_exception) shutdown ();
                    IO.run ();
                }

                shutdown ();
            } catch (...) {
                std::lock_guard<std::mutex> lock (exception_mutex);
                if (!stored_exception) {
                    stored_exception = std::current_exception (); // Capture first exception
                }
            }
        });

    while (!Shutdown) {
        if (stored_exception) shutdown ();
        IO.run ();
    }

    for (int i = 0; i < extra_threads; i++) threads[i].join ();

    if (stored_exception) std::rethrow_exception (stored_exception);

    IO.stop ();
}

enum class problem {
    unknown_method,
    invalid_method,
    invalid_content_type,
    need_entropy,
    invalid_name,
    invalid_query,
    invalid_target,
    invalid_expression,
    missing_parameter, 
    invalid_parameter,
    failed,
    unimplemented
};

std::ostream &operator << (std::ostream &, problem);

net::HTTP::response error_response (unsigned int status, meth m, problem, const std::string & = "");

net::HTTP::response favicon ();
net::HTTP::response HTML_JS_UI_response ();
net::HTTP::response help_response (meth = UNSET);
net::HTTP::response version_response ();

net::HTTP::response ok_response () {
    return net::HTTP::response (204);
}

net::HTTP::response JSON_response (const JSON &j) {
    return net::HTTP::response (200, {{"content-type", "application/json"}}, bytes (data::string (j.dump ())));
}

net::HTTP::response boolean_response (bool b) {
    return JSON_response (b);
}

namespace tao_pegtl_grammar {
    struct whole_symbol : seq<symbol_lit, eof> {};

    Diophant::symbol read_symbol (const data::string &input) {
        tao::pegtl::memory_input<> in {input, "symbol"};

        try {
            if (!tao::pegtl::parse<whole_symbol> (in))
                return {};
        } catch (tao::pegtl::parse_error &err) {
            return {};
        }

        return Diophant::symbol {input};
    }
}

// for methods that operate on the database 
// without a particular wallet. 
net::HTTP::response process_method (
    processor &,
    net::HTTP::method, meth,
    map<UTF8, UTF8> query,
    const maybe<net::HTTP::content> &,
    const data::bytes &);

// for methods that operate on a specific wallet. 
net::HTTP::response process_wallet_method (
    processor &p, net::HTTP::method http_method, meth m,
    Diophant::symbol wallet_name,
    map<UTF8, UTF8> query,
    const maybe<net::HTTP::content> &content,
    const data::bytes &body);

awaitable<net::HTTP::response> processor::operator () (const net::HTTP::request &req) {

    std::cout << "Responding to request " << req << std::endl;
    list<UTF8> path = req.Target.path ().read ('/');

    if (size (path) == 0) 
        co_return HTML_JS_UI_response ();

    // the first part of the path is always "" as long as we use "/" as a delimiter 
    // because a path always begins with "/". 
    path = rest (path);

    if (size (path) == 0 || path[0] == "") 
        co_return HTML_JS_UI_response ();

    // The favicon doesn't work right now, not sure why.
    if (size (path) == 1 && path[0] == "favicon.ico") 
        co_return favicon ();

    meth m = read_method (path[0]);

    if (m == UNSET) co_return error_response (400, m, problem::unknown_method, path[0]);

    if (m == VERSION) {
        if (req.Method != net::HTTP::method::get)
            co_return error_response (405, m, problem::invalid_method, "use get");

        co_return version_response ();
    }

    if (m == HELP) {
        if (req.Method != net::HTTP::method::get)
            co_return error_response (405, m, problem::invalid_method, "use get");

        if (path.size () == 1) co_return help_response ();
        else {
            meth help_with_method = read_method (path[1]);
            if (help_with_method == UNSET)
                co_return error_response (400, HELP, problem::unknown_method, path[1]);
            co_return help_response (help_with_method);
        }
    }

    if (m == SHUTDOWN) {
        if (req.Method != net::HTTP::method::put)
            co_return error_response (405, m, problem::invalid_method, "use put");

        Shutdown = true;
        co_return ok_response ();
    }

    if (m == ADD_ENTROPY) {
        if (req.Method != net::HTTP::method::post)
            co_return error_response (405, m, problem::invalid_method, "use post");

        maybe<net::HTTP::content> content_type = req.content_type ();
        if (!bool (content_type) || (
            *content_type != net::HTTP::content::type::application_octet_stream &&
            *content_type != net::HTTP::content::type::text_plain))
            co_return error_response (400, m, problem::invalid_content_type, "expected content-type:application/octet-stream");

        add_entropy (req.Body);

        co_return ok_response ();
    }

    if (m == LIST_WALLETS) {
        if (req.Method != net::HTTP::method::get)
            co_return error_response (405, m, problem::invalid_method, "use get");
        JSON::array_t names;
        for (const std::string &name : DB->list_wallet_names ())
            names.push_back (name);

        co_return JSON_response (names);
    }

    // we checked the methods that don't take any parameters. 
    map<UTF8, UTF8> query;
    maybe<dispatch<UTF8, UTF8>> qm = req.Target.query_map ();
    if (bool (qm)) {
        map<UTF8, list<UTF8>> q = data::dispatch_to_map (*qm);
        for (const auto &[k, v] : q) if (v.size () != 1) co_return error_response (400, m, problem::invalid_name, "duplicate query parameters");
        else query = query.insert (k, v[0]);
    }

    UTF8 fragment {};
    maybe<UTF8> fm = req.Target.fragment ();
    if (bool (fm)) fragment = *fm;

    if (fragment != "") co_return error_response (400, m, problem::invalid_target, "We don't use the fragment");

    try {

        if (path.size () < 2) co_return process_method (*this, req.Method, m, query, req.content_type (), req.Body);

        // make sure the wallet name is a valid string.
        Diophant::symbol wallet_name = tao_pegtl_grammar::read_symbol (path[1]);
        if (wallet_name == "") co_return error_response (400, m, problem::invalid_name, "name argument must be alpha alnum+");

        co_return process_wallet_method (*this, req.Method, m, wallet_name, query, req.content_type (), req.Body);

    } catch (const crypto::entropy::fail &) {
        co_return error_response (500, m, problem::need_entropy);
    } catch (const Diophant::parse_error &) {
        co_return error_response (400, m, problem::invalid_expression);
    }
}

net::HTTP::response value_response (Bitcoin::satoshi x) {
    return JSON_response (JSON (int64 (x)));
}

maybe<bool> read_bool (const std::string &utf8) {
    std::string sanitized = sanitize (utf8);
    if (sanitized == "true") return true;
    else if (sanitized == "false") return false;
    else return {};
}

net::HTTP::response process_method (
    processor &p, net::HTTP::method http_method, meth m,
    map<UTF8, UTF8> query,
    const maybe<net::HTTP::content> &content_type,
    const data::bytes &body) {

    if (m == INVERT_HASH) {

        enum class hash_function {
            unset,
            SHA1, 
            MD5,
            SHA2_256, 
            SHA2_512,
            RIPEMD160,
            SHA3_256,
            SHA3_512, 
            Hash256, 
            Hash160,
        } HashFunction {hash_function::unset};

        enum class digest_format {
            unset, 
            HEX,
            BASE58_CHECK, 
            BASE64
        } DigestFormat {digest_format::unset};

        // which function to use. 
        const UTF8 *hash_function_param = query.contains ("function");
        if (bool (hash_function_param)) {
            std::string sanitized = sanitize (*hash_function_param);
            if (sanitized == "sha1") HashFunction = hash_function::SHA1;
            else if (sanitized == "md5") HashFunction = hash_function::MD5;
            else if (sanitized == "sha2256") HashFunction = hash_function::SHA2_256;
            else if (sanitized == "sha2512") HashFunction = hash_function::SHA2_512;
            else if (sanitized == "sha3256") HashFunction = hash_function::SHA3_256;
            else if (sanitized == "sha3512") HashFunction = hash_function::SHA3_512;
            else if (sanitized == "ripmd160") HashFunction = hash_function::RIPEMD160;
            else if (sanitized == "hash256") HashFunction = hash_function::Hash256;
            else if (sanitized == "hash160") HashFunction = hash_function::Hash160;
            else return error_response (400, m, problem::invalid_query, "invalid parameter 'function'");
        } else return error_response (400, m, problem::missing_parameter, "missing required parameter 'function'");

        if (http_method != net::HTTP::method::post && http_method != net::HTTP::method::get) 
            return error_response (405, m, problem::invalid_method, "use post or get");

        const UTF8 *digest_format_param = query.contains ("digest_format");
        if (bool (digest_format_param)) {
            std::string sanitized = sanitize (*digest_format_param);
            if (sanitized == "hex") DigestFormat = digest_format::HEX;
            else if (sanitized == "base58check") DigestFormat = digest_format::BASE58_CHECK;
            else if (sanitized == "base64") DigestFormat = digest_format::BASE64;
            else return error_response (400, m, problem::invalid_parameter, "invalid parameter 'digest_format'");
        } else if (http_method == net::HTTP::method::get)
            return error_response (400, m, problem::missing_parameter, "missing required parameter 'digest_format'");

        // the digest provided by the query. In the case of POST, this is optional. 
        data::maybe<bytes> digest;

        const UTF8 *digest_param = query.contains ("digest");
        if (bool (digest_param)) {
          switch (DigestFormat) {
              case digest_format::HEX: {
                  return error_response (501, m, problem::unimplemented);
              } break;
              case digest_format::BASE58_CHECK: {
                  return error_response (501, m, problem::unimplemented);
              } break;
              case digest_format::BASE64: {
                  return error_response (501, m, problem::unimplemented);
              } break;
              default: 
                  return error_response (400, m, problem::invalid_parameter, "invalid parameter 'digest'");
          }
        } else if (DigestFormat != digest_format::unset) 
            return error_response (400, m, problem::missing_parameter, "missing required parameter 'digest'");
        
        data::maybe<const bytes &> data_to_hash;

        if (!bool (content_type)) {
            if (http_method == net::HTTP::method::post) 
                return error_response (400, m, problem::invalid_query, "data to hash should be provided in body");
        } else if (*content_type != net::HTTP::content::type::application_octet_stream && *content_type != net::HTTP::content::type::text_plain) {
            return error_response (400, m, problem::invalid_content_type, "either plain text or bytes");
        } else {
            data_to_hash = body;

            bytes hash_digest;

            switch (HashFunction) {
                case hash_function::SHA1: {} break;
                case hash_function::SHA2_256: {} break;
                case hash_function::SHA2_512: {} break;
                case hash_function::SHA3_256: {} break;
                case hash_function::SHA3_512: {} break;
                case hash_function::RIPEMD160: {} break;
                case hash_function::Hash256: {} break;
                case hash_function::Hash160: {} break;
                default: {
                    return error_response (501, m, problem::unimplemented);
                }
            }

            if (bool (digest) && hash_digest != *digest) 
                return error_response (400, m, problem::invalid_query, 
                    "provided hash digest does not match calculated");
            
            return error_response (501, m, problem::unimplemented);

        }
        

        if (http_method == net::HTTP::method::post) {

        } else {
                
        }


        return error_response (501, m, problem::unimplemented);
    }

    // Associate a secret key with a name. The key could be anything; private, public, or symmetric. 
    if (m == SET_KEY) {
        if (http_method != net::HTTP::method::post)
            return error_response (405, m, problem::invalid_method, "use post");

        enum class key_type {
            unset,
            secp256k1,
            WIF,
            HD
        } KeyType {key_type::unset};

        enum class generation_method {
            random,
            expression
        } Method {generation_method::expression};

        const UTF8 *key_type = query.contains ("type");
        if (bool (key_type)) {
            std::string key_type_san = sanitize (*key_type);
            if (key_type_san == "secp256k1") KeyType = key_type::secp256k1;
            else if (key_type_san == "wif") KeyType = key_type::WIF;
            else if (key_type_san == "hd") KeyType = key_type::HD;
            else return error_response (400, m, problem::invalid_query, "invalid parameter 'type'");
        }

        const UTF8 *method_val = query.contains ("method");
        if (bool (method_val)) {
            std::string method_san = sanitize (*method_val);
            if (method_san == "random") Method = generation_method::random;
            else if (method_san == "expression") Method = generation_method::expression;
            else return error_response (400, m, problem::invalid_query, "invalid parameter 'type'");
        }

        const UTF8 *key_name_param = query.contains ("name");
        if (!bool (key_name_param))
            return error_response (400, m, problem::missing_parameter, "required parameter 'name' not present");

        Diophant::symbol key_name = tao_pegtl_grammar::read_symbol (*key_name_param);

        // make sure the name is a valid symbol name
        if (key_name == "") return error_response (400, m, problem::invalid_name, "name argument must be alpha alnum+");

        Bitcoin::net net = Bitcoin::net::Main;
        const UTF8 *net_type_param = query.contains ("net");
        if (bool (key_name_param)) {
            std::string net_type_san = sanitize (*net_type_param);
            if (net_type_san == "main") net = Bitcoin::net::Main;
            else if (net_type_san == "test") net = Bitcoin::net::Test;
            else return error_response (400, m, problem::invalid_query, "invalid parameter 'net'");
        }

        bool compressed = true;
        const UTF8 *compressed_param = query.contains ("compressed");
        if (bool (compressed_param)) {
            maybe<bool> maybe_compressed =  read_bool (*compressed_param);
            if (!maybe_compressed) return error_response (400, m, problem::invalid_query, "invalid parameter 'compressed'");
            compressed = *maybe_compressed;
        }

        Cosmos::key_expression key_expr;

        if (Method == generation_method::expression) {
            if (KeyType != key_type::unset)
                return error_response (400, m, problem::invalid_query, "key type will be inferred from the expression provided");

            if (!bool (content_type) || *content_type != net::HTTP::content::type::text_plain)
                return error_response (400, m, problem::invalid_content_type, "expected content-type:text/plain");

            key_expr = Cosmos::key_expression {data::string (body)};

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
                    key_expr = Cosmos::key_expression {Bitcoin::secret {net, key, compressed}};
                } break;
                case (key_type::HD): {
                    HD::chain_code x;
                    random >> x;
                    key_expr = Cosmos::key_expression (HD::BIP_32::secret {key, x, net});
                } break;
                default: {
                    key_expr = Cosmos::key_expression {key};
                }
            }
        }

        if (p.DB->set_key (key_name, key_expr)) return ok_response ();
        return error_response (500, m, problem::failed, "could not create key");

    }

    if (m == TO_PRIVATE) {
        if (http_method == net::HTTP::method::post) {

            if (!bool (content_type) || *content_type != net::HTTP::content::type::text_plain)
                return error_response (400, m, problem::invalid_content_type, "expected content-type:text/plain");

            const UTF8 *key_name_param = query.contains ("name");
            if (!bool (key_name_param))
                return error_response (400, m, problem::invalid_query, "required parameter 'name' not present");

            Diophant::symbol key_name = tao_pegtl_grammar::read_symbol (*key_name_param);

            Cosmos::key_expression key_expr {data::string (body)};

            if (p.DB->set_to_private (key_name, key_expr)) return ok_response ();
            return error_response (500, m, problem::failed, "could not set private key");
        } else if (http_method != net::HTTP::method::get) {
            return error_response (501, m, problem::unimplemented, "GET TO_PRIVATE");
        } else return error_response (405, m, problem::invalid_method, "use post or get");
    }

    return error_response (500, m, problem::invalid_name, "wallet method called without wallet name");
}

bool parse_uint32 (const std::string &str, uint32_t &result) {
    try {
        size_t idx = 0;
        unsigned long val = std::stoul (str, &idx, 10);  // base 10

        if (idx != str.size ())
            return false;  // Extra characters after number

        if (val > std::numeric_limits<uint32_t>::max())
            return false;  // Out of range for uint32_t

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
    processor &p, net::HTTP::method http_method, meth m,
    Diophant::symbol wallet_name,
    map<UTF8, UTF8> query,
    const maybe<net::HTTP::content> &content,
    const data::bytes &body) {

    // make sure the wallet name is a valid string.
    if (wallet_name == "") return error_response (400, m, problem::invalid_name, "name argument must be alpha alnum+");

    if (m == MAKE_WALLET) {
        if (http_method != net::HTTP::method::post)
            return error_response (405, m, problem::invalid_method, "use post");

        bool created = p.DB->make_wallet (wallet_name);
        if (created) return ok_response ();
        else return error_response (500, m, problem::failed, "could not create wallet");
    }

    if (m == ADD_KEY_SEQUENCE) {
        if (http_method != net::HTTP::method::post)
            return error_response (405, m, problem::invalid_method, "use post");

        const UTF8 *key_name_param = query.contains ("key_name");
        if (!bool (key_name_param))
            return error_response (400, m, problem::invalid_query, "required parameter 'key_name' not present");

        Diophant::symbol key_name = tao_pegtl_grammar::read_symbol (*key_name_param);

        // make sure the name is a valid symbol name
        if (key_name == "") return error_response (400, m, problem::invalid_name, "key_name parameter must be alpha alnum+");

        const UTF8 *name_param = query.contains ("name");
        if (!bool (name_param))
            return error_response (400, m, problem::invalid_query, "required parameter 'name' not present");

        Diophant::symbol name = tao_pegtl_grammar::read_symbol (*name_param);

        // make sure the name is a valid symbol name
        if (name == "") return error_response (400, m, problem::invalid_name, "name parameter must be alpha alnum+");

        uint32 index = 0;
        const UTF8 *index_param = query.contains ("index");
        if (bool (index_param)) {
            if (!parse_uint32 (*index_param, index))
                return error_response (400, m, problem::invalid_name, "invalid parameter 'index'");
        }

        Cosmos::key_derivation key_deriv {data::string (body)};

        if (p.DB->set_wallet_derivation (wallet_name, name, Cosmos::database::derivation {key_deriv, key_name, index})) return ok_response ();
        return error_response (500, m, problem::failed, "could not set wallet derivation");
    }

    if (m == VALUE) {
        if (http_method != net::HTTP::method::get)
            return error_response (405, m, problem::invalid_method, "use get");

        return value_response (p.DB->get_wallet_account (wallet_name).value ());
    }

    if (m == DETAILS) {
        if (http_method != net::HTTP::method::get)
            return error_response (405, m, problem::invalid_method, "use get");

        return JSON_response (p.DB->get_wallet_account (wallet_name).details ());
    }

    if (m == GENERATE) {
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
                return error_response (400, m, problem::invalid_name, "invalid parameter 'number_of_words'");
        }

        if (number_of_words != 12 && number_of_words != 24)
            return error_response (400, m, problem::invalid_name, "invalid parameter 'number_of_words'");

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
            else return error_response (400, m, problem::invalid_query, "invalid parameter 'wallet_style'");
        }

        uint32 coin_type = 0;
        const UTF8 *coin_type_param = query.contains ("coin_type");
        if (bool ()) {
            if (!parse_uint32 (*number_of_words_param, number_of_words))
                return error_response (400, m, problem::invalid_name, "invalid parameter 'number_of_words'");
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

        Cosmos::key_expression master_key_expr {master};
        Cosmos::key_expression account_key_expr {account};
        Cosmos::key_expression account_derivation {static_cast<std::string> (master_key_expr) + " / `44 / `" + std::to_string (coin_type) + " / `0"};

        std::string master_key_name = static_cast<std::string> (wallet_name) + "_master";
        std::string account_name = static_cast<std::string> (wallet_name) + "_account_0";

        p.DB->set_key (master_key_name, master_key_expr);
        p.DB->set_key (account_name, account_key_expr);
        p.DB->set_to_private (account_name, account_derivation);

        p.DB->set_wallet_derivation (wallet_name, "receive",
            Cosmos::database::derivation {Cosmos::key_derivation {"@ name index -> name / 0 / index"}, account_name});

        p.DB->set_wallet_derivation (wallet_name, "change",
            Cosmos::database::derivation {Cosmos::key_derivation {"@ name index -> name / 1 / index"}, account_name});

        if (WalletStyle == wallet_style::BIP_44_plus)
            p.DB->set_wallet_derivation (wallet_name, "receivex",
                Cosmos::database::derivation {Cosmos::key_derivation {"@ name index -> name / 0 / `index"}, account_name});

        if (use_mnemonic) return JSON_response (JSON {wallet_words});
        return ok_response ();

    }

    if (m == NEXT_ADDRESS) {
        if (http_method != net::HTTP::method::post)
            return error_response (405, m, problem::invalid_method, "use post");
        
        // look at the receive key sequence.
        Diophant::symbol sequence_name;

        {
          const UTF8 *sequence_name_param = query.contains ("sequence_name");
          if (bool (sequence_name_param)) sequence_name = *sequence_name_param;
          else sequence_name = std::string {"receive"};
        }

        if (!sequence_name.valid ()) return error_response (400, m, problem::invalid_name);
        
        // do we have a sequence by this name? 
        maybe<Cosmos::database::derivation> seq = p.DB->get_wallet_derivation (wallet_name, sequence_name);
        if (!bool (seq)) return error_response (400, m, problem::invalid_query);

        // generate a new key. We must be able to generate a pubkey.
        Cosmos::key_expression k = seq->increment ();

        Bitcoin::pubkey p;
        try {
           p = Bitcoin::pubkey (k);
        } catch (...) {}

        if (!p.valid ()) return error_response (400, m, problem::invalid_query);

        // make a database entery for it. 
        return error_response (501, m, problem::unimplemented);
    }

    if (m == NEXT_XPUB) {
        if (http_method != net::HTTP::method::post)
            return error_response (405, m, problem::invalid_method, "use post");
        
        // look at the receivex key sequence.
        Diophant::symbol sequence_name;

        {
          const UTF8 *sequence_name_param = query.contains ("sequence_name");
          if (bool (sequence_name_param)) sequence_name = *sequence_name_param;
          else sequence_name = std::string {"receivex"};
        }

        if (!sequence_name.valid ()) return error_response (400, m, problem::invalid_name);

        // do we have a sequence by this name? 
        maybe<Cosmos::database::derivation> seq = p.DB->get_wallet_derivation (wallet_name, sequence_name);
        if (!bool (seq)) return error_response (400, m, problem::invalid_query);

        // generate a new key. 
        Cosmos::key_expression k = seq->increment ();

        HD::BIP_32::pubkey p;
        try {
           p = HD::BIP_32::pubkey (k);
        } catch (...) {}

        if (!p.valid ()) return error_response (400, m, problem::invalid_query);

        // if it is, then make a database entry. We don't know what the entry looks like yet. 
        return error_response (501, m, problem::unimplemented);
    }


    if (m == IMPORT) {
        if (http_method != net::HTTP::method::put)
            return error_response (405, m, problem::invalid_method, "use put");
        // we need to find addresses and xpubs that we need to check. 
        // could just check the last several keys? 

        // first we need a function that recognizes output patterns. 
        return error_response (501, m, problem::unimplemented);
    }

    if (m == SPEND) {
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

    if (m == RESTORE) {
        if (http_method != net::HTTP::method::put)
            return error_response (405, m, problem::invalid_method, "use put");

        return error_response (501, m, problem::unimplemented);
    }

    if (m == BOOST) {
        if (http_method != net::HTTP::method::post)
            return error_response (405, m, problem::invalid_method, "use post");

        return error_response (501, m, problem::unimplemented);
    }

    if (m == SPLIT) {
        if (http_method != net::HTTP::method::post)
            return error_response (405, m, problem::invalid_method, "use post");

        return error_response (501, m, problem::unimplemented);
    }

    if (m == TAXES) {
        if (http_method != net::HTTP::method::get)
            return error_response (405, m, problem::invalid_method, "use get");

        return error_response (501, m, problem::unimplemented);
    }

    if (m == ENCRYPT_KEY) {
        if (http_method != net::HTTP::method::post)
            return error_response (405, m, problem::invalid_method, "use post");

        return error_response (501, m, problem::unimplemented);
    }

    if (m == DECRYPT_KEY) {
        if (http_method != net::HTTP::method::post)
            return error_response (405, m, problem::invalid_method, "use post");

        return error_response (501, m, problem::unimplemented);
    }

    return error_response (501, m, problem::unimplemented);
}

processor::processor (const options &o) {
    SpendOptions = o.spend_options ();
    DB = load_DB (o);

}

ptr<Cosmos::database> load_DB (const options &o) {
    auto db_opts = o.db_options ();
    if (!db_opts.is<options::SQLite_options> ()) throw exception {} << "Only SQLite is supported";

    return Cosmos::SQLite::load (db_opts.get<options::SQLite_options> ().Path);
}

maybe<filepath> options::env () const {
    maybe<filepath> env_path;
    this->get ("env", env_path);
    return env_path;
}

net::IP::TCP::endpoint options::endpoint () const {
    maybe<net::IP::TCP::endpoint> endpoint;
    this->get ("endpoint", endpoint);
    if (bool (endpoint)) return *endpoint;

    const char* val = std::getenv ("COSMOS_WALLET_ENDPOINT");

    if (bool (val)) return net::IP::TCP::endpoint {val};

    maybe<net::IP::address> ip_address;
    this->get ("ip_address", ip_address);
    if (!bool (ip_address)) {
        const char *addr = std::getenv ("COSMOS_WALLET_IP_ADDRESS");

        if (bool (addr)) ip_address = net::IP::address {addr};
        else ip_address = net::IP::address {"127.0.0.1"};
    }

    maybe<uint16> port_number;
    this->get ("port", port_number);
    if (!bool (port_number)) {
        const char *pn = std::getenv ("COSMOS_WALLET_PORT_NUMBER");

        if (bool (pn)) {
            auto [_, ec] = std::from_chars (pn, pn + std::strlen (val), *port_number);
            if (ec != std::errc ()) throw exception {} << "invalid port number " << pn;
        }
    }

    if (!bool (port_number)) throw exception {} << "No port number provided";

    return net::IP::TCP::endpoint {*ip_address, *port_number};
}

uint32 options::threads () const {
    maybe<uint32> threads;
    this->get ("threads", threads);
    if (bool (threads)) return *threads;

    threads = 1;
    const char* val = std::getenv ("COSMOS_THREADS");

    if (bool (val)) {
        auto [_, ec] = std::from_chars (val, val + std::strlen (val), *threads);
        if (ec == std::errc ()) return *threads;
        else throw exception {} << "could not parse threads " << val;
    }

    return *threads;
}

either<options::JSON_DB_options, options::SQLite_options, options::MongoDB_options> options::db_options () const {
    SQLite_options sqlite;
    this->get ("sqlite_path", sqlite.Path);
    if (bool (sqlite.Path)) return sqlite;

    const char *val = std::getenv ("COSMOS_SQLITE_PATH");
    if (bool (val)) sqlite.Path = filepath {val};

    return sqlite;
}

crypto::entropy &processor::get_secure_random () {

    if (!SecureRandom) {

        if (!Entropy) throw data::crypto::entropy::fail {};

        SecureRandom = std::make_shared<crypto::NIST::DRBG> (crypto::NIST::DRBG::Hash, *Entropy, std::numeric_limits<uint32>::max ());
    }

    return *SecureRandom.get ();

}

crypto::entropy &processor::get_casual_random () {

    if (!CasualRandom) {
        uint64 seed;
        get_secure_random () >> seed;

        CasualRandom = std::make_shared<crypto::linear_combination_random> (256,
            std::static_pointer_cast<crypto::entropy> (std::make_shared<crypto::std_random<std::default_random_engine>> (seed)),
            std::static_pointer_cast<crypto::entropy> (SecureRandom));
    }

    return *CasualRandom.get ();

}

void processor::add_entropy (const bytes &b) {
    if (!FixedEntropy) FixedEntropy = std::make_shared<crypto::fixed_entropy> (b);
    else {
        FixedEntropy->Entropy = b;
        FixedEntropy->Position = 0;
    }

    if (!Entropy) Entropy = std::make_shared<crypto::entropy_sum> (FixedEntropy, std::make_shared<Gigamonkey::bitcoind_entropy> ());
}

Cosmos::spend_options options::spend_options () const {
    return Cosmos::spend_options {};
}

net::HTTP::response help_response (meth m) {
    std::stringstream ss;
    help (ss, m);
    return net::HTTP::response (200, {{"content-type", "text/plain"}}, bytes (data::string (ss.str ())));
}

net::HTTP::response version_response () {
    std::stringstream ss;
    version (ss);
    auto res = net::HTTP::response (200, {{"content-type", "text/plain"}}, bytes (data::string (ss.str ())));
    return res;
}

net::HTTP::response error_response (unsigned int status, meth m, problem tt, const std::string &detail) {
    std::stringstream meth_string;
    meth_string << m;
    std::stringstream problem_type;
    problem_type << tt;

    JSON err {
        {"method", meth_string.str ()},
        {"status", status},
        {"title", problem_type.str ()}};

    if (detail != "") err["detail"] = detail;

    return net::HTTP::response (status, {{"content-type", "application/problem+json"}}, bytes (data::string (err.dump ())));
}

std::ostream &operator << (std::ostream &o, problem p) {
    switch (p) {
        case problem::unknown_method: return o << "unknown method";
        case problem::invalid_method: return o << "invalid method";
        case problem::invalid_content_type: return o << "invalid content-type";
        case problem::need_entropy: return o << "need entropy: please call add_entropy";
        case problem::invalid_name: return o << "name argument required. Should be alpha alnum+";
        case problem::unimplemented: return o << "unimplemented method";
        default: throw data::exception {} << "invalid problem...";
    }
}

net::HTTP::response HTML_JS_UI_response () {
    data::string ui =
R"--(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <title>Node API Interface</title>
  <style>
    body {
      font-family: sans-serif;
      padding: 1em;
      background: #f0f0f0;
    }
    details {
      margin-bottom: 1em;
      background: #fff;
      border-radius: 8px;
      box-shadow: 0 0 5px rgba(0,0,0,0.1);
      padding: 1em;
    }
    summary {
      font-weight: bold;
      cursor: pointer;
    }
    button {
      margin-top: 0.5em;
      margin-bottom: 0.5em;
    }
    pre {
      background: #eee;
      padding: 0.5em;
      border-radius: 4px;
      white-space: pre-wrap;
      word-break: break-word;
    }
  </style>
</head>
<body>
  <h1>Cosmos Wallet</h1>

  <h2>Basic Functions</h2>

  <details>
    <summary>version</summary>
    <p>Get version string.</p>
    <code>GET /version</code>
    <form id="form-version">
      <button type="button" id="submit-version" onclick="callApi('GET', 'version')">Get Version</button>
    </form>
    <pre id="output-version"></pre>
  </details>

  <details>
  <summary>help</summary>
    <p>General help or get help with a specific function.</p>
    <p>
      <code>GET /help</code>
      or 
      <code>GET /help/&lt;method&gt;</code>
    </p>
    <form id="form-help">
      <label><input type="radio" name="method" value="shutdown" onclick="toggleRadio(this)">shutdown</label>
      <br>
      <label><input type="radio" name="method" value="add_entropy" onclick="toggleRadio(this)">add_entropy</label>
      <br>
      <label><input type="radio" name="method" value="invert_hash" onclick="toggleRadio(this)">invert_hash</label>
      <br>
      <label><input type="radio" name="ethod" value="add_key" onclick="toggleRadio(this)">add_key</label>
      <br>
      <label><input type="radio" name="method" value="to_private" onclick="toggleRadio(this)">to_private</label>
      <br>
      <label><input type="radio" name="method" value="list_wallets" onclick="toggleRadio(this)">list_wallets</label>
      <br>
      <label><input type="radio" name="method" value="make_wallet" onclick="toggleRadio(this)">make_wallet</label>
      <br>
      <label><input type="radio" name="method" value="add_key_sequence" onclick="toggleRadio(this)">add_key_sequence</label>
      <br>
      <label><input type="radio" name="method" value="value" onclick="toggleRadio(this)">value</label>
      <br>
      <label><input type="radio" name="method" value="details" onclick="toggleRadio(this)">details</label>
      <br>
      <label><input type="radio" name="method" value="generate" onclick="toggleRadio(this)">generate</label>
      <br>
      <button type="button" id="submit-help" onclick="callHelp()">Get Help</button>
    </form>
    <pre id="output-help"></pre>
  </details>

  <details>
    <summary>shutdown</summary>
    <p>
      Shutdown the program.
    </p>
    <p>
      <code>PUT /shutdown</code>
    </p>
    <form id="form-shutdown">
      <button type="button" id="submit-shutdown" onclick="callApi('PUT', 'shutdown')">Shutdown</button>
    </form>
    <pre id="output-shutdown"></pre>
  </details>

  <details>
    <summary>add_entropy</summary>
    <p>
      The cryptographic random number generator needs entropy periodically. Here we add it manually.
      Type in some random text with your fingers to provide it.
    </p>
    <p>
      <code>POST /add_entropy</code>
      <br>
      Provide entropy in the body of the request. <code>content-type</code> 
      should be <code>text/plain</code> or <code>application/octet-stream</code>
    </p>
    <form id="form-add_entropy">
      <textarea id="user_entropy" name="user_entropy" rows="4" cols="50"></textarea>
      <br>
      <button type="button" id="submit-add_entropy" onclick="callAddEntropy()">Add Entropy</button>
    </form>
    <pre id="output-add_entropy"></pre>
  </details>

  <h2>Keys and data</h2>

  <details>
    <summary>Invert Hash</summary>
    <p>
      Add data and its hash, which can be retrieved later by hash.
    </p>
    <p>
      <code>POST /invert_hash?digest=&lt;hex&gt;&function=&lt;hash function name&gt;&</code> or <code>GET /invert_hash?digest=&lt;hex&gt;</code>
      <br>
      Supported hash functions are <code>SHA1</code>, <code>SHA2_256</code>, <code>RIPEMD160</code>, 
      <code>Hash256</code>, and <code>Hash160</code>, 
    </p>
    <form id="form-invert_hash">
      <label><input type="radio" name="HTTP_method" value="GET" onclick="toggleRadio (this)">GET</label>
      <br>
      <label><input type="radio" name="HTTP_method" value="POST" onclick="toggleRadio (this)">POST</label>
      <br>
      <b>digest: (hex) </b><input name="hash_digest" type="text">
      <label for="digest_format"></label>
      <select name="digest_format">
        <option value="HEX">hex</option>
        <option value="BASE58_CHECK">base 58 check</option>
        <option value="BASE64">base 64</option>
      </select>
      <br>
      <label><input type="radio" name="function" value="SHA1" onclick="toggleRadio (this)">SHA1</label>
      <br>
      <label><input type="radio" name="function" value="SHA2_256" onclick="toggleRadio (this)">SHA2 256</label>
      <br>
      <label><input type="radio" name="function" value="RIPEMD160" onclick="toggleRadio (this)">RIPEMD160</label>
      <br>
      <label><input type="radio" name="function" value="Hash256" onclick="toggleRadio (this)">Hash256</label>
      <br>
      <label><input type="radio" name="function" value="Hash160" onclick="toggleRadio (this)">Hash160</label>
      <br>
      <label><input type="radio" name="data_format" value="HEX" onclick="toggleRadio (this)">hex</label>
      <br>
      <label><input type="radio" name="data_format" value="ASCII" onclick="toggleRadio (this)">ASCII</label>
      <br>
      <textarea id="hashed_data" name="hashed_data" rows="4" cols="50"></textarea>
      <br>
      <button type="button" id="submit-invert_hash" onclick="callInvertHash()">Invert Hash</button>
    </form>
  </details>

  <details>
    <summary>Set Key</summary>
    <p>
      Add or retrieve a key. This could be any kind of key: public, private,
      symmetric, whatever. You can enter it in the form of a Bitcoin
      Calculator expression or generate a random key of a specific type.
    </p>
    <p>
      <code>POST /set_key?</code> or <code>GET /set_key?</code>
    </p>
    <form id="form-add_key">
      <label><input type="radio" name="HTTP_method" value="GET" onclick="toggleRadio(this)">GET</label>
      <br>
      <label><input type="radio" name="HTTP_method" value="POST" onclick="toggleRadio(this)">POST</label>
      <br>
      <b>key name: </b><input name="name" type="text">
      <br>
      <label><input type="radio" name="method-type" value="expression" onclick="toggleRadio(this)">
        <input name="value" type="text">
      </label>
      <br>
      <label><input type="radio" name="method-type" value="random-secp256k1" onclick="toggleRadio(this)">random secp256k1</label>
      <br>
      <label><input type="radio" name="method-type" value="random-xpriv" onclick="toggleRadio(this)">random xpub</label>
      <br>
      <button type="button" id="submit-add_key" onclick="callAddKey ()">Add Key</button>
    </form>
    <pre id="output-add_key"></pre>
  </details>

  <details>
    <summary>To Private</summary>
    <p>
      Provide an expression to derive the private key of a public key named
      <b>key name</b>.
    </p>
    <p>
      <code>POST /to_private?</code> or <code>GET /to_private?</code>
    </p>
    <form id="form-to_private">
      <label><input type="radio" name="HTTP_method" value="GET" onclick="toggleRadio(this)">GET</label>
      <br>
      <label><input type="radio" name="HTTP_method" value="POST" onclick="toggleRadio(this)">POST</label>
      <br>
      <b>key name: </b><input name="key_name" type="text">
      <br>
      <b>value of private key: </b><input name="key_value" type="text">
      <br>
      <button type="button" id="submit-to_private" onclick="callToPrivate()">To Private</button>
    </form>
    <pre id="output-to_private"></pre>
  </details>

  <h2>Wallet Generation</h2>

  <details>
    <summary>Make Wallet</summary>
    <p>
      Make an empty wallet. The wallet has no keys associated with it. You have to build it up.
    </p>
    <p>
      <code>POST /make_wallet?name=</code> or <code>GET /make_wallet?</code>
    </p>
    <form id="form-make_wallet">
      <b>wallet name: </b><input name="make_wallet-wallet-name" type="text">
      <br>
      <button type="button" onclick="callMakeWallet()">Make Wallet</button>
    </form>
    <pre id="output-make_wallet"></pre>
  </details>

  <details>
    <summary>Add Key Sequence</summary>
    <p>
      Add a key sequence to a wallet.
    </p>
    <p>
      <code>POST /add_key_sequence?</code>
    </p>
    <form id="form-add_key_sequence">
      <b>wallet name: </b><input name="wallet-name" type="text">
      <br>
      <b>sequence name: </b><input name="sequence-name" type="text">
      <br>
      <b>key name: </b><input name="key-name" type="text">
      <br>
      <b>key expression: </b><input name="expression" type="text">
      <br>
      <button type="button" id="submit-add_key_sequence" onclick="callAddKeySequence()">Add Key Sequence</button>
    </form>
    <pre id="output-add_key_sequence"></pre>
  </details>

  <details>
    <summary>Generate</summary>
    <p>
      Generate a wallet.
    </p>
    <form id="form-generate">
      <b>wallet name: </b><input name="wallet_name" type="text">
      <br>
      <b>Use mnemonic: </b><input name="mnemonic" type="checkbox" checked>
      <br>Number of words:<br>
      <label><input type="radio" name="number_of_words" value="12" onclick="toggleRadio(this)" checked>
        12 words
      </label>
      <br>
      <label><input type="radio" name="number_of_words" value="24" onclick="toggleRadio(this)">
        24 words
      </label>
      <br>Schema:<br>
      <label><input type="radio" name="mnemonic_style" value="BIP_39" onclick="toggleRadio(this)" checked>
        BIP 39 (standard)
      </label>
      <br>
      <label><input type="radio" name="mnemonic_style" value="ElectrumSV" onclick="toggleRadio(this)">
        Electrum SV
      </label>
      <br>
      <label>
      </label>
      <br>
      <label><input type="radio" name="wallet_type" value="BIP_44" onclick="toggleRadio(this)">
        bip 44
      </label>
      <br>
      <label><input type="radio" name="wallet_type" value="BIP_44+" onclick="toggleRadio(this)" checked>
        bip 44+
      </label>
      <br>
      <label><input type="radio" name="wallet_type" value="experimental" onclick="toggleRadio(this)">
        experimental
      </label>
      <br>
      <button type="button" id="submit-generate" onclick="callGenerate()">Generate</button>
    </form>
    <pre id="output-generate"></pre>
  </details>

  <details>
    <summary>restore</summary>
    Restore a wallet (not yet implemented)
    <form id="form-restore">
      <button type="button" id="submit-spend">Value</button>
    </form>
      <button type="button" id="submit-restore" onclick="callRestore()">Restore</button>
    <pre id="output-restore"></pre>
  </details>

  <h2>Wallets</h2>

  <details>
    <summary>list_wallets</summary>
    <p>
      List wallets.
    </p>
    <form id="form-list_wallets">
      <button type="button" id="submit-list_wallets" onclick="callAPI('GET', 'list_wallets)">List Wallets</button>
    </form>
    <pre id="output-list_wallets"></pre>
  </details>

  <details>
    <summary>value</summary>
    <p>
      Get wallet value.
    </p>
    <form id="form-value">
      <b>wallet name: </b><input name="wallet-name" type="text">
      <br>
      <label><input type="radio" name="choice" value="BSV" onclick="toggleRadio(this)" checked>BSV sats</label>
      <br>
      <button type="button" id="submit-value" onclick="callGetValue()">Value</button>
    </form>
    <pre id="output-value"></pre>
  </details>

  <details>
    <summary>details</summary>
    <p>
      Get wallet details (includes and information about the outputs it's stored in).
    </p>
    <form id="form-details">
      <b>wallet name: </b><input name="wallet-name" type="text">
      <br>
      <button type="button" id="submit-details" onclick="callGetWalletDetails()">Details</button>
    </form>
    <pre id="output-details"></pre>
  </details>

  <details>
    <summary>Next Key</summary>
    <p>
      Get next receive key.
    </p>
    <form id="form-next_key">
      <b>wallet name: </b><input name="wallet-name" type="text">
      <br>
      <b>sequence name: </b><input name="sequence_name" type="text">
      <br>
      <button type="button" id="submit-next_key" onclick="callWalletNextKey()">Details</button>
    </form>
    <pre id="output-next_key"></pre>
  </details>

  <details>
    <summary>Next XPub</summary>
    <p>
      Get next XPUB (if supported; BIP44+ and experimental wallet formats only.)
    </p>
    <form id="form-next_xpub">
      <b>wallet name: </b><input name="wallet-name" type="text">
      <br>
      <b>sequence name: </b><input name="sequence_name" type="text">
      <br>
      <button type="button" id="submit-next_xpub" onclick="callWalletNextXPub()">Details</button>
    </form>
    <pre id="output-next_xpub"></pre>
  </details>

  <details>
    <summary>Accept</summary>
    <form id="form-accept">
      <b>wallet name: </b><input name="wallet-name" type="text">
      <br>
      <button type="button" id="submit-accept" onclick="callWalletAccept()">Accept</button>
    </form>
    <pre id="output-accept"></pre>
  </details>

  <details>
    <summary>Spend</summary>
    Spend some coins.
    <form id="form-spend">
      <b>wallet name: </b><input name="wallet-name" type="text">
      <br>
      <b>amount: </b><input name="amount" type="text">
      <br>
      <b>send to: </b><input name="to" type="text"> (Can be an address or an xpub.)
      <br>
      <button type="button" id="submit-spend" onclick="callSpend()">Spend</button>
    </form>
    <pre id="output-spend"></pre>
  </details>

  <script>
    let lastChecked = {};

    function toggleRadio(radio) {
      const group = radio.name;

      if (lastChecked[group] === radio) {
          radio.checked = false;
          lastChecked[group] = null;
      } else {
          lastChecked[group] = radio;
      }
    }

    async function callApi (http_method, endpoint) {

      const form = document.getElementById (`form-${endpoint}`);
      const formData = new FormData (form);
      let url = `/${endpoint}`;

      let options = {
          method: http_method,
          headers: {}
      };

      if (http_method === 'GET') {
        // Convert formData to URL query parameters
        const params = new URLSearchParams (formData).toString ();
        url += `?${params}`;
      } else if (http_method === 'POST') {
        // Send form data as URL-encoded body
        options.body = new URLSearchParams (formData);
        options.headers['Content-Type'] = 'application/x-www-form-urlencoded';
      }

      try {
        const res = await fetch (url, options);
        const text = await res.text ();
        document.getElementById (`output-${endpoint}`).textContent = text;
      } catch (err) {
        document.getElementById (`output-${endpoint}`).textContent = 'Error: ' + err;
      }
    }

    async function callHelp () {

      const selected = document.getElementById ('form-help').elements["method"];

      const target;

      if (selected) {
        target = '/help/' + selected.value
      } else {
        target = '/help';
      }

      try {
        const response = await fetch(target, {
          method: 'GET'
        });

        const result = await response.text(); // or .json() depending on response type
        document.getElementById (`output-add_entropy`).textContent = result;
      } catch (err) {
        document.getElementById (`output-add_entropy`).textContent = 'Error: ' + err;
      }
    }

    async function callAddEntropy () {

      try {
        const response = await fetch ('/add_entropy', {
          method: 'POST',
          headers: {
            'Content-Type': 'text/plain',
          },
          body: document.getElementById ('form-add_entropy').elements["user_entropy"].value
        });

        const result = await response.text (); // or .json() depending on response type
        document.getElementById (`output-add_entropy`).textContent = result;
      } catch (err) {
        document.getElementById (`output-add_entropy`).textContent = 'Error: ' + err;
      }
    }

    async function callGenerate () {
      const elements = document.getElementById ('form-generate').elements;
      var target = '/generate' + elements["wallet-name"].value + '?'

      // Whether we use a mnemonic at all. 
      if (elements["mnemonic"].checked) {
      
        target += 'mnemonic=true&mnemonic_style=' + 
            elements["mnemonic_style"].value + 
            '&number_of_words=' + elements["number_of_words"].value + '&';
      
      }

      target += 'wallet_style=' + elements["wallet_style"].value + '&coin_type=' + elements["coin_type"].value;

      try {
        const response = await fetch (target, {
          method: 'POST',
        });

        const result = await response.text (); // or .json() depending on response type
        document.getElementById (`output-generate`).textContent = result;
      } catch (err) {
        document.getElementById (`output-generate`).textContent = 'Error: ' + err;
      }

    }

    async function callWalletMethod (http_method, wallet_method, wallet_name, params) {
    
      let target = '/' + wallet_method + '/' + wallet_name;

      if (params =!= undefined) {
        target += '?' + new URLSearchParams (params).toString ();
      }

      let output = 'output-' + wallet_method;

      try {
        const response = await fetch (target, {
          method: http_method,
        });

        const result = await response.text (); // or .json() depending on response type
        document.getElementById (output).textContent = result;
      } catch (err) {
        document.getElementById (output).textContent = 'Error: ' + err;
      }
    }

    async function callMakeWallet () {
      callWalletMethod ('POST', 'make_wallet', document.getElementById ('form-make_wallet').elements['wallet-name']);
    }

    async function callGetValue () {
      callWalletMethod ('GET', 'value', document.getElementById ('form-value').elements['wallet-name']);
    }

    async function callGetWalletDetails () {
      callWalletMethod ('GET', 'details', document.getElementById ('form-value').elements['wallet-name']);
    }

    async function callNextAddress () {
      const elements = document.getElementById ('form-next_address').elements;
      let sequence_name = elements['sequence_name'];
      let params = {};
      if (sequence_name === "") params['sequence_name'] = sequence_name;
      callWalletMethod ('POST', 'next_address', elements['wallet_name'], params);
    }

    async function callNextXPub () {
      const elements = document.getElementById ('form-next_xpub').elements;
      let sequence_name = elements['sequence_name'];
      let params = {};
      if (sequence_name === "") params['sequence_name'] = sequence_name;
      callWalletMethod ('POST', 'next_address', elements['wallet_name'], params);
    }

    async function callInvertHash () {
      const elements = document.getElementById ('form-invert_hash').elements;
      const http_method = elements['HTTP_method'];

      let params = {
        function = element['function'];
      };

      const digest = elements['digest'];

      if (digest =!= "") {
        params['digest'] = digest;
        params['digest_format'] = elements['digest_format'];
      } else if (http_method == 'POST') {
        // TODO: this is an error
      }

      if (http_method == 'POST') {
        // TODO get the data to hash
      }

      // TODO make the query
    }

    async function callAddKey () {
      const elements = document.getElementById ('form-add_key').elements;
      const http_method = elements['HTTP_method'];
      if (http_method == 'GET') {

      } else {
        
      }
    }

    async function callToPrivate () {
      const elements = document.getElementById ('form-to_private').elements;
      const http_method = elements['HTTP_method'];
      if (http_method == 'GET') {
        
      } else {
        
      }
    }

    async function callImport () {
      const elements = document.getElementById ('form-import').elements;
    }

    async function callSpend () {
      const elements = document.getElementById ('form-spend').elements;
    }

    async function callRestore () {
      const elements = document.getElementById ('form-restore').elements;
    }
  </script>
</body>
</html>
)--";

    return net::HTTP::response (200,
        {{"content-type", "text/html"}},
        bytes (ui));
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

