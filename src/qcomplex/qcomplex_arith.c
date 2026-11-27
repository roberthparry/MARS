/**
 * @file qcomplex_arith.c
 * @brief Double-double complex arithmetic and elementary inverses.
 *
 * Implements arithmetic and supported elementary complex operations using qfloat components. Branch and domain
 * conventions must remain consistent with the public qcomplex interface.
 * Paired evaluations share real elementary factors with reciprocal functions and complex reflection formulas.
 *
 * This implements fixed double-double complex arithmetic for qcomplex_t. It is distinct from configurable
 * multiprecision complex arithmetic in the number module.
 */

#include <math.h>

#include "qcomplex-t.h"
#define MARS_SHARED_QFLOAT_INTERNAL_ACCESS
#include "internal/qfloat_internal.h"
#define MARS_QCOMPLEX_INTERNAL_ACCESS
#include "qcomplex_internal.h"

/* Native conversion of 1e-18, preserving the existing branch threshold exactly. */
static const qfloat_t QC_LOG_AXIS_TOLERANCE = {0x1.2725dd1d243acp-60, -0x1.7c628066e8ceep-114};

static qcomplex_t qc_pow_int(qcomplex_t z, int n)
{
    qcomplex_t result = QC_ONE;
    qcomplex_t base = z;

    if (n == 0)
        return QC_ONE;

    unsigned int exponent = n < 0 ? 0u - (unsigned int)n : (unsigned int)n;
    while (exponent > 0u) {
        if (exponent & 1u)
            result = qc_mul(result, base);
        exponent >>= 1;
        if (exponent > 0u)
            base = qc_mul(base, base);
    }

    return n < 0 ? qc_div(QC_ONE, result) : result;
}

qcomplex_t qc_add(qcomplex_t a, qcomplex_t b)
{
    return qc_make(qf_add(qc_real(a), qc_real(b)), qf_add(qc_imag(a), qc_imag(b)));
}
qcomplex_t qc_sub(qcomplex_t a, qcomplex_t b)
{
    return qc_make(qf_sub(qc_real(a), qc_real(b)), qf_sub(qc_imag(a), qc_imag(b)));
}
qcomplex_t qc_neg(qcomplex_t a)
{
    return qc_make(qf_neg(qc_real(a)), qf_neg(qc_imag(a)));
}
qcomplex_t qc_conj(qcomplex_t a)
{
    return qc_make(qc_real(a), qf_neg(qc_imag(a)));
}

/* Return the complex conjugate through the long-form public alias. */
qcomplex_t qc_conjugate(qcomplex_t a)
{
    return qc_conj(a);
}

qcomplex_t qc_mul(qcomplex_t a, qcomplex_t b)
{
    qfloat_t re = qf_sub(qf_mul(qc_real(a), qc_real(b)), qf_mul(qc_imag(a), qc_imag(b)));
    qfloat_t im = qf_add(qf_mul(qc_real(a), qc_imag(b)), qf_mul(qc_imag(a), qc_real(b)));
    return qc_make(re, im);
}
qcomplex_t qc_div(qcomplex_t a, qcomplex_t b)
{
    qfloat_t denom = qf_add(qf_mul(qc_real(b), qc_real(b)), qf_mul(qc_imag(b), qc_imag(b)));
    qfloat_t re = qf_div(qf_add(qf_mul(qc_real(a), qc_real(b)), qf_mul(qc_imag(a), qc_imag(b))), denom);
    qfloat_t im = qf_div(qf_sub(qf_mul(qc_imag(a), qc_real(b)), qf_mul(qc_real(a), qc_imag(b))), denom);
    return qc_make(re, im);
}

