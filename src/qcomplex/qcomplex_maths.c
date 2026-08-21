#include <limits.h>
#include <math.h>
#include <stdlib.h>

#define MARS_SHARED_QFLOAT_INTERNAL_ACCESS
#include "internal/qfloat_internal.h"
#include "qcomplex.h"

enum {
    QC_ERFINV_NEWTON_MAX_STEPS = 10,
    QC_ERFCINV_NEWTON_MAX_STEPS = 10,
    QC_GAMMAINV_NEWTON_MAX_STEPS = 20,
    QC_LAMBERT_WM1_HALLEY_MAX_STEPS = 60,
    QC_PRODUCTLOG_HALLEY_MAX_STEPS = 40,
    QC_GAMMAINC_SERIES_MAX_TERMS = 10000,
    QC_EI_SERIES_MAX_TERMS = 10000,
    QC_POLYLOG_SERIES_MAX_TERMS = 10000,
    QC_LAURICELLA_INITIAL_CAPACITY = 16,
    QC_LAURICELLA_MAX_COEFFICIENTS = 16384,
    QC_LAURICELLA_SMALL_TERM_RUN = 8,
    QC_HYPERGEOMETRIC_SERIES_MAX_TERMS = 10000
};

static qfloat_t qc_abs2_local(qcomplex_t z)
{
    return qf_add(qf_mul(qc_real(z), qc_real(z)), qf_mul(qc_imag(z), qc_imag(z)));
}

static qcomplex_t qc_faddeeva_inside(qcomplex_t z)
{
    qcomplex_t sum = QC_ZERO;

    for (size_t k = 0; k < QFI_FADDEEVA_TERM_COUNT; ++k) {
        qcomplex_t denom = qc_make(qc_real(z), qf_sub(qc_imag(z), QFI_FADDEEVA_AK[k]));
        sum = qc_add(sum, qc_div(qc_make(QFI_FADDEEVA_CK[k], QF_ZERO), denom));
    }

    /* inside = 1 + (2i / sqrt(pi)) * sum */
    qcomplex_t two_i_sum = qc_mul(qc_make(qf_from_double(0.0), qf_from_double(2.0)), sum);
    return qc_add(QC_ONE, qc_div(two_i_sum, QC_SQRT_PI));
}

qcomplex_t qc_erf(qcomplex_t z)
{
    if (qf_eq(qc_imag(z), qf_from_double(0.0)))
        return qc_make(qf_erf(qc_real(z)), QF_ZERO);
    /* Faddeeva requires Im(iz) = Re(z) >= 0; use antisymmetry erf(-z) = -erf(z) otherwise */
    if (qf_lt(qc_real(z), qf_from_double(0.0)))
        return qc_neg(qc_erf(qc_neg(z)));
    qcomplex_t iz = qc_make(qf_neg(qc_imag(z)), qc_real(z));
    return qc_sub(QC_ONE, qc_faddeeva_inside(iz));
}

qcomplex_t qc_erfc(qcomplex_t z)
{
    if (qf_eq(qc_imag(z), qf_from_double(0.0)))
        return qc_make(qf_erfc(qc_real(z)), QF_ZERO);
    /* Use erfc(-z) = 2 - erfc(z) to keep Re(z) >= 0 for Faddeeva */
    if (qf_lt(qc_real(z), qf_from_double(0.0)))
        return qc_sub(QC_TWO, qc_erfc(qc_neg(z)));
    qcomplex_t iz = qc_make(qf_neg(qc_imag(z)), qc_real(z));
    return qc_faddeeva_inside(iz);
}

qcomplex_t qc_erfinv(qcomplex_t z)
{
    /* Newton iteration: solve erf(w) = z.  Initial guess good for small |z|. */
    qcomplex_t w = qc_mul(z, qc_make(QF_SQRT_PI_OVER_TWO, QF_ZERO));

    for (int i = 0; i < QC_ERFINV_NEWTON_MAX_STEPS; i++) {
        qcomplex_t f = qc_sub(qc_erf(w), z);
        qcomplex_t fp = qc_mul(qc_make(QF_2_SQRTPI, QF_ZERO), qc_exp(qc_neg(qc_mul(w, w))));
        qcomplex_t delta = qc_div(f, fp);
        w = qc_sub(w, delta);
        if (qf_lt(qc_abs2_local(delta), qf_from_double(1e-60)))
            break;
    }
    return w;
}

qcomplex_t qc_erfcinv(qcomplex_t z)
{
    /* Newton iteration: solve erfc(w) = z.  Initial guess via erfinv(1-z). */
    qcomplex_t w = qc_erfinv(qc_sub(QC_ONE, z));

    for (int i = 0; i < QC_ERFCINV_NEWTON_MAX_STEPS; i++) {
        qcomplex_t f = qc_sub(qc_erfc(w), z);
        qcomplex_t fp = qc_mul(qc_make(QF_NEG_TWO_OVER_SQRT_PI, QF_ZERO), qc_exp(qc_neg(qc_mul(w, w))));
        qcomplex_t delta = qc_div(f, fp);
        w = qc_sub(w, delta);
        if (qf_lt(qc_abs(delta), qf_from_double(1e-30)))
            break;
    }
    return w;
}

/* Lanczos coefficients (g=7, N=9) */
static qcomplex_t lanczos_sum(qcomplex_t z_minus_one)
{
    qcomplex_t sum = qc_make(QFI_LANCZOS_C[0], QF_ZERO);
    for (size_t i = 1; i < QFI_LANCZOS_COEFF_COUNT; ++i)
        sum = qc_add(sum, qc_div(qc_make(QFI_LANCZOS_C[i], QF_ZERO),
                                 qc_add(z_minus_one, qc_make(qf_from_double((double)i), QF_ZERO))));
    return sum;
}

/* ------------------------------------------------------------ */

/* Complex Gamma */
/* ------------------------------------------------------------ */

qcomplex_t qc_gamma(qcomplex_t z)
{
    if (qf_eq(qc_imag(z), qf_from_double(0.0)))
        return qc_make(qf_gamma(qc_real(z)), QF_ZERO);

    if (qf_lt(qc_real(z), qf_from_double(0.5))) {
        /* Reflection: Γ(z) = π / (sin(πz) Γ(1-z)) */
        qcomplex_t sin_pi_z = qc_sin(qc_mul(z, QC_PI));
        qcomplex_t gamma_1mz = qc_gamma(qc_sub(QC_ONE, z));
        return qc_div(QC_PI, qc_mul(sin_pi_z, gamma_1mz));
    }

    qcomplex_t z_minus_one = qc_sub(z, QC_ONE);
    qcomplex_t sum = lanczos_sum(z_minus_one);
    qcomplex_t t = qc_add(z_minus_one, qc_make(QFI_LANCZOS_SHIFT, QF_ZERO)); /* g + 0.5, g = 7 */

    return qc_mul(qc_mul(qc_make(QF_SQRT_2PI, QF_ZERO), qc_pow(t, qc_sub(z, QC_HALF))), qc_mul(qc_exp(qc_neg(t)), sum));
}

