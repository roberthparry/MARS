/**
 * @file test_number_special_regressions.c
 * @brief Database-driven accuracy checks for benchmark special-function regressions.
 *
 * Checks defining identities and independent error-function and zeta references, rather than accepting any finite
 * answer. SQL supplies arguments and decimal precision; the C runner retains the executable mathematics. The
 * Maclaurin reference differs from the production Kummer series and uses extra working precision.
 * Hypergeometric references use full-width term summation without product trees, additive ratio polynomials or
 * shortened state, covering the accelerated paths and their ordinary-series fallbacks independently.
 */
#include <mpc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "number-t.h"
#include "string-t.h"
#include "test_harness.h"
#include "test_database.h"
#include "test_number.h"
#define MARS_NUMBER_INTERNAL_ACCESS
#include "number/number_internal.h"

/* Retain exact rational and multiprecision inputs when constructing an independent MPC reference. */
static bool regression_mpc_input(mpc_ptr out, number_t input)
{
    number_t *converted = number_coerce(&input, NUMBER_COMPLEX);
    bool ok = converted && !number_complex_get_mpc(out, number_impl_const(converted)->value.cx, mpc_get_prec(out));
    number_box_free(converted);
    return ok;
}

static number_t regression_erf_reference(number_t input)
{
    mpfr_prec_t precision = (mpfr_prec_t)num_get_default_prec_bits() + 256;
    mpc_t z, square, term, sum, addend;
    mpfr_t pi, size;
    mpc_init2(z, precision); mpc_init2(square, precision); mpc_init2(term, precision);
    mpc_init2(sum, precision); mpc_init2(addend, precision);
    mpfr_inits2(precision, pi, size, (mpfr_ptr)0);
    if (!regression_mpc_input(z, input)) mpc_set_nan(z);
    mpc_sqr(square, z, MPC_RNDNN);
    mpc_neg(square, square, MPC_RNDNN);
    mpc_set(term, z, MPC_RNDNN);
    mpc_set(sum, z, MPC_RNDNN);
    for (unsigned long n = 1; n < 10000; ++n) {
        mpc_mul(term, term, square, MPC_RNDNN);
        mpc_div_ui(term, term, n, MPC_RNDNN);
        mpc_div_ui(addend, term, 2 * n + 1, MPC_RNDNN);
        mpc_add(sum, sum, addend, MPC_RNDNN);
        mpc_abs(size, addend, MPFR_RNDN);
        if (mpfr_zero_p(size) || mpfr_get_exp(size) < -precision + 32) break;
    }
    mpfr_const_pi(pi, MPFR_RNDN);
    mpfr_sqrt(pi, pi, MPFR_RNDN);
    mpfr_ui_div(pi, 2u, pi, MPFR_RNDN);
    mpc_mul_fr(sum, sum, pi, MPC_RNDNN);
    char *text = NULL;
    int digits = (int)(precision * 0.302) + 4;
    mpfr_asprintf(&text, "%.*Rg%+.*Rgi", digits, mpc_realref(sum), digits, mpc_imagref(sum));
    number_t result = text ? num_create_from_string(text) : num_clone(NUM_NAN);
    if (text) mpfr_free_str(text);
    mpfr_clears(pi, size, (mpfr_ptr)0);
    mpc_clear(z); mpc_clear(square); mpc_clear(term); mpc_clear(sum); mpc_clear(addend);
    return result;
}

static qcomplex_t regression_qcomplex(number_t input)
{
    return qc_make(num_to_qfloat(num_real_part(input)), num_to_qfloat(num_imag_part(input)));
}

static number_t regression_erf(const number_t *a)
{
    return num_sub(num_erf(a[0]), regression_erf_reference(a[0]));
}

static number_t regression_erfc(const number_t *a)
{
    return num_sub(num_erfc(a[0]), num_sub(NUM_ONE, regression_erf_reference(a[0])));
}

static number_t regression_qc_erf(const number_t *a)
{
    return num_sub(num_create_from_qcomplex(qc_erf(regression_qcomplex(a[0]))), regression_erf_reference(a[0]));
}

static number_t regression_erfinv(const number_t *a) { return num_erf(num_erfinv(a[0])); }
static number_t regression_erfcinv(const number_t *a) { return num_erfc(num_erfcinv(a[0])); }

static number_t regression_erfinv_reference(const number_t *a)
{
    return num_sub(regression_erf_reference(num_erfinv(a[0])), a[0]);
}

static number_t regression_erfcinv_reference(const number_t *a)
{
    return num_sub(num_sub(NUM_ONE, regression_erf_reference(num_erfcinv(a[0]))), a[0]);
}

/* Reject a finite answer whose representation silently narrowed before checking its mathematics. */
static number_t regression_resolved(number_t value)
{
    return num_is_finite(value) && (num_is_exact(value) || num_get_prec_bits(value) >= num_get_default_prec_bits()) ?
           value : NUM_NAN;
}

static number_t regression_acosec(const number_t *a)
{
    number_t value = regression_resolved(num_acosec(a[0])), reference = num_asin(num_div(NUM_ONE, a[0]));
    number_t real = num_real_part(reference), imaginary = num_imag_part(reference);
    number_t real_error = num_sub(num_real_part(value), real);
    number_t imaginary_error = num_sub(num_imag_part(value), imaginary);
    if (!num_is_zero(real)) real_error = num_div(real_error, real);
    if (!num_is_zero(imaginary)) imaginary_error = num_div(imaginary_error, imaginary);
    return num_add(num_abs(real_error), num_abs(imaginary_error));
}

static number_t regression_asec(const number_t *a)
{
    number_t value = regression_resolved(num_asec(a[0])), reference = num_acos(num_div(NUM_ONE, a[0]));
    number_t real = num_real_part(reference), imaginary = num_imag_part(reference);
    number_t real_error = num_sub(num_real_part(value), real);
    number_t imaginary_error = num_sub(num_imag_part(value), imaginary);
    if (!num_is_zero(real)) real_error = num_div(real_error, real);
    if (!num_is_zero(imaginary)) imaginary_error = num_div(imaginary_error, imaginary);
    return num_add(num_abs(real_error), num_abs(imaginary_error));
}

static number_t regression_double_erfinv(const number_t *a)
{
    number_t value = num_create_from_double(num_to_double(a[0]));
    return num_sub(num_erf(num_erfinv(value)), value);
}

static number_t regression_double_erfcinv(const number_t *a)
{
    number_t value = num_create_from_double(num_to_double(a[0]));
    return num_sub(num_erfc(num_erfcinv(value)), value);
}

static number_t regression_gamma_recurrence(const number_t *a)
{
    number_t value = regression_resolved(num_gamma(a[0]));
    return num_sub(num_gamma(num_add(a[0], NUM_ONE)), num_mul(a[0], value));
}

static number_t regression_gamma_reflection(const number_t *a)
{
    number_t product = num_mul(regression_resolved(num_gamma(a[0])), num_gamma(num_sub(NUM_ONE, a[0])));
    number_t reference = num_div(num_const(NUM_PI), num_sin(num_mul(num_const(NUM_PI), a[0])));
    return num_div(num_sub(product, reference), reference);
}

static number_t regression_gamma_inverse_complex(const number_t *a)
{
    number_t value = regression_resolved(num_gammainv(a[0]));
    return num_sub(num_gamma(value), a[0]);
}

static number_t regression_lgamma_turns(const number_t *a)
{
    number_t value = regression_resolved(num_lgamma(a[0]));
    number_t difference = num_sub(num_lgamma(num_add(a[0], num_create_from_long(128))), value);
    for (unsigned j = 0; j < 128; ++j)
        difference = num_sub(difference, num_log(num_add(a[0], num_create_from_long(j))));
    return difference;
}

static number_t regression_gamma_upper_one(const number_t *a)
{
    number_t one = num_new_with_prec_bits(num_get_default_prec_bits());
    one = num_add(one, NUM_ONE);
    return num_sub(regression_resolved(num_gammainc_upper(one, a[0])), num_exp(num_neg(a[0])));
}

static number_t regression_gamma_lower_one(const number_t *a)
{
    number_t one = num_add(num_new_with_prec_bits(num_get_default_prec_bits()), NUM_ONE);
    return num_sub(regression_resolved(num_gammainc_lower(one, a[0])),
                   num_sub(NUM_ONE, num_exp(num_neg(a[0]))));
}

/* The alternating expansion omits the production series' exp(-z) factor and Pochhammer recurrence. */
static number_t regression_gamma_lower_reference(const number_t *a)
{
    mpfr_prec_t precision = (mpfr_prec_t)num_get_default_prec_bits() + 128;
    mpc_t order, z, power, sum, term, denominator, prefactor;
    mpc_init2(order, precision); mpc_init2(z, precision); mpc_init2(power, precision);
    mpc_init2(sum, precision); mpc_init2(term, precision); mpc_init2(denominator, precision);
    mpc_init2(prefactor, precision);
    mpfr_t magnitude;
    mpfr_init2(magnitude, precision);
    mpc_set_d_d(order, num_to_double(num_real_part(a[0])), num_to_double(num_imag_part(a[0])), MPC_RNDNN);
    mpc_set_d_d(z, num_to_double(num_real_part(a[1])), num_to_double(num_imag_part(a[1])), MPC_RNDNN);
    mpc_set_ui(power, 1u, MPC_RNDNN); mpc_set_ui(sum, 0u, MPC_RNDNN);
    bool converged = false;
    for (unsigned long n = 0; n < 200000; ++n) {
        mpc_add_ui(denominator, order, n, MPC_RNDNN);
        mpc_div(term, power, denominator, MPC_RNDNN);
        mpc_add(sum, sum, term, MPC_RNDNN);
        number_mpc_abs_upper(magnitude, term);
        if (mpfr_zero_p(magnitude) || mpfr_get_exp(magnitude) < -precision + 16) {
            converged = true;
            break;
        }
        mpc_mul(power, power, z, MPC_RNDNN);
        mpc_div_ui(power, power, n + 1u, MPC_RNDNN);
        mpc_neg(power, power, MPC_RNDNN);
    }
    mpc_pow(prefactor, z, order, MPC_RNDNN); mpc_mul(sum, sum, prefactor, MPC_RNDNN);
    number_t reference = converged ? number_take(number_wrap_complex_mpc(sum, precision)) : num_clone(NUM_NAN);
    number_t result = num_div(num_sub(num_gammainc_lower(a[0], a[1]), reference), num_abs(reference));
    mpfr_clear(magnitude);
    mpc_clear(order); mpc_clear(z); mpc_clear(power); mpc_clear(sum); mpc_clear(term);
    mpc_clear(denominator); mpc_clear(prefactor);
    return result;
}

