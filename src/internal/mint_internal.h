#ifndef MINT_SHARED_INTERNAL_H
#define MINT_SHARED_INTERNAL_H

#include <gmp.h>

#include "mint.h"

typedef enum mint_constant_id_t {
    MICONST_NONE = 0,
    MICONST_ZERO,
    MICONST_ONE,
    MICONST_NEG_ONE,
    MICONST_TWO,
    MICONST_TEN,
    MICONST_COUNT
} mint_constant_id_t;

extern mint_t MI_ZERO_VALUE;
extern mint_t MI_ONE_VALUE;
extern mint_t MI_NEG_ONE_VALUE;
extern mint_t MI_TWO_VALUE;
extern mint_t MI_TEN_VALUE;

struct _mint_t {
    mint_constant_id_t constant_id;
    mpz_t value;
};

void mint_constants_ensure_init(void);
void mint_constant_ensure(const mint_t *mint);

#endif