qcomplex_t qc_lgamma(qcomplex_t z)
{
    if (qf_eq(qc_imag(z), qf_from_double(0.0)))
        return qc_make(qf_lgamma(qc_real(z)), QF_ZERO);

    if (qf_lt(qc_real(z), qf_from_double(0.5))) {
        /* Reflection: lgamma(z) = log(π) - log(sin(πz)) - lgamma(1-z) */
        qcomplex_t log_sin_piz = qc_log(qc_sin(qc_mul(z, QC_PI)));
        qcomplex_t lg_1mz = qc_lgamma(qc_sub(QC_ONE, z));
        return qc_sub(qc_sub(qc_log(QC_PI), log_sin_piz), lg_1mz);
    }

    qcomplex_t z_minus_one = qc_sub(z, QC_ONE);
    qcomplex_t sum = lanczos_sum(z_minus_one);
    qcomplex_t t = qc_add(z_minus_one, qc_make(QFI_LANCZOS_SHIFT, QF_ZERO));
    return qc_add(qc_add(QC_LOG_SQRT_2PI, qc_mul(qc_sub(z, QC_HALF), qc_log(t))), qc_add(qc_neg(t), qc_log(sum)));
}

qcomplex_t qc_digamma(qcomplex_t z)
{
    if (qf_eq(qc_imag(z), qf_from_double(0.0)))
        return qc_make(qf_digamma(qc_real(z)), QF_ZERO);

    if (qf_lt(qc_real(z), qf_from_double(0.5))) {
        /* Reflection: ψ(z) = ψ(1-z) - π cot(πz) */
        qcomplex_t pi_z = qc_mul(z, QC_PI);
        qcomplex_t cotpz = qc_div(qc_cos(pi_z), qc_sin(pi_z));
        return qc_sub(qc_digamma(qc_sub(QC_ONE, z)), qc_mul(QC_PI, cotpz));
    }

    /* Shift upward until |z| >= 10 */
    qcomplex_t psi = QC_ZERO;
    qcomplex_t zz = z;
    while (qf_lt(qc_abs(zz), qf_from_double(10.0))) {
        psi = qc_sub(psi, qc_div(QC_ONE, zz));
        zz = qc_add(zz, QC_ONE);
    }

    /* Asymptotic: ψ(z) ≈ log(z) - 1/(2z) - Σ B_{2k}/(2k z^{2k}) */
    qcomplex_t invz = qc_div(QC_ONE, zz);
    qcomplex_t result = qc_sub(qc_log(zz), qc_mul(QC_HALF, invz));

    qcomplex_t z2 = qc_mul(invz, invz);
    qcomplex_t z4 = qc_mul(z2, z2);
    qcomplex_t z6 = qc_mul(z4, z2);
    qcomplex_t z8 = qc_mul(z6, z2);
    qcomplex_t z10 = qc_mul(z8, z2);

    result = qc_sub(result, qc_mul(qc_make(QFI_BERNOULLI_B2, QF_ZERO), qc_mul(QC_HALF, z2)));
    result = qc_sub(result, qc_mul(qc_make(QFI_BERNOULLI_B4, QF_ZERO), qc_mul(qc_make(QF_QUARTER, QF_ZERO), z4)));
    result = qc_sub(result, qc_mul(qc_make(QFI_BERNOULLI_B6, QF_ZERO), qc_mul(qc_make(QF_ONE_SIXTH, QF_ZERO), z6)));
    result = qc_sub(result, qc_mul(qc_make(QFI_BERNOULLI_B8, QF_ZERO), qc_mul(qc_make(QF_ONE_EIGHTH, QF_ZERO), z8)));
    result = qc_sub(result, qc_mul(qc_make(QFI_BERNOULLI_B10, QF_ZERO), qc_mul(qc_make(QF_ONE_TENTH, QF_ZERO), z10)));

    return qc_add(result, psi);
}

qcomplex_t qc_trigamma(qcomplex_t z)
{
    if (qf_eq(qc_imag(z), qf_from_double(0.0)))
        return qc_make(qf_trigamma(qc_real(z)), QF_ZERO);

    if (qf_lt(qc_real(z), qf_from_double(0.5))) {
        /* Reflection: ψ₁(z) = π² csc²(πz) - ψ₁(1-z) */
        qcomplex_t pi_z = qc_mul(z, QC_PI);
        qcomplex_t csc = qc_div(QC_ONE, qc_sin(pi_z));
        qcomplex_t term = qc_mul(qc_make(QF_PI_SQUARED, QF_ZERO), qc_mul(csc, csc));
        return qc_sub(term, qc_trigamma(qc_sub(QC_ONE, z)));
    }

    /* Recurrence upward until |z| >= 10 */
    qcomplex_t accum = QC_ZERO;
    qcomplex_t zz = z;
    while (qf_lt(qc_abs2_local(zz), qf_from_double(100.0))) {
        qcomplex_t invz = qc_div(QC_ONE, zz);
        accum = qc_add(accum, qc_mul(invz, invz));
        zz = qc_add(zz, QC_ONE);
    }

    /* Asymptotic: ψ₁(z) ≈ 1/z + 1/(2z²) + Σ B_{2k}/z^{2k+1} */
    qcomplex_t invz = qc_div(QC_ONE, zz);
    qcomplex_t invz2 = qc_mul(invz, invz);
    qcomplex_t result = qc_add(invz, qc_mul(QC_HALF, invz2));

    qcomplex_t z3 = qc_mul(invz2, invz);
    qcomplex_t z5 = qc_mul(z3, invz2);
    qcomplex_t z7 = qc_mul(z5, invz2);
    qcomplex_t z9 = qc_mul(z7, invz2);

    result = qc_add(result, qc_mul(qc_make(QFI_BERNOULLI_B2, QF_ZERO), z3));
    result = qc_add(result, qc_mul(qc_make(QFI_BERNOULLI_B4, QF_ZERO), z5));
    result = qc_add(result, qc_mul(qc_make(QFI_BERNOULLI_B6, QF_ZERO), z7));
    result = qc_add(result, qc_mul(qc_make(QFI_BERNOULLI_B8, QF_ZERO), z9));

    return qc_add(result, accum);
}

qcomplex_t qc_tetragamma(qcomplex_t z)
{
    if (qf_eq(qc_imag(z), qf_from_double(0.0)))
        return qc_make(qf_tetragamma(qc_real(z)), QF_ZERO);

    if (qf_lt(qc_real(z), qf_from_double(0.5))) {
        /* Reflection: ψ₂(z) = ψ₂(1-z) + 2π³ csc²(πz) cot(πz) */
        qcomplex_t pi_z = qc_mul(z, QC_PI);
        qcomplex_t sin_pz = qc_sin(pi_z);
        qcomplex_t csc = qc_div(QC_ONE, sin_pz);
        qcomplex_t csc2 = qc_mul(csc, csc);
        qcomplex_t cot_pz = qc_div(qc_cos(pi_z), sin_pz);
        qcomplex_t term = qc_mul(qc_make(QF_2PI_CUBED, QF_ZERO), qc_mul(csc2, cot_pz));
        return qc_add(qc_tetragamma(qc_sub(QC_ONE, z)), term);
    }

    /* Recurrence upward until |z| >= 10 */
    qcomplex_t accum = QC_ZERO;
    qcomplex_t zz = z;
    while (qf_lt(qc_abs2_local(zz), qf_from_double(100.0))) {
        qcomplex_t invz = qc_div(QC_ONE, zz);
        qcomplex_t invz3 = qc_mul(invz, qc_mul(invz, invz));
        accum = qc_add(accum, qc_mul(QC_TWO, invz3));
        zz = qc_add(zz, QC_ONE);
    }

    /* Asymptotic: ψ₂(z) ≈ 1/z² + 1/z³ + Σ B_{2k}/z^{2k+2} */
    qcomplex_t invz = qc_div(QC_ONE, zz);
    qcomplex_t invz2 = qc_mul(invz, invz);
    qcomplex_t result = qc_add(invz2, qc_mul(invz2, invz));

    qcomplex_t z4 = qc_mul(invz2, invz2);
    qcomplex_t z6 = qc_mul(z4, invz2);
    qcomplex_t z8 = qc_mul(z6, invz2);
    qcomplex_t z10 = qc_mul(z8, invz2);

    result = qc_add(result, qc_mul(qc_make(QFI_BERNOULLI_B2, QF_ZERO), z4));
    result = qc_add(result, qc_mul(qc_make(QFI_BERNOULLI_B4, QF_ZERO), z6));
    result = qc_add(result, qc_mul(qc_make(QFI_BERNOULLI_B6, QF_ZERO), z8));
    result = qc_add(result, qc_mul(qc_make(QFI_BERNOULLI_B8, QF_ZERO), z10));

    return qc_add(result, accum);
}

