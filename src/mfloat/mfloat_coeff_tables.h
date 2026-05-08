#ifndef MFLOAT_COEFF_TABLES_H
#define MFLOAT_COEFF_TABLES_H

#include <stdint.h>

#include "internal/mint_internal.h"

typedef struct mfloat_gamma_coeff_seed_t {
    const mint_t *num;
    const mint_t *den;
    unsigned power;
} mfloat_gamma_coeff_seed_t;

static uint64_t mfloat_gamma_coeff_0_num_storage[] = { UINT64_C(1) };
static struct _mint_t mfloat_gamma_coeff_0_num_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_gamma_coeff_0_num_storage };
static uint64_t mfloat_gamma_coeff_0_den_storage[] = { UINT64_C(12) };
static struct _mint_t mfloat_gamma_coeff_0_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_gamma_coeff_0_den_storage };
static uint64_t mfloat_gamma_coeff_1_num_storage[] = { UINT64_C(1) };
static struct _mint_t mfloat_gamma_coeff_1_num_static = { .sign = -1, .length = 1u, .capacity = 1u, .storage = mfloat_gamma_coeff_1_num_storage };
static uint64_t mfloat_gamma_coeff_1_den_storage[] = { UINT64_C(120) };
static struct _mint_t mfloat_gamma_coeff_1_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_gamma_coeff_1_den_storage };
static uint64_t mfloat_gamma_coeff_2_num_storage[] = { UINT64_C(1) };
static struct _mint_t mfloat_gamma_coeff_2_num_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_gamma_coeff_2_num_storage };
static uint64_t mfloat_gamma_coeff_2_den_storage[] = { UINT64_C(252) };
static struct _mint_t mfloat_gamma_coeff_2_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_gamma_coeff_2_den_storage };
static uint64_t mfloat_gamma_coeff_3_num_storage[] = { UINT64_C(1) };
static struct _mint_t mfloat_gamma_coeff_3_num_static = { .sign = -1, .length = 1u, .capacity = 1u, .storage = mfloat_gamma_coeff_3_num_storage };
static uint64_t mfloat_gamma_coeff_3_den_storage[] = { UINT64_C(240) };
static struct _mint_t mfloat_gamma_coeff_3_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_gamma_coeff_3_den_storage };
static uint64_t mfloat_gamma_coeff_4_num_storage[] = { UINT64_C(1) };
static struct _mint_t mfloat_gamma_coeff_4_num_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_gamma_coeff_4_num_storage };
static uint64_t mfloat_gamma_coeff_4_den_storage[] = { UINT64_C(132) };
static struct _mint_t mfloat_gamma_coeff_4_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_gamma_coeff_4_den_storage };
static uint64_t mfloat_gamma_coeff_5_num_storage[] = { UINT64_C(691) };
static struct _mint_t mfloat_gamma_coeff_5_num_static = { .sign = -1, .length = 1u, .capacity = 1u, .storage = mfloat_gamma_coeff_5_num_storage };
static uint64_t mfloat_gamma_coeff_5_den_storage[] = { UINT64_C(32760) };
static struct _mint_t mfloat_gamma_coeff_5_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_gamma_coeff_5_den_storage };
static uint64_t mfloat_gamma_coeff_6_num_storage[] = { UINT64_C(1) };
static struct _mint_t mfloat_gamma_coeff_6_num_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_gamma_coeff_6_num_storage };
static uint64_t mfloat_gamma_coeff_6_den_storage[] = { UINT64_C(12) };
static struct _mint_t mfloat_gamma_coeff_6_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_gamma_coeff_6_den_storage };
static uint64_t mfloat_gamma_coeff_7_num_storage[] = { UINT64_C(3617) };
static struct _mint_t mfloat_gamma_coeff_7_num_static = { .sign = -1, .length = 1u, .capacity = 1u, .storage = mfloat_gamma_coeff_7_num_storage };
static uint64_t mfloat_gamma_coeff_7_den_storage[] = { UINT64_C(8160) };
static struct _mint_t mfloat_gamma_coeff_7_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_gamma_coeff_7_den_storage };
static uint64_t mfloat_gamma_coeff_8_num_storage[] = { UINT64_C(43867) };
static struct _mint_t mfloat_gamma_coeff_8_num_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_gamma_coeff_8_num_storage };
static uint64_t mfloat_gamma_coeff_8_den_storage[] = { UINT64_C(14364) };
static struct _mint_t mfloat_gamma_coeff_8_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_gamma_coeff_8_den_storage };
static uint64_t mfloat_gamma_coeff_9_num_storage[] = { UINT64_C(174611) };
static struct _mint_t mfloat_gamma_coeff_9_num_static = { .sign = -1, .length = 1u, .capacity = 1u, .storage = mfloat_gamma_coeff_9_num_storage };
static uint64_t mfloat_gamma_coeff_9_den_storage[] = { UINT64_C(6600) };
static struct _mint_t mfloat_gamma_coeff_9_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_gamma_coeff_9_den_storage };
static uint64_t mfloat_gamma_coeff_10_num_storage[] = { UINT64_C(854513) };
static struct _mint_t mfloat_gamma_coeff_10_num_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_gamma_coeff_10_num_storage };
static uint64_t mfloat_gamma_coeff_10_den_storage[] = { UINT64_C(3036) };
static struct _mint_t mfloat_gamma_coeff_10_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_gamma_coeff_10_den_storage };
static uint64_t mfloat_gamma_coeff_11_num_storage[] = { UINT64_C(236364091) };
static struct _mint_t mfloat_gamma_coeff_11_num_static = { .sign = -1, .length = 1u, .capacity = 1u, .storage = mfloat_gamma_coeff_11_num_storage };
static uint64_t mfloat_gamma_coeff_11_den_storage[] = { UINT64_C(65520) };
static struct _mint_t mfloat_gamma_coeff_11_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_gamma_coeff_11_den_storage };
static uint64_t mfloat_gamma_coeff_12_num_storage[] = { UINT64_C(8553103) };
static struct _mint_t mfloat_gamma_coeff_12_num_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_gamma_coeff_12_num_storage };
static uint64_t mfloat_gamma_coeff_12_den_storage[] = { UINT64_C(156) };
static struct _mint_t mfloat_gamma_coeff_12_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_gamma_coeff_12_den_storage };
static uint64_t mfloat_gamma_coeff_13_num_storage[] = { UINT64_C(23749461029) };
static struct _mint_t mfloat_gamma_coeff_13_num_static = { .sign = -1, .length = 1u, .capacity = 1u, .storage = mfloat_gamma_coeff_13_num_storage };
static uint64_t mfloat_gamma_coeff_13_den_storage[] = { UINT64_C(24360) };
static struct _mint_t mfloat_gamma_coeff_13_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_gamma_coeff_13_den_storage };
static uint64_t mfloat_gamma_coeff_14_num_storage[] = { UINT64_C(8615841276005) };
static struct _mint_t mfloat_gamma_coeff_14_num_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_gamma_coeff_14_num_storage };
static uint64_t mfloat_gamma_coeff_14_den_storage[] = { UINT64_C(458304) };
static struct _mint_t mfloat_gamma_coeff_14_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_gamma_coeff_14_den_storage };
static uint64_t mfloat_gamma_coeff_15_num_storage[] = { UINT64_C(7709321041217) };
static struct _mint_t mfloat_gamma_coeff_15_num_static = { .sign = -1, .length = 1u, .capacity = 1u, .storage = mfloat_gamma_coeff_15_num_storage };
static uint64_t mfloat_gamma_coeff_15_den_storage[] = { UINT64_C(16320) };
static struct _mint_t mfloat_gamma_coeff_15_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_gamma_coeff_15_den_storage };

