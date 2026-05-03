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

static uint64_t mfloat_lgamma_term_0_num_storage[] = { UINT64_C(1) };
static struct _mint_t mfloat_lgamma_term_0_num_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_0_num_storage };
static uint64_t mfloat_lgamma_term_0_den_storage[] = { UINT64_C(12) };
static struct _mint_t mfloat_lgamma_term_0_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_0_den_storage };
static uint64_t mfloat_lgamma_term_1_num_storage[] = { UINT64_C(1) };
static struct _mint_t mfloat_lgamma_term_1_num_static = { .sign = -1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_1_num_storage };
static uint64_t mfloat_lgamma_term_1_den_storage[] = { UINT64_C(360) };
static struct _mint_t mfloat_lgamma_term_1_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_1_den_storage };
static uint64_t mfloat_lgamma_term_2_num_storage[] = { UINT64_C(1) };
static struct _mint_t mfloat_lgamma_term_2_num_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_2_num_storage };
static uint64_t mfloat_lgamma_term_2_den_storage[] = { UINT64_C(1260) };
static struct _mint_t mfloat_lgamma_term_2_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_2_den_storage };
static uint64_t mfloat_lgamma_term_3_num_storage[] = { UINT64_C(1) };
static struct _mint_t mfloat_lgamma_term_3_num_static = { .sign = -1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_3_num_storage };
static uint64_t mfloat_lgamma_term_3_den_storage[] = { UINT64_C(1680) };
static struct _mint_t mfloat_lgamma_term_3_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_3_den_storage };
static uint64_t mfloat_lgamma_term_4_num_storage[] = { UINT64_C(1) };
static struct _mint_t mfloat_lgamma_term_4_num_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_4_num_storage };
static uint64_t mfloat_lgamma_term_4_den_storage[] = { UINT64_C(1188) };
static struct _mint_t mfloat_lgamma_term_4_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_4_den_storage };
static uint64_t mfloat_lgamma_term_5_num_storage[] = { UINT64_C(691) };
static struct _mint_t mfloat_lgamma_term_5_num_static = { .sign = -1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_5_num_storage };
static uint64_t mfloat_lgamma_term_5_den_storage[] = { UINT64_C(360360) };
static struct _mint_t mfloat_lgamma_term_5_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_5_den_storage };
static uint64_t mfloat_lgamma_term_6_num_storage[] = { UINT64_C(1) };
static struct _mint_t mfloat_lgamma_term_6_num_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_6_num_storage };
static uint64_t mfloat_lgamma_term_6_den_storage[] = { UINT64_C(156) };
static struct _mint_t mfloat_lgamma_term_6_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_6_den_storage };
static uint64_t mfloat_lgamma_term_7_num_storage[] = { UINT64_C(3617) };
static struct _mint_t mfloat_lgamma_term_7_num_static = { .sign = -1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_7_num_storage };
static uint64_t mfloat_lgamma_term_7_den_storage[] = { UINT64_C(122400) };
static struct _mint_t mfloat_lgamma_term_7_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_7_den_storage };
static uint64_t mfloat_lgamma_term_8_num_storage[] = { UINT64_C(43867) };
static struct _mint_t mfloat_lgamma_term_8_num_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_8_num_storage };
static uint64_t mfloat_lgamma_term_8_den_storage[] = { UINT64_C(244188) };
static struct _mint_t mfloat_lgamma_term_8_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_8_den_storage };
static uint64_t mfloat_lgamma_term_9_num_storage[] = { UINT64_C(174611) };
static struct _mint_t mfloat_lgamma_term_9_num_static = { .sign = -1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_9_num_storage };
static uint64_t mfloat_lgamma_term_9_den_storage[] = { UINT64_C(125400) };
static struct _mint_t mfloat_lgamma_term_9_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_9_den_storage };
static uint64_t mfloat_lgamma_term_10_num_storage[] = { UINT64_C(77683) };
static struct _mint_t mfloat_lgamma_term_10_num_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_10_num_storage };
static uint64_t mfloat_lgamma_term_10_den_storage[] = { UINT64_C(5796) };
static struct _mint_t mfloat_lgamma_term_10_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_10_den_storage };
static uint64_t mfloat_lgamma_term_11_num_storage[] = { UINT64_C(236364091) };
static struct _mint_t mfloat_lgamma_term_11_num_static = { .sign = -1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_11_num_storage };
static uint64_t mfloat_lgamma_term_11_den_storage[] = { UINT64_C(1506960) };
static struct _mint_t mfloat_lgamma_term_11_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_11_den_storage };
static uint64_t mfloat_lgamma_term_12_num_storage[] = { UINT64_C(657931) };
static struct _mint_t mfloat_lgamma_term_12_num_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_12_num_storage };
static uint64_t mfloat_lgamma_term_12_den_storage[] = { UINT64_C(300) };
static struct _mint_t mfloat_lgamma_term_12_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_12_den_storage };
static uint64_t mfloat_lgamma_term_13_num_storage[] = { UINT64_C(3392780147) };
static struct _mint_t mfloat_lgamma_term_13_num_static = { .sign = -1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_13_num_storage };
static uint64_t mfloat_lgamma_term_13_den_storage[] = { UINT64_C(93960) };
static struct _mint_t mfloat_lgamma_term_13_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_13_den_storage };
static uint64_t mfloat_lgamma_term_14_num_storage[] = { UINT64_C(1723168255201) };
static struct _mint_t mfloat_lgamma_term_14_num_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_14_num_storage };
static uint64_t mfloat_lgamma_term_14_den_storage[] = { UINT64_C(2492028) };
static struct _mint_t mfloat_lgamma_term_14_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_14_den_storage };
static uint64_t mfloat_lgamma_term_15_num_storage[] = { UINT64_C(7709321041217) };
static struct _mint_t mfloat_lgamma_term_15_num_static = { .sign = -1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_15_num_storage };
static uint64_t mfloat_lgamma_term_15_den_storage[] = { UINT64_C(505920) };
static struct _mint_t mfloat_lgamma_term_15_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_15_den_storage };
static uint64_t mfloat_lgamma_term_16_num_storage[] = { UINT64_C(151628697551) };
static struct _mint_t mfloat_lgamma_term_16_num_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_16_num_storage };
static uint64_t mfloat_lgamma_term_16_den_storage[] = { UINT64_C(396) };
static struct _mint_t mfloat_lgamma_term_16_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_16_den_storage };
static uint64_t mfloat_lgamma_term_17_num_storage[] = { UINT64_C(7868527479343925757), UINT64_C(1) };
static struct _mint_t mfloat_lgamma_term_17_num_static = { .sign = -1, .length = 2u, .capacity = 2u, .storage = mfloat_lgamma_term_17_num_storage };
static uint64_t mfloat_lgamma_term_17_den_storage[] = { UINT64_C(2418179400) };
static struct _mint_t mfloat_lgamma_term_17_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_17_den_storage };
static uint64_t mfloat_lgamma_term_18_num_storage[] = { UINT64_C(154210205991661) };
static struct _mint_t mfloat_lgamma_term_18_num_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_18_num_storage };
static uint64_t mfloat_lgamma_term_18_den_storage[] = { UINT64_C(444) };
static struct _mint_t mfloat_lgamma_term_18_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_18_den_storage };
static uint64_t mfloat_lgamma_term_19_num_storage[] = { UINT64_C(2828301464515399427), UINT64_C(14) };
static struct _mint_t mfloat_lgamma_term_19_num_static = { .sign = -1, .length = 2u, .capacity = 2u, .storage = mfloat_lgamma_term_19_num_storage };
static uint64_t mfloat_lgamma_term_19_den_storage[] = { UINT64_C(21106800) };
static struct _mint_t mfloat_lgamma_term_19_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_19_den_storage };
static uint64_t mfloat_lgamma_term_20_num_storage[] = { UINT64_C(7464629873887570179), UINT64_C(82) };
static struct _mint_t mfloat_lgamma_term_20_num_static = { .sign = 1, .length = 2u, .capacity = 2u, .storage = mfloat_lgamma_term_20_num_storage };
static uint64_t mfloat_lgamma_term_20_den_storage[] = { UINT64_C(3109932) };
static struct _mint_t mfloat_lgamma_term_20_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_20_den_storage };
static uint64_t mfloat_lgamma_term_21_num_storage[] = { UINT64_C(3093296383702722701), UINT64_C(137) };
static struct _mint_t mfloat_lgamma_term_21_num_static = { .sign = -1, .length = 2u, .capacity = 2u, .storage = mfloat_lgamma_term_21_num_storage };
static uint64_t mfloat_lgamma_term_21_den_storage[] = { UINT64_C(118680) };
static struct _mint_t mfloat_lgamma_term_21_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_21_den_storage };
static uint64_t mfloat_lgamma_term_22_num_storage[] = { UINT64_C(14981602260347948127), UINT64_C(1405) };
static struct _mint_t mfloat_lgamma_term_22_num_static = { .sign = 1, .length = 2u, .capacity = 2u, .storage = mfloat_lgamma_term_22_num_storage };
static uint64_t mfloat_lgamma_term_22_den_storage[] = { UINT64_C(25380) };
static struct _mint_t mfloat_lgamma_term_22_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_22_den_storage };
static uint64_t mfloat_lgamma_term_23_num_storage[] = { UINT64_C(17538188069559811707), UINT64_C(304086365) };
static struct _mint_t mfloat_lgamma_term_23_num_static = { .sign = -1, .length = 2u, .capacity = 2u, .storage = mfloat_lgamma_term_23_num_storage };
static uint64_t mfloat_lgamma_term_23_den_storage[] = { UINT64_C(104700960) };
static struct _mint_t mfloat_lgamma_term_23_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_23_den_storage };
static uint64_t mfloat_lgamma_term_24_num_storage[] = { UINT64_C(3594421161621548957), UINT64_C(1073484) };
static struct _mint_t mfloat_lgamma_term_24_num_static = { .sign = 1, .length = 2u, .capacity = 2u, .storage = mfloat_lgamma_term_24_num_storage };
static uint64_t mfloat_lgamma_term_24_den_storage[] = { UINT64_C(6468) };
static struct _mint_t mfloat_lgamma_term_24_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_24_den_storage };
static uint64_t mfloat_lgamma_term_25_num_storage[] = { UINT64_C(17269523281192527073), UINT64_C(3340867738) };
static struct _mint_t mfloat_lgamma_term_25_num_static = { .sign = -1, .length = 2u, .capacity = 2u, .storage = mfloat_lgamma_term_25_num_storage };
static uint64_t mfloat_lgamma_term_25_den_storage[] = { UINT64_C(324360) };
static struct _mint_t mfloat_lgamma_term_25_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_25_den_storage };
static uint64_t mfloat_lgamma_term_26_num_storage[] = { UINT64_C(14406356592616628051), UINT64_C(1580222695040) };
static struct _mint_t mfloat_lgamma_term_26_num_static = { .sign = 1, .length = 2u, .capacity = 2u, .storage = mfloat_lgamma_term_26_num_storage };
static uint64_t mfloat_lgamma_term_26_den_storage[] = { UINT64_C(2283876) };
static struct _mint_t mfloat_lgamma_term_26_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_26_den_storage };
static uint64_t mfloat_lgamma_term_27_num_storage[] = { UINT64_C(18338106261423167323), UINT64_C(19201165717189) };
static struct _mint_t mfloat_lgamma_term_27_num_static = { .sign = -1, .length = 2u, .capacity = 2u, .storage = mfloat_lgamma_term_27_num_storage };
static uint64_t mfloat_lgamma_term_27_den_storage[] = { UINT64_C(382800) };
static struct _mint_t mfloat_lgamma_term_27_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_27_den_storage };
static uint64_t mfloat_lgamma_term_28_num_storage[] = { UINT64_C(9669787522055991289), UINT64_C(157926408848760) };
static struct _mint_t mfloat_lgamma_term_28_num_static = { .sign = 1, .length = 2u, .capacity = 2u, .storage = mfloat_lgamma_term_28_num_storage };
static uint64_t mfloat_lgamma_term_28_den_storage[] = { UINT64_C(40356) };
static struct _mint_t mfloat_lgamma_term_28_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_28_den_storage };
static uint64_t mfloat_lgamma_term_29_num_storage[] = { UINT64_C(5784832664286938003), UINT64_C(4597462226691178307), UINT64_C(3571) };
static struct _mint_t mfloat_lgamma_term_29_num_static = { .sign = -1, .length = 3u, .capacity = 3u, .storage = mfloat_lgamma_term_29_num_storage };
static uint64_t mfloat_lgamma_term_29_den_storage[] = { UINT64_C(201025024200) };
static struct _mint_t mfloat_lgamma_term_29_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_29_den_storage };
static uint64_t mfloat_lgamma_term_30_num_storage[] = { UINT64_C(5860676301039822329), UINT64_C(21510195887871812) };
static struct _mint_t mfloat_lgamma_term_30_num_static = { .sign = 1, .length = 2u, .capacity = 2u, .storage = mfloat_lgamma_term_30_num_storage };
static uint64_t mfloat_lgamma_term_30_den_storage[] = { UINT64_C(732) };
static struct _mint_t mfloat_lgamma_term_30_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_30_den_storage };
static uint64_t mfloat_lgamma_term_31_num_storage[] = { UINT64_C(9156223885415353217), UINT64_C(14932136560910138492), UINT64_C(313) };
static struct _mint_t mfloat_lgamma_term_31_num_static = { .sign = -1, .length = 3u, .capacity = 3u, .storage = mfloat_lgamma_term_31_num_storage };
static uint64_t mfloat_lgamma_term_31_den_storage[] = { UINT64_C(2056320) };
static struct _mint_t mfloat_lgamma_term_31_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_31_den_storage };
static uint64_t mfloat_lgamma_term_32_num_storage[] = { UINT64_C(628340919981051959), UINT64_C(10929822824017737996), UINT64_C(393416) };
static struct _mint_t mfloat_lgamma_term_32_num_static = { .sign = 1, .length = 3u, .capacity = 3u, .storage = mfloat_lgamma_term_32_num_storage };
static uint64_t mfloat_lgamma_term_32_den_storage[] = { UINT64_C(25241580) };
static struct _mint_t mfloat_lgamma_term_32_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_32_den_storage };
static uint64_t mfloat_lgamma_term_33_num_storage[] = { UINT64_C(8373108954039868905), UINT64_C(4802449790490059906), UINT64_C(13617) };
static struct _mint_t mfloat_lgamma_term_33_num_static = { .sign = -1, .length = 3u, .capacity = 3u, .storage = mfloat_lgamma_term_33_num_storage };
static uint64_t mfloat_lgamma_term_33_den_storage[] = { UINT64_C(8040) };
static struct _mint_t mfloat_lgamma_term_33_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_33_den_storage };
static uint64_t mfloat_lgamma_term_34_num_storage[] = { UINT64_C(3796493031256123929), UINT64_C(2139317340953408120), UINT64_C(126397662) };
static struct _mint_t mfloat_lgamma_term_34_num_static = { .sign = 1, .length = 3u, .capacity = 3u, .storage = mfloat_lgamma_term_34_num_storage };
static uint64_t mfloat_lgamma_term_34_den_storage[] = { UINT64_C(646668) };
static struct _mint_t mfloat_lgamma_term_34_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_34_den_storage };
static uint64_t mfloat_lgamma_term_35_num_storage[] = { UINT64_C(15094907119697487437), UINT64_C(1319937529144214021), UINT64_C(17126820335724351) };
static struct _mint_t mfloat_lgamma_term_35_num_static = { .sign = -1, .length = 3u, .capacity = 3u, .storage = mfloat_lgamma_term_35_num_storage };
static uint64_t mfloat_lgamma_term_35_den_storage[] = { UINT64_C(716195647440) };
static struct _mint_t mfloat_lgamma_term_35_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_35_den_storage };
static uint64_t mfloat_lgamma_term_36_num_storage[] = { UINT64_C(3557779432746134595), UINT64_C(2498916859548830763), UINT64_C(2712565783) };
static struct _mint_t mfloat_lgamma_term_36_num_static = { .sign = 1, .length = 3u, .capacity = 3u, .storage = mfloat_lgamma_term_36_num_storage };
static uint64_t mfloat_lgamma_term_36_den_storage[] = { UINT64_C(876) };
static struct _mint_t mfloat_lgamma_term_36_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_36_den_storage };
static uint64_t mfloat_lgamma_term_37_num_storage[] = { UINT64_C(2658486653472445427), UINT64_C(18290510968819779080), UINT64_C(3813410214987) };
static struct _mint_t mfloat_lgamma_term_37_num_static = { .sign = -1, .length = 3u, .capacity = 3u, .storage = mfloat_lgamma_term_37_num_storage };
static uint64_t mfloat_lgamma_term_37_den_storage[] = { UINT64_C(9000) };
static struct _mint_t mfloat_lgamma_term_37_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_37_den_storage };
static uint64_t mfloat_lgamma_term_38_num_storage[] = { UINT64_C(3499226856120891483), UINT64_C(9880313407122127206), UINT64_C(93778761383277145) };
static struct _mint_t mfloat_lgamma_term_38_num_static = { .sign = 1, .length = 3u, .capacity = 3u, .storage = mfloat_lgamma_term_38_num_storage };
static uint64_t mfloat_lgamma_term_38_den_storage[] = { UINT64_C(1532916) };
static struct _mint_t mfloat_lgamma_term_38_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_38_den_storage };
static uint64_t mfloat_lgamma_term_39_num_storage[] = { UINT64_C(13186113498967837091), UINT64_C(1008884250640227847), UINT64_C(7842685077010475176), UINT64_C(733) };
static struct _mint_t mfloat_lgamma_term_39_num_static = { .sign = -1, .length = 4u, .capacity = 4u, .storage = mfloat_lgamma_term_39_num_storage };
static uint64_t mfloat_lgamma_term_39_den_storage[] = { UINT64_C(1453663200) };
static struct _mint_t mfloat_lgamma_term_39_den_static = { .sign = 1, .length = 1u, .capacity = 1u, .storage = mfloat_lgamma_term_39_den_storage };

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

#endif