static qfloat_t qc_factorial_uint(unsigned int n)
{
    qfloat_t out = QF_ONE;

    for (unsigned int k = 2u; k <= n; ++k)
        out = qf_mul_double(out, (double)k);
    return out;
}

static qfloat_t qc_bernoulli_even_term(size_t index)
{
    qfloat_t coeff;

    coeff = qf_div(qf_from_double(QFI_BERNOULLI_EVEN_TERMS[index].num),
                   qf_from_double(QFI_BERNOULLI_EVEN_TERMS[index].den));
    return QFI_BERNOULLI_EVEN_TERMS[index].sign < 0 ? qf_neg(coeff) : coeff;
}

static qcomplex_t qc_pow_uint(qcomplex_t z, unsigned int n)
{
    qcomplex_t result = QC_ONE;
    qcomplex_t base = z;

    while (n > 0u) {
        if ((n & 1u) != 0u)
            result = qc_mul(result, base);
        n >>= 1u;
        if (n != 0u)
            base = qc_mul(base, base);
    }
    return result;
}

static qcomplex_t qc_polygamma_asymp(unsigned int order, qcomplex_t z)
{
    qcomplex_t inv = qc_div(QC_ONE, z);
    qcomplex_t inv2 = qc_mul(inv, inv);
    qcomplex_t sum = qc_mul(qc_make(qc_factorial_uint(order - 1u), QF_ZERO), qc_pow_uint(inv, order));
    qcomplex_t power = qc_pow_uint(inv, order + 1u);
    qfloat_t order_fact = qc_factorial_uint(order);

    sum = qc_add(sum, qc_mul(qc_make(qf_mul(order_fact, QF_HALF), QF_ZERO), power));
    power = qc_mul(power, inv);

    for (size_t k = 0u; k < QFI_BERNOULLI_EVEN_TERM_COUNT; ++k) {
        unsigned int two_k = (unsigned int)(2u * (k + 1u));
        qfloat_t coeff = qc_bernoulli_even_term(k);

        for (unsigned int j = 1u; j < order; ++j)
            coeff = qf_mul_double(coeff, (double)(two_k + j));
        sum = qc_add(sum, qc_mul(qc_make(coeff, QF_ZERO), power));
        power = qc_mul(power, inv2);
    }

    return (order % 2u) == 0u ? qc_neg(sum) : sum;
}

qcomplex_t qc_polygamma(unsigned int order, qcomplex_t z)
{
    qcomplex_t zz;
    qcomplex_t accum;
    qfloat_t fact;
    int recurrence_sign;

    if (order == 0u)
        return qc_digamma(z);
    if (order == 1u)
        return qc_trigamma(z);
    if (order == 2u)
        return qc_tetragamma(z);
    if (qf_eq(qc_imag(z), QF_ZERO))
        return qc_make(qf_polygamma(order, qc_real(z)), QF_ZERO);

    zz = z;
    accum = QC_ZERO;
    fact = qc_factorial_uint(order);
    recurrence_sign = (order % 2u) == 0u ? 1 : -1;

    while (qf_lt(qc_abs2_local(zz), qf_from_double(400.0))) {
        qcomplex_t term = qc_mul(qc_make(fact, QF_ZERO), qc_div(QC_ONE, qc_pow_uint(zz, order + 1u)));

        accum = recurrence_sign > 0 ? qc_sub(accum, term) : qc_add(accum, term);
        zz = qc_add(zz, QC_ONE);
    }

    return qc_add(qc_polygamma_asymp(order, zz), accum);
}

static void qc_zeta_euler_maclaurin(qcomplex_t s, qcomplex_t *value_out, qcomplex_t *derivative_out)
{
    const unsigned int endpoint = 64u;
    qcomplex_t sum = QC_ZERO;
    qcomplex_t derivative = QC_ZERO;
    qfloat_t log_endpoint = qf_log(qf_from_double((double)endpoint));
    qfloat_t factorial = QF_ONE;

    for (unsigned int n = 1u; n < endpoint; ++n) {
        qfloat_t log_n = qf_log(qf_from_double((double)n));
        qcomplex_t term = qc_exp(qc_mul(qc_neg(s), qc_make(log_n, QF_ZERO)));

        sum = qc_add(sum, term);
        derivative = qc_sub(derivative, qc_mul(qc_make(log_n, QF_ZERO), term));
    }

    {
        qcomplex_t s_minus_one = qc_sub(s, QC_ONE);
        qcomplex_t endpoint_power = qc_exp(qc_mul(qc_sub(QC_ONE, s), qc_make(log_endpoint, QF_ZERO)));
        qcomplex_t tail = qc_div(endpoint_power, s_minus_one);
        qcomplex_t tail_derivative = qc_mul(
            endpoint_power,
            qc_sub(qc_neg(qc_div(qc_make(log_endpoint, QF_ZERO), s_minus_one)),
                   qc_div(QC_ONE, qc_mul(s_minus_one, s_minus_one))));
        qcomplex_t half_power = qc_mul(QC_HALF, qc_exp(qc_mul(qc_neg(s), qc_make(log_endpoint, QF_ZERO))));

        sum = qc_add(sum, tail);
        derivative = qc_add(derivative, tail_derivative);
        sum = qc_add(sum, half_power);
        derivative = qc_sub(derivative, qc_mul(qc_make(log_endpoint, QF_ZERO), half_power));
    }

    for (size_t k = 0u; k < QFI_BERNOULLI_EVEN_TERM_COUNT; ++k) {
        unsigned int rising_count = (unsigned int)(2u * (k + 1u) - 1u);
        qcomplex_t rising = QC_ONE;
        qcomplex_t rising_derivative = QC_ZERO;
        qfloat_t bernoulli = qc_bernoulli_even_term(k);
        qcomplex_t power;
        qfloat_t coefficient;

        factorial = qf_mul_double(factorial, (double)(2u * k + 1u));
        factorial = qf_mul_double(factorial, (double)(2u * k + 2u));
        for (unsigned int j = 0u; j < rising_count; ++j) {
            qcomplex_t factor = qc_add(s, qc_make(qf_from_double((double)j), QF_ZERO));

            rising_derivative = qc_add(qc_mul(rising_derivative, factor), rising);
            rising = qc_mul(rising, factor);
        }
        power = qc_exp(qc_mul(qc_neg(qc_add(s, qc_make(qf_from_double((double)rising_count), QF_ZERO))),
                              qc_make(log_endpoint, QF_ZERO)));
        coefficient = qf_div(bernoulli, factorial);
        sum = qc_add(sum, qc_mul(qc_make(coefficient, QF_ZERO), qc_mul(rising, power)));
        derivative = qc_add(
            derivative,
            qc_mul(qc_make(coefficient, QF_ZERO),
                   qc_mul(qc_sub(rising_derivative, qc_mul(rising, qc_make(log_endpoint, QF_ZERO))), power)));
    }

    if (value_out)
        *value_out = sum;
    if (derivative_out)
        *derivative_out = derivative;
}

