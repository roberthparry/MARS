#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"
#include <math.h>

static void replace_value(number_t *value, number_t next)
{
    num_destroy(value);
    *value = next;
}

/* Explicitly promote both Cartesian components: merely tagging an exact complex value can leave
 * later transcendental dispatch on a fixed-precision component. */
static number_t working_value(number_t value, size_t precision)
{
    NUM_SCOPE(scope);
    if (num_is_real(value) && num_ge(value, NUM_ZERO))
        return num_scope_detach(num_as_inexact_real_prec(num_real_part(value), precision));
    number_t real = num_as_inexact_real_prec(num_real_part(value), precision);
    number_t imag = num_as_inexact_real_prec(num_imag_part(value), precision);
    complex_t *z = number_complex_create(real, imag);
    if (!z)
        return num_clone(NUM_NAN);
    number_t result = number_take(number_wrap_complex(z));
    num_set_prec_bits(&result, precision);
    return num_scope_detach(result);
}

static bool small_term(number_t term, number_t sum, number_t tolerance)
{
    NUM_SCOPE(scope);
    return num_le(num_abs(term), num_mul(tolerance, num_add(NUM_ONE, num_abs(sum))));
}

/* Convergent I-series with the leading power and gamma factor removed. */
static number_t reduced_i_series(number_t order, number_t square, number_t tolerance)
{
    NUM_SCOPE(scope);
    number_t term = num_clone(NUM_ONE), sum = num_clone(NUM_ONE);
    for (long k = 1; k <= 20000; ++k) {
        number_t index = num_create_from_long(k);
        replace_value(&term, num_div(num_mul(term, square), num_mul(index, num_add(index, order))));
        replace_value(&sum, num_add(sum, term));
        if (!num_is_finite(sum))
            return num_clone(NUM_NAN);
        if (small_term(term, sum, tolerance))
            return num_scope_detach(sum);
    }
    return num_clone(NUM_NAN);
}

/* Integer orders avoid the removable singularity in the connection formula for I_nu and I_-nu. */
static number_t integer_k(long order, number_t z, number_t square, number_t logarithm, number_t tolerance)
{
    NUM_SCOPE(scope);
    number_t term = num_clone(NUM_ONE), i0 = num_clone(NUM_ONE), harmonic = num_clone(NUM_ZERO);
    number_t weighted = num_clone(NUM_ZERO), derivative = num_clone(NUM_ZERO);
    bool converged = false;
    for (long k = 1; k <= 20000; ++k) {
        number_t index = num_create_from_long(k);
        replace_value(&harmonic, num_add(harmonic, num_div(NUM_ONE, index)));
        replace_value(&term, num_div(num_mul(term, square), num_mul(index, index)));
        replace_value(&i0, num_add(i0, term));
        number_t contribution = num_mul(term, harmonic);
        replace_value(&weighted, num_add(weighted, contribution));
        number_t differentiated = num_mul(num_mul(index, term), num_sub(logarithm, harmonic));
        replace_value(&derivative, num_add(derivative, differentiated));
        if (!num_is_finite(i0))
            return num_clone(NUM_NAN);
        if (small_term(term, i0, tolerance) && small_term(contribution, weighted, tolerance) &&
            small_term(differentiated, derivative, tolerance)) {
            converged = true;
            break;
        }
    }
    if (!converged)
        return num_clone(NUM_NAN);
    number_t previous = num_sub(weighted, num_mul(logarithm, i0));
    if (order == 0)
        return num_scope_detach(previous);
    number_t current = num_div(num_add(i0, num_mul(NUM_TWO, derivative)), z);
    for (long n = 1; n < order; ++n) {
        number_t factor = num_div(num_create_from_long(2*n), z);
        number_t next = num_add(previous, num_mul(factor, current));
        replace_value(&previous, current);
        current = next;
    }
    return num_scope_detach(current);
}

/* Evaluate the principal modified Bessel K with guard precision for cancellation in its convergent series. */
number_t num_bessel_k(const number_t order, const number_t argument)
{
    NUM_SCOPE(scope);
    if (!num_is_finite(order) || !num_is_finite(argument) || num_is_zero(argument))
        return num_clone(NUM_NAN);
    double size = num_to_double(num_abs(argument)), degree = num_to_double(num_abs(order));
    if (!isfinite(size) || !isfinite(degree) || size > 1000 || degree > 1000)
        return num_clone(NUM_NAN);
    size_t precision = number_cylindrical_precision(order, argument);
    size_t work = precision + 96u + (size_t)ceil(4*(size+degree));
    bool integral = num_is_real(order) && num_is_integer(order);
    /* Near integral orders the two gamma terms cancel; retain the distance's significant bits. */
    if (!integral) {
        number_t nearest = num_floor(num_add(num_real_part(order), NUM_HALF));
        number_t distance = num_abs(num_sub(order, nearest));
        double lost = -num_to_double(num_log(distance))/log(2.0);
        if (!isfinite(lost) || lost > 65536)
            return num_clone(NUM_NAN);
        if (lost > 0)
            work += (size_t)ceil(lost);
    }
    number_t z = working_value(argument, work), nu = working_value(order, work);
    number_t half_z = working_value(num_mul(NUM_HALF, z), work);
    number_t square = working_value(num_mul(half_z, half_z), work);
    number_t tolerance = num_pow(NUM_TWO, num_create_from_long(-(long)work+16));
    number_t result;
    if (integral) {
        number_t logarithm = num_add(num_log(half_z), num_const_prec(NUM_EULER_MASCHERONI, work));
        result = integer_k((long)degree, z, square, logarithm, tolerance);
    } else {
        number_t minus = num_neg(nu);
        number_t left = num_mul(num_gamma(nu), num_pow(half_z, minus));
        number_t right = num_mul(num_gamma(minus), num_pow(half_z, nu));
        left = num_mul(left, reduced_i_series(minus, square, tolerance));
        right = num_mul(right, reduced_i_series(nu, square, tolerance));
        result = num_mul(NUM_HALF, num_add(left, right));
    }
    if (num_is_real(order) && num_is_real(argument) && num_gt(argument, NUM_ZERO))
        result = num_real_part(result);
    num_set_prec_bits(&result, precision);
    return num_scope_detach(result);
}
