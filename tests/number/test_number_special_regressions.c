/**
 * @file test_number_special_regressions.c
 * @brief Database-driven accuracy checks for benchmark special-function regressions.
 *
 * Checks defining identities and independent error-function and zeta references, rather than accepting any finite
 * answer. SQL supplies arguments and decimal precision; the C runner retains the executable mathematics. The
 * Maclaurin reference differs from the production Kummer series and uses extra working precision.
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

static number_t regression_erf_reference(number_t input)
{
    mpfr_prec_t precision = (mpfr_prec_t)num_get_default_prec_bits() + 256;
    mpc_t z, square, term, sum, addend;
    mpfr_t pi, size;
    mpc_init2(z, precision); mpc_init2(square, precision); mpc_init2(term, precision);
    mpc_init2(sum, precision); mpc_init2(addend, precision);
    mpfr_inits2(precision, pi, size, (mpfr_ptr)0);
    /* Reference inputs are exactly representable binary fractions. */
    mpc_set_d_d(z, num_to_double(num_real_part(input)), num_to_double(num_imag_part(input)), MPC_RNDNN);
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
    mpfr_free_str(text);
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
static number_t regression_qc_erfcinv(const number_t *a)
{
    return num_create_from_qcomplex(qc_erfc(qc_erfcinv(regression_qcomplex(a[0]))));
}

static number_t regression_bessel_half(const number_t *a)
{
    number_t reference = num_mul(num_sqrt(num_div(NUM_TWO, num_mul(num_const(NUM_PI), a[0]))), num_sin(a[0]));
    return num_sub(num_bessel_j(NUM_HALF, a[0]), reference);
}

static number_t regression_qc_bessel_half(const number_t *a)
{
    number_t reference = num_mul(num_sqrt(num_div(NUM_TWO, num_mul(num_const(NUM_PI), a[0]))), num_sin(a[0]));
    return num_sub(num_create_from_qcomplex(qc_bessel_j(qc_make(QF_HALF, QF_ZERO), regression_qcomplex(a[0]))), reference);
}

static number_t regression_lommel(const number_t *a)
{
    return num_sub(num_lommel_s(NUM_ONE, NUM_ZERO, a[0]), num_sub(NUM_ONE, num_bessel_j(NUM_ZERO, a[0])));
}

static number_t regression_qc_lommel(const number_t *a)
{
    number_t value = num_create_from_qcomplex(qc_lommel_s(QC_ONE, QC_ZERO, regression_qcomplex(a[0])));
    return num_sub(value, num_sub(NUM_ONE, num_bessel_j(NUM_ZERO, a[0])));
}

static number_t regression_lommel_recurrence(const number_t *a)
{
    number_t factor = num_sub(num_sqr(num_add(a[0], NUM_ONE)), num_sqr(a[1]));
    number_t sum = num_add(num_lommel_s(num_add(a[0], NUM_TWO), a[1], a[2]),
                          num_mul(factor, num_lommel_s(a[0], a[1], a[2])));
    return num_sub(sum, num_pow(a[2], num_add(a[0], NUM_ONE)));
}

static number_t regression_appell_product(const number_t *a)
{
    number_t reference = num_mul(num_pow(num_sub(NUM_ONE, a[4]), num_neg(a[1])),
                                 num_pow(num_sub(NUM_ONE, a[5]), num_neg(a[2])));
    return num_sub(num_appell_f1(a[0], a[1], a[2], a[3], a[4], a[5]), reference);
}

static number_t regression_appell_log(const number_t *a)
{
    number_t reference = num_div(num_sub(num_ln(num_sub(NUM_ONE, a[5])), num_ln(num_sub(NUM_ONE, a[4]))),
                                 num_sub(a[4], a[5]));
    return num_sub(num_appell_f1(a[0], a[1], a[2], a[3], a[4], a[5]), reference);
}

static number_t regression_appell_linear(const number_t *a)
{
    number_t linear = num_add(num_mul(a[1], a[4]), num_mul(a[2], a[5]));
    number_t reference = num_sub(NUM_ONE, num_div(linear, a[3]));
    return num_sub(num_appell_f1(a[0], a[1], a[2], a[3], a[4], a[5]), reference);
}

static number_t regression_appell_quadratic(const number_t *a)
{
    number_t linear = num_add(num_mul(a[1], a[4]), num_mul(a[2], a[5]));
    number_t xx = num_mul(num_mul(a[1], num_add(a[1], NUM_ONE)), num_sqr(a[4]));
    number_t xy = num_mul(NUM_TWO, num_mul(num_mul(a[1], a[2]), num_mul(a[4], a[5])));
    number_t yy = num_mul(num_mul(a[2], num_add(a[2], NUM_ONE)), num_sqr(a[5]));
    number_t reference = num_sub(NUM_ONE, num_div(num_mul(NUM_TWO, linear), a[3]));
    reference = num_add(reference, num_div(num_add(num_add(xx, xy), yy), num_mul(a[3], num_add(a[3], NUM_ONE))));
    return num_sub(num_appell_f1(a[0], a[1], a[2], a[3], a[4], a[5]), reference);
}

