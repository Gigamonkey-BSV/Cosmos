#include "random.hpp"

namespace Cosmos::random {

    // statistical randomness, fast
    ptr<data::random::source> Casual;

    // cryptographic randomness, slow
    ptr<data::random::source> Secure;

    // our source of strong entropy
    ptr<data::random::source> StrongEntropy;

    ptr<data::random::source> WeakEntropy;

    ptr<user_entropy> UserEntropy {};

}

namespace data::random {

    source &get () {
        return *Cosmos::random::Casual;
    }

}

namespace data::crypto::random {

    source &get () {
        return *Cosmos::random::Secure;
    }

}

using bytes = data::bytes;
template <typename X> using maybe = data::maybe<X>;

namespace Cosmos::random {

    void setup (const options &program_options)
    {
        DATA_LOG (debug) << "setting up random generator ";
        // if a seed is provided, we use fixed entropy.
        // TODO use a derivation function.
        maybe<bytes> user_seed = program_options.seed ();
        if (bool (user_seed)) {
            Cosmos::random::StrongEntropy = std::static_pointer_cast<data::random::source> (
                std::make_shared<Cosmos::random::fixed_entropy> (*user_seed));

            Cosmos::random::WeakEntropy = Cosmos::random::StrongEntropy;
        } else {
            Cosmos::random::StrongEntropy = std::static_pointer_cast<data::random::source> (
                std::make_shared<data::crypto::random::OS_entropy_strong> ());

            Cosmos::random::WeakEntropy = std::static_pointer_cast<data::random::source> (
                std::make_shared<data::crypto::random::OS_entropy> ());
        }
        DATA_LOG (debug) << "random generator: got seed";
        bytes nonce;

        // if nonce is not provided, we generate it from our core entropy.
        maybe<bytes> user_nonce = program_options.nonce ();
        if (bool (user_nonce)) nonce = *user_nonce;
        else {
            nonce.resize (8);
            *Cosmos::random::WeakEntropy >> nonce;
        }
        DATA_LOG (debug) << "random generator: got nonce";
        // incorporate user entropy is the default.
        // we hash each http call from the user and
        // include it as additional entropy the next
        // time the RNG is called, thus providing
        // some degree of forward security.
        bool incorporate_user_entropy = program_options.incorporate_user_entropy ();
        if (incorporate_user_entropy) {
            Cosmos::random::UserEntropy = std::make_shared<Cosmos::random::user_entropy> ();

            Cosmos::random::Secure = std::static_pointer_cast<data::random::source> (
                std::shared_ptr<data::crypto::random::default_secure_random> (
                    new data::random::automatic_reseed<
                        crypto::NIST::auto_generate_with_additional_entropy<
                            crypto::NIST::HMAC_DRBG<crypto::hash::SHA2_256>>> {
                        *Cosmos::random::StrongEntropy, 1 << 30,
                        *Cosmos::random::UserEntropy,
                        data::byte_slice (nonce),
                        data::byte_slice (Cosmos::random::Personalization)}));

        } else Cosmos::random::Secure = std::static_pointer_cast<data::random::source> (
            std::shared_ptr<data::random::automatic_reseed<crypto::NIST::HMAC_DRBG<crypto::hash::SHA2_256>>> (
                new data::random::automatic_reseed<crypto::NIST::HMAC_DRBG<crypto::hash::SHA2_256>> {
            *Cosmos::random::StrongEntropy, 1 << 30,
            data::byte_slice (nonce),
            data::byte_slice (Cosmos::random::Personalization)}));
        DATA_LOG (debug) << "random generator: setup secure random";
        if (bool (user_seed) && !incorporate_user_entropy)
            DATA_LOG (warning) << "WARNING: All random number generation is totally deterministic.";

        // finally, we use our secure random to seed the casual random generator.
        Cosmos::random::Casual = std::shared_ptr<data::crypto::random::default_casual_random> (
            new data::crypto::random::default_casual_random {*Cosmos::random::Secure, 1 << 30});
        DATA_LOG (debug) << "random generator: setup casual random";
    }
}
