#ifndef SRC_INTERNAL_MINT_INTERNAL_H
#define SRC_INTERNAL_MINT_INTERNAL_H

#include <stdint.h>

#include "mint.h"

extern const mint_t MI_ZERO_VALUE;
extern const mint_t MI_ONE_VALUE;
extern const mint_t MI_NEG_ONE_VALUE;
extern const mint_t MI_TWO_VALUE;
extern const mint_t MI_TEN_VALUE;

struct _mint_t {
    short sign;       /* -1, 0, +1 */
    size_t length;    /* number of used 64-bit limbs */
    size_t capacity;  /* number of allocated 64-bit limbs */
    uint64_t *storage;
};

int mint_copy_value(mint_t *dst, const mint_t *src);
int mint_set_magnitude_u64(mint_t *mint, uint64_t magnitude, short sign);
int mint_is_immortal(const mint_t *mint);

#endif
