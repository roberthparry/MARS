#include <complex.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>

#include "number.h"
#include "number_internal.h"
#include "ustring.h"

enum {
    NUMBER_LAMBERT_W_HALLEY_STEPS = 12,
    NUMBER_BERNOULLI_EVEN_TERM_COUNT = 260,
    NUMBER_BERNOULLI_WORK_COUNT = (2 * NUMBER_BERNOULLI_EVEN_TERM_COUNT) + 1,
    NUMBER_POLYGAMMA_GUARD_BITS = 128,
    NUMBER_POLYGAMMA_TERM_GUARD_BITS = 16
};

static mpq_t number_bernoulli_even_terms[NUMBER_BERNOULLI_EVEN_TERM_COUNT];
static bool number_bernoulli_even_term_ready[NUMBER_BERNOULLI_EVEN_TERM_COUNT];
static bool number_bernoulli_even_terms_initialised;
static mpq_t *number_bernoulli_work;
static size_t number_bernoulli_next_degree;

static int number_bernoulli_prepare(void)
{
    if (number_bernoulli_even_terms_initialised)
        return 0;
    number_bernoulli_work = calloc(NUMBER_BERNOULLI_WORK_COUNT,
                                   sizeof(*number_bernoulli_work));
    if (!number_bernoulli_work)
        return -1;
    for (size_t i = 0u; i < NUMBER_BERNOULLI_EVEN_TERM_COUNT; ++i)
        mpq_init(number_bernoulli_even_terms[i]);
    for (size_t i = 0u; i < NUMBER_BERNOULLI_WORK_COUNT; ++i)
        mpq_init(number_bernoulli_work[i]);
    number_bernoulli_next_degree = 0u;
    number_bernoulli_even_terms_initialised = true;
    return 0;
}

static void __attribute__((destructor)) number_shutdown(void)
{
    if (number_bernoulli_even_terms_initialised) {
        for (size_t i = 0u; i < NUMBER_BERNOULLI_EVEN_TERM_COUNT; ++i)
            mpq_clear(number_bernoulli_even_terms[i]);
        if (number_bernoulli_work) {
            for (size_t i = 0u; i < NUMBER_BERNOULLI_WORK_COUNT; ++i)
                mpq_clear(number_bernoulli_work[i]);
            free(number_bernoulli_work);
        }
    }
    number_constants_shutdown();
    mpfr_free_cache();
}

static bool number_cursor_peek_ascii_digit(const string_cursor_t *cursor,
                                           unsigned char *out)
{
    unsigned char ch = 0u;

    if (!string_cursor_peek_ascii(cursor, &ch) || ch < '0' || ch > '9')
        return false;
    if (out)
        *out = ch;
    return true;
}

static bool number_text_to_int(const string_t *text, int *out)
{
    string_cursor_t *cursor;
    unsigned long value = 0u;
    unsigned long limit = (unsigned long)INT_MAX;
    unsigned char ch = 0u;
    bool negative = false;
    bool have_digit = false;
    bool ok = false;

    if (!text || !out)
        return false;

    cursor = string_cursor_new(text);
    if (!cursor)
        return false;

    if (string_cursor_peek_ascii(cursor, &ch) && (ch == '+' || ch == '-')) {
        negative = (ch == '-');
        limit = negative ? (unsigned long)INT_MAX + 1ul
                         : (unsigned long)INT_MAX;
        if (string_cursor_next(cursor) != 0)
            goto done;
    }

    while (number_cursor_peek_ascii_digit(cursor, &ch)) {
        unsigned long digit = (unsigned long)(ch - '0');

        have_digit = true;
        if (value > (limit - digit) / 10ul)
            goto done;
        value = value * 10ul + digit;
        if (string_cursor_next(cursor) != 0)
            goto done;
    }

    if (!have_digit || !string_cursor_done(cursor))
        goto done;

    if (negative) {
        *out = value == (unsigned long)INT_MAX + 1ul
            ? INT_MIN
            : -(int)value;
    } else {
        *out = (int)value;
    }
    ok = true;

done:
    string_cursor_free(cursor);
    return ok;
}

static int number_bernoulli_even_ensure(size_t index)
{
    mpq_t scale;
    size_t target_degree;

    if (index == 0u || index > NUMBER_BERNOULLI_EVEN_TERM_COUNT)
        return -1;
    if (number_bernoulli_even_term_ready[index - 1u])
        return 0;
    if (number_bernoulli_prepare() != 0)
        return -1;

    target_degree = index * 2u;
    mpq_init(scale);
    for (size_t i = number_bernoulli_next_degree; i <= target_degree; ++i) {
        mpq_set_ui(number_bernoulli_work[i], 1u, (unsigned long)i + 1u);
        for (size_t j = i; j >= 1u; --j) {
            mpq_sub(number_bernoulli_work[j - 1u],
                    number_bernoulli_work[j - 1u],
                    number_bernoulli_work[j]);
            mpq_set_ui(scale, (unsigned long)j, 1u);
            mpq_mul(number_bernoulli_work[j - 1u],
                    number_bernoulli_work[j - 1u],
                    scale);
        }
        if (i != 0u && (i % 2u) == 0u) {
            size_t term_index = (i / 2u) - 1u;

            mpq_set(number_bernoulli_even_terms[term_index],
                    number_bernoulli_work[0]);
            number_bernoulli_even_term_ready[term_index] = true;
        }
        number_bernoulli_next_degree = i + 1u;
    }
    mpq_clear(scale);
    return 0;
}

typedef qfloat_t (*number_qfloat_unary_fn)(qfloat_t);
typedef qcomplex_t (*number_qcomplex_unary_fn)(qcomplex_t);
typedef double _Complex (*number_cdouble_unary_fn)(double _Complex);
typedef int (*number_mpfr_unary_mut_fn)(mpfr_t);
typedef int (*number_mpc_complex_unary_mut_fn)(mpc_ptr, mpc_srcptr, mpc_rnd_t);
typedef double (*number_double_unary_fn)(double);
typedef qfloat_t (*number_qfloat_binary_fn)(qfloat_t, qfloat_t);
typedef qcomplex_t (*number_qcomplex_binary_fn)(qcomplex_t, qcomplex_t);
typedef double _Complex (*number_cdouble_binary_fn)(double _Complex,
                                                    double _Complex);
typedef int (*number_mpfr_binary_mut_fn)(mpfr_t, const mpfr_t);
typedef int (*number_mpc_complex_binary_mut_fn)(mpc_ptr, mpc_srcptr, mpc_srcptr, mpc_rnd_t);
typedef double (*number_double_binary_fn)(double, double);
typedef qfloat_t (*number_qfloat_ternary_fn)(qfloat_t, qfloat_t, qfloat_t);
typedef qcomplex_t (*number_qcomplex_ternary_fn)(qcomplex_t, qcomplex_t, qcomplex_t);
typedef int (*number_mpfr_ternary_mut_fn)(mpfr_t, const mpfr_t, const mpfr_t);
typedef int (*number_mpc_complex_ternary_mut_fn)(mpc_ptr, mpc_srcptr, mpc_srcptr, mpc_srcptr, mpc_rnd_t);
typedef double (*number_double_ternary_fn)(double, double, double);

typedef struct {
    number_qfloat_unary_fn qreal;
    number_qcomplex_unary_fn qcomplex;
    number_mpfr_unary_mut_fn mpfr;
    number_mpc_complex_unary_mut_fn mpc_complex;
} number_unary_math_ops_t;

typedef struct {
    number_qfloat_binary_fn qreal;
    number_qcomplex_binary_fn qcomplex;
    number_mpfr_binary_mut_fn mpfr;
    number_mpc_complex_binary_mut_fn mpc_complex;
} number_binary_math_ops_t;

typedef struct {
    number_qfloat_ternary_fn qreal;
    number_qcomplex_ternary_fn qcomplex;
    number_mpfr_ternary_mut_fn mpfr;
    number_mpc_complex_ternary_mut_fn mpc_complex;
} number_ternary_math_ops_t;

static int number_mpfr_apply_unary(mpfr_t value,
                                   int (*op)(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t))
{
    if (!op)
        return -1;
    op(value, value, MPFR_RNDN);
    return 0;
}

static int number_mpfr_apply_binary(mpfr_t value, const mpfr_t other,
                                    int (*op)(mpfr_ptr, mpfr_srcptr,
                                              mpfr_srcptr, mpfr_rnd_t))
{
    if (!op)
        return -1;
    op(value, value, other, MPFR_RNDN);
    return 0;
}

#define NUMBER_MPFR_UNARY(name, op) \
    static int name(mpfr_t value) { return number_mpfr_apply_unary(value, (op)); }

#define NUMBER_MPFR_BINARY(name, op) \
    static int name(mpfr_t value, const mpfr_t other) { return number_mpfr_apply_binary(value, other, (op)); }

NUMBER_MPFR_UNARY(number_mpfr_log10_mut, mpfr_log10)
NUMBER_MPFR_UNARY(number_mpfr_sin_mut, mpfr_sin)
NUMBER_MPFR_UNARY(number_mpfr_cos_mut, mpfr_cos)
NUMBER_MPFR_UNARY(number_mpfr_tan_mut, mpfr_tan)
NUMBER_MPFR_UNARY(number_mpfr_atan_mut, mpfr_atan)
NUMBER_MPFR_UNARY(number_mpfr_asin_mut, mpfr_asin)
NUMBER_MPFR_UNARY(number_mpfr_acos_mut, mpfr_acos)
NUMBER_MPFR_UNARY(number_mpfr_sinh_mut, mpfr_sinh)
NUMBER_MPFR_UNARY(number_mpfr_cosh_mut, mpfr_cosh)
NUMBER_MPFR_UNARY(number_mpfr_tanh_mut, mpfr_tanh)
NUMBER_MPFR_UNARY(number_mpfr_asinh_mut, mpfr_asinh)
NUMBER_MPFR_UNARY(number_mpfr_acosh_mut, mpfr_acosh)
NUMBER_MPFR_UNARY(number_mpfr_atanh_mut, mpfr_atanh)
NUMBER_MPFR_UNARY(number_mpfr_gamma_mut, mpfr_gamma)
NUMBER_MPFR_UNARY(number_mpfr_lgamma_mut, mpfr_lngamma)
NUMBER_MPFR_UNARY(number_mpfr_digamma_mut, mpfr_digamma)
NUMBER_MPFR_UNARY(number_mpfr_erf_mut, mpfr_erf)
NUMBER_MPFR_UNARY(number_mpfr_erfc_mut, mpfr_erfc)
NUMBER_MPFR_UNARY(number_mpfr_ei_mut, mpfr_eint)

NUMBER_MPFR_BINARY(number_mpfr_atan2_mut, mpfr_atan2)
NUMBER_MPFR_BINARY(number_mpfr_pow_mut, mpfr_pow)
NUMBER_MPFR_BINARY(number_mpfr_hypot_mut, mpfr_hypot)
NUMBER_MPFR_BINARY(number_mpfr_beta_mut, mpfr_beta)

static int number_mpfr_recip_after_unary(mpfr_t value,
                                         int (*op)(mpfr_ptr, mpfr_srcptr,
                                                   mpfr_rnd_t))
{
    if (!op)
        return -1;
    op(value, value, MPFR_RNDN);
    mpfr_ui_div(value, 1u, value, MPFR_RNDN);
    return 0;
}

static int number_mpfr_unary_after_recip(mpfr_t value,
                                         int (*op)(mpfr_ptr, mpfr_srcptr,
                                                   mpfr_rnd_t))
{
    if (!op)
        return -1;
    if (mpfr_zero_p(value)) {
        mpfr_set_nan(value);
        return 0;
    }
    mpfr_ui_div(value, 1u, value, MPFR_RNDN);
    op(value, value, MPFR_RNDN);
    return 0;
}

static int number_mpfr_sec_mut(mpfr_t value)
{
    return number_mpfr_recip_after_unary(value, mpfr_cos);
}

static int number_mpfr_cosec_mut(mpfr_t value)
{
    return number_mpfr_recip_after_unary(value, mpfr_sin);
}

static int number_mpfr_cot_mut(mpfr_t value)
{
    mpfr_t cos_value;

    mpfr_init2(cos_value, mpfr_get_prec(value));
    mpfr_cos(cos_value, value, MPFR_RNDN);
    mpfr_sin(value, value, MPFR_RNDN);
    mpfr_div(value, cos_value, value, MPFR_RNDN);
    mpfr_clear(cos_value);
    return 0;
}

static int number_mpfr_sech_mut(mpfr_t value)
{
    return number_mpfr_recip_after_unary(value, mpfr_cosh);
}

static int number_mpfr_cosech_mut(mpfr_t value)
{
    return number_mpfr_recip_after_unary(value, mpfr_sinh);
}

static int number_mpfr_coth_mut(mpfr_t value)
{
    mpfr_t cosh_value;

    mpfr_init2(cosh_value, mpfr_get_prec(value));
    mpfr_cosh(cosh_value, value, MPFR_RNDN);
    mpfr_sinh(value, value, MPFR_RNDN);
    mpfr_div(value, cosh_value, value, MPFR_RNDN);
    mpfr_clear(cosh_value);
    return 0;
}

static int number_mpfr_asec_mut(mpfr_t value)
{
    return number_mpfr_unary_after_recip(value, mpfr_acos);
}

static int number_mpfr_acosec_mut(mpfr_t value)
{
    return number_mpfr_unary_after_recip(value, mpfr_asin);
}

static int number_mpfr_acot_mut(mpfr_t value)
{
    mpfr_t one;

    mpfr_init2(one, mpfr_get_prec(value));
    mpfr_set_ui(one, 1u, MPFR_RNDN);
    mpfr_atan2(value, one, value, MPFR_RNDN);
    mpfr_clear(one);
    return 0;
}

static int number_mpfr_asech_mut(mpfr_t value)
{
    return number_mpfr_unary_after_recip(value, mpfr_acosh);
}

static int number_mpfr_acosech_mut(mpfr_t value)
{
    return number_mpfr_unary_after_recip(value, mpfr_asinh);
}

static int number_mpfr_acoth_mut(mpfr_t value)
{
    return number_mpfr_unary_after_recip(value, mpfr_atanh);
}

static int number_mpfr_sqr_mut(mpfr_t value)
{
    mpfr_mul(value, value, value, MPFR_RNDN);
    return 0;
}

static int number_mpfr_floor_mut(mpfr_t value)
{
    mpfr_floor(value, value);
    return 0;
}

static int number_mpfr_logbeta_mut(mpfr_t value, const mpfr_t other)
{
    mpfr_beta(value, value, other, MPFR_RNDN);
    mpfr_log(value, value, MPFR_RNDN);
    return 0;
}

static int number_mpfr_binomial_mut(mpfr_t value, const mpfr_t other)
{
    mpfr_t a, b, t;
    mpfr_prec_t prec = mpfr_get_prec(value);

    if (mpfr_get_prec(other) > prec)
        prec = mpfr_get_prec(other);
    mpfr_inits2(prec, a, b, t, (mpfr_ptr)0);
    mpfr_set(a, value, MPFR_RNDN);
    mpfr_set(b, other, MPFR_RNDN);
    mpfr_add_ui(a, a, 1u, MPFR_RNDN);
    mpfr_lngamma(value, a, MPFR_RNDN);
    mpfr_add_ui(b, b, 1u, MPFR_RNDN);
    mpfr_lngamma(t, b, MPFR_RNDN);
    mpfr_sub(value, value, t, MPFR_RNDN);
    mpfr_sub(t, a, b, MPFR_RNDN);
    mpfr_add_ui(t, t, 1u, MPFR_RNDN);
    mpfr_lngamma(t, t, MPFR_RNDN);
    mpfr_sub(value, value, t, MPFR_RNDN);
    mpfr_exp(value, value, MPFR_RNDN);
    mpfr_clears(a, b, t, (mpfr_ptr)0);
    return 0;
}

static int number_mpfr_normal_pdf_mut(mpfr_t value)
{
    mpfr_t scale;

    mpfr_init2(scale, mpfr_get_prec(value));
    mpfr_const_pi(scale, MPFR_RNDN);
    mpfr_mul_ui(scale, scale, 2u, MPFR_RNDN);
    mpfr_sqrt(scale, scale, MPFR_RNDN);
    mpfr_ui_div(scale, 1u, scale, MPFR_RNDN);
    mpfr_mul(value, value, value, MPFR_RNDN);
    mpfr_div_2ui(value, value, 1u, MPFR_RNDN);
    mpfr_neg(value, value, MPFR_RNDN);
    mpfr_exp(value, value, MPFR_RNDN);
    mpfr_mul(value, value, scale, MPFR_RNDN);
    mpfr_clear(scale);
    return 0;
}

static int number_mpfr_normal_cdf_mut(mpfr_t value)
{
    mpfr_t sqrt2;

    mpfr_init2(sqrt2, mpfr_get_prec(value));
    mpfr_set_ui(sqrt2, 2u, MPFR_RNDN);
    mpfr_sqrt(sqrt2, sqrt2, MPFR_RNDN);
    mpfr_div(value, value, sqrt2, MPFR_RNDN);
    mpfr_erf(value, value, MPFR_RNDN);
    mpfr_add_ui(value, value, 1u, MPFR_RNDN);
    mpfr_div_2ui(value, value, 1u, MPFR_RNDN);
    mpfr_clear(sqrt2);
    return 0;
}

static int number_mpfr_normal_logpdf_mut(mpfr_t value)
{
    mpfr_t log_scale;

    mpfr_init2(log_scale, mpfr_get_prec(value));
    mpfr_const_pi(log_scale, MPFR_RNDN);
    mpfr_mul_ui(log_scale, log_scale, 2u, MPFR_RNDN);
    mpfr_log(log_scale, log_scale, MPFR_RNDN);
    mpfr_div_2ui(log_scale, log_scale, 1u, MPFR_RNDN);
    mpfr_mul(value, value, value, MPFR_RNDN);
    mpfr_div_2ui(value, value, 1u, MPFR_RNDN);
    mpfr_neg(value, value, MPFR_RNDN);
    mpfr_sub(value, value, log_scale, MPFR_RNDN);
    mpfr_clear(log_scale);
    return 0;
}

static double number_double_logbeta(double a, double b)
{
    return lgamma(a) + lgamma(b) - lgamma(a + b);
}

static double number_double_beta(double a, double b)
{
    return exp(number_double_logbeta(a, b));
}

static double number_double_binomial(double n, double k)
{
    return exp(lgamma(n + 1.0) - lgamma(k + 1.0) - lgamma(n - k + 1.0));
}

static double number_double_beta_pdf(double x, double a, double b)
{
    return exp((a - 1.0) * log(x) + (b - 1.0) * log1p(-x) -
               number_double_logbeta(a, b));
}

static double number_double_logbeta_pdf(double x, double a, double b)
{
    return (a - 1.0) * log(x) + (b - 1.0) * log1p(-x) -
           number_double_logbeta(a, b);
}

static double number_double_normal_pdf(double x)
{
    const double inv_sqrt_2pi = 0.39894228040143267794;

    return inv_sqrt_2pi * exp(-0.5 * x * x);
}

static double number_double_normal_cdf(double x)
{
    const double inv_sqrt2 = 0.70710678118654752440;

    return 0.5 * erfc(-x * inv_sqrt2);
}

static double number_double_normal_logpdf(double x)
{
    const double log_sqrt_2pi = 0.91893853320467274178;

    return -0.5 * x * x - log_sqrt_2pi;
}

static double _Complex number_cdouble_log10(double _Complex value)
{
    return clog(value) / log(10.0);
}

static double number_double_sec(double value) { return 1.0 / cos(value); }
static double number_double_cosec(double value) { return 1.0 / sin(value); }
static double number_double_cot(double value) { return cos(value) / sin(value); }
static double number_double_sech(double value) { return 1.0 / cosh(value); }
static double number_double_cosech(double value) { return 1.0 / sinh(value); }
static double number_double_coth(double value) { return cosh(value) / sinh(value); }
static double number_double_asec(double value)
{
    return value == 0.0 ? NAN : acos(1.0 / value);
}

