#define MARS_QFLOAT_NO_INLINE_DIV

#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mfloat.h"
#include "number.h"
#include "qcomplex.h"
#include "qfloat.h"

typedef number_t (*number_unary_fn)(number_t);
typedef number_t (*number_binary_fn)(number_t, number_t);
typedef number_t (*number_ternary_fn)(number_t, number_t, number_t);
typedef double (*double_unary_fn)(double);
typedef double (*double_binary_fn)(double, double);
typedef double (*double_ternary_fn)(double, double, double);
typedef int (*mfloat_unary_fn)(mfloat_t *);
typedef int (*mfloat_binary_fn)(mfloat_t *, const mfloat_t *);
typedef int (*mfloat_ternary_fn)(mfloat_t *, const mfloat_t *, const mfloat_t *);
typedef qfloat_t (*qfloat_unary_fn)(qfloat_t);
typedef qfloat_t (*qfloat_binary_fn)(qfloat_t, qfloat_t);
typedef qfloat_t (*qfloat_ternary_fn)(qfloat_t, qfloat_t, qfloat_t);
typedef qcomplex_t (*qcomplex_unary_fn)(qcomplex_t);
typedef qcomplex_t (*qcomplex_binary_fn)(qcomplex_t, qcomplex_t);
typedef qcomplex_t (*qcomplex_ternary_fn)(qcomplex_t, qcomplex_t, qcomplex_t);

typedef enum bench_binary_op_t {
    BENCH_BINARY_GENERIC,
    BENCH_BINARY_ADD,
    BENCH_BINARY_SUB,
    BENCH_BINARY_MUL,
    BENCH_BINARY_DIV
} bench_binary_op_t;

typedef struct bench_stats_t {
    double median_us;
    double min_us;
    double max_us;
} bench_stats_t;

typedef struct real_unary_case_t {
    const char *label;
    const char *text;
    int iters;
    number_unary_fn number_fn;
    mfloat_unary_fn mfloat_fn;
    qfloat_unary_fn qfloat_fn;
} real_unary_case_t;

typedef struct real_binary_case_t {
    const char *label;
    const char *lhs;
    const char *rhs;
    int iters;
    number_binary_fn number_fn;
    mfloat_binary_fn mfloat_fn;
    qfloat_binary_fn qfloat_fn;
} real_binary_case_t;

typedef struct real_ternary_case_t {
    const char *label;
    const char *x;
    const char *a;
    const char *b;
    int iters;
    number_ternary_fn number_fn;
    mfloat_ternary_fn mfloat_fn;
    qfloat_ternary_fn qfloat_fn;
} real_ternary_case_t;

typedef struct complex_unary_case_t {
    const char *label;
    const char *text;
    int iters;
    number_unary_fn number_fn;
    qcomplex_unary_fn qcomplex_fn;
} complex_unary_case_t;

typedef struct complex_binary_case_t {
    const char *label;
    const char *lhs;
    const char *rhs;
    int iters;
    number_binary_fn number_fn;
    qcomplex_binary_fn qcomplex_fn;
} complex_binary_case_t;

typedef struct complex_ternary_case_t {
    const char *label;
    const char *x;
    const char *a;
    const char *b;
    int iters;
    number_ternary_fn number_fn;
    qcomplex_ternary_fn qcomplex_fn;
} complex_ternary_case_t;

typedef struct double_unary_case_t {
    const char *label;
    const char *text;
    int iters;
    number_unary_fn number_fn;
    double_unary_fn double_fn;
} double_unary_case_t;

typedef struct double_binary_case_t {
    const char *label;
    const char *lhs;
    const char *rhs;
    int iters;
    number_binary_fn number_fn;
    double_binary_fn double_fn;
} double_binary_case_t;

typedef struct double_ternary_case_t {
    const char *label;
    const char *x;
    const char *a;
    const char *b;
    int iters;
    number_ternary_fn number_fn;
    double_ternary_fn double_fn;
} double_ternary_case_t;

static volatile double bench_double_sink = 0.0;
static volatile size_t bench_size_sink = 0u;

static void consume_qfloat(qfloat_t value);
static void consume_qcomplex(qcomplex_t value);

static qfloat_t bench_qf_inv(qfloat_t x)
{
    return qf_div(QF_ONE, x);
}

static qcomplex_t bench_qc_inv(qcomplex_t z)
{
    return qc_div(qc_make(QF_ONE, QF_ZERO), z);
}

static double bench_d_neg(double x)
{
    return -x;
}

static double bench_d_inv(double x)
{
    return 1.0 / x;
}

static double bench_d_sqr(double x)
{
    return x * x;
}

static double bench_d_add(double x, double y)
{
    return x + y;
}

static double bench_d_sub(double x, double y)
{
    return x - y;
}

static double bench_d_mul(double x, double y)
{
    return x * y;
}

static double bench_d_div(double x, double y)
{
    return x / y;
}

static double bench_d_beta(double a, double b)
{
    return exp(lgamma(a) + lgamma(b) - lgamma(a + b));
}

static double bench_d_logbeta(double a, double b)
{
    return lgamma(a) + lgamma(b) - lgamma(a + b);
}

static double bench_d_binomial(double n, double k)
{
    return exp(lgamma(n + 1.0) - lgamma(k + 1.0) - lgamma(n - k + 1.0));
}

static double bench_d_normal_pdf(double x)
{
    const double inv_sqrt_2pi = 0.39894228040143267794;

    return inv_sqrt_2pi * exp(-0.5 * x * x);
}

static double bench_d_normal_cdf(double x)
{
    const double inv_sqrt2 = 0.70710678118654752440;

    return 0.5 * erfc(-x * inv_sqrt2);
}

static double bench_d_normal_logpdf(double x)
{
    const double log_sqrt_2pi = 0.91893853320467274178;

    return -0.5 * x * x - log_sqrt_2pi;
}

static double bench_d_beta_pdf(double x, double a, double b)
{
    return exp((a - 1.0) * log(x) + (b - 1.0) * log1p(-x) -
               bench_d_logbeta(a, b));
}

static double bench_d_logbeta_pdf(double x, double a, double b)
{
    return (a - 1.0) * log(x) + (b - 1.0) * log1p(-x) -
           bench_d_logbeta(a, b);
}

static const double_binary_case_t double_binary_cases[] = {
    { "add", "1.23456789", "2.34567891", 1000, num_add, bench_d_add },
    { "sub", "1.23456789", "2.34567891", 1000, num_sub, bench_d_sub },
    { "mul", "1.23456789", "2.34567891", 1000, num_mul, bench_d_mul },
    { "div", "1.23456789", "2.34567891", 800, num_div, bench_d_div },
    { "pow", "1.23456789", "2.34567891", 250, num_pow, pow },
    { "hypot", "1.23456789", "2.34567891", 500, num_hypot, hypot },
    { "atan2", "1.23456789", "2.34567891", 500, num_atan2, atan2 },
    { "beta", "2.3", "4.5", 120, num_beta, bench_d_beta },
    { "logbeta", "2.3", "4.5", 120, num_logbeta, bench_d_logbeta },
    { "binomial", "52", "5", 200, num_binomial, bench_d_binomial }
};

