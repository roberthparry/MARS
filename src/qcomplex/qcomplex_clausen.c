#include <stdbool.h>

#include "qcomplex.h"
#define MARS_SHARED_QFLOAT_INTERNAL_ACCESS
#include "internal/qfloat_internal.h"

static qcomplex_t qc_clausen_scale(qcomplex_t z, qfloat_t scale)
{
    return qc_make(qf_mul(qc_real(z), scale), qf_mul(qc_imag(z), scale));
}

static qcomplex_t qc_clausen_square(qcomplex_t z)
{
    qfloat_t real = qc_real(z);
    qfloat_t imag = qc_imag(z);

    return qc_make(qf_sub(qf_sqr(real), qf_sqr(imag)), qf_mul_double(qf_mul(real, imag), 2.0));
}

static qcomplex_t qc_clausen2_near_pi(qcomplex_t z)
{
    qcomplex_t d = qc_sub(qc_make(QF_PI, QF_ZERO), z);
    qcomplex_t ratio = qc_clausen_scale(qc_clausen_square(d), qf_div(QF_ONE, qf_sqr(QF_PI)));
    qcomplex_t power = ratio;
    qcomplex_t sum = qc_make(QF_LN2, QF_ZERO);
    qfloat_t quarter_power = qf_from_double(0.25);

    for (unsigned int j = 1u; j <= 128u; ++j) {
        qfloat_t coefficient = qf_div(qf_mul(qf_zeta(qf_from_double(2.0 * j)), qf_sub(QF_ONE, quarter_power)),
                                      qf_from_double((double)j * (2.0 * j + 1.0)));
        qcomplex_t add = qc_clausen_scale(power, coefficient);

        sum = qc_sub(sum, add);
        if (qf_le(qc_abs(add), qf_from_double(1e-34)))
            break;
        power = qc_mul(power, ratio);
        quarter_power = qf_mul_double(quarter_power, 0.25);
    }
    return qc_mul(d, sum);
}

static qcomplex_t qc_clausen_central(unsigned long order, qcomplex_t z)
{
    qcomplex_t square = qc_clausen_square(z);
    unsigned long k = (order - 1ul) & 1ul;
    qcomplex_t term = k ? z : QC_ONE;
    qcomplex_t sum = QC_ZERO;
    qfloat_t harmonic = QF_ZERO;
    qcomplex_t ratio;

    for (; k < order - 1ul; k += 2ul) {
        sum = qc_add(sum, qc_clausen_scale(term, qf_zeta(qf_from_double((double)(order - k)))));
        term = qc_neg(qc_clausen_scale(qc_mul(term, square),
                                       qf_div(QF_ONE, qf_from_double((double)(k + 1ul) * (k + 2ul)))));
    }
    for (k = 1ul; k < order; ++k)
        harmonic = qf_add(harmonic, qf_div(QF_ONE, qf_from_double((double)k)));
    sum = qc_add(sum, qc_mul(term, qc_sub(qc_make(harmonic, QF_ZERO), qc_log(z))));
    ratio = qc_clausen_scale(square, qf_div(QF_ONE, qf_sqr(QF_2PI)));
    term = qc_clausen_scale(qc_mul(term, ratio),
                            qf_div(QF_TWO, qf_from_double((double)order * (order + 1ul))));
    for (unsigned int j = 1u; j <= 128u; ++j) {
        qcomplex_t add = qc_clausen_scale(term, qf_zeta(qf_from_double(2.0 * j)));

        sum = qc_add(sum, add);
        if (qf_le(qc_abs(add), qf_mul(qf_from_double(1e-34), qc_abs(sum))))
            break;
        term = qc_clausen_scale(qc_mul(term, ratio),
                                qf_div(qf_from_double((2.0 * j) * (2.0 * j + 1.0)),
                                       qf_from_double((order + 2.0 * j) * (order + 2.0 * j + 1.0))));
    }
    return sum;
}