static double number_double_acosec(double value)
{
    return value == 0.0 ? NAN : asin(1.0 / value);
}

static double number_double_acot(double value) { return atan2(1.0, value); }
static double number_double_asech(double value) { return acosh(1.0 / value); }
static double number_double_acosech(double value) { return asinh(1.0 / value); }
static double number_double_acoth(double value) { return atanh(1.0 / value); }

static bool number_cdouble_is_zero(double _Complex value)
{
    return creal(value) == 0.0 && cimag(value) == 0.0;
}

static double _Complex number_cdouble_nan(void)
{
    return NAN + NAN * I;
}

static double _Complex number_cdouble_sec(double _Complex value)
{
    return 1.0 / ccos(value);
}

static double _Complex number_cdouble_cosec(double _Complex value)
{
    return 1.0 / csin(value);
}

static double _Complex number_cdouble_cot(double _Complex value)
{
    return ccos(value) / csin(value);
}

static double _Complex number_cdouble_sech(double _Complex value)
{
    return 1.0 / ccosh(value);
}

static double _Complex number_cdouble_cosech(double _Complex value)
{
    return 1.0 / csinh(value);
}

static double _Complex number_cdouble_coth(double _Complex value)
{
    return ccosh(value) / csinh(value);
}

static double _Complex number_cdouble_asec(double _Complex value)
{
    if (number_cdouble_is_zero(value))
        return number_cdouble_nan();
    return cacos(1.0 / value);
}

static double _Complex number_cdouble_acosec(double _Complex value)
{
    if (number_cdouble_is_zero(value))
        return number_cdouble_nan();
    return casin(1.0 / value);
}

static double _Complex number_cdouble_acot(double _Complex value)
{
    return (clog(value + I) - clog(value - I)) / (2.0 * I);
}

static double _Complex number_cdouble_asech(double _Complex value)
{
    return cacosh(1.0 / value);
}

static double _Complex number_cdouble_acosech(double _Complex value)
{
    return casinh(1.0 / value);
}

static double _Complex number_cdouble_acoth(double _Complex value)
{
    return catanh(1.0 / value);
}

static int number_mpc_recip_after_unary(mpc_ptr out,
                                        mpc_srcptr in,
                                        mpc_rnd_t rnd,
                                        int (*op)(mpc_ptr, mpc_srcptr,
                                                  mpc_rnd_t))
{
    mpc_t tmp;

    if (!op)
        return -1;
    mpc_init2(tmp, mpc_get_prec(out));
    op(tmp, in, rnd);
    mpc_set_ui(out, 1u, rnd);
    mpc_div(out, out, tmp, rnd);
    mpc_clear(tmp);
    return 0;
}

static int number_mpc_unary_after_recip(mpc_ptr out,
                                        mpc_srcptr in,
                                        mpc_rnd_t rnd,
                                        int (*op)(mpc_ptr, mpc_srcptr,
                                                  mpc_rnd_t))
{
    mpc_t tmp;

    if (!op)
        return -1;
    if (mpfr_zero_p(mpc_realref(in)) && mpfr_zero_p(mpc_imagref(in))) {
        mpfr_set_nan(mpc_realref(out));
        mpfr_set_nan(mpc_imagref(out));
        return 0;
    }
    mpc_init2(tmp, mpc_get_prec(out));
    mpc_set_ui(tmp, 1u, rnd);
    mpc_div(tmp, tmp, in, rnd);
    op(out, tmp, rnd);
    mpc_clear(tmp);
    return 0;
}

static int number_mpc_sec(mpc_ptr out, mpc_srcptr in, mpc_rnd_t rnd)
{
    return number_mpc_recip_after_unary(out, in, rnd, mpc_cos);
}

static int number_mpc_cosec(mpc_ptr out, mpc_srcptr in, mpc_rnd_t rnd)
{
    return number_mpc_recip_after_unary(out, in, rnd, mpc_sin);
}

static int number_mpc_cot(mpc_ptr out, mpc_srcptr in, mpc_rnd_t rnd)
{
    mpc_t cos_value;
    mpc_t sin_value;

    mpc_init2(cos_value, mpc_get_prec(out));
    mpc_init2(sin_value, mpc_get_prec(out));
    mpc_cos(cos_value, in, rnd);
    mpc_sin(sin_value, in, rnd);
    mpc_div(out, cos_value, sin_value, rnd);
    mpc_clear(sin_value);
    mpc_clear(cos_value);
    return 0;
}

static int number_mpc_sech(mpc_ptr out, mpc_srcptr in, mpc_rnd_t rnd)
{
    return number_mpc_recip_after_unary(out, in, rnd, mpc_cosh);
}

static int number_mpc_cosech(mpc_ptr out, mpc_srcptr in, mpc_rnd_t rnd)
{
    return number_mpc_recip_after_unary(out, in, rnd, mpc_sinh);
}

static int number_mpc_coth(mpc_ptr out, mpc_srcptr in, mpc_rnd_t rnd)
{
    mpc_t cosh_value;
    mpc_t sinh_value;

    mpc_init2(cosh_value, mpc_get_prec(out));
    mpc_init2(sinh_value, mpc_get_prec(out));
    mpc_cosh(cosh_value, in, rnd);
    mpc_sinh(sinh_value, in, rnd);
    mpc_div(out, cosh_value, sinh_value, rnd);
    mpc_clear(sinh_value);
    mpc_clear(cosh_value);
    return 0;
}

static int number_mpc_asec(mpc_ptr out, mpc_srcptr in, mpc_rnd_t rnd)
{
    return number_mpc_unary_after_recip(out, in, rnd, mpc_acos);
}

static int number_mpc_acosec(mpc_ptr out, mpc_srcptr in, mpc_rnd_t rnd)
{
    return number_mpc_unary_after_recip(out, in, rnd, mpc_asin);
}

static int number_mpc_acot(mpc_ptr out, mpc_srcptr in, mpc_rnd_t rnd)
{
    mpc_t plus_i;
    mpc_t minus_i;
    mpc_t log_plus;
    mpc_t log_minus;
    mpc_t i_unit;
    mpfr_prec_t prec = mpc_get_prec(out);

    mpc_init2(plus_i, prec);
    mpc_init2(minus_i, prec);
    mpc_init2(log_plus, prec);
    mpc_init2(log_minus, prec);
    mpc_init2(i_unit, prec);
    mpc_set_ui_ui(i_unit, 0u, 1u, rnd);
    mpc_add(plus_i, in, i_unit, rnd);
    mpc_sub(minus_i, in, i_unit, rnd);
    mpc_log(log_plus, plus_i, rnd);
    mpc_log(log_minus, minus_i, rnd);
    mpc_sub(out, log_plus, log_minus, rnd);
    mpc_mul_i(out, out, -1, rnd);
    mpc_div_2ui(out, out, 1u, rnd);
    mpc_clear(i_unit);
    mpc_clear(log_minus);
    mpc_clear(log_plus);
    mpc_clear(minus_i);
    mpc_clear(plus_i);
    return 0;
}

static int number_mpc_asech(mpc_ptr out, mpc_srcptr in, mpc_rnd_t rnd)
{
    return number_mpc_unary_after_recip(out, in, rnd, mpc_acosh);
}

static int number_mpc_acosech(mpc_ptr out, mpc_srcptr in, mpc_rnd_t rnd)
{
    return number_mpc_unary_after_recip(out, in, rnd, mpc_asinh);
}

static int number_mpc_acoth(mpc_ptr out, mpc_srcptr in, mpc_rnd_t rnd)
{
    return number_mpc_unary_after_recip(out, in, rnd, mpc_atanh);
}

static number_t number_double_cdouble_unary(double value,
                                            number_cdouble_unary_fn fn)
{
    return fn ? num_create_from_cdouble(fn(value + 0.0 * I))
              : number_invalid();
}

static number_t number_double_cdouble_binary(double a,
                                             double b,
                                             number_cdouble_binary_fn fn)
{
    return fn ? num_create_from_cdouble(fn(a + 0.0 * I, b + 0.0 * I))
              : number_invalid();
}

static bool number_is_cdouble_value(const number_t *number)
{
    return number_kind_value(number) == NUMBER_CDOUBLE;
}

static double _Complex number_cdouble_math_value(const number_t *number)
{
    return number_impl_const(number)->value.cd.value;
}

static number_t number_apply_cdouble_unary(const number_t number,
                                           number_cdouble_unary_fn fn)
{
    return fn && number_is_cdouble_value(&number)
        ? num_create_from_cdouble(fn(number_cdouble_math_value(&number)))
        : number_invalid();
}

static number_t number_apply_cdouble_binary(const number_t a,
                                            const number_t b,
                                            number_cdouble_binary_fn fn)
{
    return fn && number_is_cdouble_value(&a) && number_is_cdouble_value(&b)
        ? num_create_from_cdouble(fn(number_cdouble_math_value(&a),
            number_cdouble_math_value(&b)))
        : number_invalid();
}

static number_t number_qfloat_qcomplex_unary(qfloat_t value,
                                             number_qcomplex_unary_fn fn)
{
    return fn ? num_create_from_qcomplex(fn(qc_make(value, QF_ZERO)))
              : number_invalid();
}

static number_t number_qfloat_qcomplex_binary(qfloat_t a,
                                              qfloat_t b,
                                              number_qcomplex_binary_fn fn)
{
    return fn ? num_create_from_qcomplex(fn(qc_make(a, QF_ZERO),
                                            qc_make(b, QF_ZERO)))
              : number_invalid();
}

static int number_mpfr_e1_mut(mpfr_t value)
{
    mpfr_neg(value, value, MPFR_RNDN);
    mpfr_eint(value, value, MPFR_RNDN);
    mpfr_neg(value, value, MPFR_RNDN);
    return 0;
}

static int number_mpfr_logbeta_pdf_mut(mpfr_t value,
                                       const mpfr_t a,
                                       const mpfr_t b)
{
    mpfr_t x, alpha, beta, t;
    mpfr_prec_t prec = mpfr_get_prec(value);

    if (mpfr_get_prec(a) > prec)
        prec = mpfr_get_prec(a);
    if (mpfr_get_prec(b) > prec)
        prec = mpfr_get_prec(b);
    mpfr_inits2(prec, x, alpha, beta, t, (mpfr_ptr)0);
    mpfr_set(x, value, MPFR_RNDN);
    mpfr_set(alpha, a, MPFR_RNDN);
    mpfr_set(beta, b, MPFR_RNDN);
    mpfr_sub_ui(alpha, alpha, 1u, MPFR_RNDN);
    mpfr_log(value, x, MPFR_RNDN);
    mpfr_mul(value, value, alpha, MPFR_RNDN);
    mpfr_ui_sub(t, 1u, x, MPFR_RNDN);
    mpfr_sub_ui(beta, beta, 1u, MPFR_RNDN);
    mpfr_log(t, t, MPFR_RNDN);
    mpfr_mul(t, t, beta, MPFR_RNDN);
    mpfr_add(value, value, t, MPFR_RNDN);
    mpfr_beta(t, a, b, MPFR_RNDN);
    mpfr_log(t, t, MPFR_RNDN);
    mpfr_sub(value, value, t, MPFR_RNDN);
    mpfr_clears(x, alpha, beta, t, (mpfr_ptr)0);
    return 0;
}

static int number_mpfr_beta_pdf_mut(mpfr_t value, const mpfr_t a, const mpfr_t b)
{
    if (number_mpfr_logbeta_pdf_mut(value, a, b) != 0)
        return -1;
    mpfr_exp(value, value, MPFR_RNDN);
    return 0;
}

typedef enum {
    NUMBER_MPFR_GAMMAINC_LOWER,
    NUMBER_MPFR_GAMMAINC_UPPER,
    NUMBER_MPFR_GAMMAINC_P,
    NUMBER_MPFR_GAMMAINC_Q
} number_mpfr_gammainc_mode_t;

static int number_mpfr_gammainc_mut(mpfr_t value,
                                    const mpfr_t other,
                                    number_mpfr_gammainc_mode_t mode)
{
    mpfr_t gamma_a, upper, tmp;
    mpfr_prec_t prec = mpfr_get_prec(value);

    if (mpfr_get_prec(other) > prec)
        prec = mpfr_get_prec(other);
    mpfr_inits2(prec, gamma_a, upper, tmp, (mpfr_ptr)0);
    mpfr_gamma(gamma_a, value, MPFR_RNDN);
    mpfr_gamma_inc(upper, value, other, MPFR_RNDN);
    if (mode == NUMBER_MPFR_GAMMAINC_LOWER) {
        mpfr_sub(value, gamma_a, upper, MPFR_RNDN);
    } else if (mode == NUMBER_MPFR_GAMMAINC_UPPER) {
        mpfr_set(value, upper, MPFR_RNDN);
    } else if (mode == NUMBER_MPFR_GAMMAINC_P) {
        mpfr_sub(tmp, gamma_a, upper, MPFR_RNDN);
        mpfr_div(value, tmp, gamma_a, MPFR_RNDN);
    } else {
        mpfr_div(value, upper, gamma_a, MPFR_RNDN);
    }
    mpfr_clears(gamma_a, upper, tmp, (mpfr_ptr)0);
    return 0;
}

static int number_mpfr_gammainc_lower_mut(mpfr_t value, const mpfr_t other)
{
    return number_mpfr_gammainc_mut(value, other, NUMBER_MPFR_GAMMAINC_LOWER);
}

static int number_mpfr_gammainc_upper_mut(mpfr_t value, const mpfr_t other)
{
    return number_mpfr_gammainc_mut(value, other, NUMBER_MPFR_GAMMAINC_UPPER);
}

static int number_mpfr_gammainc_p_mut(mpfr_t value, const mpfr_t other)
{
    return number_mpfr_gammainc_mut(value, other, NUMBER_MPFR_GAMMAINC_P);
}

static int number_mpfr_gammainc_q_mut(mpfr_t value, const mpfr_t other)
{
    return number_mpfr_gammainc_mut(value, other, NUMBER_MPFR_GAMMAINC_Q);
}

static int number_mpfr_erfinv_mut(mpfr_t value)
{
    mpfr_set_d(value, qf_to_double(qf_erfinv((qfloat_t){
        mpfr_get_d(value, MPFR_RNDN), 0.0
    })), MPFR_RNDN);
    return 0;
}

static int number_mpfr_erfcinv_mut(mpfr_t value)
{
    mpfr_set_d(value, qf_to_double(qf_erfcinv((qfloat_t){
        mpfr_get_d(value, MPFR_RNDN), 0.0
    })), MPFR_RNDN);
    return 0;
}

static int number_mpfr_set_bernoulli_even(mpfr_t value, size_t index)
{
    if (number_bernoulli_even_ensure(index) != 0)
        return -1;
    mpfr_set_q(value, number_bernoulli_even_terms[index - 1u], MPFR_RNDN);
    return 0;
}

static bool number_mpfr_term_below_target(mpfr_srcptr term,
                                          mpfr_prec_t target_prec)
{
    mpfr_exp_t exponent;

    if (mpfr_zero_p(term))
        return true;
    exponent = mpfr_get_exp(term);
    return exponent <= -(mpfr_exp_t)(target_prec + NUMBER_POLYGAMMA_TERM_GUARD_BITS);
}

static bool number_mpc_term_below_target(mpc_srcptr term,
                                         mpfr_prec_t target_prec)
{
    mpfr_t magnitude;
    bool small;

    mpfr_init2(magnitude, target_prec + NUMBER_POLYGAMMA_GUARD_BITS);
    mpc_abs(magnitude, term, MPFR_RNDN);
    small = number_mpfr_term_below_target(magnitude, target_prec);
    mpfr_clear(magnitude);
    return small;
}

static unsigned long number_polygamma_shift_target(mpfr_prec_t target_prec)
{
    unsigned long target = (unsigned long)target_prec;

    if (target < 64ul)
        target = 64ul;
    if (target > 4096ul)
        target = 4096ul;
    return target;
}

static int number_mpfr_trigamma_mut(mpfr_t value)
{
    size_t bernoulli_terms;
    mpfr_prec_t prec;
    mpfr_t y, inv, inv2, power, term, sum;
    size_t n;

    if (!mpfr_number_p(value) ||
        (mpfr_sgn(value) <= 0 && mpfr_integer_p(value))) {
        mpfr_set_nan(value);
        return 0;
    }

    prec = mpfr_get_prec(value) + 128;
    mpfr_inits2(prec, y, inv, inv2, power, term, sum, (mpfr_ptr)0);
    mpfr_set(y, value, MPFR_RNDN);
    mpfr_set_zero(sum, 1);

    while (mpfr_cmp_ui(y, 256u) < 0) {
        mpfr_sqr(term, y, MPFR_RNDN);
        mpfr_ui_div(term, 1u, term, MPFR_RNDN);
        mpfr_add(sum, sum, term, MPFR_RNDN);
        mpfr_add_ui(y, y, 1u, MPFR_RNDN);
    }

    mpfr_ui_div(inv, 1u, y, MPFR_RNDN);
    mpfr_sqr(inv2, inv, MPFR_RNDN);
    mpfr_add(sum, sum, inv, MPFR_RNDN);
    mpfr_div_2ui(term, inv2, 1u, MPFR_RNDN);
    mpfr_add(sum, sum, term, MPFR_RNDN);
    mpfr_mul(power, inv2, inv, MPFR_RNDN);

    bernoulli_terms = NUMBER_BERNOULLI_EVEN_TERM_COUNT;
    for (n = 1u; n <= bernoulli_terms; ++n) {
        if (number_mpfr_set_bernoulli_even(term, n) != 0) {
            mpfr_clears(y, inv, inv2, power, term, sum, (mpfr_ptr)0);
            return -1;
        }
        mpfr_mul(term, term, power, MPFR_RNDN);
        mpfr_add(sum, sum, term, MPFR_RNDN);
        mpfr_mul(power, power, inv2, MPFR_RNDN);
    }

    mpfr_set(value, sum, MPFR_RNDN);
    mpfr_clears(y, inv, inv2, power, term, sum, (mpfr_ptr)0);
    return 0;
}

static int number_mpfr_tetragamma_mut(mpfr_t value)
{
    size_t bernoulli_terms;
    mpfr_prec_t prec;
    mpfr_t y, inv, inv2, power, term, sum;
    size_t n;

    if (!mpfr_number_p(value) ||
        (mpfr_sgn(value) <= 0 && mpfr_integer_p(value))) {
        mpfr_set_nan(value);
        return 0;
    }

    prec = mpfr_get_prec(value) + 128;
    mpfr_inits2(prec, y, inv, inv2, power, term, sum, (mpfr_ptr)0);
    mpfr_set(y, value, MPFR_RNDN);
    mpfr_set_zero(sum, 1);

    while (mpfr_cmp_ui(y, 256u) < 0) {
        mpfr_sqr(term, y, MPFR_RNDN);
        mpfr_mul(term, term, y, MPFR_RNDN);
        mpfr_ui_div(term, 2u, term, MPFR_RNDN);
        mpfr_sub(sum, sum, term, MPFR_RNDN);
        mpfr_add_ui(y, y, 1u, MPFR_RNDN);
    }

    mpfr_ui_div(inv, 1u, y, MPFR_RNDN);
    mpfr_sqr(inv2, inv, MPFR_RNDN);
    mpfr_neg(term, inv2, MPFR_RNDN);
    mpfr_add(sum, sum, term, MPFR_RNDN);
    mpfr_mul(power, inv2, inv, MPFR_RNDN);
    mpfr_sub(sum, sum, power, MPFR_RNDN);
    mpfr_mul(power, power, inv, MPFR_RNDN);

    bernoulli_terms = NUMBER_BERNOULLI_EVEN_TERM_COUNT;
    for (n = 1u; n <= bernoulli_terms; ++n) {
        if (number_mpfr_set_bernoulli_even(term, n) != 0) {
            mpfr_clears(y, inv, inv2, power, term, sum, (mpfr_ptr)0);
            return -1;
        }
        mpfr_mul_ui(term, term, (unsigned long)(2 * n + 1), MPFR_RNDN);
        mpfr_mul(term, term, power, MPFR_RNDN);
        mpfr_sub(sum, sum, term, MPFR_RNDN);
        mpfr_mul(power, power, inv2, MPFR_RNDN);
    }

    mpfr_set(value, sum, MPFR_RNDN);
    mpfr_clears(y, inv, inv2, power, term, sum, (mpfr_ptr)0);
    return 0;
}