static void qc_zetah_euler_maclaurin(qcomplex_t s, qcomplex_t a, qcomplex_t *value_out,
                                     qcomplex_t *derivative_out)
{
    const unsigned int term_count = 64u;
    qcomplex_t sum = QC_ZERO;
    qcomplex_t derivative = QC_ZERO;
    qcomplex_t endpoint = qc_add(a, qc_make(qf_from_double((double)term_count), QF_ZERO));
    qcomplex_t log_endpoint = qc_log(endpoint);
    qfloat_t factorial = QF_ONE;

    for (unsigned int n = 0u; n < term_count; ++n) {
        qcomplex_t base = qc_add(a, qc_make(qf_from_double((double)n), QF_ZERO));
        qcomplex_t log_base = qc_log(base);
        qcomplex_t term = qc_exp(qc_mul(qc_neg(s), log_base));

        sum = qc_add(sum, term);
        derivative = qc_sub(derivative, qc_mul(log_base, term));
    }

    {
        qcomplex_t s_minus_one = qc_sub(s, QC_ONE);
        qcomplex_t endpoint_power = qc_exp(qc_mul(qc_sub(QC_ONE, s), log_endpoint));
        qcomplex_t tail = qc_div(endpoint_power, s_minus_one);
        qcomplex_t tail_derivative = qc_mul(
            endpoint_power,
            qc_sub(qc_neg(qc_div(log_endpoint, s_minus_one)), qc_div(QC_ONE, qc_mul(s_minus_one, s_minus_one))));
        qcomplex_t half_power = qc_mul(QC_HALF, qc_exp(qc_mul(qc_neg(s), log_endpoint)));

        sum = qc_add(sum, tail);
        derivative = qc_add(derivative, tail_derivative);
        sum = qc_add(sum, half_power);
        derivative = qc_sub(derivative, qc_mul(log_endpoint, half_power));
    }

    for (size_t k = 0u; k < QFI_BERNOULLI_EVEN_TERM_COUNT; ++k) {
        unsigned int rising_count = (unsigned int)(2u * (k + 1u) - 1u);
        qcomplex_t rising = QC_ONE;
        qcomplex_t rising_derivative = QC_ZERO;
        qfloat_t bernoulli = qc_bernoulli_even_term(k);
        qcomplex_t power;
        qfloat_t coefficient;

        factorial = qf_mul_double(factorial, (double)(2u * k + 1u));
        factorial = qf_mul_double(factorial, (double)(2u * k + 2u));
        for (unsigned int j = 0u; j < rising_count; ++j) {
            qcomplex_t factor = qc_add(s, qc_make(qf_from_double((double)j), QF_ZERO));

            rising_derivative = qc_add(qc_mul(rising_derivative, factor), rising);
            rising = qc_mul(rising, factor);
        }
        power = qc_exp(qc_mul(qc_neg(qc_add(s, qc_make(qf_from_double((double)rising_count), QF_ZERO))),
                              log_endpoint));
        coefficient = qf_div(bernoulli, factorial);
        sum = qc_add(sum, qc_mul(qc_make(coefficient, QF_ZERO), qc_mul(rising, power)));
        derivative = qc_add(
            derivative,
            qc_mul(qc_make(coefficient, QF_ZERO),
                   qc_mul(qc_sub(rising_derivative, qc_mul(rising, log_endpoint)), power)));
    }

    if (value_out)
        *value_out = sum;
    if (derivative_out)
        *derivative_out = derivative;
}

/* Evaluate the Riemann zeta function across the complex analytic continuation. */
qcomplex_t qc_zeta(qcomplex_t s)
{
    qcomplex_t value;

    if (qf_eq(qc_imag(s), QF_ZERO))
        return qc_make(qf_zeta(qc_real(s)), QF_ZERO);
    if (qc_isnan(s) || qc_isinf(s))
        return QC_NAN;
    if (qf_lt(qc_real(s), QF_ZERO)) {
        qcomplex_t one_minus_s = qc_sub(QC_ONE, s);
        qcomplex_t two_power = qc_exp(qc_mul(s, qc_make(qf_log(QF_TWO), QF_ZERO)));
        qcomplex_t pi_power = qc_exp(qc_mul(qc_sub(s, QC_ONE), qc_make(qf_log(QF_PI), QF_ZERO)));
        qcomplex_t sine = qc_sin(qc_mul(qc_make(qf_mul(QF_HALF, QF_PI), QF_ZERO), s));

        return qc_mul(qc_mul(qc_mul(qc_mul(two_power, pi_power), sine), qc_gamma(one_minus_s)), qc_zeta(one_minus_s));
    }

    qc_zeta_euler_maclaurin(s, &value, NULL);
    return value;
}

/* Evaluate the Hurwitz zeta function on its principal branch. */
qcomplex_t qc_zetah(qcomplex_t s, qcomplex_t a)
{
    qcomplex_t value;

    if (qf_eq(qc_imag(s), QF_ZERO) && qf_eq(qc_imag(a), QF_ZERO) && qf_gt(qc_real(a), QF_ZERO))
        return qc_make(qf_zetah(qc_real(s), qc_real(a)), QF_ZERO);
    if (qc_isnan(s) || qc_isnan(a) || qc_isinf(s) || qc_isinf(a))
        return QC_NAN;
    if (qf_eq(qc_real(s), QF_ONE) && qf_eq(qc_imag(s), QF_ZERO))
        return QC_INF;
    if (qf_eq(qc_imag(a), QF_ZERO) && qf_le(qc_real(a), QF_ZERO) &&
        qf_eq(qc_real(a), qf_floor(qc_real(a))))
        return QC_NAN;
    qc_zetah_euler_maclaurin(s, a, &value, NULL);
    return value;
}

/* Evaluate the first Hurwitz zeta derivative with respect to its exponent. */
qcomplex_t qc_zatahp(qcomplex_t s, qcomplex_t a)
{
    qcomplex_t derivative;

    if (qf_eq(qc_imag(s), QF_ZERO) && qf_eq(qc_imag(a), QF_ZERO) && qf_gt(qc_real(a), QF_ZERO))
        return qc_make(qf_zatahp(qc_real(s), qc_real(a)), QF_ZERO);
    if (qc_isnan(s) || qc_isnan(a) || qc_isinf(s) || qc_isinf(a))
        return QC_NAN;
    if (qf_eq(qc_real(s), QF_ONE) && qf_eq(qc_imag(s), QF_ZERO))
        return qc_neg(QC_INF);
    if (qf_eq(qc_imag(a), QF_ZERO) && qf_le(qc_real(a), QF_ZERO) &&
        qf_eq(qc_real(a), qf_floor(qc_real(a))))
        return QC_NAN;
    qc_zetah_euler_maclaurin(s, a, NULL, &derivative);
    return derivative;
}