static number_t regression_gamma_regularised(const number_t *a)
{
    number_t p = regression_resolved(num_gammainc_P(a[0], a[1]));
    number_t q = regression_resolved(num_gammainc_Q(a[0], a[1]));
    return num_sub(num_add(p, q), NUM_ONE);
}

static number_t regression_gamma_upper_recurrence(const number_t *a)
{
    number_t value = regression_resolved(num_gammainc_upper(a[0], a[1]));
    number_t next = num_gammainc_upper(num_add(a[0], NUM_ONE), a[1]);
    number_t tail = num_mul(num_pow(a[1], a[0]), num_exp(num_neg(a[1])));
    return num_sub(next, num_add(num_mul(a[0], value), tail));
}

static number_t regression_hypergeometric_exp(const number_t *a)
{
    number_t one = num_add(num_new_with_prec_bits(num_get_default_prec_bits()), NUM_ONE);
    number_t two = num_add(one, NUM_ONE);
    number_t value = regression_resolved(num_hypergeometric_pFq(&one, 1, &two, 1, a[0]));
    return num_sub(value, num_div(num_sub(num_exp(a[0]), NUM_ONE), a[0]));
}

static number_t regression_hypergeometric_exact(const number_t *a)
{
    number_t argument = num_create_from_long((long)num_to_double(a[0]));
    number_t value = regression_resolved(num_hypergeometric_pFq(NULL, 0, NULL, 0, argument));
    return num_sub(value, num_exp(a[0]));
}

static number_t regression_hypergeometric_binomial(const number_t *a)
{
    number_t value = regression_resolved(num_hypergeometric_pFq(a, 1, NULL, 0, a[1]));
    return num_sub(value, num_pow(num_sub(NUM_ONE, a[1]), num_neg(a[0])));
}

static number_t regression_hypergeometric_terminating(const number_t *a)
{
    const number_t upper[] = {num_create_from_long(-3), a[0]};
    number_t value = regression_resolved(num_hypergeometric_pFq(upper, 2, a, 1, a[1]));
    return num_sub(value, num_pow_int(num_sub(NUM_ONE, a[1]), 3));
}

/* Independent full-precision term summation uses ordinary MPC products and divisions, never either fast kernel. */
static number_t regression_pfq_reference_value(const number_t *a, size_t p, size_t q)
{
    mpfr_prec_t precision = (mpfr_prec_t)num_get_default_prec_bits() + 256;
    mpc_t parameters[5], z, term, sum, factor, denominator, shift;
    mpc_init2(z, precision); mpc_init2(term, precision); mpc_init2(sum, precision);
    mpc_init2(factor, precision); mpc_init2(denominator, precision); mpc_init2(shift, precision);
    bool ok = regression_mpc_input(z, a[p + q]), converged = false;
    for (size_t j = 0; j < p + q; ++j) {
        mpc_init2(parameters[j], precision);
        ok = regression_mpc_input(parameters[j], a[j]) && ok;
    }
    mpfr_t magnitude;
    mpfr_init2(magnitude, precision);
    mpc_set_ui(term, 1u, MPC_RNDNN); mpc_set_ui(sum, 1u, MPC_RNDNN);
    for (unsigned long n = 0; ok && n < 50000ul; ++n) {
        mpc_set(factor, z, MPC_RNDNN); mpc_set_ui(denominator, n + 1ul, MPC_RNDNN);
        for (size_t j = 0; j < p + q; ++j) {
            mpc_add_ui(shift, parameters[j], n, MPC_RNDNN);
            if (j < p) mpc_mul(factor, factor, shift, MPC_RNDNN);
            else mpc_mul(denominator, denominator, shift, MPC_RNDNN);
        }
        mpc_mul(term, term, factor, MPC_RNDNN); mpc_div(term, term, denominator, MPC_RNDNN);
        mpc_add(sum, sum, term, MPC_RNDNN);
        mpc_abs(magnitude, term, MPFR_RNDU);
        /* Authored balanced cases have |z|<=3/4 and bounded parameters, so the late geometric ratio is <0.9. */
        if (n >= 64ul && (mpfr_zero_p(magnitude) || mpfr_get_exp(magnitude) < -precision + 32)) {
            converged = true;
            break;
        }
    }
    number_t value = regression_resolved(num_hypergeometric_pFq(a, p, a + p, q, a[p + q]));
    number_t reference = converged ? number_take(number_wrap_complex_mpc(sum, precision)) : NUM_NAN;
    number_t error = num_sub(value, reference);
    if (!num_is_zero(reference)) error = num_div(error, num_abs(reference));
    for (size_t j = 0; j < p + q; ++j) mpc_clear(parameters[j]);
    mpfr_clear(magnitude);
    mpc_clear(z); mpc_clear(term); mpc_clear(sum); mpc_clear(factor); mpc_clear(denominator); mpc_clear(shift);
    return error;
}

static number_t regression_pfq_0f0_reference(const number_t *a) { return regression_pfq_reference_value(a, 0, 0); }
static number_t regression_pfq_0f1_reference(const number_t *a) { return regression_pfq_reference_value(a, 0, 1); }
static number_t regression_pfq_1f0_reference(const number_t *a) { return regression_pfq_reference_value(a, 1, 0); }
static number_t regression_pfq_1f1_reference(const number_t *a) { return regression_pfq_reference_value(a, 1, 1); }
static number_t regression_pfq_2f1_reference(const number_t *a) { return regression_pfq_reference_value(a, 2, 1); }
static number_t regression_pfq_2f2_reference(const number_t *a) { return regression_pfq_reference_value(a, 2, 2); }
static number_t regression_pfq_3f2_reference(const number_t *a) { return regression_pfq_reference_value(a, 3, 2); }

static number_t regression_pfq_pole(const number_t *a)
{
    return num_is_nan(num_hypergeometric_pFq(a, 1, a + 1, 1, a[2])) ? NUM_ZERO : NUM_ONE;
}

static number_t regression_lerch_log(const number_t *a)
{
    number_t one = num_add(num_new_with_prec_bits(num_get_default_prec_bits()), NUM_ONE);
    number_t value = regression_resolved(num_lerch_phi(a[0], one, one));
    return num_add(value, num_div(num_ln(num_sub(NUM_ONE, a[0])), a[0]));
}

static number_t regression_lerch_exact(const number_t *a)
{
    number_t denominator = num_create_from_long((long)num_to_double(a[0]));
    number_t z = num_div(NUM_ONE, denominator);
    number_t value = regression_resolved(num_lerch_phi(z, NUM_ONE, NUM_ONE));
    number_t argument = num_div(NUM_ONE, a[0]);
    return num_add(value, num_div(num_ln(num_sub(NUM_ONE, argument)), argument));
}

static number_t regression_lerch_shift(const number_t *a)
{
    number_t value = regression_resolved(num_lerch_phi(a[0], a[1], a[2]));
    number_t next = num_lerch_phi(a[0], a[1], num_add(a[2], NUM_ONE));
    return num_sub(num_sub(value, num_mul(a[0], next)), num_pow(a[2], num_neg(a[1])));
}

/* SQL supplies small dyadic arguments. This independent full-precision MPC sum neither adapts term precision
 * nor uses the production logarithm/power caches. The two fixture ranges have term ratios below 0.9 after
 * k=100 or k=2000 respectively, including the large negative order which forces uniform-precision retry. */
typedef enum {
    LERCH_REFERENCE_PUBLIC,
    LERCH_REFERENCE_BLOCKS,
    LERCH_REFERENCE_INTEGER
} lerch_reference_kind_t;

static number_t regression_lerch_reference_value(const number_t *a, lerch_reference_kind_t kind)
{
    mpfr_prec_t precision = (mpfr_prec_t)num_get_default_prec_bits() + 256;
    mpc_t z, negative, offset, base, power, term, sum;
    mpc_init2(z, precision); mpc_init2(negative, precision); mpc_init2(offset, precision);
    mpc_init2(base, precision); mpc_init2(power, precision); mpc_init2(term, precision); mpc_init2(sum, precision);
    bool inputs = regression_mpc_input(z, a[0]) && regression_mpc_input(negative, a[1]) &&
                  regression_mpc_input(offset, a[2]);
    mpc_neg(negative, negative, MPC_RNDNN);
    mpc_set_ui(power, 1u, MPC_RNDNN); mpc_set_ui(sum, 0u, MPC_RNDNN);
    mpfr_t magnitude, bound;
    mpfr_inits2(precision, magnitude, bound, (mpfr_ptr)0);
    unsigned long reference_start = num_gt(num_abs(a[1]), num_create_from_long(10)) ? 2000u : 100u;
    bool converged = false;
    for (unsigned long k = 0; inputs && k < 20000; ++k) {
        mpc_add_ui(base, offset, k, MPC_RNDNN); mpc_pow(base, base, negative, MPC_RNDNN);
        mpc_mul(term, power, base, MPC_RNDNN); mpc_add(sum, sum, term, MPC_RNDNN);
        if (k > reference_start && k % 8 == 0) {
            mpc_abs(magnitude, term, MPFR_RNDU); mpc_abs(bound, sum, MPFR_RNDD);
            mpfr_mul_2si(bound, bound, -precision + 32, MPFR_RNDD);
            if (mpfr_lessequal_p(magnitude, bound)) { converged = true; break; }
        }
        mpc_mul(power, power, z, MPC_RNDNN);
    }
    char *text = NULL;
    int digits = (int)(precision * 0.302) + 4;
    if (converged) mpfr_asprintf(&text, "%.*Rg%+.*Rgi", digits, mpc_realref(sum), digits, mpc_imagref(sum));
    number_t reference = text ? num_create_from_string(text) : NUM_NAN;
    number_t result;
    if (kind != LERCH_REFERENCE_PUBLIC) {
        mpc_neg(negative, negative, MPC_RNDNN);
        mpfr_prec_t bits = (mpfr_prec_t)num_get_default_prec_bits();
        bool complete = kind == LERCH_REFERENCE_BLOCKS ? number_lerch_blocks(term, z, negative, offset, bits) :
            number_lerch_integer(term, a[0], a[2], (unsigned)-num_to_double(a[1]), bits);
        char *rendering = NULL;
        if (complete)
            mpfr_asprintf(&rendering, "%.*Rg%+.*Rgi", digits, mpc_realref(term), digits, mpc_imagref(term));
        result = rendering ? num_create_from_string(rendering) : NUM_NAN;
        if (rendering) mpfr_free_str(rendering);
    } else result = regression_resolved(num_lerch_phi(a[0], a[1], a[2]));
    number_t scale = num_abs(reference);
    if (num_lt(scale, NUM_ONE)) scale = NUM_ONE;
    number_t error = num_div(num_sub(result, reference), scale);
    if (kind == LERCH_REFERENCE_INTEGER) {
        number_t public_value = regression_resolved(num_lerch_phi(a[0], a[1], a[2]));
        number_t public_error = num_div(num_sub(public_value, reference), scale);
        error = num_add(num_abs(error), num_abs(public_error));
    }
    if (text) mpfr_free_str(text);
    mpfr_clears(magnitude, bound, (mpfr_ptr)0);
    mpc_clear(z); mpc_clear(negative); mpc_clear(offset); mpc_clear(base);
    mpc_clear(power); mpc_clear(term); mpc_clear(sum);
    return error;
}

