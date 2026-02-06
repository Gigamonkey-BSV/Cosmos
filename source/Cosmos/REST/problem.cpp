#include <Cosmos/REST/problem.hpp>
#include <data/io/exception.hpp>

namespace Cosmos {

    std::ostream &operator << (std::ostream &o, problem p) {
        switch (p) {
            case problem::unknown: return o << "unknown problem";
            case problem::invalid_method: return o << "invalid method";
            case problem::unknown_method: return o << "unknown method";
            case problem::invalid_content_type: return o << "invalid content-type";
            case problem::need_entropy: return o << "need entropy: please call add_entropy";
            case problem::invalid_target: return o << "could not read target to wallet resource";
            case problem::invalid_wallet_name: return o << "name argument required. Should be alpha alnum+";
            case problem::invalid_query: return o << "invalid query";
            case problem::invalid_expression: return o << "invalid expression";
            case problem::missing_parameter: return o << "missing parameter";
            case problem::invalid_parameter: return o << "invalid parameter";
            case problem::unexpected_parameter: return o << "unexpected parameter";
            case problem::failed: return o << "failed";
            case problem::unimplemented: return o << "unimplemented method";
            default: throw data::exception {} << "invalid problem...";
        }
    }

}
