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
