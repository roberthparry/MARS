#include "qcomplex.h"

/* Real support functions have no ordered complex continuation. */
static qcomplex_t signal_real(qcomplex_t z, qfloat_t (*function)(qfloat_t))
{
    return qf_eq(qc_imag(z), QF_ZERO) ? qc_make(function(qc_real(z)), QF_ZERO) : qc_make(QF_NAN, QF_NAN);
}

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
