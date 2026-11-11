/**
 * @file number_bessel_k.c
 * @brief Modified Bessel K evaluation.
 *
 * Evaluates the modified Bessel K family, including the supported order-dependent branches. It supplies a
 * decaying-solution counterpart to Bessel I while respecting this numeric layer's domain. Integer orders use
 * local MPC scratch values for their harmonic series and recurrence, avoiding per-term numeric wrapper allocation.
 *
 * This belongs to number_t's shared scalar layer. Preserve exactness, precision policy and the distinction between
 * borrowed aliases and independently owned numeric values.
 */

#include <math.h>

#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"

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
    if (num_is_real(value))
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
static bool integer_k_small(mpc_srcptr term, mpc_srcptr sum, mpfr_srcptr tolerance,
                            mpfr_ptr magnitude, mpfr_ptr threshold)
{
    number_mpc_abs_upper(magnitude, term);
    number_mpc_abs_lower(threshold, sum);
    mpfr_add_ui(threshold, threshold, 1u, MPFR_RNDD);
    mpfr_mul(threshold, threshold, tolerance, MPFR_RNDD);
    return mpfr_lessequal_p(magnitude, threshold);
}

static number_t integer_k(long order, number_t argument, size_t precision)
{
    mpc_t z, square, logarithm, term, i0, weighted, derivative, contribution, differentiated;
    mpc_t previous, current, next, scratch;
    mpc_init2(z, precision); mpc_init2(square, precision); mpc_init2(logarithm, precision);
    mpc_init2(term, precision); mpc_init2(i0, precision); mpc_init2(weighted, precision);
    mpc_init2(derivative, precision); mpc_init2(contribution, precision); mpc_init2(differentiated, precision);
    mpc_init2(previous, precision); mpc_init2(current, precision); mpc_init2(next, precision);
    mpc_init2(scratch, precision);
    mpfr_t harmonic, coefficient, tolerance, magnitude, threshold;
    mpfr_inits2(precision, harmonic, coefficient, tolerance, magnitude, threshold, (mpfr_ptr)0);
    number_t result = number_invalid();
    number_t *promoted = number_coerce(&argument, NUMBER_COMPLEX);
    bool ready = promoted && !number_complex_get_mpc(z, number_impl_const(promoted)->value.cx, precision);
    number_box_free(promoted);
    if (!ready) goto done;
    mpc_div_2ui(square, z, 1u, MPC_RNDNN);
    mpc_log(logarithm, square, MPC_RNDNN);
    mpfr_const_euler(coefficient, MPFR_RNDN);
    mpc_add_fr(logarithm, logarithm, coefficient, MPC_RNDNN);
    mpc_sqr(square, square, MPC_RNDNN);
    mpc_set_ui(term, 1u, MPC_RNDNN); mpc_set_ui(i0, 1u, MPC_RNDNN);
    mpc_set_ui(weighted, 0u, MPC_RNDNN); mpc_set_ui(derivative, 0u, MPC_RNDNN);
    mpfr_set_zero(harmonic, 1);
    mpfr_set_ui_2exp(tolerance, 1u, -(long)precision + 16, MPFR_RNDN);
    bool converged = false;
    for (long k = 1; k <= 20000; ++k) {
        mpfr_set_ui(coefficient, (unsigned long)k, MPFR_RNDN);
        mpfr_ui_div(coefficient, 1u, coefficient, MPFR_RNDN);
        mpfr_add(harmonic, harmonic, coefficient, MPFR_RNDN);
        mpc_mul(term, term, square, MPC_RNDNN);
        mpc_div_ui(term, term, (unsigned long)k, MPC_RNDNN);
        mpc_div_ui(term, term, (unsigned long)k, MPC_RNDNN);
        mpc_add(i0, i0, term, MPC_RNDNN);
        mpc_mul_fr(contribution, term, harmonic, MPC_RNDNN);
        mpc_add(weighted, weighted, contribution, MPC_RNDNN);
        mpc_sub_fr(scratch, logarithm, harmonic, MPC_RNDNN);
        mpc_mul(differentiated, term, scratch, MPC_RNDNN);
        mpc_mul_ui(differentiated, differentiated, (unsigned long)k, MPC_RNDNN);
        mpc_add(derivative, derivative, differentiated, MPC_RNDNN);
        if (!mpfr_number_p(mpc_realref(i0)) || !mpfr_number_p(mpc_imagref(i0))) goto done;
        if (integer_k_small(term, i0, tolerance, magnitude, threshold) &&
            integer_k_small(contribution, weighted, tolerance, magnitude, threshold) &&
            integer_k_small(differentiated, derivative, tolerance, magnitude, threshold)) {
            converged = true;
            break;
        }
    }
    if (!converged) goto done;
    mpc_mul(previous, logarithm, i0, MPC_RNDNN);
    mpc_sub(previous, weighted, previous, MPC_RNDNN);
    if (!order) {
        number_t *wrapped = number_wrap_complex_mpc(previous, precision);
        if (wrapped) result = number_take(wrapped);
        goto done;
    }
    mpc_mul_2ui(current, derivative, 1u, MPC_RNDNN);
    mpc_add(current, current, i0, MPC_RNDNN);
    mpc_div(current, current, z, MPC_RNDNN);
    for (long n = 1; n < order; ++n) {
        mpc_mul_ui(next, current, (unsigned long)(2 * n), MPC_RNDNN);
        mpc_div(next, next, z, MPC_RNDNN);
        mpc_add(next, next, previous, MPC_RNDNN);
        mpc_swap(previous, current); mpc_swap(current, next);
    }
    number_t *wrapped = number_wrap_complex_mpc(current, precision);
    if (wrapped) result = number_take(wrapped);
done:
    mpfr_clears(harmonic, coefficient, tolerance, magnitude, threshold, (mpfr_ptr)0);
    mpc_clear(z); mpc_clear(square); mpc_clear(logarithm); mpc_clear(term); mpc_clear(i0);
    mpc_clear(weighted); mpc_clear(derivative); mpc_clear(contribution); mpc_clear(differentiated);
    mpc_clear(previous); mpc_clear(current); mpc_clear(next); mpc_clear(scratch);
    return result;
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
    number_t result;
    if (integral) {
        result = integer_k((long)degree, argument, work);
    } else {
        number_t z = working_value(argument, work), nu = working_value(order, work);
        number_t half_z = working_value(num_mul(NUM_HALF, z), work);
        number_t square = working_value(num_mul(half_z, half_z), work);
        number_t tolerance = num_pow(NUM_TWO, num_create_from_long(-(long)work+16));
        number_t minus = num_neg(nu);
        number_t left = num_mul(num_gamma(nu), num_pow(half_z, minus));
        number_t right = num_mul(num_gamma(minus), num_pow(half_z, nu));
        number_t left_series = reduced_i_series(minus, square, tolerance);
        number_t right_series = reduced_i_series(nu, square, tolerance);

        left = num_mul(left, left_series);
        right = num_mul(right, right_series);
        num_destroy(&right_series);
        num_destroy(&left_series);
        result = num_mul(NUM_HALF, num_add(left, right));
        num_destroy(&square); num_destroy(&half_z); num_destroy(&nu); num_destroy(&z);
    }
    if (num_is_real(order) && num_is_real(argument) && num_gt(argument, NUM_ZERO))
        replace_value(&result, num_real_part(result));
    num_set_prec_bits(&result, precision);
    return num_scope_detach(result);
}