/* Evaluate the first derivative of the Riemann zeta function. */
qcomplex_t qc_zetap(qcomplex_t s)
{
    qcomplex_t derivative;

    if (qf_eq(qc_imag(s), QF_ZERO))
        return qc_make(qf_zetap(qc_real(s)), QF_ZERO);
    if (qc_isnan(s) || qc_isinf(s))
        return QC_NAN;
    if (qf_lt(qc_real(s), QF_ZERO)) {
        qcomplex_t one_minus_s = qc_sub(QC_ONE, s);
        qcomplex_t reflected = qc_zeta(one_minus_s);
        qcomplex_t logarithmic_derivative = qc_make(qf_add(qf_log(QF_TWO), qf_log(QF_PI)), QF_ZERO);
        qcomplex_t cotangent = qc_cot(qc_mul(qc_make(qf_mul(QF_HALF, QF_PI), QF_ZERO), s));

        logarithmic_derivative = qc_add(
            logarithmic_derivative, qc_mul(qc_make(qf_mul(QF_HALF, QF_PI), QF_ZERO), cotangent));
        logarithmic_derivative = qc_sub(logarithmic_derivative, qc_digamma(one_minus_s));
        logarithmic_derivative = qc_sub(logarithmic_derivative, qc_div(qc_zetap(one_minus_s), reflected));
        return qc_mul(qc_zeta(s), logarithmic_derivative);
    }

    qc_zeta_euler_maclaurin(s, NULL, &derivative);
    return derivative;
}

qcomplex_t qc_gammainv(qcomplex_t z)
{
    if (qf_eq(qc_imag(z), qf_from_double(0.0)))
        return qc_make(qf_gammainv(qc_real(z)), QF_ZERO);

    if (qc_isnan(z) || (qf_eq(qc_real(z), QF_ZERO) && qf_eq(qc_imag(z), QF_ZERO)))
        return qc_make(QF_NAN, QF_NAN);

    qcomplex_t logz = qc_log(z);
    qcomplex_t w;
    w = qc_add(qc_make(QF_ONE_AND_HALF, QF_ZERO), logz);

    for (int i = 0; i < QC_GAMMAINV_NEWTON_MAX_STEPS; i++) {
        qcomplex_t delta = qc_div(qc_sub(qc_lgamma(w), logz), qc_digamma(w));
        w = qc_sub(w, delta);
        if (qf_lt(qc_abs(delta), qf_from_double(1e-30)))
            break;
    }
    return w;
}

qcomplex_t qc_beta(qcomplex_t a, qcomplex_t b)
{
    if (qf_eq(qc_imag(a), qf_from_double(0.0)) && qf_eq(qc_imag(b), qf_from_double(0.0)))
        return qc_make(qf_beta(qc_real(a), qc_real(b)), QF_ZERO);

    return qc_exp(qc_logbeta(a, b));
}

qcomplex_t qc_logbeta(qcomplex_t a, qcomplex_t b)
{
    if (qf_eq(qc_imag(a), qf_from_double(0.0)) && qf_eq(qc_imag(b), qf_from_double(0.0)))
        return qc_make(qf_logbeta(qc_real(a), qc_real(b)), QF_ZERO);

    return qc_sub(qc_add(qc_lgamma(a), qc_lgamma(b)), qc_lgamma(qc_add(a, b)));
}

qcomplex_t qc_binomial(qcomplex_t a, qcomplex_t b)
{
    if (qf_eq(qc_imag(a), qf_from_double(0.0)) && qf_eq(qc_imag(b), qf_from_double(0.0)))
        return qc_make(qf_binomial(qc_real(a), qc_real(b)), QF_ZERO);

    /* C(a,b) = Γ(a+1) / (Γ(b+1) Γ(a-b+1)) */
    qcomplex_t a1 = qc_add(a, QC_ONE);
    qcomplex_t b1 = qc_add(b, QC_ONE);
    qcomplex_t amb1 = qc_add(qc_sub(a, b), QC_ONE);
    return qc_div(qc_gamma(a1), qc_mul(qc_gamma(b1), qc_gamma(amb1)));
}

qcomplex_t qc_beta_pdf(qcomplex_t x, qcomplex_t a, qcomplex_t b)
{
    if (qf_eq(qc_imag(x), qf_from_double(0.0)) && qf_eq(qc_imag(a), qf_from_double(0.0)) &&
        qf_eq(qc_imag(b), qf_from_double(0.0)))
        return qc_make(qf_beta_pdf(qc_real(x), qc_real(a), qc_real(b)), QF_ZERO);

    /* f(x; a, b) = x^(a-1) * (1-x)^(b-1) / B(a,b) */
    qcomplex_t one_minus_x = qc_sub(QC_ONE, x);
    qcomplex_t num = qc_mul(qc_pow(x, qc_sub(a, QC_ONE)), qc_pow(one_minus_x, qc_sub(b, QC_ONE)));
    return qc_div(num, qc_beta(a, b));
}

qcomplex_t qc_logbeta_pdf(qcomplex_t x, qcomplex_t a, qcomplex_t b)
{
    if (qf_eq(qc_imag(x), qf_from_double(0.0)) && qf_eq(qc_imag(a), qf_from_double(0.0)) &&
        qf_eq(qc_imag(b), qf_from_double(0.0)))
        return qc_make(qf_logbeta_pdf(qc_real(x), qc_real(a), qc_real(b)), QF_ZERO);

    /* log f(x; a,b) = (a-1)log(x) + (b-1)log(1-x) - log B(a,b) */
    return qc_sub(qc_add(qc_mul(qc_sub(a, QC_ONE), qc_log(x)), qc_mul(qc_sub(b, QC_ONE), qc_log(qc_sub(QC_ONE, x)))),
                  qc_logbeta(a, b));
}

qcomplex_t qc_normal_pdf(qcomplex_t z)
{
    if (qf_eq(qc_imag(z), qf_from_double(0.0)))
        return qc_make(qf_normal_pdf(qc_real(z)), QF_ZERO);

    /* φ(z) = exp(-z²/2) / sqrt(2π) */
    return qc_mul(QC_INV_SQRT_2PI, qc_exp(qc_mul(qc_neg(QC_HALF), qc_mul(z, z))));
}

qcomplex_t qc_normal_cdf(qcomplex_t z)
{
    if (qf_eq(qc_imag(z), qf_from_double(0.0)))
        return qc_make(qf_normal_cdf(qc_real(z)), QF_ZERO);

    /* Φ(z) = 0.5 * (1 + erf(z / sqrt(2))) */
    return qc_mul(QC_HALF, qc_add(QC_ONE, qc_erf(qc_div(z, QC_SQRT2))));
}

qcomplex_t qc_normal_logpdf(qcomplex_t z)
{
    if (qf_eq(qc_imag(z), qf_from_double(0.0)))
        return qc_make(qf_normal_logpdf(qc_real(z)), QF_ZERO);

    /* log φ(z) = -z²/2 - log(2π)/2 */
    return qc_sub(qc_mul(qc_neg(QC_HALF), qc_mul(z, z)), QC_LOG_SQRT_2PI);
}

static qcomplex_t qc_lambert_w_series_guess(qcomplex_t z, int branch)
{
    qcomplex_t ez = qc_mul(QC_E, z);
    qcomplex_t p = qc_sqrt(qc_mul(QC_TWO, qc_add(QC_ONE, ez)));
    qcomplex_t p2 = qc_mul(p, p);
    qcomplex_t p3 = qc_mul(p2, p);
    qcomplex_t sign_p = (branch == -1) ? qc_neg(p) : p;
    qcomplex_t sign_p3 = (branch == -1) ? qc_neg(p3) : p3;
    qcomplex_t w = QC_NEG_ONE;

    w = qc_add(w, sign_p);
    w = qc_sub(w, qc_mul(qc_make(QF_ONE_THIRD, QF_ZERO), p2));
    w = qc_add(w, qc_mul(qc_make(QFI_ELEVEN_OVER_SEVENTY_TWO, QF_ZERO), sign_p3));
    return w;
}

