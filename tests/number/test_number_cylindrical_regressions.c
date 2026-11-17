/**
 * @file test_number_cylindrical_regressions.c
 * @brief Independent defining-series checks for cylindrical and Lommel acceleration.
 *
 * Supplies the SQL regression runner with full-width MPC references, differentiated Lommel terms and
 * rejected-attempt alias checks. The references do not call the production product-tree summation.
 */
#include <mpc.h>

#include "number-t.h"
#define MARS_NUMBER_INTERNAL_ACCESS
#include "number/number_internal.h"
#include "test_number.h"

/* Retain exact rational and multiprecision inputs when constructing an independent MPC reference. */
static bool regression_mpc_input(mpc_ptr out, number_t input)
{
    number_t *converted = number_coerce(&input, NUMBER_COMPLEX);
    bool ok = converted && !number_complex_get_mpc(out, number_impl_const(converted)->value.cx, mpc_get_prec(out));
    number_box_free(converted);
    return ok;
}

/* Reject a finite answer whose representation silently narrowed before checking its mathematics. */
static number_t regression_resolved(number_t value)
{
    return num_is_finite(value) && (num_is_exact(value) || num_get_prec_bits(value) >= num_get_default_prec_bits()) ?
           value : NUM_NAN;
}

/* Independent unnormalised two-factor series: no product tree, guarded products or shared convergence helper. */
static bool regression_two_lower_sum(mpc_ptr out, mpc_srcptr a, mpc_srcptr b, mpc_srcptr w, mpc_srcptr power)
{
    mpfr_prec_t precision = mpc_get_prec(out);
    mpc_t term, denominator, shift, contribution;
    mpc_init2(term, precision); mpc_init2(denominator, precision);
    mpc_init2(shift, precision); mpc_init2(contribution, precision);
    mpfr_t size, scale;
    mpfr_inits2(precision, size, scale, (mpfr_ptr)0);
    mpc_set_ui(term, 1u, MPC_RNDNN);
    if (power) mpc_set(out, power, MPC_RNDNN);
    else mpc_set_ui(out, 1u, MPC_RNDNN);
    bool converged = false;
    for (unsigned long k = 1; k < 10000ul; ++k) {
        mpc_add_ui(denominator, a, k - 1ul, MPC_RNDNN);
        mpc_add_ui(shift, b, k - 1ul, MPC_RNDNN);
        mpc_mul(denominator, denominator, shift, MPC_RNDNN);
        mpc_mul(term, term, w, MPC_RNDNN); mpc_div(term, term, denominator, MPC_RNDNN);
        if (power) {
            mpc_add_ui(shift, power, 2ul * k, MPC_RNDNN);
            mpc_mul(contribution, term, shift, MPC_RNDNN);
        } else mpc_set(contribution, term, MPC_RNDNN);
        mpc_add(out, out, contribution, MPC_RNDNN);
        mpc_abs(size, contribution, MPFR_RNDU); mpc_abs(scale, out, MPFR_RNDD);
        mpfr_mul_2si(scale, scale, -precision + 32, MPFR_RNDD);
        if (k >= 64ul && mpfr_number_p(size) && mpfr_lessequal_p(size, scale)) {
            converged = true;
            break;
        }
    }
    mpfr_clears(size, scale, (mpfr_ptr)0);
    mpc_clear(term); mpc_clear(denominator); mpc_clear(shift); mpc_clear(contribution);
    return converged;
}

