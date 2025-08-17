#ifndef SERVER_OPTIONS
#define SERVER_OPTIONS

#include "db.hpp"

#include <Cosmos/types.hpp>
#include <data/io/arg_parser.hpp>

using arg_parser = data::io::arg_parser;
using uint16 = data::uint16;

struct options : arg_parser {
    options (arg_parser &&ap) : arg_parser {ap} {}

    // path to an env file containing program options. 
    maybe<filepath> env () const;
    
    uint32 threads () const;

    ::db_options db_options () const;

    // if the wallet is offline, then commands that 
    // require an internet connection won't work. 
    bool offline () const;

    // whether we accept connections over the Internet.
    bool local () const;

    net::IP::address ip_address () const;
    uint16 port () const;

    net::IP::TCP::endpoint endpoint () const;

    Cosmos::spend_options spend_options () const;
};

#endif
