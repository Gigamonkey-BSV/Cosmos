#ifndef SERVER_PROBLEM
#define SERVER_PROBLEM

#include <iostream>

namespace Cosmos {

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
}

#endif
