#ifndef QFLOAT_INTERNAL_H
#define QFLOAT_INTERNAL_H

#include <math.h>

#include "qfloat.h"

#define QF_SPLIT 134217729.0

static inline void qf_two_sum(double a, double b, double *s, double *e)
{
    *s = a + b;
    double bb = *s - a;
    *e = (a - (*s - bb)) + (b - bb);
}

static inline void qf_two_prod(double a, double b, double *p, double *e)
{
    *p = a * b;
    *e = fma(a, b, -*p);
}

static inline void qf_quick_two_sum(double a, double b, double *s, double *e)
{
    double t = a + b;
    *s = t;
    *e = b - (t - a);
}

static inline void qf_split_double(double x, double *hi, double *lo)
{
    double t = QF_SPLIT * x;
    *hi = t - (t - x);
    *lo = x - *hi;
}

static inline int qf_to_int(qfloat_t x)
{
    return (int)(x.hi + x.lo);
}

qfloat_t qf_renorm(double hi, double lo);

#endif
