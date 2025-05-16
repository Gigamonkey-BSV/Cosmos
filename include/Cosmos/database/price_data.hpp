#ifndef COSMOS_DATABASE_PRICE_DATA
#define COSMOS_DATABASE_PRICE_DATA

#include <Cosmos/network.hpp>

namespace Cosmos {

    struct price_data {
        constexpr const static uint32 OneDay = 86400;

        virtual maybe<double> get_price (monetary_unit, const Bitcoin::timestamp &t) = 0;
        virtual ~price_data () {}
    };

    struct local_price_data : price_data {
        virtual void set_price (monetary_unit, const Bitcoin::timestamp &t, double) = 0;
        virtual ~local_price_data () {}
    };

    struct remote_price_data final : price_data {
        network &Net;
        remote_price_data (network &n) : Net {n} {}
        maybe<double> get_price (monetary_unit, const Bitcoin::timestamp &t) final override;
    };

    struct cached_remote_price_data final : price_data {
        ptr<price_data> Remote;
        local_price_data &Local;
        maybe<double> get_price (monetary_unit, const Bitcoin::timestamp &t) final override;
        cached_remote_price_data (ptr<price_data> n, local_price_data &l) : Remote {n}, Local {l} {}
    };

    // we ask the user for price data.
    struct ask_for_price_data final : price_data {
        ask_for_price_data () {}
        maybe<double> get_price (monetary_unit, const Bitcoin::timestamp &t) final override;
    };
}

#endif
