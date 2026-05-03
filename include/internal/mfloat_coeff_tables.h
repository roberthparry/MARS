#ifndef MFLOAT_COEFF_TABLES_H
#define MFLOAT_COEFF_TABLES_H

#include <stdint.h>

#include "internal/mint_layout.h"

typedef struct mfloat_seed_rational_pair_t {
    const mint_t *num;
    const mint_t *den;
} mfloat_seed_rational_pair_t;

typedef struct mfloat_gamma_coeff_seed_t {
    const mint_t *num;
    const mint_t *den;
    unsigned power;
} mfloat_gamma_coeff_seed_t;

#define MFLOAT_STATIC_MINT_1(name, sign_value, a0) \
    static uint64_t name##_storage[] = { UINT64_C(a0) }; \
    static struct _mint_t name##_static = { .sign = sign_value, .length = 1u, .capacity = 1u, .storage = name##_storage }

#define MFLOAT_STATIC_MINT_2(name, sign_value, a0, a1) \
    static uint64_t name##_storage[] = { UINT64_C(a0), UINT64_C(a1) }; \
    static struct _mint_t name##_static = { .sign = sign_value, .length = 2u, .capacity = 2u, .storage = name##_storage }

#define MFLOAT_STATIC_MINT_3(name, sign_value, a0, a1, a2) \
    static uint64_t name##_storage[] = { UINT64_C(a0), UINT64_C(a1), UINT64_C(a2) }; \
    static struct _mint_t name##_static = { .sign = sign_value, .length = 3u, .capacity = 3u, .storage = name##_storage }

#define MFLOAT_STATIC_MINT_4(name, sign_value, a0, a1, a2, a3) \
    static uint64_t name##_storage[] = { UINT64_C(a0), UINT64_C(a1), UINT64_C(a2), UINT64_C(a3) }; \
    static struct _mint_t name##_static = { .sign = sign_value, .length = 4u, .capacity = 4u, .storage = name##_storage }

MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_0_num, 1, 1);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_0_den, 1, 12);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_1_num, -1, 1);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_1_den, 1, 360);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_2_num, 1, 1);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_2_den, 1, 1260);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_3_num, -1, 1);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_3_den, 1, 1680);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_4_num, 1, 1);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_4_den, 1, 1188);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_5_num, -1, 691);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_5_den, 1, 360360);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_6_num, 1, 1);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_6_den, 1, 156);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_7_num, -1, 3617);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_7_den, 1, 122400);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_8_num, 1, 43867);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_8_den, 1, 244188);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_9_num, -1, 174611);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_9_den, 1, 125400);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_10_num, 1, 77683);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_10_den, 1, 5796);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_11_num, -1, 236364091);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_11_den, 1, 1506960);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_12_num, 1, 657931);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_12_den, 1, 300);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_13_num, -1, 3392780147);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_13_den, 1, 93960);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_14_num, 1, 1723168255201);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_14_den, 1, 2492028);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_15_num, -1, 7709321041217);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_15_den, 1, 505920);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_16_num, 1, 151628697551);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_16_den, 1, 396);
MFLOAT_STATIC_MINT_2(mfloat_lgamma_term_17_num, -1, 7868527479343925757, 1);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_17_den, 1, 2418179400);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_18_num, 1, 154210205991661);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_18_den, 1, 444);
MFLOAT_STATIC_MINT_2(mfloat_lgamma_term_19_num, -1, 2828301464515399427, 14);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_19_den, 1, 21106800);
MFLOAT_STATIC_MINT_2(mfloat_lgamma_term_20_num, 1, 7464629873887570179, 82);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_20_den, 1, 3109932);
MFLOAT_STATIC_MINT_2(mfloat_lgamma_term_21_num, -1, 3093296383702722701, 137);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_21_den, 1, 118680);
MFLOAT_STATIC_MINT_2(mfloat_lgamma_term_22_num, 1, 14981602260347948127, 1405);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_22_den, 1, 25380);
MFLOAT_STATIC_MINT_2(mfloat_lgamma_term_23_num, -1, 17538188069559811707, 304086365);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_23_den, 1, 104700960);
MFLOAT_STATIC_MINT_2(mfloat_lgamma_term_24_num, 1, 3594421161621548957, 1073484);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_24_den, 1, 6468);
MFLOAT_STATIC_MINT_2(mfloat_lgamma_term_25_num, -1, 17269523281192527073, 3340867738);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_25_den, 1, 324360);
MFLOAT_STATIC_MINT_2(mfloat_lgamma_term_26_num, 1, 14406356592616628051, 1580222695040);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_26_den, 1, 2283876);
MFLOAT_STATIC_MINT_2(mfloat_lgamma_term_27_num, -1, 18338106261423167323, 19201165717189);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_27_den, 1, 382800);
MFLOAT_STATIC_MINT_2(mfloat_lgamma_term_28_num, 1, 9669787522055991289, 157926408848760);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_28_den, 1, 40356);
MFLOAT_STATIC_MINT_3(mfloat_lgamma_term_29_num, -1, 5784832664286938003, 4597462226691178307, 3571);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_29_den, 1, 201025024200);
MFLOAT_STATIC_MINT_2(mfloat_lgamma_term_30_num, 1, 5860676301039822329, 21510195887871812);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_30_den, 1, 732);
MFLOAT_STATIC_MINT_3(mfloat_lgamma_term_31_num, -1, 9156223885415353217, 14932136560910138492, 313);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_31_den, 1, 2056320);
MFLOAT_STATIC_MINT_3(mfloat_lgamma_term_32_num, 1, 628340919981051959, 10929822824017737996, 393416);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_32_den, 1, 25241580);
MFLOAT_STATIC_MINT_3(mfloat_lgamma_term_33_num, -1, 8373108954039868905, 4802449790490059906, 13617);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_33_den, 1, 8040);
MFLOAT_STATIC_MINT_3(mfloat_lgamma_term_34_num, 1, 3796493031256123929, 2139317340953408120, 126397662);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_34_den, 1, 646668);
MFLOAT_STATIC_MINT_3(mfloat_lgamma_term_35_num, -1, 15094907119697487437, 1319937529144214021, 17126820335724351);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_35_den, 1, 716195647440);
MFLOAT_STATIC_MINT_3(mfloat_lgamma_term_36_num, 1, 3557779432746134595, 2498916859548830763, 2712565783);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_36_den, 1, 876);
MFLOAT_STATIC_MINT_3(mfloat_lgamma_term_37_num, -1, 2658486653472445427, 18290510968819779080, 3813410214987);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_37_den, 1, 9000);
MFLOAT_STATIC_MINT_3(mfloat_lgamma_term_38_num, 1, 3499226856120891483, 9880313407122127206, 93778761383277145);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_38_den, 1, 1532916);
MFLOAT_STATIC_MINT_4(mfloat_lgamma_term_39_num, -1, 13186113498967837091, 1008884250640227847, 7842685077010475176, 733);
MFLOAT_STATIC_MINT_1(mfloat_lgamma_term_39_den, 1, 1453663200);

