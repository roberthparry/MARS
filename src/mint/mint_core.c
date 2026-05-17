#include "mint_internal.h"

#include <stdlib.h>

static void mint_prepare_constant(const mint_t *mint)
{
    if (mint && mint->constant_id != MICONST_NONE)
        mint_constant_ensure(mint);
}

mint_t *mi_new(void)
{
    mint_t *mint = calloc(1u, sizeof(*mint));

    if (!mint)
        return NULL;

    mint->constant_id = MICONST_NONE;
    mpz_init(mint->value);
    return mint;
}

mint_t *mi_const(const mint_t *constant)
{
    return mi_clone(constant);
}

mint_t *mi_create_long(long value)
{
    mint_t *mint = mi_new();

    if (!mint)
        return NULL;
    mpz_set_si(mint->value, value);
    return mint;
}

mint_t *mi_create_ulong(unsigned long value)
{
    mint_t *mint = mi_new();

    if (!mint)
        return NULL;
    mpz_set_ui(mint->value, value);
    return mint;
}

mint_t *mi_create_2pow(uint64_t n)
{
    mint_t *mint = mi_new();

    if (!mint)
        return NULL;
    mpz_set_ui(mint->value, 1u);
    mpz_mul_2exp(mint->value, mint->value, (mp_bitcnt_t)n);
    return mint;
}

mint_t *mi_clone(const mint_t *mint)
{
    mint_t *copy;

    if (!mint)
        return NULL;
    mint_prepare_constant(mint);

    copy = mi_new();
    if (!copy)
        return NULL;
    mpz_set(copy->value, mint->value);
    return copy;
}

void mi_free(mint_t *mint)
{
    if (!mint || mint->constant_id != MICONST_NONE)
        return;
    mpz_clear(mint->value);
    free(mint);
}

void mi_clear(mint_t *mint)
{
    if (!mint || mint->constant_id != MICONST_NONE)
        return;
    mpz_set_ui(mint->value, 0u);
}

int mi_set_long(mint_t *mint, long value)
{
    if (!mint || mint->constant_id != MICONST_NONE)
        return -1;
    mpz_set_si(mint->value, value);
    return 0;
}

int mi_set_ulong(mint_t *mint, unsigned long value)
{
    if (!mint || mint->constant_id != MICONST_NONE)
        return -1;
    mpz_set_ui(mint->value, value);
    return 0;
}
