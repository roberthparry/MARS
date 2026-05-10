#include <math.h>

#include "qcomplex.h"

static qcomplex_t qc_pow_int(qcomplex_t z, int n)
{
    qcomplex_t result = QC_ONE;
    qcomplex_t base = z;

    if (n == 0)
        return QC_ONE;

    if (n < 0)
        return qc_div(QC_ONE, qc_pow_int(z, -n));

    while (n > 0) {
        if (n & 1)
            result = qc_mul(result, base);
        base = qc_mul(base, base);
        n >>= 1;
    }

    return result;
}

qcomplex_t qc_add(qcomplex_t a, qcomplex_t b) {
    return qc_make(qf_add(qc_real(a), qc_real(b)), qf_add(qc_imag(a), qc_imag(b)));
}
qcomplex_t qc_sub(qcomplex_t a, qcomplex_t b) {
    return qc_make(qf_sub(qc_real(a), qc_real(b)), qf_sub(qc_imag(a), qc_imag(b)));
}
qcomplex_t qc_neg(qcomplex_t a) {
    return qc_make(qf_neg(qc_real(a)), qf_neg(qc_imag(a)));
}
qcomplex_t qc_conj(qcomplex_t a) {
    return qc_make(qc_real(a), qf_neg(qc_imag(a)));
}
qcomplex_t qc_mul(qcomplex_t a, qcomplex_t b) {
    qfloat_t re = qf_sub(qf_mul(qc_real(a), qc_real(b)), qf_mul(qc_imag(a), qc_imag(b)));
    qfloat_t im = qf_add(qf_mul(qc_real(a), qc_imag(b)), qf_mul(qc_imag(a), qc_real(b)));
    return qc_make(re, im);
}
qcomplex_t qc_div(qcomplex_t a, qcomplex_t b) {
    qfloat_t denom = qf_add(qf_mul(qc_real(b), qc_real(b)), qf_mul(qc_imag(b), qc_imag(b)));
    qfloat_t re = qf_div(qf_add(qf_mul(qc_real(a), qc_real(b)), qf_mul(qc_imag(a), qc_imag(b))), denom);
    qfloat_t im = qf_div(qf_sub(qf_mul(qc_imag(a), qc_real(b)), qf_mul(qc_real(a), qc_imag(b))), denom);
    return qc_make(re, im);
}

/* Elementary functions */
qcomplex_t qc_exp(qcomplex_t z) {
    qfloat_t exp_re = qf_exp(qc_real(z));
    return qc_make(qf_mul(exp_re, qf_cos(qc_imag(z))), qf_mul(exp_re, qf_sin(qc_imag(z))));
}
qcomplex_t qc_log(qcomplex_t z) {
    if (qf_eq(qc_real(z), qf_from_double(-1.0)) &&
        qf_lt(qf_abs(qc_imag(z)), qf_from_string("1e-18")) &&
        !qf_eq(qc_imag(z), QF_ZERO)) {
        return qc_make(QF_ZERO, qf_signbit(qc_imag(z)) ? qf_neg(QF_PI) : QF_PI);
    }
    return qc_make(qf_log(qc_abs(z)), qc_arg(z));
}
qcomplex_t qc_pow(qcomplex_t a, qcomplex_t b) {
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
        return qc_make(qf_mul(mag, qf_cos(theta)),
                       qf_mul(mag, qf_sin(theta)));
    }

    qcomplex_t log_a = qc_log(a);
    qcomplex_t prod = qc_mul(b, log_a);
    return qc_exp(prod);
}
qcomplex_t qc_sqrt(qcomplex_t z) {
    qfloat_t r          = qc_abs(z);
    qfloat_t sqrt_r     = qf_sqrt(r);
    qfloat_t half_theta = qf_ldexp(qc_arg(z), -1);
    return qc_make(qf_mul(sqrt_r, qf_cos(half_theta)),
                   qf_mul(sqrt_r, qf_sin(half_theta)));
}

