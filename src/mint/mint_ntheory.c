#include "mint_internal.h"

#include <stdlib.h>
mint_factors_t *mi_factors(const mint_t *mint)
{
    mint_factors_t *factors;

    if (!mint || mint->sign <= 0)
        return NULL;

    factors = calloc(1, sizeof(*factors));
    if (!factors)
        return NULL;

    if (mint_factor_recursive(mint, factors) != 0) {
        mi_factors_free(factors);
        return NULL;
    }

    mint_factors_sort_and_compress(factors);
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
    unsigned long ulong_limit;
    size_t mersenne_exponent;
    size_t i;
    uint64_t rem;

    if (!mint || mint->sign <= 0)
        return 0;
    if (mi_cmp(mint, MI_ONE) <= 0)
        return 0;
    if (mint->length == 1) {
        uint64_t n = mint->storage[0];

        if (n == 2 || n == 3)
            return 1;
    }
    if (mint_is_even_internal(mint))
        return 0;

    for (i = 0; i < sizeof(mint_small_primes) / sizeof(mint_small_primes[0]); ++i) {
        if (mint->length == 1 && mint->storage[0] == mint_small_primes[i])
            return true;
        rem = mint_mod_u64(mint, mint_small_primes[i]);
        if (rem == 0)
            return false;
    }

    if (mint_get_ulong_if_fits(mint, &ulong_limit)) {
        unsigned long root_limit = 1;

        while ((root_limit + 1) <= ulong_limit / (root_limit + 1))
            root_limit++;
        return mint_isprime_sieved_upto_ulong(mint, root_limit) > 0;
    }

    {
        mint_t *plus_one = mint_dup_value(mint);
        int is_mersenne_prime = 0;

        if (!plus_one)
            return false;
        if (mint_add_small(plus_one, 1) != 0) {
            mi_free(plus_one);
            return false;
        }
        if (mint_detect_power_of_two_exponent(plus_one, &mersenne_exponent) &&
            mint_isprime_small_ulong((unsigned long)mersenne_exponent)) {
            is_mersenne_prime = mint_isprime_lucas_lehmer(mint, mersenne_exponent);
            mi_free(plus_one);
            return is_mersenne_prime;
        }
        mi_free(plus_one);
    }

    if (mint_isprime_strong_probable_prime_base2(mint) <= 0)
        return false;

    return mint_isprime_lucas_selfridge(mint) > 0;
}

mint_primality_result_t mi_prove_prime(const mint_t *mint)
{
    return mint_prove_prime_internal(mint);
}

int mi_nextprime(mint_t *mint)
{
    if (!mint || mint_is_immortal(mint))
        return -1;

    if (mi_cmp_long(mint, 2) < 0)
        return mi_set_long(mint, 2);
    if (mi_cmp_long(mint, 2) == 0)
        return 0;
    if (mi_cmp_long(mint, 3) == 0)
        return 0;
    if (mi_cmp_long(mint, 5) < 0)
        return mi_set_long(mint, 5);
    if (mi_cmp_long(mint, 5) == 0)
        return 0;
    if (mi_cmp_long(mint, 7) < 0)
        return mi_set_long(mint, 7);
    if (mi_cmp_long(mint, 7) == 0)
        return 0;

    if (mint_adjust_to_next_wheel210(mint) != 0)
        return -1;

    while (!mi_isprime(mint)) {
        uint64_t rem = mint_mod_u64(mint, 210);
        long step = mint_nextprime_wheel210_step(rem);

        if (step < 0 || mi_add_long(mint, step) != 0)
            return -1;
    }

    return 0;
}

int mi_prevprime(mint_t *mint)
{
    if (!mint || mint_is_immortal(mint))
        return -1;

    if (mi_cmp_long(mint, 2) < 0)
        return -1;
    if (mi_cmp_long(mint, 2) == 0)
        return 0;
    if (mi_cmp_long(mint, 3) == 0)
        return 0;
    if (mi_cmp_long(mint, 5) < 0)
        return mi_set_long(mint, 3);
    if (mi_cmp_long(mint, 5) == 0)
        return 0;
    if (mi_cmp_long(mint, 7) < 0)
        return mi_set_long(mint, 5);
    if (mi_cmp_long(mint, 7) == 0)
        return 0;

    if (mint_adjust_to_prev_wheel210(mint) != 0)
        return -1;

    while (mi_cmp_long(mint, 11) >= 0 && !mi_isprime(mint)) {
        uint64_t rem = mint_mod_u64(mint, 210);
        long step = mint_prevprime_wheel210_step(rem);

        if (step < 0 || mi_sub_long(mint, step) != 0)
            return -1;
    }

    return 0;
}

