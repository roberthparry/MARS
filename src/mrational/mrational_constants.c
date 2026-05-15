#include "internal/mint_internal.h"
#include "mrational_internal.h"

/* Small exact numerator and denominator limbs for immortal rationals. */
static uint64_t mr_half_num_storage[] = { 1u };
static uint64_t mr_half_den_storage[] = { 2u };
static uint64_t mr_three_num_storage[] = { 3u };
static uint64_t mr_third_den_storage[] = { 3u };
static uint64_t mr_quarter_den_storage[] = { 4u };
static uint64_t mr_sixth_den_storage[] = { 6u };
static uint64_t mr_eighth_den_storage[] = { 8u };
static uint64_t mr_tenth_den_storage[] = { 10u };

static mint_t mr_half_num_mint = {
    .sign = 1,
    .length = 1,
    .capacity = 1,
    .storage = mr_half_num_storage,
    .scope_owned_container = false
};
static mint_t mr_half_den_mint = {
    .sign = 1,
    .length = 1,
    .capacity = 1,
    .storage = mr_half_den_storage,
    .scope_owned_container = false
};
static mint_t mr_three_num_mint = {
    .sign = 1,
    .length = 1,
    .capacity = 1,
    .storage = mr_three_num_storage,
    .scope_owned_container = false
};
static mint_t mr_third_den_mint = {
    .sign = 1,
    .length = 1,
    .capacity = 1,
    .storage = mr_third_den_storage,
    .scope_owned_container = false
};
static mint_t mr_quarter_den_mint = {
    .sign = 1,
    .length = 1,
    .capacity = 1,
    .storage = mr_quarter_den_storage,
    .scope_owned_container = false
};
static mint_t mr_sixth_den_mint = {
    .sign = 1,
    .length = 1,
    .capacity = 1,
    .storage = mr_sixth_den_storage,
    .scope_owned_container = false
};
static mint_t mr_eighth_den_mint = {
    .sign = 1,
    .length = 1,
    .capacity = 1,
    .storage = mr_eighth_den_storage,
    .scope_owned_container = false
};
static mint_t mr_tenth_den_mint = {
    .sign = 1,
    .length = 1,
    .capacity = 1,
    .storage = mr_tenth_den_storage,
    .scope_owned_container = false
};

/* Canonical immortal mrational values. */
const mrational_t MR_HALF_VALUE = {
    .numerator = &mr_half_num_mint,
    .denominator = &mr_half_den_mint,
    .immortal = true
};
const mrational_t MR_ONE_AND_HALF_VALUE = {
    .numerator = &mr_three_num_mint,
    .denominator = &mr_half_den_mint,
    .immortal = true
};
const mrational_t MR_ONE_THIRD_VALUE = {
    .numerator = &mr_half_num_mint,
    .denominator = &mr_third_den_mint,
    .immortal = true
};
const mrational_t MR_QUARTER_VALUE = {
    .numerator = &mr_half_num_mint,
    .denominator = &mr_quarter_den_mint,
    .immortal = true
};
const mrational_t MR_ONE_SIXTH_VALUE = {
    .numerator = &mr_half_num_mint,
    .denominator = &mr_sixth_den_mint,
    .immortal = true
};
const mrational_t MR_ONE_EIGHTH_VALUE = {
    .numerator = &mr_half_num_mint,
    .denominator = &mr_eighth_den_mint,
    .immortal = true
};
const mrational_t MR_ONE_TENTH_VALUE = {
    .numerator = &mr_half_num_mint,
    .denominator = &mr_tenth_den_mint,
    .immortal = true
};

/* Exported canonical handles. */
const mrational_t * const MR_HALF = &MR_HALF_VALUE;
const mrational_t * const MR_ONE_AND_HALF = &MR_ONE_AND_HALF_VALUE;
const mrational_t * const MR_ONE_THIRD = &MR_ONE_THIRD_VALUE;
const mrational_t * const MR_QUARTER = &MR_QUARTER_VALUE;
const mrational_t * const MR_ONE_SIXTH = &MR_ONE_SIXTH_VALUE;
const mrational_t * const MR_ONE_EIGHTH = &MR_ONE_EIGHTH_VALUE;
const mrational_t * const MR_ONE_TENTH = &MR_ONE_TENTH_VALUE;