static int number_mpfr_polygamma_order_mut(mpfr_t value, unsigned int order)
{
    mpfr_prec_t target_prec;
    mpfr_prec_t prec;
    mpfr_t y, inv, inv2, power, term, sum, asymp, fact;
    unsigned long shift_target;
    size_t n;
    bool converged = false;

    if (order == 0u)
        return number_mpfr_digamma_mut(value);
    if (order == 1u)
        return number_mpfr_trigamma_mut(value);
    if (order == 2u)
        return number_mpfr_tetragamma_mut(value);
    if (!mpfr_number_p(value) ||
        (mpfr_sgn(value) <= 0 && mpfr_integer_p(value))) {
        mpfr_set_nan(value);
        return 0;
    }

    target_prec = mpfr_get_prec(value);
    prec = target_prec + NUMBER_POLYGAMMA_GUARD_BITS;
    shift_target = number_polygamma_shift_target(target_prec);
    mpfr_inits2(prec, y, inv, inv2, power, term, sum, asymp, fact, (mpfr_ptr)0);
    mpfr_set(y, value, MPFR_RNDN);
    mpfr_set_zero(sum, 1);
    mpfr_set_zero(asymp, 1);
    mpfr_fac_ui(fact, order, MPFR_RNDN);

    while (mpfr_cmp_ui(y, shift_target) < 0) {
        mpfr_pow_ui(term, y, order + 1u, MPFR_RNDN);
        mpfr_div(term, fact, term, MPFR_RNDN);
        if ((order % 2u) == 0u)
            mpfr_sub(sum, sum, term, MPFR_RNDN);
        else
            mpfr_add(sum, sum, term, MPFR_RNDN);
        mpfr_add_ui(y, y, 1u, MPFR_RNDN);
    }

    mpfr_ui_div(inv, 1u, y, MPFR_RNDN);
    mpfr_sqr(inv2, inv, MPFR_RNDN);
    mpfr_fac_ui(term, order - 1u, MPFR_RNDN);
    mpfr_pow_ui(power, inv, order, MPFR_RNDN);
    mpfr_mul(term, term, power, MPFR_RNDN);
    mpfr_add(asymp, asymp, term, MPFR_RNDN);

    mpfr_pow_ui(power, inv, order + 1u, MPFR_RNDN);
    mpfr_mul(term, fact, power, MPFR_RNDN);
    mpfr_div_2ui(term, term, 1u, MPFR_RNDN);
    mpfr_add(asymp, asymp, term, MPFR_RNDN);

    mpfr_mul(power, power, inv, MPFR_RNDN);
    for (n = 1u; n <= NUMBER_BERNOULLI_EVEN_TERM_COUNT; ++n) {
        if (number_mpfr_set_bernoulli_even(term, n) != 0) {
            mpfr_clears(y, inv, inv2, power, term, sum, asymp, fact, (mpfr_ptr)0);
            return -1;
        }
        for (unsigned int j = 1u; j < order; ++j)
            mpfr_mul_ui(term, term, (unsigned long)(2u * n + j), MPFR_RNDN);
        mpfr_mul(term, term, power, MPFR_RNDN);
        mpfr_add(asymp, asymp, term, MPFR_RNDN);
        if (number_mpfr_term_below_target(term, target_prec)) {
            converged = true;
            break;
        }
        mpfr_mul(power, power, inv2, MPFR_RNDN);
    }

    if (!converged) {
        mpfr_clears(y, inv, inv2, power, term, sum, asymp, fact, (mpfr_ptr)0);
        return -1;
    }

    if ((order % 2u) == 0u)
        mpfr_neg(asymp, asymp, MPFR_RNDN);
    mpfr_add(sum, sum, asymp, MPFR_RNDN);
    mpfr_set(value, sum, MPFR_RNDN);
    mpfr_clears(y, inv, inv2, power, term, sum, asymp, fact, (mpfr_ptr)0);
    return 0;
}

static int number_mpc_polygamma_order(mpc_ptr out,
                                      mpc_srcptr z,
                                      unsigned int order,
                                      mpc_rnd_t rnd)
{
    mpfr_prec_t target_prec;
    mpfr_prec_t work_prec;
    unsigned long shift_target;
    mpc_t y, inv, inv2, power, term, sum, asymp;
    mpfr_t fact, coeff, radius2;
    size_t n;
    bool converged = false;
    int rc = 0;

    if (order < 3u)
        return -1;

    target_prec = mpc_get_prec(out);
    work_prec = target_prec + NUMBER_POLYGAMMA_GUARD_BITS;
    shift_target = number_polygamma_shift_target(target_prec);

    mpc_init2(y, work_prec);
    mpc_init2(inv, work_prec);
    mpc_init2(inv2, work_prec);
    mpc_init2(power, work_prec);
    mpc_init2(term, work_prec);
    mpc_init2(sum, work_prec);
    mpc_init2(asymp, work_prec);
    mpfr_init2(fact, work_prec);
    mpfr_init2(coeff, work_prec);
    mpfr_init2(radius2, work_prec);

    mpc_set(y, z, MPC_RNDNN);
    mpc_set_ui(sum, 0u, MPC_RNDNN);
    mpc_set_ui(asymp, 0u, MPC_RNDNN);
    mpfr_fac_ui(fact, order, MPFR_RNDN);

    for (;;) {
        mpc_norm(radius2, y, MPFR_RNDN);
        if (mpfr_cmp_ui(radius2, shift_target * shift_target) >= 0)
            break;
        mpc_pow_ui(term, y, order + 1u, MPC_RNDNN);
        mpc_fr_div(term, fact, term, MPC_RNDNN);
        if ((order % 2u) == 0u)
            mpc_sub(sum, sum, term, MPC_RNDNN);
        else
            mpc_add(sum, sum, term, MPC_RNDNN);
        mpc_add_ui(y, y, 1u, MPC_RNDNN);
    }

    mpc_ui_div(inv, 1u, y, MPC_RNDNN);
    mpc_sqr(inv2, inv, MPC_RNDNN);

    mpfr_fac_ui(coeff, order - 1u, MPFR_RNDN);
    mpc_pow_ui(power, inv, order, MPC_RNDNN);
    mpc_mul_fr(term, power, coeff, MPC_RNDNN);
    mpc_add(asymp, asymp, term, MPC_RNDNN);

    mpc_pow_ui(power, inv, order + 1u, MPC_RNDNN);
    mpc_mul_fr(term, power, fact, MPC_RNDNN);
    mpc_div_2ui(term, term, 1u, MPC_RNDNN);
    mpc_add(asymp, asymp, term, MPC_RNDNN);

    mpc_mul(power, power, inv, MPC_RNDNN);
    for (n = 1u; n <= NUMBER_BERNOULLI_EVEN_TERM_COUNT; ++n) {
        if (number_mpfr_set_bernoulli_even(coeff, n) != 0) {
            rc = -1;
            break;
        }
        for (unsigned int j = 1u; j < order; ++j)
            mpfr_mul_ui(coeff, coeff, (unsigned long)(2u * n + j), MPFR_RNDN);
        mpc_mul_fr(term, power, coeff, MPC_RNDNN);
        mpc_add(asymp, asymp, term, MPC_RNDNN);
        if (number_mpc_term_below_target(term, target_prec)) {
            converged = true;
            break;
        }
        mpc_mul(power, power, inv2, MPC_RNDNN);
    }

    if (rc == 0 && !converged)
        rc = -1;
    if (rc == 0) {
        if ((order % 2u) == 0u)
            mpc_neg(asymp, asymp, MPC_RNDNN);
        mpc_add(sum, sum, asymp, MPC_RNDNN);
        mpc_set(out, sum, rnd);
    }

    mpfr_clear(radius2);
    mpfr_clear(coeff);
    mpfr_clear(fact);
    mpc_clear(asymp);
    mpc_clear(sum);
    mpc_clear(term);
    mpc_clear(power);
    mpc_clear(inv2);
    mpc_clear(inv);
    mpc_clear(y);
    return rc;
}

static int number_mpfr_gammainv_mut(mpfr_t value)
{
    mpfr_prec_t result_prec = mpfr_get_prec(value);
    mpfr_prec_t work_prec = result_prec + (result_prec > 256 ? result_prec / 4 : 64);
    mpfr_t y, gamma_min, log_y, x, lgamma_x, digamma_x, residual, step;
    qfloat_t qstart;
    double y_double;
    double x_start;
    int max_iterations = (int)(result_prec / 8u) + 32;

    if (!mpfr_number_p(value) || mpfr_sgn(value) <= 0) {
        mpfr_set_nan(value);
        return 0;
    }

    mpfr_inits2(work_prec, y, gamma_min, log_y, x, lgamma_x,
                digamma_x, residual, step, (mpfr_ptr)0);
    mpfr_set(y, value, MPFR_RNDN);
    mpfr_set_str(gamma_min,
                 "0.885603194410888700278815900582588733207951533669903448871200165",
                 10, MPFR_RNDN);
    if (mpfr_cmp(y, gamma_min) < 0) {
        mpfr_set_nan(value);
        mpfr_clears(y, gamma_min, log_y, x, lgamma_x,
                    digamma_x, residual, step, (mpfr_ptr)0);
        return 0;
    }

    mpfr_log(log_y, y, MPFR_RNDN);
    y_double = mpfr_get_d(y, MPFR_RNDN);
    qstart = qf_gammainv((qfloat_t){ y_double, 0.0 });
    x_start = qf_to_double(qstart);

    if (isfinite(x_start) && x_start > 0.0) {
        mpfr_set_d(x, x_start, MPFR_RNDN);
    } else if (mpfr_cmp_ui(y, 1u) <= 0) {
        mpfr_set_ui(x, 1u, MPFR_RNDN);
    } else {
        mpfr_set(x, log_y, MPFR_RNDN);
        if (mpfr_cmp_d(x, 1.5) < 0)
            mpfr_set_d(x, 1.5, MPFR_RNDN);
    }

    for (int i = 0; i < max_iterations; ++i) {
        mpfr_lngamma(lgamma_x, x, MPFR_RNDN);
        mpfr_sub(residual, lgamma_x, log_y, MPFR_RNDN);
        mpfr_digamma(digamma_x, x, MPFR_RNDN);
        if (mpfr_zero_p(digamma_x) || !mpfr_number_p(digamma_x))
            break;

        mpfr_div(step, residual, digamma_x, MPFR_RNDN);
        mpfr_sub(x, x, step, MPFR_RNDN);
        if (!mpfr_number_p(x) || mpfr_sgn(x) <= 0) {
            mpfr_set_d(x, x_start > 0.0 ? x_start : 1.5, MPFR_RNDN);
            break;
        }
        mpfr_abs(step, step, MPFR_RNDN);
        if (mpfr_zero_p(step) || mpfr_cmp_ui_2exp(step, 1u, -((long)result_prec + 8L)) <= 0)
            break;
    }

    mpfr_set(value, x, MPFR_RNDN);
    mpfr_clears(y, gamma_min, log_y, x, lgamma_x,
                digamma_x, residual, step, (mpfr_ptr)0);
    return 0;
}

static int number_mpfr_lambert_w_mut(mpfr_t value, bool branch_m1)
{
    mpfr_prec_t prec = mpfr_get_prec(value);
    mpfr_t x, w, e, f, denom, tmp;
    int i;

    mpfr_inits2(prec, x, w, e, f, denom, tmp, (mpfr_ptr)0);

    mpfr_set(x, value, MPFR_RNDN);
    if (branch_m1) {
        mpfr_neg(w, x, MPFR_RNDN);
        mpfr_log(w, w, MPFR_RNDN);
        if (mpfr_cmp_si(w, -1) > 0)
            mpfr_set_si(w, -2, MPFR_RNDN);
    } else if (mpfr_cmp_ui(x, 1u) < 0) {
        mpfr_set(w, x, MPFR_RNDN);
    } else {
        mpfr_log(w, x, MPFR_RNDN);
    }

    for (i = 0; i < NUMBER_LAMBERT_W_HALLEY_STEPS; ++i) {
        mpfr_exp(e, w, MPFR_RNDN);
        mpfr_mul(f, w, e, MPFR_RNDN);
        mpfr_sub(f, f, x, MPFR_RNDN);

        mpfr_add_ui(denom, w, 1u, MPFR_RNDN);
        mpfr_mul(denom, denom, e, MPFR_RNDN);

        mpfr_add_ui(tmp, w, 2u, MPFR_RNDN);
        mpfr_mul(tmp, tmp, f, MPFR_RNDN);
        mpfr_add_ui(e, w, 1u, MPFR_RNDN);
        mpfr_mul_2ui(e, e, 1u, MPFR_RNDN);
        mpfr_div(tmp, tmp, e, MPFR_RNDN);
        mpfr_sub(denom, denom, tmp, MPFR_RNDN);

        mpfr_div(tmp, f, denom, MPFR_RNDN);
        mpfr_sub(w, w, tmp, MPFR_RNDN);
    }

    mpfr_set(value, w, MPFR_RNDN);
    mpfr_clears(x, w, e, f, denom, tmp, (mpfr_ptr)0);
    return 0;
}

static int number_mpfr_lambert_w0_mut(mpfr_t value)
{
    return number_mpfr_lambert_w_mut(value, false);
}

static int number_mpfr_lambert_wm1_mut(mpfr_t value)
{
    return number_mpfr_lambert_w_mut(value, true);
}

static int number_mpfr_productlog_mut(mpfr_t value)
{
    return number_mpfr_lambert_w0_mut(value);
}

typedef struct {
    const number_t *angle;
    number_const_id_t value_id;
    int sign;
    int imag;
} number_angle_fastpath_t;

typedef struct {
    const number_t *angle;
    number_const_id_t first_id;
    int first_sign;
    int first_imag;
    number_const_id_t second_id;
    int second_sign;
    int second_imag;
} number_angle_pair_fastpath_t;

typedef struct {
    const number_t *angle;
    number_const_id_t angle_id;
    number_const_id_t value_id;
    int sign;
    int reciprocal;
} number_tan_fastpath_t;

static const number_angle_pair_fastpath_t number_sincos_fastpaths[];
static const number_angle_pair_fastpath_t number_sinhcosh_fastpaths[];

static int number_try_get_pure_imag(const number_t number,
                                    number_t *imag_out);
static number_t number_apply_unary_math_with_double(const number_t number,
                                                    number_double_unary_fn d_fn,
                                                    number_qfloat_unary_fn qf_fn,
                                                    number_qcomplex_unary_fn qc_fn,
                                                    number_mpfr_unary_mut_fn mpfr_fn,
                                                    number_mpc_complex_unary_mut_fn mpc_fn);
static number_t number_apply_unary_mpc_complex(const number_t *number,
                                               const number_unary_math_ops_t *ops);

static size_t number_log_fastpath_precision(const number_t *number)
{
    size_t precision_bits = number ? num_get_prec_bits(*number) : 0u;

    if (number && number_kind_value(number) == NUMBER_COMPLEX &&
        precision_bits <= 1u)
        precision_bits = num_get_default_prec_bits();
    if (precision_bits == 0u)
        precision_bits = num_get_default_prec_bits();
    return precision_bits;
}

static bool number_is_plain_mpfr_value(const number_t *number)
{
    const number_vtable_t *vt = number_vt(number);

    return number_kind_value(number) == NUMBER_MPFR &&
        (!vt || !vt->is_immortal || !vt->is_immortal(number));
}

static bool number_is_plain_inexact_value(const number_t *number)
{
    const number_vtable_t *vt = number_vt(number);

    return number && number_is_valid_value(number) && vt && !vt->exact &&
        (!vt->is_immortal || !vt->is_immortal(number));
}

static number_t number_log_imag_multiple(const number_t *number,
                                          number_const_id_t angle_id,
                                          int sign)
{
    NUM_SCOPE(scope);
    size_t precision_bits;
    number_t imag_unit;
    number_t angle;
    number_t out;

    precision_bits = number_log_fastpath_precision(number);
    imag_unit = num_const_prec(NUM_I, precision_bits);
    angle = number_const_like(number, angle_id);
    if (num_get_prec_bits(angle) != 0u)
        num_set_prec_bits(&angle, precision_bits);
    out = num_mul(imag_unit, angle);
    if (sign < 0) {
        number_t neg = num_neg(out);

        num_destroy(&out);
        out = neg;
    }
    return num_scope_detach(out);
}

static int number_try_get_exact_int(const number_t number, int *out)
{
    string_t *text;
    uint64_t mantissa;
    long exponent2;
    int sign;
    int value;

    if (!out || !num_is_integer(number) || !num_is_real(number) ||
        !num_is_finite(number))
        return 0;

    if (num_get_mantissa_u64(number, &mantissa)) {
        exponent2 = num_get_exponent2(number);
        if (exponent2 < 0 || exponent2 >= (long)(sizeof(int) * 8u - 1u) ||
            mantissa > ((uint64_t)INT_MAX >> exponent2))
            return 0;

        value = (int)(mantissa << exponent2);
        sign = num_get_sign(number);
        if (sign < 0)
            value = -value;
        *out = value;
        return 1;
    }

    text = num_to_string(number);
    if (!text)
        return 0;
    if (!number_text_to_int(text, out)) {
        string_free(text);
        return 0;
    }
    string_free(text);
    return 1;
}

static bool number_get_exact_integer_mpz(const number_t number, mpz_t out)
{
    const number_private_t *impl;

    if (!out || !num_is_exact(number) || !num_is_real(number) ||
        !num_is_integer(number))
        return false;

    impl = number_impl_const(&number);
    if (impl->kind == NUMBER_MPZ) {
        if (number_mpz_ensure(impl->value.mpz) != 0)
            return false;
        mpz_set(out, impl->value.mpz->value);
        return true;
    }
    if (impl->kind == NUMBER_MPQ) {
        if (number_mpq_ensure(impl->value.mpq) != 0 ||
            mpz_cmp_ui(mpq_denref(impl->value.mpq->value), 1u) != 0)
            return false;
        mpz_set(out, mpq_numref(impl->value.mpq->value));
        return true;
    }
    return false;
}

static mpz_srcptr number_mpz_src_if_mpz(const number_t *number)
{
    const number_private_t *impl;

    if (!number)
        return NULL;
    impl = number_impl_const(number);
    if (impl->kind != NUMBER_MPZ || number_mpz_ensure(impl->value.mpz) != 0)
        return NULL;
    return impl->value.mpz->value;
}

static number_t number_from_mpz_value(mpz_srcptr value)
{
    number_mpz_t *out = value ? number_mpz_from_mpz(value) : NULL;

    return out ? number_take_mpz(out) : NUM_NAN;
}

static void number_assign_take_mpz(number_t *dst, number_mpz_t *value)
{
    num_destroy(dst);
    dst->storage[0] = NUMBER_MPZ;
    number_impl(dst)->value.mpz = value;
    number_scope_register_value(dst);
}

static number_t number_return_like_signed(const number_t *like,
                                          number_const_id_t id,
                                          int sign)
{
    return sign < 0
        ? number_neg_const_return_like(like, id)
        : number_const_return_like(like, id);
}

static number_t number_return_like_imag_signed(const number_t *like,
                                               number_const_id_t id,
                                               int sign)
{
    return sign < 0
        ? number_neg_const_return_like(like, id)
        : number_imag_const_return_like(like, id);
}

static int number_find_angle_fastpath(const number_t *value,
                                      const number_angle_fastpath_t *table,
                                      size_t count,
                                      const number_angle_fastpath_t **match_out)
{
    for (size_t i = 0; i < count; ++i) {
        if (number_matches_value(value, table[i].angle)) {
            *match_out = &table[i];
            return 1;
        }
    }
    return 0;
}

