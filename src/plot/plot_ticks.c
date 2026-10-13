/**
 * @file plot_ticks.c
 * @brief Precision-preserving decimal tick selection for linear SVG axes.
 *
 * Steps use 1, 2, 2.5 or 5 times an integral power of ten, targeting five
 * intervals. All axis arithmetic stays in number_t, including tiny ranges.
 */
#include <limits.h>
#include <math.h>

#define MARS_PLOT_INTERNAL_ACCESS
#include "plot_internal.h"

size_t plot_ticks(number_t low, number_t high, number_t values[12])
{
    NUM_SCOPE(scope);
    number_t span = num_sub(high, low), five = num_create_from_long(5), raw = num_div(span, five);
    number_t logarithm = num_log10(raw), exponent = num_floor(logarithm);
    double power = num_to_double(exponent);
    if (!isfinite(power) || power <= INT_MIN || power >= INT_MAX)
        return 0;
    number_t scale = num_pow10((int)power), ratio = num_div(raw, scale);
    double mantissa = num_to_double(ratio);
    double factor = mantissa < 1.5 ? 1 : mantissa < 2.25 ? 2 : mantissa < 3.75 ? 2.5 : mantissa < 7.5 ? 5 : 10;
    number_t step = num_mul(scale, num_create_from_double(factor));
    number_t start = num_ceil(num_div(low, step));
    size_t count = 0;
    for (unsigned i = 0; i < 12; ++i) {
        number_t value = num_mul(num_add(start, num_create_from_long(i)), step);
        if (num_gt(value, high))
            break;
        if (num_ge(value, low))
            values[count++] = num_scope_detach(value);
    }
    return count;
}