static number_t regression_lerch_reference(const number_t *a)
{
    return regression_lerch_reference_value(a, LERCH_REFERENCE_PUBLIC);
}
static number_t regression_lerch_blocks_reference(const number_t *a)
{
    return regression_lerch_reference_value(a, LERCH_REFERENCE_BLOCKS);
}

static number_t regression_lerch_integer_reference(const number_t *a)
{
    return regression_lerch_reference_value(a, LERCH_REFERENCE_INTEGER);
}

static number_t regression_lerch_integer_exact(const number_t *a)
{
    number_t inputs[] = {num_create_from_frac(1, 5), num_neg(a[0]), num_create_from_frac(1, 3)};
    return regression_lerch_reference_value(inputs, LERCH_REFERENCE_INTEGER);
}

static number_t regression_lerch_integer_reject(const number_t *a)
{
    mpfr_prec_t bits = (mpfr_prec_t)num_get_default_prec_bits();
    mpc_t value;
    mpc_init2(value, bits); mpc_set_d_d(value, 7.25, 0.5, MPC_RNDNN);
    bool ok = !number_lerch_integer(value, a[0], a[1], (unsigned)num_to_double(a[2]), bits) &&
        !mpfr_cmp_d(mpc_realref(value), 7.25) && !mpfr_cmp_d(mpc_imagref(value), 0.5);
    mpc_clear(value);
    return ok ? NUM_ZERO : NUM_ONE;
}

static number_t regression_lerch_blocks_reject(const number_t *a)
{
    mpfr_prec_t bits = (mpfr_prec_t)num_get_default_prec_bits();
    mpc_t z, order, offset, output;
    mpc_init2(z, bits); mpc_init2(order, bits); mpc_init2(offset, bits); mpc_init2(output, bits);
    mpc_set_d_d(z, num_to_double(num_real_part(a[0])), num_to_double(num_imag_part(a[0])), MPC_RNDNN);
    mpc_set_d_d(order, num_to_double(num_real_part(a[1])), num_to_double(num_imag_part(a[1])), MPC_RNDNN);
    mpc_set_d_d(offset, num_to_double(num_real_part(a[2])), num_to_double(num_imag_part(a[2])), MPC_RNDNN);
    mpc_set_d_d(output, 7.25, 0.5, MPC_RNDNN);
    bool rejected = !number_lerch_blocks(output, z, order, offset, bits);
    bool unchanged = !mpfr_cmp_d(mpc_realref(output), 7.25) && !mpfr_cmp_d(mpc_imagref(output), 0.5);
    mpc_clear(z); mpc_clear(order); mpc_clear(offset); mpc_clear(output);
    return rejected && unchanged ? NUM_ZERO : NUM_ONE;
}
static number_t regression_qc_erfcinv(const number_t *a)
{
    return num_create_from_qcomplex(qc_erfc(qc_erfcinv(regression_qcomplex(a[0]))));
}

static bool regression_mpc_close(mpc_srcptr value, mpc_srcptr reference, mpfr_prec_t bits)
{
    mpfr_prec_t work = bits + 256;
    mpc_t difference;
    mpc_init2(difference, work);
    mpfr_t error, bound;
    mpfr_inits2(work, error, bound, (mpfr_ptr)0);
    mpc_sub(difference, value, reference, MPC_RNDNN); mpc_abs(error, difference, MPFR_RNDU);
    mpc_abs(bound, reference, MPFR_RNDD); mpfr_add_ui(bound, bound, 1u, MPFR_RNDD);
    mpfr_mul_2si(bound, bound, -bits + 8, MPFR_RNDD);
    bool ok = mpfr_number_p(error) && mpfr_lessequal_p(error, bound);
    mpfr_clears(error, bound, (mpfr_ptr)0); mpc_clear(difference);
    return ok;
}

static number_t regression_polygamma_blocks(const number_t *a)
{
    mpfr_prec_t bits = (mpfr_prec_t)num_get_default_prec_bits(), work = bits + 128;
    unsigned power = (unsigned)num_to_double(a[1]);
    unsigned long limit = (unsigned long)num_to_double(a[2]);
    mpc_t base, original, result, reference, shifted, term;
    mpc_init2(base, work); mpc_init2(original, work); mpc_init2(result, work);
    mpc_init2(reference, work + 256); mpc_init2(shifted, work + 256); mpc_init2(term, work + 256);
    bool ok = regression_mpc_input(base, a[0]);
    mpc_set(original, base, MPC_RNDNN);
    ok = number_mpc_reciprocal_sum(result, base, power, limit) && ok;
    mpc_set_ui(reference, 0u, MPC_RNDNN);
    mpc_set(shifted, original, MPC_RNDNN);
    unsigned count = 0;
    while (ok && mpfr_less_p(mpc_realref(shifted), mpc_realref(base)) && count++ < 65552) {
        mpc_pow_ui(term, shifted, power, MPC_RNDNN);
        mpc_ui_div(term, 1u, term, MPC_RNDNN);
        mpc_add(reference, reference, term, MPC_RNDNN);
        mpc_add_ui(shifted, shifted, 1u, MPC_RNDNN);
    }
    ok = mpc_cmp(shifted, base) == 0 && regression_mpc_close(result, reference, bits + 32) && ok;
    if (mpfr_zero_p(mpc_imagref(original))) {
        mpfr_t real_base, real_result;
        mpfr_inits2(work, real_base, real_result, (mpfr_ptr)0);
        mpfr_set(real_base, mpc_realref(original), MPFR_RNDN);
        ok = number_mpfr_reciprocal_sum(real_result, real_base, power, limit) && ok;
        ok = mpfr_equal_p(real_base, mpc_realref(base)) && mpfr_equal_p(real_result, mpc_realref(result)) && ok;
        mpfr_clears(real_base, real_result, (mpfr_ptr)0);
    }
    mpc_clear(term); mpc_clear(shifted); mpc_clear(reference); mpc_clear(result);
    mpc_clear(original); mpc_clear(base);
    return ok ? NUM_ZERO : NUM_ONE;
}

static number_t regression_polygamma_blocks_reject(const number_t *a)
{
    mpfr_prec_t work = (mpfr_prec_t)num_get_default_prec_bits() + 128;
    mpc_t base, original, result;
    mpc_init2(base, work); mpc_init2(original, work); mpc_init2(result, work);
    bool ok = regression_mpc_input(base, a[0]);
    mpc_set(original, base, MPC_RNDNN); mpc_set_si(result, 37, MPC_RNDNN);
    ok = !number_mpc_reciprocal_sum(result, base, (unsigned)num_to_double(a[1]),
                                   (unsigned long)num_to_double(a[2])) && ok;
    ok = mpc_cmp(base, original) == 0 && mpc_cmp_si(result, 37) == 0 && ok;
    mpc_clear(result); mpc_clear(original); mpc_clear(base);
    return ok ? NUM_ZERO : NUM_ONE;
}

/* SQL uses dyadic components. Independent full-width powers and logarithms check both finite sums.
 * A value-only request precedes the paired request, exercising the finite cache's derivative upgrade. */
