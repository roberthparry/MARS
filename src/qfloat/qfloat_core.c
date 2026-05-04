#include "qfloat_internal.h"

#include <math.h>

/* Constants */

const qfloat_t QF_ZERO = { 0.0, 0.0 };

const qfloat_t QF_ONE  = { 1.0, 0.0 };

const qfloat_t QF_NEG_ONE = { -1.0, 0.0 };

const qfloat_t QF_HALF = { 0.5, 0.0 };

const qfloat_t QF_TWO = { 2.0, 0.0 };

const qfloat_t QF_TEN = { 10.0, 0.0 };

const qfloat_t QF_NAN = { NAN, NAN };

const qfloat_t QF_INF  = { INFINITY, 0.0 };

const qfloat_t QF_NINF = { -INFINITY, 0.0 };

const qfloat_t QF_MAX = {
    1.79769313486231570815e+308,
    9.97920154767359795037e+291
};

const qfloat_t QF_PI = {
    3.14159265358979311600e+00,
    1.2246467991473532072e-16
};

const qfloat_t QF_2PI = {
    6.28318530717958623200e+00,
    2.4492935982947064144e-16
};

const qfloat_t QF_PI_2 = {
    1.57079632679489655800e+00,
    6.123233995736766036e-17
};

const qfloat_t QF_PI_4 = {
    7.8539816339744827900e-01,
    3.061616997868383018e-17
};

const qfloat_t QF_3PI_4 = {
    2.356194490192344837e+00,
    9.1848509936051484375e-17
};

const qfloat_t QF_PI_6 = {
    0.52359877559829893,
    -5.3604088322554746e-17
};

const qfloat_t QF_PI_3 = {
    1.0471975511965979,
    -1.0720817664510948e-16
};

const qfloat_t QF_2_PI = {
    0.63661977236758134308,   /* hi */
    -1.073741823e-17          /* lo (approx; compute with your qfloat_t printer) */
};

const qfloat_t QF_E = {
    2.718281828459045091e+00,
    1.445646891729250158e-16
};

const qfloat_t QF_INV_E = {
    0.36787944117144233,
    -1.2428753672788614e-17
};

const qfloat_t QF_LN2 = {
    6.9314718055994530941723212145817656e-01,   // hi (double)
    2.3190468138462995584177710792423e-17       // lo = ln2 - hi
};

const qfloat_t QF_INVLN2 = {
    1.4426950408889634073599246810019e+00,      // hi (double)
    2.0355273740931032980408127082449e-17       // lo = 1/ln2 - hi
};

const qfloat_t QF_SQRT_HALF = {
    0.70710678118654757,
    -4.8336466567264851e-17
};

const qfloat_t QF_SQRT_PI = {
    1.7724538509055161,
    -7.6665864998258049e-17
};

const qfloat_t QF_SQRT1ONPI = {
    0.56418958354775628,
    7.6677298065829314e-18
};

const qfloat_t QF_2_SQRTPI = {
    1.1283791670955126,
    1.5335459613165487e-17
};

const qfloat_t QF_INV_SQRT_2PI = {
    0.3989422804014327,
    -2.4923272022777433e-17
};

const qfloat_t QF_LOG_SQRT_2PI = {
    0.91893853320467278,
    -3.8782941580672716e-17
};

const qfloat_t QF_LN_2PI = {
    1.8378770664093456,
    -7.7565883161345433e-17
};

const qfloat_t QF_EULER_MASCHERONI = {
    0.57721566490153287,
    -4.9429151524310308e-18
};

const double QF_EPS = 4.93038065763132e-32;

qfloat_t qf_renorm(double hi, double lo)
{
    qfloat_t r;
    double s, e;
    qf_quick_two_sum(hi, lo, &s, &e);
    r.hi = s;
    r.lo = e;
    return r;
}

/* Constructors */

qfloat_t qf_from_double(double x)
{
    return (qfloat_t) { x, 0.0 };
}

double qf_to_double(qfloat_t x) {
    return x.hi + x.lo;
}


qfloat_t qf_floor(qfloat_t x)
{
    double fh = floor(x.hi);
    double fl = floor(x.lo);

    double t1 = x.hi - fh;
    double t2 = x.lo - fl;
    double t3 = t1 + t2;   // fractional part of (hi+lo)

    int t = (int)floor(t3);

    qfloat_t r;

    switch (t) {
        case 0:
            r = qf_from_double(fh);
            r = qf_add(r, qf_from_double(fl));
            break;

        case 1:
            r = qf_from_double(fh);
            r = qf_add(r, qf_from_double(fl + 1.0));
            break;

        case 2:
            r = qf_from_double(fh);
            r = qf_add(r, qf_from_double(fl + 1.0));
            break;
    }

    return r;
}

/* Round qfloat_t to nearest integer (ties to even) */
qfloat_t qf_rint(qfloat_t x)
{
    qfloat_t t = QF_HALF;
    t = qf_add(t, x);
    return qf_floor(t);
}


qfloat_t qf_abs(qfloat_t x)
{
    if (x.hi < 0.0 || (x.hi == 0.0 && x.lo < 0.0)) {
        qfloat_t r;
        r.hi = -x.hi;
        r.lo = -x.lo;
        return r;
    }
    return x;
}

qfloat_t qf_neg(qfloat_t x) {
    qfloat_t r = { -x.hi, -x.lo };
    return r;
}

bool qf_eq(qfloat_t a, qfloat_t b)
{
    return a.hi == b.hi && a.lo == b.lo;
}

bool qf_lt(qfloat_t a, qfloat_t b)
{
    if (a.hi < b.hi) return 1;
    if (a.hi > b.hi) return 0;
    return a.lo < b.lo;
}

bool qf_le(qfloat_t a, qfloat_t b)
{
    if (a.hi < b.hi) return 1;
    if (a.hi > b.hi) return 0;
    return a.lo <= b.lo;
}

bool qf_gt(qfloat_t a, qfloat_t b)
{
    if (a.hi > b.hi) return 1;
    if (a.hi < b.hi) return 0;
    return a.lo > b.lo;
}

bool qf_ge(qfloat_t a, qfloat_t b)
{
    if (a.hi > b.hi) return 1;
    if (a.hi < b.hi) return 0;
    return a.lo >= b.lo;
}

int qf_cmp(qfloat_t a, qfloat_t b) {
    if (qf_eq(a, b)) return 0;
    if (qf_lt(a, b)) return -1;
    return 1;
}

int qf_signbit(qfloat_t x)
{
    return signbit(x.hi);
}

qfloat_t qf_mul_pow10(qfloat_t x, int k)
{
    double p = pow(10.0, (double)k);
    return qf_mul(x, qf_from_double(p));
}
