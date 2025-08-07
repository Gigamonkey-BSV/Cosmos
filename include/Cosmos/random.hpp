#ifndef COSMOS_RANDOM
#define COSMOS_RANDOM

#include <data/crypto/NIST_DRBG.hpp>
#include <gigamonkey/types.hpp>
#include <mutex>

namespace crypto = data::crypto;

namespace Cosmos {
    // these functions are not provided by the library.
    crypto::entropy *get_random ();// depricated
    crypto::entropy *get_secure_random ();
    crypto::entropy *get_casual_random ();

    // Some stuff having to do with random number generators. We do not need 
    // strong cryptographic random numbers for boost. It is fine to use 
    // basic random number generators that you would use in a game or something. 
    
    struct random {
        
        virtual double range01 () = 0;

        virtual data::uint64 uint64 (data::uint64 max) = 0;

        virtual data::uint32 uint32 (data::uint32 max) = 0;

        data::uint64 uint64 () {
            return uint64 (std::numeric_limits<data::uint64>::max ());
        }

        data::uint32 uint32 () {
            return uint32 (std::numeric_limits<data::uint32>::max ());
        }

        virtual bool boolean () = 0;
        
        virtual ~random () {}
        
    };

    template <std::uniform_random_bit_generator engine>
    struct std_random : random, crypto::std_random<engine> {
        using crypto::std_random<engine>::std_random;

        static double range01 (engine &gen) {
            return std::uniform_real_distribution<double> {0.0, 1.0} (gen);
        }

        static data::uint64 uint64 (engine &gen, data::uint64 max) {
            return std::uniform_int_distribution<data::uint64> {
                std::numeric_limits<data::uint64>::min (), max} (gen);
        }

        static data::uint32 uint32 (engine &gen, data::uint32 max) {
            return std::uniform_int_distribution<data::uint32> {
                std::numeric_limits<data::uint32>::min (), max} (gen);
        }

        static bool boolean (engine &gen) {
            return static_cast<bool> (std::uniform_int_distribution<data::uint32> {0, 1} (gen));
        }

        double range01 () override {
            return range01 (crypto::std_random<engine>::Engine);
        }

        data::uint64 uint64 (data::uint64 max = std::numeric_limits<data::uint64>::max ()) override {
            return uint64 (crypto::std_random<engine>::Engine, max);
        }

        data::uint32 uint32 (data::uint32 max = std::numeric_limits<data::uint32>::max ()) override {
            return uint32 (crypto::std_random<engine>::Engine, max);
        }

        bool boolean () override {
            return boolean (crypto::std_random<engine>::Engine);
        }

    };

    using casual_random = std_random<std::default_random_engine>;
    using NIST_DRBG_random = std_random<data::crypto::NIST::DRBG>;
    
    template <std::uniform_random_bit_generator engine>
    class random_threadsafe : random {
        std_random<engine> Random;
        std::mutex Mutex;
        
        double range01 () override {
            std::lock_guard<std::mutex> Lock (Mutex);
            return Random.range01 ();
        }

        data::uint64 uint64 (data::uint64 max) override {
            std::lock_guard<std::mutex> Lock (Mutex);
            return Random.uint64 (max);
        }

        data::uint32 uint32 (data::uint32 max) override {
            std::lock_guard<std::mutex> Lock (Mutex);
            return Random.uint32 (max);
        }

        bool boolean () override {
            std::lock_guard<std::mutex> Lock (Mutex);
            return Random.boolean ();
        }
        
        random_threadsafe () : random {} {}
        random_threadsafe (data::uint64 seed) : random {seed} {}

    };
    
    using casual_random_threadsafe = random_threadsafe<std::default_random_engine>;
    using crypto_random_threadsafe = random_threadsafe<data::crypto::NIST::DRBG>;
    
}

#endif
