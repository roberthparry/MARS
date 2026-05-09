#include <math.h>

#include "internal/qfloat_internal.h"
#include "qcomplex.h"

static qfloat_t qc_abs2_local(qcomplex_t z)
{
    return qf_add(qf_mul(z.re, z.re), qf_mul(z.im, z.im));
}

static qcomplex_t qc_faddeeva_inside(qcomplex_t z)
{
    const int N = 32;
    qcomplex_t sum = QC_ZERO;

    for (int k = 1; k <= N; k++) {
        qcomplex_t denom = qc_make(z.re, qf_sub(z.im, QFI_FADDEEVA_AK[k - 1]));
        sum = qc_add(sum, qc_div(qc_make(QFI_FADDEEVA_CK[k - 1], QF_ZERO), denom));
    }

    /* inside = 1 + (2i / sqrt(pi)) * sum */
    qcomplex_t two_i_sum = qc_mul(qc_make(qf_from_double(0.0), qf_from_double(2.0)), sum);
    return qc_add(QC_ONE, qc_div(two_i_sum, QC_SQRT_PI));
}

qcomplex_t qc_erf(qcomplex_t z) {
    if (qf_eq(z.im, qf_from_double(0.0)))
        return qc_make(qf_erf(z.re), QF_ZERO);
    /* Faddeeva requires Im(iz) = Re(z) >= 0; use antisymmetry erf(-z) = -erf(z) otherwise */
    if (qf_lt(z.re, qf_from_double(0.0)))
        return qc_neg(qc_erf(qc_neg(z)));
    qcomplex_t iz = qc_make(qf_neg(z.im), z.re);
    return qc_sub(QC_ONE, qc_faddeeva_inside(iz));
}

qcomplex_t qc_erfc(qcomplex_t z)
{
    if (qf_eq(z.im, qf_from_double(0.0)))
        return qc_make(qf_erfc(z.re), QF_ZERO);
    /* Use erfc(-z) = 2 - erfc(z) to keep Re(z) >= 0 for Faddeeva */
    if (qf_lt(z.re, qf_from_double(0.0)))
        return qc_sub(QC_TWO, qc_erfc(qc_neg(z)));
    qcomplex_t iz = qc_make(qf_neg(z.im), z.re);
    return qc_faddeeva_inside(iz);
}

