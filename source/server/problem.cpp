#include "problem.hpp"

std::ostream &operator << (std::ostream &o, problem p) {
    switch (p) {
        case problem::unknown_method: return o << "unknown method";
        case problem::invalid_method: return o << "invalid method";
        case problem::invalid_content_type: return o << "invalid content-type";
        case problem::need_entropy: return o << "need entropy: please call add_entropy";
        case problem::invalid_name: return o << "name argument required. Should be alpha alnum+";
        case problem::unimplemented: return o << "unimplemented method";
        default: throw data::exception {} << "invalid problem...";
    }
}

net::HTTP::response error_response (unsigned int status, meth m, problem tt, const std::string &detail) {
    std::stringstream meth_string;
    meth_string << m;
    std::stringstream problem_type;
    problem_type << tt;

    JSON err {
        {"method", meth_string.str ()},
        {"status", status},
        {"title", problem_type.str ()}};

    if (detail != "") err["detail"] = detail;

    return net::HTTP::response (status, {{"content-type", "application/problem+json"}}, bytes (data::string (err.dump ())));
}

net::HTTP::response help_response (meth m) {
    std::stringstream ss;
    help (ss, m);
    return net::HTTP::response (200, {{"content-type", "text/plain"}}, bytes (data::string (ss.str ())));
}