static number_t number_tan_fastpath_value(const number_t *like,
                                          const number_tan_fastpath_t *match)
{
    if (match->value_id == NUMBER_CONST_INF)
        return match->sign < 0 ? NUM_NINF : NUM_INF;

    if (match->reciprocal) {
        number_t numerator = number_const_like(like, NUMBER_CONST_ONE);
        number_t denominator = number_const_like(like, match->value_id);
        number_t out = num_div(numerator, denominator);

        num_destroy(&numerator);
        num_destroy(&denominator);
        if (match->sign < 0) {
            number_t neg = num_neg(out);

            num_destroy(&out);
            out = neg;
        }
        return out;
    }
    return number_return_like_signed(like, match->value_id, match->sign);
}

static bool number_tan_fastpath_by_const_id(const number_t *number,
                                            const number_tan_fastpath_t *table,
                                            size_t count,
                                            number_t *out)
{
    number_const_id_t id;

    if (!out || !number_const_id_from_immortal(number, &id))
        return false;
    for (size_t i = 0u; i < count; ++i) {
        if (table[i].angle_id == id) {
            *out = number_tan_fastpath_value(number, &table[i]);
            return true;
        }
    }
    return false;
}

static bool number_tan_fastpath_by_value(const number_t *number,
                                         const number_tan_fastpath_t *table,
                                         size_t count,
                                         number_t *out)
{
    if (!out)
        return false;
    for (size_t i = 0u; i < count; ++i) {
        if (table[i].angle && num_eq(*number, *table[i].angle)) {
            *out = number_tan_fastpath_value(number, &table[i]);
            return true;
        }
    }
    return false;
}

static const number_angle_fastpath_t number_exp_quarter_turn_fastpaths[] = {
    { &NUM_ZERO, NUMBER_CONST_ONE, 1, 0 },
    { &NUM_ZERO, NUMBER_CONST_I, 1, 0 },
    { &NUM_ZERO, NUMBER_CONST_NEG_ONE, 1, 0 },
    { &NUM_ZERO, NUMBER_CONST_I, -1, 0 }
};

static int number_exp_quarter_turn(const number_t *number,
                                   number_t *out)
{
    number_t real;
    number_t imag;
    number_t ratio;
    int quarter_turns;
    int mod4;
    double ratio_d;

    if (num_is_real(*number))
        return 0;

    real = num_real_part(*number);
    if (!num_is_zero(real)) {
        num_destroy(&real);
        return 0;
    }

    imag = num_imag_part(*number);
    ratio = num_div(imag, NUM_PI_2);
    num_destroy(&real);
    num_destroy(&imag);

    if (!number_try_get_exact_int(ratio, &quarter_turns)) {
        ratio_d = num_to_double(ratio);
        if (!isfinite(ratio_d) || ratio_d < (double)INT_MIN ||
            ratio_d > (double)INT_MAX) {
            num_destroy(&ratio);
            return 0;
        }
        double nearest = round(ratio_d);
        if (fabs(ratio_d - nearest) > 1e-12) {
            num_destroy(&ratio);
            return 0;
        }
        quarter_turns = (int)lround(ratio_d);
    }
    num_destroy(&ratio);

    mod4 = quarter_turns % 4;
    if (mod4 < 0)
        mod4 += 4;

    *out = number_return_like_signed(number,
        number_exp_quarter_turn_fastpaths[mod4].value_id,
        number_exp_quarter_turn_fastpaths[mod4].sign);
    return 1;
}

static number_t number_exp_backend(const number_t *number)
{
    const number_vtable_t *vt;
    number_t *promoted = NULL;
    number_t *result = NULL;

    vt = number_vt(number);
    if (!vt)
        return number_invalid();
    if (vt->exp_same)
        return number_take(vt->exp_same(number));

    promoted = number_coerce(number, NUMBER_MPFR);
    vt = number_vt(promoted);
    if (!vt || !vt->exp_same)
        goto done;
    result = vt->exp_same(promoted);
done:
    number_box_free(promoted);
    return number_take(result);
}

static number_t number_log_backend(const number_t *number)
{
    const number_vtable_t *vt;
    number_t *promoted = NULL;
    number_t *result = NULL;

    vt = number_vt(number);
    if (!vt)
        return number_invalid();
    if (vt->log_same)
        return number_take(vt->log_same(number));

    promoted = number_coerce(number, NUMBER_MPFR);
    vt = number_vt(promoted);
    if (!vt || !vt->log_same)
        goto done;
    result = vt->log_same(promoted);
done:
    number_box_free(promoted);
    return number_take(result);
}

static number_t number_apply_binary_same_mpfr(const number_t *a,
                                                const number_t *b,
                                                number_mpfr_binary_mut_fn fn)
{
    number_mpfr_t *copy;
    number_t *wrapped;

    if (!a || !b || !fn ||
        number_kind_value(a) != NUMBER_MPFR ||
        number_kind_value(b) != NUMBER_MPFR)
        return number_invalid();

    copy = number_mpfr_clone(number_impl_const(a)->value.mpfr);
    if (!copy || number_mpfr_ensure(copy, num_get_prec_bits(*a)) != 0 ||
        number_mpfr_ensure(number_impl_const(b)->value.mpfr, num_get_prec_bits(*b)) != 0 ||
        fn(copy->value, number_impl_const(b)->value.mpfr->value) != 0) {
        number_mpfr_free(copy);
        return number_invalid();
    }

    wrapped = number_wrap_mpfr(copy);
    return wrapped ? number_take(wrapped) : number_invalid();
}

static number_t number_take_mpc_complex_result(mpc_srcptr value,
                                               size_t precision_bits)
{
    number_t *wrapped;

    if (!value)
        return number_invalid();
    wrapped = number_wrap_complex_mpc(value, precision_bits);
    return wrapped ? number_take(wrapped) : number_invalid();
}

static number_t number_apply_complex_mpc_unary_direct(const number_t *number,
                                                      number_mpc_complex_unary_mut_fn fn)
{
    const complex_t *value;
    size_t precision_bits;
    mpc_t in;
    mpc_t out;
    number_t result = number_invalid();

    if (!number || !fn || number_kind_value(number) != NUMBER_COMPLEX)
        return number_invalid();

    value = number_impl_const(number)->value.cx;
    if (!value)
        return number_invalid();

    precision_bits = num_get_prec_bits(*number);
    if (precision_bits == 0u)
        precision_bits = num_get_default_prec_bits();

    mpc_init2(in, (mpfr_prec_t)precision_bits);
    mpc_init2(out, (mpfr_prec_t)precision_bits);
    if (number_complex_get_mpc(in, value, precision_bits) == 0) {
        (void)fn(out, in, MPC_RNDNN);
        result = number_take_mpc_complex_result(out, precision_bits);
    }
    mpc_clear(out);
    mpc_clear(in);
    return result;
}

static number_t number_apply_nonreal_complex_unary_or_dispatch(
    const number_t number,
    number_double_unary_fn d_fn,
    number_qfloat_unary_fn qf_fn,
    number_qcomplex_unary_fn qc_fn,
    number_mpfr_unary_mut_fn mpfr_fn,
    number_mpc_complex_unary_mut_fn mpc_fn)
{
    if (number_kind_value(&number) == NUMBER_COMPLEX && !num_is_real(number) &&
        mpc_fn)
        return number_apply_complex_mpc_unary_direct(&number, mpc_fn);
    return number_apply_unary_math_with_double(number, d_fn, qf_fn, qc_fn,
                                              mpfr_fn, mpc_fn);
}

static number_t number_apply_qcomplex_unary(const number_t number,
                                            number_qcomplex_unary_fn fn)
{
    const number_unary_math_ops_t ops = {
        .qreal = NULL,
        .qcomplex = fn,
        .mpfr = NULL,
        .mpc_complex = NULL
    };

    return number_apply_unary_mpc_complex(&number, &ops);
}

static bool number_lambert_w0_requires_complex(const number_t *number)
{
    return number && num_is_real(*number) && num_lt(*number, NUM_NEG_INV_E);
}

static bool number_lambert_wm1_requires_complex(const number_t *number)
{
    return number && num_is_real(*number) &&
        (num_lt(*number, NUM_NEG_INV_E) || num_ge(*number, NUM_ZERO));
}

static int number_trig_real_fastpath(const number_t *number,
                                     const number_angle_fastpath_t *table,
                                     size_t count,
                                     number_t *out)
{
    const number_angle_fastpath_t *match;

    if (!number_find_angle_fastpath(number, table, count, &match))
        return 0;
    *out = num_scope_detach(match->imag
        ? number_return_like_imag_signed(number, match->value_id, match->sign)
        : number_return_like_signed(number, match->value_id, match->sign));
    return 1;
}

static int number_hyperbolic_imag_fastpath(const number_t *number,
                                           const number_angle_fastpath_t *table,
                                           size_t count,
                                           number_t *out)
{
    number_t imag;
    const number_angle_fastpath_t *match;

    if (!number_try_get_pure_imag(*number, &imag))
        return 0;
    if (!number_find_angle_fastpath(&imag, table, count, &match)) {
        num_destroy(&imag);
        return 0;
    }
    num_destroy(&imag);
    *out = match->imag
        ? number_return_like_imag_signed(number, match->value_id, match->sign)
        : number_return_like_signed(number, match->value_id, match->sign);
    return 1;
}

static int number_trig_real_pair_fastpath(const number_t *number,
                                          const number_angle_pair_fastpath_t *table,
                                          size_t count,
                                          number_t *first_out,
                                          number_t *second_out)
{
    for (size_t i = 0; i < count; ++i) {
        if (!number_matches_value(number, table[i].angle))
            continue;
        *first_out = num_scope_detach(table[i].first_imag
            ? number_return_like_imag_signed(number, table[i].first_id, table[i].first_sign)
            : number_return_like_signed(number, table[i].first_id, table[i].first_sign));
        *second_out = num_scope_detach(table[i].second_imag
            ? number_return_like_imag_signed(number, table[i].second_id, table[i].second_sign)
            : number_return_like_signed(number, table[i].second_id, table[i].second_sign));
        return 1;
    }
    return 0;
}

static int number_hyperbolic_imag_pair_fastpath(const number_t *number,
                                                const number_angle_pair_fastpath_t *table,
                                                size_t count,
                                                number_t *first_out,
                                                number_t *second_out)
{
    number_t imag;

    if (!number_try_get_pure_imag(*number, &imag))
        return 0;
    for (size_t i = 0; i < count; ++i) {
        if (!number_matches_value(&imag, table[i].angle))
            continue;
        num_destroy(&imag);
        *first_out = table[i].first_imag
            ? number_return_like_imag_signed(number, table[i].first_id, table[i].first_sign)
            : number_return_like_signed(number, table[i].first_id, table[i].first_sign);
        *second_out = table[i].second_imag
            ? number_return_like_imag_signed(number, table[i].second_id, table[i].second_sign)
            : number_return_like_signed(number, table[i].second_id, table[i].second_sign);
        return 1;
    }
    num_destroy(&imag);
    return 0;
}

typedef number_t (*number_unary_math_apply_fn)(const number_t *number,
                                               const number_unary_math_ops_t *ops);
typedef number_t (*number_binary_math_apply_fn)(const number_t *a,
                                                const number_t *b,
                                                number_kind_t target_kind,
                                                const number_binary_math_ops_t *ops);
typedef number_t (*number_ternary_math_apply_fn)(const number_t *x,
                                                 const number_t *a,
                                                 const number_t *b,
                                                 number_kind_t target_kind,
                                                 const number_ternary_math_ops_t *ops);

static number_t number_apply_unary_qreal(const number_t *number,
                                         const number_unary_math_ops_t *ops)
{
    return ops && ops->qreal && number
        ? num_create_from_qfloat(ops->qreal(number_value_to_qfloat(number)))
        : number_invalid();
}

static number_t number_apply_unary_qcomplex(const number_t *number,
                                            const number_unary_math_ops_t *ops)
{
    return ops && ops->qcomplex && number
        ? num_create_from_qcomplex(ops->qcomplex(number_value_to_qcomplex(number)))
        : number_invalid();
}

static number_t number_apply_unary_mpfr_bridge(const number_t *number,
                                         const number_unary_math_ops_t *ops)
{
    number_t *promoted = NULL;
    number_mpfr_t *copy = NULL;

    if (!ops || !ops->mpfr || !number)
        return number_invalid();
    if (number_kind_value(number) == NUMBER_MPFR) {
        copy = number_mpfr_clone(number_impl_const(number)->value.mpfr);
        if (!copy || number_mpfr_ensure(copy, num_get_prec_bits(*number)) != 0 ||
            ops->mpfr(copy->value) != 0) {
            number_mpfr_free(copy);
            return number_invalid();
        }
        promoted = number_wrap_mpfr(copy);
        return promoted ? number_take(promoted) : number_invalid();
    }
    promoted = number_coerce(number, NUMBER_MPFR);
    if (!promoted ||
        number_mpfr_ensure(number_impl(promoted)->value.mpfr, num_get_prec_bits(*promoted)) != 0 ||
        ops->mpfr(number_impl(promoted)->value.mpfr->value) != 0) {
        number_box_free(promoted);
        return number_invalid();
    }
    return number_take(promoted);
}

static number_t number_apply_unary_mpc_complex(const number_t *number,
                                               const number_unary_math_ops_t *ops)
{
    number_t *promoted = NULL;
    size_t precision_bits;
    mpc_t in;
    mpc_t out;
    number_t result = number_invalid();

    if (!ops || !number)
        return number_invalid();
    if (!ops->mpc_complex)
        return ops->qcomplex
            ? num_create_from_qcomplex(ops->qcomplex(number_value_to_qcomplex(number)))
            : number_invalid();
    promoted = number_coerce(number, NUMBER_COMPLEX);
    if (!promoted)
        return number_invalid();
    precision_bits = num_get_prec_bits(*promoted);
    if (precision_bits == 0u)
        precision_bits = num_get_default_prec_bits();
    mpc_init2(in, (mpfr_prec_t)precision_bits);
    mpc_init2(out, (mpfr_prec_t)precision_bits);
    if (number_complex_get_mpc(in, number_impl_const(promoted)->value.cx,
            precision_bits) == 0) {
        (void)ops->mpc_complex(out, in, MPC_RNDNN);
        result = number_take_mpc_complex_result(out, precision_bits);
    }
    mpc_clear(out);
    mpc_clear(in);
    number_box_free(promoted);
    return result;
}

static number_t number_apply_binary_qreal(const number_t *a,
                                          const number_t *b,
                                          number_kind_t target_kind,
                                          const number_binary_math_ops_t *ops)
{
    (void)target_kind;
    return ops && ops->qreal && a && b
        ? num_create_from_qfloat(ops->qreal(number_value_to_qfloat(a),
            number_value_to_qfloat(b)))
        : number_invalid();
}

static number_t number_apply_binary_qcomplex(const number_t *a,
                                             const number_t *b,
                                             number_kind_t target_kind,
                                             const number_binary_math_ops_t *ops)
{
    (void)target_kind;
    return ops && ops->qcomplex && a && b
        ? num_create_from_qcomplex(ops->qcomplex(number_value_to_qcomplex(a),
            number_value_to_qcomplex(b)))
        : number_invalid();
}

static number_t number_apply_binary_mpfr_bridge(const number_t *a,
                                          const number_t *b,
                                          number_kind_t target_kind,
                                          const number_binary_math_ops_t *ops)
{
    number_t *lhs = NULL;
    number_t *rhs = NULL;

    if (!ops || !ops->mpfr || !a || !b)
        return number_invalid();
    lhs = number_coerce(a, target_kind);
    rhs = number_coerce(b, target_kind);
    if (!lhs || !rhs ||
        number_mpfr_ensure(number_impl(lhs)->value.mpfr, num_get_prec_bits(*lhs)) != 0 ||
        number_mpfr_ensure(number_impl_const(rhs)->value.mpfr, num_get_prec_bits(*rhs)) != 0 ||
        ops->mpfr(number_impl(lhs)->value.mpfr->value,
                  number_impl_const(rhs)->value.mpfr->value) != 0) {
        number_box_free(lhs);
        number_box_free(rhs);
        return number_invalid();
    }
    number_box_free(rhs);
    return number_take(lhs);
}

static number_t number_apply_binary_mpc_complex(const number_t *a,
                                                const number_t *b,
                                                number_kind_t target_kind,
                                                const number_binary_math_ops_t *ops)
{
    number_t *lhs = NULL;
    number_t *rhs = NULL;
    size_t precision_bits;
    mpc_t lhs_value;
    mpc_t rhs_value;
    mpc_t out_value;
    number_t result = number_invalid();

    if (!ops || !a || !b)
        return number_invalid();
    if (!ops->mpc_complex)
        return ops->qcomplex
            ? num_create_from_qcomplex(ops->qcomplex(
                number_value_to_qcomplex(a), number_value_to_qcomplex(b)))
            : number_invalid();
    (void)target_kind;
    lhs = number_coerce(a, NUMBER_COMPLEX);
    rhs = number_coerce(b, NUMBER_COMPLEX);
    if (!lhs || !rhs)
        goto fail;
    precision_bits = num_get_prec_bits(*lhs);
    if (num_get_prec_bits(*rhs) > precision_bits)
        precision_bits = num_get_prec_bits(*rhs);
    if (precision_bits == 0u)
        precision_bits = num_get_default_prec_bits();
    mpc_init2(lhs_value, (mpfr_prec_t)precision_bits);
    mpc_init2(rhs_value, (mpfr_prec_t)precision_bits);
    mpc_init2(out_value, (mpfr_prec_t)precision_bits);
    if (number_complex_get_mpc(lhs_value, number_impl_const(lhs)->value.cx,
            precision_bits) == 0 &&
        number_complex_get_mpc(rhs_value, number_impl_const(rhs)->value.cx,
            precision_bits) == 0) {
        (void)ops->mpc_complex(out_value, lhs_value, rhs_value, MPC_RNDNN);
        result = number_take_mpc_complex_result(out_value, precision_bits);
    }
    mpc_clear(out_value);
    mpc_clear(rhs_value);
    mpc_clear(lhs_value);
    number_box_free(rhs);
    number_box_free(lhs);
    return result;

fail:
    number_box_free(lhs);
    number_box_free(rhs);
    return number_invalid();
}

static number_t number_apply_ternary_qreal(const number_t *x,
                                           const number_t *a,
                                           const number_t *b,
                                           number_kind_t target_kind,
                                           const number_ternary_math_ops_t *ops)
{
    (void)target_kind;
    return ops && ops->qreal && x && a && b
        ? num_create_from_qfloat(ops->qreal(number_value_to_qfloat(x),
            number_value_to_qfloat(a),
            number_value_to_qfloat(b)))
        : number_invalid();
}

static number_t number_apply_ternary_qcomplex(const number_t *x,
                                              const number_t *a,
                                              const number_t *b,
                                              number_kind_t target_kind,
                                              const number_ternary_math_ops_t *ops)
{
    (void)target_kind;
    return ops && ops->qcomplex && x && a && b
        ? num_create_from_qcomplex(ops->qcomplex(number_value_to_qcomplex(x),
            number_value_to_qcomplex(a),
            number_value_to_qcomplex(b)))
        : number_invalid();
}

static number_t number_apply_ternary_mpfr_bridge(const number_t *x,
                                           const number_t *a,
                                           const number_t *b,
                                           number_kind_t target_kind,
                                           const number_ternary_math_ops_t *ops)
{
    number_t *nx = NULL;
    number_t *na = NULL;
    number_t *nb = NULL;

    if (!ops || !ops->mpfr || !x || !a || !b)
        return number_invalid();
    nx = number_coerce(x, target_kind);
    na = number_coerce(a, target_kind);
    nb = number_coerce(b, target_kind);
    if (!nx || !na || !nb ||
        number_mpfr_ensure(number_impl(nx)->value.mpfr, num_get_prec_bits(*nx)) != 0 ||
        number_mpfr_ensure(number_impl_const(na)->value.mpfr, num_get_prec_bits(*na)) != 0 ||
        number_mpfr_ensure(number_impl_const(nb)->value.mpfr, num_get_prec_bits(*nb)) != 0 ||
        ops->mpfr(number_impl(nx)->value.mpfr->value,
                  number_impl_const(na)->value.mpfr->value,
                  number_impl_const(nb)->value.mpfr->value) != 0) {
        number_box_free(nx);
        number_box_free(na);
        number_box_free(nb);
        return number_invalid();
    }
    number_box_free(na);
    number_box_free(nb);
    return number_take(nx);
}