/* Trigonometric */
qcomplex_t qc_sin(qcomplex_t z) {
    /* sin(z) = sin(x)cosh(y) + i cos(x)sinh(y) */
    return qc_make(qf_mul(qf_sin(qc_real(z)), qf_cosh(qc_imag(z))),
                   qf_mul(qf_cos(qc_real(z)), qf_sinh(qc_imag(z))));
}
qcomplex_t qc_cos(qcomplex_t z) {
    /* cos(z) = cos(x)cosh(y) - i sin(x)sinh(y) */
    return qc_make(qf_mul(qf_cos(qc_real(z)), qf_cosh(qc_imag(z))),
                   qf_neg(qf_mul(qf_sin(qc_real(z)), qf_sinh(qc_imag(z)))));
}
qcomplex_t qc_tan(qcomplex_t z) {
    return qc_div(qc_sin(z), qc_cos(z));
}
qcomplex_t qc_asin(qcomplex_t z) {
    /* asin(z) = -i log(iz + sqrt(1-z^2)) */
    /* -i*(a+bi) = b - ai, so re=qc_imag(ln), im=-qc_real(ln) */
    qcomplex_t iz        = qc_make(qf_neg(qc_imag(z)), qc_real(z));
    qcomplex_t sqrt_term = qc_sqrt(qc_sub(QC_ONE, qc_mul(z, z)));
    qcomplex_t ln        = qc_log(qc_add(iz, sqrt_term));
    return qc_make(qc_imag(ln), qf_neg(qc_real(ln)));
}
qcomplex_t qc_acos(qcomplex_t z) {
    /* acos(z) = -i log(z + i sqrt(1-z^2)) */
    /* -i*(a+bi) = b - ai, so re=qc_imag(ln), im=-qc_real(ln) */
    qcomplex_t sqrt_term = qc_sqrt(qc_sub(QC_ONE, qc_mul(z, z)));
    qcomplex_t iz        = qc_make(qf_neg(qc_imag(sqrt_term)), qc_real(sqrt_term));
    qcomplex_t ln        = qc_log(qc_add(z, iz));
    return qc_make(qc_imag(ln), qf_neg(qc_real(ln)));
}
qcomplex_t qc_atan(qcomplex_t z) {
    /* atan(z) = (i/2) * [log(1 - iz) - log(1 + iz)] */
    /* (i/2)*(a+bi) = -b/2 + (a/2)i, so re=-qc_imag(diff)/2, im=qc_real(diff)/2 */
    qcomplex_t iz   = qc_make(qf_neg(qc_imag(z)), qc_real(z));
    qcomplex_t diff = qc_sub(qc_log(qc_sub(QC_ONE, iz)),
                             qc_log(qc_add(QC_ONE, iz)));
    return qc_make(qf_mul_double(qf_neg(qc_imag(diff)), 0.5),
                   qf_mul_double(qc_real(diff), 0.5));
}
qcomplex_t qc_atan2(qcomplex_t y, qcomplex_t x) {
    if (qf_eq(qc_imag(y), qf_from_double(0.0)) && qf_eq(qc_imag(x), qf_from_double(0.0)))
        return qc_make(qf_atan2(qc_real(y), qc_real(x)), QF_ZERO);
    return qc_atan(qc_div(y, x));
}

/* Hyperbolic */
qcomplex_t qc_sinh(qcomplex_t z) {
    /* sinh(z) = sinh(x)cos(y) + i cosh(x)sin(y) */
    return qc_make(qf_mul(qf_sinh(qc_real(z)), qf_cos(qc_imag(z))),
                   qf_mul(qf_cosh(qc_real(z)), qf_sin(qc_imag(z))));
}
qcomplex_t qc_cosh(qcomplex_t z) {
    /* cosh(z) = cosh(x)cos(y) + i sinh(x)sin(y) */
    return qc_make(qf_mul(qf_cosh(qc_real(z)), qf_cos(qc_imag(z))),
                   qf_mul(qf_sinh(qc_real(z)), qf_sin(qc_imag(z))));
}
qcomplex_t qc_tanh(qcomplex_t z) {
    return qc_div(qc_sinh(z), qc_cosh(z));
}
qcomplex_t qc_asinh(qcomplex_t z) {
    /* asinh(z) = log(z + sqrt(z^2 + 1)) */
    return qc_log(qc_add(z, qc_sqrt(qc_add(qc_mul(z, z), QC_ONE))));
}
qcomplex_t qc_acosh(qcomplex_t z) {
    /* acosh(z) = log(z + sqrt(z+1) * sqrt(z-1)) */
    return qc_log(qc_add(z, qc_mul(qc_sqrt(qc_add(z, QC_ONE)),
                                   qc_sqrt(qc_sub(z, QC_ONE)))));
}
qcomplex_t qc_atanh(qcomplex_t z) {
    /* atanh(z) = 0.5 * log((1+z)/(1-z)) */
    return qc_ldexp(qc_log(qc_div(qc_add(QC_ONE, z),
                                  qc_sub(QC_ONE, z))), -1);
}
