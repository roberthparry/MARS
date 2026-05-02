#include "qcomplex_internal.h"

const qcomplex_t QC_ZERO = {
    .re = { .hi = 0.0, .lo = 0.0 },
    .im = { .hi = 0.0, .lo = 0.0 }
};

const qcomplex_t QC_ONE = {
    .re = { .hi = 1.0, .lo = 0.0 },
    .im = { .hi = 0.0, .lo = 0.0 }
};
qfloat_t qc_abs(qcomplex_t z) {
    return qf_hypot(z.re, z.im);
}
qfloat_t qc_arg(qcomplex_t z) {
    return qf_atan2(z.im, z.re);
}

qcomplex_t qc_from_polar(qfloat_t r, qfloat_t theta) {
    return qc_make(qf_mul(r, qf_cos(theta)), qf_mul(r, qf_sin(theta)));
}
void qc_to_polar(qcomplex_t z, qfloat_t *r, qfloat_t *theta) {
    *r     = qc_abs(z);
    *theta = qc_arg(z);
}

// Elementary functions
qcomplex_t qc_ldexp(qcomplex_t z, int k) {
    return qc_make(qf_ldexp(z.re, k), qf_ldexp(z.im, k));
}
qcomplex_t qc_floor(qcomplex_t z) {
    return qc_make(qf_floor(z.re), qf_floor(z.im));
}
qcomplex_t qc_hypot(qcomplex_t x, qcomplex_t y) {
    // Not standard for complex; defined as sqrt(|x|^2 + |y|^2)
    return qcrf(qf_hypot(qc_abs(x), qc_abs(y)));
}

// Comparison
bool qc_eq(qcomplex_t a, qcomplex_t b) {
    return qf_eq(a.re, b.re) && qf_eq(a.im, b.im);
}
bool qc_isnan(qcomplex_t z) {
    return qf_isnan(z.re) || qf_isnan(z.im);
}
bool qc_isinf(qcomplex_t z) {
    return qf_isinf(z.re) || qf_isinf(z.im);
}
bool qc_isposinf(qcomplex_t z) {
    return qf_isposinf(z.re) || qf_isposinf(z.im);
}
bool qc_isneginf(qcomplex_t z) {
    return qf_isneginf(z.re) || qf_isneginf(z.im);
}

