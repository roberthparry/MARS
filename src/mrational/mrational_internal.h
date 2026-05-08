#ifndef MRATIONAL_INTERNAL_H
#define MRATIONAL_INTERNAL_H

#include "mrational.h"

struct _mrational_t {
    mint_t *numerator;
    mint_t *denominator;
};

int mr_normalise(mrational_t *rational);
int mr_copy_value(mrational_t *dst, const mrational_t *src);

#endif
