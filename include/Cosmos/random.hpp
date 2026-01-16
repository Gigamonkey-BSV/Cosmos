#ifndef COSMOS_RANDOM
#define COSMOS_RANDOM

#include <data/crypto/NIST_DRBG.hpp>
#include <data/crypto/random.hpp>
#include <gigamonkey/types.hpp>
#include <mutex>

namespace crypto = data::crypto;
using byte = std::uint8_t;
template <typename X> using ptr = data::ptr<X>;

namespace Cosmos::random {

    // we collect entropy from user input for the random number generator.
    // this does not need to be especially great entropy. It's just extra
    // for forward secrecy.
    struct user_entropy final : data::random::source, data::writer<byte> {
        data::uint32_little Counter {0};
        data::crypto::hash::SHA2_512 Hash {};

        user_entropy () {}

        void write (const byte *b, size_t x) final override {
            Hash.Update (b, x);
        }

        void read (byte *result, size_t remaining) final override {
            data::uint32_little counter = Counter;

            while (remaining > 0) {
                Hash.Update (counter.data (), 4);
                counter++;

                byte digest[64];
                Hash.Final (digest);

                size_t bytes_to_write = remaining > 64 ? 64 : remaining;
                std::copy (digest, digest + bytes_to_write, result);
                result += bytes_to_write;
                remaining -= bytes_to_write;

                Hash.Restart ();
                Hash.Update (digest, 64);
            }

            Counter++;
        }
    };

}

#endif