static number_t regression_zeta_blocks(const number_t *a)
{
    mpfr_prec_t bits = (mpfr_prec_t)num_get_default_prec_bits(), work = bits + 256;
    unsigned long count = (unsigned long)num_to_double(a[2]);
    mpc_t s, offset, value, slope, reference, reference_slope, base, negative, power, term, logarithm;
    mpc_init2(s, work); mpc_init2(offset, work); mpc_init2(value, bits); mpc_init2(slope, bits);
    mpc_init2(reference, work); mpc_init2(reference_slope, work); mpc_init2(base, work);
    mpc_init2(negative, work); mpc_init2(power, work); mpc_init2(term, work); mpc_init2(logarithm, work);
    mpc_set_d_d(s, num_to_double(num_real_part(a[0])), num_to_double(num_imag_part(a[0])), MPC_RNDNN);
    mpc_set_d_d(offset, num_to_double(num_real_part(a[1])), num_to_double(num_imag_part(a[1])), MPC_RNDNN);
    mpc_neg(negative, s, MPC_RNDNN);
    mpc_set_ui(reference, 0u, MPC_RNDNN); mpc_set_ui(reference_slope, 0u, MPC_RNDNN);
    for (unsigned long n = 0; n < count; ++n) {
        mpc_add_ui(base, offset, n, MPC_RNDNN); mpc_pow(power, base, negative, MPC_RNDNN);
        mpc_add(reference, reference, power, MPC_RNDNN);
        mpc_log(logarithm, base, MPC_RNDNN); mpc_mul(term, power, logarithm, MPC_RNDNN);
        mpc_sub(reference_slope, reference_slope, term, MPC_RNDNN);
    }
    bool ok = number_zeta_shifted_sum(value, NULL, s, offset, count) && regression_mpc_close(value, reference, bits);
    ok = number_zeta_shifted_sum(value, slope, s, offset, count) && ok;
    ok = regression_mpc_close(value, reference, bits) && regression_mpc_close(slope, reference_slope, bits) && ok;
    ok = number_zeta_shifted_sum(value, slope, s, offset, count) && ok;
    ok = regression_mpc_close(value, reference, bits) && regression_mpc_close(slope, reference_slope, bits) && ok;
    mpc_clear(logarithm); mpc_clear(term); mpc_clear(power); mpc_clear(negative); mpc_clear(base);
    mpc_clear(reference_slope); mpc_clear(reference); mpc_clear(slope); mpc_clear(value); mpc_clear(offset); mpc_clear(s);
    return ok ? NUM_ZERO : NUM_ONE;
}

static number_t regression_zeta_blocks_reject(const number_t *a)
{
    mpfr_prec_t bits = (mpfr_prec_t)num_get_default_prec_bits();
    mpc_t s, offset, value, slope;
    mpc_init2(s, bits); mpc_init2(offset, bits); mpc_init2(value, bits); mpc_init2(slope, bits);
    mpc_set_d_d(s, num_to_double(num_real_part(a[0])), num_to_double(num_imag_part(a[0])), MPC_RNDNN);
    mpc_set_d_d(offset, num_to_double(num_real_part(a[1])), num_to_double(num_imag_part(a[1])), MPC_RNDNN);
    mpc_set_d_d(value, 7.25, 0.5, MPC_RNDNN); mpc_set_d_d(slope, 8.25, 0.75, MPC_RNDNN);
    bool ok = !number_zeta_shifted_sum(value, slope, s, offset, (unsigned long)num_to_double(a[2])) &&
        !mpfr_cmp_d(mpc_realref(value), 7.25) && !mpfr_cmp_d(mpc_imagref(value), 0.5) &&
        !mpfr_cmp_d(mpc_realref(slope), 8.25) && !mpfr_cmp_d(mpc_imagref(slope), 0.75);
    mpc_clear(slope); mpc_clear(value); mpc_clear(offset); mpc_clear(s);
    return ok ? NUM_ZERO : NUM_ONE;
}

/* A deliberately unsupported short-path budget selects the original full-state recurrence for the reference.
 * Inputs are dyadic; the reference retains 256 extra target bits and independent full-width state. */
static number_t regression_zeta_tail_reference(const number_t *a)
{
    mpfr_prec_t bits = (mpfr_prec_t)num_get_default_prec_bits(), work = bits + 384;
    mpc_t s, endpoint, value, slope, reference, reference_slope;
    mpc_init2(s, work); mpc_init2(endpoint, work);
    mpc_init2(value, bits + 128); mpc_init2(slope, bits + 128);
    mpc_init2(reference, work); mpc_init2(reference_slope, work);
    mpc_set_d_d(s, num_to_double(num_real_part(a[0])), num_to_double(num_imag_part(a[0])), MPC_RNDNN);
    mpc_set_d_d(endpoint, num_to_double(num_real_part(a[1])), num_to_double(num_imag_part(a[1])), MPC_RNDNN);
    mpc_set_ui(value, 0u, MPC_RNDNN); mpc_set_ui(slope, 0u, MPC_RNDNN);
    mpc_set_ui(reference, 0u, MPC_RNDNN); mpc_set_ui(reference_slope, 0u, MPC_RNDNN);
    bool ok = !number_zeta_tail(reference, reference_slope, s, endpoint, bits + 256, 8193, 32);
    ok = number_zeta_tail_short(value, slope, s, endpoint, bits, 8192, 32) && ok;
    ok = regression_mpc_close(value, reference, bits) && regression_mpc_close(slope, reference_slope, bits) && ok;
    mpc_set_ui(value, 0u, MPC_RNDNN);
    ok = number_zeta_tail_short(value, NULL, s, endpoint, bits, 8192, 32) && ok;
    ok = regression_mpc_close(value, reference, bits) && ok;
    mpc_clear(reference_slope); mpc_clear(reference); mpc_clear(slope); mpc_clear(value);
    mpc_clear(endpoint); mpc_clear(s);
    return ok ? NUM_ZERO : NUM_ONE;
}

static number_t regression_zeta_tail_rejected(const number_t *a, size_t budget)
{
    mpfr_prec_t bits = (mpfr_prec_t)num_get_default_prec_bits();
    mpc_t s, endpoint, value, slope;
    mpc_init2(s, bits + 128); mpc_init2(endpoint, bits + 128);
    mpc_init2(value, bits + 128); mpc_init2(slope, bits + 128);
    mpc_set_d_d(s, num_to_double(num_real_part(a[0])), num_to_double(num_imag_part(a[0])), MPC_RNDNN);
    mpc_set_d_d(endpoint, num_to_double(num_real_part(a[1])), num_to_double(num_imag_part(a[1])), MPC_RNDNN);
    mpc_set_d_d(value, 7.25, 0.5, MPC_RNDNN); mpc_set_d_d(slope, 8.25, 0.75, MPC_RNDNN);
    bool ok = !number_zeta_tail_short(value, slope, s, endpoint, bits, budget, 32) &&
        !mpfr_cmp_d(mpc_realref(value), 7.25) && !mpfr_cmp_d(mpc_imagref(value), 0.5) &&
        !mpfr_cmp_d(mpc_realref(slope), 8.25) && !mpfr_cmp_d(mpc_imagref(slope), 0.75);
    mpc_clear(slope); mpc_clear(value); mpc_clear(endpoint); mpc_clear(s);
    return ok ? NUM_ZERO : NUM_ONE;
}

static number_t regression_zeta_tail_reject(const number_t *a)
{
    return regression_zeta_tail_rejected(a, 8192);
}

static number_t regression_zeta_tail_budget(const number_t *a)
{
    return regression_zeta_tail_rejected(a, 4);
}

static number_t regression_bessel_half(const number_t *a)
{
    number_t reference = num_mul(num_sqrt(num_div(NUM_TWO, num_mul(num_const(NUM_PI), a[0]))), num_sin(a[0]));
    return num_sub(num_bessel_J(NUM_HALF, a[0]), reference);
}

static number_t regression_qc_bessel_half(const number_t *a)
{
    number_t reference = num_mul(num_sqrt(num_div(NUM_TWO, num_mul(num_const(NUM_PI), a[0]))), num_sin(a[0]));
    return num_sub(num_create_from_qcomplex(qc_bessel_J(qc_make(QF_HALF, QF_ZERO), regression_qcomplex(a[0]))), reference);
}

static number_t regression_lommel(const number_t *a)
{
    return num_sub(num_lommel_S(NUM_ONE, NUM_ZERO, a[0]), num_sub(NUM_ONE, num_bessel_J(NUM_ZERO, a[0])));
}

static number_t regression_qc_lommel(const number_t *a)
{
    number_t value = num_create_from_qcomplex(qc_lommel_S(QC_ONE, QC_ZERO, regression_qcomplex(a[0])));
    return num_sub(value, num_sub(NUM_ONE, num_bessel_J(NUM_ZERO, a[0])));
}

static number_t regression_lommel_recurrence(const number_t *a)
{
    number_t factor = num_sub(num_sqr(num_add(a[0], NUM_ONE)), num_sqr(a[1]));
    number_t sum = num_add(num_lommel_S(num_add(a[0], NUM_TWO), a[1], a[2]),
                          num_mul(factor, num_lommel_S(a[0], a[1], a[2])));
    return num_sub(sum, num_pow(a[2], num_add(a[0], NUM_ONE)));
}

static number_t regression_appell_product(const number_t *a)
{
    number_t reference = num_mul(num_pow(num_sub(NUM_ONE, a[4]), num_neg(a[1])),
                                 num_pow(num_sub(NUM_ONE, a[5]), num_neg(a[2])));
    return num_sub(num_appell_F1(a[0], a[1], a[2], a[3], a[4], a[5]), reference);
}

static number_t regression_appell_log(const number_t *a)
{
    number_t reference = num_div(num_sub(num_ln(num_sub(NUM_ONE, a[5])), num_ln(num_sub(NUM_ONE, a[4]))),
                                 num_sub(a[4], a[5]));
    return num_sub(num_appell_F1(a[0], a[1], a[2], a[3], a[4], a[5]), reference);
}

static number_t regression_appell_linear(const number_t *a)
{
    number_t linear = num_add(num_mul(a[1], a[4]), num_mul(a[2], a[5]));
    number_t reference = num_sub(NUM_ONE, num_div(linear, a[3]));
    return num_sub(num_appell_F1(a[0], a[1], a[2], a[3], a[4], a[5]), reference);
}

static number_t regression_appell_quadratic(const number_t *a)
{
    number_t linear = num_add(num_mul(a[1], a[4]), num_mul(a[2], a[5]));
    number_t xx = num_mul(num_mul(a[1], num_add(a[1], NUM_ONE)), num_sqr(a[4]));
    number_t xy = num_mul(NUM_TWO, num_mul(num_mul(a[1], a[2]), num_mul(a[4], a[5])));
    number_t yy = num_mul(num_mul(a[2], num_add(a[2], NUM_ONE)), num_sqr(a[5]));
    number_t reference = num_sub(NUM_ONE, num_div(num_mul(NUM_TWO, linear), a[3]));
    reference = num_add(reference, num_div(num_add(num_add(xx, xy), yy), num_mul(a[3], num_add(a[3], NUM_ONE))));
    return num_sub(num_appell_F1(a[0], a[1], a[2], a[3], a[4], a[5]), reference);
}

