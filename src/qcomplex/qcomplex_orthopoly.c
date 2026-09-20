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