/* Evaluate the complex exponential with one paired polar construction. */
qcomplex_t qc_exp(qcomplex_t z)
{
    qfloat_t exp_re = qf_exp(qc_real(z));
    return qc_from_polar(exp_re, qc_imag(z));
}
/* Evaluate the principal logarithm with the existing branch threshold held as a native constant. */
qcomplex_t qc_log(qcomplex_t z)
{
    if (qf_eq(qc_real(z), QF_NEG_ONE) && qf_lt(qf_abs(qc_imag(z)), QC_LOG_AXIS_TOLERANCE) &&
        !qf_eq(qc_imag(z), QF_ZERO)) {
        return qc_make(QF_ZERO, qf_signbit(qc_imag(z)) ? qf_neg(QF_PI) : QF_PI);
    }
    return qc_make(qf_log(qc_abs(z)), qc_arg(z));
}

/* Provide the conventional ln shorthand for the principal natural logarithm. */
qcomplex_t qc_ln(qcomplex_t z)
{
    return qc_log(z);
}

qcomplex_t qc_log10(qcomplex_t z)
{
    qfloat_t inv_ln10 = qf_div(QF_ONE, QF_LN10);
    return qc_make(qf_mul(qf_log(qc_abs(z)), inv_ln10), qf_mul(qc_arg(z), inv_ln10));
}

/* Provide the conventional lg shorthand for the principal common logarithm. */
qcomplex_t qc_lg(qcomplex_t z)
{
    return qc_log10(z);
}
qcomplex_t qc_pow(qcomplex_t a, qcomplex_t b)
{
    if (qc_eq(b, QC_ZERO))
        return QC_ONE;

    if (qc_eq(a, QC_ONE))
        return QC_ONE;

    if (qc_eq(b, QC_ONE))
        return a;

    if (qf_eq(qc_imag(b), QF_ZERO)) {
        double yd = qf_to_double(qc_real(b));
        double yi = nearbyint(yd);

        if (fabs(yd - yi) <= 1e-30)
            return qc_pow_int(a, (int)yi);
    }

    if (qf_eq(qc_imag(a), QF_ZERO) && qf_eq(qc_imag(b), QF_ZERO)) {
        if (!qf_signbit(qc_real(a)))
            return qc_make(qf_pow(qc_real(a), qc_real(b)), QF_ZERO);

        qfloat_t mag = qf_pow(qf_abs(qc_real(a)), qc_real(b));
        qfloat_t theta = qf_mul(qc_real(b), QF_PI);
        return qc_from_polar(mag, theta);
    }

    qcomplex_t log_a = qc_log(a);
    qcomplex_t prod = qc_mul(b, log_a);
    return qc_exp(prod);
}
qcomplex_t qc_sqrt(qcomplex_t z)
{
    qfloat_t r = qc_abs(z);
    qfloat_t sqrt_r = qf_sqrt(r);
    qfloat_t half_theta = qf_ldexp(qc_arg(z), -1);
    return qc_from_polar(sqrt_r, half_theta);
}

/* Return the single principal n-th root. */
qcomplex_t qc_root(qcomplex_t z, unsigned int n)
{
    qfloat_t reciprocal;

    if (n <= 1u)
        return qc_make(QF_NAN, QF_NAN);
    if (n == 2u)
        return qc_sqrt(z);
    if (qc_eq(z, QC_ZERO) || qc_eq(z, QC_ONE))
        return z;
    reciprocal = qf_div(QF_ONE, qf_from_double((double)n));
    return qc_exp(qc_mul(qc_make(reciprocal, QF_ZERO), qc_log(z)));
}

/* Return the single principal cube root. */
qcomplex_t qc_cubrt(qcomplex_t z)
{
    return qc_root(z, 3u);
}

/* Trigonometric */
/* Share real trigonometric and hyperbolic factors for paired complex consumers. */
void qci_sin_cos(qcomplex_t z, qcomplex_t *sine, qcomplex_t *cosine)
{
    qfloat_t s, c, sh, ch;
    qfi_sin_cos(qc_real(z), &s, &c);
    qfi_sinh_cosh(qc_imag(z), &sh, &ch);

    *sine = qc_make(qf_mul(s, ch), qf_mul(c, sh));
    *cosine = qc_make(qf_mul(c, ch), qf_neg(qf_mul(s, sh)));
}

