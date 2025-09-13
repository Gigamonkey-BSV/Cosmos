#include "server.hpp"
#include "problem.hpp"
#include "method.hpp"
#include "invert_hash.hpp"

#include <gigamonkey/p2p/checksum.hpp>

#include <data/net/REST.hpp>

#include <data/tools/map_schema.hpp>

using hash_function = Cosmos::hash_function;

namespace base58 = Gigamonkey::base58;

std::ostream &operator << (std::ostream &o, digest_format form) {
    switch (form) {
        case digest_format::HEX: return o << "hex";
        case digest_format::BASE58_CHECK: return o << "base58_check";
        case digest_format::BASE64: return o << "base64";
        default: return o << "invalid";
    }
}

std::istream &operator >> (std::istream &i, digest_format &x) {
    std::string word;
    i >> word;
    if (!i) return i;
    std::string sanitized = sanitize (word);
    if (sanitized == "hex") x = digest_format::HEX;
    else if (sanitized == "base58check") x = digest_format::BASE58_CHECK;
    else if (sanitized == "base64") x = digest_format::BASE64;
    else i.setstate (std::ios::failbit);
    return i;
}

template <typename K, typename V> using map = data::map<K, V>;

invert_hash_request_options::operator net::HTTP::request () const {

    if (!bool (Method))
        throw data::exception {} << "No HTTP method provided for method invert_hash";

    auto req = net::HTTP::request::make ().method (*Method).path ("/invert_hash").host ("localhost");

    // if the HTTP method is PUT, then we need a body.
    if (*Method == net::HTTP::method::put) {
        if (!bool (Data))
            throw data::exception {} << "with HTTP method PUT in method invert_hash, a request body is required to provide the data to be hashed.";
        else req = req.body (*Data);

        if (Function == Cosmos::hash_function::invalid)
            throw data::exception {} << "No hash function provided for method invert_hash";

    // if the HTTP method is GET, then we must not have a body and we must have a digest.
    } else if (*Method == net::HTTP::method::get) {
        if (bool (Data))
            throw data::exception {} << "with HTTP method GET in invert_hash, a body was provided. "
                << "If we have a body already, why do we need to use this method?";
        if (!bool (Digest)) throw data::exception {} << "Digest is required for invert_hash HTTP method GET";
    } else throw data::exception {} << "Only GET or PUT is allowed with method invert_hash";

    data::dispatch<UTF8, UTF8> query;

    if (Function != ::hash_function::invalid) query <<= entry<UTF8, UTF8> {UTF8 {"function"}, string::write (Function)};

    // if a digest is provided, then a digest format must be provided.
    if (bool (Digest)) {
        query <<= entry<UTF8, UTF8> {UTF8 {"format"}, string::write (Format)};

        if (Digest->is<std::string> ()) {
            query <<= entry<UTF8, UTF8> {"digest", Digest->get<std::string> ()};
        } else {
            switch (Format) {
                case digest_format::HEX: {
                    query <<= entry<UTF8, UTF8> {UTF8 {"digest"}, encoding::hex::write (Digest->get<bytes> ())};
                    break;
                }
                case digest_format::BASE58_CHECK: {
                    query <<= entry<UTF8, UTF8> {UTF8 {"digest"}, UTF8 {base58::check (Base58CheckVersion, Digest->get<bytes> ())}};
                    break;
                }
                case digest_format::BASE64: {
                    query <<= entry<UTF8, UTF8> {UTF8 {"digest"}, encoding::base64::write (Digest->get<bytes> ())};
                    break;
                }
                default: throw data::exception {} << "No digest format provided for method invert_hash";
            }
        }
    }

    return req.query_map (query);
}

template <size_t size>
std::function<bytes (data::byte_slice)> inline get_hash_fn (data::digest<size> (*hash_fn) (data::byte_slice)) {
    return [hash_fn] (data::byte_slice bb) -> bytes {
        data::digest<size> dig = hash_fn (bb);
        bytes d;
        d.resize (size);
        std::copy (dig.begin (), dig.end (), d.begin ());
        return d;
    };
}

