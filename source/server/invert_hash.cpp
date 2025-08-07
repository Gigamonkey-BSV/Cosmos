#include "server.hpp"
#include "problem.hpp"
#include "method.hpp"

#include <gigamonkey/p2p/checksum.hpp>

using hash_function = Cosmos::hash_function;

template <size_t size>
std::function<bytes (data::byte_slice)> inline get_hash_fn (crypto::digest<size> (*hash_fn) (data::byte_slice)) {
    return [hash_fn] (data::byte_slice bb) -> bytes {
        crypto::digest<size> dig = hash_fn (bb);
        bytes d;
        d.resize (size);
        std::copy (dig.begin (), dig.end (), d.begin ());
        return d;
    };
}

net::HTTP::response invert_hash (server &p, 
    net::HTTP::method http_method, map<UTF8, UTF8> query,
    const maybe<net::HTTP::content> &content_type,
    const data::bytes &body) {

        hash_function HashFunction {0};

        enum class digest_format {
            unset, 
            HEX,
            BASE58_CHECK, 
            BASE64
        } DigestFormat {digest_format::unset};

        // which function to use. 
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
        } else return error_response (400, method::INVERT_HASH, problem::missing_parameter, "missing required parameter 'function'");

        if (http_method != net::HTTP::method::post && http_method != net::HTTP::method::get) 
            return error_response (405, method::INVERT_HASH, problem::invalid_method, "use post or get");

        const UTF8 *digest_format_param = query.contains ("digest_format");
        if (bool (digest_format_param)) {
            std::string sanitized = sanitize (*digest_format_param);
            if (sanitized == "hex") DigestFormat = digest_format::HEX;
            else if (sanitized == "base58check") DigestFormat = digest_format::BASE58_CHECK;
            else if (sanitized == "base64") DigestFormat = digest_format::BASE64;
            else return error_response (400, method::INVERT_HASH, problem::invalid_parameter, "invalid parameter 'digest_format'");
        } else if (http_method == net::HTTP::method::get) 
            return error_response (400, method::INVERT_HASH, problem::missing_parameter, "missing required parameter 'digest_format'");

        // the digest provided by the query. In the case of POST, this is optional. 
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
              } 
          }

          if (!bool (digest)) return error_response (400, method::INVERT_HASH, problem::invalid_parameter, "invalid parameter 'digest'");
        } else if (DigestFormat != digest_format::unset) 
            return error_response (400, method::INVERT_HASH, problem::missing_parameter, "missing required parameter 'digest'");
        
        data::maybe<const bytes &> data_to_hash;

        if (!bool (content_type)) {
            if (http_method == net::HTTP::method::post) 
                return error_response (400, method::INVERT_HASH, problem::invalid_query, "data to hash should be provided in body");
        } else if (*content_type != net::HTTP::content::type::application_octet_stream && *content_type != net::HTTP::content::type::text_plain) {
            return error_response (400, method::INVERT_HASH, problem::invalid_content_type, "either plain text or bytes");
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

            if (bool (digest) && hash_digest != *digest) 
                return error_response (400, method::INVERT_HASH, problem::invalid_query, 
                    "provided hash digest does not match calculated");

        }
        

        if (http_method == net::HTTP::method::post) {

        } else {
                
        }


        return error_response (501, method::INVERT_HASH, problem::unimplemented);
    }
