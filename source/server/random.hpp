#ifndef SERVER_RANDOM
#define SERVER_RANDOM

#include <data/crypto/random.hpp>
#include <Cosmos/random.hpp>
#include "options.hpp"

namespace Cosmos::random {
    const auto Personalization = data::bytes (data::string ("Cosmos Wallet alpha v2 meep moop!"));

    void setup (const options &);

    // this type exists to enable a mode with absolutely
    // deterministic RNG starting a given seed. This is
    // not what we would want with ordinary use of the
    // program. It would be used to produce a
    // replicatable condition for testing.

    // TODO: we should use a proper derivation function
    // so that it's not totally insecure like this.
    struct fixed_entropy final : data::random::source {
        bytes String;
        size_t Index;
        fixed_entropy (const bytes &b): String {b}, Index {0} {}
        void read (byte *result, size_t remaining) final override {
            while (remaining > 0) {
                size_t bytes_to_copy = remaining > (String.size () - Index) ? (String.size () - Index) : remaining;
                std::copy (String.begin () + Index, String.begin () + Index + bytes_to_copy, result);
                result += bytes_to_copy;
                remaining -= bytes_to_copy;
                if (Index == String.size ()) Index = 0;
            }
        }
    };

    extern ptr<user_entropy> UserEntropy;

    // statistical randomness, fast
    extern ptr<data::random::source> Casual;

    // cryptographic randomness, slow
    extern ptr<data::random::source> Secure;

}

#endif
