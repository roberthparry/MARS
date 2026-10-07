/**
 * @file qcomplex_special.c
 * @brief Orthogonal polynomial recurrences and scalar signal functions.
 *
 * Groups the small algebraic function families implemented directly with qcomplex arithmetic: Chebyshev and
 * Hermite recurrences, sign and step conventions, pulse profiles and normalised sinc. These operations retain
 * their existing domain and endpoint behaviour and remain separate from the guarded cylindrical-function adapters.
 */

#include "qcomplex.h"

static qcomplex_t polynomial(unsigned long degree, qcomplex_t x, unsigned family)
{
    qcomplex_t previous = QC_ONE;
    qcomplex_t twice_x = qc_add(x, x);
    qcomplex_t current = family == 0u ? x : twice_x;
    if (degree == 0ul)
        return previous;
    for (unsigned long k = 1ul; k < degree; ++k) {
        qcomplex_t factor = family == 2u ? qc_make(qf_from_double((double)k), QF_ZERO) : QC_ONE;
        if (family == 2u)
            factor = qc_add(factor, factor);
        qcomplex_t next = qc_sub(qc_mul(twice_x, current), qc_mul(factor, previous));
        previous = current;
        current = next;
    }
    return current;
}

/* Evaluate the first-kind Chebyshev polynomial by its three-term recurrence. */
qcomplex_t qc_chebyshev_t(unsigned long degree, qcomplex_t x) { return polynomial(degree, x, 0u); }

/* Evaluate the second-kind Chebyshev polynomial by its three-term recurrence. */
qcomplex_t qc_chebyshev_u(unsigned long degree, qcomplex_t x) { return polynomial(degree, x, 1u); }

/* Evaluate the physicists' Hermite polynomial by its three-term recurrence. */
qcomplex_t qc_hermite_h(unsigned long degree, qcomplex_t x) { return polynomial(degree, x, 2u); }

/* Real support functions have no ordered complex continuation. */
static qcomplex_t signal_real(qcomplex_t z, qfloat_t (*function)(qfloat_t))
{
    return qf_eq(qc_imag(z), QF_ZERO) ? qc_make(function(qc_real(z)), QF_ZERO) : qc_make(QF_NAN, QF_NAN);
}

/* Restrict signum to real arguments. */
qcomplex_t qc_sgn(qcomplex_t z) { return signal_real(z, qf_sgn); }
/* Restrict the unit step to real arguments. */
qcomplex_t qc_step(qcomplex_t z) { return signal_real(z, qf_step); }
/* Restrict the rectangular pulse to real arguments. */
qcomplex_t qc_rect(qcomplex_t z) { return signal_real(z, qf_rect); }
/* Restrict the triangular pulse to real arguments. */
qcomplex_t qc_tri(qcomplex_t z) { return signal_real(z, qf_tri); }
/* Restrict the radial aperture profile to real arguments. */
qcomplex_t qc_circ(qcomplex_t z) { return signal_real(z, qf_circ); }

/* Evaluate the entire normalised sinc function. */
qcomplex_t qc_sinc(qcomplex_t z)
{
    if (qf_eq(qc_real(z), QF_ZERO) && qf_eq(qc_imag(z), QF_ZERO))
        return qc_make(QF_ONE, QF_ZERO);
    qcomplex_t angle = qc_mul(qc_make(QF_PI, QF_ZERO), z);
    return qc_div(qc_sin(angle), angle);
}