/* In the upper half-plane exp(i*z) is small. The inversion polynomial replaces exp(-i*z). */
static qcomplex_t qc_clausen_upper(unsigned long order, qcomplex_t z)
{
    qcomplex_t iz = qc_make(qf_neg(qc_imag(z)), qc_real(z));
    qcomplex_t w = qc_exp(iz);
    qcomplex_t power = w;
    qcomplex_t li = QC_ZERO;
    qcomplex_t polynomial = QC_ZERO;
    qcomplex_t term = QC_ONE;

    for (unsigned int k = 1u; k <= 128u; ++k) {
        qfloat_t divisor = qf_pow(qf_from_double((double)k), qf_from_double((double)order));
        qcomplex_t add = qf_isinf(divisor) ? QC_ZERO : qc_clausen_scale(power, qf_div(QF_ONE, divisor));

        li = qc_add(li, add);
        if (qf_le(qc_abs(add), qf_mul(qf_from_double(1e-34), qf_add(QF_ONE, qc_abs(li)))))
            break;
        power = qc_mul(power, w);
    }
    for (unsigned long k = 0ul; k < order; ++k) {
        if (k + 1ul == order) {
            polynomial = qc_add(polynomial, qc_mul(qc_make(QF_ZERO, QF_PI), term));
        } else if (((order - k) & 1ul) == 0ul) {
            polynomial = qc_add(polynomial,
                                 qc_clausen_scale(term, qf_mul_double(qf_zeta(qf_from_double((double)(order - k))), 2.0)));
        }
        term = qc_clausen_scale(qc_mul(term, iz), qf_div(QF_ONE, qf_from_double((double)(k + 1ul))));
        if (qc_isinf(term) || qc_isnan(term))
            return QC_NAN;
    }
    polynomial = qc_sub(polynomial, term);
    li = qc_sub(li, qc_clausen_scale(polynomial, QF_HALF));
    return (order & 1ul) ? li : qc_make(qc_imag(li), qf_neg(qc_real(li)));
}

/* Evaluate the periodic holomorphic Clausen branches, with a real-axis bridge. */
qcomplex_t qc_clausen(unsigned long order, qcomplex_t z)
{
    qfloat_t real;
    bool reflect, conjugate;
    qcomplex_t result;

    if (order == 0ul || qc_isnan(z) || qc_isinf(z))
        return QC_NAN;
    if (qf_eq(qc_imag(z), QF_ZERO))
        return qc_make(qf_clausen(order, qc_real(z)), QF_ZERO);
    real = qf_clausen_reduce(qc_real(z));
    if (qf_eq(real, QF_ZERO))
        return QC_NAN;
    reflect = qf_lt(real, QF_ZERO);
    z = qc_make(qf_abs(real), reflect ? qf_neg(qc_imag(z)) : qc_imag(z));
    conjugate = qf_lt(qc_imag(z), QF_ZERO);
    if (conjugate)
        z = qc_conj(z);

    if ((double)order >= 256.0 + 8.0 * qf_to_double(qc_imag(z))) {
        /* The higher Fourier harmonics and the continuation remainder are below native precision. */
        result = (order & 1ul) ? qc_cos(z) : qc_sin(z);
    } else if (order == 2ul &&
               qf_lt(qc_abs(qc_sub(qc_make(QF_PI, QF_ZERO), z)), qf_from_double(1.5))) {
        result = qc_clausen2_near_pi(z);
    } else if (qf_le(qc_imag(z), QF_TWO)) {
        result = order == 1ul
                     ? qc_neg(qc_log(qc_clausen_scale(qc_sin(qc_clausen_scale(z, QF_HALF)), QF_TWO)))
                     : qc_clausen_central(order, z);
    } else {
        result = qc_clausen_upper(order, z);
    }
    if (conjugate)
        result = qc_conj(result);
    return reflect && (order & 1ul) == 0ul ? qc_neg(result) : result;
}

/* Evaluate the order-two holomorphic Clausen function. */
qcomplex_t qc_clausen2(qcomplex_t z)
{
    return qc_clausen(2ul, z);
}