/* Compare all four public cylinder normalisations with a full-width independently summed defining series. */
static number_t regression_cylinder_reference(const number_t *a, bool struve, bool alternating)
{
    mpfr_prec_t precision = (mpfr_prec_t)num_get_default_prec_bits() + 256;
    mpc_t lower_a, lower_b, w, sum;
    mpc_init2(lower_a, precision); mpc_init2(lower_b, precision);
    mpc_init2(w, precision); mpc_init2(sum, precision);
    bool ok = regression_mpc_input(lower_b, a[0]) && regression_mpc_input(w, a[1]);
    mpc_set_d(lower_a, struve ? 1.5 : 1.0, MPC_RNDNN);
    mpc_add(lower_b, lower_b, lower_a, MPC_RNDNN);
    mpc_sqr(w, w, MPC_RNDNN); mpc_div_2ui(w, w, 2u, MPC_RNDNN);
    if (alternating) mpc_neg(w, w, MPC_RNDNN);
    ok = ok && regression_two_lower_sum(sum, lower_a, lower_b, w, NULL);
    number_t shift = struve ? NUM_ONE_AND_HALF : NUM_ONE;
    number_t power = struve ? num_add(a[0], NUM_ONE) : a[0];
    number_t prefactor = num_div(num_pow(num_div(a[1], NUM_TWO), power),
                                 num_mul(num_gamma(shift), num_gamma(num_add(a[0], shift))));
    number_t reference = ok ? num_mul(prefactor, number_take(number_wrap_complex_mpc(sum, precision))) : NUM_NAN;
    number_t value = struve ? (alternating ? num_struve_H(a[0], a[1]) : num_struve_L(a[0], a[1])) :
                              (alternating ? num_bessel_J(a[0], a[1]) : num_bessel_I(a[0], a[1]));
    number_t error = num_sub(regression_resolved(value), reference);
    if (!num_is_zero(reference)) error = num_div(error, num_abs(reference));
    mpc_clear(lower_a); mpc_clear(lower_b); mpc_clear(w); mpc_clear(sum);
    return error;
}

/* Supply the SQL runner with an independent mathematical check. */
number_t test_number_bessel_i_reference(const number_t *a)
{
    return regression_cylinder_reference(a, false, false);
}

/* Supply the SQL runner with an independent mathematical check. */
number_t test_number_bessel_j_reference(const number_t *a)
{
    return regression_cylinder_reference(a, false, true);
}

/* Supply the SQL runner with an independent mathematical check. */
number_t test_number_struve_h_reference(const number_t *a)
{
    return regression_cylinder_reference(a, true, true);
}

/* Supply the SQL runner with an independent mathematical check. */
number_t test_number_struve_l_reference(const number_t *a)
{
    return regression_cylinder_reference(a, true, false);
}

/* Use the original denominator polynomial and differentiated terms, independently of the transformed series. */
/* Supply the SQL runner with an independent mathematical check. */
number_t test_number_lommel_reference(const number_t *a)
{
    mpfr_prec_t precision = (mpfr_prec_t)num_get_default_prec_bits() + 256;
    bool derivative = !num_is_zero(a[3]);
    mpc_t mu, nu, z, exponent, square, denominator, term, contribution, sum;
    mpc_init2(mu, precision); mpc_init2(nu, precision); mpc_init2(z, precision);
    mpc_init2(exponent, precision); mpc_init2(square, precision); mpc_init2(denominator, precision);
    mpc_init2(term, precision); mpc_init2(contribution, precision); mpc_init2(sum, precision);
    mpfr_t size, scale;
    mpfr_inits2(precision, size, scale, (mpfr_ptr)0);
    bool ok = regression_mpc_input(mu, a[0]) && regression_mpc_input(nu, a[1]) && regression_mpc_input(z, a[2]);
    if (mpfr_zero_p(mpc_imagref(z))) mpfr_set_zero(mpc_imagref(z), 1);
    mpc_sqr(square, z, MPC_RNDNN);
    mpc_add_ui(exponent, mu, 1u, MPC_RNDNN); mpc_pow(term, z, exponent, MPC_RNDNN);
    bool converged = false;
    for (unsigned long k = 0; ok && k < 10000ul; ++k) {
        mpc_add_ui(exponent, mu, 2ul * k + 1ul, MPC_RNDNN);
        mpc_sqr(denominator, exponent, MPC_RNDNN); mpc_sqr(contribution, nu, MPC_RNDNN);
        mpc_sub(denominator, denominator, contribution, MPC_RNDNN);
        if (k) { mpc_mul(term, term, square, MPC_RNDNN); mpc_neg(term, term, MPC_RNDNN); }
        mpc_div(term, term, denominator, MPC_RNDNN);
        if (derivative) {
            mpc_mul(contribution, term, exponent, MPC_RNDNN); mpc_div(contribution, contribution, z, MPC_RNDNN);
        } else mpc_set(contribution, term, MPC_RNDNN);
        if (!k) mpc_set(sum, contribution, MPC_RNDNN);
        else mpc_add(sum, sum, contribution, MPC_RNDNN);
        mpc_abs(size, contribution, MPFR_RNDU); mpc_abs(scale, sum, MPFR_RNDD);
        mpfr_mul_2si(scale, scale, -precision + 32, MPFR_RNDD);
        if (k >= 64ul && mpfr_number_p(size) && mpfr_lessequal_p(size, scale)) { converged = true; break; }
    }
    number_t value = derivative ? num_lommel_S_derivative_internal(a[0], a[1], a[2]) :
                                  num_lommel_S(a[0], a[1], a[2]);
    number_t reference = converged ? number_take(number_wrap_complex_mpc(sum, precision)) : NUM_NAN;
    number_t error = num_sub(regression_resolved(value), reference);
    if (!num_is_zero(reference)) error = num_div(error, num_abs(reference));
    mpfr_clears(size, scale, (mpfr_ptr)0);
    mpc_clear(mu); mpc_clear(nu); mpc_clear(z); mpc_clear(exponent); mpc_clear(square);
    mpc_clear(denominator); mpc_clear(term); mpc_clear(contribution); mpc_clear(sum);
    return error;
}

