
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

void signal_handler (int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        Shutdown = true;
        if (Server != nullptr) Server->close ();
    }
}

ptr<Cosmos::database> load_DB (const options &o);

struct processor {
    Cosmos::spend_options SpendOptions;

    processor (const options &o);

    awaitable<net::HTTP::response> operator () (const net::HTTP::request &);

    ptr<crypto::fixed_entropy> Entropy {nullptr};
    ptr<crypto::NIST::DRBG> SecureRandom {nullptr};
    ptr<crypto::linear_combination_random> CasualRandom {nullptr};

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

};

void run (const options &program_options) {
    auto envpath = program_options.env ();
    if (bool (envpath)) dotenv::init (envpath->c_str ());
    else dotenv::init ();

    net::IP::TCP::endpoint endpoint = program_options.endpoint ();
    if (endpoint.address () != net::IP::address {"127.0.0.1"})
        throw exception {} << "This program is not ready to be connected to the Internet: localhost is required";

    Server = std::make_shared<net::HTTP::server> (IO.get_executor (),
        endpoint,
        processor {program_options});

    uint32 num_threads = program_options.threads ();

    if (num_threads < 1) throw exception {} << "We cannot run with zero threads.";

    co_spawn (IO,
        [&] () -> awaitable<void> {
            while (co_await Server->accept ()) {}
        },
        boost::asio::detached
    );

    std::exception_ptr stored_exception = nullptr;
    std::mutex exception_mutex;

    std::vector<std::thread> threads {};
    for (int i = 0; i < num_threads; i++)
        threads.emplace_back ([&]() {
            try {
                while (!Shutdown) IO.run ();
            } catch (...) {
                std::lock_guard<std::mutex> lock (exception_mutex);
                if (!stored_exception) {
                    stored_exception = std::current_exception (); // Capture first exception
                }
            }
        });

    while (!Shutdown) IO.run ();

    for (int i = 0; i < num_threads; i++) threads[i].join ();
}

net::HTTP::response HTML_JS_UI_response ();
net::HTTP::response error_response (unsigned int status, meth m, const std::string & = "");
net::HTTP::response help_response (meth = UNSET);
net::HTTP::response version_response ();
net::HTTP::response ok_response ();
net::HTTP::response boolean_response (bool);
net::HTTP::response value_response (Bitcoin::satoshi);
net::HTTP::response JSON_response (const JSON &);

awaitable<net::HTTP::response> processor::operator () (const net::HTTP::request &req) {
    list<UTF8> path = req.Target.path ().read ('/');

    if (path.size () == 0) co_return HTML_JS_UI_response ();

    meth m = read_method (path[0]);

    if (m == UNSET) co_return error_response (400, m, std::string {"Unknown method "} + path[0]);

    if (m == VERSION) {
        if (req.Method != net::HTTP::method::get) co_return error_response (405, m, std::string {"use get"});
        co_return version_response ();
    }

    if (m == HELP) {
        if (req.Method != net::HTTP::method::get) co_return error_response (405, m, std::string {"use get"});
        if (path.size () == 1) co_return help_response ();
        else {
            meth help_with_method = read_method (path[1]);
            if (help_with_method == UNSET)
                co_return error_response (400, HELP, std::string {"Unknown method"} + path[1]);
            co_return help_response (help_with_method);
        }
    }

    if (m == SHUTDOWN) {
        if (req.Method != net::HTTP::method::put) co_return error_response (405, m, std::string {"use put"});
        Shutdown = true;
        co_return ok_response ();
    }

    if (m == ADD_ENTROPY) {
        if (req.Method != net::HTTP::method::post) co_return error_response (405, m, std::string {"use post"});
        maybe<net::HTTP::content> content_type = req.content_type ();
        if (!bool (content_type) || *content_type != net::HTTP::content::type::application_octet_stream)
            co_return error_response (400, m, "expected content-type:application/octet-stream");

        add_entropy (req.Body);

        co_return ok_response ();
    }

    if (path.size () == 1) co_return error_response (405, m, std::string {"use put"});

    const UTF8 &name = path[1];

    if (m == MAKE_WALLET) co_return error_response (501, m);

    if (m == ADD_KEY) co_return error_response (501, m);

    if (m == TO_PRIVATE) co_return error_response (501, m);

    if (m == ENCRYPT_KEY) co_return error_response (501, m);

    if (m == DECRYPT_KEY) co_return error_response (501, m);

    if (m == VALUE) co_return error_response (501, m);

    if (m == DETAILS) co_return error_response (501, m);

    if (m == SPEND) co_return error_response (501, m);

    if (m == IMPORT) co_return error_response (501, m);

    if (m == RESTORE) co_return error_response (501, m);

    if (m == BOOST) co_return error_response (501, m);

    if (m == SPLIT) co_return error_response (501, m);

    if (m == TAXES) co_return error_response (501, m);

    co_return error_response (501, m);
}

processor::processor (const options &o) {
    SpendOptions = o.spend_options ();
    DB = load_DB (o);

    // TODO set up random numbers and entropy.

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

    const char* val = std::getenv ("COSMOS_ENDPOINT");

    if (bool (val)) return net::IP::TCP::endpoint {val};

    return net::IP::TCP::endpoint {"127.0.0.1:3456"};
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

net::HTTP::response error_response (unsigned int status, meth m, const std::string &) {
    throw method::unimplemented {"error_response"};
}

net::HTTP::response HTML_JS_UI_response () {
    throw method::unimplemented {"HTML UI"};
}

net::HTTP::response help_response (meth) {
    throw method::unimplemented {"help response"};
}

net::HTTP::response ok_response () {
    throw method::unimplemented {"ok_response"};
}

net::HTTP::response version_response () {
    throw method::unimplemented {"version_response"};
}

