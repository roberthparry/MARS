#include <math.h>
#include <limits.h>

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
    QC_APPELL_F1_SERIES_MAX_TERMS = 10000
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

qcomplex_t qc_erf(qcomplex_t z) {
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
        qcomplex_t f  = qc_sub(qc_erf(w), z);
        qcomplex_t fp = qc_mul(qc_make(QF_2_SQRTPI, QF_ZERO),
                               qc_exp(qc_neg(qc_mul(w, w))));
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
        qcomplex_t f  = qc_sub(qc_erfc(w), z);
        qcomplex_t fp = qc_mul(qc_make(QF_NEG_TWO_OVER_SQRT_PI, QF_ZERO),
                               qc_exp(qc_neg(qc_mul(w, w))));
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
        qcomplex_t sin_pi_z  = qc_sin(qc_mul(z, QC_PI));
        qcomplex_t gamma_1mz = qc_gamma(qc_sub(QC_ONE, z));
        return qc_div(QC_PI, qc_mul(sin_pi_z, gamma_1mz));
    }

    qcomplex_t z_minus_one = qc_sub(z, QC_ONE);
    qcomplex_t sum         = lanczos_sum(z_minus_one);
    qcomplex_t t           = qc_add(z_minus_one, qc_make(QFI_LANCZOS_SHIFT, QF_ZERO));  /* g + 0.5, g = 7 */

    return qc_mul(qc_mul(qc_make(QF_SQRT_2PI, QF_ZERO), qc_pow(t, qc_sub(z, QC_HALF))),
                  qc_mul(qc_exp(qc_neg(t)), sum));
}

qcomplex_t qc_lgamma(qcomplex_t z)
{
    if (qf_eq(qc_imag(z), qf_from_double(0.0)))
        return qc_make(qf_lgamma(qc_real(z)), QF_ZERO);

    if (qf_lt(qc_real(z), qf_from_double(0.5))) {
        /* Reflection: lgamma(z) = log(π) - log(sin(πz)) - lgamma(1-z) */
        qcomplex_t log_sin_piz = qc_log(qc_sin(qc_mul(z, QC_PI)));
        qcomplex_t lg_1mz      = qc_lgamma(qc_sub(QC_ONE, z));
        return qc_sub(qc_sub(qc_log(QC_PI), log_sin_piz), lg_1mz);
    }

    qcomplex_t z_minus_one = qc_sub(z, QC_ONE);
    qcomplex_t sum         = lanczos_sum(z_minus_one);
    qcomplex_t t           = qc_add(z_minus_one, qc_make(QFI_LANCZOS_SHIFT, QF_ZERO));
    return qc_add(
        qc_add(QC_LOG_SQRT_2PI, qc_mul(qc_sub(z, QC_HALF), qc_log(t))),
        qc_add(qc_neg(t), qc_log(sum)));
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
    qcomplex_t zz  = z;
    while (qf_lt(qc_abs(zz), qf_from_double(10.0))) {
        psi = qc_sub(psi, qc_div(QC_ONE, zz));
        zz  = qc_add(zz, QC_ONE);
    }

    /* Asymptotic: ψ(z) ≈ log(z) - 1/(2z) - Σ B_{2k}/(2k z^{2k}) */
    qcomplex_t invz   = qc_div(QC_ONE, zz);
    qcomplex_t result = qc_sub(qc_log(zz), qc_mul(QC_HALF, invz));

    qcomplex_t z2  = qc_mul(invz, invz);
    qcomplex_t z4  = qc_mul(z2, z2);
    qcomplex_t z6  = qc_mul(z4, z2);
    qcomplex_t z8  = qc_mul(z6, z2);
    qcomplex_t z10 = qc_mul(z8, z2);

    result = qc_sub(result, qc_mul(qc_make(QFI_BERNOULLI_B2, QF_ZERO),  qc_mul(QC_HALF, z2)));
    result = qc_sub(result, qc_mul(qc_make(QFI_BERNOULLI_B4, QF_ZERO),  qc_mul(qc_make(QF_QUARTER, QF_ZERO), z4)));
    result = qc_sub(result, qc_mul(qc_make(QFI_BERNOULLI_B6, QF_ZERO),  qc_mul(qc_make(QF_ONE_SIXTH, QF_ZERO), z6)));
    result = qc_sub(result, qc_mul(qc_make(QFI_BERNOULLI_B8, QF_ZERO),  qc_mul(qc_make(QF_ONE_EIGHTH, QF_ZERO), z8)));
    result = qc_sub(result, qc_mul(qc_make(QFI_BERNOULLI_B10, QF_ZERO), qc_mul(qc_make(QF_ONE_TENTH, QF_ZERO), z10)));

    return qc_add(result, psi);
}