int mi_gcd(mint_t *mint, const mint_t *other)
{
    mint_t *a, *b, *r;
    unsigned long qs[8];
    int rc = -1;

    if (!mint || !other || mint_is_immortal(mint))
        return -1;

    a = mint_dup_value(mint);
    b = mint_dup_value(other);
    r = mi_new();
    if (!a || !b || !r)
        goto cleanup;

    a->sign = mint_is_zero_internal(a) ? 0 : 1;
    b->sign = mint_is_zero_internal(b) ? 0 : 1;

    while (!mint_is_zero_internal(b)) {
        size_t qcount = mint_lehmer_collect_quotients(a, b, qs,
                                                      sizeof(qs) / sizeof(qs[0]));

        if (qcount > 1) {
            if (mint_lehmer_apply_quotients(a, b, qs, qcount, r) != 0)
                goto cleanup;
            continue;
        }

        if (mint_copy_value(r, a) != 0)
            goto cleanup;
        if (mint_mod_inplace(r, b) != 0)
            goto cleanup;
        if (mint_copy_value(a, b) != 0)
            goto cleanup;
        if (mint_copy_value(b, r) != 0)
            goto cleanup;
        b->sign = mint_is_zero_internal(b) ? 0 : 1;
    }

    if (mint_copy_value(mint, a) != 0)
        goto cleanup;
    mint->sign = mint_is_zero_internal(mint) ? 0 : 1;
    rc = 0;

cleanup:
    mi_free(a);
    mi_free(b);
    mi_free(r);
    return rc;
}

int mi_lcm(mint_t *mint, const mint_t *other)
{
    mint_t *a, *b, *g;
    int rc = -1;

    if (!mint || !other || mint_is_immortal(mint))
        return -1;
    if (mint_is_zero_internal(mint) || mint_is_zero_internal(other)) {
        mi_clear(mint);
        return 0;
    }

    a = mint_dup_value(mint);
    b = mint_dup_value(other);
    g = mint_dup_value(mint);
    if (!a || !b || !g)
        goto cleanup;

    a->sign = 1;
    b->sign = 1;

    if (mi_gcd(g, other) != 0)
        goto cleanup;
    if (mint_div_inplace(a, g, NULL) != 0)
        goto cleanup;
    if (mint_mul_inplace(a, b) != 0)
        goto cleanup;
    a->sign = mint_is_zero_internal(a) ? 0 : 1;

    if (mint_copy_value(mint, a) != 0)
        goto cleanup;
    mint->sign = mint_is_zero_internal(mint) ? 0 : 1;
    rc = 0;

cleanup:
    mi_free(a);
    mi_free(b);
    mi_free(g);
    return rc;
}

int mi_gcdext(mint_t *g, mint_t *x, mint_t *y,
                const mint_t *a, const mint_t *b)
{
    mint_t *old_r = NULL, *r = NULL, *q = NULL, *rem = NULL;
    mint_t *old_s = NULL, *s = NULL, *old_t = NULL, *t = NULL;
    mint_t *tmp = NULL;
    unsigned long qs[8];
    int rc = -1;

    if (!a || !b)
        return -1;
    if ((g && mint_is_immortal(g)) ||
        (x && mint_is_immortal(x)) ||
        (y && mint_is_immortal(y)))
        return -1;

    old_r = mint_dup_value(a);
    r = mint_dup_value(b);
    q = mi_new();
    rem = mi_new();
    old_s = mi_create_long(1);
    s = mi_create_long(0);
    old_t = mi_create_long(0);
    t = mi_create_long(1);
    tmp = mi_new();
    if (!old_r || !r || !q || !rem || !old_s || !s || !old_t || !t || !tmp)
        goto cleanup;

    if (mi_abs(old_r) != 0 || mi_abs(r) != 0)
        goto cleanup;

    while (!mint_is_zero_internal(r)) {
        size_t qcount = mint_lehmer_collect_quotients(old_r, r, qs,
                                                      sizeof(qs) / sizeof(qs[0]));
        mint_t *swap;

        if (qcount > 1) {
            size_t i;

            for (i = 0; i < qcount; ++i) {
                if (mint_copy_value(rem, r) != 0)
                    goto cleanup;
                if (mint_mul_word_inplace(rem, (uint64_t)qs[i]) != 0 ||
                    mi_sub(old_r, rem) != 0)
                    goto cleanup;

                if (mint_copy_value(tmp, s) != 0)
                    goto cleanup;
                if (mint_mul_word_inplace(tmp, (uint64_t)qs[i]) != 0 ||
                    mi_sub(old_s, tmp) != 0)
                    goto cleanup;

                if (mint_copy_value(tmp, t) != 0)
                    goto cleanup;
                if (mint_mul_word_inplace(tmp, (uint64_t)qs[i]) != 0 ||
                    mi_sub(old_t, tmp) != 0)
                    goto cleanup;

                swap = old_r;
                old_r = r;
                r = swap;

                swap = old_s;
                old_s = s;
                s = swap;

                swap = old_t;
                old_t = t;
                t = swap;
            }
            continue;
        }

        if (mi_divmod(old_r, r, q, rem) != 0)
            goto cleanup;

        if (mint_copy_value(tmp, s) != 0)
            goto cleanup;
        if (mi_mul(tmp, q) != 0 || mi_sub(old_s, tmp) != 0)
            goto cleanup;

        if (mint_copy_value(tmp, t) != 0)
            goto cleanup;
        if (mi_mul(tmp, q) != 0 || mi_sub(old_t, tmp) != 0)
            goto cleanup;

        swap = old_r;
        old_r = r;
        r = rem;
        rem = swap;

        swap = old_s;
        old_s = s;
        s = swap;

        swap = old_t;
        old_t = t;
        t = swap;
    }

    if (a->sign < 0 && mi_neg(old_s) != 0)
        goto cleanup;
    if (b->sign < 0 && mi_neg(old_t) != 0)
        goto cleanup;

    if (g) {
        if (mint_copy_value(g, old_r) != 0)
            goto cleanup;
        g->sign = mint_is_zero_internal(g) ? 0 : 1;
    }
    if (x) {
        if (mint_copy_value(x, old_s) != 0)
            goto cleanup;
        x->sign = old_s->sign;
    }
    if (y) {
        if (mint_copy_value(y, old_t) != 0)
            goto cleanup;
        y->sign = old_t->sign;
    }

    rc = 0;

cleanup:
    mi_free(old_r);
    mi_free(r);
    mi_free(q);
    mi_free(rem);
    mi_free(old_s);
    mi_free(s);
    mi_free(old_t);
    mi_free(t);
    mi_free(tmp);
    return rc;
}

