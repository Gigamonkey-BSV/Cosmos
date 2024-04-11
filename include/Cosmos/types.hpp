#ifndef COSMOS_TYPES
#define COSMOS_TYPES

#include <data/tools.hpp>
#include <data/math.hpp>
#include <data/numbers.hpp>
#include <Gigamonkey.hpp>

namespace Cosmos {
    using namespace data;
    namespace Bitcoin = Gigamonkey::Bitcoin;
    namespace Merkle = Gigamonkey::Merkle;
    namespace secp256k1 = Gigamonkey::secp256k1;
    using digest512 = Gigamonkey::digest512;
    using digest256 = Gigamonkey::digest256;
    using digest160 = Gigamonkey::digest160;

    struct inpoint : Bitcoin::outpoint {
        using Bitcoin::outpoint::outpoint;

        bool valid () const {
            return this->Digest.valid ();
        }

        explicit inpoint (const Bitcoin::outpoint &o) : outpoint {o} {}
    };
}

#endif