static qcomplex_t qc_lambert_w_asymptotic_guess(qcomplex_t z, int branch)
{
    qfloat_t two_pi = qf_mul_double(QF_PI, 2.0);
    qcomplex_t L1 = qc_log(z);
    L1 = qc_make(qc_real(L1), qf_add(qc_imag(L1), qf_mul_double(two_pi, (double)branch)));

    if (qf_eq(qc_abs2_local(L1), QF_ZERO))
        return L1;

    qcomplex_t L2 = qc_log(L1);
    return qc_add(qc_sub(L1, L2), qc_div(L2, L1));
}

static qcomplex_t qc_lambert_wm1_complex(qcomplex_t z)
{
    qfloat_t zero_tol = qf_from_double(1e-30);

    if (qc_isnan(z))
        return qc_make(QF_NAN, QF_NAN);

    if (qf_eq(qc_imag(z), qf_from_double(0.0)) && qf_ge(qc_real(z), QF_NEG_INV_E) && qf_lt(qc_real(z), QF_ZERO))
        return qc_make(qf_lambert_wm1(qc_real(z)), QF_ZERO);

    if (qf_eq(qc_real(z), QF_ZERO) && qf_eq(qc_imag(z), QF_ZERO))
        return qc_make(QF_NINF, QF_NAN);

    qcomplex_t branch_probe = qc_add(qc_mul(QC_E, z), QC_ONE);
    qcomplex_t w = qf_lt(qc_abs2_local(branch_probe), qf_from_double(0.0625)) ? qc_lambert_w_series_guess(z, -1)
                                                                              : qc_lambert_w_asymptotic_guess(z, -1);

    for (int i = 0; i < QC_LAMBERT_WM1_HALLEY_MAX_STEPS; i++) {
        qcomplex_t ew = qc_exp(w);
        qcomplex_t wew = qc_mul(w, ew);
        qcomplex_t f = qc_sub(wew, z);
        qcomplex_t wp1 = qc_add(w, QC_ONE);
        qcomplex_t denom;

        if (qf_lt(qc_abs(wp1), zero_tol)) {
            denom = ew;
        } else {
            qcomplex_t halley_corr = qc_div(qc_mul(qc_add(w, QC_TWO), f), qc_mul(QC_TWO, wp1));
            denom = qc_sub(qc_mul(ew, wp1), halley_corr);
        }

        qcomplex_t delta = qc_div(f, denom);
        w = qc_sub(w, delta);

        if (qf_lt(qc_abs2_local(delta), qf_mul(zero_tol, zero_tol)))
            break;
    }

    return w;
}

qcomplex_t qc_lambert_wm1(qcomplex_t z)
{
    return qc_lambert_wm1_complex(z);
}

qcomplex_t qc_lambert_wn(int branch, qcomplex_t z)
{
    qfloat_t zero_tol = qf_from_double(1e-30);
    qcomplex_t w;

    if (branch == 0)
        return qc_productlog(z);
    if (branch == -1)
        return qc_lambert_wm1_complex(z);
    if (qc_isnan(z))
        return QC_NAN;
    if (qf_eq(qc_real(z), QF_ZERO) && qf_eq(qc_imag(z), QF_ZERO))
        return QC_NAN;

    w = qc_lambert_w_asymptotic_guess(z, branch);

    for (int i = 0; i < QC_LAMBERT_WM1_HALLEY_MAX_STEPS; i++) {
        qcomplex_t ew = qc_exp(w);
        qcomplex_t wew = qc_mul(w, ew);
        qcomplex_t f = qc_sub(wew, z);
        qcomplex_t wp1 = qc_add(w, QC_ONE);
        qcomplex_t denom;

        if (qf_lt(qc_abs(wp1), zero_tol)) {
            denom = ew;
        } else {
            qcomplex_t halley_corr = qc_div(qc_mul(qc_add(w, QC_TWO), f), qc_mul(QC_TWO, wp1));
            denom = qc_sub(qc_mul(ew, wp1), halley_corr);
        }

        qcomplex_t delta = qc_div(f, denom);
        w = qc_sub(w, delta);

        if (qf_lt(qc_abs2_local(delta), qf_mul(zero_tol, zero_tol)))
            break;
    }

    return w;
}

qcomplex_t qc_productlog(qcomplex_t z)
{
    if (qf_eq(qc_imag(z), qf_from_double(0.0)) && qf_ge(qc_real(z), QF_NEG_INV_E))
        return qc_make(qf_productlog(qc_real(z)), QF_ZERO);

    /* Halley iteration on the principal branch: w e^w = z.
     * Near zero the principal solution is near z itself; starting at log(z)
     * can converge to a non-principal branch. */
    qcomplex_t w = qf_lt(qc_abs2_local(z), qf_from_double(0.25)) ? z : qc_log(z);
    for (int i = 0; i < QC_PRODUCTLOG_HALLEY_MAX_STEPS; i++) {
        qcomplex_t ew = qc_exp(w);
        qcomplex_t wew = qc_mul(w, ew);
        qcomplex_t f = qc_sub(wew, z);
        qcomplex_t wp1 = qc_add(w, QC_ONE);
        qcomplex_t f1 = qc_mul(ew, wp1);
        qcomplex_t f2 = qc_mul(ew, qc_add(w, QC_TWO));
        qcomplex_t corr2 = qc_mul(QC_HALF, qc_mul(qc_div(f, f1), f2));
        qcomplex_t delta = qc_div(f, qc_sub(f1, corr2));
        w = qc_sub(w, delta);
        if (qf_lt(qc_abs(delta), qf_from_double(1e-30)))
            break;
    }
    return w;
}

static qcomplex_t qc_gammainc_lower_series(qcomplex_t s, qcomplex_t x)
{
    qfloat_t tol = qf_from_double(1e-30);

    qcomplex_t term = qc_div(QC_ONE, s);
    qcomplex_t sum = term;

    for (int i = 1; i < QC_GAMMAINC_SERIES_MAX_TERMS; i++) {
        term = qc_mul(term, qc_div(x, qc_add(s, qc_make(qf_from_double((double)i), QF_ZERO))));
        sum = qc_add(sum, term);
        if (qf_lt(qc_abs(term), qf_mul(tol, qc_abs(sum))))
            break;
    }

    return qc_mul(qc_mul(qc_pow(x, s), qc_exp(qc_neg(x))), sum);
}

qcomplex_t qc_gammainc_lower(qcomplex_t s, qcomplex_t x)
{
    if (qf_eq(qc_imag(s), qf_from_double(0.0)) && qf_eq(qc_imag(x), qf_from_double(0.0)))
        return qc_make(qf_gammainc_lower(qc_real(s), qc_real(x)), QF_ZERO);

    return qc_gammainc_lower_series(s, x);
}

qcomplex_t qc_gammainc_upper(qcomplex_t s, qcomplex_t x)
{
    if (qf_eq(qc_imag(s), qf_from_double(0.0)) && qf_eq(qc_imag(x), qf_from_double(0.0)))
        return qc_make(qf_gammainc_upper(qc_real(s), qc_real(x)), QF_ZERO);

    return qc_sub(qc_gamma(s), qc_gammainc_lower_series(s, x));
}

qcomplex_t qc_gammainc_P(qcomplex_t s, qcomplex_t x)
{
    if (qf_eq(qc_imag(s), qf_from_double(0.0)) && qf_eq(qc_imag(x), qf_from_double(0.0)))
        return qc_make(qf_gammainc_P(qc_real(s), qc_real(x)), QF_ZERO);

    return qc_div(qc_gammainc_lower_series(s, x), qc_gamma(s));
}

