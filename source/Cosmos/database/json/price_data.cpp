#include <Cosmos/database/json/price_data.hpp>

namespace Cosmos {

    JSON_price_data::JSON_price_data (const JSON &j) {
        for (const JSON &x : j) Price[Bitcoin::timestamp {uint32 (x[0])}] = double (x[1]);
    }

    JSON_price_data::operator JSON () const {
        JSON::array_t data {};
        for (const auto &[key, value] : Price)
            data.push_back (JSON::array_t {uint32 (key), value});

        return data;
    }

    maybe<double> JSON_price_data::get (const Bitcoin::timestamp &t) {
        if (auto x = Price.find (t); x != Price.end ()) return x->second;
        return {};
    }

    void JSON_price_data::set (const Bitcoin::timestamp &t, double p) {
        if (auto x = Price.find (t); x != Price.end () &&
            !(x->second - p == 0 || (std::max (x->second, p) / std::abs (x->second - p)) < .005))
                throw exception {} << " We already have a different value for that price already: " << x->second << " vs " << p;
        Price[t] = p;
    }
}