net::HTTP::response handle_invert_hash (server &p,
    net::HTTP::method http_method, map<UTF8, UTF8> query,
    const maybe<net::HTTP::content> &content_type,
    const data::bytes &body) {

    if (http_method != net::HTTP::method::put && http_method != net::HTTP::method::get)
        return error_response (405, method::INVERT_HASH, problem::invalid_method, "use put or get");

    // Required if method is PUT, optional otherwise.
    hash_function HashFunction {hash_function::invalid};

    // the other two are required if GET, optional otherwise.
    digest_format DigestFormat {digest_format::unset};

    // the digest provided by the query. In the case of PUT, this is optional because it can be calculated.
    maybe<bytes> digest;

    maybe<std::string> digest_string;

    if (http_method == net::HTTP::method::put) {
        auto [f, dig, fmt] = data::schema::validate<> (query,
            data::schema::key<hash_function> ("function") &
            *data::schema::key<std::string> ("digest") &
            *data::schema::key<digest_format> ("format"));

        HashFunction = f.Value;

        // if either of these is present, then they both most be.
        if (bool (dig) || bool (fmt)) {
            if (!(bool (dig) && bool (fmt)))
                return error_response (400, method::INVERT_HASH, problem::invalid_query,
                    "if parameter digest or format is provided, the other must also be provided");

            digest_string = dig->Value;
            DigestFormat = fmt->Value;
        }
    } else {
        auto [dig, fmt, hash_func_provided] = data::schema::validate<> (query,
            data::schema::key<std::string> ("digest") &
            data::schema::key<digest_format> ("format") &
            *data::schema::key<hash_function> ("function"));

        if (bool (hash_func_provided))
            HashFunction = hash_func_provided->Value;

        digest_string = dig.Value;
        DigestFormat = fmt.Value;
    }

    if (bool (digest_string)) {
        switch (DigestFormat) {
            case digest_format::HEX: {
                digest = encoding::hex::read (*digest_string);
            } break;
            case digest_format::BASE58_CHECK: {
                digest = Gigamonkey::base58::check::decode (*digest_string).payload ();
            } break;
            case digest_format::BASE64: {
                digest = encoding::base64::read (*digest_string);
            } break;
            default: return error_response (400, method::INVERT_HASH, problem::missing_parameter, "missing required parameter 'digest_format'");
        }

        if (!bool (digest)) return error_response (400, method::INVERT_HASH, problem::invalid_parameter, "invalid parameter 'digest'");
    }

    if (!bool (content_type)) {
        if (http_method == net::HTTP::method::put)
            return error_response (400, method::INVERT_HASH, problem::invalid_query, "data to hash should be provided in body");
    } else if (*content_type != net::HTTP::content::type::application_octet_stream && *content_type != net::HTTP::content::type::text_plain) {
        return error_response (400, method::INVERT_HASH, problem::invalid_content_type, "either plain text or bytes");
    } else if (http_method == net::HTTP::method::get) {
        return error_response (400, method::INVERT_HASH, problem::invalid_query, "body provided with GET");
    } else if (HashFunction == hash_function::invalid) {
        return error_response (400, method::INVERT_HASH, problem::invalid_query, "Body provided with invalid hash function");
    } else {
        std::function<bytes (const bytes &)> hash;

        switch (HashFunction) {
            case hash_function::SHA1: {
                hash = get_hash_fn (&crypto::SHA1);
            } break;
            case hash_function::SHA2_256: {
                hash = get_hash_fn (&crypto::SHA2_256);
            } break;
            case hash_function::SHA2_512: {
                hash = get_hash_fn (&crypto::SHA2_512);
            } break;
            case hash_function::SHA3_256: {
                hash = get_hash_fn (&crypto::SHA3_256);
            } break;
            case hash_function::SHA3_512: {
                hash = get_hash_fn (&crypto::SHA3_512);
            } break;
            case hash_function::RIPEMD160: {
                hash = get_hash_fn (&crypto::RIPEMD_160);
            } break;
            case hash_function::Hash256: {
                hash = get_hash_fn (&crypto::Bitcoin_256);
            } break;
            case hash_function::Hash160: {
                hash = get_hash_fn (&crypto::Bitcoin_160);
            } break;
            default: {
                return error_response (501, method::INVERT_HASH, problem::unimplemented, 
                    string::write ("hash function ", HashFunction, " is unimplemented"));
            }
        }

        bytes hash_digest = hash (body);

        if (!bool (digest)) digest = hash_digest;
        else if (hash_digest != *digest)
            return error_response (400, method::INVERT_HASH, problem::invalid_query, 
                "provided hash digest does not match calculated");
        
    }

    if (!bool (digest)) return error_response (400, method::INVERT_HASH, problem::invalid_query, "No digest provided");

    if (http_method == net::HTTP::method::put) {
        if (p.DB->set_invert_hash (*digest, HashFunction, body)) return ok_response ();
        return error_response (500, method::INVERT_HASH, problem::failed, "could not create key");
    } else {
        maybe<tuple<Cosmos::hash_function, bytes>> inverted = p.DB->get_invert_hash (*digest);
        if (!bool (inverted)) return error_response (404, method::INVERT_HASH, problem::failed, "Hash digest not found");

        Cosmos::hash_function return_hash_function = std::get<0> (*inverted);

        if (HashFunction != Cosmos::hash_function::invalid && std::get<0> (*inverted) != HashFunction)
            return error_response (500, method::INVERT_HASH, problem::failed, "inconsestent hash function");

        return data_response (std::get<1> (*inverted));
    }
}