static number_t regression_appell_nan(const number_t *a)
{
    return num_is_nan(num_appell_F1(a[0], a[1], a[2], a[3], a[4], a[5])) ? NUM_ZERO : NUM_NAN;
}

static number_t regression_normal_cdf(const number_t *a)
{
    number_t scaled = num_div(a[0], num_sqrt(NUM_TWO));
    return num_sub(num_normal_cdf(a[0]), num_mul(NUM_HALF, num_add(NUM_ONE, num_erf(scaled))));
}

static number_t regression_normal_pdf(const number_t *a)
{
    number_t reference = num_div(num_exp(num_mul(num_neg(NUM_HALF), num_sqr(a[0]))),
                                 num_sqrt(num_mul(NUM_TWO, num_const(NUM_PI))));
    return num_sub(num_normal_pdf(a[0]), reference);
}

static number_t regression_normal_logpdf(const number_t *a)
{
    return num_sub(num_exp(num_normal_logpdf(a[0])), num_normal_pdf(a[0]));
}

static number_t regression_normal_tail(const number_t *a)
{
    number_t reference = num_mul(NUM_HALF, num_erfc(num_div(num_neg(a[0]), num_sqrt(NUM_TWO))));
    return num_div(num_normal_cdf(a[0]), reference);
}

static number_t regression_qf_normal_tail(const number_t *a)
{
    number_t reference = num_mul(NUM_HALF, num_erfc(num_div(num_neg(a[0]), num_sqrt(NUM_TWO))));
    return num_div(num_create_from_qfloat(qf_normal_cdf(num_to_qfloat(a[0]))), reference);
}

static number_t regression_digamma(const number_t *a)
{
    return num_sub(num_sub(num_digamma(num_add(a[0], NUM_ONE)), num_digamma(a[0])), num_div(NUM_ONE, a[0]));
}

static number_t regression_polygamma(const number_t *a)
{
    number_t delta = num_sub(num_polygamma(3u, num_add(a[0], NUM_ONE)), num_polygamma(3u, a[0]));
    return num_add(delta, num_div(num_create_from_long(6), num_pow(a[0], num_create_from_long(4))));
}

static number_t regression_polygamma_value(const number_t *a)
{
    return num_sub(num_polygamma(3u, a[0]), num_div(num_pow(num_const(NUM_PI), num_create_from_long(4)),
                                                  num_create_from_long(15)));
}

static number_t regression_trigamma(const number_t *a)
{
    return num_add(num_sub(num_trigamma(num_add(a[0], NUM_ONE)), num_trigamma(a[0])),
                    num_div(NUM_ONE, num_sqr(a[0])));
}

static number_t regression_tetragamma(const number_t *a)
{
    return num_sub(num_sub(num_tetragamma(num_add(a[0], NUM_ONE)), num_tetragamma(a[0])),
                    num_div(NUM_TWO, num_pow(a[0], num_create_from_long(3))));
}

static number_t regression_trigamma_reflection(const number_t *a)
{
    number_t pi = num_const(NUM_PI), sine = num_sin(num_mul(pi, a[0]));
    number_t reference = num_div(num_sqr(pi), num_sqr(sine));
    return num_sub(num_add(num_trigamma(a[0]), num_trigamma(num_sub(NUM_ONE, a[0]))), reference);
}

static number_t regression_tetragamma_reflection(const number_t *a)
{
    number_t pi = num_const(NUM_PI), angle = num_mul(pi, a[0]);
    number_t reference = num_div(num_mul(NUM_TWO, num_mul(num_pow(pi, num_create_from_long(3)), num_cos(angle))),
                                 num_pow(num_sin(angle), num_create_from_long(3)));
    return num_sub(num_sub(num_tetragamma(num_sub(NUM_ONE, a[0])), num_tetragamma(a[0])), reference);
}

static number_t regression_zeta(const number_t *a)
{
    return num_sub(num_zeta(a[0]), num_div(num_sqr(num_const(NUM_PI)), num_create_from_long(6)));
}

static number_t regression_zeta_reference_value(const number_t *a, bool hurwitz)
{
    /* SQL inputs are binary fractions, represented exactly by this independent MPFR input. */
    mpfr_t reference, order, factor;
    mpfr_inits2((mpfr_prec_t)num_get_default_prec_bits() + 128, reference, order, factor, (mpfr_ptr)0);
    mpfr_set_d(order, num_to_double(a[0]), MPFR_RNDN);
    mpfr_zeta(reference, order, MPFR_RNDN);
    if (hurwitz && num_eq(a[1], NUM_HALF)) {
        mpfr_exp2(factor, order, MPFR_RNDN);
        mpfr_sub_ui(factor, factor, 1u, MPFR_RNDN);
        mpfr_mul(reference, reference, factor, MPFR_RNDN);
    }
    char *text = NULL;
    mpfr_asprintf(&text, "%.*Rg", (int)(mpfr_get_prec(reference) * 0.302) + 4, reference);
    number_t expected = text ? num_create_from_string(text) : NUM_NAN;
    number_t scale = num_abs(expected);
    if (num_lt(scale, NUM_ONE)) scale = NUM_ONE;
    number_t value = hurwitz ? num_zetah(a[0], a[1]) : num_zeta(a[0]);
    number_t error = num_div(num_sub(value, expected), scale);
    if (text) mpfr_free_str(text);
    mpfr_clears(reference, order, factor, (mpfr_ptr)0);
    return error;
}

static number_t regression_zeta_reference(const number_t *a) { return regression_zeta_reference_value(a, false); }
static number_t regression_zetah_reference(const number_t *a) { return regression_zeta_reference_value(a, true); }

static number_t regression_dilog_diagonal(const number_t *a)
{
    number_t sum = num_add(num_dilog(a[0]), num_dilog(num_conj(a[0])));
    number_t log_two = num_ln(NUM_TWO);
    number_t reference = num_sub(num_mul(num_create_from_frac(5, 48), num_sqr(num_const(NUM_PI))),
                                 num_div(num_sqr(log_two), num_create_from_long(4)));
    return num_sub(sum, reference);
}

/* Independent integer-zeta coefficients check the new dilogarithm continuation against the defining expansion. */
static number_t regression_clausen_reference(const number_t *a)
{
    mpfr_prec_t precision = (mpfr_prec_t)num_get_default_prec_bits() + 128;
    mpc_t z, ratio, term, sum, logarithm, add;
    mpc_init2(z, precision); mpc_init2(ratio, precision); mpc_init2(term, precision);
    mpc_init2(sum, precision); mpc_init2(logarithm, precision); mpc_init2(add, precision);
    mpfr_t period, coefficient, magnitude;
    mpfr_inits2(precision, period, coefficient, magnitude, (mpfr_ptr)0);
    mpc_set_d_d(z, num_to_double(num_real_part(a[0])), num_to_double(num_imag_part(a[0])), MPC_RNDNN);
    bool negative = mpfr_sgn(mpc_realref(z)) < 0;
    if (negative) mpc_neg(z, z, MPC_RNDNN);
    mpc_log(logarithm, z, MPC_RNDNN);
    mpc_ui_sub(logarithm, 1u, logarithm, MPC_RNDNN);
    mpc_mul(sum, z, logarithm, MPC_RNDNN);
    mpfr_const_pi(period, MPFR_RNDN); mpfr_mul_2ui(period, period, 1u, MPFR_RNDN);
    mpfr_sqr(period, period, MPFR_RNDN);
    mpc_sqr(ratio, z, MPC_RNDNN); mpc_div_fr(ratio, ratio, period, MPC_RNDNN);
    mpc_mul(term, z, ratio, MPC_RNDNN); mpc_div_ui(term, term, 3u, MPC_RNDNN);
    bool converged = false;
    for (unsigned long j = 1; j < 10000; ++j) {
        mpfr_zeta_ui(coefficient, 2u * j, MPFR_RNDN);
        mpc_mul_fr(add, term, coefficient, MPC_RNDNN); mpc_add(sum, sum, add, MPC_RNDNN);
        number_mpc_abs_upper(magnitude, add);
        if (mpfr_zero_p(magnitude) || mpfr_get_exp(magnitude) < -precision + 16) {
            converged = true;
            break;
        }
        mpc_mul(term, term, ratio, MPC_RNDNN);
        mpc_mul_ui(term, term, (2u * j) * (2u * j + 1u), MPC_RNDNN);
        mpc_div_ui(term, term, (2u * j + 2u) * (2u * j + 3u), MPC_RNDNN);
    }
    if (negative) mpc_neg(sum, sum, MPC_RNDNN);
    number_t reference = converged ? number_take(number_wrap_complex_mpc(sum, precision)) : num_clone(NUM_NAN);
    number_t result = num_sub(num_clausen2(a[0]), reference);
    mpfr_clears(period, coefficient, magnitude, (mpfr_ptr)0);
    mpc_clear(z); mpc_clear(ratio); mpc_clear(term); mpc_clear(sum); mpc_clear(logarithm); mpc_clear(add);
    return result;
}

static number_t regression_appell_reduced(const number_t *a)
{
    number_t value = num_appell_F1(a[0], a[1], a[2], a[3], a[4], a[5]);
    return num_sub(value, num_pow(num_sub(NUM_ONE, a[4]), num_neg(a[0])));
}

/* Equal arguments reduce the double series to an independently evaluated Gauss hypergeometric series. */
static number_t regression_appell_equal(const number_t *a)
{
    number_t upper[] = {a[0], num_add(a[1], a[2])};
    number_t reference = num_hypergeometric_pFq(upper, 2, a + 3, 1, a[4]);
    return num_sub(num_appell_F1(a[0], a[1], a[2], a[3], a[4], a[5]), reference);
}

/* For b1=b2=1, partial fractions reduce distinct arguments to two independent Gauss evaluations.
 * Extra reference bits protect the subtraction when x and y are close, while preserving the exact supplied inputs. */
