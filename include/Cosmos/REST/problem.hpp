#ifndef SERVER_PROBLEM
#define SERVER_PROBLEM

#include "method.hpp"

namespace Cosmos::command {

    enum class problem {
        unknown,
        invalid_method,
        unknown_method,
        invalid_content_type,
        need_entropy,
        invalid_target,
        invalid_wallet_name,
        invalid_query,
        invalid_expression,
        missing_parameter,
        invalid_parameter,
        unexpected_parameter,
        failed,
        unimplemented
    };

    std::ostream &operator << (std::ostream &, problem);

    struct exception : data::exception::base<exception> {
        problem Problem;
        method Method;
        exception (int code, problem p, method m) : data::exception::base<exception> {code}, Problem {p}, Method {m} {}
    };
}

#endif
