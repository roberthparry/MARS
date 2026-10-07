/**
 * @file qfloat_special.c
 * @brief Orthogonal polynomial recurrences and scalar signal functions.
 *
 * Groups the small algebraic function families implemented directly with qfloat arithmetic: Chebyshev and Hermite
 * recurrences, sign and step conventions, pulse profiles and normalised sinc. These operations retain their
 * existing domain and endpoint behaviour and remain separate from the guarded cylindrical-function adapters.
 */

#include "qfloat.h"

static qfloat_t polynomial(unsigned long degree, qfloat_t x, unsigned family)
{
    qfloat_t previous = QF_ONE;
    qfloat_t twice_x = qf_add(x, x);
    qfloat_t current = family == 0u ? x : twice_x;
    if (degree == 0ul)
        return previous;
    for (unsigned long k = 1ul; k < degree; ++k) {
        qfloat_t factor = family == 2u ? qf_from_double((double)k) : QF_ONE;
        if (family == 2u)
            factor = qf_add(factor, factor);
        qfloat_t next = qf_sub(qf_mul(twice_x, current), qf_mul(factor, previous));
        previous = current;
        current = next;
    }
    return current;
}

/* Evaluate the first-kind Chebyshev polynomial by its three-term recurrence. */
qfloat_t qf_chebyshev_t(unsigned long degree, qfloat_t x) { return polynomial(degree, x, 0u); }

/* Evaluate the second-kind Chebyshev polynomial by its three-term recurrence. */
qfloat_t qf_chebyshev_u(unsigned long degree, qfloat_t x) { return polynomial(degree, x, 1u); }

/* Evaluate the physicists' Hermite polynomial by its three-term recurrence. */
qfloat_t qf_hermite_h(unsigned long degree, qfloat_t x) { return polynomial(degree, x, 2u); }

/* Evaluate real signum, preserving NaN and treating both signed zeros as zero. */
qfloat_t qf_sgn(qfloat_t x)
{
    return qf_isnan(x) ? QF_NAN : qf_eq(x, QF_ZERO) ? QF_ZERO : qf_gt(x, QF_ZERO) ? QF_ONE : QF_NEG_ONE;
}

/* Evaluate the symmetric endpoint convention for the real unit step. */
qfloat_t qf_step(qfloat_t x)
{
    return qf_isnan(x) ? QF_NAN : qf_eq(x, QF_ZERO) ? QF_HALF : qf_gt(x, QF_ZERO) ? QF_ONE : QF_ZERO;
}

/* Evaluate the unit-width rectangular pulse. */
qfloat_t qf_rect(qfloat_t x)
{
    return qf_step(qf_sub(QF_HALF, qf_abs(x)));
}

/* Evaluate the unit-height triangular pulse. */
qfloat_t qf_tri(qfloat_t x)
{
    qfloat_t a = qf_abs(x);
    return qf_isnan(a) ? QF_NAN : qf_ge(a, QF_ONE) ? QF_ZERO : qf_sub(QF_ONE, a);
}

/* Extend the radial aperture evenly to real arguments. */
qfloat_t qf_circ(qfloat_t x)
{
    return qf_step(qf_sub(QF_ONE, qf_abs(x)));
}

/* Evaluate normalised sinc without dividing by zero at its removable singularity. */
qfloat_t qf_sinc(qfloat_t x)
{
    qfloat_t angle = qf_mul(QF_PI, x);
    return qf_eq(x, QF_ZERO) ? QF_ONE : qf_div(qf_sin(angle), angle);
}