static number_t number_apply_ternary_mpc_complex(const number_t *x,
                                                 const number_t *a,
                                                 const number_t *b,
                                                 number_kind_t target_kind,
                                                 const number_ternary_math_ops_t *ops)
{
    number_t *nx = NULL;
    number_t *na = NULL;
    number_t *nb = NULL;
    size_t precision_bits;
    mpc_t x_value;
    mpc_t a_value;
    mpc_t b_value;
    mpc_t out_value;
    number_t result = number_invalid();

    if (!ops || !x || !a || !b)
        return number_invalid();
    if (!ops->mpc_complex)
        return ops->qcomplex
            ? num_create_from_qcomplex(ops->qcomplex(
                number_value_to_qcomplex(x),
                number_value_to_qcomplex(a),
                number_value_to_qcomplex(b)))
            : number_invalid();
    (void)target_kind;
    nx = number_coerce(x, NUMBER_COMPLEX);
    na = number_coerce(a, NUMBER_COMPLEX);
    nb = number_coerce(b, NUMBER_COMPLEX);
    if (!nx || !na || !nb)
        goto fail;
    precision_bits = num_get_prec_bits(*nx);
    if (num_get_prec_bits(*na) > precision_bits)
        precision_bits = num_get_prec_bits(*na);
    if (num_get_prec_bits(*nb) > precision_bits)
        precision_bits = num_get_prec_bits(*nb);
    if (precision_bits == 0u)
        precision_bits = num_get_default_prec_bits();
    mpc_init2(x_value, (mpfr_prec_t)precision_bits);
    mpc_init2(a_value, (mpfr_prec_t)precision_bits);
    mpc_init2(b_value, (mpfr_prec_t)precision_bits);
    mpc_init2(out_value, (mpfr_prec_t)precision_bits);
    if (number_complex_get_mpc(x_value, number_impl_const(nx)->value.cx,
            precision_bits) == 0 &&
        number_complex_get_mpc(a_value, number_impl_const(na)->value.cx,
            precision_bits) == 0 &&
        number_complex_get_mpc(b_value, number_impl_const(nb)->value.cx,
            precision_bits) == 0) {
        (void)ops->mpc_complex(out_value, x_value, a_value, b_value, MPC_RNDNN);
        result = number_take_mpc_complex_result(out_value, precision_bits);
    }
    mpc_clear(out_value);
    mpc_clear(b_value);
    mpc_clear(a_value);
    mpc_clear(x_value);
    number_box_free(na);
    number_box_free(nb);
    number_box_free(nx);
    return result;

fail:
    number_box_free(nx);
    number_box_free(na);
    number_box_free(nb);
    return number_invalid();
}

static number_t number_apply_unary_math(const number_t number,
                                        number_qfloat_unary_fn qf_fn,
                                        number_qcomplex_unary_fn qc_fn,
                                        number_mpfr_unary_mut_fn mpfr_fn,
                                        number_mpc_complex_unary_mut_fn mpc_fn)
{
    static const number_unary_math_apply_fn dispatch[] = {
        [NUMBER_MATH_INVALID] = NULL,
        [NUMBER_MATH_QREAL] = number_apply_unary_qreal,
        [NUMBER_MATH_QCOMPLEX] = number_apply_unary_qcomplex,
        [NUMBER_MATH_MPFR] = number_apply_unary_mpfr_bridge,
        [NUMBER_MATH_COMPLEX] = number_apply_unary_mpc_complex
    };
    const number_unary_math_ops_t ops = {
        .qreal = qf_fn,
        .qcomplex = qc_fn,
        .mpfr = mpfr_fn,
        .mpc_complex = mpc_fn
    };
    number_math_family_t family = number_math_family_value(&number);

    return (unsigned)family <= NUMBER_MATH_COMPLEX && dispatch[family]
        ? dispatch[family](&number, &ops)
        : number_invalid();
}

static number_t number_apply_unary_math_with_double(const number_t number,
                                                    number_double_unary_fn d_fn,
                                                    number_qfloat_unary_fn qf_fn,
                                                    number_qcomplex_unary_fn qc_fn,
                                                    number_mpfr_unary_mut_fn mpfr_fn,
                                                    number_mpc_complex_unary_mut_fn mpc_fn)
{
    return number_is_valid_value(&number) &&
           number_kind_value(&number) == NUMBER_DOUBLE && d_fn
        ? num_create_from_double(d_fn(number_impl_const(&number)->value.d))
        : number_apply_unary_math(number, qf_fn, qc_fn, mpfr_fn, mpc_fn);
}

static number_t number_apply_binary_math(const number_t a,
                                         const number_t b,
                                         number_qfloat_binary_fn qf_fn,
                                         number_qcomplex_binary_fn qc_fn,
                                         number_mpfr_binary_mut_fn mpfr_fn,
                                         number_mpc_complex_binary_mut_fn mpc_fn)
{
    static const number_binary_math_apply_fn dispatch[] = {
        [NUMBER_MATH_INVALID] = NULL,
        [NUMBER_MATH_QREAL] = number_apply_binary_qreal,
        [NUMBER_MATH_QCOMPLEX] = number_apply_binary_qcomplex,
        [NUMBER_MATH_MPFR] = number_apply_binary_mpfr_bridge,
        [NUMBER_MATH_COMPLEX] = number_apply_binary_mpc_complex
    };
    const number_binary_math_ops_t ops = {
        .qreal = qf_fn,
        .qcomplex = qc_fn,
        .mpfr = mpfr_fn,
        .mpc_complex = mpc_fn
    };
    number_math_family_t family = number_math_family_binary(
        number_math_family_value(&a),
        number_math_family_value(&b));
    number_kind_t target_kind = number_math_family_target_kind(family);

    return (unsigned)family <= NUMBER_MATH_COMPLEX && dispatch[family]
        ? dispatch[family](&a, &b, target_kind, &ops)
        : number_invalid();
}

static number_t number_apply_binary_math_with_double(const number_t a,
                                                     const number_t b,
                                                     number_double_binary_fn d_fn,
                                                     number_qfloat_binary_fn qf_fn,
                                                     number_qcomplex_binary_fn qc_fn,
                                                     number_mpfr_binary_mut_fn mpfr_fn,
                                                     number_mpc_complex_binary_mut_fn mpc_fn)
{
    return number_is_valid_value(&a) && number_is_valid_value(&b) &&
           number_kind_value(&a) == NUMBER_DOUBLE &&
           number_kind_value(&b) == NUMBER_DOUBLE && d_fn
        ? num_create_from_double(d_fn(number_impl_const(&a)->value.d,
            number_impl_const(&b)->value.d))
        : number_apply_binary_math(a, b, qf_fn, qc_fn, mpfr_fn, mpc_fn);
}

static number_t number_apply_ternary_math(const number_t x,
                                          const number_t a,
                                          const number_t b,
                                          number_qfloat_ternary_fn qf_fn,
                                          number_qcomplex_ternary_fn qc_fn,
                                          number_mpfr_ternary_mut_fn mpfr_fn,
                                          number_mpc_complex_ternary_mut_fn mpc_fn)
{
    static const number_ternary_math_apply_fn dispatch[] = {
        [NUMBER_MATH_INVALID] = NULL,
        [NUMBER_MATH_QREAL] = number_apply_ternary_qreal,
        [NUMBER_MATH_QCOMPLEX] = number_apply_ternary_qcomplex,
        [NUMBER_MATH_MPFR] = number_apply_ternary_mpfr_bridge,
        [NUMBER_MATH_COMPLEX] = number_apply_ternary_mpc_complex
    };
    const number_ternary_math_ops_t ops = {
        .qreal = qf_fn,
        .qcomplex = qc_fn,
        .mpfr = mpfr_fn,
        .mpc_complex = mpc_fn
    };
    number_math_family_t family = number_math_family_binary(
        number_math_family_value(&x),
        number_math_family_value(&a));
    number_kind_t target_kind;

    family = number_math_family_binary(family, number_math_family_value(&b));
    target_kind = number_math_family_target_kind(family);
    return (unsigned)family <= NUMBER_MATH_COMPLEX && dispatch[family]
        ? dispatch[family](&x, &a, &b, target_kind, &ops)
        : number_invalid();
}

static number_t number_apply_ternary_math_with_double(
    const number_t x,
    const number_t a,
    const number_t b,
    number_double_ternary_fn d_fn,
    number_qfloat_ternary_fn qf_fn,
    number_qcomplex_ternary_fn qc_fn,
    number_mpfr_ternary_mut_fn mpfr_fn,
    number_mpc_complex_ternary_mut_fn mpc_fn)
{
    return number_is_valid_value(&x) && number_is_valid_value(&a) &&
           number_is_valid_value(&b) &&
           number_kind_value(&x) == NUMBER_DOUBLE &&
           number_kind_value(&a) == NUMBER_DOUBLE &&
           number_kind_value(&b) == NUMBER_DOUBLE && d_fn
        ? num_create_from_double(d_fn(number_impl_const(&x)->value.d,
            number_impl_const(&a)->value.d,
            number_impl_const(&b)->value.d))
        : number_apply_ternary_math(x, a, b, qf_fn, qc_fn, mpfr_fn, mpc_fn);
}

number_t num_exp(const number_t number)
{
    number_t root;
    number_t root2;
    number_t out;

    if (number_is_cdouble_value(&number))
        return number_apply_cdouble_unary(number, cexp);
    if (number_is_plain_inexact_value(&number))
        return number_exp_backend(&number);

    if (!num_is_real(number)) {
        if (number_exp_quarter_turn(&number, &out))
            return out;
        return number_exp_backend(&number);
    }

    if (num_eq(number, NUM_ZERO))
        return number_const_return_like(&number, NUMBER_CONST_ONE);
    if (num_eq(number, NUM_ONE))
        return number_const_return_like(&number, NUMBER_CONST_E);
    if (num_eq(number, NUM_NEG_ONE))
        return number_const_return_like(&number, NUMBER_CONST_INV_E);
    if (num_eq(number, NUM_HALF)) {
        number_t e = number_const_like(&number, NUMBER_CONST_E);
        number_t out = num_sqrt(e);

        num_destroy(&e);
        return out;
    }
    if (num_eq(number, NUM_QUARTER)) {
        number_t out;
        number_t e = number_const_like(&number, NUMBER_CONST_E);
        root = num_sqrt(e);
        num_destroy(&e);
        out = num_sqrt(root);
        num_destroy(&root);
        return out;
    }
    if (num_eq(number, NUM_ONE_EIGHTH)) {
        number_t out;
        number_t e = number_const_like(&number, NUMBER_CONST_E);
        root = num_sqrt(e);
        num_destroy(&e);
        root2 = num_sqrt(root);
        out = num_sqrt(root2);
        num_destroy(&root);
        num_destroy(&root2);
        return out;
    }
    return number_exp_backend(&number);
}

number_t num_log(const number_t number)
{
    number_kind_t kind = number_kind_value(&number);
    number_t imag;
    number_t neg_i;
    double d;
    qfloat_t qf;

    if (kind == NUMBER_DOUBLE) {
        d = number_impl_const(&number)->value.d;
        return d < 0.0 ? number_double_cdouble_unary(d, clog)
                       : num_create_from_double(log(d));
    }
    if (kind == NUMBER_CDOUBLE)
        return number_apply_cdouble_unary(number, clog);
    if (kind == NUMBER_QFLOAT) {
        qf = number_impl_const(&number)->value.qf;
        return qf_lt(qf, QF_ZERO) ? number_qfloat_qcomplex_unary(qf, qc_log)
                                  : num_create_from_qfloat(qf_log(qf));
    }
    if (number_is_plain_inexact_value(&number))
        return number_log_backend(&number);

    if (!num_is_real(number)) {
        if (number_try_get_pure_imag(number, &imag)) {
            if (num_eq(imag, NUM_ONE)) {
                num_destroy(&imag);
                return number_log_imag_multiple(&number, NUMBER_CONST_PI_2, 1);
            }
            if (num_eq(imag, NUM_NEG_ONE)) {
                num_destroy(&imag);
                return number_log_imag_multiple(&number, NUMBER_CONST_PI_2, -1);
            }
            num_destroy(&imag);
        }
        return number_log_backend(&number);
    }

    if (num_eq(number, NUM_ONE))
        return number_const_return_like(&number, NUMBER_CONST_ZERO);
    if (num_eq(number, NUM_E))
        return number_const_return_like(&number, NUMBER_CONST_ONE);
    if (num_eq(number, NUM_INV_E))
        return number_const_return_like(&number, NUMBER_CONST_NEG_ONE);
    if (num_eq(number, NUM_TWO))
        return number_const_return_like(&number, NUMBER_CONST_LN2);
    if (num_eq(number, NUM_HALF))
        return number_neg_const_return_like(&number, NUMBER_CONST_LN2);
    if (num_eq(number, NUM_I))
        return number_log_imag_multiple(&number, NUMBER_CONST_PI_2, 1);
    if (num_eq(number, NUM_NEG_ONE))
        return number_log_imag_multiple(&number, NUMBER_CONST_PI, 1);
    neg_i = number_neg_const_return_like(&number, NUMBER_CONST_I);
    if (num_eq(number, neg_i)) {
        num_destroy(&neg_i);
        return number_log_imag_multiple(&number, NUMBER_CONST_PI_2, -1);
    }
    num_destroy(&neg_i);

    return number_log_backend(&number);
}

number_t num_log10(const number_t number)
{
    number_kind_t kind = number_kind_value(&number);
    number_t log_value;
    number_t ln10;
    number_t out;
    double d;
    qfloat_t qf;

    if (kind == NUMBER_DOUBLE) {
        d = number_impl_const(&number)->value.d;
        return d < 0.0 ? number_double_cdouble_unary(d, number_cdouble_log10)
                       : num_create_from_double(log10(d));
    }
    if (kind == NUMBER_CDOUBLE)
        return number_apply_cdouble_unary(number, number_cdouble_log10);
    if (kind == NUMBER_QFLOAT) {
        qf = number_impl_const(&number)->value.qf;
        return qf_lt(qf, QF_ZERO) ? number_qfloat_qcomplex_unary(qf, qc_log10)
                                  : num_create_from_qfloat(qf_log10(qf));
    }
    if (num_is_real(number) && num_lt(number, NUM_ZERO)) {
        log_value = num_log(number);
        ln10 = number_const_like(&number, NUMBER_CONST_LN10);
        out = num_div(log_value, ln10);
        num_destroy(&ln10);
        num_destroy(&log_value);
        return out;
    }
    return number_apply_unary_math_with_double(number, log10, qf_log10, qc_log10, number_mpfr_log10_mut, mpc_log10);
}

number_t num_sqrt(const number_t number)
{
    const number_vtable_t *vt;
    number_t *promoted = NULL;
    number_t *result = NULL;
    number_kind_t kind = number_impl_const(&number)->kind;
    double d;
    qfloat_t qf;

    if (kind == NUMBER_DOUBLE) {
        d = number_impl_const(&number)->value.d;
        return d < 0.0 ? number_double_cdouble_unary(d, csqrt)
                       : num_create_from_double(sqrt(d));
    }
    if (kind == NUMBER_CDOUBLE)
        return number_apply_cdouble_unary(number, csqrt);
    if (kind == NUMBER_QFLOAT) {
        qf = number_impl_const(&number)->value.qf;
        return qf_lt(qf, QF_ZERO) ? number_qfloat_qcomplex_unary(qf, qc_sqrt)
                                  : num_create_from_qfloat(qf_sqrt(qf));
    }
    if (kind == NUMBER_QCOMPLEX)
        return num_create_from_qcomplex(qc_sqrt(number_impl_const(&number)->value.qc));
    if ((kind == NUMBER_MPZ || kind == NUMBER_MPQ || kind == NUMBER_MPFR) &&
        num_get_sign(number) < 0) {
        number_t positive = num_neg(number);
        number_t real = num_create_from_long(0);
        number_t imag = num_sqrt(positive);

        num_destroy(&positive);
        return number_take(number_wrap_complex_parts(real, imag));
    }
    vt = number_vt(&number);
    if (!vt)
        return number_invalid();
    if (vt->sqrt_same)
        return number_take(vt->sqrt_same(&number));

    promoted = number_coerce(&number, NUMBER_MPFR);
    vt = number_vt(promoted);
    if (!vt || !vt->sqrt_same)
        goto done;
    result = vt->sqrt_same(promoted);
done:
    number_box_free(promoted);
    return number_take(result);
}

number_t num_pow(const number_t base, const number_t exponent)
{
    number_t half;
    number_t quarter;
    number_t eighth;
    number_t root;
    double bd;
    double ed;
    double rd;
    qfloat_t bqf;
    qfloat_t eqf;
    qfloat_t rqf;
    int exp_int;

    if (!number_is_valid_value(&base) || !number_is_valid_value(&exponent))
        return number_invalid();
    if (number_kind_value(&base) == NUMBER_DOUBLE &&
        number_kind_value(&exponent) == NUMBER_DOUBLE) {
        bd = number_impl_const(&base)->value.d;
        ed = number_impl_const(&exponent)->value.d;
        rd = pow(bd, ed);
        return isnan(rd) && bd < 0.0 && isfinite(bd) && isfinite(ed)
            ? number_double_cdouble_binary(bd, ed, cpow)
            : num_create_from_double(rd);
    }
    if (number_kind_value(&base) == NUMBER_CDOUBLE &&
        number_kind_value(&exponent) == NUMBER_CDOUBLE)
        return number_apply_cdouble_binary(base, exponent, cpow);
    if (number_kind_value(&base) == NUMBER_QFLOAT &&
        number_kind_value(&exponent) == NUMBER_QFLOAT) {
        bqf = number_impl_const(&base)->value.qf;
        eqf = number_impl_const(&exponent)->value.qf;
        rqf = qf_pow(bqf, eqf);
        return qf_isnan(rqf) && qf_lt(bqf, QF_ZERO)
            ? number_qfloat_qcomplex_binary(bqf, eqf, qc_pow)
            : num_create_from_qfloat(rqf);
    }
    if (number_is_plain_inexact_value(&base) &&
        number_is_plain_inexact_value(&exponent))
        return number_apply_binary_math(base, exponent, qf_pow, qc_pow,
            number_mpfr_pow_mut, mpc_pow);

    if (num_eq(exponent, NUM_ZERO))
        return number_const_return_like(&base, NUMBER_CONST_ONE);
    if (num_eq(exponent, NUM_ONE))
        return num_clone(base);
    if (num_eq(exponent, NUM_NEG_ONE))
        return num_inv(base);
    if (num_eq(exponent, NUM_TWO))
        return num_sqr(base);

    half = NUM_HALF;
    if (num_eq(exponent, half))
        return num_sqrt(base);
    quarter = NUM_QUARTER;
    if (num_eq(exponent, quarter)) {
        number_t result;
        root = num_sqrt(base);
        result = num_sqrt(root);
        num_destroy(&root);
        return result;
    }
    eighth = NUM_ONE_EIGHTH;
    if (num_eq(exponent, eighth)) {
        number_t root2;
        number_t result;
        root = num_sqrt(base);
        root2 = num_sqrt(root);
        result = num_sqrt(root2);
        num_destroy(&root);
        num_destroy(&root2);
        return result;
    }

    if (number_try_get_exact_int(exponent, &exp_int))
        return num_pow_int(base, exp_int);

    return number_apply_binary_math(base, exponent, qf_pow, qc_pow, number_mpfr_pow_mut, mpc_pow);
}

number_t num_pow_int(const number_t base, int exponent)
{
    const number_vtable_t *vt = number_vt(&base);
    number_t expnum;
    number_t result;

    if (!number_is_valid_value(&base))
        return number_invalid();
    if (vt && vt->pow_int)
        return number_take(vt->pow_int(&base, exponent));

    expnum = num_create_from_long(exponent);
    result = number_apply_binary_math(base, expnum, qf_pow, qc_pow, number_mpfr_pow_mut, mpc_pow);
    num_destroy(&expnum);
    return result;
}