static number_t regression_appell_partial_fraction(const number_t *a)
{
    number_t values[] = {num_clone(a[0]), num_clone(a[3]), num_clone(a[4]), num_clone(a[5])};
    size_t saved = num_get_default_prec_bits(), reference_bits = saved + 128;
    if (num_set_default_prec_bits(reference_bits)) return NUM_NAN;
    for (unsigned i = 0; i < 4; ++i) {
        if (num_set_prec_bits(values + i, reference_bits)) {
            num_set_default_prec_bits(saved);
            return NUM_NAN;
        }
    }
    number_t upper[] = {values[0], NUM_ONE};
    number_t left = num_hypergeometric_pFq(upper, 2, values + 1, 1, values[2]);
    number_t right = num_hypergeometric_pFq(upper, 2, values + 1, 1, values[3]);
    number_t reference = num_div(num_sub(num_mul(values[2], left), num_mul(values[3], right)),
                                 num_sub(values[2], values[3]));
    num_set_default_prec_bits(saved);
    return num_sub(num_appell_F1(a[0], a[1], a[2], a[3], a[4], a[5]), reference);
}

/* Dyadic SQL inputs are exact in double; the reference sums individual powers at extra precision. */
static number_t regression_polylog_reference(const number_t *a)
{
    unsigned long order = (unsigned long)num_to_double(a[0]);
    mpfr_prec_t precision = (mpfr_prec_t)num_get_default_prec_bits() + 128;
    mpc_t z, power, sum, term;
    mpc_init2(z, precision); mpc_init2(power, precision); mpc_init2(sum, precision); mpc_init2(term, precision);
    mpfr_t denominator, magnitude;
    mpfr_inits2(precision, denominator, magnitude, (mpfr_ptr)0);
    mpc_set_d_d(z, num_to_double(num_real_part(a[1])), num_to_double(num_imag_part(a[1])), MPC_RNDNN);
    mpc_set(power, z, MPC_RNDNN); mpc_set_ui(sum, 0u, MPC_RNDNN);
    bool converged = false;
    for (unsigned long k = 1; k <= 200000; ++k) {
        mpfr_ui_pow_ui(denominator, k, order, MPFR_RNDN);
        mpc_div_fr(term, power, denominator, MPC_RNDNN);
        mpc_add(sum, sum, term, MPC_RNDNN);
        number_mpc_abs_upper(magnitude, term);
        if (mpfr_zero_p(magnitude) || mpfr_get_exp(magnitude) < -precision + 16) {
            converged = true;
            break;
        }
        mpc_mul(power, power, z, MPC_RNDNN);
    }
    if (order == 2 && num_is_real(a[1])) {
        mpfr_li2(mpc_realref(sum), mpc_realref(z), MPFR_RNDN);
        mpfr_set_zero(mpc_imagref(sum), 1);
    }
    number_t reference = converged ? number_take(number_wrap_complex_mpc(sum, precision)) : num_clone(NUM_NAN);
    number_t result = num_sub(num_polylog(a[0], a[1]), reference);
    mpfr_clears(denominator, magnitude, (mpfr_ptr)0);
    mpc_clear(z); mpc_clear(power); mpc_clear(sum); mpc_clear(term);
    return result;
}

static number_t regression_polylog_half(const number_t *a)
{
    number_t log_two = num_ln(NUM_TWO);
    number_t reference = num_sub(num_mul(num_create_from_frac(7, 8), num_zeta(num_create_from_long(3))),
        num_div(num_mul(num_sqr(num_const(NUM_PI)), log_two), num_create_from_long(12)));
    reference = num_add(reference, num_div(num_pow(log_two, num_create_from_long(3)), num_create_from_long(6)));
    return num_sub(num_polylog(a[0], NUM_HALF), reference);
}

static number_t regression_polylog_limit(const number_t *a) { return num_polylog(a[0], a[1]); }
static number_t regression_chi_limit(const number_t *a) { return num_legendre_chi(a[0], a[1]); }

static number_t regression_zetap(const number_t *a)
{
    return num_add(num_zetap(a[0]), num_mul(NUM_HALF, num_ln(num_mul(NUM_TWO, num_const(NUM_PI)))));
}

static number_t regression_zeta_pair(const number_t *a)
{
    number_t derivative;
    number_t value = regression_resolved(num_zeta_pair(a[0], &derivative));
    derivative = regression_resolved(derivative);
    number_t value_error = num_abs(num_sub(value, num_zetah(a[0], NUM_ONE)));
    number_t derivative_error = num_abs(num_sub(derivative, num_zatahp(a[0], NUM_ONE)));
    return num_add(value_error, derivative_error);
}

static number_t regression_zetah(const number_t *a)
{
    number_t delta = num_sub(num_zetah(a[0], a[1]), num_zetah(a[0], num_add(a[1], NUM_ONE)));
    return num_sub(delta, num_pow(a[1], num_neg(a[0])));
}

static number_t regression_zatahp(const number_t *a)
{
    number_t delta = num_sub(num_zatahp(a[0], a[1]), num_zatahp(a[0], num_add(a[1], NUM_ONE)));
    return num_add(delta, num_mul(num_ln(a[1]), num_pow(a[1], num_neg(a[0]))));
}

/* Check public integer-order derivatives against individually evaluated finite powers and logarithms.
 * The reference tail keeps extra precision and uses the original full-state recurrence. */
static number_t regression_zatahp_reference(const number_t *a)
{
    mpfr_prec_t bits = (mpfr_prec_t)num_get_default_prec_bits(), work = bits + 384;
    unsigned long count = 128ul + (bits > 512 ? (unsigned long)(bits - 512) / 8ul : 0ul);
    mpc_t s, offset, negative, base, logarithm, power, term, sum, slope, endpoint, value;
    mpc_init2(s, work); mpc_init2(offset, work); mpc_init2(negative, work); mpc_init2(base, work);
    mpc_init2(logarithm, work); mpc_init2(power, work); mpc_init2(term, work); mpc_init2(sum, work);
    mpc_init2(slope, work); mpc_init2(endpoint, work); mpc_init2(value, work);
    bool ok = regression_mpc_input(s, a[0]) && regression_mpc_input(offset, a[1]);
    mpc_neg(negative, s, MPC_RNDNN); mpc_set_ui(sum, 0u, MPC_RNDNN); mpc_set_ui(slope, 0u, MPC_RNDNN);
    for (unsigned long n = 0; ok && n < count; ++n) {
        mpc_add_ui(base, offset, n, MPC_RNDNN); mpc_pow(power, base, negative, MPC_RNDNN);
        mpc_add(sum, sum, power, MPC_RNDNN);
        mpc_log(logarithm, base, MPC_RNDNN); mpc_mul(term, power, logarithm, MPC_RNDNN);
        mpc_sub(slope, slope, term, MPC_RNDNN);
    }
    mpc_add_ui(endpoint, offset, count, MPC_RNDNN);
    ok = ok && !number_zeta_tail(sum, slope, s, endpoint, bits + 256, 8193, 32);
    number_t result = regression_resolved(num_zatahp(a[0], a[1]));
    ok = regression_mpc_input(value, result) && ok;
    ok = regression_mpc_close(value, slope, bits) && ok;
    mpc_clear(s); mpc_clear(offset); mpc_clear(negative); mpc_clear(base); mpc_clear(logarithm);
    mpc_clear(power); mpc_clear(term); mpc_clear(sum); mpc_clear(slope); mpc_clear(endpoint); mpc_clear(value);
    return ok ? NUM_ZERO : NUM_ONE;
}

static number_t regression_zatahp_zero(const number_t *a)
{
    number_t reference = num_sub(num_lgamma(a[0]), num_mul(NUM_HALF, num_ln(num_mul(NUM_TWO, num_const(NUM_PI)))));
    return num_sub(num_zatahp(NUM_ZERO, a[0]), reference);
}

static number_t regression_zetah_value(const number_t *a) { return num_zetah(a[0], a[1]); }

static number_t regression_zetah_negative_one(const number_t *a)
{
    number_t polynomial = num_add(num_sub(num_sqr(a[0]), a[0]), num_create_from_frac(1, 6));
    return num_add(num_zetah(NUM_NEG_ONE, a[0]), num_mul(NUM_HALF, polynomial));
}

static number_t regression_clausen_catalan(const number_t *a)
{
    mpfr_t reference;
    mpfr_init2(reference, (mpfr_prec_t)num_get_default_prec_bits() + 64);
    mpfr_const_catalan(reference, MPFR_RNDN);
    char *text = NULL;
    int digits = (int)(mpfr_get_prec(reference) * 0.302) + 4;
    mpfr_asprintf(&text, "%.*Rg", digits, reference);
    number_t expected = text ? num_create_from_string(text) : NUM_NAN;
    number_t result = num_sub(num_clausen2(num_mul(a[0], num_const(NUM_PI))), expected);
    if (text) mpfr_free_str(text);
    mpfr_clear(reference);
    return result;
}

static number_t regression_clausen_odd(const number_t *a)
{
    return num_add(num_clausen(3u, num_const(NUM_PI)), num_mul(num_create_from_string("3/4"), num_zeta(a[0])));
}

static number_t regression_polygamma_half(const number_t *a)
{
    unsigned long order = (unsigned long)num_to_double(a[0]);
    number_t degree = num_add(a[0], NUM_ONE);
    number_t factor = num_mul(num_factorial(order), num_sub(num_pow(NUM_TWO, degree), NUM_ONE));
    if (!(order & 1u)) factor = num_neg(factor);
    number_t reference = num_mul(factor, num_zeta(degree));
    return num_div(num_sub(num_polygamma((unsigned int)order, NUM_HALF), reference), reference);
}

static number_t regression_appell_family_value(unsigned family, number_t x, number_t y, bool terminating)
{
    number_t first = terminating ? NUM_NEG_ONE : NUM_ONE;
    if (family == 2) return num_appell_F2(first, NUM_ONE, NUM_ONE, NUM_ONE, NUM_ONE, x, y);
    if (family == 3) return num_appell_F3(first, first, NUM_ONE, NUM_ONE, NUM_TWO, x, y);
    return num_appell_F4(first, NUM_ONE, NUM_ONE, NUM_ONE, x, y);
}

