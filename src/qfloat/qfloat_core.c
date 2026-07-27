#define MARS_QFLOAT_INTERNAL_ACCESS
#include "qfloat_internal.h"

#include <math.h>

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
    double t3 = t1 + t2;   /* fractional part of (hi+lo) */

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
