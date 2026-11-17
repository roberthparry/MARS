/**
 * @file qfloat_appell.c
 * @brief Real double-double entry points for all four Appell functions.
 *
 * F1 delegates to the real two-variable Lauricella FD implementation. F2--F4 share the fixed-precision
 * triangular recurrence with the complex module using exactly zero imaginary parts. Computed values retain
 * both double-double components; no arbitrary-precision bridge, shared mutable state or ownership transfer is used.
 */
#include "qcomplex-t.h"
#include "qfloat-t.h"

/* Evaluate the first Appell function as the two-variable real Lauricella FD member. */
qfloat_t qf_appell_F1(qfloat_t a, qfloat_t b1, qfloat_t b2, qfloat_t c, qfloat_t x, qfloat_t y)
{
    const qfloat_t b[2] = {b1, b2};
    const qfloat_t variables[2] = {x, y};

    return qf_lauricella_F(a, b, c, variables, 2u);
}

/* Evaluate the real second Appell function through the native zero-imaginary series. */
qfloat_t qf_appell_F2(qfloat_t a, qfloat_t b1, qfloat_t b2, qfloat_t c1, qfloat_t c2, qfloat_t x, qfloat_t y)
{
    return qc_real(qc_appell_F2(qc_make(a, QF_ZERO), qc_make(b1, QF_ZERO), qc_make(b2, QF_ZERO),
        qc_make(c1, QF_ZERO), qc_make(c2, QF_ZERO), qc_make(x, QF_ZERO), qc_make(y, QF_ZERO)));
}

/* Evaluate the real third Appell function without changing fixed precision. */
qfloat_t qf_appell_F3(qfloat_t a1, qfloat_t a2, qfloat_t b1, qfloat_t b2, qfloat_t c, qfloat_t x, qfloat_t y)
{
    return qc_real(qc_appell_F3(qc_make(a1, QF_ZERO), qc_make(a2, QF_ZERO), qc_make(b1, QF_ZERO),
        qc_make(b2, QF_ZERO), qc_make(c, QF_ZERO), qc_make(x, QF_ZERO), qc_make(y, QF_ZERO)));
}

/* Evaluate the real fourth Appell function through the native zero-imaginary series. */
qfloat_t qf_appell_F4(qfloat_t a, qfloat_t b, qfloat_t c1, qfloat_t c2, qfloat_t x, qfloat_t y)
{
    return qc_real(qc_appell_F4(qc_make(a, QF_ZERO), qc_make(b, QF_ZERO), qc_make(c1, QF_ZERO),
        qc_make(c2, QF_ZERO), qc_make(x, QF_ZERO), qc_make(y, QF_ZERO)));
}