/* A failed bounded attempt must preserve every input even when used as its destination. */
/* Supply the SQL runner with an independent mathematical check. */
number_t test_number_two_lower_reject(const number_t *a)
{
    mpfr_prec_t target = (mpfr_prec_t)num_get_default_prec_bits(), work = target + 96;
    mpc_t values[4], before;
    for (unsigned j = 0; j < 4u; ++j) mpc_init2(values[j], work);
    mpc_init2(before, work);
    bool ok = true;
    for (unsigned alias = 0; ok && alias < 4u; ++alias) {
        for (unsigned j = 0; j < 4u; ++j) ok = regression_mpc_input(values[j], a[j]) && ok;
        mpc_set(before, values[alias], MPC_RNDNN);
        ok = ok && !number_mpc_two_lower_series(values[alias], values[0], values[1], values[2], values[3], target)
                && mpc_cmp(before, values[alias]) == 0;
    }
    for (unsigned j = 0; j < 4u; ++j) mpc_clear(values[j]);
    mpc_clear(before);
    return ok ? NUM_ZERO : NUM_ONE;
}

/* Check every accepted output alias against the independent weighted series at wider precision. */
number_t test_number_two_lower_alias(const number_t *a)
{
    mpfr_prec_t target = (mpfr_prec_t)num_get_default_prec_bits(), work = target + 96;
    mpc_t values[4], reference, difference;
    for (unsigned j = 0; j < 4u; ++j) mpc_init2(values[j], target + 256);
    mpc_init2(reference, target + 256); mpc_init2(difference, target + 256);
    bool ok = true;
    for (unsigned j = 0; j < 4u; ++j) ok = regression_mpc_input(values[j], a[j]) && ok;
    ok = ok && regression_two_lower_sum(reference, values[0], values[1], values[2], values[3]);
    mpfr_t error, tolerance;
    mpfr_inits2(target + 256, error, tolerance, (mpfr_ptr)0);
    mpc_abs(tolerance, reference, MPFR_RNDD);
    mpfr_mul_2si(tolerance, tolerance, -target + 8, MPFR_RNDD);
    for (unsigned j = 0; j < 4u; ++j) mpc_set_prec(values[j], work);
    for (unsigned alias = 0; ok && alias < 4u; ++alias) {
        for (unsigned j = 0; j < 4u; ++j) ok = regression_mpc_input(values[j], a[j]) && ok;
        ok = ok && number_mpc_two_lower_series(values[alias], values[0], values[1], values[2], values[3], target);
        mpc_sub(difference, values[alias], reference, MPC_RNDNN);
        mpc_abs(error, difference, MPFR_RNDU);
        ok = ok && mpfr_number_p(error) && mpfr_lessequal_p(error, tolerance);
    }
    mpfr_clears(error, tolerance, (mpfr_ptr)0);
    for (unsigned j = 0; j < 4u; ++j) mpc_clear(values[j]);
    mpc_clear(reference); mpc_clear(difference);
    return ok ? NUM_ZERO : NUM_ONE;
}
