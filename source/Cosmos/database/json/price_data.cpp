#include <Cosmos/database/json/price_data.hpp>

namespace Cosmos {

    JSON_price_data::JSON_price_data (const JSON &j) {
        for (const JSON &x : j) Price = Price.insert (Bitcoin::timestamp {uint32 (x[0])}, double (x[1]));
    }

    JSON_price_data::operator JSON () const {
        JSON::array_t data {};
        for (const auto &[key, value] : Price)
            data.push_back (JSON::array_t {uint32 (key), value});

        return data;
    }

    uint32 time_distance (Bitcoin::timestamp a, Bitcoin::timestamp b) {
        if (a > b) return a - b;
        else return b - a;
    }

    maybe<const data::entry<const Bitcoin::timestamp, double> &>
    get_price_inner (data::map<Bitcoin::timestamp, double> price, const Bitcoin::timestamp &t) {
        if (data::empty (price)) return {};

        const data::entry<const Bitcoin::timestamp, double> &root = price.root ();

        if (root.Key < t) {
            if (uint32 (root.Key) + price_data::OneDay > uint32 (t)) {
                auto z = get_price_inner (data::left (price), t);
                if (!bool (z) || time_distance (root.Key, t) < time_distance (z->Key, t))
                    return {root};
                    else return {z};
            } else return get_price_inner (data::left (price), t);
        } else if (root.Key > t) {
            if (uint32 (root.Key) - price_data::OneDay < uint32 (t)) {
                auto z = get_price_inner (data::right (price), t);
                if (!bool (z), time_distance (root.Key, t) < time_distance (z->Key, t))
                    return {root};
                    else return {z};
            } else return get_price_inner (data::right (price), t);
        } else return price.root ();

    }

    maybe<double> JSON_price_data::get_price (monetary_unit, const Bitcoin::timestamp &t) {
        auto e = get_price_inner (Price, t);
        if (!bool (e)) return {};
        return {e->Value};
    }

    void JSON_price_data::set_price (monetary_unit, const Bitcoin::timestamp &t, double p) {
        if (!Price.contains (t)) Price = Price.insert (t, p);
    }
}

