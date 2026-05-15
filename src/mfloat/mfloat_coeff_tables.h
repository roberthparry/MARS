#ifndef MFLOAT_COEFF_TABLES_H
#define MFLOAT_COEFF_TABLES_H

#include "internal/mint_internal.h"

typedef struct mfloat_gamma_coeff_seed_t {
    const mint_t *num;
    const mint_t *den;
    unsigned power;
} mfloat_gamma_coeff_seed_t;

#define MFLOAT_EULER_GAMMA_COEFF_COUNT 16u

extern const mfloat_gamma_coeff_seed_t
mfloat_euler_gamma_coeffs[MFLOAT_EULER_GAMMA_COEFF_COUNT];

#endif