qcomplex_t qc_trigamma(qcomplex_t z)
{
    if (qf_eq(qc_imag(z), qf_from_double(0.0)))
        return qc_make(qf_trigamma(qc_real(z)), QF_ZERO);

    if (qf_lt(qc_real(z), qf_from_double(0.5))) {
        /* Reflection: ψ₁(z) = π² csc²(πz) - ψ₁(1-z) */
        qcomplex_t pi_z  = qc_mul(z, QC_PI);
        qcomplex_t csc   = qc_div(QC_ONE, qc_sin(pi_z));
        qcomplex_t term  = qc_mul(qc_make(QF_PI_SQUARED, QF_ZERO), qc_mul(csc, csc));
        return qc_sub(term, qc_trigamma(qc_sub(QC_ONE, z)));
    }

    /* Recurrence upward until |z| >= 10 */
    qcomplex_t accum = QC_ZERO;
    qcomplex_t zz    = z;
    while (qf_lt(qc_abs2_local(zz), qf_from_double(100.0))) {
        qcomplex_t invz = qc_div(QC_ONE, zz);
        accum = qc_add(accum, qc_mul(invz, invz));
        zz    = qc_add(zz, QC_ONE);
    }

    /* Asymptotic: ψ₁(z) ≈ 1/z + 1/(2z²) + Σ B_{2k}/z^{2k+1} */
    qcomplex_t invz  = qc_div(QC_ONE, zz);
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
        qcomplex_t pi_z   = qc_mul(z, QC_PI);
        qcomplex_t sin_pz = qc_sin(pi_z);
        qcomplex_t csc    = qc_div(QC_ONE, sin_pz);
        qcomplex_t csc2   = qc_mul(csc, csc);
        qcomplex_t cot_pz = qc_div(qc_cos(pi_z), sin_pz);
        qcomplex_t term = qc_mul(qc_make(QF_2PI_CUBED, QF_ZERO),
                                 qc_mul(csc2, cot_pz));
        return qc_add(qc_tetragamma(qc_sub(QC_ONE, z)), term);
    }

    /* Recurrence upward until |z| >= 10 */
    qcomplex_t accum = QC_ZERO;
    qcomplex_t zz    = z;
    while (qf_lt(qc_abs2_local(zz), qf_from_double(100.0))) {
        qcomplex_t invz  = qc_div(QC_ONE, zz);
        qcomplex_t invz3 = qc_mul(invz, qc_mul(invz, invz));
        accum = qc_add(accum, qc_mul(QC_TWO, invz3));
        zz    = qc_add(zz, QC_ONE);
    }

    /* Asymptotic: ψ₂(z) ≈ 1/z² + 1/z³ + Σ B_{2k}/z^{2k+2} */
    qcomplex_t invz  = qc_div(QC_ONE, zz);
    qcomplex_t invz2 = qc_mul(invz, invz);
    qcomplex_t result = qc_add(invz2, qc_mul(invz2, invz));

    qcomplex_t z4  = qc_mul(invz2, invz2);
    qcomplex_t z6  = qc_mul(z4, invz2);
    qcomplex_t z8  = qc_mul(z6, invz2);
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
    qcomplex_t sum = qc_mul(qc_make(qc_factorial_uint(order - 1u), QF_ZERO),
        qc_pow_uint(inv, order));
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
        qcomplex_t term = qc_mul(qc_make(fact, QF_ZERO),
            qc_div(QC_ONE, qc_pow_uint(zz, order + 1u)));

        accum = recurrence_sign > 0 ? qc_sub(accum, term) : qc_add(accum, term);
        zz = qc_add(zz, QC_ONE);
    }

    return qc_add(qc_polygamma_asymp(order, zz), accum);
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
    qcomplex_t a1   = qc_add(a, QC_ONE);
    qcomplex_t b1   = qc_add(b, QC_ONE);
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
    qcomplex_t num = qc_mul(qc_pow(x,           qc_sub(a, QC_ONE)),
                            qc_pow(one_minus_x, qc_sub(b, QC_ONE)));
    return qc_div(num, qc_beta(a, b));
}

