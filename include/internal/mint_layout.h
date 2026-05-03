#ifndef MINT_LAYOUT_H
#define MINT_LAYOUT_H

#include <stdint.h>

#include "mint.h"

struct _mint_t {
    short sign;       /* -1, 0, +1 */
    size_t length;    /* number of used 64-bit limbs */
    size_t capacity;  /* number of allocated 64-bit limbs */
    uint64_t *storage;
};

#endif