MFLOAT_STATIC_MINT_1(mfloat_gamma_coeff_0_num, 1, 1);
MFLOAT_STATIC_MINT_1(mfloat_gamma_coeff_0_den, 1, 12);
MFLOAT_STATIC_MINT_1(mfloat_gamma_coeff_1_num, -1, 1);
MFLOAT_STATIC_MINT_1(mfloat_gamma_coeff_1_den, 1, 120);
MFLOAT_STATIC_MINT_1(mfloat_gamma_coeff_2_num, 1, 1);
MFLOAT_STATIC_MINT_1(mfloat_gamma_coeff_2_den, 1, 252);
MFLOAT_STATIC_MINT_1(mfloat_gamma_coeff_3_num, -1, 1);
MFLOAT_STATIC_MINT_1(mfloat_gamma_coeff_3_den, 1, 240);
MFLOAT_STATIC_MINT_1(mfloat_gamma_coeff_4_num, 1, 1);
MFLOAT_STATIC_MINT_1(mfloat_gamma_coeff_4_den, 1, 132);
MFLOAT_STATIC_MINT_1(mfloat_gamma_coeff_5_num, -1, 691);
MFLOAT_STATIC_MINT_1(mfloat_gamma_coeff_5_den, 1, 32760);
MFLOAT_STATIC_MINT_1(mfloat_gamma_coeff_6_num, 1, 1);
MFLOAT_STATIC_MINT_1(mfloat_gamma_coeff_6_den, 1, 12);
MFLOAT_STATIC_MINT_1(mfloat_gamma_coeff_7_num, -1, 3617);
MFLOAT_STATIC_MINT_1(mfloat_gamma_coeff_7_den, 1, 8160);
MFLOAT_STATIC_MINT_1(mfloat_gamma_coeff_8_num, 1, 43867);
MFLOAT_STATIC_MINT_1(mfloat_gamma_coeff_8_den, 1, 14364);
MFLOAT_STATIC_MINT_1(mfloat_gamma_coeff_9_num, -1, 174611);
MFLOAT_STATIC_MINT_1(mfloat_gamma_coeff_9_den, 1, 6600);
MFLOAT_STATIC_MINT_1(mfloat_gamma_coeff_10_num, 1, 854513);
MFLOAT_STATIC_MINT_1(mfloat_gamma_coeff_10_den, 1, 3036);
MFLOAT_STATIC_MINT_1(mfloat_gamma_coeff_11_num, -1, 236364091);
MFLOAT_STATIC_MINT_1(mfloat_gamma_coeff_11_den, 1, 65520);
MFLOAT_STATIC_MINT_1(mfloat_gamma_coeff_12_num, 1, 8553103);
MFLOAT_STATIC_MINT_1(mfloat_gamma_coeff_12_den, 1, 156);
MFLOAT_STATIC_MINT_1(mfloat_gamma_coeff_13_num, -1, 23749461029);
MFLOAT_STATIC_MINT_1(mfloat_gamma_coeff_13_den, 1, 24360);
MFLOAT_STATIC_MINT_1(mfloat_gamma_coeff_14_num, 1, 8615841276005);
MFLOAT_STATIC_MINT_1(mfloat_gamma_coeff_14_den, 1, 458304);
MFLOAT_STATIC_MINT_1(mfloat_gamma_coeff_15_num, -1, 7709321041217);
MFLOAT_STATIC_MINT_1(mfloat_gamma_coeff_15_den, 1, 16320);

