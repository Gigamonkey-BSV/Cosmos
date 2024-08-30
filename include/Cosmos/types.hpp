#ifndef COSMOS_TYPES
#define COSMOS_TYPES

#include <data/tools.hpp>
#include <data/math.hpp>
#include <data/numbers.hpp>
#include <Gigamonkey.hpp>
#include <gigamonkey/script/pattern/pay_to_address.hpp>

namespace Cosmos {
    using namespace data;
    namespace Bitcoin = Gigamonkey::Bitcoin;
    namespace Merkle = Gigamonkey::Merkle;
    namespace secp256k1 = Gigamonkey::secp256k1;
    using digest512 = Gigamonkey::digest512;
    using digest256 = Gigamonkey::digest256;
    using digest160 = Gigamonkey::digest160;
    using pay_to_address = Gigamonkey::pay_to_address;

    // an inpoint is similar to an outpoint except that it points
    // to an input rather than to an output.
    struct inpoint : Bitcoin::outpoint {
        using Bitcoin::outpoint::outpoint;

        bool valid () const {
            return this->Digest.valid ();
        }

        inpoint (const Bitcoin::TXID &id, const Bitcoin::index &i) : Bitcoin::outpoint {id, i} {}
        explicit inpoint (const Bitcoin::outpoint &o) : outpoint {o} {}
    };
}

#endif
