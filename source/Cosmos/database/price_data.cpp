#include <Cosmos/database/price_data.hpp>
#include <data/io/wait_for_enter.hpp>

namespace Cosmos {

    maybe<double> remote_price_data::get (const Bitcoin::timestamp &t) {
        try {
            return synced (&network::price, &Net, t);
        } catch (const net::HTTP::exception &) {
            return {};
        }
    }

    maybe<double> cached_remote_price_data::get (const Bitcoin::timestamp &t) {
        auto v = Local.get (t);
        if (bool (v)) return *v;
        maybe<double> p = Remote->get (t);
        if (!bool (p)) return {};
        Local.set (t, *p);
        return p;
    }

    maybe<double> ask_for_price_data::get (const Bitcoin::timestamp &t) {
        std::stringstream ss;
        ss << "What was BSV/USD on " << t << std::endl;
        return data::read_decimal (ss.str ());
    }
}