static number_t regression_appell_family(const number_t *a)
{
    unsigned family = (unsigned)num_to_double(a[0]);
    number_t x = a[1], y = a[2], reference;
    number_t remaining = num_sub(num_sub(NUM_ONE, x), y);
    if (family == 2) reference = num_inv(remaining);
    else if (family == 3) {
        number_t logarithm = num_ln(num_mul(num_sub(NUM_ONE, x), num_sub(NUM_ONE, y)));
        reference = num_div(num_neg(logarithm), num_sub(num_add(x, y), num_mul(x, y)));
    } else {
        number_t discriminant = num_sub(num_sqr(remaining), num_mul(num_create_from_long(4), num_mul(x, y)));
        reference = num_inv(num_sqrt(discriminant));
    }
    return num_div(num_sub(regression_appell_family_value(family, x, y, false), reference), reference);
}

static number_t regression_appell_family_terminate(const number_t *a)
{
    unsigned family = (unsigned)num_to_double(a[0]);
    number_t x = a[1], y = a[2], reference = num_sub(num_sub(NUM_ONE, x), y);
    if (family == 3) reference = num_add(num_sub(NUM_ONE, num_div(num_add(x, y), NUM_TWO)),
                                      num_div(num_mul(x, y), num_create_from_long(6)));
    return num_sub(regression_appell_family_value(family, x, y, true), reference);
}

static number_t regression_appell_family_reject(const number_t *a)
{
    return num_is_nan(regression_appell_family_value((unsigned)num_to_double(a[0]), a[1], a[2], false)) ?
        NUM_ZERO : NUM_ONE;
}

static number_t regression_appell_family_native(const number_t *a)
{
    unsigned family = (unsigned)num_to_double(a[0]);
    qcomplex_t x = regression_qcomplex(a[1]), y = regression_qcomplex(a[2]);
    qcomplex_t value = family == 2 ? qc_appell_F2(QC_ONE, QC_ONE, QC_ONE, QC_ONE, QC_ONE, x, y) :
        family == 3 ? qc_appell_F3(QC_ONE, QC_ONE, QC_ONE, QC_ONE, qc_make(qf_from_double(2), QF_ZERO), x, y) :
                      qc_appell_F4(QC_ONE, QC_ONE, QC_ONE, QC_ONE, x, y);
    number_t reference = regression_appell_family_value(family, a[1], a[2], false);
    number_t error = num_div(num_sub(num_create_from_qcomplex(value), reference), reference);
    if (num_is_real(a[1]) && num_is_real(a[2])) {
        qfloat_t real = family == 2 ? qf_appell_F2(QF_ONE, QF_ONE, QF_ONE, QF_ONE, QF_ONE,
                                                  qc_real(x), qc_real(y)) :
            family == 3 ? qf_appell_F3(QF_ONE, QF_ONE, QF_ONE, QF_ONE, qf_from_double(2),
                                       qc_real(x), qc_real(y)) :
                          qf_appell_F4(QF_ONE, QF_ONE, QF_ONE, QF_ONE, qc_real(x), qc_real(y));
        error = num_add(num_abs(error), num_abs(num_div(num_sub(num_create_from_qfloat(real), reference), reference)));
    }
    return error;
}

static number_t regression_appell_family_reduction_value(const number_t *a, bool native)
{
    unsigned family = (unsigned)num_to_double(a[0]);
    number_t x = a[1], y = a[2], value, reference;
    number_t half = NUM_HALF, three_quarters = num_create_from_frac(3, 4);
    number_t five_quarters = num_create_from_frac(5, 4), five_halves = num_create_from_frac(5, 2);
    if (family == 2) {
        reference = num_pow(num_sub(num_sub(NUM_ONE, x), y), num_neg(half));
        value = native ? num_create_from_qcomplex(qc_appell_F2(regression_qcomplex(half),
            regression_qcomplex(three_quarters), regression_qcomplex(five_quarters),
            regression_qcomplex(three_quarters), regression_qcomplex(five_quarters),
            regression_qcomplex(x), regression_qcomplex(y))) :
            num_appell_F2(half, three_quarters, five_quarters, three_quarters, five_quarters, x, y);
    } else if (family == 3) {
        reference = num_mul(num_pow(num_sub(NUM_ONE, y), num_neg(five_quarters)),
            num_appell_F1(half, three_quarters, five_quarters, five_halves, x, num_div(y, num_sub(y, NUM_ONE))));
        value = native ? num_create_from_qcomplex(qc_appell_F3(regression_qcomplex(half), QC_TWO,
            regression_qcomplex(three_quarters), regression_qcomplex(five_quarters),
            regression_qcomplex(five_halves), regression_qcomplex(x), regression_qcomplex(y))) :
            num_appell_F3(half, NUM_TWO, three_quarters, five_quarters, five_halves, x, y);
    } else {
        number_t upper[] = {half, three_quarters};
        number_t first = num_hypergeometric_pFq(upper, 2, &five_quarters, 1, x);
        number_t second = num_hypergeometric_pFq(upper, 2, &NUM_ONE, 1, y);
        reference = num_mul(first, second);
        number_t left = num_mul(x, num_sub(NUM_ONE, y)), right = num_mul(y, num_sub(NUM_ONE, x));
        value = native ? num_create_from_qcomplex(qc_appell_F4(regression_qcomplex(half),
            regression_qcomplex(three_quarters), regression_qcomplex(five_quarters), QC_ONE,
            regression_qcomplex(left), regression_qcomplex(right))) :
            num_appell_F4(half, three_quarters, five_quarters, NUM_ONE, left, right);
    }
    return num_div(num_sub(value, reference), reference);
}

static number_t regression_appell_family_reduction(const number_t *a)
{
    return regression_appell_family_reduction_value(a, false);
}

static number_t regression_appell_family_reduction_native(const number_t *a)
{
    return regression_appell_family_reduction_value(a, true);
}

static number_t regression_zeta_reflection(const number_t *a)
{
    /* Reflect the Hurwitz offset about one half: zeta(s, 1/2) = (2^s - 1) zeta(s).
     * This independently exercises the shifted finite sum and avoids a complex gamma reference. */
    number_t factor = num_sub(num_pow(NUM_TWO, a[0]), NUM_ONE);
    return num_sub(num_zetah(a[0], NUM_HALF), num_mul(factor, num_zeta(a[0])));
}

static number_t regression_zetap_reflection(const number_t *a)
{
    number_t power = num_pow(NUM_TWO, a[0]);
    number_t reference = num_mul(num_sub(power, NUM_ONE), num_zetap(a[0]));
    reference = num_add(reference, num_mul(num_mul(num_ln(NUM_TWO), power), num_zeta(a[0])));
    return num_sub(num_zatahp(a[0], NUM_HALF), reference);
}

static number_t regression_gammainv(const number_t *a)
{
    return num_sub(num_lgamma(num_gammainv(a[0])), num_ln(a[0]));
}

static number_t regression_chi(const number_t *a)
{
    number_t reference = num_mul(NUM_HALF, num_sub(num_polylog(a[0], a[1]), num_polylog(a[0], num_neg(a[1]))));
    return num_sub(num_legendre_chi(a[0], a[1]), reference);
}

static number_t regression_bessel_K(const number_t *a)
{
    number_t next = num_add(a[0], NUM_ONE);
    number_t sum = num_add(num_mul(num_bessel_I(a[0], a[1]), num_bessel_K(next, a[1])),
                           num_mul(num_bessel_I(next, a[1]), num_bessel_K(a[0], a[1])));
    return num_sub(sum, num_div(NUM_ONE, a[1]));
}

static number_t regression_double_zeta(const number_t *a)
{
    number_t value = num_zeta(num_create_from_double(num_to_double(a[0])));
    return num_sub(value, num_zeta(a[0]));
}

static number_t regression_double_zetap(const number_t *a)
{
    number_t value = num_zetap(num_create_from_double(num_to_double(a[0])));
    return num_sub(value, num_zetap(a[0]));
}

static number_t regression_double_zetah(const number_t *a)
{
    number_t value = num_zetah(num_create_from_double(num_to_double(a[0])),
                               num_create_from_double(num_to_double(a[1])));
    return num_sub(value, num_zetah(a[0], a[1]));
}

static number_t regression_double_zatahp(const number_t *a)
{
    number_t value = num_zatahp(num_create_from_double(num_to_double(a[0])),
                                num_create_from_double(num_to_double(a[1])));
    return num_sub(value, num_zatahp(a[0], a[1]));
}

static number_t regression_double_clausen(const number_t *a)
{
    number_t value = num_clausen2(num_create_from_double(num_to_double(a[0])));
    return num_sub(value, num_clausen2(a[0]));
}

/* A real operand reduces the guarded product to correctly rounded independent component products.
 * Exercise each input as the destination as well as a disjoint result, retaining both inputs until conversion. */
static number_t regression_guarded_scalar_product(const number_t *a)
{
    mpfr_prec_t bits = (mpfr_prec_t)num_get_default_prec_bits();
    mpc_t left, right, output, reference;
    mpc_init2(left, bits); mpc_init2(right, bits); mpc_init2(output, bits); mpc_init2(reference, bits);
    mpfr_t scratch[4];
    for (unsigned j = 0; j < 4u; ++j) mpfr_init2(scratch[j], bits);
    bool ok = true;
    for (unsigned alias = 0; ok && alias < 3u; ++alias) {
        ok = regression_mpc_input(left, a[0]) && regression_mpc_input(right, a[1]);
        if (!ok) break;
        mpc_mul(reference, left, right, MPC_RNDNN);
        mpc_ptr destination = alias == 1u ? left : alias == 2u ? right : output;
        number_mpc_mul_guarded(destination, left, right, scratch);
        ok = mpc_cmp(destination, reference) == 0;
    }
    for (unsigned j = 0; j < 4u; ++j) mpfr_clear(scratch[j]);
    mpc_clear(left); mpc_clear(right); mpc_clear(output); mpc_clear(reference);
    return ok ? NUM_ZERO : NUM_ONE;
}

typedef struct {
    const char *name;
    size_t arity;
    number_t (*check)(const number_t *arguments);
} regression_operation_t;

