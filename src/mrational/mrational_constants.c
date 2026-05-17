#include <stdbool.h>

#include "mrational_internal.h"

#define MRATIONAL_CONST_INIT(id_) { .constant_id = (id_) }

mrational_t MR_HALF_VALUE = MRATIONAL_CONST_INIT(MRCONST_HALF);
mrational_t MR_ONE_AND_HALF_VALUE = MRATIONAL_CONST_INIT(MRCONST_ONE_AND_HALF);
mrational_t MR_ONE_THIRD_VALUE = MRATIONAL_CONST_INIT(MRCONST_ONE_THIRD);
mrational_t MR_QUARTER_VALUE = MRATIONAL_CONST_INIT(MRCONST_QUARTER);
mrational_t MR_ONE_SIXTH_VALUE = MRATIONAL_CONST_INIT(MRCONST_ONE_SIXTH);
mrational_t MR_ONE_EIGHTH_VALUE = MRATIONAL_CONST_INIT(MRCONST_ONE_EIGHTH);
mrational_t MR_ONE_TENTH_VALUE = MRATIONAL_CONST_INIT(MRCONST_ONE_TENTH);

const mrational_t * const MR_HALF = &MR_HALF_VALUE;
const mrational_t * const MR_ONE_AND_HALF = &MR_ONE_AND_HALF_VALUE;
const mrational_t * const MR_ONE_THIRD = &MR_ONE_THIRD_VALUE;
const mrational_t * const MR_QUARTER = &MR_QUARTER_VALUE;
const mrational_t * const MR_ONE_SIXTH = &MR_ONE_SIXTH_VALUE;
const mrational_t * const MR_ONE_EIGHTH = &MR_ONE_EIGHTH_VALUE;
const mrational_t * const MR_ONE_TENTH = &MR_ONE_TENTH_VALUE;

static bool mrational_runtime_initialised;
static bool mr_half_ready;
static bool mr_one_and_half_ready;
static bool mr_one_third_ready;
static bool mr_quarter_ready;
static bool mr_one_sixth_ready;
static bool mr_one_eighth_ready;
static bool mr_one_tenth_ready;

typedef struct mrational_const_cache_t {
    mrational_t *value;
    bool *ready;
    long numerator;
    long denominator;
} mrational_const_cache_t;

static const mrational_const_cache_t mrational_const_cache[MRCONST_COUNT] = {
    [MRCONST_HALF] = { &MR_HALF_VALUE, &mr_half_ready, 1, 2 },
    [MRCONST_ONE_AND_HALF] = { &MR_ONE_AND_HALF_VALUE, &mr_one_and_half_ready, 3, 2 },
    [MRCONST_ONE_THIRD] = { &MR_ONE_THIRD_VALUE, &mr_one_third_ready, 1, 3 },
    [MRCONST_QUARTER] = { &MR_QUARTER_VALUE, &mr_quarter_ready, 1, 4 },
    [MRCONST_ONE_SIXTH] = { &MR_ONE_SIXTH_VALUE, &mr_one_sixth_ready, 1, 6 },
    [MRCONST_ONE_EIGHTH] = { &MR_ONE_EIGHTH_VALUE, &mr_one_eighth_ready, 1, 8 },
    [MRCONST_ONE_TENTH] = { &MR_ONE_TENTH_VALUE, &mr_one_tenth_ready, 1, 10 }
};

static void mrational_ensure_runtime_init(void)
{
    if (mrational_runtime_initialised)
        return;
    mrational_runtime_initialised = true;
}

static void mrational_const_init_once(mrational_constant_id_t id)
{
    const mrational_const_cache_t *entry;

    if (id <= MRCONST_NONE || id >= MRCONST_COUNT)
        return;

    entry = &mrational_const_cache[id];
    if (!entry->ready || !entry->value || *entry->ready)
        return;

    mpq_init(entry->value->value);
    mpq_set_si(entry->value->value, entry->numerator, entry->denominator);
    mpq_canonicalize(entry->value->value);
    *entry->ready = true;
}

static void __attribute__((destructor)) mrational_constants_shutdown(void)
{
    mrational_constant_id_t id;

    if (!mrational_runtime_initialised)
        return;

    for (id = MRCONST_HALF; id < MRCONST_COUNT; ++id) {
        if (mrational_const_cache[id].ready && *mrational_const_cache[id].ready) {
            mpq_clear(mrational_const_cache[id].value->value);
            *mrational_const_cache[id].ready = false;
        }
        mi_free(mrational_const_cache[id].value->numerator_view);
        mi_free(mrational_const_cache[id].value->denominator_view);
        mrational_const_cache[id].value->numerator_view = NULL;
        mrational_const_cache[id].value->denominator_view = NULL;
    }

    mrational_runtime_initialised = false;
}

void mrational_constant_ensure(const mrational_t *rational)
{
    if (!rational || rational->constant_id == MRCONST_NONE)
        return;
    mrational_ensure_runtime_init();
    mrational_const_init_once(rational->constant_id);
}

void mrational_constants_ensure_init(void)
{
    mrational_constant_id_t id;

    mrational_ensure_runtime_init();
    for (id = MRCONST_HALF; id < MRCONST_COUNT; ++id)
        mrational_const_init_once(id);
}
