#include "qfloat.h"

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