static const double_unary_case_t double_unary_cases[] = {
    { "neg", "1.23456789", 1000, num_neg, bench_d_neg },
    { "abs", "-1.23456789", 1000, num_abs, fabs },
    { "inv", "1.23456789", 800, num_inv, bench_d_inv },
    { "sqrt", "1.23456789", 800, num_sqrt, sqrt },
    { "sqr", "1.23456789", 1000, num_sqr, bench_d_sqr },
    { "floor", "1.23456789", 1000, num_floor, floor },
    { "exp", "1.23456789", 500, num_exp, exp },
    { "log", "2.34567891", 500, num_log, log },
    { "log10", "2.34567891", 500, num_log10, log10 },
    { "sin", "0.567", 500, num_sin, sin },
    { "cos", "0.567", 500, num_cos, cos },
    { "tan", "0.567", 500, num_tan, tan },
    { "atan", "0.567", 500, num_atan, atan },
    { "asin", "0.567", 500, num_asin, asin },
    { "acos", "0.567", 500, num_acos, acos },
    { "sinh", "0.567", 500, num_sinh, sinh },
    { "cosh", "0.567", 500, num_cosh, cosh },
    { "tanh", "0.567", 500, num_tanh, tanh },
    { "asinh", "0.567", 500, num_asinh, asinh },
    { "acosh", "1.567", 500, num_acosh, acosh },
    { "atanh", "0.567", 500, num_atanh, atanh },
    { "gamma", "2.345", 120, num_gamma, tgamma },
    { "erf", "0.567", 300, num_erf, erf },
    { "erfc", "0.567", 300, num_erfc, erfc },
    { "lgamma", "2.345", 120, num_lgamma, lgamma },
    { "normal_pdf", "0.567", 300, num_normal_pdf, bench_d_normal_pdf },
    { "normal_cdf", "0.567", 300, num_normal_cdf, bench_d_normal_cdf },
    { "normal_log", "0.567", 300, num_normal_logpdf, bench_d_normal_logpdf }
};

static const double_ternary_case_t double_ternary_cases[] = {
    { "beta_pdf", "0.4", "2.3", "4.5", 120, num_beta_pdf, bench_d_beta_pdf },
    { "logbeta_pdf", "0.4", "2.3", "4.5", 120, num_logbeta_pdf, bench_d_logbeta_pdf }
};

static const real_binary_case_t real_binary_cases[] = {
    { "add", "1.23456789", "2.34567891", 1000, num_add, mf_add, qf_add },
    { "sub", "1.23456789", "2.34567891", 1000, num_sub, mf_sub, qf_sub },
    { "mul", "1.23456789", "2.34567891", 1000, num_mul, mf_mul, qf_mul },
    { "div", "1.23456789", "2.34567891", 800, num_div, mf_div, qf_div },
    { "pow", "1.23456789", "2.34567891", 250, num_pow, mf_pow, qf_pow },
    { "hypot", "1.23456789", "2.34567891", 500, num_hypot, mf_hypot, qf_hypot },
    { "atan2", "1.23456789", "2.34567891", 500, num_atan2, mf_atan2, qf_atan2 },
    { "beta", "2.3", "4.5", 120, num_beta, mf_beta, qf_beta },
    { "logbeta", "2.3", "4.5", 120, num_logbeta, mf_logbeta, qf_logbeta },
    { "binomial", "52", "5", 200, num_binomial, mf_binomial, qf_binomial },
    { "gammainc_l", "2.3", "4.5", 80, num_gammainc_lower, mf_gammainc_lower, qf_gammainc_lower },
    { "gammainc_u", "2.3", "4.5", 80, num_gammainc_upper, mf_gammainc_upper, qf_gammainc_upper },
    { "gammainc_P", "2.3", "4.5", 80, num_gammainc_P, mf_gammainc_P, qf_gammainc_P },
    { "gammainc_Q", "2.3", "4.5", 80, num_gammainc_Q, mf_gammainc_Q, qf_gammainc_Q }
};

static const real_unary_case_t real_unary_cases[] = {
    { "neg", "1.23456789", 1000, num_neg, mf_neg, qf_neg },
    { "abs", "-1.23456789", 1000, num_abs, mf_abs, qf_abs },
    { "inv", "1.23456789", 800, num_inv, mf_inv, bench_qf_inv },
    { "sqrt", "1.23456789", 800, num_sqrt, mf_sqrt, qf_sqrt },
    { "sqr", "1.23456789", 1000, num_sqr, mf_sqr, qf_sqr },
    { "floor", "1.23456789", 1000, num_floor, mf_floor, qf_floor },
    { "exp", "1.23456789", 500, num_exp, mf_exp, qf_exp },
    { "log", "2.34567891", 500, num_log, mf_log, qf_log },
    { "log10", "2.34567891", 500, num_log10, mf_log10, qf_log10 },
    { "sin", "0.567", 500, num_sin, mf_sin, qf_sin },
    { "cos", "0.567", 500, num_cos, mf_cos, qf_cos },
    { "tan", "0.567", 500, num_tan, mf_tan, qf_tan },
    { "atan", "0.567", 500, num_atan, mf_atan, qf_atan },
    { "asin", "0.567", 500, num_asin, mf_asin, qf_asin },
    { "acos", "0.567", 500, num_acos, mf_acos, qf_acos },
    { "sinh", "0.567", 500, num_sinh, mf_sinh, qf_sinh },
    { "cosh", "0.567", 500, num_cosh, mf_cosh, qf_cosh },
    { "tanh", "0.567", 500, num_tanh, mf_tanh, qf_tanh },
    { "asinh", "0.567", 500, num_asinh, mf_asinh, qf_asinh },
    { "acosh", "1.567", 500, num_acosh, mf_acosh, qf_acosh },
    { "atanh", "0.567", 500, num_atanh, mf_atanh, qf_atanh },
    { "gamma", "2.345", 120, num_gamma, mf_gamma, qf_gamma },
    { "erf", "0.567", 300, num_erf, mf_erf, qf_erf },
    { "erfc", "0.567", 300, num_erfc, mf_erfc, qf_erfc },
    { "erfinv", "0.567", 300, num_erfinv, mf_erfinv, qf_erfinv },
    { "erfcinv", "0.567", 300, num_erfcinv, mf_erfcinv, qf_erfcinv },
    { "lgamma", "2.345", 120, num_lgamma, mf_lgamma, qf_lgamma },
    { "digamma", "2.345", 120, num_digamma, mf_digamma, qf_digamma },
    { "trigamma", "2.345", 120, num_trigamma, mf_trigamma, qf_trigamma },
    { "tetragamma", "2.345", 120, num_tetragamma, mf_tetragamma, qf_tetragamma },
    { "lambert_w0", "0.567", 120, num_lambert_w0, mf_lambert_w0, qf_lambert_w0 },
    { "lambert_wm1", "-0.1", 120, num_lambert_wm1, mf_lambert_wm1, qf_lambert_wm1 },
    { "normal_pdf", "0.567", 300, num_normal_pdf, mf_normal_pdf, qf_normal_pdf },
    { "normal_cdf", "0.567", 300, num_normal_cdf, mf_normal_cdf, qf_normal_cdf },
    { "normal_log", "0.567", 300, num_normal_logpdf, mf_normal_logpdf, qf_normal_logpdf },
    { "productlog", "0.567", 120, num_productlog, mf_productlog, qf_productlog },
    { "ei", "0.567", 120, num_ei, mf_ei, qf_ei },
    { "e1", "0.567", 120, num_e1, mf_e1, qf_e1 }
};

static const real_ternary_case_t real_ternary_cases[] = {
    { "beta_pdf", "0.4", "2.3", "4.5", 120, num_beta_pdf, mf_beta_pdf, qf_beta_pdf },
    { "logbeta_pdf", "0.4", "2.3", "4.5", 120, num_logbeta_pdf, mf_logbeta_pdf, qf_logbeta_pdf }
};

