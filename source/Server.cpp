
#include <charconv>

#include <cstdlib>  // for std::getenv
#include <laserpants/dotenv/dotenv.h>

#include <sv/random.h>

#include <data/io/arg_parser.hpp>
#include <data/io/error.hpp>
#include <data/async.hpp>
#include <data/net/URL.hpp>
#include <data/net/JSON.hpp>
#include <data/net/HTTP_server.hpp>

#include <Cosmos/database.hpp>
#include <Cosmos/random.hpp>
#include <Cosmos/options.hpp>

#include <Cosmos/database/SQLite/SQLite.hpp>

using namespace data;
namespace Bitcoin = Gigamonkey::Bitcoin;

using filepath = Cosmos::filepath;

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
    bool offline ();
};

using error = io::error;

void run (const options &);

void signal_handler (int signal);

template <typename fun, typename ...args>
requires std::regular_invocable<fun, args...>
error catch_all (fun f, args... a) {
    try {
        std::invoke (std::forward<fun> (f), std::forward<args> (a)...);
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

struct instruction : JSON {};

struct processor {
    Cosmos::spend_options SpendOptions;

    processor (const options &o);

    awaitable<net::HTTP::response> operator () (const net::HTTP::request &);

    ptr<crypto::fixed_entropy> Entropy {nullptr};
    ptr<crypto::NIST::DRBG> SecureRandom {nullptr};
    ptr<crypto::linear_combination_random> CasualRandom {nullptr};

    awaitable<bool> add_entropy (const instruction &);

    ptr<Cosmos::database> DB;
    awaitable<bool> make_wallet (const instruction &);
    awaitable<bool> add_key (const instruction &);
    awaitable<bool> to_private (const instruction &);

    awaitable<Bitcoin::satoshi> value (const instruction &);

    struct wallet_info {
        Bitcoin::satoshi Value;
        uint32 Outputs;
        Bitcoin::satoshi MaxOutput;

        operator JSON () const;
    };

    awaitable<wallet_info> details (const instruction &);

    map<Bitcoin::outpoint, Bitcoin::output> account (const instruction &);

    tuple<Bitcoin::TXID, wallet_info> spend (const instruction &);

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

enum class method {
    UNSET,
    HELP,           // print help messages
    VERSION,        // print a version message
    SHUTDOWN,
    ADD_ENTROPY,    // add entropy to the random number generator.
    MAKE_WALLET,
    ADD_KEY,
    TO_PRIVATE,
    GENERATE,       // generate a wallet
    RESTORE,        // restore a wallet
    VALUE,          // the value in the wallet.
    DETAILS,
    SPEND,          // check pending txs for having been mined. (depricated)
    REQUEST,        // request a payment
    ACCEPT,         // accept a payment
    PAY,            // make a payment.
    SIGN,           // sign an unsigned transaction
    IMPORT,         // import a utxo with private key
    BOOST,          // boost some content
    SPLIT,          // split your wallet into tiny pieces for privacy.
    TAXES,          // calculate income and capital gain for a given year.
    ENCRYPT_KEY,
    DECRYPT_KEY
};

struct command : net::HTTP::request {
    command (const net::HTTP::request &);
    bool valid () const;
    ::method method () const;
    ::instruction instruction () const;
    JSON operator [] (const uint32) const;
    JSON operator [] (const char *) const;
};

net::HTTP::response error_response (unsigned int);
net::HTTP::response help_response (const instruction &);
net::HTTP::response version_response ();
net::HTTP::response shutdown_response ();
net::HTTP::response boolean_response (bool);
net::HTTP::response value_response (Bitcoin::satoshi);
net::HTTP::response JSON_response (const JSON &);

awaitable<net::HTTP::response> processor::operator () (const net::HTTP::request &req) {
    command comm {req};
    if (!comm.valid ()) co_return error_response (404);
    switch (comm.method ()) {
        case (method::VERSION): co_return version_response ();
        case (method::HELP): co_return help_response (comm.instruction ());
        case (method::SHUTDOWN): {
            Shutdown = true;
            co_return shutdown_response ();
        }
        case (method::ADD_ENTROPY): co_return boolean_response (co_await add_entropy (comm.instruction ()));
        case (method::MAKE_WALLET): co_return boolean_response (co_await make_wallet (comm.instruction ()));
        case (method::ADD_KEY): co_return boolean_response (co_await add_key (comm.instruction ()));
        case (method::TO_PRIVATE): co_return boolean_response (co_await to_private (comm.instruction ()));
        case (method::VALUE): co_return value_response (co_await value (comm.instruction ()));
        case (method::DETAILS): co_return JSON_response (co_await details (comm.instruction ()));
        case (method::SPEND):
        case (method::REQUEST):
        case (method::ACCEPT):
        case (method::PAY):
        case (method::SIGN):
        case (method::IMPORT):
        case (method::BOOST):
        case (method::SPLIT):
        case (method::TAXES):
        case (method::ENCRYPT_KEY):
        case (method::DECRYPT_KEY): co_return error_response (501);
        default: co_return error_response (400);
    }
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

