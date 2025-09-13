#include "to_private.hpp"
#include "method.hpp"

#include <data/tools/map_schema.hpp>

net::HTTP::response handle_to_private (
    server &p, net::HTTP::method http_method, map<UTF8, UTF8> query,
    const maybe<net::HTTP::content> &content_type, const data::bytes &body) {

    Diophant::symbol key_name = data::schema::validate<> (query, data::schema::key<Diophant::symbol> ("name")).Value;

    if (!key_name.valid ())
        return error_response (400, method::TO_PRIVATE, problem::invalid_parameter,
            "invalid parameter 'name'");

    if (http_method == net::HTTP::method::put) {

        if (!bool (content_type) || *content_type != net::HTTP::content::type::text_plain)
            return error_response (400, method::TO_PRIVATE, problem::invalid_content_type, "expected content-type:text/plain");

        key_expression key_expr {data::string (body)};

        if (p.DB->set_to_private (key_name, key_expr)) return ok_response ();
        return error_response (500, method::TO_PRIVATE, problem::failed, "could not set private key");
    } else if (http_method != net::HTTP::method::get) {
        key_expression pk = p.DB->get_to_private (key_name);
        if (!pk.valid ()) return error_response (404, method::TO_PRIVATE, problem::failed, "Could not retrieve key");
        return string_response (pk);
    } else return error_response (405, method::TO_PRIVATE, problem::invalid_method, "use put or get");
}
