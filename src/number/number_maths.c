#include "number.h"
#include "number_internal.h"
#include "internal/mrational_internal.h"

#include <errno.h>
#include <complex.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>

enum {
    NUMBER_LAMBERT_W_HALLEY_STEPS = 12
};

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
    return number_kind_value(number) == NUMBER_CPLX_DOUBLE;
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
    const mrational_t *bernoulli = mr_bernoulli_even_term(index);
    mpq_t q;
    int rc;

    if (!bernoulli)
        return -1;
    mpq_init(q);
    rc = mr_copy_mpq(q, bernoulli);
    if (rc == 0)
        mpfr_set_q(value, q, MPFR_RNDN);
    mpq_clear(q);
    return rc;
}

static int number_mpfr_trigamma_mut(mpfr_t value)
{
    size_t bernoulli_terms;
    mpfr_prec_t prec;
    mpfr_t y, inv, inv2, power, term, sum;
    size_t n;

    if (!mpfr_number_p(value) || mpfr_sgn(value) <= 0) {
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

    bernoulli_terms = mr_bernoulli_even_term_count();
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

    if (!mpfr_number_p(value) || mpfr_sgn(value) <= 0) {
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

    bernoulli_terms = mr_bernoulli_even_term_count();
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

static int number_mpfr_gammainv_mut(mpfr_t value)
{
    mpfr_set_d(value, qf_to_double(qf_gammainv((qfloat_t){
        mpfr_get_d(value, MPFR_RNDN), 0.0
    })), MPFR_RNDN);
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
    char *text;
    char *end;
    long parsed;
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
    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno != 0 || !end || *end != '\0' ||
        parsed < (long)INT_MIN || parsed > (long)INT_MAX)
    {
        free(text);
        return 0;
    }
    free(text);
    *out = (int)parsed;
    return 1;
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
    return number_apply_unary_math_with_double(number, d_fn, qf_fn, qc_fn, mpfr_fn, mpc_fn);
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
    if (kind == NUMBER_CPLX_DOUBLE)
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
    double d;
    qfloat_t qf;

    if (kind == NUMBER_DOUBLE) {
        d = number_impl_const(&number)->value.d;
        return d < 0.0 ? number_double_cdouble_unary(d, number_cdouble_log10)
                       : num_create_from_double(log10(d));
    }
    if (kind == NUMBER_CPLX_DOUBLE)
        return number_apply_cdouble_unary(number, number_cdouble_log10);
    if (kind == NUMBER_QFLOAT) {
        qf = number_impl_const(&number)->value.qf;
        return qf_lt(qf, QF_ZERO) ? number_qfloat_qcomplex_unary(qf, qc_log10)
                                  : num_create_from_qfloat(qf_log10(qf));
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
    if (kind == NUMBER_CPLX_DOUBLE)
        return number_apply_cdouble_unary(number, csqrt);
    if (kind == NUMBER_QFLOAT) {
        qf = number_impl_const(&number)->value.qf;
        return qf_lt(qf, QF_ZERO) ? number_qfloat_qcomplex_unary(qf, qc_sqrt)
                                  : num_create_from_qfloat(qf_sqrt(qf));
    }
    if (kind == NUMBER_QCOMPLEX)
        return num_create_from_qcomplex(qc_sqrt(number_impl_const(&number)->value.qc));
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
    if (number_kind_value(&base) == NUMBER_CPLX_DOUBLE &&
        number_kind_value(&exponent) == NUMBER_CPLX_DOUBLE)
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
    if (number_is_plain_inexact_value(&number))
        return number_apply_nonreal_complex_unary_or_dispatch(number, tan,
            qf_tan, qc_tan, number_mpfr_tan_mut, mpc_tan);
    if (num_is_real(number)) {
        if (number_tan_fastpath_by_const_id(&number, number_tan_fastpaths,
                sizeof(number_tan_fastpaths) / sizeof(number_tan_fastpaths[0]), &out))
            return out;
        if (number_tan_fastpath_by_value(&number, number_tan_fastpaths,
                sizeof(number_tan_fastpaths) / sizeof(number_tan_fastpaths[0]), &out))
            return out;
    }
    return number_apply_nonreal_complex_unary_or_dispatch(number, tan, qf_tan, qc_tan, number_mpfr_tan_mut, mpc_tan);
}

number_t num_atan(const number_t number)
{
    if (number_is_cdouble_value(&number))
        return number_apply_cdouble_unary(number, catan);
    return number_apply_nonreal_complex_unary_or_dispatch(number, atan, qf_atan, qc_atan, number_mpfr_atan_mut, mpc_atan);
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
    if (kind == NUMBER_CPLX_DOUBLE)
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
    if (kind == NUMBER_CPLX_DOUBLE)
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
    if (kind == NUMBER_CPLX_DOUBLE)
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
    if (kind == NUMBER_CPLX_DOUBLE)
        return number_apply_cdouble_unary(number, catanh);
    if (kind == NUMBER_QFLOAT) {
        qf = number_impl_const(&number)->value.qf;
        return qf_lt(qf, qf_from_double(-1.0)) || qf_gt(qf, qf_from_double(1.0))
            ? number_qfloat_qcomplex_unary(qf, qc_atanh)
            : num_create_from_qfloat(qf_atanh(qf));
    }
    return number_apply_nonreal_complex_unary_or_dispatch(number, atanh, qf_atanh, qc_atanh, number_mpfr_atanh_mut, mpc_atanh);
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
    return number_apply_nonreal_complex_unary_or_dispatch(number, NULL, qf_lambert_w0, qc_productlog, number_mpfr_lambert_w0_mut, NULL);
}

number_t num_lambert_wm1(const number_t number)
{
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
    int n;
    int k;
    mint_t *value;

    if (number_try_get_exact_int(a, &n) &&
        number_try_get_exact_int(b, &k) &&
        n >= 0 && k >= 0) {
        value = mi_new();
        if (!value)
            return number_invalid();
        if (mi_binomial(value, (unsigned long)n, (unsigned long)k) != 0) {
            mi_free(value);
            return number_invalid();
        }
        return number_take(number_wrap_mint(value));
    }

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
