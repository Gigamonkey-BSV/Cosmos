#include "key.hpp"
#include "method.hpp"
#include "server.hpp"
#include <data/tools/map_schema.hpp>

std::ostream &operator << (std::ostream &o, key_type k) {
    switch (k) {
        case key_type::secp256k1: return o << "secp256k1";
        case key_type::WIF: return o << "WIF";
        case key_type::xpriv: return o << "xpriv";
        default: return o << "invalid";
    }
}

std::istream &operator >> (std::istream &i, key_type &x) {
    std::string word;
    i >> word;
    if (!i) return i;
    std::string key_type_san = sanitize (word);
    if (key_type_san == "secp256k1") x = key_type::secp256k1;
    else if (key_type_san == "wif") x = key_type::WIF;
    else if (key_type_san == "xpriv") x = key_type::xpriv;
    else i.setstate (std::ios::failbit);
    return i;
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

    if (http_method != net::HTTP::method::get && http_method != net::HTTP::method::post)
        return error_response (405, method::KEY, problem::invalid_method, "use get or post");

    // if the method is get, then we do not have a body.
    if (http_method == net::HTTP::method::get && bool (content_type))
        return error_response (400, method::KEY, problem::invalid_query,
            "When method is GET, no body should be present");

    bool method_random = http_method == net::HTTP::method::post && !bool (content_type);

    // this parameter is always required.
    Diophant::symbol KeyName;

    // If we are going to generate a key randomly then these remaining parameters may be present.

    // key type is required if we are going to randomly generate.
    key_type KeyType;

    // these other two are not required and are not allowed if key type is secp256k1.
    bool Compressed {true};
    Bitcoin::net Net {Bitcoin::net::Main};

    // in these cases the parameter list is just the key name.
    if (http_method == net::HTTP::method::get || !method_random) {
        KeyName = data::schema::validate<> (query, data::schema::key<Diophant::symbol> ("name")).Value;
    } else {
        auto [qname, qtype, qnet, qcmp] = data::schema::validate<> (query,
            data::schema::key<Diophant::symbol> ("name") &
            data::schema::key<key_type> ("type") &
            *data::schema::key<Bitcoin::net> ("net") &
            *data::schema::key<bool> ("compressed"));

        KeyName = qname.Value;
        KeyType = qtype.Value;

        if (KeyType == key_type::secp256k1) {
            if (qcmp) error_response (400, method::KEY, problem::invalid_query,
                "When key type is secp256k1, parameter compressed is not allowed");
            if (qnet) error_response (400, method::KEY, problem::invalid_query,
                "When key type is secp256k1, parameter net is not allowed");
        } else {
            if (qcmp) Compressed = qcmp->Value;
            if (qnet) Net = qnet->Value;
        }
    }

    // make sure the name is a valid symbol name
    if (!KeyName.valid ())
        return error_response (400, method::KEY, problem::invalid_parameter,
            "parameter 'name' must be alpha alnum+");

    // if the method is get, then we're done here.
    if (http_method == net::HTTP::method::get) {

        key_expression secret = p.DB->get_key (wallet_name, KeyName);

        if (secret.valid ()) return string_response (string (secret));

        return error_response (404, method::KEY, problem::failed);
    }

    // now we have to generate this key expression somehow.
    key_expression key_expr;

    if (method_random) {
        data::entropy &random = p.get_secure_random ();
        secp256k1::secret key;
        random >> key.Value;

        switch (KeyType) {
            case (key_type::WIF): {
                key_expr = key_expression {Bitcoin::secret {Net, key, Compressed}};
            } break;
            case (key_type::xpriv): {
                HD::chain_code x;
                random >> x;
                key_expr = key_expression (HD::BIP_32::secret {key, x, Net});
            } break;
            default: {
                key_expr = key_expression {key};
            }
        }
    } else key_expr = key_expression {data::string (body)};

    if (p.DB->set_key (wallet_name, KeyName, key_expr)) {
        // we return the key if it was generated randomly.
        if (!method_random) return ok_response ();
        return string_response (std::string (key_expr));
    }

    return error_response (500, method::KEY, problem::failed, "could not create key");

}
