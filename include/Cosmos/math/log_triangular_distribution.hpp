#ifndef COSMOS_MATH_LOG_TRIANGULAR_DISTRIBUTION
#define COSMOS_MATH_LOG_TRIANGULAR_DISTRIBUTION

#include <data/math/probability/triangular_distribution.hpp>

namespace math {
    using namespace data::math;

    static double inline ln (double x) {
        return std::log (x);
    }

    static double inline exp (double x) {
        return std::exp (x);
    }

    struct log_triangular_distribution {
        triangular_distribution<double> Triangular;

        log_triangular_distribution (double min, double max, double mean);

        template <std::uniform_random_bit_generator engine>
        double operator () (engine &e) const;

    };


    template <std::uniform_random_bit_generator engine>
    double inline log_triangular_distribution::operator () (engine &e) const {
        return exp (Triangular (e));
    }

}

#endif