qcomplex_t qc_logbeta_pdf(qcomplex_t x, qcomplex_t a, qcomplex_t b)
{
    if (qf_eq(qc_imag(x), qf_from_double(0.0)) && qf_eq(qc_imag(a), qf_from_double(0.0)) &&
        qf_eq(qc_imag(b), qf_from_double(0.0)))
        return qc_make(qf_logbeta_pdf(qc_real(x), qc_real(a), qc_real(b)), QF_ZERO);

    /* log f(x; a,b) = (a-1)log(x) + (b-1)log(1-x) - log B(a,b) */
    return qc_sub(qc_add(qc_mul(qc_sub(a, QC_ONE), qc_log(x)),
                         qc_mul(qc_sub(b, QC_ONE), qc_log(qc_sub(QC_ONE, x)))),
                  qc_logbeta(a, b));
}

qcomplex_t qc_normal_pdf(qcomplex_t z)
{
    if (qf_eq(qc_imag(z), qf_from_double(0.0)))
        return qc_make(qf_normal_pdf(qc_real(z)), QF_ZERO);

    /* φ(z) = exp(-z²/2) / sqrt(2π) */
    return qc_mul(QC_INV_SQRT_2PI,
                  qc_exp(qc_mul(qc_neg(QC_HALF), qc_mul(z, z))));
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
    return qc_sub(qc_mul(qc_neg(QC_HALF), qc_mul(z, z)),
                  QC_LOG_SQRT_2PI);
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

    if (qf_eq(qc_imag(z), qf_from_double(0.0)) &&
        qf_ge(qc_real(z), QF_NEG_INV_E) &&
        qf_lt(qc_real(z), QF_ZERO))
        return qc_make(qf_lambert_wm1(qc_real(z)), QF_ZERO);

    if (qf_eq(qc_real(z), QF_ZERO) && qf_eq(qc_imag(z), QF_ZERO))
        return qc_make(QF_NINF, QF_NAN);

    qcomplex_t branch_probe = qc_add(qc_mul(QC_E, z), QC_ONE);
    qcomplex_t w = qf_lt(qc_abs2_local(branch_probe), qf_from_double(0.0625))
        ? qc_lambert_w_series_guess(z, -1)
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
            qcomplex_t halley_corr = qc_div(qc_mul(qc_add(w, QC_TWO), f),
                                            qc_mul(QC_TWO, wp1));
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
            qcomplex_t halley_corr = qc_div(qc_mul(qc_add(w, QC_TWO), f),
                                            qc_mul(QC_TWO, wp1));
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
    if (qf_eq(qc_imag(z), qf_from_double(0.0)) &&
        qf_ge(qc_real(z), QF_NEG_INV_E))
        return qc_make(qf_productlog(qc_real(z)), QF_ZERO);

    /* Halley iteration on the principal branch: w e^w = z.
     * Near zero the principal solution is near z itself; starting at log(z)
     * can converge to a non-principal branch. */
    qcomplex_t w = qf_lt(qc_abs2_local(z), qf_from_double(0.25))
        ? z
        : qc_log(z);
    for (int i = 0; i < QC_PRODUCTLOG_HALLEY_MAX_STEPS; i++) {
        qcomplex_t ew    = qc_exp(w);
        qcomplex_t wew   = qc_mul(w, ew);
        qcomplex_t f     = qc_sub(wew, z);
        qcomplex_t wp1   = qc_add(w, QC_ONE);
        qcomplex_t f1    = qc_mul(ew, wp1);
        qcomplex_t f2    = qc_mul(ew, qc_add(w, QC_TWO));
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
    qcomplex_t sum  = term;

    for (int i = 1; i < QC_GAMMAINC_SERIES_MAX_TERMS; i++) {
        term = qc_mul(term, qc_div(x, qc_add(s, qc_make(qf_from_double((double)i), QF_ZERO))));
        sum  = qc_add(sum, term);
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

qcomplex_t qc_ei(qcomplex_t z)
{
    if (qf_eq(qc_imag(z), qf_from_double(0.0)))
        return qc_make(qf_ei(qc_real(z)), QF_ZERO);

    /* Ei(z) = γ + log(z) + Σ_{k=1}^∞ z^k / (k × k!) */
    qfloat_t tol  = qf_from_double(1e-30);
    qcomplex_t sum = qc_add(QC_EULER_MASCHERONI, qc_log(z));

    qfloat_t one  = qf_from_double(1.0);
    qfloat_t kf   = one;                    /* k */
    qfloat_t fact = one;                    /* k! */
    qcomplex_t term = z;                    /* z^k */

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

qcomplex_t qc_e1(qcomplex_t z)
{
    if (qf_eq(qc_imag(z), qf_from_double(0.0)))
        return qc_make(qf_e1(qc_real(z)), QF_ZERO);

    return qc_neg(qc_ei(qc_neg(z)));
}

static int qc_to_integer_order(qcomplex_t value, int *order)
{
    double raw;
    double rounded;

    if (!order || !qf_eq(qc_imag(value), QF_ZERO) ||
        qf_isnan(qc_real(value)) || qf_isinf(qc_real(value)))
        return 0;

    raw = qf_to_double(qc_real(value));
    rounded = nearbyint(raw);
    if (fabs(raw - rounded) > 1e-28 ||
        rounded < (double)INT_MIN ||
        rounded > (double)INT_MAX)
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
        qcomplex_t half_log_sq = qc_mul(qc_make(qf_from_double(0.5), QF_ZERO),
                                        qc_mul(log_neg_z, log_neg_z));

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

qcomplex_t qc_appell_f1(qcomplex_t a, qcomplex_t b1, qcomplex_t b2,
                        qcomplex_t c, qcomplex_t x, qcomplex_t y)
{
    qcomplex_t sum = QC_ZERO;
    qcomplex_t row_start = QC_ONE;
    qfloat_t tol = qf_from_double(1e-34);

    if (qc_isnan(a) || qc_isnan(b1) || qc_isnan(b2) || qc_isnan(c) ||
        qc_isnan(x) || qc_isnan(y) ||
        qc_isinf(a) || qc_isinf(b1) || qc_isinf(b2) || qc_isinf(c) ||
        qc_isinf(x) || qc_isinf(y))
        return QC_NAN;
    if (qf_ge(qc_abs(x), qf_from_double(0.95)) ||
        qf_ge(qc_abs(y), qf_from_double(0.95)))
        return QC_NAN;

    for (int m = 0; m < QC_APPELL_F1_SERIES_MAX_TERMS; ++m) {
        qcomplex_t row_sum = QC_ZERO;
        qcomplex_t term = row_start;

        for (int n = 0; n < QC_APPELL_F1_SERIES_MAX_TERMS; ++n) {
            qfloat_t scale = qf_mul(tol, qf_add(QF_ONE, qc_abs(row_sum)));

            row_sum = qc_add(row_sum, term);
            if (qf_le(qc_abs(term), scale))
                break;

            qcomplex_t mn = qc_make(qf_from_double((double)(m + n)), QF_ZERO);
            qcomplex_t nn = qc_make(qf_from_double((double)n), QF_ZERO);
            qcomplex_t np1 = qc_make(qf_from_double((double)(n + 1)), QF_ZERO);
            qcomplex_t num = qc_mul(qc_add(a, mn), qc_add(b2, nn));
            qcomplex_t den = qc_mul(qc_add(c, mn), np1);

            if (qc_eq(den, QC_ZERO))
                return QC_NAN;
            term = qc_mul(term, qc_mul(qc_div(num, den), y));
        }

        sum = qc_add(sum, row_sum);
        if (qf_le(qc_abs(row_sum), qf_mul(tol, qf_add(QF_ONE, qc_abs(sum)))))
            break;

        qcomplex_t mm = qc_make(qf_from_double((double)m), QF_ZERO);
        qcomplex_t mp1 = qc_make(qf_from_double((double)(m + 1)), QF_ZERO);
        qcomplex_t num = qc_mul(qc_add(a, mm), qc_add(b1, mm));
        qcomplex_t den = qc_mul(qc_add(c, mm), mp1);

        if (qc_eq(den, QC_ZERO))
            return QC_NAN;
        row_start = qc_mul(row_start, qc_mul(qc_div(num, den), x));
    }

    return sum;
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
