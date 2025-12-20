#include "key.hpp"
#include "method.hpp"
#include "server.hpp"
#include <data/tools/map_schema.hpp>

namespace Gigamonkey::Bitcoin {
    std::istream &operator >> (std::istream &i, net &x) {
        std::string word;
        i >> word;
        if (!i) return i;
        std::string key_type_san = sanitize (word);
        if (key_type_san == "main") x = net::Main;
        else if (key_type_san == "test") x = net::Test;
        else i.setstate (std::ios::failbit);
        return i;
    }
}

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
            if (Net == Bitcoin::net::Main) query_stream << "&net=Main";
            else query_stream << "&net=Test";
        }
    }

    if (bool (Body)) make = make.body (Body);

    return make.query (query_stream.str ());
}

net::HTTP::response handle_key (server &p,
    const Diophant::symbol &wallet_name,
    net::HTTP::method http_method, map<UTF8, UTF8> query,
    const maybe<net::HTTP::content> &post_key,
    const data::bytes &body) {

    if (bool (post_key) && http_method != net::HTTP::method::post)
        return error_response (405, method::KEY, problem::invalid_method, "use POST");

    auto [KeyName, method_random] = data::schema::validate<> (query,
        data::schema::key<Diophant::symbol> ("name") &&
        *(data::schema::key<key_type> ("type") &&
            *data::schema::key<Bitcoin::net> ("net") &&
            *data::schema::key<bool> ("compressed")));

    // make sure the name is a valid symbol name
    if (!KeyName.valid ())
        return error_response (400, method::KEY, problem::invalid_parameter,
            "parameter 'name' must be alpha alnum+");

    // method is GET
    if (!bool (method_random) && !bool (post_key)) {

        if (http_method != net::HTTP::method::get)
            return error_response (405, method::KEY, problem::invalid_method, "use GET");

        key_expression secret = p.DB->get_key (wallet_name, KeyName);

        if (secret.valid ()) return string_response (string (secret));

        return error_response (404, method::KEY, problem::failed);
    }

    // if method is not GET, then it must be POST.
    if (http_method != net::HTTP::method::post)
        return error_response (405, method::KEY, problem::invalid_method, "use POST");

    if (bool (method_random) && bool (post_key))
        return error_response (400, method::KEY, problem::invalid_query,
            "If you provide a 'type' parameter, we assume you want to generate a key randomly. "
            "If that is what you want, then don't provide a key in the body.");

    // we have now established that either a key has been
    // posted in the body or the generation method is random.

    if (bool (post_key)) {
        key_expression key_expr = key_expression {data::string (body)};

        if (p.DB->set_key (wallet_name, KeyName, key_expr))
            return string_response (std::string (key_expr));

        return error_response (500, method::KEY, problem::failed, "could not create key");
    }

    // now we know that the method is random.

    auto [KeyType, net_param, compressed_param] = *method_random;

    if (KeyType == key_type::secp256k1) {
        if (bool (compressed_param)) error_response (400, method::KEY, problem::invalid_query,
            "When key type is secp256k1, parameter compressed is not allowed");
        if (bool (net_param)) error_response (400, method::KEY, problem::invalid_query,
            "When key type is secp256k1, parameter net is not allowed");
    }

    bool Compressed = bool (compressed_param) ? *compressed_param : true;
    Bitcoin::net Net = net_param ? *net_param : Bitcoin::net::Main;

    data::entropy &random = p.get_secure_random ();
    secp256k1::secret key;
    random >> key.Value;

    key_expression key_expr;
    switch (KeyType) {
        case (key_type::WIF): {
            key_expr = key_expression {Bitcoin::secret {Net, key, Compressed}};
        } break;
        // TODO: I think HD keys have a way of using the compressed parameter as well.
        case (key_type::xpriv): {
            HD::chain_code x;
            random >> x;
            key_expr = key_expression (HD::BIP_32::secret {key, x, Net});
        } break;
        default: {
            key_expr = key_expression {key};
        }
    }

    if (p.DB->set_key (wallet_name, KeyName, key_expr)) {
        if (!method_random) return ok_response ();
        else return string_response (key_expr);
    }

    return error_response (500, method::KEY, problem::failed, "could not create key");

}
