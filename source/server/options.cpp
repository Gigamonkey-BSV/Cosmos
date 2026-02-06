#include "options.hpp"
#include <Cosmos/REST/method.hpp>

using uint16 = data::uint16;

maybe<filepath> options::env () const {
    maybe<filepath> env_path;
    this->get ("env", env_path);
    return env_path;
}

bool options::offline () const {
    return this->has ("offline");
}

bool has_accept_remote (const args::parsed &o) {
    return o.has ("accept_remote");
}

maybe<net::IP::TCP::endpoint> read_endpoint_option (const args::parsed &o) {
    maybe<net::IP::TCP::endpoint> endpoint;
    o.get ("endpoint", endpoint);
    if (!bool (endpoint)) {
        const char* val = std::getenv ("COSMOS_WALLET_ENDPOINT");
        if (bool (val)) endpoint = net::IP::TCP::endpoint {val};
    }

    if (bool (endpoint) && !endpoint->valid ())
        throw data::exception {} << "invalid endpoint provided: " << *endpoint << 
            " (it should be something like \"tcp:\\127.0.0.1:4567\")";
    
    return endpoint;
}

maybe<net::IP::address> read_ip_address_option (const args::parsed &o) {
    maybe<net::IP::address> ip_address;
    
    o.get ("ip_address", ip_address);
    if (!bool (ip_address)) {
        const char *addr = std::getenv ("COSMOS_WALLET_IP_ADDRESS");

        if (bool (addr)) ip_address = net::IP::address {addr};
    }

    if (bool (ip_address) && !ip_address->valid ())
        throw data::exception {} << "invalid IP address provided: " << *ip_address;
    
    return ip_address;
}

maybe<uint16> read_port_option (const args::parsed &o) {
    maybe<uint16> port_number;

    o.get ("port", port_number);
    if (!bool (port_number)) {
        const char *pn = std::getenv ("COSMOS_WALLET_PORT_NUMBER");

        if (bool (pn)) {
            unsigned int read_port_number;
            auto [_, ec] = std::from_chars (pn, pn + std::strlen (pn), read_port_number);
            if (ec != std::errc ()) throw data::exception {} << "invalid port number " << pn;
            if (read_port_number > std::numeric_limits<uint16>::max ())
                throw data::exception {} << "Port number is too big to be a uint16";
            port_number = static_cast<uint16> (read_port_number);
        }
    }

    return port_number;
}

// an ip address can be provided as part of an endpoint, 
// as its own options, as an ENV option, or 120.0.0.1 by default.
net::IP::address options::ip_address () const {
    maybe<net::IP::TCP::endpoint> endpoint = read_endpoint_option (*this);
    maybe<net::IP::address> ip_address = read_ip_address_option (*this);

    // if these are both provided, they must be consistent. 
    if (bool (endpoint) && bool (ip_address) && endpoint->address () != *ip_address) 
        throw data::exception {} << 
            "Inconsistent ip address provided: " << endpoint->address () << " vs " << *ip_address;

    bool accept_remote = has_accept_remote (*this);

    net::IP::address localhost {"127.0.0.1"};
    
    // must be consistent with has_accept_remote.
    if (accept_remote) {
        if (bool (endpoint) && endpoint->address () == localhost || 
            bool (ip_address) && *ip_address == localhost) 
            throw data::exception {} << 
                "Cannot accept remote connections with IP address " << localhost;
    } 

    // if neither is provided, throw.
    if (!bool (endpoint) && !bool (ip_address)) {
        if (!accept_remote) return localhost;
        throw data::exception {} << "No IP address provided";
    }

    if (bool (ip_address)) return *ip_address;
    return endpoint->address ();

}

uint16 options::port () const {
    maybe<net::IP::TCP::endpoint> endpoint = read_endpoint_option (*this);
    maybe<uint16> port = read_port_option (*this);

    // if these are both provided, they must be consistent. 
    if (bool (endpoint) && bool (port) && endpoint->port () != *port) 
        throw data::exception {} << "Inconsistent ports provided: " << endpoint->port () << " vs " << *port;

    // if neither is provided, throw.
    if (!bool (endpoint) && !bool (port)) 
        throw data::exception {} << "No port provided.";
    
    if (bool (port)) return *port;
    return endpoint->port ();
}

net::IP::TCP::endpoint options::endpoint () const {
    return net::IP::TCP::endpoint {this->ip_address (), this->port ()};
}

bool options::local () const {
    bool is_offline = this->offline ();
    bool accept_remote = has_accept_remote (*this);

    // if accept remote is enabled, then we are not local. 
    if (is_offline && accept_remote) throw data::exception {} << "cannot be offline and accept remote connections at the same time.";

    // We do not need to know the ip_address, but this will throw 
    // an exception if there is a problem with an inconsistency with 
    // the ip address. 
    auto _ = this->ip_address ();

    return !accept_remote;
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
    if (bool (db_type) && Cosmos::sanitize (*db_type) != "sqlite")
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
        throw data::exception {} << "No SQLite database path provided.";

    if (param_in_memory)
        DATA_LOG (warning) << "WARNING: SQLite database is in-memory. All information will be erased on program exit.";

    return sqlite;
}

Cosmos::spend_options options::spend_options () const {
    return Cosmos::spend_options {};
}

maybe<bytes> options::nonce () const {
    maybe<data::hex_string> nonce;
    this->get ("nonce", nonce);
    if (bool (nonce)) return bytes (*nonce);

    const char* val = std::getenv ("COSMOS_NONCE");

    if (bool (val))
        return bytes (data::hex_string {val});

    return {};
}

maybe<bytes> options::seed () const {
    maybe<data::hex_string> seed;
    this->get ("seed", seed);
    if (bool (seed)) return bytes (*seed);

    const char* val = std::getenv ("COSMOS_SEED");

    if (bool (val))
        return bytes (data::hex_string {val});

    return {};
}

bool options::incorporate_user_entropy () const {
    return !this->has ("ignore_user_entropy");
}
