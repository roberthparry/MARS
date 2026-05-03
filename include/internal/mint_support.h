#ifndef MINT_SUPPORT_H
#define MINT_SUPPORT_H

#include "mint_layout.h"

int mint_copy_value(mint_t *dst, const mint_t *src);
int mint_set_magnitude_u64(mint_t *mint, uint64_t magnitude, short sign);

#endif
