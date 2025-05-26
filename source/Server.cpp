
#include <charconv>

#include <cstdlib>  // for std::getenv
#include <laserpants/dotenv/dotenv.h>

#include <sv/random.h>

#include <data/async.hpp>
#include <data/net/URL.hpp>
#include <data/net/JSON.hpp>
#include <data/net/HTTP_server.hpp>

#include <Cosmos/database.hpp>
#include <Cosmos/random.hpp>
#include <Cosmos/options.hpp>

#include <Cosmos/database/SQLite/SQLite.hpp>

#include "Cosmos.hpp"

namespace Bitcoin = Gigamonkey::Bitcoin;

struct options : io::arg_parser {
    options (io::arg_parser &&ap) : io::arg_parser {ap} {}

    maybe<filepath> env () const;

    struct JSON_DB_options {
        std::string Path; // should be a directory
    };

    struct SQLite_options {
        maybe<filepath> Path {}; // missing for in-memory
    };

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

void shutdown () {
    if (Shutdown) return;
    std::cout << "\nTime to shut down the program!" << std::endl;
    Shutdown = true;
    if (Server != nullptr) Server->close ();
}

void signal_handler (int signal) {
    if (signal == SIGINT || signal == SIGTERM) shutdown ();
}

ptr<Cosmos::database> load_DB (const options &o);

struct processor {
    crypto::random *get_secure_random ();
    crypto::random *get_casual_random ();

    Cosmos::spend_options SpendOptions;

    processor (const options &o);

    awaitable<net::HTTP::response> operator () (const net::HTTP::request &);