static const complex_binary_case_t complex_binary_cases[] = {
    { "add", "1.23456789 + 2.34567891i", "3.45678912 - 0.45678912i", 1000, num_add, qc_add },
    { "sub", "1.23456789 + 2.34567891i", "3.45678912 - 0.45678912i", 1000, num_sub, qc_sub },
    { "mul", "1.23456789 + 2.34567891i", "3.45678912 - 0.45678912i", 1000, num_mul, qc_mul },
    { "div", "1.23456789 + 2.34567891i", "3.45678912 - 0.45678912i", 500, num_div, qc_div },
    { "pow", "1.23456789 + 2.34567891i", "3.45678912 - 0.45678912i", 120, num_pow, qc_pow },
    { "hypot", "1.23456789 + 2.34567891i", "3.45678912 - 0.45678912i", 300, num_hypot, qc_hypot },
    { "atan2", "1.23456789 + 2.34567891i", "3.45678912 - 0.45678912i", 300, num_atan2, qc_atan2 },
    { "beta", "1.5 + 0.5i", "2 - 0.3i", 100, num_beta, qc_beta },
    { "logbeta", "1.5 + 0.5i", "2 - 0.3i", 100, num_logbeta, qc_logbeta },
    { "binomial", "5 + 0.5i", "2 - 0.3i", 100, num_binomial, qc_binomial },
    { "gammainc_l", "2.3 + 0.2i", "4.5 - 0.1i", 80, num_gammainc_lower, qc_gammainc_lower },
    { "gammainc_u", "2.3 + 0.2i", "4.5 - 0.1i", 80, num_gammainc_upper, qc_gammainc_upper },
    { "gammainc_P", "2.3 + 0.2i", "4.5 - 0.1i", 80, num_gammainc_P, qc_gammainc_P },
    { "gammainc_Q", "2.3 + 0.2i", "4.5 - 0.1i", 80, num_gammainc_Q, qc_gammainc_Q }
};

static const complex_unary_case_t complex_unary_cases[] = {
    { "neg", "0.567 + 0.321i", 1000, num_neg, qc_neg },
    { "inv", "0.567 + 0.321i", 500, num_inv, bench_qc_inv },
    { "conj", "0.567 + 0.321i", 1000, num_conj, qc_conj },
    { "sqrt", "0.567 + 0.321i", 500, num_sqrt, qc_sqrt },
    { "floor", "0.567 + 0.321i", 500, num_floor, qc_floor },
    { "exp", "0.567 + 0.321i", 300, num_exp, qc_exp },
    { "log", "0.567 + 0.321i", 300, num_log, qc_log },
    { "log10", "0.567 + 0.321i", 300, num_log10, qc_log10 },
    { "sin", "0.567 + 0.321i", 300, num_sin, qc_sin },
    { "cos", "0.567 + 0.321i", 300, num_cos, qc_cos },
    { "tan", "0.567 + 0.321i", 300, num_tan, qc_tan },
    { "atan", "0.567 + 0.321i", 300, num_atan, qc_atan },
    { "asin", "0.567 + 0.321i", 300, num_asin, qc_asin },
    { "acos", "0.567 + 0.321i", 300, num_acos, qc_acos },
    { "sinh", "0.567 + 0.321i", 300, num_sinh, qc_sinh },
    { "cosh", "0.567 + 0.321i", 300, num_cosh, qc_cosh },
    { "tanh", "0.567 + 0.321i", 300, num_tanh, qc_tanh },
    { "asinh", "0.567 + 0.321i", 300, num_asinh, qc_asinh },
    { "acosh", "1.567 + 0.321i", 300, num_acosh, qc_acosh },
    { "atanh", "0.267 + 0.121i", 300, num_atanh, qc_atanh },
    { "gamma", "1.5 + 0.7i", 100, num_gamma, qc_gamma },
    { "erf", "0.567 + 0.321i", 120, num_erf, qc_erf },
    { "erfc", "0.567 + 0.321i", 120, num_erfc, qc_erfc },
    { "erfinv", "0.267 + 0.121i", 120, num_erfinv, qc_erfinv },
    { "erfcinv", "0.267 + 0.121i", 120, num_erfcinv, qc_erfcinv },
    { "lgamma", "1.5 + 0.7i", 100, num_lgamma, qc_lgamma },
    { "digamma", "1.5 + 0.7i", 100, num_digamma, qc_digamma },
    { "trigamma", "1.5 + 0.7i", 100, num_trigamma, qc_trigamma },
    { "tetragamma", "1.5 + 0.7i", 100, num_tetragamma, qc_tetragamma },
    { "lambert_wm1", "-0.1 + 0.1i", 100, num_lambert_wm1, qc_lambert_wm1 },
    { "productlog", "0.567 + 0.321i", 100, num_productlog, qc_productlog },
    { "normal_pdf", "0.567 + 0.321i", 120, num_normal_pdf, qc_normal_pdf },
    { "normal_cdf", "0.567 + 0.321i", 120, num_normal_cdf, qc_normal_cdf },
    { "normal_log", "0.567 + 0.321i", 120, num_normal_logpdf, qc_normal_logpdf },
    { "ei", "0.567 + 0.321i", 100, num_ei, qc_ei },
    { "e1", "0.567 + 0.321i", 100, num_e1, qc_e1 }
};

static const complex_ternary_case_t complex_ternary_cases[] = {
    { "beta_pdf", "0.4 + 0.1i", "2.3 + 0.2i", "4.5 - 0.1i", 80, num_beta_pdf, qc_beta_pdf },
    { "logbeta_pdf", "0.4 + 0.1i", "2.3 + 0.2i", "4.5 - 0.1i", 80, num_logbeta_pdf, qc_logbeta_pdf }
};

