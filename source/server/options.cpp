#include "options.hpp"
#include "method.hpp"

using uint16 = data::uint16;

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
            if (ec != std::errc ()) throw data::exception {} << "invalid port number " << pn;
        }
    }

    if (!bool (port_number)) throw data::exception {} << "No port number provided";

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
        else throw data::exception {} << "could not parse threads " << val;
    }

    return *threads;
}

db_options options::db_options () const {
    SQLite_options sqlite;

    maybe<std::string> db_type;
    this->get ("db_type", db_type);
    if (bool (db_type) && sanitize (*db_type) != "sqlite")
        throw data::exception {} << "only SQLite is supported as a database";

    bool param_in_memory = this->has ("sqlite_in_memory");

    this->get ("sqlite_path", sqlite.Path);
    if (!bool (sqlite.Path)) {
        const char *val = std::getenv ("COSMOS_SQLITE_PATH");
        if (bool (val)) sqlite.Path = filepath {val};
    }

    if (param_in_memory && bool (sqlite.Path)) 
        throw data::exception {} << "SQLite database set as in-memory but a path was also set.";

    if (!bool (sqlite.Path) && !param_in_memory)
        std::cout << "warning: SQLite database is in-memory. All information will be erased on program exit." << std::endl; 

    return sqlite;
}

Cosmos::spend_options options::spend_options () const {
    return Cosmos::spend_options {};
}
