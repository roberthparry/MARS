#ifndef QCOMPLEX_INTERNAL_H
#define QCOMPLEX_INTERNAL_H

#include "qcomplex.h"

static inline qcomplex_t qcr(double x)
{
    return qc_make(qf_from_double(x), qf_from_double(0.0));
}

static inline qcomplex_t qcrf(qfloat_t x)
{
    return qc_make(x, qf_from_double(0.0));
}

#endif
