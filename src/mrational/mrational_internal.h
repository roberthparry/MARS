#ifndef MRATIONAL_INTERNAL_H
#define MRATIONAL_INTERNAL_H

#include <gmp.h>

#include "mrational.h"

typedef enum mrational_constant_id_t {
    MRCONST_NONE = 0,
    MRCONST_HALF,
    MRCONST_ONE_AND_HALF,
    MRCONST_ONE_THIRD,
    MRCONST_QUARTER,
    MRCONST_ONE_SIXTH,
    MRCONST_ONE_EIGHTH,
    MRCONST_ONE_TENTH,
    MRCONST_COUNT
} mrational_constant_id_t;

struct _mrational_t {
    mrational_constant_id_t constant_id;
    mpq_t value;
    mint_t *numerator_view;
    mint_t *denominator_view;
};

void mrational_constant_ensure(const mrational_t *rational);
void mrational_constants_ensure_init(void);
int mrational_prepare_mutable(mrational_t *rational);
int mrational_sync_views(mrational_t *rational);

#endif