qcomplex_t qc_erfinv(qcomplex_t z)
{
    /* Newton iteration: solve erf(w) = z.  Initial guess good for small |z|. */
    qcomplex_t w = qc_mul(z, qc_make(QF_SQRT_PI_OVER_TWO, QF_ZERO));

    for (int i = 0; i < 10; i++) {
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

    for (int i = 0; i < 10; i++) {
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
    for (int i = 1; i < 9; i++)
        sum = qc_add(sum, qc_div(qc_make(QFI_LANCZOS_C[i], QF_ZERO),
                                 qc_add(z_minus_one, qc_make(qf_from_double((double)i), QF_ZERO))));
    return sum;
}

/* ------------------------------------------------------------ */

/* Complex Gamma */
/* ------------------------------------------------------------ */

qcomplex_t qc_gamma(qcomplex_t z)
{
    if (qf_eq(z.im, qf_from_double(0.0)))
        return qc_make(qf_gamma(z.re), QF_ZERO);

    if (qf_lt(z.re, qf_from_double(0.5))) {
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
    if (qf_eq(z.im, qf_from_double(0.0)))
        return qc_make(qf_lgamma(z.re), QF_ZERO);

    if (qf_lt(z.re, qf_from_double(0.5))) {
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
    if (qf_eq(z.im, qf_from_double(0.0)))
        return qc_make(qf_digamma(z.re), QF_ZERO);

    if (qf_lt(z.re, qf_from_double(0.5))) {
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
    if (qf_eq(z.im, qf_from_double(0.0)))
        return qc_make(qf_trigamma(z.re), QF_ZERO);

    if (qf_lt(z.re, qf_from_double(0.5))) {
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
    if (qf_eq(z.im, qf_from_double(0.0)))
        return qc_make(qf_tetragamma(z.re), QF_ZERO);

    if (qf_lt(z.re, qf_from_double(0.5))) {
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

qcomplex_t qc_gammainv(qcomplex_t z)
{
    if (qf_eq(z.im, qf_from_double(0.0)))
        return qc_make(qf_gammainv(z.re), QF_ZERO);

    if (qc_isnan(z) || (qf_eq(z.re, QF_ZERO) && qf_eq(z.im, QF_ZERO)))
        return qc_make(QF_NAN, QF_NAN);

    qcomplex_t logz = qc_log(z);
    qcomplex_t w;
    w = qc_add(qc_make(QF_ONE_AND_HALF, QF_ZERO), logz);

    for (int i = 0; i < 20; i++) {
        qcomplex_t delta = qc_div(qc_sub(qc_lgamma(w), logz), qc_digamma(w));
        w = qc_sub(w, delta);
        if (qf_lt(qc_abs(delta), qf_from_double(1e-30)))
            break;
    }
    return w;
}

qcomplex_t qc_beta(qcomplex_t a, qcomplex_t b)
{
    if (qf_eq(a.im, qf_from_double(0.0)) && qf_eq(b.im, qf_from_double(0.0)))
        return qc_make(qf_beta(a.re, b.re), QF_ZERO);

    return qc_exp(qc_logbeta(a, b));
}

qcomplex_t qc_logbeta(qcomplex_t a, qcomplex_t b)
{
    if (qf_eq(a.im, qf_from_double(0.0)) && qf_eq(b.im, qf_from_double(0.0)))
        return qc_make(qf_logbeta(a.re, b.re), QF_ZERO);

    return qc_sub(qc_add(qc_lgamma(a), qc_lgamma(b)), qc_lgamma(qc_add(a, b)));
}

qcomplex_t qc_binomial(qcomplex_t a, qcomplex_t b)
{
    if (qf_eq(a.im, qf_from_double(0.0)) && qf_eq(b.im, qf_from_double(0.0)))
        return qc_make(qf_binomial(a.re, b.re), QF_ZERO);

    /* C(a,b) = Γ(a+1) / (Γ(b+1) Γ(a-b+1)) */
    qcomplex_t a1   = qc_add(a, QC_ONE);
    qcomplex_t b1   = qc_add(b, QC_ONE);
    qcomplex_t amb1 = qc_add(qc_sub(a, b), QC_ONE);
    return qc_div(qc_gamma(a1), qc_mul(qc_gamma(b1), qc_gamma(amb1)));
}

qcomplex_t qc_beta_pdf(qcomplex_t x, qcomplex_t a, qcomplex_t b)
{
    if (qf_eq(x.im, qf_from_double(0.0)) && qf_eq(a.im, qf_from_double(0.0)) &&
        qf_eq(b.im, qf_from_double(0.0)))
        return qc_make(qf_beta_pdf(x.re, a.re, b.re), QF_ZERO);

    /* f(x; a, b) = x^(a-1) * (1-x)^(b-1) / B(a,b) */
    qcomplex_t one_minus_x = qc_sub(QC_ONE, x);
    qcomplex_t num = qc_mul(qc_pow(x,           qc_sub(a, QC_ONE)),
                            qc_pow(one_minus_x, qc_sub(b, QC_ONE)));
    return qc_div(num, qc_beta(a, b));
}

qcomplex_t qc_logbeta_pdf(qcomplex_t x, qcomplex_t a, qcomplex_t b)
{
    if (qf_eq(x.im, qf_from_double(0.0)) && qf_eq(a.im, qf_from_double(0.0)) &&
        qf_eq(b.im, qf_from_double(0.0)))
        return qc_make(qf_logbeta_pdf(x.re, a.re, b.re), QF_ZERO);

    /* log f(x; a,b) = (a-1)log(x) + (b-1)log(1-x) - log B(a,b) */
    return qc_sub(qc_add(qc_mul(qc_sub(a, QC_ONE), qc_log(x)),
                         qc_mul(qc_sub(b, QC_ONE), qc_log(qc_sub(QC_ONE, x)))),
                  qc_logbeta(a, b));
}

qcomplex_t qc_normal_pdf(qcomplex_t z)
{
    if (qf_eq(z.im, qf_from_double(0.0)))
        return qc_make(qf_normal_pdf(z.re), QF_ZERO);

    /* φ(z) = exp(-z²/2) / sqrt(2π) */
    return qc_mul(QC_INV_SQRT_2PI,
                  qc_exp(qc_mul(qc_neg(QC_HALF), qc_mul(z, z))));
}

qcomplex_t qc_normal_cdf(qcomplex_t z)
{
    if (qf_eq(z.im, qf_from_double(0.0)))
        return qc_make(qf_normal_cdf(z.re), QF_ZERO);

    /* Φ(z) = 0.5 * (1 + erf(z / sqrt(2))) */
    return qc_mul(QC_HALF, qc_add(QC_ONE, qc_erf(qc_div(z, QC_SQRT2))));
}

qcomplex_t qc_normal_logpdf(qcomplex_t z)
{
    if (qf_eq(z.im, qf_from_double(0.0)))
        return qc_make(qf_normal_logpdf(z.re), QF_ZERO);

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
    L1.im = qf_add(L1.im, qf_mul_double(two_pi, (double)branch));

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

    if (qf_eq(z.im, qf_from_double(0.0)))
        return qc_make(qf_lambert_wm1(z.re), QF_ZERO);

    if (qf_eq(z.re, QF_ZERO) && qf_eq(z.im, QF_ZERO))
        return qc_make(QF_NINF, QF_NAN);

    qcomplex_t branch_probe = qc_add(qc_mul(QC_E, z), QC_ONE);
    qcomplex_t w = qf_lt(qc_abs2_local(branch_probe), qf_from_double(0.0625))
        ? qc_lambert_w_series_guess(z, -1)
        : qc_lambert_w_asymptotic_guess(z, -1);

    for (int i = 0; i < 60; i++) {
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

qcomplex_t qc_productlog(qcomplex_t z)
{
    if (qf_eq(z.im, qf_from_double(0.0)))
        return qc_make(qf_productlog(z.re), QF_ZERO);

    /* Halley iteration on the principal branch: w e^w = z */
    qcomplex_t w = qc_log(z);
    for (int i = 0; i < 40; i++) {
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

    for (int i = 1; i < 10000; i++) {
        term = qc_mul(term, qc_div(x, qc_add(s, qc_make(qf_from_double((double)i), QF_ZERO))));
        sum  = qc_add(sum, term);
        if (qf_lt(qc_abs(term), qf_mul(tol, qc_abs(sum))))
            break;
    }

    return qc_mul(qc_mul(qc_pow(x, s), qc_exp(qc_neg(x))), sum);
}

qcomplex_t qc_gammainc_lower(qcomplex_t s, qcomplex_t x)
{
    if (qf_eq(s.im, qf_from_double(0.0)) && qf_eq(x.im, qf_from_double(0.0)))
        return qc_make(qf_gammainc_lower(s.re, x.re), QF_ZERO);

    return qc_gammainc_lower_series(s, x);
}

qcomplex_t qc_gammainc_upper(qcomplex_t s, qcomplex_t x)
{
    if (qf_eq(s.im, qf_from_double(0.0)) && qf_eq(x.im, qf_from_double(0.0)))
        return qc_make(qf_gammainc_upper(s.re, x.re), QF_ZERO);

    return qc_sub(qc_gamma(s), qc_gammainc_lower_series(s, x));
}

qcomplex_t qc_gammainc_P(qcomplex_t s, qcomplex_t x)
{
    if (qf_eq(s.im, qf_from_double(0.0)) && qf_eq(x.im, qf_from_double(0.0)))
        return qc_make(qf_gammainc_P(s.re, x.re), QF_ZERO);

    return qc_div(qc_gammainc_lower_series(s, x), qc_gamma(s));
}

qcomplex_t qc_gammainc_Q(qcomplex_t s, qcomplex_t x)
{
    if (qf_eq(s.im, qf_from_double(0.0)) && qf_eq(x.im, qf_from_double(0.0)))
        return qc_make(qf_gammainc_Q(s.re, x.re), QF_ZERO);

    return qc_sub(QC_ONE, qc_gammainc_P(s, x));
}

qcomplex_t qc_ei(qcomplex_t z)
{
    if (qf_eq(z.im, qf_from_double(0.0)))
        return qc_make(qf_ei(z.re), QF_ZERO);

    /* Ei(z) = γ + log(z) + Σ_{k=1}^∞ z^k / (k × k!) */
    qfloat_t tol  = qf_from_double(1e-30);
    qcomplex_t sum = qc_add(QC_EULER_MASCHERONI, qc_log(z));

    qfloat_t one  = qf_from_double(1.0);
    qfloat_t kf   = one;                    /* k */
    qfloat_t fact = one;                    /* k! */
    qcomplex_t term = z;                    /* z^k */

    for (int k = 1; k < 10000; k++) {
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
    if (qf_eq(z.im, qf_from_double(0.0)))
        return qc_make(qf_e1(z.re), QF_ZERO);

    return qc_neg(qc_ei(qc_neg(z)));
}
