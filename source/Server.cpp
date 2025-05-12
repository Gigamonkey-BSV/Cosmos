
#include <data/io/arg_parser.hpp>
#include <data/io/error.hpp>
#include <data/async.hpp>
#include <data/net/URL.hpp>
#include <data/net/JSON.hpp>
#include <data/net/HTTP_server.hpp>
#include <Cosmos/database/txdb.hpp>

//#include <laserpants/dotenv/dotenv.h>
#include <cstdlib>  // for std::getenv

using namespace data;

// whether we have received a shutdown notice.
std::atomic<bool> should_exit {false};

void signal_handler (int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        should_exit = true;
    }
}

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
    net::port port () const;
    uint32 threads () const;
};

using error = io::error;

error run (const options &program_options);

int main (int arg_count, char **arg_values) {
    std::signal (SIGINT, signal_handler);
    std::signal (SIGTERM, signal_handler);

    error err = run (options {io::arg_parser {arg_count, arg_values}});

    if (bool (err)) {
        if (err.Message) std::cout << "Fail code " << err.Code << ": " << *err.Message << std::endl;
        else std::cout << "Fail code " << err.Code << "." << std::endl;
    }

    return err.Code;

}

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

void make_do (const options &program_options);

error run (const options &program_options) {
    return catch_all (&make_do, program_options);
}

ptr<Cosmos::local_TXDB> load_DB (options);

void make_do (const options &program_options) {
    auto db = load_DB (program_options);

    boost::asio::io_context io;

    uint32 num_threads = program_options.threads ();

    std::vector<std::thread> threads {};
    for (int i = 0; i < num_threads; i++) threads.emplace_back ([&]() {
        catch_all ([&io] () {
            io.run ();
        });
    });

    io.run ();  // Main thread also processes work

    for (int i = 0; i < num_threads; i++) threads[i].join ();
}