/* Evaluate complex sine with a shared pair of real exponentials. */
qcomplex_t qc_sin(qcomplex_t z)
{
    qfloat_t s, c, sh, ch;
    qfi_sin_cos(qc_real(z), &s, &c);
    qfi_sinh_cosh(qc_imag(z), &sh, &ch);
    return qc_make(qf_mul(s, ch), qf_mul(c, sh));
}

/* Evaluate complex cosine with a shared pair of real exponentials. */
qcomplex_t qc_cos(qcomplex_t z)
{
    qfloat_t s, c, sh, ch;
    qfi_sin_cos(qc_real(z), &s, &c);
    qfi_sinh_cosh(qc_imag(z), &sh, &ch);
    return qc_make(qf_mul(c, ch), qf_neg(qf_mul(s, sh)));
}

/* Form the complex tangent from one paired sine/cosine evaluation. */
qcomplex_t qc_tan(qcomplex_t z)
{
    qcomplex_t sine, cosine;
    qci_sin_cos(z, &sine, &cosine);
    return qc_div(sine, cosine);
}
qcomplex_t qc_sec(qcomplex_t z)
{
    return qc_div(QC_ONE, qc_cos(z));
}
qcomplex_t qc_cosec(qcomplex_t z)
{
    return qc_div(QC_ONE, qc_sin(z));
}
/* Form the complex cotangent from one paired sine/cosine evaluation. */
qcomplex_t qc_cot(qcomplex_t z)
{
    qcomplex_t sine, cosine;
    qci_sin_cos(z, &sine, &cosine);
    return qc_div(cosine, sine);
}
qcomplex_t qc_versin(qcomplex_t z)
{
    return qc_sub(QC_ONE, qc_cos(z));
}
qcomplex_t qc_vercos(qcomplex_t z)
{
    return qc_add(QC_ONE, qc_cos(z));
}
qcomplex_t qc_coversin(qcomplex_t z)
{
    return qc_sub(QC_ONE, qc_sin(z));
}
qcomplex_t qc_covercos(qcomplex_t z)
{
    return qc_add(QC_ONE, qc_sin(z));
}
qcomplex_t qc_haversin(qcomplex_t z)
{
    return qc_ldexp(qc_versin(z), -1);
}
qcomplex_t qc_havercos(qcomplex_t z)
{
    return qc_ldexp(qc_vercos(z), -1);
}
qcomplex_t qc_hacoversin(qcomplex_t z)
{
    return qc_ldexp(qc_coversin(z), -1);
}
qcomplex_t qc_hacovercos(qcomplex_t z)
{
    return qc_ldexp(qc_covercos(z), -1);
}
/* Return the principal complex inverse sine. */
qcomplex_t qc_asin(qcomplex_t z)
{
    if (qf_eq(qc_imag(z), QF_ZERO) && qf_gt(qf_abs(qc_real(z)), QF_ONE)) {
        qfloat_t magnitude = qf_acosh(qf_abs(qc_real(z)));

        return qf_signbit(qc_real(z)) ? qc_make(qf_neg(qf_ldexp(QF_PI, -1)), magnitude)
                                      : qc_make(qf_ldexp(QF_PI, -1), magnitude);
    }
    /* asin(z) = -i log(iz + sqrt(1-z^2)) */
    /* -i*(a+bi) = b - ai, so re=qc_imag(ln), im=-qc_real(ln) */
    qcomplex_t iz = qc_make(qf_neg(qc_imag(z)), qc_real(z));
    qcomplex_t sqrt_term = qc_sqrt(qc_sub(QC_ONE, qc_mul(z, z)));
    qcomplex_t ln = qc_log(qc_add(iz, sqrt_term));
    return qc_make(qc_imag(ln), qf_neg(qc_real(ln)));
}
/* Return the principal complex inverse cosine. */
qcomplex_t qc_acos(qcomplex_t z)
{
    if (qf_eq(qc_imag(z), QF_ZERO) && qf_gt(qf_abs(qc_real(z)), QF_ONE)) {
        qfloat_t magnitude = qf_acosh(qf_abs(qc_real(z)));

        return qf_signbit(qc_real(z)) ? qc_make(QF_PI, qf_neg(magnitude)) : qc_make(QF_ZERO, qf_neg(magnitude));
    }
    /* acos(z) = -i log(z + i sqrt(1-z^2)) */
    /* -i*(a+bi) = b - ai, so re=qc_imag(ln), im=-qc_real(ln) */
    qcomplex_t sqrt_term = qc_sqrt(qc_sub(QC_ONE, qc_mul(z, z)));
    qcomplex_t iz = qc_make(qf_neg(qc_imag(sqrt_term)), qc_real(sqrt_term));
    qcomplex_t ln = qc_log(qc_add(z, iz));
    return qc_make(qc_imag(ln), qf_neg(qc_real(ln)));
}
qcomplex_t qc_atan(qcomplex_t z)
{
    /* atan(z) = (i/2) * [log(1 - iz) - log(1 + iz)] */
    /* (i/2)*(a+bi) = -b/2 + (a/2)i, so re=-qc_imag(diff)/2, im=qc_real(diff)/2 */
    qcomplex_t iz = qc_make(qf_neg(qc_imag(z)), qc_real(z));
    qcomplex_t diff = qc_sub(qc_log(qc_sub(QC_ONE, iz)), qc_log(qc_add(QC_ONE, iz)));
    return qc_make(qf_mul_double(qf_neg(qc_imag(diff)), 0.5), qf_mul_double(qc_real(diff), 0.5));
}
qcomplex_t qc_asec(qcomplex_t z)
{
    if (qc_eq(z, QC_ZERO))
        return QC_NAN;
    if (qf_isinf(qc_real(z)) && qf_eq(qc_imag(z), QF_ZERO))
        return QC_PI_2;
    return qc_acos(qc_div(QC_ONE, z));
}
qcomplex_t qc_acosec(qcomplex_t z)
{
    if (qc_eq(z, QC_ZERO))
        return QC_NAN;
    if (qf_isinf(qc_real(z)) && qf_eq(qc_imag(z), QF_ZERO))
        return QC_ZERO;
    return qc_asin(qc_div(QC_ONE, z));
}
qcomplex_t qc_acot(qcomplex_t z)
{
    if (qf_isposinf(qc_real(z)) && qf_eq(qc_imag(z), QF_ZERO))
        return QC_ZERO;
    if (qf_isneginf(qc_real(z)) && qf_eq(qc_imag(z), QF_ZERO))
        return QC_PI;

    /* acot(z) = (1 / 2i) * log((z + i) / (z - i)).
     * This keeps the branch choice in acot itself rather than inheriting
     * atan(1/z)'s singular behaviour at z = 0. */
    qcomplex_t plus_i = qc_add(z, QC_I);
    qcomplex_t minus_i = qc_sub(z, QC_I);
    qcomplex_t diff = qc_sub(qc_log(plus_i), qc_log(minus_i));

    return qc_make(qf_mul_double(qc_imag(diff), 0.5), qf_mul_double(qf_neg(qc_real(diff)), 0.5));
}
qcomplex_t qc_arcversin(qcomplex_t z)
{
    return qc_acos(qc_sub(QC_ONE, z));
}
qcomplex_t qc_arcvercos(qcomplex_t z)
{
    return qc_acos(qc_sub(z, QC_ONE));
}
qcomplex_t qc_arccoversin(qcomplex_t z)
{
    return qc_asin(qc_sub(QC_ONE, z));
}
qcomplex_t qc_arccovercos(qcomplex_t z)
{
    return qc_asin(qc_sub(z, QC_ONE));
}
qcomplex_t qc_archaversin(qcomplex_t z)
{
    return qc_acos(qc_sub(QC_ONE, qc_mul(QC_TWO, z)));
}
qcomplex_t qc_archavercos(qcomplex_t z)
{
    return qc_acos(qc_sub(qc_mul(QC_TWO, z), QC_ONE));
}
qcomplex_t qc_archacoversin(qcomplex_t z)
{
    return qc_asin(qc_sub(QC_ONE, qc_mul(QC_TWO, z)));
}
qcomplex_t qc_archacovercos(qcomplex_t z)
{
    return qc_asin(qc_sub(qc_mul(QC_TWO, z), QC_ONE));
}
qcomplex_t qc_atan2(qcomplex_t y, qcomplex_t x)
{
    if (qf_eq(qc_imag(y), qf_from_double(0.0)) && qf_eq(qc_imag(x), qf_from_double(0.0)))
        return qc_make(qf_atan2(qc_real(y), qc_real(x)), QF_ZERO);
    return qc_atan(qc_div(y, x));
}