qcomplex_t qc_gammainc_Q(qcomplex_t s, qcomplex_t x)
{
    if (qf_eq(qc_imag(s), qf_from_double(0.0)) && qf_eq(qc_imag(x), qf_from_double(0.0)))
        return qc_make(qf_gammainc_Q(qc_real(s), qc_real(x)), QF_ZERO);

    return qc_sub(QC_ONE, qc_gammainc_P(s, x));
}

qcomplex_t qc_Ei(qcomplex_t z)
{
    if (qf_eq(qc_imag(z), qf_from_double(0.0)))
        return qc_make(qf_Ei(qc_real(z)), QF_ZERO);

    /* Ei(z) = γ + log(z) + Σ_{k=1}^∞ z^k / (k × k!) */
    qfloat_t tol = qf_from_double(1e-30);
    qcomplex_t sum = qc_add(QC_EULER_MASCHERONI, qc_log(z));

    qfloat_t one = qf_from_double(1.0);
    qfloat_t kf = one;   /* k */
    qfloat_t fact = one; /* k! */
    qcomplex_t term = z; /* z^k */

    for (int k = 1; k < QC_EI_SERIES_MAX_TERMS; k++) {
        qcomplex_t add = qc_div(term, qc_make(qf_mul(kf, fact), QF_ZERO));
        sum = qc_add(sum, add);
        if (qf_lt(qc_abs(add), qf_mul(tol, qc_abs(sum))))
            break;
        kf = qf_add(kf, one);
        fact = qf_mul(fact, kf);
        term = qc_mul(term, z);
    }
    return sum;
}

qcomplex_t qc_E1(qcomplex_t z)
{
    if (qf_eq(qc_imag(z), qf_from_double(0.0)))
        return qc_make(qf_E1(qc_real(z)), QF_ZERO);

    return qc_neg(qc_Ei(qc_neg(z)));
}

static int qc_to_integer_order(qcomplex_t value, int *order)
{
    double raw;
    double rounded;

    if (!order || !qf_eq(qc_imag(value), QF_ZERO) || qf_isnan(qc_real(value)) || qf_isinf(qc_real(value)))
        return 0;

    raw = qf_to_double(qc_real(value));
    rounded = nearbyint(raw);
    if (fabs(raw - rounded) > 1e-28 || rounded < (double)INT_MIN || rounded > (double)INT_MAX)
        return 0;

    *order = (int)rounded;
    return 1;
}

static qcomplex_t qc_polylog_series_int(int order, qcomplex_t z)
{
    qcomplex_t sum = QC_ZERO;
    qcomplex_t term = z;
    qfloat_t tol = qf_from_double(1e-34);

    for (int k = 1; k < QC_POLYLOG_SERIES_MAX_TERMS; ++k) {
        qfloat_t denom = qf_pow_int(qf_from_double((double)k), order);
        qcomplex_t add = qc_div(term, qc_make(denom, QF_ZERO));

        sum = qc_add(sum, add);
        if (qf_le(qc_abs(add), qf_mul(tol, qf_add(QF_ONE, qc_abs(sum)))))
            break;
        term = qc_mul(term, z);
    }

    return sum;
}

qcomplex_t qc_dilog(qcomplex_t z)
{
    qcomplex_t pi2_over_6;
    qcomplex_t log_term;
    qcomplex_t inner;

    if (qc_isnan(z) || qc_isinf(z))
        return QC_NAN;
    if (qc_eq(z, QC_ZERO))
        return QC_ZERO;
    if (qc_eq(z, QC_ONE))
        return qc_make(qf_div(qf_mul(QF_PI, QF_PI), qf_from_double(6.0)), QF_ZERO);
    if (qf_eq(qc_imag(z), QF_ZERO) && qf_le(qc_real(z), QF_ONE))
        return qc_make(qf_dilog(qc_real(z)), QF_ZERO);

    if (qf_gt(qc_abs(z), QF_ONE)) {
        qcomplex_t inv_z = qc_div(QC_ONE, z);
        qcomplex_t log_neg_z = qc_log(qc_neg(z));
        qcomplex_t half_log_sq = qc_mul(qc_make(qf_from_double(0.5), QF_ZERO), qc_mul(log_neg_z, log_neg_z));

        inner = qc_dilog(inv_z);
        pi2_over_6 = qc_make(qf_div(qf_mul(QF_PI, QF_PI), qf_from_double(6.0)), QF_ZERO);
        return qc_neg(qc_add(qc_add(inner, pi2_over_6), half_log_sq));
    }

    if (qf_lt(qc_abs(qc_sub(QC_ONE, z)), qf_from_double(0.5))) {
        qcomplex_t one_minus_z = qc_sub(QC_ONE, z);

        pi2_over_6 = qc_make(qf_div(qf_mul(QF_PI, QF_PI), qf_from_double(6.0)), QF_ZERO);
        log_term = qc_mul(qc_log(z), qc_log(one_minus_z));
        inner = qc_dilog(one_minus_z);
        return qc_sub(qc_sub(pi2_over_6, log_term), inner);
    }

    return qc_polylog_series_int(2, z);
}

qcomplex_t qc_polylog(qcomplex_t s, qcomplex_t z)
{
    int order;

    if (!qc_to_integer_order(s, &order))
        return QC_NAN;
    if (order < 0)
        return QC_NAN;
    if (order == 0)
        return qc_div(z, qc_sub(QC_ONE, z));
    if (order == 1)
        return qc_neg(qc_log(qc_sub(QC_ONE, z)));
    if (order == 2)
        return qc_dilog(z);
    if (qf_ge(qc_abs(z), qf_from_double(0.95)))
        return QC_NAN;

    return qc_polylog_series_int(order, z);
}

static int qc_lauricella_append(qcomplex_t **values, size_t *count, size_t *capacity, qcomplex_t value)
{
    qcomplex_t *grown;
    size_t next_capacity;

    if (!values || !count || !capacity || *count >= QC_LAURICELLA_MAX_COEFFICIENTS)
        return 0;
    if (*count < *capacity) {
        (*values)[(*count)++] = value;
        return 1;
    }

    next_capacity = *capacity == 0u ? QC_LAURICELLA_INITIAL_CAPACITY : *capacity * 2u;
    if (next_capacity > QC_LAURICELLA_MAX_COEFFICIENTS)
        next_capacity = QC_LAURICELLA_MAX_COEFFICIENTS;
    grown = realloc(*values, next_capacity * sizeof(*grown));
    if (!grown)
        return 0;
    *values = grown;
    *capacity = next_capacity;
    (*values)[(*count)++] = value;
    return 1;
}

static int qc_lauricella_factor(qcomplex_t parameter, qcomplex_t variable, qfloat_t tolerance, qcomplex_t **values_out,
                                size_t *count_out)
{
    qcomplex_t *values = NULL;
    qcomplex_t term = QC_ONE;
    qfloat_t magnitude_sum = QF_ZERO;
    qfloat_t previous_magnitude = QF_INF;
    size_t count = 0u;
    size_t capacity = 0u;
    unsigned int small_terms = 0u;

    if (!values_out || !count_out)
        return 0;

    for (size_t k = 0u; k < QC_LAURICELLA_MAX_COEFFICIENTS; ++k) {
        qfloat_t magnitude = qc_abs(term);
        qfloat_t scale;

        if (k > 0u && qc_eq(term, QC_ZERO)) {
            *values_out = values;
            *count_out = count;
            return 1;
        }
        if (!qc_lauricella_append(&values, &count, &capacity, term)) {
            free(values);
            return 0;
        }
        magnitude_sum = qf_add(magnitude_sum, magnitude);
        scale = qf_mul(tolerance, qf_add(QF_ONE, magnitude_sum));

        if (k >= 8u && qf_le(magnitude, scale) && qf_le(magnitude, previous_magnitude)) {
            ++small_terms;
            if (small_terms >= QC_LAURICELLA_SMALL_TERM_RUN) {
                *values_out = values;
                *count_out = count;
                return 1;
            }
        } else {
            small_terms = 0u;
        }
        previous_magnitude = magnitude;
        qcomplex_t index = qc_make(qf_from_double((double)k), QF_ZERO);
        qcomplex_t next_index = qc_make(qf_from_double((double)(k + 1u)), QF_ZERO);
        term = qc_mul(term, qc_mul(qc_div(qc_add(parameter, index), next_index), variable));
        if (qc_isnan(term) || qc_isinf(term)) {
            free(values);
            return 0;
        }
    }

    free(values);
    return 0;
}

