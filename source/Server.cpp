#include <charconv>

#include <cstdlib>  // for std::getenv
#include <laserpants/dotenv/dotenv.h>

#include <data/async.hpp>
#include <data/net/URL.hpp>
#include <data/net/JSON.hpp>
#include <data/net/HTTP_server.hpp>

// TODO: in here we have 'using namespace data' and that causes a
// problem if the includes are in the wrong order. This should be fixed. 
#include "Cosmos.hpp"
#include "server/server.hpp"

#include <Cosmos/options.hpp>

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
        (IO.get_executor (), endpoint, server {program_options});

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

