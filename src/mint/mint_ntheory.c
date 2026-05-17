#include <stdlib.h>

#include "mint_internal.h"

static void mint_prepare_constant(const mint_t *mint)
{
    if (mint && mint->constant_id != MICONST_NONE)
        mint_constant_ensure(mint);
}

static int mint_prepare_mutable(mint_t *mint)
{
    if (!mint || mint->constant_id != MICONST_NONE)
        return -1;
    return 0;
}

static int mint_factors_append(mint_factors_t *factors, const mint_t *prime,
                               unsigned long exponent)
{
    mint_factor_t *grown;
    mint_t *copy;

    if (!factors || !prime || exponent == 0)
        return -1;

    copy = mi_clone(prime);
    if (!copy)
        return -1;

    grown = realloc(factors->items, (factors->count + 1u) * sizeof(*grown));
    if (!grown) {
        mi_free(copy);
        return -1;
    }

    factors->items = grown;
    factors->items[factors->count].prime = copy;
    factors->items[factors->count].exponent = exponent;
    factors->count++;
    return 0;
}

mint_factors_t *mi_factors(const mint_t *mint)
{
    mint_factors_t *factors;
    mint_t *work;
    mint_t *prime;
    mint_t *quot;
    mint_t *rem;

    if (!mint)
        return NULL;
    mint_prepare_constant(mint);
    if (mpz_sgn(mint->value) <= 0)
        return NULL;

    factors = calloc(1u, sizeof(*factors));
    if (!factors)
        return NULL;

    work = mi_clone(mint);
    prime = mi_create_ulong(2u);
    quot = mi_new();
    rem = mi_new();
    if (!work || !prime || !quot || !rem) {
        mi_free(work);
        mi_free(prime);
        mi_free(quot);
        mi_free(rem);
        mi_factors_free(factors);
        return NULL;
    }

    while (mpz_cmp_ui(work->value, 1u) > 0) {
        unsigned long exponent = 0;

        if (mpz_probab_prime_p(work->value, 25) > 0) {
            if (mint_factors_append(factors, work, 1u) != 0) {
                mi_free(work);
                mi_free(prime);
                mi_free(quot);
                mi_free(rem);
                mi_factors_free(factors);
                return NULL;
            }
            break;
        }

        for (;;) {
            mpz_tdiv_qr(quot->value, rem->value, work->value, prime->value);
            if (mpz_sgn(rem->value) != 0)
                break;
            exponent++;
            mpz_set(work->value, quot->value);
        }

        if (exponent > 0) {
            if (mint_factors_append(factors, prime, exponent) != 0) {
                mi_free(work);
                mi_free(prime);
                mi_free(quot);
                mi_free(rem);
                mi_factors_free(factors);
                return NULL;
            }
        }

        if (mpz_cmp_ui(prime->value, 2u) == 0)
            mpz_set_ui(prime->value, 3u);
        else
            mpz_nextprime(prime->value, prime->value);
    }

    mi_free(work);
    mi_free(prime);
    mi_free(quot);
    mi_free(rem);
    return factors;
}

void mi_factors_free(mint_factors_t *factors)
{
    size_t i;

    if (!factors)
        return;
    for (i = 0; i < factors->count; ++i)
        mi_free(factors->items[i].prime);
    free(factors->items);
    free(factors);
}

bool mi_isprime(const mint_t *mint)
{
    if (!mint)
        return false;
    mint_prepare_constant(mint);
    if (mpz_sgn(mint->value) <= 0)
        return false;
    return mpz_probab_prime_p(mint->value, 25) > 0;
}

mint_primality_result_t mi_prove_prime(const mint_t *mint)
{
    int rc;

    if (!mint)
        return MI_PRIMALITY_UNKNOWN;
    mint_prepare_constant(mint);
    if (mpz_sgn(mint->value) <= 0)
        return MI_PRIMALITY_COMPOSITE;

    rc = mpz_probab_prime_p(mint->value, 50);
    if (rc == 0)
        return MI_PRIMALITY_COMPOSITE;
    return MI_PRIMALITY_PRIME;
}

int mi_nextprime(mint_t *mint)
{
    if (mint_prepare_mutable(mint) != 0)
        return -1;
    if (mpz_cmp_ui(mint->value, 2u) < 0) {
        mpz_set_ui(mint->value, 2u);
        return 0;
    }
    mpz_nextprime(mint->value, mint->value);
    return 0;
}

int mi_prevprime(mint_t *mint)
{
    if (mint_prepare_mutable(mint) != 0)
        return -1;
    if (mpz_cmp_ui(mint->value, 2u) < 0)
        return -1;
    if (mpz_cmp_ui(mint->value, 2u) == 0)
        return 0;

    mpz_sub_ui(mint->value, mint->value, 1u);
    while (mpz_cmp_ui(mint->value, 2u) >= 0) {
        if (mpz_probab_prime_p(mint->value, 25) > 0)
            return 0;
        mpz_sub_ui(mint->value, mint->value, 1u);
    }
    return -1;
}

int mi_gcd(mint_t *mint, const mint_t *other)
{
    if (mint_prepare_mutable(mint) != 0 || !other)
        return -1;
    mint_prepare_constant(other);
    mpz_gcd(mint->value, mint->value, other->value);
    return 0;
}

int mi_lcm(mint_t *mint, const mint_t *other)
{
    if (mint_prepare_mutable(mint) != 0 || !other)
        return -1;
    mint_prepare_constant(other);
    mpz_lcm(mint->value, mint->value, other->value);
    return 0;
}

int mi_gcdext(mint_t *g, mint_t *x, mint_t *y,
              const mint_t *a, const mint_t *b)
{
    mpz_t gz;
    mpz_t xz;
    mpz_t yz;

    if (!a || !b)
        return -1;
    if ((g && g->constant_id != MICONST_NONE) ||
        (x && x->constant_id != MICONST_NONE) ||
        (y && y->constant_id != MICONST_NONE))
        return -1;

    mint_prepare_constant(a);
    mint_prepare_constant(b);

    mpz_init(gz);
    mpz_init(xz);
    mpz_init(yz);
    mpz_gcdext(gz, xz, yz, a->value, b->value);

    if (g)
        mpz_set(g->value, gz);
    if (x)
        mpz_set(x->value, xz);
    if (y)
        mpz_set(y->value, yz);

    mpz_clear(gz);
    mpz_clear(xz);
    mpz_clear(yz);
    return 0;
}

int mi_modinv(mint_t *mint, const mint_t *modulus)
{
    if (mint_prepare_mutable(mint) != 0 || !modulus)
        return -1;
    mint_prepare_constant(modulus);
    if (mpz_sgn(modulus->value) <= 0)
        return -1;
    return mpz_invert(mint->value, mint->value, modulus->value) == 0 ? -1 : 0;
}

int mi_factorial(mint_t *mint, unsigned long n)
{
    if (mint_prepare_mutable(mint) != 0)
        return -1;
    mpz_fac_ui(mint->value, n);
    return 0;
}

int mi_fibonacci(mint_t *mint, unsigned long n)
{
    if (mint_prepare_mutable(mint) != 0)
        return -1;
    mpz_fib_ui(mint->value, n);
    return 0;
}

int mi_binomial(mint_t *mint, unsigned long n, unsigned long k)
{
    if (mint_prepare_mutable(mint) != 0)
        return -1;
    if (k > n)
        return -1;
    mpz_bin_uiui(mint->value, n, k);
    return 0;
}
