#ifndef SERVER_PROBLEM
#define SERVER_PROBLEM

#include "../Cosmos.hpp"

enum class problem {
    unknown_method,
    invalid_method,
    invalid_content_type,
    need_entropy,
    invalid_name,
    invalid_query,
    invalid_target,
    invalid_expression,
    missing_parameter,
    invalid_parameter,
    failed,
    unimplemented
};

std::ostream &operator << (std::ostream &, problem);

net::HTTP::response error_response (unsigned int status, meth m, problem, const std::string & = "");

net::HTTP::response help_response (meth = UNSET);

#endif