    void add_entropy (const bytes &);

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
    ptr<crypto::fixed_entropy> Entropy {nullptr};
    ptr<crypto::NIST::DRBG> SecureRandom {nullptr};
    ptr<crypto::linear_combination_random> CasualRandom {nullptr};

};

void run (const options &program_options) {

    // if the user provided an env path, it
    // is an error if it is not found. If not,
    // we check the default location, but it
    // is not an error if it is not found.
    auto envpath = program_options.env ();
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
        throw exception {} << "This program is not ready to be connected to the Internet: localhost is required";

    uint32 num_threads = program_options.threads ();
    if (num_threads < 1) throw exception {} << "We cannot run with zero threads.";

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
    for (int i = 0; i < num_threads - 1; i++)
        threads.emplace_back ([&]() {
            try {
                while (!Shutdown) {
                    if (stored_exception) shutdown ();
                    IO.run ();
                }
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

    for (int i = 0; i < num_threads; i++) threads[i].join ();

    if (stored_exception) std::rethrow_exception (stored_exception);

    IO.stop ();
}

net::HTTP::response HTML_JS_UI_response ();
net::HTTP::response help_response (meth = UNSET);
net::HTTP::response version_response ();
net::HTTP::response ok_response ();
net::HTTP::response boolean_response (bool);
net::HTTP::response value_response (Bitcoin::satoshi);
net::HTTP::response JSON_response (const JSON &);
net::HTTP::response favicon ();

enum class problem {
    unknown_method,
    invalid_method,
    invalid_type,
    need_entropy,
    unimplemented
};

std::ostream &operator << (std::ostream &, problem);

net::HTTP::response error_response (unsigned int status, meth m, problem, const std::string & = "");

awaitable<net::HTTP::response> processor::operator () (const net::HTTP::request &req) {

    list<UTF8> path = req.Target.path ().read ('/');

    if (path.size () == 0) co_return HTML_JS_UI_response ();

    // the first part of the path is always "".
    path = data::rest (path);

    if (path.size () == 0 || path[0] == "") co_return HTML_JS_UI_response ();

    if (path.size () == 1 && path[0] == "favicon.ico") co_return favicon ();

    meth m = read_method (path[0]);

    if (m == UNSET) co_return error_response (400, m, problem::unknown_method, path[0]);

    if (m == VERSION) {
        if (req.Method != net::HTTP::method::get) co_return error_response (405, m, problem::invalid_method, "use get");
        co_return version_response ();
    }

    if (m == HELP) {
        if (req.Method != net::HTTP::method::get) co_return error_response (405, m, problem::invalid_method, "use get");
        if (path.size () == 1) co_return help_response ();
        else {
            meth help_with_method = read_method (path[1]);
            if (help_with_method == UNSET)
                co_return error_response (400, HELP, problem::unknown_method, path[1]);
            co_return help_response (help_with_method);
        }
    }

    if (m == SHUTDOWN) {
        if (req.Method != net::HTTP::method::put) co_return error_response (405, m, problem::invalid_method, "use put");
        Shutdown = true;
        co_return ok_response ();
    }

    if (m == ADD_ENTROPY) {
        if (req.Method != net::HTTP::method::post) co_return error_response (405, m, problem::invalid_method, "use post");
        maybe<net::HTTP::content> content_type = req.content_type ();
        if (!bool (content_type) || *content_type != net::HTTP::content::type::application_octet_stream)
            co_return error_response (400, m, problem::invalid_type, "expected content-type:application/octet-stream");

        add_entropy (req.Body);

        co_return ok_response ();
    }

    if (path.size () == 1) co_return error_response (405, m, problem::invalid_method, "use put");

    const UTF8 &name = path[1];

    if (m == MAKE_WALLET) co_return error_response (501, m, problem::unimplemented);

    if (m == VALUE) co_return error_response (501, m, problem::unimplemented);

    if (m == DETAILS) co_return error_response (501, m, problem::unimplemented);

    if (m == RESTORE) co_return error_response (501, m, problem::unimplemented);

    if (m == TAXES) co_return error_response (501, m, problem::unimplemented);

    if (m == IMPORT) co_return error_response (501, m, problem::unimplemented);

    try {

        if (m == ADD_KEY) co_return error_response (501, m, problem::unimplemented);

        if (m == TO_PRIVATE) co_return error_response (501, m, problem::unimplemented);

        if (m == SPEND) co_return error_response (501, m, problem::unimplemented);

        if (m == ENCRYPT_KEY) co_return error_response (501, m, problem::unimplemented);

        if (m == DECRYPT_KEY) co_return error_response (501, m, problem::unimplemented);

        if (m == BOOST) co_return error_response (501, m, problem::unimplemented);

        if (m == SPLIT) co_return error_response (501, m, problem::unimplemented);

        co_return error_response (501, m, problem::unimplemented);

    } catch (crypto::entropy::fail) {
        co_return error_response (500, m, problem::need_entropy);
    }
}

processor::processor (const options &o) {
    SpendOptions = o.spend_options ();
    DB = load_DB (o);

    Entropy = std::make_shared<crypto::fixed_entropy> (bytes_view {});

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

void processor::add_entropy (const bytes &b) {
    Entropy->Entropy = b;
    Entropy->Position = 0;
}

Cosmos::spend_options options::spend_options () const {
    return Cosmos::spend_options {};
}

net::HTTP::response ok_response () {
    return net::HTTP::response (204);
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
    std::cout << " generated version response" << std::endl;
    return res;
}

net::HTTP::response error_response (unsigned int status, meth m, problem tt, const std::string &detail) {
    std::stringstream meth_string;
    meth_string << m;
    std::stringstream problem_type;
    problem_type << tt;

    throw exception {} << "this is the only way to shut down the program";

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
        case problem::invalid_type: return o << "invalid content-type";
        case problem::need_entropy: return o << "need entropy: please call add_entropy";
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
  <h1>Node Control Panel</h1>

  <details>
    <summary>/version</summary>
    <button onclick="callApi('POST', 'version')">Get Version</button>
    <pre id="output-version"></pre>
  </details>

  <details>
    <summary>/help</summary>
    <button onclick="callApi('GET', 'help')">Get Help</button>
    <pre id="output-help"></pre>
  </details>

  <details>
    <summary>/shutdown</summary>
    <button onclick="callApi('PUT', 'shutdown')">Shutdown</button>
    <pre id="output-shutdown"></pre>
  </details>

  <details>
    <summary>/add_entropy</summary>
    <button onclick="callApi('POST', 'add_entropy')">Add Entropy</button>
    <pre id="output-add_entropy"></pre>
  </details>

  <details>
    <summary>/list_wallets</summary>
    <button onclick="callApi('GET', 'list_wallets')">List Wallets</button>
    <pre id="output-list_wallets"></pre>
  </details>

  <script>
    async function callApi(http_method, endpoint) {
      try {
        const res = await fetch(`/${endpoint}`, {
          method: http_method
        });
        const text = await res.text();
        document.getElementById(`output-${endpoint}`).textContent = text;
      } catch (err) {
        document.getElementById(`output-${endpoint}`).textContent = 'Error: ' + err;
      }
    }
  </script>
</body>
</html>
)--";

    std::cout << "trying to return UI response" << std::endl;
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