/* Hyperbolic */
/* Share real hyperbolic and trigonometric factors for paired complex consumers. */
void qci_sinh_cosh(qcomplex_t z, qcomplex_t *sine, qcomplex_t *cosine)
{
    qfloat_t s, c, sh, ch;
    qfi_sin_cos(qc_imag(z), &s, &c);
    qfi_sinh_cosh(qc_real(z), &sh, &ch);

    *sine = qc_make(qf_mul(sh, c), qf_mul(ch, s));
    *cosine = qc_make(qf_mul(ch, c), qf_mul(sh, s));
}

/* Evaluate complex hyperbolic sine with shared real elementary factors. */
qcomplex_t qc_sinh(qcomplex_t z)
{
    qfloat_t s, c, sh, ch;
    qfi_sin_cos(qc_imag(z), &s, &c);
    qfi_sinh_cosh(qc_real(z), &sh, &ch);
    return qc_make(qf_mul(sh, c), qf_mul(ch, s));
}

/* Evaluate complex hyperbolic cosine with shared real elementary factors. */
qcomplex_t qc_cosh(qcomplex_t z)
{
    qfloat_t s, c, sh, ch;
    qfi_sin_cos(qc_imag(z), &s, &c);
    qfi_sinh_cosh(qc_real(z), &sh, &ch);
    return qc_make(qf_mul(ch, c), qf_mul(sh, s));
}

