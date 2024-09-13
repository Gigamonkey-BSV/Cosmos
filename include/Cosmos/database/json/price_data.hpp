#ifndef COSMOS_DATABASE_JSON_PRICE_DATA
#define COSMOS_DATABASE_JSON_PRICE_DATA

#include <Cosmos/network.hpp>
#include <Cosmos/database/price_data.hpp>

namespace Cosmos {

    struct JSON_price_data : local_price_data {
        // TODO we should probably stick to price data for the day rather than by the second.
        std::map<Bitcoin::timestamp, double> Price;
        maybe<double> get (const Bitcoin::timestamp &t) final override;
        void set (const Bitcoin::timestamp &t, double) final override;
        JSON_price_data () {}
        JSON_price_data (const JSON &);
        explicit operator JSON () const;
    };
}

#endif

