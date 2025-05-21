
#include <data/io/arg_parser.hpp>
#include <data/io/error.hpp>
#include <data/async.hpp>
#include <data/net/URL.hpp>
#include <data/net/JSON.hpp>
#include <data/net/HTTP_server.hpp>
#include <Cosmos/database.hpp>

//#include <laserpants/dotenv/dotenv.h>
#include <cstdlib>  // for std::getenv

using namespace data;
namespace Bitcoin = Gigamonkey::Bitcoin;

struct options : io::arg_parser {
    options (io::arg_parser &&ap) : io::arg_parser {ap} {}

    struct JSON_DB_options {
        std::string Path; // should be a directory
    };

    struct SQLite_options {
        maybe<std::string> Path; // missing for in-memory
    };

    struct MongoDB_options {
        net::URL URL;
        std::string UserName;
        std::string Password;
    }; // not yet supported.

    either<JSON_DB_options, SQLite_options, MongoDB_options> db_options () const;

    bool is_local () const;
    uint16 port_number () const;
    uint32 threads () const;
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

ptr<Cosmos::database> load_DB (options);

struct instruction : JSON {};

struct processor {
    ptr<Cosmos::database> DB;
    awaitable<net::HTTP::response> operator () (const net::HTTP::request &);
    processor (ptr<Cosmos::database> db): DB {db} {}
    awaitable<bool> add_entropy (const instruction &);
    awaitable<bool> make_wallet (const instruction &);
    awaitable<bool> add_key (const instruction &);
    awaitable<bool> to_private (const instruction &);
    awaitable<Bitcoin::satoshi> value (const instruction &);
};

void run (const options &program_options) {
    if (!program_options.is_local ())
        throw exception {} << "This program is not ready to be connected to the Internet.";

    Server = std::make_shared<net::HTTP::server> (IO.get_executor (),
        net::IP::TCP::endpoint {
            net::IP::address {"127.0.0.1"},
            program_options.port_number ()},
        processor {load_DB (program_options)});

    uint32 num_threads = program_options.threads ();

    if (num_threads < 1) throw exception {} << "We cannot run with zero threads.";
    if (num_threads > 1) throw exception {} << "Only one thread allowed for now.";

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

net::HTTP::response error_response ();
net::HTTP::response help_response (const command &comm);
net::HTTP::response version_response ();
net::HTTP::response shutdown_response ();
net::HTTP::response boolean_response (method, bool);
net::HTTP::response value_response (Bitcoin::satoshi);

awaitable<net::HTTP::response> processor::operator () (const net::HTTP::request &req) {
    command comm {req};
    if (!comm.valid ()) co_return error_response ();
    switch (comm.method ()) {
        case (method::VERSION): co_return version_response ();
        case (method::HELP): co_return help_response (comm);
        case (method::SHUTDOWN): {
            Shutdown = true;
            co_return shutdown_response ();
        }
        case (method::ADD_ENTROPY): co_return boolean_response (method::ADD_ENTROPY, co_await add_entropy (comm.instruction ()));
        case (method::MAKE_WALLET): co_return boolean_response (method::MAKE_WALLET, co_await make_wallet (comm.instruction ()));
        case (method::ADD_KEY): co_return boolean_response (method::ADD_KEY, co_await add_key (comm.instruction ()));
        case (method::TO_PRIVATE): co_return boolean_response (method::TO_PRIVATE, co_await to_private (comm.instruction ()));
        case (method::VALUE): co_return value_response (co_await value (comm.instruction ()));
        default: co_return error_response ();
    }
}
