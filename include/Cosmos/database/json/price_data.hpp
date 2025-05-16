#ifndef COSMOS_DATABASE_JSON_PRICE_DATA
#define COSMOS_DATABASE_JSON_PRICE_DATA

#include <Cosmos/network.hpp>
#include <Cosmos/database/price_data.hpp>

namespace Cosmos {

    struct JSON_price_data : local_price_data {

        // TODO we should probably stick to price data for the day rather than by the second.
        map<Bitcoin::timestamp, double> Price;
        // return the nearest price record we have to within one day.
        maybe<double> get_price (monetary_unit, const Bitcoin::timestamp &t) final override;
        void set_price (monetary_unit, const Bitcoin::timestamp &t, double) final override;
        JSON_price_data () {}
        JSON_price_data (const JSON &);
        explicit operator JSON () const;
    };
}

#endif

