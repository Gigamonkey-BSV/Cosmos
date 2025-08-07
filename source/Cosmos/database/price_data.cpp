#include <Cosmos/database/price_data.hpp>
#include <data/io/wait_for_enter.hpp>

namespace Cosmos {

    maybe<double> remote_price_data::get_price (monetary_unit u, const Bitcoin::timestamp &t) {
        try {
            return data::synced (&network::price, &Net, u, t);
        } catch (const net::HTTP::exception &) {
            return {};
        }
    }

    maybe<double> cached_remote_price_data::get_price (monetary_unit u, const Bitcoin::timestamp &t) {
        auto v = Local.get_price (u, t);
        if (bool (v)) return *v;
        maybe<double> p = Remote->get_price (u, t);
        if (!bool (p)) return {};
        Local.set_price (u, t, *p);
        return p;
    }

    maybe<double> ask_for_price_data::get_price (monetary_unit u, const Bitcoin::timestamp &t) {
        std::stringstream ss;
        ss << "What was BSV/USD on " << t << std::endl;
        return data::read_decimal (ss.str ());
    }
}
