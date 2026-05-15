#include "qcomplex.h"
qfloat_t qc_abs(qcomplex_t z) {
    return qf_hypot(qc_real(z), qc_imag(z));
}
qfloat_t qc_arg(qcomplex_t z) {
    return qf_atan2(qc_imag(z), qc_real(z));
}

qcomplex_t qc_from_polar(qfloat_t r, qfloat_t theta) {
    return qc_make(qf_mul(r, qf_cos(theta)), qf_mul(r, qf_sin(theta)));
}
void qc_to_polar(qcomplex_t z, qfloat_t *r, qfloat_t *theta) {
    *r     = qc_abs(z);
    *theta = qc_arg(z);
}

/* Elementary functions */
qcomplex_t qc_ldexp(qcomplex_t z, int k) {
    return qc_make(qf_ldexp(qc_real(z), k), qf_ldexp(qc_imag(z), k));
}
qcomplex_t qc_floor(qcomplex_t z) {
    return qc_make(qf_floor(qc_real(z)), qf_floor(qc_imag(z)));
}
qcomplex_t qc_hypot(qcomplex_t x, qcomplex_t y) {
    /* Not standard for complex; defined as sqrt(|x|^2 + |y|^2) */
    return qc_make(qf_hypot(qc_abs(x), qc_abs(y)), QF_ZERO);
}

/* Comparison */
bool qc_eq(qcomplex_t a, qcomplex_t b) {
    return qf_eq(qc_real(a), qc_real(b)) && qf_eq(qc_imag(a), qc_imag(b));
}
bool qc_isnan(qcomplex_t z) {
    return qf_isnan(qc_real(z)) || qf_isnan(qc_imag(z));
}
bool qc_isinf(qcomplex_t z) {
    return qf_isinf(qc_real(z)) || qf_isinf(qc_imag(z));
}
bool qc_isposinf(qcomplex_t z) {
    return qf_isposinf(qc_real(z)) || qf_isposinf(qc_imag(z));
}
bool qc_isneginf(qcomplex_t z) {
    return qf_isneginf(qc_real(z)) || qf_isneginf(qc_imag(z));
}
