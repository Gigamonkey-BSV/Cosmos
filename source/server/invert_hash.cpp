#include "server.hpp"
#include "problem.hpp"
#include "method.hpp"
#include "invert_hash.hpp"

#include <gigamonkey/p2p/checksum.hpp>

#include <data/net/REST.hpp>

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

    hash_function HashFunction {hash_function::invalid};

    // Which hash function do we use? 
    const UTF8 *hash_function_param = query.contains ("function");
    if (bool (hash_function_param)) {
        std::string sanitized = sanitize (*hash_function_param);
        if (sanitized == "sha1") HashFunction = hash_function::SHA1;
        else if (sanitized == "md5") HashFunction = hash_function::MD5;
        else if (sanitized == "sha2256") HashFunction = hash_function::SHA2_256;
        else if (sanitized == "sha2512") HashFunction = hash_function::SHA2_512;
        else if (sanitized == "sha3256") HashFunction = hash_function::SHA3_256;
        else if (sanitized == "sha3512") HashFunction = hash_function::SHA3_512;
        else if (sanitized == "ripmd160") HashFunction = hash_function::RIPEMD160;
        else if (sanitized == "hash256") HashFunction = hash_function::Hash256;
        else if (sanitized == "hash160") HashFunction = hash_function::Hash160;
        else return error_response (400, method::INVERT_HASH, problem::invalid_query, "invalid parameter 'function'");
    } else if (http_method == net::HTTP::method::post)
        return error_response (400, method::INVERT_HASH, problem::missing_parameter, "missing required parameter 'function'");

    digest_format DigestFormat {digest_format::unset};

    // there are some options in terms of how the hash digest is formatted.
    const UTF8 *digest_format_param = query.contains ("format");
    if (bool (digest_format_param)) {
        std::string sanitized = sanitize (*digest_format_param);
        if (sanitized == "hex") DigestFormat = digest_format::HEX;
        else if (sanitized == "base58check") DigestFormat = digest_format::BASE58_CHECK;
        else if (sanitized == "base64") DigestFormat = digest_format::BASE64;
        else return error_response (400, method::INVERT_HASH, problem::invalid_parameter, "invalid parameter 'format'");
    } else if (http_method == net::HTTP::method::get) 
        return error_response (400, method::INVERT_HASH, problem::missing_parameter, "missing required parameter 'format'");

    // the digest provided by the query. In the case of PUT, this is optional because it can be calculated.
    maybe<bytes> digest;

    const UTF8 *digest_param = query.contains ("digest");
    if (bool (digest_param)) {
        switch (DigestFormat) {
            case digest_format::HEX: {
                digest = encoding::hex::read (*digest_param);
            } break;
            case digest_format::BASE58_CHECK: {
                digest = Gigamonkey::base58::check::decode (*digest_param).payload ();
            } break;
            case digest_format::BASE64: {
                digest = encoding::base64::read (*digest_param);
            } break;
            default: return error_response (400, method::INVERT_HASH, problem::missing_parameter, "missing required parameter 'digest_format'");
        }

        if (!bool (digest)) return error_response (400, method::INVERT_HASH, problem::invalid_parameter, "invalid parameter 'digest'");
    } else if (DigestFormat != digest_format::unset) 
        return error_response (400, method::INVERT_HASH, problem::missing_parameter, "missing required parameter 'digest'");

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