static number_t regression_appell_nan(const number_t *a)
{
    return num_is_nan(num_appell_f1(a[0], a[1], a[2], a[3], a[4], a[5])) ? NUM_ZERO : NUM_NAN;
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

static number_t regression_zeta(const number_t *a)
{
    return num_sub(num_zeta(a[0]), num_div(num_sqr(num_const(NUM_PI)), num_create_from_long(6)));
}

static number_t regression_zeta_reference(const number_t *a)
{
    /* SQL inputs are binary fractions, represented exactly by this independent MPFR input. */
    mpfr_t reference;
    mpfr_init2(reference, (mpfr_prec_t)num_get_default_prec_bits() + 128);
    mpfr_set_d(reference, num_to_double(a[0]), MPFR_RNDN);
    mpfr_zeta(reference, reference, MPFR_RNDN);
    char *text = NULL;
    mpfr_asprintf(&text, "%.*Rg", (int)(mpfr_get_prec(reference) * 0.302) + 4, reference);
    number_t expected = text ? num_create_from_string(text) : NUM_NAN;
    number_t scale = num_abs(expected);
    if (num_lt(scale, NUM_ONE)) scale = NUM_ONE;
    number_t error = num_div(num_sub(num_zeta(a[0]), expected), scale);
    mpfr_free_str(text);
    mpfr_clear(reference);
    return error;
}

static number_t regression_dilog_diagonal(const number_t *a)
{
    number_t sum = num_add(num_dilog(a[0]), num_dilog(num_conj(a[0])));
    number_t log_two = num_ln(NUM_TWO);
    number_t reference = num_sub(num_mul(num_create_from_frac(5, 48), num_sqr(num_const(NUM_PI))),
                                 num_div(num_sqr(log_two), num_create_from_long(4)));
    return num_sub(sum, reference);
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
    mpfr_free_str(text);
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

static number_t regression_bessel_k(const number_t *a)
{
    number_t next = num_add(a[0], NUM_ONE);
    number_t sum = num_add(num_mul(num_bessel_i(a[0], a[1]), num_bessel_k(next, a[1])),
                           num_mul(num_bessel_i(next, a[1]), num_bessel_k(a[0], a[1])));
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

typedef struct {
    const char *name;
    size_t arity;
    number_t (*check)(const number_t *arguments);
} regression_operation_t;

static const regression_operation_t regression_operations[] = {
    {"appell_linear", 6, regression_appell_linear},
    {"appell_log", 6, regression_appell_log},
    {"appell_nan", 6, regression_appell_nan},
    {"appell_product", 6, regression_appell_product},
    {"appell_quadratic", 6, regression_appell_quadratic},
    {"bessel_half", 1, regression_bessel_half},
    {"bessel_k", 2, regression_bessel_k},
    {"chi", 2, regression_chi},
    {"chi_limit", 2, regression_chi_limit},
    {"clausen_catalan", 1, regression_clausen_catalan},
    {"clausen_odd", 1, regression_clausen_odd},
    {"digamma", 1, regression_digamma},
    {"dilog_diagonal", 1, regression_dilog_diagonal},
    {"double_clausen", 1, regression_double_clausen},
    {"double_zatahp", 2, regression_double_zatahp},
    {"double_zeta", 1, regression_double_zeta},
    {"double_zetah", 2, regression_double_zetah},
    {"double_zetap", 1, regression_double_zetap},
    {"erf", 1, regression_erf},
    {"erfc", 1, regression_erfc},
    {"erfcinv", 1, regression_erfcinv},
    {"erfinv", 1, regression_erfinv},
    {"gammainv", 1, regression_gammainv},
    {"lommel", 1, regression_lommel},
    {"lommel_recurrence", 3, regression_lommel_recurrence},
    {"normal_cdf", 1, regression_normal_cdf},
    {"normal_logpdf", 1, regression_normal_logpdf},
    {"normal_pdf", 1, regression_normal_pdf},
    {"normal_tail", 1, regression_normal_tail},
    {"polygamma", 1, regression_polygamma},
    {"polygamma_half", 1, regression_polygamma_half},
    {"polygamma_value", 1, regression_polygamma_value},
    {"polylog_half", 1, regression_polylog_half},
    {"polylog_limit", 2, regression_polylog_limit},
    {"qc_bessel_half", 1, regression_qc_bessel_half},
    {"qc_erf", 1, regression_qc_erf},
    {"qc_erfcinv", 1, regression_qc_erfcinv},
    {"qc_lommel", 1, regression_qc_lommel},
    {"qf_normal_tail", 1, regression_qf_normal_tail},
    {"tetragamma", 1, regression_tetragamma},
    {"trigamma", 1, regression_trigamma},
    {"zatahp", 2, regression_zatahp},
    {"zatahp_zero", 1, regression_zatahp_zero},
    {"zeta", 1, regression_zeta},
    {"zeta_reference", 1, regression_zeta_reference},
    {"zeta_reflection", 1, regression_zeta_reflection},
    {"zetah", 2, regression_zetah},
    {"zetah_negative_one", 1, regression_zetah_negative_one},
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
    test_run_database_cases(__FILE__, __LINE__, "number.benchmark_regressions",
        "tests.run_number_benchmark_regressions", regression_row);
}