static const mfloat_gamma_coeff_seed_t mfloat_euler_gamma_coeffs[] = {
    { &mfloat_gamma_coeff_0_num_static, &mfloat_gamma_coeff_0_den_static, 2u },
    { &mfloat_gamma_coeff_1_num_static, &mfloat_gamma_coeff_1_den_static, 4u },
    { &mfloat_gamma_coeff_2_num_static, &mfloat_gamma_coeff_2_den_static, 6u },
    { &mfloat_gamma_coeff_3_num_static, &mfloat_gamma_coeff_3_den_static, 8u },
    { &mfloat_gamma_coeff_4_num_static, &mfloat_gamma_coeff_4_den_static, 10u },
    { &mfloat_gamma_coeff_5_num_static, &mfloat_gamma_coeff_5_den_static, 12u },
    { &mfloat_gamma_coeff_6_num_static, &mfloat_gamma_coeff_6_den_static, 14u },
    { &mfloat_gamma_coeff_7_num_static, &mfloat_gamma_coeff_7_den_static, 16u },
    { &mfloat_gamma_coeff_8_num_static, &mfloat_gamma_coeff_8_den_static, 18u },
    { &mfloat_gamma_coeff_9_num_static, &mfloat_gamma_coeff_9_den_static, 20u },
    { &mfloat_gamma_coeff_10_num_static, &mfloat_gamma_coeff_10_den_static, 22u },
    { &mfloat_gamma_coeff_11_num_static, &mfloat_gamma_coeff_11_den_static, 24u },
    { &mfloat_gamma_coeff_12_num_static, &mfloat_gamma_coeff_12_den_static, 26u },
    { &mfloat_gamma_coeff_13_num_static, &mfloat_gamma_coeff_13_den_static, 28u },
    { &mfloat_gamma_coeff_14_num_static, &mfloat_gamma_coeff_14_den_static, 30u },
    { &mfloat_gamma_coeff_15_num_static, &mfloat_gamma_coeff_15_den_static, 32u }
};

#endif