qcomplex_t qc_lauricella_f(qcomplex_t a, const qcomplex_t *b, qcomplex_t c, const qcomplex_t *x, size_t variable_count)
{
    qcomplex_t *coefficients = NULL;
    qcomplex_t sum = QC_ONE;
    qcomplex_t ratio = QC_ONE;
    qfloat_t tol = qf_from_double(1e-34);
    size_t coefficient_count = 1u;
    int small_terms = 0;

    if ((variable_count > 0u && (!b || !x)) || qc_isnan(a) || qc_isnan(c) || qc_isinf(a) || qc_isinf(c))
        return QC_NAN;
    if (variable_count == 0u)
        return QC_ONE;
    for (size_t i = 0u; i < variable_count; ++i) {
        if (qc_isnan(b[i]) || qc_isinf(b[i]) || qc_isnan(x[i]) || qc_isinf(x[i]) || qf_ge(qc_abs(x[i]), QF_ONE))
            return QC_NAN;
    }

    coefficients = malloc(sizeof(*coefficients));
    if (!coefficients)
        return QC_NAN;
    coefficients[0] = QC_ONE;

    for (size_t i = 0u; i < variable_count; ++i) {
        qcomplex_t *factor = NULL;
        qcomplex_t *next;
        size_t factor_count = 0u;
        size_t next_count;

        if (!qc_lauricella_factor(b[i], x[i], tol, &factor, &factor_count) || factor_count == 0u ||
            factor_count > QC_LAURICELLA_MAX_COEFFICIENTS - coefficient_count + 1u) {
            free(factor);
            free(coefficients);
            return QC_NAN;
        }
        next_count = coefficient_count + factor_count - 1u;
        next = calloc(next_count, sizeof(*next));
        if (!next) {
            free(factor);
            free(coefficients);
            return QC_NAN;
        }
        for (size_t m = 0u; m < coefficient_count; ++m) {
            for (size_t k = 0u; k < factor_count; ++k) {
                next[m + k] = qc_add(next[m + k], qc_mul(coefficients[m], factor[k]));
            }
        }
        free(factor);
        free(coefficients);
        coefficients = next;
        coefficient_count = next_count;
    }

    for (size_t total = 1u; total < coefficient_count; ++total) {
        qcomplex_t index = qc_make(qf_from_double((double)(total - 1u)), QF_ZERO);
        qcomplex_t denominator = qc_add(c, index);
        qcomplex_t contribution;
        qfloat_t scale;

        if (qc_eq(denominator, QC_ZERO)) {
            free(coefficients);
            return QC_NAN;
        }
        ratio = qc_mul(ratio, qc_div(qc_add(a, index), denominator));
        contribution = qc_mul(ratio, coefficients[total]);
        sum = qc_add(sum, contribution);
        scale = qf_mul(tol, qf_add(QF_ONE, qc_abs(sum)));
        small_terms = qf_le(qc_abs(contribution), scale) ? small_terms + 1 : 0;
        if (small_terms >= 8)
            break;
    }

    free(coefficients);
    return sum;
}

qcomplex_t qc_appell_f1(qcomplex_t a, qcomplex_t b1, qcomplex_t b2, qcomplex_t c, qcomplex_t x, qcomplex_t y)
{
    const qcomplex_t b[2] = {b1, b2};
    const qcomplex_t variables[2] = {x, y};

    return qc_lauricella_f(a, b, c, variables, 2u);
}

qcomplex_t qc_hypergeometric_pFq(const qcomplex_t *upper, size_t upper_count, const qcomplex_t *lower,
                                 size_t lower_count, qcomplex_t argument)
{
    qcomplex_t sum = QC_ONE;
    qcomplex_t term = QC_ONE;
    qfloat_t tolerance = qf_from_double(1e-34);
    bool terminating = false;

    if ((upper_count > 0u && !upper) || (lower_count > 0u && !lower) || qc_isnan(argument) || qc_isinf(argument))
        return QC_NAN;

    for (size_t i = 0u; i < upper_count; ++i) {
        qfloat_t real = qc_real(upper[i]);

        if (qc_isnan(upper[i]) || qc_isinf(upper[i]))
            return QC_NAN;
        if (qf_eq(qc_imag(upper[i]), QF_ZERO) && qf_le(real, QF_ZERO) && qf_eq(real, qf_floor(real)))
            terminating = true;
    }
    for (size_t i = 0u; i < lower_count; ++i) {
        if (qc_isnan(lower[i]) || qc_isinf(lower[i]))
            return QC_NAN;
    }

    if (!terminating) {
        if (upper_count > lower_count + 1u)
            return QC_NAN;
        if (upper_count == lower_count + 1u && qf_ge(qc_abs(argument), QF_ONE))
            return QC_NAN;
    }

    for (int k = 0; k < QC_HYPERGEOMETRIC_SERIES_MAX_TERMS; ++k) {
        qcomplex_t index = qc_make(qf_from_double((double)k), QF_ZERO);
        qcomplex_t next_index = qc_make(qf_from_double((double)(k + 1)), QF_ZERO);
        qcomplex_t numerator = QC_ONE;
        qcomplex_t denominator = next_index;
        qcomplex_t next_sum;
        qfloat_t scale;

        for (size_t i = 0u; i < upper_count; ++i)
            numerator = qc_mul(numerator, qc_add(upper[i], index));
        for (size_t i = 0u; i < lower_count; ++i)
            denominator = qc_mul(denominator, qc_add(lower[i], index));
        if (qc_eq(denominator, QC_ZERO))
            return QC_NAN;
        term = qc_mul(term, qc_mul(argument, qc_div(numerator, denominator)));
        if (qc_eq(term, QC_ZERO))
            return sum;
        next_sum = qc_add(sum, term);
        scale = qf_mul(tolerance, qf_add(QF_ONE, qc_abs(next_sum)));
        sum = next_sum;
        if (qf_le(qc_abs(term), scale))
            return sum;
    }

    return QC_NAN;
}

qcomplex_t qc_legendre_chi(qcomplex_t s, qcomplex_t z)
{
    int order;
    qcomplex_t z_sq;
    qcomplex_t pos;
    qcomplex_t neg;

    if (!qc_to_integer_order(s, &order))
        return QC_NAN;
    if (order < 0)
        return QC_NAN;
    if (order == 0) {
        z_sq = qc_mul(z, z);
        return qc_div(z, qc_sub(QC_ONE, z_sq));
    }
    if (order == 1)
        return qc_atanh(z);

    pos = qc_polylog(s, z);
    neg = qc_polylog(s, qc_neg(z));
    return qc_mul(qc_make(qf_from_double(0.5), QF_ZERO), qc_sub(pos, neg));
}
