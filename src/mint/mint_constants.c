#include "mint_internal.h"

/* Small exact immortal mantissas. */
static uint64_t mnt_one_storage[] = { 1u };
static uint64_t mnt_two_storage[] = { 2u };
static uint64_t mnt_ten_storage[] = { 10u };

/* Canonical immortal mint values. */
const mint_t MI_ZERO_VALUE = {
    .sign = 0,
    .length = 0,
    .capacity = 0,
    .storage = NULL,
    .scope_owned_container = false
};

const mint_t MI_ONE_VALUE = {
    .sign = 1,
    .length = 1,
    .capacity = 1,
    .storage = mnt_one_storage,
    .scope_owned_container = false
};

const mint_t MI_NEG_ONE_VALUE = {
    .sign = -1,
    .length = 1,
    .capacity = 1,
    .storage = mnt_one_storage,
    .scope_owned_container = false
};

const mint_t MI_TWO_VALUE = {
    .sign = 1,
    .length = 1,
    .capacity = 1,
    .storage = mnt_two_storage,
    .scope_owned_container = false
};

const mint_t MI_TEN_VALUE = {
    .sign = 1,
    .length = 1,
    .capacity = 1,
    .storage = mnt_ten_storage,
    .scope_owned_container = false
};

/* Exported canonical handles. */
const mint_t * const MI_ZERO = &MI_ZERO_VALUE;
const mint_t * const MI_ONE = &MI_ONE_VALUE;
const mint_t * const MI_NEG_ONE = &MI_NEG_ONE_VALUE;
const mint_t * const MI_TWO = &MI_TWO_VALUE;
const mint_t * const MI_TEN = &MI_TEN_VALUE;
