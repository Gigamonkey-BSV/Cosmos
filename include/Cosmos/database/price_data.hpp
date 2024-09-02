#ifndef COSMOS_DATABASE_PRICE_DATA
#define COSMOS_DATABASE_PRICE_DATA

#include <Cosmos/network.hpp>

namespace Cosmos {

    struct price_data {
        virtual maybe<double> get (const Bitcoin::timestamp &t) = 0;
        virtual ~price_data () {}
    };

    struct local_price_data : price_data {
        virtual void set (const Bitcoin::timestamp &t, double) = 0;
        virtual ~local_price_data () {}
    };

    struct cached_remote_price_data final : price_data {
        network &Net;
        local_price_data &Local;
        maybe<double> get (const Bitcoin::timestamp &t) final override;
        cached_remote_price_data (network &n, local_price_data &l) : Net {n}, Local {l} {}
    };
}

#endif