/* Form complex hyperbolic tangent from one paired evaluation. */
qcomplex_t qc_tanh(qcomplex_t z)
{
    qcomplex_t sine, cosine;
    qci_sinh_cosh(z, &sine, &cosine);
    return qc_div(sine, cosine);
}
qcomplex_t qc_sech(qcomplex_t z)
{
    return qc_div(QC_ONE, qc_cosh(z));
}
qcomplex_t qc_cosech(qcomplex_t z)
{
    return qc_div(QC_ONE, qc_sinh(z));
}
/* Form complex hyperbolic cotangent from one paired evaluation. */
qcomplex_t qc_coth(qcomplex_t z)
{
    qcomplex_t sine, cosine;
    qci_sinh_cosh(z, &sine, &cosine);
    return qc_div(cosine, sine);
}
qcomplex_t qc_asinh(qcomplex_t z)
{
    /* asinh(z) = log(z + sqrt(z^2 + 1)) */
    return qc_log(qc_add(z, qc_sqrt(qc_add(qc_mul(z, z), QC_ONE))));
}
qcomplex_t qc_acosh(qcomplex_t z)
{
    /* acosh(z) = log(z + sqrt(z+1) * sqrt(z-1)) */
    return qc_log(qc_add(z, qc_mul(qc_sqrt(qc_add(z, QC_ONE)), qc_sqrt(qc_sub(z, QC_ONE)))));
}
qcomplex_t qc_atanh(qcomplex_t z)
{
    /* atanh(z) = 0.5 * log((1+z)/(1-z)) */
    return qc_ldexp(qc_log(qc_div(qc_add(QC_ONE, z), qc_sub(QC_ONE, z))), -1);
}
qcomplex_t qc_asech(qcomplex_t z)
{
    return qc_acosh(qc_div(QC_ONE, z));
}
qcomplex_t qc_acosech(qcomplex_t z)
{
    return qc_asinh(qc_div(QC_ONE, z));
}
qcomplex_t qc_acoth(qcomplex_t z)
{
    return qc_atanh(qc_div(QC_ONE, z));
}
