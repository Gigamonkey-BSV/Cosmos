
#include <Cosmos/math/log_triangular_distribution.hpp>
#include <data/io/exception.hpp>

namespace math {


    // here e_x means exponential of x. Thus the variables are not all independent.
    double log_triangular_distribution_mean (double a, double b, double m, double e_a, double e_b, double e_m);

    double inline log_triangular_distribution_mean (double a, double b, double m, double e_a, double e_b) {
        return log_triangular_distribution_mean (a, b, m, e_a, e_b, exp (m));
    }

    double inline log_triangular_distribution_mean (double a, double b, double m) {
        return log_triangular_distribution_mean (a, b, m, exp (a), exp (b));
    }

    double inline log_triangular_distribution_mean (double a, double b, double m, double e_a, double e_b, double e_m) {
        return (((e_m * (a - m + 1) - e_a) / (a - m)) + ((e_m * (m - b - 1) + e_b) / (b - m))) * 2 / (b - a);
    }

    // when m -> a or m -> b we get a limit of 0 / 0 so we have to have a special case for those cases, which
    // represent the maximum and minimum allowed values of the mean.
    double min_log_triangular_distribution_mean (double a, double b, double e_a, double e_b);
    double max_log_triangular_distribution_mean (double a, double b, double e_a, double e_b);

    double inline min_log_triangular_distribution_mean (double a, double b) {
        return min_log_triangular_distribution_mean (a, b, exp (a), exp (b));
    }

    double inline max_log_triangular_distribution_mean (double a, double b) {
        return max_log_triangular_distribution_mean (a, b, exp (a), exp (b));
    }

    double inline min_log_triangular_distribution_mean (double a, double b, double e_a, double e_b) {
        return (e_a * (a - b - 1) + e_b) * 2 / ((b - a) * (b - a));
    }

    double inline max_log_triangular_distribution_mean (double a, double b, double e_a, double e_b) {
        return (e_b * (a - b + 1) - e_a) * 2 / ((a - b) * (b - a));
    }

    double find_triangle_mode (double a, double b, double mean, double e_a, double e_b) {
        double min = a;
        double max = b;
        double guess, m;

        while (true) {
            // it's possible to converge a lot faster than this but let's see if this works well enough.
            double m = (max - min) / 2 + min;
            double guess = log_triangular_distribution_mean (a, b, m, e_a, e_b);
            if (std::max (mean - guess, guess - mean) > 1) return m;
            (guess > mean ? max : min) = m;
        }
    }

    log_triangular_distribution::log_triangular_distribution (double min, double max, double mean) {

        if (max < min) throw data::exception {} <<
            "log triangular distribution: max (" << max << ") must not be less than min (" << min << ")";

        if (mean > max) throw data::exception {} <<
            "log triangular distribution: mean (" << mean << ") must not be greater than max (" << max << ")";

        if (mean < min) throw data::exception {} <<
            "log triangular distribution: mean (" << mean << ") must not be less than min (" << min << ")";

        double a = ln (double (min));
        double b = ln (double (max));

        double min_mean = min_log_triangular_distribution_mean (a, b, min, max);
        double max_mean = max_log_triangular_distribution_mean (a, b, min, max);

        if (mean < min_mean) throw data::exception {} <<
            "Minimum possible mean value for max " << max << " and min " << min << " is " << min_mean;

        if (mean > max_mean) throw data::exception {} <<
            "Maximum possible mean value for max " << max << " and min " << min << " is " << max_mean;

        Triangular = math::triangular_distribution<double> {a, find_triangle_mode (a, b, mean, min, max), b};

    }

}
