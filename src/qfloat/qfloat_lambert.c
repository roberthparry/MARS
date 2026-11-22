/**
 * @file qfloat_lambert.c
 * @brief Real double-double entry point for MARS's defined Lambert sum.
 *
 * Delegates to the native complex kernel with zero imaginary arguments, retaining both components of every
 * double-double real value. Non-real results return NaN rather than discarding an imaginary component.
 */
#include "qcomplex-t.h"
#include "qfloat-t.h"

/* Evaluate the real Lambert sum without introducing a multiprecision dependency. */
qfloat_t qf_lambert_S(qfloat_t a, qfloat_t q, qfloat_t z, qfloat_t weight, qfloat_t order, qfloat_t count)
{
    qcomplex_t value = qc_lambert_S(qc_make(a, QF_ZERO), qc_make(q, QF_ZERO), qc_make(z, QF_ZERO),
                                   qc_make(weight, QF_ZERO), qc_make(order, QF_ZERO), qc_make(count, QF_ZERO));
    return qf_eq(qc_imag(value), QF_ZERO) ? qc_real(value) : QF_NAN;
}
