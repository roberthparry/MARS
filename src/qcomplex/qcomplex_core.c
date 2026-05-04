#include <math.h>

#include "qcomplex_internal.h"

const qcomplex_t QC_ZERO = {
    .re = { .hi = 0.0, .lo = 0.0 },
    .im = { .hi = 0.0, .lo = 0.0 }
};

const qcomplex_t QC_ONE = {
    .re = { .hi = 1.0, .lo = 0.0 },
    .im = { .hi = 0.0, .lo = 0.0 }
};

const qcomplex_t QC_NEG_ONE = {
    .re = { .hi = -1.0, .lo = 0.0 },
    .im = { .hi = 0.0, .lo = 0.0 }
};

const qcomplex_t QC_HALF = {
    .re = { .hi = 0.5, .lo = 0.0 },
    .im = { .hi = 0.0, .lo = 0.0 }
};

const qcomplex_t QC_TWO = {
    .re = { .hi = 2.0, .lo = 0.0 },
    .im = { .hi = 0.0, .lo = 0.0 }
};

const qcomplex_t QC_I = {
    .re = { .hi = 0.0, .lo = 0.0 },
    .im = { .hi = 1.0, .lo = 0.0 }
};

const qcomplex_t QC_NAN = {
    .re = { .hi = NAN, .lo = NAN },
    .im = { .hi = NAN, .lo = NAN }
};

const qcomplex_t QC_INF = {
    .re = { .hi = INFINITY, .lo = 0.0 },
    .im = { .hi = 0.0, .lo = 0.0 }
};

const qcomplex_t QC_NINF = {
    .re = { .hi = -INFINITY, .lo = 0.0 },
    .im = { .hi = 0.0, .lo = 0.0 }
};

const qcomplex_t QC_MAX = {
    .re = { .hi = 1.79769313486231570815e+308, .lo = 9.97920154767359795037e+291 },
    .im = { .hi = 0.0, .lo = 0.0 }
};

const qcomplex_t QC_PI = {
    .re = { .hi = 3.14159265358979311600e+00, .lo = 1.2246467991473532072e-16 },
    .im = { .hi = 0.0, .lo = 0.0 }
};

const qcomplex_t QC_2PI = {
    .re = { .hi = 6.28318530717958623200e+00, .lo = 2.4492935982947064144e-16 },
    .im = { .hi = 0.0, .lo = 0.0 }
};

const qcomplex_t QC_PI_2 = {
    .re = { .hi = 1.57079632679489655800e+00, .lo = 6.123233995736766036e-17 },
    .im = { .hi = 0.0, .lo = 0.0 }
};

const qcomplex_t QC_PI_4 = {
    .re = { .hi = 7.8539816339744827900e-01, .lo = 3.061616997868383018e-17 },
    .im = { .hi = 0.0, .lo = 0.0 }
};

const qcomplex_t QC_3PI_4 = {
    .re = { .hi = 2.356194490192344837e+00, .lo = 9.1848509936051484375e-17 },
    .im = { .hi = 0.0, .lo = 0.0 }
};

const qcomplex_t QC_PI_6 = {
    .re = { .hi = 0.52359877559829893, .lo = -5.3604088322554746e-17 },
    .im = { .hi = 0.0, .lo = 0.0 }
};

const qcomplex_t QC_PI_3 = {
    .re = { .hi = 1.0471975511965979, .lo = -1.0720817664510948e-16 },
    .im = { .hi = 0.0, .lo = 0.0 }
};

const qcomplex_t QC_2_PI = {
    .re = { .hi = 0.63661977236758134308, .lo = -1.073741823e-17 },
    .im = { .hi = 0.0, .lo = 0.0 }
};

const qcomplex_t QC_E = {
    .re = { .hi = 2.718281828459045091e+00, .lo = 1.445646891729250158e-16 },
    .im = { .hi = 0.0, .lo = 0.0 }
};

const qcomplex_t QC_INV_E = {
    .re = { .hi = 0.36787944117144233, .lo = -1.2428753672788614e-17 },
    .im = { .hi = 0.0, .lo = 0.0 }
};

const qcomplex_t QC_LN2 = {
    .re = { .hi = 6.9314718055994530941723212145817656e-01, .lo = 2.3190468138462995584177710792423e-17 },
    .im = { .hi = 0.0, .lo = 0.0 }
};

const qcomplex_t QC_INVLN2 = {
    .re = { .hi = 1.4426950408889634073599246810019e+00, .lo = 2.0355273740931032980408127082449e-17 },
    .im = { .hi = 0.0, .lo = 0.0 }
};

const qcomplex_t QC_SQRT_HALF = {
    .re = { .hi = 0.70710678118654757, .lo = -4.8336466567264851e-17 },
    .im = { .hi = 0.0, .lo = 0.0 }
};

const qcomplex_t QC_SQRT2 = {
    .re = { .hi = 1.4142135623730951, .lo = -9.6672933134529704e-17 },
    .im = { .hi = 0.0, .lo = 0.0 }
};

const qcomplex_t QC_SQRT_PI = {
    .re = { .hi = 1.7724538509055161, .lo = -7.6665864998258049e-17 },
    .im = { .hi = 0.0, .lo = 0.0 }
};

const qcomplex_t QC_SQRT1ONPI = {
    .re = { .hi = 0.56418958354775628, .lo = 7.6677298065829314e-18 },
    .im = { .hi = 0.0, .lo = 0.0 }
};

const qcomplex_t QC_2_SQRTPI = {
    .re = { .hi = 1.1283791670955126, .lo = 1.5335459613165487e-17 },
    .im = { .hi = 0.0, .lo = 0.0 }
};

const qcomplex_t QC_INV_SQRT_2PI = {
    .re = { .hi = 0.3989422804014327, .lo = -2.4923272022777433e-17 },
    .im = { .hi = 0.0, .lo = 0.0 }
};

const qcomplex_t QC_LOG_SQRT_2PI = {
    .re = { .hi = 0.91893853320467278, .lo = -3.8782941580672716e-17 },
    .im = { .hi = 0.0, .lo = 0.0 }
};

const qcomplex_t QC_LN_2PI = {
    .re = { .hi = 1.8378770664093456, .lo = -7.7565883161345433e-17 },
    .im = { .hi = 0.0, .lo = 0.0 }
};

const qcomplex_t QC_EULER_MASCHERONI = {
    .re = { .hi = 0.57721566490153287, .lo = -4.9429151524310308e-18 },
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

/* Elementary functions */
qcomplex_t qc_ldexp(qcomplex_t z, int k) {
    return qc_make(qf_ldexp(z.re, k), qf_ldexp(z.im, k));
}
qcomplex_t qc_floor(qcomplex_t z) {
    return qc_make(qf_floor(z.re), qf_floor(z.im));
}
qcomplex_t qc_hypot(qcomplex_t x, qcomplex_t y) {
    /* Not standard for complex; defined as sqrt(|x|^2 + |y|^2) */
    return qcrf(qf_hypot(qc_abs(x), qc_abs(y)));
}

/* Comparison */
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