number_t num_ldexp(const number_t number, int exponent2)
{
    const number_vtable_t *vt = number_vt(&number);
    number_t two;
    number_t scale;
    number_t result;

    if (!number_is_valid_value(&number))
        return number_invalid();
    if (vt && vt->ldexp_value)
        return number_take(vt->ldexp_value(&number, exponent2));

    two = num_create_from_long(2);
    scale = num_pow_int(two, exponent2);
    result = num_mul(number, scale);
    num_destroy(&two);
    num_destroy(&scale);
    return result;
}

number_t num_sqr(const number_t number)
{
    number_kind_t kind = number_impl_const(&number)->kind;

    if (kind == NUMBER_DOUBLE)
        return num_create_from_double(number_impl_const(&number)->value.d *
            number_impl_const(&number)->value.d);
    if (kind == NUMBER_QFLOAT)
        return num_create_from_qfloat(qf_sqr(number_impl_const(&number)->value.qf));
    if (kind == NUMBER_INVALID)
        return number_invalid();
    if (!num_is_real(number))
        return num_mul(number, number);
    return number_apply_unary_math(number, qf_sqr, NULL, number_mpfr_sqr_mut, NULL);
}

number_t num_floor(const number_t number)
{
    const number_vtable_t *vt;
    number_kind_t kind = number_impl_const(&number)->kind;

    if (kind == NUMBER_DOUBLE)
        return num_create_from_double(floor(number_impl_const(&number)->value.d));
    if (kind == NUMBER_QFLOAT)
        return num_create_from_qfloat(qf_floor(number_impl_const(&number)->value.qf));
    if (kind == NUMBER_QCOMPLEX)
        return num_create_from_qcomplex(qc_floor(number_impl_const(&number)->value.qc));
    vt = number_vt(&number);
    if (vt && vt->floor_value)
        return number_take(vt->floor_value(&number));
    return number_apply_unary_math(number, qf_floor, qc_floor, number_mpfr_floor_mut, NULL);
}

number_t num_ceil(const number_t number)
{
    NUM_SCOPE(scope);
    number_t neg = num_neg(number);
    number_t floor_neg = num_floor(neg);

    return num_scope_detach(num_neg(floor_neg));
}

number_t num_pow10(int exponent10)
{
    number_mpfr_t *out = number_mpfr_new_prec(number_default_precision_bits);
    unsigned int exponent = exponent10 < 0
        ? (unsigned int)-exponent10
        : (unsigned int)exponent10;

    if (!out)
        return number_invalid();
    mpfr_ui_pow_ui(out->value, 10u, exponent, MPFR_RNDN);
    if (exponent10 < 0)
        mpfr_ui_div(out->value, 1u, out->value, MPFR_RNDN);
    return number_take(number_wrap_mpfr(out));
}

number_t num_mul_pow10(const number_t number, int exponent10)
{
    const number_vtable_t *vt = number_vt(&number);
    number_t scale;
    number_t result;

    if (!number_is_valid_value(&number))
        return number_invalid();
    if (vt && vt->mul_pow10_value)
        return number_take(vt->mul_pow10_value(&number, exponent10));
    scale = num_pow10(exponent10);
    result = num_mul(number, scale);
    num_destroy(&scale);
    return result;
}

number_t num_factorial(unsigned long n)
{
    mpz_t value;
    number_t out;

    mpz_init(value);
    mpz_fac_ui(value, n);
    out = number_from_mpz_value(value);
    mpz_clear(value);
    return out;
}

number_t num_fibonacci(unsigned long n)
{
    mpz_t value;
    number_t out;

    mpz_init(value);
    mpz_fib_ui(value, n);
    out = number_from_mpz_value(value);
    mpz_clear(value);
    return out;
}

number_t num_partition(const number_t number)
{
    mpz_t n_value;
    unsigned long n;
    mpz_t *parts;
    number_t out;

    mpz_init(n_value);
    if (!number_get_exact_integer_mpz(number, n_value)) {
        mpz_clear(n_value);
        return NUM_NAN;
    }
    if (mpz_sgn(n_value) < 0) {
        mpz_clear(n_value);
        return num_create_from_long(0);
    }
    if (!mpz_fits_ulong_p(n_value)) {
        mpz_clear(n_value);
        return NUM_NAN;
    }
    n = mpz_get_ui(n_value);
    mpz_clear(n_value);

    if (n > (((size_t)-1) / sizeof(*parts)) - 1u)
        return NUM_NAN;
    parts = calloc((size_t)n + 1u, sizeof(*parts));
    if (!parts)
        return NUM_NAN;

    for (unsigned long i = 0u; i <= n; ++i)
        mpz_init(parts[i]);
    mpz_set_ui(parts[0], 1u);

    for (unsigned long total = 1u; total <= n; ++total) {
        for (unsigned long k = 1u; ; ++k) {
            unsigned long three_k;
            unsigned long g1;
            unsigned long g2;
            bool add;

            if (k > ULONG_MAX / 3u)
                break;
            three_k = 3u * k;
            if (k > ULONG_MAX / (three_k - 1u))
                break;
            g1 = (k * (three_k - 1u)) / 2u;
            if (g1 > total)
                break;

            add = (k % 2u) != 0u;
            if (add)
                mpz_add(parts[total], parts[total], parts[total - g1]);
            else
                mpz_sub(parts[total], parts[total], parts[total - g1]);

            if (three_k == ULONG_MAX || k > ULONG_MAX / (three_k + 1u))
                continue;
            g2 = (k * (three_k + 1u)) / 2u;
            if (g2 <= total) {
                if (add)
                    mpz_add(parts[total], parts[total], parts[total - g2]);
                else
                    mpz_sub(parts[total], parts[total], parts[total - g2]);
            }
        }
    }

    out = number_from_mpz_value(parts[n]);
    for (unsigned long i = 0u; i <= n; ++i)
        mpz_clear(parts[i]);
    free(parts);
    return out;
}

number_t num_isqrt(const number_t number)
{
    mpz_srcptr direct = number_mpz_src_if_mpz(&number);
    mpz_t value;
    number_t out = NUM_NAN;

    if (direct) {
        number_mpz_t *result;

        if (mpz_sgn(direct) < 0)
            return NUM_NAN;
        result = number_mpz_new();
        if (!result)
            return NUM_NAN;
        mpz_sqrt(result->value, direct);
        return number_take_mpz(result);
    }

    mpz_init(value);
    if (number_get_exact_integer_mpz(number, value) && mpz_sgn(value) >= 0) {
        mpz_sqrt(value, value);
        out = number_from_mpz_value(value);
    }
    mpz_clear(value);
    return out;
}

number_t num_gcd(const number_t a, const number_t b)
{
    mpz_srcptr ad = number_mpz_src_if_mpz(&a);
    mpz_srcptr bd = number_mpz_src_if_mpz(&b);
    mpz_t av;
    mpz_t bv;
    mpz_t gv;
    number_t out = NUM_NAN;

    if (ad && bd) {
        number_mpz_t *result = number_mpz_new();

        if (!result)
            return NUM_NAN;
        mpz_gcd(result->value, ad, bd);
        return number_take_mpz(result);
    }

    mpz_init(av);
    mpz_init(bv);
    mpz_init(gv);
    if (number_get_exact_integer_mpz(a, av) &&
        number_get_exact_integer_mpz(b, bv)) {
        mpz_gcd(gv, av, bv);
        out = number_from_mpz_value(gv);
    }
    mpz_clear(gv);
    mpz_clear(bv);
    mpz_clear(av);
    return out;
}

number_t num_lcm(const number_t a, const number_t b)
{
    mpz_srcptr ad = number_mpz_src_if_mpz(&a);
    mpz_srcptr bd = number_mpz_src_if_mpz(&b);
    mpz_t av;
    mpz_t bv;
    mpz_t lv;
    number_t out = NUM_NAN;

    if (ad && bd) {
        number_mpz_t *result = number_mpz_new();

        if (!result)
            return NUM_NAN;
        mpz_lcm(result->value, ad, bd);
        return number_take_mpz(result);
    }

    mpz_init(av);
    mpz_init(bv);
    mpz_init(lv);
    if (number_get_exact_integer_mpz(a, av) &&
        number_get_exact_integer_mpz(b, bv)) {
        mpz_lcm(lv, av, bv);
        out = number_from_mpz_value(lv);
    }
    mpz_clear(lv);
    mpz_clear(bv);
    mpz_clear(av);
    return out;
}

number_t num_mod(const number_t number, const number_t modulus)
{
    mpz_srcptr nd = number_mpz_src_if_mpz(&number);
    mpz_srcptr md = number_mpz_src_if_mpz(&modulus);
    mpz_t nv;
    mpz_t mv;
    mpz_t rv;
    number_t out = NUM_NAN;

    if (nd && md) {
        number_mpz_t *result;

        if (mpz_sgn(md) == 0)
            return NUM_NAN;
        result = number_mpz_new();
        if (!result)
            return NUM_NAN;
        mpz_tdiv_r(result->value, nd, md);
        return number_take_mpz(result);
    }

    mpz_init(nv);
    mpz_init(mv);
    mpz_init(rv);
    if (number_get_exact_integer_mpz(number, nv) &&
        number_get_exact_integer_mpz(modulus, mv) &&
        mpz_sgn(mv) != 0) {
        mpz_tdiv_r(rv, nv, mv);
        out = number_from_mpz_value(rv);
    }
    mpz_clear(rv);
    mpz_clear(mv);
    mpz_clear(nv);
    return out;
}

int num_divmod(const number_t number,
               const number_t divisor,
               number_t *quotient,
               number_t *remainder)
{
    mpz_srcptr nd = number_mpz_src_if_mpz(&number);
    mpz_srcptr dd = number_mpz_src_if_mpz(&divisor);
    mpz_t nv;
    mpz_t dv;
    mpz_t qv;
    mpz_t rv;
    int rc = -1;

    if (!quotient || !remainder || quotient == remainder)
        return -1;
    if (nd && dd) {
        number_mpz_t *q;
        number_mpz_t *r;

        if (mpz_sgn(dd) == 0)
            return -1;
        q = number_mpz_new();
        r = number_mpz_new();
        if (!q || !r) {
            number_mpz_free(q);
            number_mpz_free(r);
            return -1;
        }
        mpz_tdiv_qr(q->value, r->value, nd, dd);
        number_assign_take_mpz(quotient, q);
        number_assign_take_mpz(remainder, r);
        return 0;
    }

    mpz_init(nv);
    mpz_init(dv);
    mpz_init(qv);
    mpz_init(rv);
    if (number_get_exact_integer_mpz(number, nv) &&
        number_get_exact_integer_mpz(divisor, dv) &&
        mpz_sgn(dv) != 0) {
        number_t q;
        number_t r;

        mpz_tdiv_qr(qv, rv, nv, dv);
        q = number_from_mpz_value(qv);
        r = number_from_mpz_value(rv);
        if (!num_is_nan(q) && !num_is_nan(r)) {
            number_assign(quotient, q);
            number_assign(remainder, r);
            rc = 0;
        } else {
            num_destroy(&q);
            num_destroy(&r);
        }
    }
    mpz_clear(rv);
    mpz_clear(qv);
    mpz_clear(dv);
    mpz_clear(nv);
    return rc;
}

int num_gcdext(const number_t a,
               const number_t b,
               number_t *gcd_out,
               number_t *x_out,
               number_t *y_out)
{
    mpz_t av;
    mpz_t bv;
    mpz_t gv;
    mpz_t xv;
    mpz_t yv;
    int rc = -1;

    mpz_init(av);
    mpz_init(bv);
    mpz_init(gv);
    mpz_init(xv);
    mpz_init(yv);
    if (number_get_exact_integer_mpz(a, av) &&
        number_get_exact_integer_mpz(b, bv)) {
        number_t g;
        number_t x;
        number_t y;

        mpz_gcdext(gv, xv, yv, av, bv);
        g = number_from_mpz_value(gv);
        x = number_from_mpz_value(xv);
        y = number_from_mpz_value(yv);
        if (!num_is_nan(g) && !num_is_nan(x) && !num_is_nan(y)) {
            if (gcd_out)
                number_assign(gcd_out, g);
            else
                num_destroy(&g);
            if (x_out)
                number_assign(x_out, x);
            else
                num_destroy(&x);
            if (y_out)
                number_assign(y_out, y);
            else
                num_destroy(&y);
            rc = 0;
        } else {
            num_destroy(&g);
            num_destroy(&x);
            num_destroy(&y);
        }
    }
    mpz_clear(yv);
    mpz_clear(xv);
    mpz_clear(gv);
    mpz_clear(bv);
    mpz_clear(av);
    return rc;
}

number_t num_powmod(const number_t base,
                    const number_t exponent,
                    const number_t modulus)
{
    mpz_srcptr bd = number_mpz_src_if_mpz(&base);
    mpz_srcptr ed = number_mpz_src_if_mpz(&exponent);
    mpz_srcptr md = number_mpz_src_if_mpz(&modulus);
    mpz_t bv;
    mpz_t ev;
    mpz_t mv;
    mpz_t outv;
    number_t out = NUM_NAN;

    if (bd && ed && md) {
        number_mpz_t *result;

        if (mpz_sgn(ed) < 0 || mpz_sgn(md) <= 0)
            return NUM_NAN;
        result = number_mpz_new();
        if (!result)
            return NUM_NAN;
        mpz_powm(result->value, bd, ed, md);
        return number_take_mpz(result);
    }

    mpz_init(bv);
    mpz_init(ev);
    mpz_init(mv);
    mpz_init(outv);
    if (number_get_exact_integer_mpz(base, bv) &&
        number_get_exact_integer_mpz(exponent, ev) &&
        number_get_exact_integer_mpz(modulus, mv) &&
        mpz_sgn(ev) >= 0 && mpz_sgn(mv) > 0) {
        mpz_powm(outv, bv, ev, mv);
        out = number_from_mpz_value(outv);
    }
    mpz_clear(outv);
    mpz_clear(mv);
    mpz_clear(ev);
    mpz_clear(bv);
    return out;
}

number_t num_modinv(const number_t number, const number_t modulus)
{
    mpz_t nv;
    mpz_t mv;
    mpz_t outv;
    number_t out = NUM_NAN;

    mpz_init(nv);
    mpz_init(mv);
    mpz_init(outv);
    if (number_get_exact_integer_mpz(number, nv) &&
        number_get_exact_integer_mpz(modulus, mv) &&
        mpz_sgn(mv) > 0 &&
        mpz_invert(outv, nv, mv) != 0) {
        out = number_from_mpz_value(outv);
    }
    mpz_clear(outv);
    mpz_clear(mv);
    mpz_clear(nv);
    return out;
}

bool num_is_prime(const number_t number)
{
    mpz_t value;
    bool rc = false;

    mpz_init(value);
    if (number_get_exact_integer_mpz(number, value) && mpz_sgn(value) > 0)
        rc = mpz_probab_prime_p(value, 25) > 0;
    mpz_clear(value);
    return rc;
}

number_primality_t num_prove_prime(const number_t number)
{
    mpz_t value;
    number_primality_t rc = NUMBER_PRIMALITY_UNKNOWN;

    mpz_init(value);
    if (number_get_exact_integer_mpz(number, value)) {
        if (mpz_sgn(value) <= 0)
            rc = NUMBER_PRIMALITY_COMPOSITE;
        else
            rc = mpz_probab_prime_p(value, 50) == 0
                ? NUMBER_PRIMALITY_COMPOSITE : NUMBER_PRIMALITY_PRIME;
    }
    mpz_clear(value);
    return rc;
}

number_t num_next_prime(const number_t number)
{
    mpz_t value;
    number_t out = NUM_NAN;

    mpz_init(value);
    if (number_get_exact_integer_mpz(number, value)) {
        if (mpz_cmp_ui(value, 2u) < 0)
            mpz_set_ui(value, 2u);
        else
            mpz_nextprime(value, value);
        out = number_from_mpz_value(value);
    }
    mpz_clear(value);
    return out;
}

number_t num_prev_prime(const number_t number)
{
    mpz_t value;
    number_t out = NUM_NAN;

    mpz_init(value);
    if (number_get_exact_integer_mpz(number, value) &&
        mpz_cmp_ui(value, 2u) >= 0) {
        if (mpz_cmp_ui(value, 2u) != 0) {
            mpz_sub_ui(value, value, 1u);
            while (mpz_cmp_ui(value, 2u) >= 0 &&
                   mpz_probab_prime_p(value, 25) == 0)
                mpz_sub_ui(value, value, 1u);
        }
        if (mpz_cmp_ui(value, 2u) >= 0)
            out = number_from_mpz_value(value);
    }
    mpz_clear(value);
    return out;
}

static int number_factors_append(number_factors_t *factors,
                                 mpz_srcptr prime,
                                 unsigned long exponent)
{
    number_factor_t *grown;
    number_t prime_number;

    if (!factors || !prime || exponent == 0u)
        return -1;
    prime_number = number_from_mpz_value(prime);
    if (num_is_nan(prime_number))
        return -1;
    grown = realloc(factors->items, (factors->count + 1u) * sizeof(*grown));
    if (!grown) {
        num_destroy(&prime_number);
        return -1;
    }
    factors->items = grown;
    factors->items[factors->count].prime = prime_number;
    factors->items[factors->count].exponent = exponent;
    factors->count++;
    return 0;
}

number_factors_t *num_factors(const number_t number)
{
    number_factors_t *factors;
    mpz_t work;
    mpz_t prime;
    mpz_t quotient;
    mpz_t remainder;

    mpz_init(work);
    if (!number_get_exact_integer_mpz(number, work) || mpz_sgn(work) <= 0) {
        mpz_clear(work);
        return NULL;
    }

    factors = calloc(1u, sizeof(*factors));
    if (!factors) {
        mpz_clear(work);
        return NULL;
    }

    mpz_init_set_ui(prime, 2u);
    mpz_init(quotient);
    mpz_init(remainder);
    while (mpz_cmp_ui(work, 1u) > 0) {
        unsigned long exponent = 0u;

        if (mpz_probab_prime_p(work, 25) > 0) {
            if (number_factors_append(factors, work, 1u) != 0) {
                num_factors_free(factors);
                factors = NULL;
            }
            break;
        }

        for (;;) {
            mpz_tdiv_qr(quotient, remainder, work, prime);
            if (mpz_sgn(remainder) != 0)
                break;
            exponent++;
            mpz_set(work, quotient);
        }
        if (exponent > 0u &&
            number_factors_append(factors, prime, exponent) != 0) {
            num_factors_free(factors);
            factors = NULL;
            break;
        }

        if (mpz_cmp_ui(prime, 2u) == 0)
            mpz_set_ui(prime, 3u);
        else
            mpz_nextprime(prime, prime);
    }
    mpz_clear(remainder);
    mpz_clear(quotient);
    mpz_clear(prime);
    mpz_clear(work);
    return factors;
}

void num_factors_free(number_factors_t *factors)
{
    if (!factors)
        return;
    for (size_t i = 0u; i < factors->count; ++i)
        num_destroy(&factors->items[i].prime);
    free(factors->items);
    free(factors);
}

size_t num_bit_length(const number_t number)
{
    mpz_t value;
    size_t bits = 0u;

    mpz_init(value);
    if (number_get_exact_integer_mpz(number, value) && mpz_sgn(value) != 0)
        bits = mpz_sizeinbase(value, 2);
    mpz_clear(value);
    return bits;
}

bool num_test_bit(const number_t number, size_t bit_index)
{
    mpz_t value;
    bool rc = false;

    mpz_init(value);
    if (number_get_exact_integer_mpz(number, value))
        rc = mpz_tstbit(value, (mp_bitcnt_t)bit_index) != 0;
    mpz_clear(value);
    return rc;
}

