#include <stdbool.h>

#include "mint_internal.h"

#define MINT_CONST_INIT(id_) { .constant_id = (id_) }

mint_t MI_ZERO_VALUE = MINT_CONST_INIT(MICONST_ZERO);
mint_t MI_ONE_VALUE = MINT_CONST_INIT(MICONST_ONE);
mint_t MI_NEG_ONE_VALUE = MINT_CONST_INIT(MICONST_NEG_ONE);
mint_t MI_TWO_VALUE = MINT_CONST_INIT(MICONST_TWO);
mint_t MI_TEN_VALUE = MINT_CONST_INIT(MICONST_TEN);

const mint_t * const MI_ZERO = &MI_ZERO_VALUE;
const mint_t * const MI_ONE = &MI_ONE_VALUE;
const mint_t * const MI_NEG_ONE = &MI_NEG_ONE_VALUE;
const mint_t * const MI_TWO = &MI_TWO_VALUE;
const mint_t * const MI_TEN = &MI_TEN_VALUE;

static bool mint_runtime_initialised;
static bool mi_zero_ready;
static bool mi_one_ready;
static bool mi_neg_one_ready;
static bool mi_two_ready;
static bool mi_ten_ready;

typedef struct mint_const_cache_t {
    mint_t *value;
    bool *ready;
    long initial;
} mint_const_cache_t;

static const mint_const_cache_t mint_const_cache[MICONST_COUNT] = {
    [MICONST_ZERO] = { &MI_ZERO_VALUE, &mi_zero_ready, 0 },
    [MICONST_ONE] = { &MI_ONE_VALUE, &mi_one_ready, 1 },
    [MICONST_NEG_ONE] = { &MI_NEG_ONE_VALUE, &mi_neg_one_ready, -1 },
    [MICONST_TWO] = { &MI_TWO_VALUE, &mi_two_ready, 2 },
    [MICONST_TEN] = { &MI_TEN_VALUE, &mi_ten_ready, 10 }
};

static void mint_ensure_runtime_init(void)
{
    if (mint_runtime_initialised)
        return;
    mint_runtime_initialised = true;
}

static void mint_const_init_once(mint_constant_id_t id)
{
    const mint_const_cache_t *entry;

    if (id <= MICONST_NONE || id >= MICONST_COUNT)
        return;

    entry = &mint_const_cache[id];
    if (!entry->ready || !entry->value || *entry->ready)
        return;

    mpz_init_set_si(entry->value->value, entry->initial);
    *entry->ready = true;
}

static void __attribute__((destructor)) mint_constants_shutdown(void)
{
    mint_constant_id_t id;

    if (!mint_runtime_initialised)
        return;

    for (id = MICONST_ZERO; id < MICONST_COUNT; ++id) {
        if (mint_const_cache[id].ready && *mint_const_cache[id].ready) {
            mpz_clear(mint_const_cache[id].value->value);
            *mint_const_cache[id].ready = false;
        }
    }

    mint_runtime_initialised = false;
}

void mint_constant_ensure(const mint_t *mint)
{
    if (!mint || mint->constant_id == MICONST_NONE)
        return;
    mint_ensure_runtime_init();
    mint_const_init_once(mint->constant_id);
}

void mint_constants_ensure_init(void)
{
    mint_constant_id_t id;

    mint_ensure_runtime_init();
    for (id = MICONST_ZERO; id < MICONST_COUNT; ++id)
        mint_const_init_once(id);
}
