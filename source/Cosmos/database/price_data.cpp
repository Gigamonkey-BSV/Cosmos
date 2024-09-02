#include <Cosmos/database/price_data.hpp>

namespace Cosmos {

    maybe<double> cached_remote_price_data::get (const Bitcoin::timestamp &t) {
        auto v = Local.get (t);
        if (bool (v)) return *v;
        double p = Net.price (t);
        Local.set (t, p);
        return p;
    }
}