static const mfloat_seed_rational_pair_t mfloat_lgamma_asymptotic_terms[] = {
    { &mfloat_lgamma_term_0_num_static, &mfloat_lgamma_term_0_den_static },
    { &mfloat_lgamma_term_1_num_static, &mfloat_lgamma_term_1_den_static },
    { &mfloat_lgamma_term_2_num_static, &mfloat_lgamma_term_2_den_static },
    { &mfloat_lgamma_term_3_num_static, &mfloat_lgamma_term_3_den_static },
    { &mfloat_lgamma_term_4_num_static, &mfloat_lgamma_term_4_den_static },
    { &mfloat_lgamma_term_5_num_static, &mfloat_lgamma_term_5_den_static },
    { &mfloat_lgamma_term_6_num_static, &mfloat_lgamma_term_6_den_static },
    { &mfloat_lgamma_term_7_num_static, &mfloat_lgamma_term_7_den_static },
    { &mfloat_lgamma_term_8_num_static, &mfloat_lgamma_term_8_den_static },
    { &mfloat_lgamma_term_9_num_static, &mfloat_lgamma_term_9_den_static },
    { &mfloat_lgamma_term_10_num_static, &mfloat_lgamma_term_10_den_static },
    { &mfloat_lgamma_term_11_num_static, &mfloat_lgamma_term_11_den_static },
    { &mfloat_lgamma_term_12_num_static, &mfloat_lgamma_term_12_den_static },
    { &mfloat_lgamma_term_13_num_static, &mfloat_lgamma_term_13_den_static },
    { &mfloat_lgamma_term_14_num_static, &mfloat_lgamma_term_14_den_static },
    { &mfloat_lgamma_term_15_num_static, &mfloat_lgamma_term_15_den_static },
    { &mfloat_lgamma_term_16_num_static, &mfloat_lgamma_term_16_den_static },
    { &mfloat_lgamma_term_17_num_static, &mfloat_lgamma_term_17_den_static },
    { &mfloat_lgamma_term_18_num_static, &mfloat_lgamma_term_18_den_static },
    { &mfloat_lgamma_term_19_num_static, &mfloat_lgamma_term_19_den_static },
    { &mfloat_lgamma_term_20_num_static, &mfloat_lgamma_term_20_den_static },
    { &mfloat_lgamma_term_21_num_static, &mfloat_lgamma_term_21_den_static },
    { &mfloat_lgamma_term_22_num_static, &mfloat_lgamma_term_22_den_static },
    { &mfloat_lgamma_term_23_num_static, &mfloat_lgamma_term_23_den_static },
    { &mfloat_lgamma_term_24_num_static, &mfloat_lgamma_term_24_den_static },
    { &mfloat_lgamma_term_25_num_static, &mfloat_lgamma_term_25_den_static },
    { &mfloat_lgamma_term_26_num_static, &mfloat_lgamma_term_26_den_static },
    { &mfloat_lgamma_term_27_num_static, &mfloat_lgamma_term_27_den_static },
    { &mfloat_lgamma_term_28_num_static, &mfloat_lgamma_term_28_den_static },
    { &mfloat_lgamma_term_29_num_static, &mfloat_lgamma_term_29_den_static },
    { &mfloat_lgamma_term_30_num_static, &mfloat_lgamma_term_30_den_static },
    { &mfloat_lgamma_term_31_num_static, &mfloat_lgamma_term_31_den_static },
    { &mfloat_lgamma_term_32_num_static, &mfloat_lgamma_term_32_den_static },
    { &mfloat_lgamma_term_33_num_static, &mfloat_lgamma_term_33_den_static },
    { &mfloat_lgamma_term_34_num_static, &mfloat_lgamma_term_34_den_static },
    { &mfloat_lgamma_term_35_num_static, &mfloat_lgamma_term_35_den_static },
    { &mfloat_lgamma_term_36_num_static, &mfloat_lgamma_term_36_den_static },
    { &mfloat_lgamma_term_37_num_static, &mfloat_lgamma_term_37_den_static },
    { &mfloat_lgamma_term_38_num_static, &mfloat_lgamma_term_38_den_static },
    { &mfloat_lgamma_term_39_num_static, &mfloat_lgamma_term_39_den_static }
};

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

#undef MFLOAT_STATIC_MINT_1
#undef MFLOAT_STATIC_MINT_2
#undef MFLOAT_STATIC_MINT_3
#undef MFLOAT_STATIC_MINT_4

#endif