static uint64_t now_ns(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static int env_int(const char *name, int fallback)
{
    const char *text = getenv(name);
    char *end = NULL;
    long value;

    if (!text || !*text)
        return fallback;
    value = strtol(text, &end, 10);
    return end && *end == '\0' && value > 0 ? (int)value : fallback;
}

static size_t env_size(const char *name, size_t fallback)
{
    const char *text = getenv(name);
    char *end = NULL;
    unsigned long value;

    if (!text || !*text)
        return fallback;
    value = strtoul(text, &end, 10);
    return end && *end == '\0' && value > 0u ? (size_t)value : fallback;
}

static int scaled_iters(int base_iters)
{
    return base_iters * env_int("MARS_BENCH_SCALE", 1);
}

static bench_binary_op_t binary_op_for_label(const char *label)
{
    if (strcmp(label, "add") == 0)
        return BENCH_BINARY_ADD;
    if (strcmp(label, "sub") == 0)
        return BENCH_BINARY_SUB;
    if (strcmp(label, "mul") == 0)
        return BENCH_BINARY_MUL;
    if (strcmp(label, "div") == 0)
        return BENCH_BINARY_DIV;
    return BENCH_BINARY_GENERIC;
}

static qfloat_t qfloat_bounded_rhs(bench_binary_op_t op, qfloat_t rhs)
{
    if (op == BENCH_BINARY_MUL || op == BENCH_BINARY_DIV)
        return qf_from_string("1.0000000000000001");
    return rhs;
}

static qcomplex_t qcomplex_bounded_rhs(bench_binary_op_t op, qcomplex_t rhs)
{
    if (op == BENCH_BINARY_MUL || op == BENCH_BINARY_DIV)
        return qc_make(qf_from_string("1.0000000000000001"),
                       qf_from_string("0.0000000000000001"));
    return rhs;
}

static double double_bounded_rhs(bench_binary_op_t op, double rhs)
{
    if (op == BENCH_BINARY_MUL || op == BENCH_BINARY_DIV)
        return 1.0000000000000002;
    return rhs;
}

static int compare_double(const void *lhs, const void *rhs)
{
    double a = *(const double *)lhs;
    double b = *(const double *)rhs;

    if (a < b)
        return -1;
    if (a > b)
        return 1;
    return 0;
}

static bench_stats_t estimate(double *samples, int count)
{
    bench_stats_t stats = {0};

    qsort(samples, (size_t)count, sizeof(*samples), compare_double);
    stats.min_us = samples[0];
    stats.max_us = samples[count - 1];
    stats.median_us = count % 2
        ? samples[count / 2]
        : (samples[count / 2 - 1] + samples[count / 2]) / 2.0;
    return stats;
}

static void consume_number(number_t value)
{
    uint32_t kind = (uint32_t)value.storage[0];

    bench_size_sink += (size_t)kind;
    if (kind == 2u) {
        consume_qfloat(number_inline_qfloat(value));
    } else if (kind == 3u) {
        consume_qcomplex(number_inline_qcomplex(value));
    } else {
        bench_size_sink += (size_t)value.storage[1];
    }
}

static void consume_mfloat(const mfloat_t *value)
{
    bench_size_sink += mf_get_precision(value);
}

static void consume_qfloat(qfloat_t value)
{
    bench_double_sink += value.hi;
    bench_double_sink += value.lo;
}

static void consume_qcomplex(qcomplex_t value)
{
    consume_qfloat(qc_real(value));
    consume_qfloat(qc_imag(value));
}

static void consume_double(double value)
{
    bench_double_sink += value;
}

static void fail_case(const char *backend, const char *label, const char *message)
{
    fprintf(stderr, "%s %s: %s\n", backend, label, message);
    exit(EXIT_FAILURE);
}

static bench_stats_t time_number_unary_double(const double_unary_case_t *bench_case,
                                              int repeats)
{
    double *samples = calloc((size_t)repeats, sizeof(*samples));
    number_t src = num_create_from_double(strtod(bench_case->text, NULL));
    int iters = scaled_iters(bench_case->iters);

    if (!samples)
        fail_case("number/double", bench_case->label, "sample allocation failed");

    for (int r = 0; r < repeats; ++r) {
        uint64_t start = now_ns();

        for (int i = 0; i < iters; ++i) {
            number_t out = bench_case->number_fn(src);

            consume_number(out);
            num_destroy(&out);
        }
        samples[r] = (double)(now_ns() - start) / (double)iters / 1000.0;
    }

    num_destroy(&src);
    bench_stats_t stats = estimate(samples, repeats);
    free(samples);
    return stats;
}

static bench_stats_t time_number_binary_double(const double_binary_case_t *bench_case,
                                               int repeats)
{
    double *samples = calloc((size_t)repeats, sizeof(*samples));
    int iters = scaled_iters(bench_case->iters);
    bench_binary_op_t op = binary_op_for_label(bench_case->label);
    number_t lhs = num_create_from_double(strtod(bench_case->lhs, NULL));
    number_t rhs = num_create_from_double(
        double_bounded_rhs(op, strtod(bench_case->rhs, NULL)));

    if (!samples)
        fail_case("number/double", bench_case->label, "sample allocation failed");

#define TIME_NUMBER_DOUBLE_BINARY(expr)                                             \
    do {                                                                            \
        for (int r = 0; r < repeats; ++r) {                                         \
            number_t acc = lhs;                                                     \
            uint64_t start = now_ns();                                              \
                                                                                    \
            for (int i = 0; i < iters; ++i) {                                       \
                number_t out = (expr);                                              \
                                                                                    \
                acc = out;                                                          \
            }                                                                       \
            consume_number(acc);                                                    \
            samples[r] = (double)(now_ns() - start) / (double)iters / 1000.0;       \
        }                                                                           \
    } while (0)

    switch (op) {
    case BENCH_BINARY_ADD:
        TIME_NUMBER_DOUBLE_BINARY(num_add(acc, rhs));
        break;
    case BENCH_BINARY_SUB:
        TIME_NUMBER_DOUBLE_BINARY(num_sub(acc, rhs));
        break;
    case BENCH_BINARY_MUL:
        TIME_NUMBER_DOUBLE_BINARY(num_mul(acc, rhs));
        break;
    case BENCH_BINARY_DIV:
        TIME_NUMBER_DOUBLE_BINARY(num_div(acc, rhs));
        break;
    default:
        TIME_NUMBER_DOUBLE_BINARY(bench_case->number_fn(lhs, rhs));
        break;
    }

#undef TIME_NUMBER_DOUBLE_BINARY

    num_destroy(&lhs);
    num_destroy(&rhs);
    bench_stats_t stats = estimate(samples, repeats);
    free(samples);
    return stats;
}

static bench_stats_t time_number_ternary_double(const double_ternary_case_t *bench_case,
                                                int repeats)
{
    double *samples = calloc((size_t)repeats, sizeof(*samples));
    number_t x = num_create_from_double(strtod(bench_case->x, NULL));
    number_t a = num_create_from_double(strtod(bench_case->a, NULL));
    number_t b = num_create_from_double(strtod(bench_case->b, NULL));
    int iters = scaled_iters(bench_case->iters);

    if (!samples)
        fail_case("number/double", bench_case->label, "sample allocation failed");

    for (int r = 0; r < repeats; ++r) {
        uint64_t start = now_ns();

        for (int i = 0; i < iters; ++i) {
            number_t out = bench_case->number_fn(x, a, b);

            consume_number(out);
            num_destroy(&out);
        }
        samples[r] = (double)(now_ns() - start) / (double)iters / 1000.0;
    }

    num_destroy(&x);
    num_destroy(&a);
    num_destroy(&b);
    bench_stats_t stats = estimate(samples, repeats);
    free(samples);
    return stats;
}

static bench_stats_t time_double_unary(const double_unary_case_t *bench_case,
                                       int repeats)
{
    double *samples = calloc((size_t)repeats, sizeof(*samples));
    double src = strtod(bench_case->text, NULL);
    int iters = scaled_iters(bench_case->iters);

    if (!samples)
        fail_case("double", bench_case->label, "sample allocation failed");

    for (int r = 0; r < repeats; ++r) {
        uint64_t start = now_ns();

        for (int i = 0; i < iters; ++i)
            consume_double(bench_case->double_fn(src));
        samples[r] = (double)(now_ns() - start) / (double)iters / 1000.0;
    }

    bench_stats_t stats = estimate(samples, repeats);
    free(samples);
    return stats;
}

static bench_stats_t time_double_binary(const double_binary_case_t *bench_case,
                                        int repeats)
{
    double *samples = calloc((size_t)repeats, sizeof(*samples));
    int iters = scaled_iters(bench_case->iters);
    bench_binary_op_t op = binary_op_for_label(bench_case->label);
    double lhs = strtod(bench_case->lhs, NULL);
    double rhs = double_bounded_rhs(op, strtod(bench_case->rhs, NULL));

    if (!samples)
        fail_case("double", bench_case->label, "sample allocation failed");

#define TIME_DOUBLE_BINARY(expr)                                                    \
    do {                                                                            \
        for (int r = 0; r < repeats; ++r) {                                         \
            double acc = lhs;                                                       \
            uint64_t start = now_ns();                                              \
                                                                                    \
            for (int i = 0; i < iters; ++i)                                         \
                acc = (expr);                                                       \
            consume_double(acc);                                                    \
            samples[r] = (double)(now_ns() - start) / (double)iters / 1000.0;       \
        }                                                                           \
    } while (0)

    switch (op) {
    case BENCH_BINARY_ADD:
        TIME_DOUBLE_BINARY(acc + rhs);
        break;
    case BENCH_BINARY_SUB:
        TIME_DOUBLE_BINARY(acc - rhs);
        break;
    case BENCH_BINARY_MUL:
        TIME_DOUBLE_BINARY(acc * rhs);
        break;
    case BENCH_BINARY_DIV:
        TIME_DOUBLE_BINARY(acc / rhs);
        break;
    default:
        TIME_DOUBLE_BINARY(bench_case->double_fn(lhs, rhs));
        break;
    }

#undef TIME_DOUBLE_BINARY

    bench_stats_t stats = estimate(samples, repeats);
    free(samples);
    return stats;
}

static bench_stats_t time_double_ternary(const double_ternary_case_t *bench_case,
                                         int repeats)
{
    double *samples = calloc((size_t)repeats, sizeof(*samples));
    double x = strtod(bench_case->x, NULL);
    double a = strtod(bench_case->a, NULL);
    double b = strtod(bench_case->b, NULL);
    int iters = scaled_iters(bench_case->iters);

    if (!samples)
        fail_case("double", bench_case->label, "sample allocation failed");

    for (int r = 0; r < repeats; ++r) {
        uint64_t start = now_ns();

        for (int i = 0; i < iters; ++i)
            consume_double(bench_case->double_fn(x, a, b));
        samples[r] = (double)(now_ns() - start) / (double)iters / 1000.0;
    }

    bench_stats_t stats = estimate(samples, repeats);
    free(samples);
    return stats;
}

static bench_stats_t time_number_unary_mpfr(const real_unary_case_t *bench_case,
                                            size_t precision,
                                            int repeats)
{
    double *samples = calloc((size_t)repeats, sizeof(*samples));
    size_t old_precision = num_get_default_prec_bits();
    number_t src;
    int iters = scaled_iters(bench_case->iters);

    if (!samples)
        fail_case("number/mpfr", bench_case->label, "sample allocation failed");
    if (num_set_default_prec_bits(precision) != 0)
        fail_case("number/mpfr", bench_case->label, "set precision failed");
    src = num_create_from_string(bench_case->text);

    for (int r = 0; r < repeats; ++r) {
        uint64_t start = now_ns();

        for (int i = 0; i < iters; ++i) {
            number_t out = bench_case->number_fn(src);

            consume_number(out);
            num_destroy(&out);
        }
        samples[r] = (double)(now_ns() - start) / (double)iters / 1000.0;
    }

    num_destroy(&src);
    (void)num_set_default_prec_bits(old_precision);
    bench_stats_t stats = estimate(samples, repeats);
    free(samples);
    return stats;
}

static bench_stats_t time_number_binary_mpfr(const real_binary_case_t *bench_case,
                                             size_t precision,
                                             int repeats)
{
    double *samples = calloc((size_t)repeats, sizeof(*samples));
    size_t old_precision = num_get_default_prec_bits();
    number_t lhs;
    number_t rhs;
    int iters = scaled_iters(bench_case->iters);

    if (!samples)
        fail_case("number/mpfr", bench_case->label, "sample allocation failed");
    if (num_set_default_prec_bits(precision) != 0)
        fail_case("number/mpfr", bench_case->label, "set precision failed");
    lhs = num_create_from_string(bench_case->lhs);
    rhs = num_create_from_string(bench_case->rhs);

    for (int r = 0; r < repeats; ++r) {
        uint64_t start = now_ns();

        for (int i = 0; i < iters; ++i) {
            number_t out = bench_case->number_fn(lhs, rhs);

            consume_number(out);
            num_destroy(&out);
        }
        samples[r] = (double)(now_ns() - start) / (double)iters / 1000.0;
    }

    num_destroy(&lhs);
    num_destroy(&rhs);
    (void)num_set_default_prec_bits(old_precision);
    bench_stats_t stats = estimate(samples, repeats);
    free(samples);
    return stats;
}

static bench_stats_t time_number_ternary_mpfr(const real_ternary_case_t *bench_case,
                                              size_t precision,
                                              int repeats)
{
    double *samples = calloc((size_t)repeats, sizeof(*samples));
    size_t old_precision = num_get_default_prec_bits();
    number_t x;
    number_t a;
    number_t b;
    int iters = scaled_iters(bench_case->iters);

    if (!samples)
        fail_case("number/mpfr", bench_case->label, "sample allocation failed");
    if (num_set_default_prec_bits(precision) != 0)
        fail_case("number/mpfr", bench_case->label, "set precision failed");
    x = num_create_from_string(bench_case->x);
    a = num_create_from_string(bench_case->a);
    b = num_create_from_string(bench_case->b);

    for (int r = 0; r < repeats; ++r) {
        uint64_t start = now_ns();

        for (int i = 0; i < iters; ++i) {
            number_t out = bench_case->number_fn(x, a, b);

            consume_number(out);
            num_destroy(&out);
        }
        samples[r] = (double)(now_ns() - start) / (double)iters / 1000.0;
    }

    num_destroy(&x);
    num_destroy(&a);
    num_destroy(&b);
    (void)num_set_default_prec_bits(old_precision);
    bench_stats_t stats = estimate(samples, repeats);
    free(samples);
    return stats;
}

static bench_stats_t time_mfloat_unary(const real_unary_case_t *bench_case,
                                       size_t precision,
                                       int repeats)
{
    double *samples = calloc((size_t)repeats, sizeof(*samples));
    size_t old_precision = mf_get_default_precision();
    mfloat_t *src;
    int iters = scaled_iters(bench_case->iters);

    if (!samples)
        fail_case("mfloat", bench_case->label, "sample allocation failed");
    if (mf_set_default_precision(precision) != 0)
        fail_case("mfloat", bench_case->label, "set precision failed");
    src = mf_create_string(bench_case->text);
    if (!src)
        fail_case("mfloat", bench_case->label, "source creation failed");

    for (int r = 0; r < repeats; ++r) {
        uint64_t start = now_ns();

        for (int i = 0; i < iters; ++i) {
            mfloat_t *out = mf_clone(src);

            if (!out || bench_case->mfloat_fn(out) != 0)
                fail_case("mfloat", bench_case->label, "operation failed");
            consume_mfloat(out);
            mf_free(out);
        }
        samples[r] = (double)(now_ns() - start) / (double)iters / 1000.0;
    }

    mf_free(src);
    (void)mf_set_default_precision(old_precision);
    bench_stats_t stats = estimate(samples, repeats);
    free(samples);
    return stats;
}

static bench_stats_t time_mfloat_binary(const real_binary_case_t *bench_case,
                                        size_t precision,
                                        int repeats)
{
    double *samples = calloc((size_t)repeats, sizeof(*samples));
    size_t old_precision = mf_get_default_precision();
    mfloat_t *lhs;
    mfloat_t *rhs;
    int iters = scaled_iters(bench_case->iters);

    if (!samples)
        fail_case("mfloat", bench_case->label, "sample allocation failed");
    if (mf_set_default_precision(precision) != 0)
        fail_case("mfloat", bench_case->label, "set precision failed");
    lhs = mf_create_string(bench_case->lhs);
    rhs = mf_create_string(bench_case->rhs);
    if (!lhs || !rhs)
        fail_case("mfloat", bench_case->label, "source creation failed");

    for (int r = 0; r < repeats; ++r) {
        uint64_t start = now_ns();

        for (int i = 0; i < iters; ++i) {
            mfloat_t *out = mf_clone(lhs);

            if (!out || bench_case->mfloat_fn(out, rhs) != 0)
                fail_case("mfloat", bench_case->label, "operation failed");
            consume_mfloat(out);
            mf_free(out);
        }
        samples[r] = (double)(now_ns() - start) / (double)iters / 1000.0;
    }

    mf_free(lhs);
    mf_free(rhs);
    (void)mf_set_default_precision(old_precision);
    bench_stats_t stats = estimate(samples, repeats);
    free(samples);
    return stats;
}

static bench_stats_t time_mfloat_ternary(const real_ternary_case_t *bench_case,
                                         size_t precision,
                                         int repeats)
{
    double *samples = calloc((size_t)repeats, sizeof(*samples));
    size_t old_precision = mf_get_default_precision();
    mfloat_t *x;
    mfloat_t *a;
    mfloat_t *b;
    int iters = scaled_iters(bench_case->iters);

    if (!samples)
        fail_case("mfloat", bench_case->label, "sample allocation failed");
    if (mf_set_default_precision(precision) != 0)
        fail_case("mfloat", bench_case->label, "set precision failed");
    x = mf_create_string(bench_case->x);
    a = mf_create_string(bench_case->a);
    b = mf_create_string(bench_case->b);
    if (!x || !a || !b)
        fail_case("mfloat", bench_case->label, "source creation failed");

    for (int r = 0; r < repeats; ++r) {
        uint64_t start = now_ns();

        for (int i = 0; i < iters; ++i) {
            mfloat_t *out = mf_clone(x);

            if (!out || bench_case->mfloat_fn(out, a, b) != 0)
                fail_case("mfloat", bench_case->label, "operation failed");
            consume_mfloat(out);
            mf_free(out);
        }
        samples[r] = (double)(now_ns() - start) / (double)iters / 1000.0;
    }

    mf_free(x);
    mf_free(a);
    mf_free(b);
    (void)mf_set_default_precision(old_precision);
    bench_stats_t stats = estimate(samples, repeats);
    free(samples);
    return stats;
}

static bench_stats_t time_number_unary_qfloat(const real_unary_case_t *bench_case,
                                              int repeats)
{
    double *samples = calloc((size_t)repeats, sizeof(*samples));
    number_t src = num_create_from_qfloat(qf_from_string(bench_case->text));
    int iters = scaled_iters(bench_case->iters);

    if (!samples)
        fail_case("number/qfloat", bench_case->label, "sample allocation failed");

    for (int r = 0; r < repeats; ++r) {
        uint64_t start = now_ns();

        for (int i = 0; i < iters; ++i) {
            number_t out = bench_case->number_fn(src);

            consume_number(out);
            num_destroy(&out);
        }
        samples[r] = (double)(now_ns() - start) / (double)iters / 1000.0;
    }

    num_destroy(&src);
    bench_stats_t stats = estimate(samples, repeats);
    free(samples);
    return stats;
}

static bench_stats_t time_number_binary_qfloat(const real_binary_case_t *bench_case,
                                               int repeats)
{
    double *samples = calloc((size_t)repeats, sizeof(*samples));
    int iters = scaled_iters(bench_case->iters);
    bench_binary_op_t op = binary_op_for_label(bench_case->label);
    qfloat_t lhs_value = qf_from_string(bench_case->lhs);
    qfloat_t rhs_value = qfloat_bounded_rhs(op, qf_from_string(bench_case->rhs));
    number_t lhs = num_create_from_qfloat(lhs_value);
    number_t rhs = num_create_from_qfloat(rhs_value);

    if (!samples)
        fail_case("number/qfloat", bench_case->label, "sample allocation failed");

#define TIME_NUMBER_QFLOAT_BINARY(expr)                                             \
    do {                                                                            \
        for (int r = 0; r < repeats; ++r) {                                         \
            number_t acc = lhs;                                                     \
            uint64_t start = now_ns();                                              \
                                                                                    \
            for (int i = 0; i < iters; ++i) {                                       \
                number_t out = (expr);                                              \
                                                                                    \
                acc = out;                                                          \
            }                                                                       \
            consume_number(acc);                                                    \
            samples[r] = (double)(now_ns() - start) / (double)iters / 1000.0;       \
        }                                                                           \
    } while (0)

    switch (op) {
    case BENCH_BINARY_ADD:
        TIME_NUMBER_QFLOAT_BINARY(num_add(acc, rhs));
        break;
    case BENCH_BINARY_SUB:
        TIME_NUMBER_QFLOAT_BINARY(num_sub(acc, rhs));
        break;
    case BENCH_BINARY_MUL:
        TIME_NUMBER_QFLOAT_BINARY(num_mul(acc, rhs));
        break;
    case BENCH_BINARY_DIV:
        TIME_NUMBER_QFLOAT_BINARY(num_div(acc, rhs));
        break;
    default:
        TIME_NUMBER_QFLOAT_BINARY(bench_case->number_fn(lhs, rhs));
        break;
    }

#undef TIME_NUMBER_QFLOAT_BINARY

    num_destroy(&lhs);
    num_destroy(&rhs);
    bench_stats_t stats = estimate(samples, repeats);
    free(samples);
    return stats;
}

static bench_stats_t time_number_ternary_qfloat(const real_ternary_case_t *bench_case,
                                                int repeats)
{
    double *samples = calloc((size_t)repeats, sizeof(*samples));
    number_t x = num_create_from_qfloat(qf_from_string(bench_case->x));
    number_t a = num_create_from_qfloat(qf_from_string(bench_case->a));
    number_t b = num_create_from_qfloat(qf_from_string(bench_case->b));
    int iters = scaled_iters(bench_case->iters);

    if (!samples)
        fail_case("number/qfloat", bench_case->label, "sample allocation failed");

    for (int r = 0; r < repeats; ++r) {
        uint64_t start = now_ns();

        for (int i = 0; i < iters; ++i) {
            number_t out = bench_case->number_fn(x, a, b);

            consume_number(out);
            num_destroy(&out);
        }
        samples[r] = (double)(now_ns() - start) / (double)iters / 1000.0;
    }

    num_destroy(&x);
    num_destroy(&a);
    num_destroy(&b);
    bench_stats_t stats = estimate(samples, repeats);
    free(samples);
    return stats;
}

static bench_stats_t time_qfloat_ternary(const real_ternary_case_t *bench_case,
                                         int repeats)
{
    double *samples = calloc((size_t)repeats, sizeof(*samples));
    qfloat_t x = qf_from_string(bench_case->x);
    qfloat_t a = qf_from_string(bench_case->a);
    qfloat_t b = qf_from_string(bench_case->b);
    int iters = scaled_iters(bench_case->iters);

    if (!samples)
        fail_case("qfloat", bench_case->label, "sample allocation failed");

    for (int r = 0; r < repeats; ++r) {
        uint64_t start = now_ns();

        for (int i = 0; i < iters; ++i)
            consume_qfloat(bench_case->qfloat_fn(x, a, b));
        samples[r] = (double)(now_ns() - start) / (double)iters / 1000.0;
    }

    bench_stats_t stats = estimate(samples, repeats);
    free(samples);
    return stats;
}

static bench_stats_t time_qfloat_unary(const real_unary_case_t *bench_case,
                                       int repeats)
{
    double *samples = calloc((size_t)repeats, sizeof(*samples));
    qfloat_t src = qf_from_string(bench_case->text);
    int iters = scaled_iters(bench_case->iters);

    if (!samples)
        fail_case("qfloat", bench_case->label, "sample allocation failed");

    for (int r = 0; r < repeats; ++r) {
        uint64_t start = now_ns();

        for (int i = 0; i < iters; ++i)
            consume_qfloat(bench_case->qfloat_fn(src));
        samples[r] = (double)(now_ns() - start) / (double)iters / 1000.0;
    }

    bench_stats_t stats = estimate(samples, repeats);
    free(samples);
    return stats;
}

static bench_stats_t time_qfloat_binary(const real_binary_case_t *bench_case,
                                        int repeats)
{
    double *samples = calloc((size_t)repeats, sizeof(*samples));
    qfloat_t lhs = qf_from_string(bench_case->lhs);
    int iters = scaled_iters(bench_case->iters);
    bench_binary_op_t op = binary_op_for_label(bench_case->label);
    qfloat_t rhs = qfloat_bounded_rhs(op, qf_from_string(bench_case->rhs));

    if (!samples)
        fail_case("qfloat", bench_case->label, "sample allocation failed");

#define TIME_QFLOAT_BINARY(expr)                                                    \
    do {                                                                            \
        for (int r = 0; r < repeats; ++r) {                                         \
            qfloat_t acc = lhs;                                                     \
            uint64_t start = now_ns();                                              \
                                                                                    \
            for (int i = 0; i < iters; ++i)                                         \
                acc = (expr);                                                       \
            consume_qfloat(acc);                                                    \
            samples[r] = (double)(now_ns() - start) / (double)iters / 1000.0;       \
        }                                                                           \
    } while (0)

    switch (op) {
    case BENCH_BINARY_ADD:
        TIME_QFLOAT_BINARY(qf_add(acc, rhs));
        break;
    case BENCH_BINARY_SUB:
        TIME_QFLOAT_BINARY(qf_sub(acc, rhs));
        break;
    case BENCH_BINARY_MUL:
        TIME_QFLOAT_BINARY(qf_mul(acc, rhs));
        break;
    case BENCH_BINARY_DIV:
        TIME_QFLOAT_BINARY(qf_div(acc, rhs));
        break;
    default:
        TIME_QFLOAT_BINARY(bench_case->qfloat_fn(lhs, rhs));
        break;
    }

#undef TIME_QFLOAT_BINARY

    bench_stats_t stats = estimate(samples, repeats);
    free(samples);
    return stats;
}

static bench_stats_t time_number_unary_qcomplex(const complex_unary_case_t *bench_case,
                                                int repeats)
{
    double *samples = calloc((size_t)repeats, sizeof(*samples));
    number_t src = num_create_from_qcomplex(qc_from_string(bench_case->text));
    int iters = scaled_iters(bench_case->iters);

    if (!samples)
        fail_case("number/qcomplex", bench_case->label, "sample allocation failed");

    for (int r = 0; r < repeats; ++r) {
        uint64_t start = now_ns();

        for (int i = 0; i < iters; ++i) {
            number_t out = bench_case->number_fn(src);

            consume_number(out);
            num_destroy(&out);
        }
        samples[r] = (double)(now_ns() - start) / (double)iters / 1000.0;
    }

    num_destroy(&src);
    bench_stats_t stats = estimate(samples, repeats);
    free(samples);
    return stats;
}

static bench_stats_t time_number_binary_qcomplex(const complex_binary_case_t *bench_case,
                                                 int repeats)
{
    double *samples = calloc((size_t)repeats, sizeof(*samples));
    int iters = scaled_iters(bench_case->iters);
    bench_binary_op_t op = binary_op_for_label(bench_case->label);
    qcomplex_t lhs_value = qc_from_string(bench_case->lhs);
    qcomplex_t rhs_value = qcomplex_bounded_rhs(op, qc_from_string(bench_case->rhs));
    number_t lhs = num_create_from_qcomplex(lhs_value);
    number_t rhs = num_create_from_qcomplex(rhs_value);

    if (!samples)
        fail_case("number/qcomplex", bench_case->label, "sample allocation failed");

#define TIME_NUMBER_QCOMPLEX_BINARY(expr)                                           \
    do {                                                                            \
        for (int r = 0; r < repeats; ++r) {                                         \
            number_t acc = lhs;                                                     \
            uint64_t start = now_ns();                                              \
                                                                                    \
            for (int i = 0; i < iters; ++i) {                                       \
                number_t out = (expr);                                              \
                                                                                    \
                acc = out;                                                          \
            }                                                                       \
            consume_number(acc);                                                    \
            samples[r] = (double)(now_ns() - start) / (double)iters / 1000.0;       \
        }                                                                           \
    } while (0)

    switch (op) {
    case BENCH_BINARY_ADD:
        TIME_NUMBER_QCOMPLEX_BINARY(num_add(acc, rhs));
        break;
    case BENCH_BINARY_SUB:
        TIME_NUMBER_QCOMPLEX_BINARY(num_sub(acc, rhs));
        break;
    case BENCH_BINARY_MUL:
        TIME_NUMBER_QCOMPLEX_BINARY(num_mul(acc, rhs));
        break;
    case BENCH_BINARY_DIV:
        TIME_NUMBER_QCOMPLEX_BINARY(num_div(acc, rhs));
        break;
    default:
        TIME_NUMBER_QCOMPLEX_BINARY(bench_case->number_fn(lhs, rhs));
        break;
    }

#undef TIME_NUMBER_QCOMPLEX_BINARY

    num_destroy(&lhs);
    num_destroy(&rhs);
    bench_stats_t stats = estimate(samples, repeats);
    free(samples);
    return stats;
}

static bench_stats_t time_qcomplex_unary(const complex_unary_case_t *bench_case,
                                         int repeats)
{
    double *samples = calloc((size_t)repeats, sizeof(*samples));
    qcomplex_t src = qc_from_string(bench_case->text);
    int iters = scaled_iters(bench_case->iters);

    if (!samples)
        fail_case("qcomplex", bench_case->label, "sample allocation failed");

    for (int r = 0; r < repeats; ++r) {
        uint64_t start = now_ns();

        for (int i = 0; i < iters; ++i)
            consume_qcomplex(bench_case->qcomplex_fn(src));
        samples[r] = (double)(now_ns() - start) / (double)iters / 1000.0;
    }

    bench_stats_t stats = estimate(samples, repeats);
    free(samples);
    return stats;
}

static bench_stats_t time_qcomplex_binary(const complex_binary_case_t *bench_case,
                                          int repeats)
{
    double *samples = calloc((size_t)repeats, sizeof(*samples));
    qcomplex_t lhs = qc_from_string(bench_case->lhs);
    int iters = scaled_iters(bench_case->iters);
    bench_binary_op_t op = binary_op_for_label(bench_case->label);
    qcomplex_t rhs = qcomplex_bounded_rhs(op, qc_from_string(bench_case->rhs));

    if (!samples)
        fail_case("qcomplex", bench_case->label, "sample allocation failed");

#define TIME_QCOMPLEX_BINARY(expr)                                                  \
    do {                                                                            \
        for (int r = 0; r < repeats; ++r) {                                         \
            qcomplex_t acc = lhs;                                                   \
            uint64_t start = now_ns();                                              \
                                                                                    \
            for (int i = 0; i < iters; ++i)                                         \
                acc = (expr);                                                       \
            consume_qcomplex(acc);                                                  \
            samples[r] = (double)(now_ns() - start) / (double)iters / 1000.0;       \
        }                                                                           \
    } while (0)

    switch (op) {
    case BENCH_BINARY_ADD:
        TIME_QCOMPLEX_BINARY(qc_add(acc, rhs));
        break;
    case BENCH_BINARY_SUB:
        TIME_QCOMPLEX_BINARY(qc_sub(acc, rhs));
        break;
    case BENCH_BINARY_MUL:
        TIME_QCOMPLEX_BINARY(qc_mul(acc, rhs));
        break;
    case BENCH_BINARY_DIV:
        TIME_QCOMPLEX_BINARY(qc_div(acc, rhs));
        break;
    default:
        TIME_QCOMPLEX_BINARY(bench_case->qcomplex_fn(lhs, rhs));
        break;
    }

#undef TIME_QCOMPLEX_BINARY

    bench_stats_t stats = estimate(samples, repeats);
    free(samples);
    return stats;
}

static bench_stats_t time_number_ternary_qcomplex(const complex_ternary_case_t *bench_case,
                                                  int repeats)
{
    double *samples = calloc((size_t)repeats, sizeof(*samples));
    number_t x = num_create_from_qcomplex(qc_from_string(bench_case->x));
    number_t a = num_create_from_qcomplex(qc_from_string(bench_case->a));
    number_t b = num_create_from_qcomplex(qc_from_string(bench_case->b));
    int iters = scaled_iters(bench_case->iters);

    if (!samples)
        fail_case("number/qcomplex", bench_case->label, "sample allocation failed");

    for (int r = 0; r < repeats; ++r) {
        uint64_t start = now_ns();

        for (int i = 0; i < iters; ++i) {
            number_t out = bench_case->number_fn(x, a, b);

            consume_number(out);
            num_destroy(&out);
        }
        samples[r] = (double)(now_ns() - start) / (double)iters / 1000.0;
    }

    num_destroy(&x);
    num_destroy(&a);
    num_destroy(&b);
    bench_stats_t stats = estimate(samples, repeats);
    free(samples);
    return stats;
}

static bench_stats_t time_qcomplex_ternary(const complex_ternary_case_t *bench_case,
                                           int repeats)
{
    double *samples = calloc((size_t)repeats, sizeof(*samples));
    qcomplex_t x = qc_from_string(bench_case->x);
    qcomplex_t a = qc_from_string(bench_case->a);
    qcomplex_t b = qc_from_string(bench_case->b);
    int iters = scaled_iters(bench_case->iters);

    if (!samples)
        fail_case("qcomplex", bench_case->label, "sample allocation failed");

    for (int r = 0; r < repeats; ++r) {
        uint64_t start = now_ns();

        for (int i = 0; i < iters; ++i)
            consume_qcomplex(bench_case->qcomplex_fn(x, a, b));
        samples[r] = (double)(now_ns() - start) / (double)iters / 1000.0;
    }

    bench_stats_t stats = estimate(samples, repeats);
    free(samples);
    return stats;
}

static void print_header(const char *title)
{
    printf("\n== %s ==\n", title);
    printf("%-12s %12s %12s %10s\n", "case", "raw_us", "number_us", "ratio");
    printf("%-12s %12s %12s %10s\n", "----", "------", "---------", "-----");
    fflush(stdout);
}

static void print_row(const char *label,
                      bench_stats_t raw,
                      bench_stats_t number)
{
    double ratio = raw.median_us > 0.0 ? number.median_us / raw.median_us : 0.0;

    printf("%-12s %12.3f %12.3f %9.2fx\n",
           label,
           raw.median_us,
           number.median_us,
           ratio);
    fflush(stdout);
}

int main(void)
{
    int repeats = env_int("MARS_BENCH_REPEATS", 9);
    size_t precision = env_size("MARS_BENCH_BITS", 256u);
    int arithmetic_only = env_int("MARS_BENCH_ARITHMETIC_ONLY", 0);
    size_t real_binary_count = arithmetic_only
        ? 4u : sizeof(real_binary_cases) / sizeof(real_binary_cases[0]);
    size_t double_binary_count = arithmetic_only
        ? 4u : sizeof(double_binary_cases) / sizeof(double_binary_cases[0]);
    size_t complex_binary_count = arithmetic_only
        ? 4u : sizeof(complex_binary_cases) / sizeof(complex_binary_cases[0]);

    printf("number backend comparison bench\n");
    printf("repeats=%d precision=%zu bits scale=%d%s\n",
           repeats,
           precision,
           env_int("MARS_BENCH_SCALE", 1),
           arithmetic_only ? " arithmetic-only" : "");
    fflush(stdout);

    print_header("number created from double vs double");
    for (size_t i = 0u; i < double_binary_count; ++i)
        print_row(double_binary_cases[i].label,
                  time_double_binary(&double_binary_cases[i], repeats),
                  time_number_binary_double(&double_binary_cases[i], repeats));
    if (!arithmetic_only) {
        for (size_t i = 0u; i < sizeof(double_ternary_cases) / sizeof(double_ternary_cases[0]); ++i)
            print_row(double_ternary_cases[i].label,
                      time_double_ternary(&double_ternary_cases[i], repeats),
                      time_number_ternary_double(&double_ternary_cases[i], repeats));
        for (size_t i = 0u; i < sizeof(double_unary_cases) / sizeof(double_unary_cases[0]); ++i)
            print_row(double_unary_cases[i].label,
                      time_double_unary(&double_unary_cases[i], repeats),
                      time_number_unary_double(&double_unary_cases[i], repeats));
    }

    print_header("number MPFR vs mfloat_t");
    for (size_t i = 0u; i < real_binary_count; ++i)
        print_row(real_binary_cases[i].label,
                  time_mfloat_binary(&real_binary_cases[i], precision, repeats),
                  time_number_binary_mpfr(&real_binary_cases[i], precision, repeats));
    if (!arithmetic_only) {
        for (size_t i = 0u; i < sizeof(real_ternary_cases) / sizeof(real_ternary_cases[0]); ++i)
            print_row(real_ternary_cases[i].label,
                      time_mfloat_ternary(&real_ternary_cases[i], precision, repeats),
                      time_number_ternary_mpfr(&real_ternary_cases[i], precision, repeats));
        for (size_t i = 0u; i < sizeof(real_unary_cases) / sizeof(real_unary_cases[0]); ++i)
            print_row(real_unary_cases[i].label,
                      time_mfloat_unary(&real_unary_cases[i], precision, repeats),
                      time_number_unary_mpfr(&real_unary_cases[i], precision, repeats));
    }

    print_header("number created from qfloat_t vs qfloat_t");
    for (size_t i = 0u; i < real_binary_count; ++i)
        print_row(real_binary_cases[i].label,
                  time_qfloat_binary(&real_binary_cases[i], repeats),
                  time_number_binary_qfloat(&real_binary_cases[i], repeats));
    if (!arithmetic_only) {
        for (size_t i = 0u; i < sizeof(real_ternary_cases) / sizeof(real_ternary_cases[0]); ++i)
            print_row(real_ternary_cases[i].label,
                      time_qfloat_ternary(&real_ternary_cases[i], repeats),
                      time_number_ternary_qfloat(&real_ternary_cases[i], repeats));
        for (size_t i = 0u; i < sizeof(real_unary_cases) / sizeof(real_unary_cases[0]); ++i)
            print_row(real_unary_cases[i].label,
                      time_qfloat_unary(&real_unary_cases[i], repeats),
                      time_number_unary_qfloat(&real_unary_cases[i], repeats));
    }

    print_header("number created from qcomplex_t vs qcomplex_t");
    for (size_t i = 0u; i < complex_binary_count; ++i)
        print_row(complex_binary_cases[i].label,
                  time_qcomplex_binary(&complex_binary_cases[i], repeats),
                  time_number_binary_qcomplex(&complex_binary_cases[i], repeats));
    if (!arithmetic_only) {
        for (size_t i = 0u; i < sizeof(complex_ternary_cases) / sizeof(complex_ternary_cases[0]); ++i)
            print_row(complex_ternary_cases[i].label,
                      time_qcomplex_ternary(&complex_ternary_cases[i], repeats),
                      time_number_ternary_qcomplex(&complex_ternary_cases[i], repeats));
        for (size_t i = 0u; i < sizeof(complex_unary_cases) / sizeof(complex_unary_cases[0]); ++i)
            print_row(complex_unary_cases[i].label,
                      time_qcomplex_unary(&complex_unary_cases[i], repeats),
                      time_number_unary_qcomplex(&complex_unary_cases[i], repeats));
    }

    printf("\nsinks: %.3f %zu\n", bench_double_sink, bench_size_sink);
    return 0;
}
