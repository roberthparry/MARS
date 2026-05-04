#ifndef MCOMPLEX_INTERNAL_H
#define MCOMPLEX_INTERNAL_H

#include <stdbool.h>

#include "mcomplex.h"
#include "qcomplex.h"

struct _mcomplex_t {
    mfloat_t *real;
    mfloat_t *imag;
    bool immortal;
};

int mcomplex_ensure_mutable(mcomplex_t *mcomplex);
int mcomplex_apply_unary(mcomplex_t *mcomplex, qcomplex_t (*fn)(qcomplex_t));
int mcomplex_apply_binary(mcomplex_t *mcomplex,
                          const mcomplex_t *other,
                          qcomplex_t (*fn)(qcomplex_t, qcomplex_t));

#endif
