#include <Cosmos/random.hpp>
#include <sv/random.h>

template <typename X> using ptr = data::ptr<X>;
using byte = data::byte;
using uint32 = data::uint32;
using uint64 = data::uint64;

namespace Cosmos {

    ptr<crypto::user_entropy> Entropy {nullptr};
    ptr<crypto::NIST::DRBG> SecureRandom {nullptr};
    ptr<crypto::linear_combination_random> CasualRandom {nullptr};

    crypto::entropy *get_random () {
        return get_secure_random ();
    }

    crypto::entropy *get_secure_random () {

        if (!SecureRandom) {

            Entropy = std::make_shared<crypto::user_entropy> (
                "We need some entropy for this operation. Please type random characters.",
                "Thank you for your entropy so far. That was not enough. Please give us more random characters.",
                "Sufficient entropy provided.", std::cout, std::cin);

            SecureRandom = std::make_shared<crypto::NIST::DRBG> (crypto::NIST::DRBG::Hash, *Entropy, std::numeric_limits<uint32>::max ());
        }

        return SecureRandom.get ();

    }

    crypto::entropy *get_casual_random () {

        if (!CasualRandom) {
            if (!SecureRandom) get_secure_random ();

            Satoshi::RandomInit ();
            uint64 seed;
            Satoshi::GetStrongRandBytes ((byte *) &seed, 8);

            CasualRandom = std::make_shared<crypto::linear_combination_random> (256,
                std::static_pointer_cast<crypto::entropy> (std::make_shared<crypto::std_random<std::default_random_engine>> (seed)),
                std::static_pointer_cast<crypto::entropy> (SecureRandom));
        }

        return CasualRandom.get ();

    }

}