static const regression_operation_t regression_operations[] = {
    {"acosec", 1, regression_acosec},
    {"appell_equal", 6, regression_appell_equal},
    {"appell_family", 3, regression_appell_family},
    {"appell_family_native", 3, regression_appell_family_native},
    {"appell_family_reduction", 3, regression_appell_family_reduction},
    {"appell_family_reduction_native", 3, regression_appell_family_reduction_native},
    {"appell_family_reject", 3, regression_appell_family_reject},
    {"appell_family_terminate", 3, regression_appell_family_terminate},
    {"appell_linear", 6, regression_appell_linear},
    {"appell_log", 6, regression_appell_log},
    {"appell_nan", 6, regression_appell_nan},
    {"appell_partial_fraction", 6, regression_appell_partial_fraction},
    {"appell_product", 6, regression_appell_product},
    {"appell_quadratic", 6, regression_appell_quadratic},
    {"appell_reduced", 6, regression_appell_reduced},
    {"asec", 1, regression_asec},
    {"bessel_half", 1, regression_bessel_half},
    {"bessel_i_reference", 2, test_number_bessel_i_reference},
    {"bessel_j_reference", 2, test_number_bessel_j_reference},
    {"bessel_k", 2, regression_bessel_K},
    {"chi", 2, regression_chi},
    {"chi_limit", 2, regression_chi_limit},
    {"clausen_catalan", 1, regression_clausen_catalan},
    {"clausen_odd", 1, regression_clausen_odd},
    {"clausen_reference", 1, regression_clausen_reference},
    {"digamma", 1, regression_digamma},
    {"dilog_diagonal", 1, regression_dilog_diagonal},
    {"double_clausen", 1, regression_double_clausen},
    {"double_erfcinv", 1, regression_double_erfcinv},
    {"double_erfinv", 1, regression_double_erfinv},
    {"double_zatahp", 2, regression_double_zatahp},
    {"double_zeta", 1, regression_double_zeta},
    {"double_zetah", 2, regression_double_zetah},
    {"double_zetap", 1, regression_double_zetap},
    {"erf", 1, regression_erf},
    {"erfc", 1, regression_erfc},
    {"erfcinv", 1, regression_erfcinv},
    {"erfcinv_reference", 1, regression_erfcinv_reference},
    {"erfinv", 1, regression_erfinv},
    {"erfinv_reference", 1, regression_erfinv_reference},
    {"gamma_inverse_complex", 1, regression_gamma_inverse_complex},
    {"gamma_lower_one", 1, regression_gamma_lower_one},
    {"gamma_lower_reference", 2, regression_gamma_lower_reference},
    {"gamma_recurrence", 1, regression_gamma_recurrence},
    {"gamma_reflection", 1, regression_gamma_reflection},
    {"gamma_regularised", 2, regression_gamma_regularised},
    {"gamma_upper_one", 1, regression_gamma_upper_one},
    {"gamma_upper_recurrence", 2, regression_gamma_upper_recurrence},
    {"gammainv", 1, regression_gammainv},
    {"guarded_scalar_product", 2, regression_guarded_scalar_product},
    {"hypergeometric_binomial", 2, regression_hypergeometric_binomial},
    {"hypergeometric_exact", 1, regression_hypergeometric_exact},
    {"hypergeometric_exp", 1, regression_hypergeometric_exp},
    {"hypergeometric_terminating", 2, regression_hypergeometric_terminating},
    {"lerch_blocks_reference", 3, regression_lerch_blocks_reference},
    {"lerch_blocks_reject", 3, regression_lerch_blocks_reject},
    {"lerch_exact", 1, regression_lerch_exact},
    {"lerch_integer_exact", 1, regression_lerch_integer_exact},
    {"lerch_integer_reference", 3, regression_lerch_integer_reference},
    {"lerch_integer_reject", 3, regression_lerch_integer_reject},
    {"lerch_log", 1, regression_lerch_log},
    {"lerch_reference", 3, regression_lerch_reference},
    {"lerch_shift", 3, regression_lerch_shift},
    {"lgamma_turns", 1, regression_lgamma_turns},
    {"lommel", 1, regression_lommel},
    {"lommel_recurrence", 3, regression_lommel_recurrence},
    {"lommel_reference", 4, test_number_lommel_reference},
    {"normal_cdf", 1, regression_normal_cdf},
    {"normal_logpdf", 1, regression_normal_logpdf},
    {"normal_pdf", 1, regression_normal_pdf},
    {"normal_tail", 1, regression_normal_tail},
    {"pfq_0f0_reference", 1, regression_pfq_0f0_reference},
    {"pfq_0f1_reference", 2, regression_pfq_0f1_reference},
    {"pfq_1f0_reference", 2, regression_pfq_1f0_reference},
    {"pfq_1f1_reference", 3, regression_pfq_1f1_reference},
    {"pfq_2f1_reference", 4, regression_pfq_2f1_reference},
    {"pfq_2f2_reference", 5, regression_pfq_2f2_reference},
    {"pfq_3f2_reference", 6, regression_pfq_3f2_reference},
    {"pfq_pole", 3, regression_pfq_pole},
    {"polygamma", 1, regression_polygamma},
    {"polygamma_blocks", 3, regression_polygamma_blocks},
    {"polygamma_blocks_reject", 3, regression_polygamma_blocks_reject},
    {"polygamma_half", 1, regression_polygamma_half},
    {"polygamma_value", 1, regression_polygamma_value},
    {"polylog_half", 1, regression_polylog_half},
    {"polylog_limit", 2, regression_polylog_limit},
    {"polylog_reference", 2, regression_polylog_reference},
    {"qc_bessel_half", 1, regression_qc_bessel_half},
    {"qc_erf", 1, regression_qc_erf},
    {"qc_erfcinv", 1, regression_qc_erfcinv},
    {"qc_lommel", 1, regression_qc_lommel},
    {"qf_normal_tail", 1, regression_qf_normal_tail},
    {"struve_h_reference", 2, test_number_struve_h_reference},
    {"struve_l_reference", 2, test_number_struve_l_reference},
    {"tetragamma", 1, regression_tetragamma},
    {"tetragamma_reflection", 1, regression_tetragamma_reflection},
    {"trigamma", 1, regression_trigamma},
    {"trigamma_reflection", 1, regression_trigamma_reflection},
    {"two_lower_alias", 4, test_number_two_lower_alias},
    {"two_lower_reject", 4, test_number_two_lower_reject},
    {"zatahp", 2, regression_zatahp},
    {"zatahp_reference", 2, regression_zatahp_reference},
    {"zatahp_zero", 1, regression_zatahp_zero},
    {"zeta", 1, regression_zeta},
    {"zeta_blocks", 3, regression_zeta_blocks},
    {"zeta_blocks_reject", 3, regression_zeta_blocks_reject},
    {"zeta_pair", 1, regression_zeta_pair},
    {"zeta_reference", 1, regression_zeta_reference},
    {"zeta_reflection", 1, regression_zeta_reflection},
    {"zeta_tail_budget", 2, regression_zeta_tail_budget},
    {"zeta_tail_reference", 2, regression_zeta_tail_reference},
    {"zeta_tail_reject", 2, regression_zeta_tail_reject},
    {"zetah", 2, regression_zetah},
    {"zetah_negative_one", 1, regression_zetah_negative_one},
    {"zetah_reference", 2, regression_zetah_reference},
    {"zetah_value", 2, regression_zetah_value},
    {"zetap", 1, regression_zetap},
    {"zetap_reflection", 1, regression_zetap_reflection}
};

static int regression_compare(const void *key, const void *entry)
{
    return strcmp(key, ((const regression_operation_t *)entry)->name);
}

static void regression_row(const void *data)
{
    const test_database_case_t *row = data;
    const regression_operation_t *operation = bsearch(string_c_str(row->operation), regression_operations,
        sizeof regression_operations / sizeof *regression_operations, sizeof *regression_operations, regression_compare);
    size_t saved = num_get_default_prec_bits(), count = 0;
    num_set_default_prec_digits(row->precision);
    NUM_SCOPE(scope);
    string_t **inputs = string_split(row->input, ";", &count);
    number_t arguments[6] = {0};
    bool ok = operation && inputs && count == operation->arity;
    for (size_t i = 0; ok && i < count; ++i) {
        arguments[i] = num_create_from_text(inputs[i]);
        ok = !num_set_prec_digits(arguments + i, row->precision) && num_is_finite(arguments[i]);
    }
    number_t result = ok ? operation->check(arguments) : NUM_NAN;
    number_t expected = num_create_from_text(row->expected);
    number_t error = num_abs(num_sub(result, expected));
    string_t *bound = string_sprintf("1e-%u", row->precision > 6 ? row->precision - 6 : 1);
    number_t tolerance = num_create_from_text(bound);
    /* Non-zero references, including tiny erfc probabilities, require relative accuracy. */
    number_t scale = num_abs(expected);
    if (num_is_zero(scale)) scale = NUM_ONE;
    ok = ok && num_is_finite(error) && num_lt(error, num_mul(tolerance, scale));
    if (!ok) printf("%s: absolute error %.6g at %u digits\n", string_c_str(row->name),
                     num_to_double(error), row->precision);
    string_free(bound);
    string_split_free(inputs, count);
    num_set_default_prec_bits(saved);
    TEST_ASSERT_TRUE(ok, "special function satisfies its independent reference or defining identity");
}

/* Run SQL-authored accuracy regressions at fixed and multiprecision working sizes. */
void run_number_benchmark_regressions(void)
{
    /* Validate the sorted test registry once, before any binary-search dispatch can hide an authored case. */
    bool sorted = true;
    for (size_t j = 1; j < sizeof regression_operations / sizeof *regression_operations; ++j) {
        if (strcmp(regression_operations[j - 1].name, regression_operations[j].name) >= 0) {
            sorted = false;
            break;
        }
    }
    TEST_ASSERT_TRUE(sorted, "special-function test operations remain strictly sorted and unique");
    if (!sorted) return;
    test_run_database_cases(__FILE__, __LINE__, "number.benchmark_regressions",
        "tests.run_number_benchmark_regressions", regression_row);
}
