#include "key.hpp"
#include "method.hpp"
#include "server.hpp"

std::ostream &operator << (std::ostream &o, key_type k) {
    switch (k) {
        case key_type::secp256k1: return o << "secp256k1";
        case key_type::WIF: return o << "WIF";
        case key_type::xpriv: return o << "xpriv";
        default: return o << "invalid";
    }
}

key_request_options::operator net::HTTP::request () const {

    if (!bool (HTTPMethod)) throw data::exception {} << "No HTTP method provided for method key";

    if (*HTTPMethod == net::HTTP::method::post) {
        if (bool (Body) && bool (KeyType))
            throw data::exception {} << "with HTTP method POST in method key, either HTTP body or parameter key type should be provided, not both";
        if (!bool (Body) && !bool (KeyType))
            throw data::exception {} << "with HTTP method POST in method key, either an HTTP body or parameter type key must be provided";
    } else if (*HTTPMethod == net::HTTP::method::get) {
        if (bool (Body) || bool (KeyType))
            throw data::exception {} << "HTTP body and parameter type are only allowed with HTTP method POST";
    } else throw data::exception {} << "Only GET or POST is allowed with method key";

    auto make = net::HTTP::request::make {}.path (string::write ("/key/", WalletName)).host ("localhost").method (*HTTPMethod);
    std::stringstream query_stream;
    query_stream << "name=" << KeyName;

    if (bool (KeyType)) {
        query_stream << "&type=" << *KeyType;
        if (*KeyType != key_type::unset && *KeyType != key_type::secp256k1) {
            if (Net == Bitcoin::net::Main) query_stream << "&net=main";
            else query_stream << "&net=test";
        }
    }

    if (bool (Body)) make = make.body (Body);

    return make.query (query_stream.str ());
}

net::HTTP::response handle_key (server &p,
    const Diophant::symbol &wallet_name,
    net::HTTP::method http_method, map<UTF8, UTF8> query,
    const maybe<net::HTTP::content> &content_type,
    const data::bytes &body) {

    const UTF8 *key_name_param = query.contains ("name");
    if (!bool (key_name_param))
        return error_response (400, method::KEY, problem::missing_parameter, "required parameter 'name' not present");

    Diophant::symbol key_name {*key_name_param};

    // make sure the name is a valid symbol name
    if (!key_name.valid ())
        return error_response (400, method::KEY, problem::invalid_parameter, "parameter 'name' must be alpha alnum+");

    key_type KeyType {key_type::unset};

    const UTF8 *key_type = query.contains ("type");
    if (bool (key_type)) {
        std::string key_type_san = sanitize (*key_type);
        if (key_type_san == "secp256k1") KeyType = key_type::secp256k1;
        else if (key_type_san == "wif") KeyType = key_type::WIF;
        else if (key_type_san == "xpriv") KeyType = key_type::xpriv;
        else if (key_type_san != "unset")
            return error_response (400, method::KEY, problem::invalid_query, "invalid parameter 'type'");
    }

    if (bool (content_type) && KeyType != key_type::unset)
        return error_response (400, method::KEY, problem::invalid_query);

    Bitcoin::net net = Bitcoin::net::Main;
    const UTF8 *net_type_param = query.contains ("net");
    if (bool (net_type_param)) {
        std::string net_type_san = sanitize (*net_type_param);
        if (net_type_san == "main") net = Bitcoin::net::Main;
        else if (net_type_san == "test") net = Bitcoin::net::Test;
        else return error_response (400, method::KEY, problem::invalid_query, "invalid parameter 'net'");
    }

    if (bool (content_type) && bool (net_type_param))
        return error_response (400, method::KEY, problem::invalid_query);

    bool compressed = true;
    const UTF8 *compressed_param = query.contains ("compressed");
    if (bool (compressed_param)) {
        maybe<bool> maybe_compressed = read_bool (*compressed_param);
        if (!maybe_compressed)
            return error_response (400, method::KEY, problem::invalid_query, "invalid parameter 'compressed'");
        compressed = *maybe_compressed;
    }

    if (bool (compressed_param) && (bool (content_type) ||
        (KeyType != key_type::WIF && KeyType != key_type::xpriv)))
        return error_response (400, method::KEY, problem::invalid_query);

    if (http_method == net::HTTP::method::get) {
        // when we get a key, none of these parameters are needed.
        if (KeyType != key_type::unset)
            return error_response (400, method::KEY, problem::invalid_query);

        if (bool (net_type_param))
            return error_response (400, method::KEY, problem::invalid_query);

        if (bool (compressed_param))
            return error_response (400, method::KEY, problem::invalid_query);

        if (bool (content_type))
            return error_response (400, method::KEY, problem::invalid_query);

        // TODO we expect to use content-type text/plain.
        return error_response (501, method::KEY, problem::unimplemented);

    } else if (http_method == net::HTTP::method::post) {

        key_expression key_expr;

        if (bool (content_type)) {
            if (KeyType != key_type::unset)
                return error_response (400, method::KEY, problem::invalid_query, "key type will be inferred from the expression provided");

            if (*content_type != net::HTTP::content::type::text_plain)
                return error_response (400, method::KEY, problem::invalid_content_type, "expected content-type:text/plain");

            key_expr = key_expression {data::string (body)};

        } else {
            if (KeyType == key_type::unset)
                return error_response (400, method::KEY, problem::invalid_query, "need a key type to tell us what to generate");

            if (bool (content_type))
                return error_response (400, method::KEY, problem::invalid_query, "no body when we generate random keys");

            data::entropy &random = p.get_secure_random ();
            secp256k1::secret key;
            random >> key.Value;

            switch (KeyType) {
                case (key_type::WIF): {
                    key_expr = key_expression {Bitcoin::secret {net, key, compressed}};
                } break;
                case (key_type::xpriv): {
                    HD::chain_code x;
                    random >> x;
                    key_expr = key_expression (HD::BIP_32::secret {key, x, net});
                } break;
                default: {
                    key_expr = key_expression {key};
                }
            }
        }

        if (p.DB->set_key (wallet_name, key_name, key_expr)) {
            // we return the key if it was generated randomly.
            if (bool (content_type)) return ok_response ();
            return string_response (std::string (key_expr));
        }
        return error_response (500, method::KEY, problem::failed, "could not create key");
    } else return error_response (405, method::KEY, problem::invalid_method, "use get or post");

}
