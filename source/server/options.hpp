#ifndef SERVER_OPTIONS
#define SERVER_OPTIONS

#include "db.hpp"

#include <data/io/arg_parser.hpp>

using arg_parser = io::arg_parser;

struct options : arg_parser {
    options (arg_parser &&ap) : arg_parser {ap} {}

    // path to an env file containing program options. 
    maybe<filepath> env () const;

    ::db_options db_options () const;

    net::IP::TCP::endpoint endpoint () const;
    uint32 threads () const;
    bool offline () const;

    Cosmos::spend_options spend_options () const;
};

#endif