number_t num_set_bit(const number_t number, size_t bit_index)
{
    mpz_t value;
    number_t out = NUM_NAN;

    mpz_init(value);
    if (number_get_exact_integer_mpz(number, value)) {
        mpz_setbit(value, (mp_bitcnt_t)bit_index);
        out = number_from_mpz_value(value);
    }
    mpz_clear(value);
    return out;
}

number_t num_clear_bit(const number_t number, size_t bit_index)
{
    mpz_t value;
    number_t out = NUM_NAN;

    mpz_init(value);
    if (number_get_exact_integer_mpz(number, value)) {
        mpz_clrbit(value, (mp_bitcnt_t)bit_index);
        out = number_from_mpz_value(value);
    }
    mpz_clear(value);
    return out;
}

number_t num_bit_not(const number_t number)
{
    mpz_t value;
    mpz_t mask;
    number_t out = NUM_NAN;

    mpz_init(value);
    mpz_init(mask);
    if (number_get_exact_integer_mpz(number, value) && mpz_sgn(value) >= 0) {
        mp_bitcnt_t bits = mpz_sizeinbase(value, 2);

        if (mpz_sgn(value) == 0) {
            mpz_set_ui(value, 1u);
        } else {
            mpz_set_ui(mask, 1u);
            mpz_mul_2exp(mask, mask, bits);
            mpz_sub_ui(mask, mask, 1u);
            mpz_xor(value, value, mask);
        }
        out = number_from_mpz_value(value);
    }
    mpz_clear(mask);
    mpz_clear(value);
    return out;
}

static number_t number_apply_mpz_binary(const number_t a,
                                        const number_t b,
                                        void (*op)(mpz_ptr, mpz_srcptr, mpz_srcptr))
{
    mpz_t av;
    mpz_t bv;
    mpz_t outv;
    number_t out = NUM_NAN;

    mpz_init(av);
    mpz_init(bv);
    mpz_init(outv);
    if (op && number_get_exact_integer_mpz(a, av) &&
        number_get_exact_integer_mpz(b, bv)) {
        op(outv, av, bv);
        out = number_from_mpz_value(outv);
    }
    mpz_clear(outv);
    mpz_clear(bv);
    mpz_clear(av);
    return out;
}

number_t num_bit_and(const number_t a, const number_t b)
{
    return number_apply_mpz_binary(a, b, mpz_and);
}

number_t num_bit_or(const number_t a, const number_t b)
{
    return number_apply_mpz_binary(a, b, mpz_ior);
}

number_t num_bit_xor(const number_t a, const number_t b)
{
    return number_apply_mpz_binary(a, b, mpz_xor);
}

number_t num_shl(const number_t number, long bits)
{
    mpz_t value;
    number_t out = NUM_NAN;

    if (bits < 0)
        return num_shr(number, -bits);
    mpz_init(value);
    if (number_get_exact_integer_mpz(number, value)) {
        mpz_mul_2exp(value, value, (mp_bitcnt_t)bits);
        out = number_from_mpz_value(value);
    }
    mpz_clear(value);
    return out;
}

number_t num_shr(const number_t number, long bits)
{
    mpz_t value;
    number_t out = NUM_NAN;

    if (bits < 0)
        return num_shl(number, -bits);
    mpz_init(value);
    if (number_get_exact_integer_mpz(number, value)) {
        mpz_tdiv_q_2exp(value, value, (mp_bitcnt_t)bits);
        out = number_from_mpz_value(value);
    }
    mpz_clear(value);
    return out;
}

number_t num_hypot(const number_t a, const number_t b)
{
    return number_apply_binary_math_with_double(a, b, hypot, qf_hypot,
        qc_hypot, number_mpfr_hypot_mut, NULL);
}

static int number_try_get_pure_imag(const number_t number,
                                    number_t *imag_out)
{
    NUM_SCOPE(scope);
    number_t real;

    if (num_is_real(number))
        return 0;
    real = num_real_part(number);
    if (!num_is_zero(real)) {
        return 0;
    }
    *imag_out = num_scope_detach(num_imag_part(number));
    return 1;
}

static const number_angle_fastpath_t number_sin_fastpaths[] = {
    { &NUM_ZERO, NUMBER_CONST_ZERO, 1, 0 },
    { &NUM_PI, NUMBER_CONST_ZERO, 1, 0 },
    { &NUM_2PI, NUMBER_CONST_ZERO, 1, 0 },
    { &NUM_PI_6, NUMBER_CONST_HALF, 1, 0 },
    { &NUM_PI_4, NUMBER_CONST_SQRT2_OVER_TWO, 1, 0 },
    { &NUM_PI_3, NUMBER_CONST_SQRT3_OVER_TWO, 1, 0 },
    { &NUM_PI_2, NUMBER_CONST_ONE, 1, 0 },
    { &NUM_3PI_4, NUMBER_CONST_SQRT2_OVER_TWO, 1, 0 }
};

static const number_angle_fastpath_t number_cos_fastpaths[] = {
    { &NUM_ZERO, NUMBER_CONST_ONE, 1, 0 },
    { &NUM_2PI, NUMBER_CONST_ONE, 1, 0 },
    { &NUM_PI_6, NUMBER_CONST_SQRT3_OVER_TWO, 1, 0 },
    { &NUM_PI_4, NUMBER_CONST_SQRT2_OVER_TWO, 1, 0 },
    { &NUM_PI_3, NUMBER_CONST_HALF, 1, 0 },
    { &NUM_PI_2, NUMBER_CONST_ZERO, 1, 0 },
    { &NUM_3PI_4, NUMBER_CONST_SQRT2_OVER_TWO, -1, 0 },
    { &NUM_PI, NUMBER_CONST_NEG_ONE, 1, 0 }
};

static const number_angle_pair_fastpath_t number_sincos_fastpaths[] = {
    { &NUM_ZERO,   NUMBER_CONST_ZERO,           1, 0, NUMBER_CONST_ONE,            1, 0 },
    { &NUM_PI,     NUMBER_CONST_ZERO,           1, 0, NUMBER_CONST_NEG_ONE,        1, 0 },
    { &NUM_2PI,    NUMBER_CONST_ZERO,           1, 0, NUMBER_CONST_ONE,            1, 0 },
    { &NUM_PI_6,   NUMBER_CONST_HALF,           1, 0, NUMBER_CONST_SQRT3_OVER_TWO, 1, 0 },
    { &NUM_PI_4,   NUMBER_CONST_SQRT2_OVER_TWO, 1, 0,
                   NUMBER_CONST_SQRT2_OVER_TWO, 1, 0 },
    { &NUM_PI_3,   NUMBER_CONST_SQRT3_OVER_TWO, 1, 0, NUMBER_CONST_HALF,           1, 0 },
    { &NUM_PI_2,   NUMBER_CONST_ONE,            1, 0, NUMBER_CONST_ZERO,           1, 0 },
    { &NUM_3PI_4,  NUMBER_CONST_SQRT2_OVER_TWO, 1, 0,
                   NUMBER_CONST_SQRT2_OVER_TWO, -1, 0 }
};

static const number_angle_fastpath_t number_tanh_imag_fastpaths[] = {
    { &NUM_ZERO, NUMBER_CONST_ZERO, 1, 0 },
    { &NUM_PI, NUMBER_CONST_ZERO, 1, 0 },
    { &NUM_2PI, NUMBER_CONST_ZERO, 1, 0 },
    { &NUM_PI_4, NUMBER_CONST_I, 1, 0 },
    { &NUM_PI_3, NUMBER_CONST_SQRT3, 1, 1 },
    { &NUM_3PI_4, NUMBER_CONST_I, -1, 0 }
};

static const number_angle_fastpath_t number_sinh_imag_fastpaths[] = {
    { &NUM_ZERO, NUMBER_CONST_ZERO, 1, 0 },
    { &NUM_PI, NUMBER_CONST_ZERO, 1, 0 },
    { &NUM_2PI, NUMBER_CONST_ZERO, 1, 0 },
    { &NUM_PI_6, NUMBER_CONST_HALF, 1, 1 },
    { &NUM_PI_4, NUMBER_CONST_SQRT2_OVER_TWO, 1, 1 },
    { &NUM_PI_3, NUMBER_CONST_SQRT3_OVER_TWO, 1, 1 },
    { &NUM_PI_2, NUMBER_CONST_I, 1, 0 },
    { &NUM_3PI_4, NUMBER_CONST_SQRT2_OVER_TWO, 1, 1 }
};

static const number_angle_fastpath_t number_cosh_imag_fastpaths[] = {
    { &NUM_ZERO, NUMBER_CONST_ONE, 1, 0 },
    { &NUM_2PI, NUMBER_CONST_ONE, 1, 0 },
    { &NUM_PI_6, NUMBER_CONST_SQRT3_OVER_TWO, 1, 0 },
    { &NUM_PI_4, NUMBER_CONST_SQRT2_OVER_TWO, 1, 0 },
    { &NUM_PI_3, NUMBER_CONST_HALF, 1, 0 },
    { &NUM_PI_2, NUMBER_CONST_ZERO, 1, 0 },
    { &NUM_3PI_4, NUMBER_CONST_SQRT2_OVER_TWO, -1, 0 },
    { &NUM_PI, NUMBER_CONST_NEG_ONE, 1, 0 }
};

static const number_angle_pair_fastpath_t number_sinhcosh_fastpaths[] = {
    { &NUM_ZERO,   NUMBER_CONST_ZERO,           1, 0, NUMBER_CONST_ONE,            1, 0 },
    { &NUM_PI_6,   NUMBER_CONST_HALF,           1, 1, NUMBER_CONST_SQRT3_OVER_TWO, 1, 0 },
    { &NUM_PI_4,   NUMBER_CONST_SQRT2_OVER_TWO, 1, 1,
                   NUMBER_CONST_SQRT2_OVER_TWO, 1, 0 },
    { &NUM_PI_3,   NUMBER_CONST_SQRT3_OVER_TWO, 1, 1, NUMBER_CONST_HALF,           1, 0 },
    { &NUM_PI_2,   NUMBER_CONST_ONE,            1, 1, NUMBER_CONST_ZERO,           1, 0 },
    { &NUM_3PI_4,  NUMBER_CONST_SQRT2_OVER_TWO, 1, 1,
                   NUMBER_CONST_SQRT2_OVER_TWO, -1, 0 },
    { &NUM_PI,     NUMBER_CONST_ZERO,           1, 0, NUMBER_CONST_NEG_ONE,        1, 0 },
    { &NUM_2PI,    NUMBER_CONST_ZERO,           1, 0, NUMBER_CONST_ONE,            1, 0 }
};

static const number_tan_fastpath_t number_tan_fastpaths[] = {
    { &NUM_ZERO,     NUMBER_CONST_ZERO,      NUMBER_CONST_ZERO,  1, 0 },
    { &NUM_PI,       NUMBER_CONST_PI,        NUMBER_CONST_ZERO,  1, 0 },
    { &NUM_2PI,      NUMBER_CONST_2PI,       NUMBER_CONST_ZERO,  1, 0 },
    { &NUM_PI_2,     NUMBER_CONST_PI_2,      NUMBER_CONST_INF,   1, 0 },
    { &NUM_NEG_PI_2, NUMBER_CONST_NEG_PI_2,  NUMBER_CONST_INF,  -1, 0 },
    { &NUM_PI_6,     NUMBER_CONST_PI_6,      NUMBER_CONST_SQRT3, 1, 1 },
    { &NUM_PI_4,     NUMBER_CONST_PI_4,      NUMBER_CONST_ONE,   1, 0 },
    { &NUM_PI_3,     NUMBER_CONST_PI_3,      NUMBER_CONST_SQRT3, 1, 0 },
    { &NUM_3PI_4,    NUMBER_CONST_3PI_4,     NUMBER_CONST_ONE,  -1, 0 }
};

int num_sincos(const number_t x, number_t *sin_out, number_t *cos_out)
{
    const number_vtable_t *vt = number_vt(&x);

    if (!sin_out || !cos_out || !number_is_valid_value(&x))
        return -1;
    if (number_trig_real_pair_fastpath(&x, number_sincos_fastpaths,
            sizeof(number_sincos_fastpaths) / sizeof(number_sincos_fastpaths[0]),
            sin_out, cos_out))
        return 0;
    if (number_is_plain_mpfr_value(&x) && vt && vt->sincos_value)
        return vt->sincos_value(&x, sin_out, cos_out);
    if (vt && vt->sincos_value)
        return vt->sincos_value(&x, sin_out, cos_out);
    return -1;
}

number_t num_sin(const number_t number)
{
    number_t out;

    if (number_is_cdouble_value(&number))
        return number_apply_cdouble_unary(number, csin);
    if (number_is_plain_inexact_value(&number))
        return number_apply_nonreal_complex_unary_or_dispatch(number, sin,
            qf_sin, qc_sin, number_mpfr_sin_mut, mpc_sin);
    if (num_is_real(number) &&
        number_trig_real_fastpath(&number, number_sin_fastpaths,
            sizeof(number_sin_fastpaths) / sizeof(number_sin_fastpaths[0]), &out))
        return out;
    return number_apply_nonreal_complex_unary_or_dispatch(number, sin, qf_sin, qc_sin, number_mpfr_sin_mut, mpc_sin);
}

number_t num_cos(const number_t number)
{
    number_t out;

    if (number_is_cdouble_value(&number))
        return number_apply_cdouble_unary(number, ccos);
    if (number_is_plain_inexact_value(&number))
        return number_apply_nonreal_complex_unary_or_dispatch(number, cos,
            qf_cos, qc_cos, number_mpfr_cos_mut, mpc_cos);
    if (num_is_real(number) &&
        number_trig_real_fastpath(&number, number_cos_fastpaths,
            sizeof(number_cos_fastpaths) / sizeof(number_cos_fastpaths[0]), &out))
        return out;
    return number_apply_nonreal_complex_unary_or_dispatch(number, cos, qf_cos, qc_cos, number_mpfr_cos_mut, mpc_cos);
}

number_t num_tan(const number_t number)
{
    number_t out;

    if (number_is_cdouble_value(&number))
        return number_apply_cdouble_unary(number, ctan);
    if (num_is_real(number)) {
        if (number_tan_fastpath_by_const_id(&number, number_tan_fastpaths,
                sizeof(number_tan_fastpaths) / sizeof(number_tan_fastpaths[0]), &out))
            return out;
        if (number_tan_fastpath_by_value(&number, number_tan_fastpaths,
                sizeof(number_tan_fastpaths) / sizeof(number_tan_fastpaths[0]), &out))
            return out;
    }
    if (number_is_plain_inexact_value(&number))
        return number_apply_nonreal_complex_unary_or_dispatch(number, tan,
            qf_tan, qc_tan, number_mpfr_tan_mut, mpc_tan);
    return number_apply_nonreal_complex_unary_or_dispatch(number, tan, qf_tan, qc_tan, number_mpfr_tan_mut, mpc_tan);
}

number_t num_sec(const number_t number)
{
    if (number_is_cdouble_value(&number))
        return number_apply_cdouble_unary(number, number_cdouble_sec);
    return number_apply_nonreal_complex_unary_or_dispatch(number,
        number_double_sec, qf_sec, qc_sec, number_mpfr_sec_mut, number_mpc_sec);
}

number_t num_cosec(const number_t number)
{
    if (number_is_cdouble_value(&number))
        return number_apply_cdouble_unary(number, number_cdouble_cosec);
    return number_apply_nonreal_complex_unary_or_dispatch(number,
        number_double_cosec, qf_cosec, qc_cosec, number_mpfr_cosec_mut,
        number_mpc_cosec);
}

number_t num_cot(const number_t number)
{
    if (number_is_cdouble_value(&number))
        return number_apply_cdouble_unary(number, number_cdouble_cot);
    return number_apply_nonreal_complex_unary_or_dispatch(number,
        number_double_cot, qf_cot, qc_cot, number_mpfr_cot_mut, number_mpc_cot);
}

number_t num_atan(const number_t number)
{
    if (number_is_cdouble_value(&number))
        return number_apply_cdouble_unary(number, catan);
    return number_apply_nonreal_complex_unary_or_dispatch(number, atan, qf_atan, qc_atan, number_mpfr_atan_mut, mpc_atan);
}

number_t num_asec(const number_t number)
{
    number_kind_t kind = number_kind_value(&number);
    double d;
    qfloat_t qf;

    if (kind == NUMBER_DOUBLE) {
        d = number_impl_const(&number)->value.d;
        return fabs(d) < 1.0
            ? number_double_cdouble_unary(d, number_cdouble_asec)
            : num_create_from_double(number_double_asec(d));
    }
    if (kind == NUMBER_CDOUBLE)
        return number_apply_cdouble_unary(number, number_cdouble_asec);
    if (kind == NUMBER_QFLOAT) {
        qf = number_impl_const(&number)->value.qf;
        return qf_lt(qf_abs(qf), QF_ONE)
            ? number_qfloat_qcomplex_unary(qf, qc_asec)
            : num_create_from_qfloat(qf_asec(qf));
    }
    return number_apply_nonreal_complex_unary_or_dispatch(number,
        number_double_asec, qf_asec, qc_asec, number_mpfr_asec_mut,
        number_mpc_asec);
}

number_t num_acosec(const number_t number)
{
    number_kind_t kind = number_kind_value(&number);
    double d;
    qfloat_t qf;

    if (kind == NUMBER_DOUBLE) {
        d = number_impl_const(&number)->value.d;
        return fabs(d) < 1.0
            ? number_double_cdouble_unary(d, number_cdouble_acosec)
            : num_create_from_double(number_double_acosec(d));
    }
    if (kind == NUMBER_CDOUBLE)
        return number_apply_cdouble_unary(number, number_cdouble_acosec);
    if (kind == NUMBER_QFLOAT) {
        qf = number_impl_const(&number)->value.qf;
        return qf_lt(qf_abs(qf), QF_ONE)
            ? number_qfloat_qcomplex_unary(qf, qc_acosec)
            : num_create_from_qfloat(qf_acosec(qf));
    }
    return number_apply_nonreal_complex_unary_or_dispatch(number,
        number_double_acosec, qf_acosec, qc_acosec, number_mpfr_acosec_mut,
        number_mpc_acosec);
}

number_t num_acot(const number_t number)
{
    if (number_is_cdouble_value(&number))
        return number_apply_cdouble_unary(number, number_cdouble_acot);
    return number_apply_nonreal_complex_unary_or_dispatch(number,
        number_double_acot, qf_acot, qc_acot, number_mpfr_acot_mut,
        number_mpc_acot);
}

number_t num_atan2(const number_t y, const number_t x)
{
    if (number_kind_value(&y) == NUMBER_MPFR &&
        number_kind_value(&x) == NUMBER_MPFR)
        return number_apply_binary_same_mpfr(&y, &x, number_mpfr_atan2_mut);
    return number_apply_binary_math_with_double(y, x, atan2, qf_atan2, qc_atan2, number_mpfr_atan2_mut, NULL);
}

number_t num_asin(const number_t number)
{
    number_kind_t kind = number_kind_value(&number);
    double d;
    qfloat_t qf;

    if (kind == NUMBER_DOUBLE) {
        d = number_impl_const(&number)->value.d;
        return d < -1.0 || d > 1.0
            ? number_double_cdouble_unary(d, casin)
            : num_create_from_double(asin(d));
    }
    if (kind == NUMBER_CDOUBLE)
        return number_apply_cdouble_unary(number, casin);
    if (kind == NUMBER_QFLOAT) {
        qf = number_impl_const(&number)->value.qf;
        return qf_lt(qf, qf_from_double(-1.0)) || qf_gt(qf, qf_from_double(1.0))
            ? number_qfloat_qcomplex_unary(qf, qc_asin)
            : num_create_from_qfloat(qf_asin(qf));
    }
    return number_apply_nonreal_complex_unary_or_dispatch(number, asin, qf_asin, qc_asin, number_mpfr_asin_mut, mpc_asin);
}