int mi_modinv(mint_t *mint, const mint_t *modulus)
{
    mint_t *g = NULL, *x = NULL;
    int rc = -1;

    if (!mint || !modulus || mint_is_immortal(mint))
        return -1;
    if (modulus->sign <= 0)
        return -1;

    g = mi_new();
    x = mi_new();
    if (!g || !x)
        goto cleanup;

    if (mi_gcdext(g, x, NULL, mint, modulus) != 0)
        goto cleanup;
    if (mi_cmp(g, MI_ONE) != 0)
        goto cleanup;

    if (mint_mod_positive_inplace(x, modulus) != 0)
        goto cleanup;

    if (mint_copy_value(mint, x) != 0)
        goto cleanup;
    mint->sign = mint_is_zero_internal(mint) ? 0 : 1;
    rc = 0;

cleanup:
    mi_free(g);
    mi_free(x);
    return rc;
}

int mi_factorial(mint_t *mint, unsigned long n)
{
    unsigned long i;

    if (!mint || mint_is_immortal(mint))
        return -1;
    if (mi_set_long(mint, 1) != 0)
        return -1;
    if (n < 2)
        return 0;

    for (i = 2; i <= n; ++i) {
        if (mint_mul_word_inplace(mint, (uint64_t)i) != 0)
            return -1;
    }
    return 0;
}

int mi_fibonacci(mint_t *mint, unsigned long n)
{
    mint_t *fn1 = NULL;
    int rc = -1;

    if (!mint || mint_is_immortal(mint))
        return -1;

    fn1 = mi_new();
    if (!fn1)
        return -1;

    if (mint_fibonacci_pair(mint, fn1, n) != 0)
        goto cleanup;

    rc = 0;

cleanup:
    mi_free(fn1);
    return rc;
}

int mi_binomial(mint_t *mint, unsigned long n, unsigned long k)
{
    unsigned long i;
    unsigned long limit;

    if (!mint || mint_is_immortal(mint))
        return -1;
    if (k > n)
        return -1;

    limit = k;
    if (limit > n - limit)
        limit = n - limit;

    if (mi_set_long(mint, 1) != 0)
        return -1;
    if (limit == 0)
        return 0;

    for (i = 1; i <= limit; ++i) {
        uint64_t num = (uint64_t)(n - limit + i);
        uint64_t den = (uint64_t)i;
        uint64_t g = mint_gcd_u64(num, den);
        uint64_t rem;

        num /= g;
        den /= g;

        if (mint_mul_word_inplace(mint, num) != 0)
            return -1;

        rem = mint_div_word_inplace(mint, den);
        if (rem != 0)
            return -1;
        mint->sign = mint_is_zero_internal(mint) ? 0 : 1;
    }

    return 0;
}