number_t num_acos(const number_t number)
{
    number_kind_t kind = number_kind_value(&number);
    double d;
    qfloat_t qf;

    if (kind == NUMBER_DOUBLE) {
        d = number_impl_const(&number)->value.d;
        return d < -1.0 || d > 1.0
            ? number_double_cdouble_unary(d, cacos)
            : num_create_from_double(acos(d));
    }
    if (kind == NUMBER_CDOUBLE)
        return number_apply_cdouble_unary(number, cacos);
    if (kind == NUMBER_QFLOAT) {
        qf = number_impl_const(&number)->value.qf;
        return qf_lt(qf, qf_from_double(-1.0)) || qf_gt(qf, qf_from_double(1.0))
            ? number_qfloat_qcomplex_unary(qf, qc_acos)
            : num_create_from_qfloat(qf_acos(qf));
    }
    return number_apply_nonreal_complex_unary_or_dispatch(number, acos, qf_acos, qc_acos, number_mpfr_acos_mut, mpc_acos);
}

number_t num_sinh(const number_t number)
{
    number_t out;

    if (number_is_cdouble_value(&number))
        return number_apply_cdouble_unary(number, csinh);
    if (number_is_plain_inexact_value(&number))
        return number_apply_nonreal_complex_unary_or_dispatch(number, sinh,
            qf_sinh, qc_sinh, number_mpfr_sinh_mut, mpc_sinh);
    if (number_hyperbolic_imag_fastpath(&number, number_sinh_imag_fastpaths,
            sizeof(number_sinh_imag_fastpaths) / sizeof(number_sinh_imag_fastpaths[0]), &out))
        return out;
    return number_apply_nonreal_complex_unary_or_dispatch(number, sinh, qf_sinh, qc_sinh, number_mpfr_sinh_mut, mpc_sinh);
}

number_t num_cosh(const number_t number)
{
    number_t out;

    if (number_is_cdouble_value(&number))
        return number_apply_cdouble_unary(number, ccosh);
    if (number_is_plain_inexact_value(&number))
        return number_apply_nonreal_complex_unary_or_dispatch(number, cosh,
            qf_cosh, qc_cosh, number_mpfr_cosh_mut, mpc_cosh);
    if (number_hyperbolic_imag_fastpath(&number, number_cosh_imag_fastpaths,
            sizeof(number_cosh_imag_fastpaths) / sizeof(number_cosh_imag_fastpaths[0]), &out))
        return out;
    return number_apply_nonreal_complex_unary_or_dispatch(number, cosh, qf_cosh, qc_cosh, number_mpfr_cosh_mut, mpc_cosh);
}

int num_sinhcosh(const number_t x, number_t *sinh_out, number_t *cosh_out)
{
    const number_vtable_t *vt = number_vt(&x);

    if (!sinh_out || !cosh_out || !number_is_valid_value(&x))
        return -1;
    if (number_is_plain_mpfr_value(&x) && vt && vt->sinhcosh_value)
        return vt->sinhcosh_value(&x, sinh_out, cosh_out);
    if (number_hyperbolic_imag_pair_fastpath(&x, number_sinhcosh_fastpaths,
            sizeof(number_sinhcosh_fastpaths) / sizeof(number_sinhcosh_fastpaths[0]),
            sinh_out, cosh_out))
        return 0;
    if (vt && vt->sinhcosh_value)
        return vt->sinhcosh_value(&x, sinh_out, cosh_out);
    return -1;
}

number_t num_tanh(const number_t number)
{
    number_t imag;
    number_t out;

    if (number_is_cdouble_value(&number))
        return number_apply_cdouble_unary(number, ctanh);
    if (number_is_plain_inexact_value(&number))
        return number_apply_nonreal_complex_unary_or_dispatch(number, tanh,
            qf_tanh, qc_tanh, number_mpfr_tanh_mut, mpc_tanh);
    if (number_hyperbolic_imag_fastpath(&number, number_tanh_imag_fastpaths,
            sizeof(number_tanh_imag_fastpaths) / sizeof(number_tanh_imag_fastpaths[0]), &out))
        return out;
    if (number_try_get_pure_imag(number, &imag)) {
        if (number_matches_value(&imag, &NUM_PI_6)) {
            number_t sqrt3 = number_const_like(&number, NUMBER_CONST_SQRT3);
            number_t inv = num_div(NUM_ONE, sqrt3);
            number_t imag_unit = number_const_like(&number, NUMBER_CONST_I);

            out = num_mul(imag_unit, inv);
            num_destroy(&sqrt3);
            num_destroy(&inv);
            num_destroy(&imag_unit);
            num_destroy(&imag);
            return out;
        }
        num_destroy(&imag);
    }
    return number_apply_nonreal_complex_unary_or_dispatch(number, tanh, qf_tanh, qc_tanh, number_mpfr_tanh_mut, mpc_tanh);
}

number_t num_sech(const number_t number)
{
    if (number_is_cdouble_value(&number))
        return number_apply_cdouble_unary(number, number_cdouble_sech);
    return number_apply_nonreal_complex_unary_or_dispatch(number,
        number_double_sech, qf_sech, qc_sech, number_mpfr_sech_mut,
        number_mpc_sech);
}

number_t num_cosech(const number_t number)
{
    if (number_is_cdouble_value(&number))
        return number_apply_cdouble_unary(number, number_cdouble_cosech);
    return number_apply_nonreal_complex_unary_or_dispatch(number,
        number_double_cosech, qf_cosech, qc_cosech, number_mpfr_cosech_mut,
        number_mpc_cosech);
}

number_t num_coth(const number_t number)
{
    if (number_is_cdouble_value(&number))
        return number_apply_cdouble_unary(number, number_cdouble_coth);
    return number_apply_nonreal_complex_unary_or_dispatch(number,
        number_double_coth, qf_coth, qc_coth, number_mpfr_coth_mut,
        number_mpc_coth);
}

number_t num_asinh(const number_t number)
{
    if (number_is_cdouble_value(&number))
        return number_apply_cdouble_unary(number, casinh);
    return number_apply_nonreal_complex_unary_or_dispatch(number, asinh, qf_asinh, qc_asinh, number_mpfr_asinh_mut, mpc_asinh);
}

number_t num_acosh(const number_t number)
{
    number_kind_t kind = number_kind_value(&number);
    double d;
    qfloat_t qf;

    if (kind == NUMBER_DOUBLE) {
        d = number_impl_const(&number)->value.d;
        return d < 1.0 ? number_double_cdouble_unary(d, cacosh)
                       : num_create_from_double(acosh(d));
    }
    if (kind == NUMBER_CDOUBLE)
        return number_apply_cdouble_unary(number, cacosh);
    if (kind == NUMBER_QFLOAT) {
        qf = number_impl_const(&number)->value.qf;
        return qf_lt(qf, qf_from_double(1.0))
            ? number_qfloat_qcomplex_unary(qf, qc_acosh)
            : num_create_from_qfloat(qf_acosh(qf));
    }
    return number_apply_nonreal_complex_unary_or_dispatch(number, acosh, qf_acosh, qc_acosh, number_mpfr_acosh_mut, mpc_acosh);
}

number_t num_atanh(const number_t number)
{
    number_kind_t kind = number_kind_value(&number);
    double d;
    qfloat_t qf;

    if (kind == NUMBER_DOUBLE) {
        d = number_impl_const(&number)->value.d;
        return d < -1.0 || d > 1.0
            ? number_double_cdouble_unary(d, catanh)
            : num_create_from_double(atanh(d));
    }
    if (kind == NUMBER_CDOUBLE)
        return number_apply_cdouble_unary(number, catanh);
    if (kind == NUMBER_QFLOAT) {
        qf = number_impl_const(&number)->value.qf;
        return qf_lt(qf, qf_from_double(-1.0)) || qf_gt(qf, qf_from_double(1.0))
            ? number_qfloat_qcomplex_unary(qf, qc_atanh)
            : num_create_from_qfloat(qf_atanh(qf));
    }
    return number_apply_nonreal_complex_unary_or_dispatch(number, atanh, qf_atanh, qc_atanh, number_mpfr_atanh_mut, mpc_atanh);
}

number_t num_asech(const number_t number)
{
    number_kind_t kind = number_kind_value(&number);
    double d;
    qfloat_t qf;

    if (kind == NUMBER_DOUBLE) {
        d = number_impl_const(&number)->value.d;
        return d <= 0.0 || d > 1.0
            ? number_double_cdouble_unary(d, number_cdouble_asech)
            : num_create_from_double(number_double_asech(d));
    }
    if (kind == NUMBER_CDOUBLE)
        return number_apply_cdouble_unary(number, number_cdouble_asech);
    if (kind == NUMBER_QFLOAT) {
        qf = number_impl_const(&number)->value.qf;
        return qf_le(qf, QF_ZERO) || qf_gt(qf, QF_ONE)
            ? number_qfloat_qcomplex_unary(qf, qc_asech)
            : num_create_from_qfloat(qf_asech(qf));
    }
    return number_apply_nonreal_complex_unary_or_dispatch(number,
        number_double_asech, qf_asech, qc_asech, number_mpfr_asech_mut,
        number_mpc_asech);
}

number_t num_acosech(const number_t number)
{
    if (number_is_cdouble_value(&number))
        return number_apply_cdouble_unary(number, number_cdouble_acosech);
    return number_apply_nonreal_complex_unary_or_dispatch(number,
        number_double_acosech, qf_acosech, qc_acosech,
        number_mpfr_acosech_mut, number_mpc_acosech);
}

number_t num_acoth(const number_t number)
{
    number_kind_t kind = number_kind_value(&number);
    double d;
    qfloat_t qf;

    if (kind == NUMBER_DOUBLE) {
        d = number_impl_const(&number)->value.d;
        return fabs(d) <= 1.0
            ? number_double_cdouble_unary(d, number_cdouble_acoth)
            : num_create_from_double(number_double_acoth(d));
    }
    if (kind == NUMBER_CDOUBLE)
        return number_apply_cdouble_unary(number, number_cdouble_acoth);
    if (kind == NUMBER_QFLOAT) {
        qf = number_impl_const(&number)->value.qf;
        return qf_le(qf_abs(qf), QF_ONE)
            ? number_qfloat_qcomplex_unary(qf, qc_acoth)
            : num_create_from_qfloat(qf_acoth(qf));
    }
    return number_apply_nonreal_complex_unary_or_dispatch(number,
        number_double_acoth, qf_acoth, qc_acoth, number_mpfr_acoth_mut,
        number_mpc_acoth);
}

number_t num_gamma(const number_t number)
{
    return number_apply_nonreal_complex_unary_or_dispatch(number, tgamma,
        qf_gamma, qc_gamma, number_mpfr_gamma_mut, NULL);
}

number_t num_lgamma(const number_t number)
{
    return number_apply_nonreal_complex_unary_or_dispatch(number, lgamma,
        qf_lgamma, qc_lgamma, number_mpfr_lgamma_mut, NULL);
}

number_t num_digamma(const number_t number)
{
    return number_apply_nonreal_complex_unary_or_dispatch(number, NULL, qf_digamma, qc_digamma, number_mpfr_digamma_mut, NULL);
}

number_t num_trigamma(const number_t number)
{
    return number_apply_nonreal_complex_unary_or_dispatch(number, NULL, qf_trigamma, qc_trigamma, number_mpfr_trigamma_mut, NULL);
}

number_t num_tetragamma(const number_t number)
{
    return number_apply_nonreal_complex_unary_or_dispatch(number, NULL, qf_tetragamma, qc_tetragamma, number_mpfr_tetragamma_mut, NULL);
}

number_t num_polygamma(unsigned int order, const number_t number)
{
    number_kind_t kind;
    number_t *promoted = NULL;
    number_mpfr_t *copy = NULL;
    number_t *wrapped = NULL;

    if (order == 0u)
        return num_digamma(number);
    if (order == 1u)
        return num_trigamma(number);
    if (order == 2u)
        return num_tetragamma(number);
    kind = number_kind_value(&number);
    if (kind == NUMBER_COMPLEX && !num_is_real(number)) {
        const complex_t *value = number_impl_const(&number)->value.cx;
        size_t precision_bits = num_get_prec_bits(number);
        mpc_t in;
        mpc_t out;
        number_t result = number_invalid();

        if (precision_bits == 0u)
            precision_bits = num_get_default_prec_bits();
        mpc_init2(in, (mpfr_prec_t)precision_bits);
        mpc_init2(out, (mpfr_prec_t)precision_bits);
        if (number_complex_get_mpc(in, value, precision_bits) == 0 &&
            number_mpc_polygamma_order(out, in, order, MPC_RNDNN) == 0)
            result = number_take_mpc_complex_result(out, precision_bits);
        mpc_clear(out);
        mpc_clear(in);
        return result;
    }
    if (!num_is_real(number))
        return num_create_from_qcomplex(qc_polygamma(order,
            number_value_to_qcomplex(&number)));

    if (kind == NUMBER_QFLOAT)
        return num_create_from_qfloat(qf_polygamma(order,
            number_impl_const(&number)->value.qf));
    if (kind == NUMBER_QCOMPLEX)
        return num_create_from_qcomplex(qc_polygamma(order,
            number_impl_const(&number)->value.qc));
    if (kind == NUMBER_MPFR) {
        copy = number_mpfr_clone(number_impl_const(&number)->value.mpfr);
        if (!copy || number_mpfr_ensure(copy, num_get_prec_bits(number)) != 0 ||
            number_mpfr_polygamma_order_mut(copy->value, order) != 0) {
            number_mpfr_free(copy);
            return number_invalid();
        }
        wrapped = number_wrap_mpfr(copy);
        return wrapped ? number_take(wrapped) : number_invalid();
    }

    promoted = number_coerce(&number, NUMBER_MPFR);
    if (!promoted ||
        number_mpfr_ensure(number_impl(promoted)->value.mpfr,
            num_get_prec_bits(*promoted)) != 0 ||
        number_mpfr_polygamma_order_mut(
            number_impl(promoted)->value.mpfr->value, order) != 0) {
        number_box_free(promoted);
        return number_invalid();
    }
    return number_take(promoted);
}

number_t num_gammainv(const number_t number)
{
    return number_apply_nonreal_complex_unary_or_dispatch(number, NULL, qf_gammainv, qc_gammainv, number_mpfr_gammainv_mut, NULL);
}

number_t num_erf(const number_t number)
{
    return number_apply_unary_math_with_double(number, erf, qf_erf, qc_erf,
        number_mpfr_erf_mut, NULL);
}

number_t num_erfc(const number_t number)
{
    return number_apply_unary_math_with_double(number, erfc, qf_erfc, qc_erfc,
        number_mpfr_erfc_mut, NULL);
}

number_t num_erfinv(const number_t number)
{
    return number_apply_unary_math(number, qf_erfinv, qc_erfinv, number_mpfr_erfinv_mut, NULL);
}

number_t num_erfcinv(const number_t number)
{
    return number_apply_unary_math(number, qf_erfcinv, qc_erfcinv, number_mpfr_erfcinv_mut, NULL);
}

number_t num_lambert_w0(const number_t number)
{
    if (num_eq(number, NUM_NEG_INV_E))
        return number_const_return_like(&number, NUMBER_CONST_NEG_ONE);
    if (number_lambert_w0_requires_complex(&number))
        return number_apply_qcomplex_unary(number, qc_productlog);
    return number_apply_nonreal_complex_unary_or_dispatch(number, NULL, qf_lambert_w0, qc_productlog, number_mpfr_lambert_w0_mut, NULL);
}

number_t num_lambert_wm1(const number_t number)
{
    if (num_eq(number, NUM_NEG_INV_E))
        return number_const_return_like(&number, NUMBER_CONST_NEG_ONE);
    if (number_lambert_wm1_requires_complex(&number))
        return number_apply_qcomplex_unary(number, qc_lambert_wm1);
    return number_apply_nonreal_complex_unary_or_dispatch(number, NULL, qf_lambert_wm1, qc_lambert_wm1, number_mpfr_lambert_wm1_mut, NULL);
}

number_t num_beta(const number_t a, const number_t b)
{
    return number_apply_binary_math_with_double(a, b, number_double_beta,
        qf_beta, qc_beta, number_mpfr_beta_mut, NULL);
}

number_t num_logbeta(const number_t a, const number_t b)
{
    return number_apply_binary_math_with_double(a, b, number_double_logbeta,
        qf_logbeta, qc_logbeta, number_mpfr_logbeta_mut, NULL);
}

number_t num_binomial(const number_t a, const number_t b)
{
    mpz_t n;
    mpz_t k;
    mpz_t value;
    number_t out = NUM_NAN;

    mpz_init(n);
    mpz_init(k);
    if (number_get_exact_integer_mpz(a, n) &&
        number_get_exact_integer_mpz(b, k) &&
        mpz_sgn(n) >= 0 && mpz_sgn(k) >= 0 &&
        mpz_fits_ulong_p(n) && mpz_fits_ulong_p(k)) {
        mpz_init(value);
        mpz_bin_uiui(value, mpz_get_ui(n), mpz_get_ui(k));
        out = number_from_mpz_value(value);
        mpz_clear(value);
    }
    mpz_clear(k);
    mpz_clear(n);
    if (!num_is_nan(out))
        return out;

    return number_apply_binary_math_with_double(a, b, number_double_binomial,
        qf_binomial, qc_binomial, number_mpfr_binomial_mut, NULL);
}

number_t num_beta_pdf(const number_t x, const number_t a, const number_t b)
{
    return number_apply_ternary_math_with_double(x, a, b, number_double_beta_pdf,
        qf_beta_pdf, qc_beta_pdf, number_mpfr_beta_pdf_mut, NULL);
}

number_t num_logbeta_pdf(const number_t x, const number_t a, const number_t b)
{
    return number_apply_ternary_math_with_double(x, a, b,
        number_double_logbeta_pdf, qf_logbeta_pdf, qc_logbeta_pdf,
        number_mpfr_logbeta_pdf_mut, NULL);
}

number_t num_normal_pdf(const number_t number)
{
    return number_apply_unary_math_with_double(number, number_double_normal_pdf,
        qf_normal_pdf, qc_normal_pdf, number_mpfr_normal_pdf_mut, NULL);
}

number_t num_normal_cdf(const number_t number)
{
    return number_apply_unary_math_with_double(number, number_double_normal_cdf,
        qf_normal_cdf, qc_normal_cdf, number_mpfr_normal_cdf_mut, NULL);
}

number_t num_normal_logpdf(const number_t number)
{
    return number_apply_unary_math_with_double(number,
        number_double_normal_logpdf, qf_normal_logpdf, qc_normal_logpdf,
        number_mpfr_normal_logpdf_mut, NULL);
}

number_t num_productlog(const number_t number)
{
    if (num_eq(number, NUM_NEG_INV_E))
        return number_const_return_like(&number, NUMBER_CONST_NEG_ONE);
    if (number_lambert_w0_requires_complex(&number))
        return number_apply_qcomplex_unary(number, qc_productlog);
    return number_apply_nonreal_complex_unary_or_dispatch(number, NULL, qf_productlog, qc_productlog, number_mpfr_productlog_mut, NULL);
}

number_t num_gammainc_lower(const number_t a, const number_t b)
{
    return number_apply_binary_math(a, b, qf_gammainc_lower, qc_gammainc_lower, number_mpfr_gammainc_lower_mut, NULL);
}

number_t num_gammainc_upper(const number_t a, const number_t b)
{
    return number_apply_binary_math(a, b, qf_gammainc_upper, qc_gammainc_upper, number_mpfr_gammainc_upper_mut, NULL);
}

number_t num_gammainc_P(const number_t a, const number_t b)
{
    return number_apply_binary_math(a, b, qf_gammainc_P, qc_gammainc_P, number_mpfr_gammainc_p_mut, NULL);
}

number_t num_gammainc_Q(const number_t a, const number_t b)
{
    return number_apply_binary_math(a, b, qf_gammainc_Q, qc_gammainc_Q, number_mpfr_gammainc_q_mut, NULL);
}

number_t num_ei(const number_t number)
{
    return number_apply_nonreal_complex_unary_or_dispatch(number, NULL, qf_ei, qc_ei, number_mpfr_ei_mut, NULL);
}

number_t num_e1(const number_t number)
{
    return number_apply_nonreal_complex_unary_or_dispatch(number, NULL, qf_e1, qc_e1, number_mpfr_e1_mut, NULL);
}
