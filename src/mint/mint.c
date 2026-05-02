#include "mint.h"

#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MINT_SIEVE_SEGMENT_ODDS 32768ul

static const unsigned long mint_small_primes[] = {
    2ul, 3ul, 5ul, 7ul, 11ul, 13ul, 17ul, 19ul, 23ul, 29ul,
    31ul, 37ul, 41ul, 43ul, 47ul, 53ul, 59ul, 61ul, 67ul, 71ul,
    73ul, 79ul, 83ul, 89ul, 97ul, 101ul, 103ul, 107ul, 109ul, 113ul,
    127ul, 131ul, 137ul, 139ul, 149ul, 151ul, 157ul, 163ul, 167ul,
    173ul, 179ul, 181ul, 191ul, 193ul, 197ul, 199ul, 211ul
};

static const uint64_t mint_wheel210_residues[] = {
    1u, 11u, 13u, 17u, 19u, 23u, 29u, 31u,
    37u, 41u, 43u, 47u, 53u, 59u, 61u, 67u,
    71u, 73u, 79u, 83u, 89u, 97u, 101u, 103u,
    107u, 109u, 113u, 121u, 127u, 131u, 137u, 139u,
    143u, 149u, 151u, 157u, 163u, 167u, 169u, 173u,
    179u, 181u, 187u, 191u, 193u, 197u, 199u, 209u
};

struct _mint_t {
    short sign;       /* -1, 0, +1 */
    size_t length;    /* number of used 64-bit limbs */
    size_t capacity;  /* number of allocated 64-bit limbs */
    uint64_t *storage;
};

static uint64_t mnt_one_storage[] = { 1 };
static uint64_t mnt_ten_storage[] = { 10 };

static struct _mint_t mnt_zero_static = {
    .sign = 0,
    .length = 0,
    .capacity = 0,
    .storage = NULL
};

static struct _mint_t mnt_one_static = {
    .sign = 1,
    .length = 1,
    .capacity = 1,
    .storage = mnt_one_storage
};

static struct _mint_t mnt_ten_static = {
    .sign = 1,
    .length = 1,
    .capacity = 1,
    .storage = mnt_ten_storage
};

mint_t *MNT_ZERO = &mnt_zero_static;
mint_t *MNT_ONE = &mnt_one_static;
mint_t *MNT_TEN = &mnt_ten_static;

static int mint_add_inplace(mint_t *mint, const mint_t *other);
static int mint_and_inplace(mint_t *mint, const mint_t *other);
static int mint_mul_inplace(mint_t *mint, const mint_t *other);
static int mint_div_inplace(mint_t *mint, const mint_t *other, mint_t *rem);
static int mint_mod_inplace(mint_t *mint, const mint_t *other);
static int mint_neg_inplace(mint_t *mint);
static int mint_not_inplace(mint_t *mint);
static int mint_or_inplace(mint_t *mint, const mint_t *other);
static int mint_pow_inplace(mint_t *mint, unsigned long exponent);
static int mint_powmod_inplace(mint_t *mint, const mint_t *exponent,
                               const mint_t *modulus);
static int mint_pow10_inplace(mint_t *mint, long exponent);
static int mint_shl_inplace(mint_t *mint, long bits);
static int mint_shr_inplace(mint_t *mint, long bits);
static int mint_sqrt_inplace(mint_t *mint);
static int mint_xor_inplace(mint_t *mint, const mint_t *other);
static mint_t *mint_dup_value(const mint_t *src);
static int mint_copy_value(mint_t *dst, const mint_t *src);
static int mint_abs_diff(mint_t *dst, const mint_t *a, const mint_t *b);
static int mint_pollard_rho_factor(const mint_t *n, mint_t *factor);
static int mint_factor_recursive(const mint_t *n, mint_factors_t *factors);
static void mint_factors_sort_and_compress(mint_factors_t *factors);
static size_t mint_bit_length_internal(const mint_t *mint);
static int mint_get_bit(const mint_t *mint, size_t bit_index);
static int mint_mod_positive_inplace(mint_t *mint, const mint_t *modulus);
static int mint_half_mod_odd_inplace(mint_t *mint, const mint_t *modulus);
static int mint_jacobi_u64(uint64_t a, uint64_t n);
static int mint_jacobi_small_over_mint(long a, const mint_t *n);
static int mint_isprime_lucas_selfridge(const mint_t *mint);
static mint_primality_result_t mint_prove_prime_internal(const mint_t *mint);
static mint_primality_result_t mint_prove_prime_pocklington(const mint_t *mint);
static mint_primality_result_t mint_prove_prime_ec_nplus1_supersingular(const mint_t *mint);
static mint_primality_result_t mint_prove_prime_ec_witness(const mint_t *mint);
static int mint_factors_append(mint_factors_t *factors,
                               const mint_t *prime,
                               unsigned long exponent);
static int mint_collect_proven_factors_partial(const mint_t *n,
                                               mint_factors_t *proven,
                                               mint_t *remaining);

typedef enum mint_ec_step_result_t {
    MINT_EC_STEP_OK = 0,
    MINT_EC_STEP_INFINITY = 1,
    MINT_EC_STEP_COMPOSITE = 2,
    MINT_EC_STEP_ERROR = 3
} mint_ec_step_result_t;

typedef struct mint_ec_point_t {
    mint_t *x;
    mint_t *y;
    int infinity;
} mint_ec_point_t;

static int mint_is_immortal(const mint_t *mint)
{
    return mint == MNT_ZERO || mint == MNT_ONE || mint == MNT_TEN;
}

static int mint_is_zero_internal(const mint_t *mint)
{
    return !mint || mint->sign == 0 || mint->length == 0;
}

static void mint_normalise(mint_t *mint)
{
    if (!mint)
        return;

    while (mint->length > 0 && mint->storage[mint->length - 1] == 0)
        mint->length--;

    if (mint->length == 0)
        mint->sign = 0;
}

static int mint_cmp_abs(const mint_t *a, const mint_t *b)
{
    size_t i;

    if (a->length < b->length)
        return -1;
    if (a->length > b->length)
        return 1;

    for (i = a->length; i > 0; --i) {
        if (a->storage[i - 1] < b->storage[i - 1])
            return -1;
        if (a->storage[i - 1] > b->storage[i - 1])
            return 1;
    }
    return 0;
}

static int mint_has_negative_operand(const mint_t *a, const mint_t *b)
{
    return (a && a->sign < 0) || (b && b->sign < 0);
}

static int mint_is_even_internal(const mint_t *mint)
{
    return mint_is_zero_internal(mint) || ((mint->storage[0] & 1u) == 0);
}

static int mint_is_odd_internal(const mint_t *mint)
{
    return !mint_is_zero_internal(mint) && ((mint->storage[0] & 1u) != 0);
}

static void mint_zero_spare_limbs(mint_t *mint, size_t from)
{
    if (!mint || !mint->storage || from >= mint->capacity)
        return;
    memset(mint->storage + from, 0, (mint->capacity - from) * sizeof(*mint->storage));
}

static int mint_ensure_capacity(mint_t *mint, size_t needed)
{
    uint64_t *grown;
    size_t new_cap;

    if (!mint)
        return -1;
    if (needed <= mint->capacity)
        return 0;
    if (mint_is_immortal(mint))
        return -1;

    new_cap = mint->capacity ? mint->capacity : 1;
    while (new_cap < needed) {
        if (new_cap > SIZE_MAX / 2) {
            new_cap = needed;
        } else {
            new_cap *= 2;
        }
    }

    grown = realloc(mint->storage, new_cap * sizeof(*grown));
    if (!grown)
        return -1;

    if (new_cap > mint->capacity)
        memset(grown + mint->capacity, 0,
               (new_cap - mint->capacity) * sizeof(*grown));

    mint->storage = grown;
    mint->capacity = new_cap;
    return 0;
}

static int mint_set_magnitude_u64(mint_t *mint, uint64_t magnitude, short sign)
{
    if (!mint || mint_is_immortal(mint))
        return -1;

    if (magnitude == 0) {
        mint_clear(mint);
        return 0;
    }

    if (mint_ensure_capacity(mint, 1) != 0)
        return -1;

    mint->storage[0] = magnitude;
    mint->length = 1;
    mint->sign = sign < 0 ? -1 : 1;
    return 0;
}

static int mint_abs_add_inplace(mint_t *dst, const mint_t *src)
{
    __uint128_t carry = 0;
    size_t i, max_len;

    if (!dst || !src)
        return -1;

    max_len = dst->length > src->length ? dst->length : src->length;
    if (mint_ensure_capacity(dst, max_len + 1) != 0)
        return -1;

    for (i = 0; i < max_len; ++i) {
        __uint128_t sum = carry;

        if (i < dst->length)
            sum += dst->storage[i];
        else
            dst->storage[i] = 0;
        if (i < src->length)
            sum += src->storage[i];

        dst->storage[i] = (uint64_t)sum;
        carry = sum >> 64;
    }

    dst->length = max_len;
    if (carry != 0)
        dst->storage[dst->length++] = (uint64_t)carry;
    return 0;
}

static void mint_abs_sub_inplace(mint_t *dst, const mint_t *src)
{
    __uint128_t borrow = 0;
    size_t i;

    for (i = 0; i < dst->length; ++i) {
        __uint128_t lhs = dst->storage[i];
        __uint128_t rhs = borrow;

        if (i < src->length)
            rhs += src->storage[i];

        dst->storage[i] = (uint64_t)(lhs - rhs);
        borrow = lhs < rhs ? 1 : 0;
    }

    mint_normalise(dst);
}

static int mint_mul_small(mint_t *mint, uint32_t factor)
{
    __uint128_t carry = 0;
    size_t i;

    if (!mint)
        return -1;
    if (mint->sign == 0 || factor == 1)
        return 0;
    if (factor == 0) {
        mint_clear(mint);
        return 0;
    }

    for (i = 0; i < mint->length; ++i) {
        __uint128_t prod = (__uint128_t)mint->storage[i] * factor + carry;

        mint->storage[i] = (uint64_t)prod;
        carry = prod >> 64;
    }

    if (carry != 0) {
        if (mint_ensure_capacity(mint, mint->length + 1) != 0)
            return -1;
        mint->storage[mint->length++] = (uint64_t)carry;
    }

    return 0;
}

static int mint_shift_left_one(mint_t *mint)
{
    uint64_t carry = 0;
    size_t i;

    if (!mint)
        return -1;
    if (mint_is_zero_internal(mint))
        return 0;

    if (mint_ensure_capacity(mint, mint->length + 1) != 0)
        return -1;

    for (i = 0; i < mint->length; ++i) {
        uint64_t next = mint->storage[i] >> 63;

        mint->storage[i] = (mint->storage[i] << 1) | carry;
        carry = next;
    }

    if (carry != 0)
        mint->storage[mint->length++] = carry;

    return 0;
}

static int mint_sub_small(mint_t *mint, uint32_t subtrahend)
{
    size_t i;
    uint64_t borrow = subtrahend;

    if (!mint)
        return -1;
    if (mint_is_zero_internal(mint))
        return subtrahend == 0 ? 0 : -1;
    if (mint->sign < 0)
        return -1;

    for (i = 0; i < mint->length && borrow != 0; ++i) {
        uint64_t lhs = mint->storage[i];

        mint->storage[i] = lhs - borrow;
        borrow = lhs < borrow ? 1 : 0;
    }

    if (borrow != 0)
        return -1;

    mint_normalise(mint);
    return 0;
}

static int mint_add_small(mint_t *mint, uint32_t addend)
{
    __uint128_t carry;
    size_t i;

    if (!mint)
        return -1;
    if (addend == 0)
        return 0;

    if (mint->sign == 0)
        return mint_set_magnitude_u64(mint, addend, 1);

    carry = addend;
    for (i = 0; i < mint->length && carry != 0; ++i) {
        __uint128_t sum = (__uint128_t)mint->storage[i] + carry;

        mint->storage[i] = (uint64_t)sum;
        carry = sum >> 64;
    }

    if (carry != 0) {
        if (mint_ensure_capacity(mint, mint->length + 1) != 0)
            return -1;
        mint->storage[mint->length++] = (uint64_t)carry;
    }

    return 0;
}

static uint32_t mint_div_small_inplace(mint_t *mint, uint32_t divisor)
{
    __uint128_t rem = 0;
    size_t i;

    for (i = mint->length; i > 0; --i) {
        __uint128_t cur = (rem << 64) | mint->storage[i - 1];

        mint->storage[i - 1] = (uint64_t)(cur / divisor);
        rem = cur % divisor;
    }

    mint_normalise(mint);
    return (uint32_t)rem;
}

static int mint_hex_digit_value(unsigned char ch)
{
    if (ch >= '0' && ch <= '9')
        return (int)(ch - '0');
    if (ch >= 'a' && ch <= 'f')
        return 10 + (int)(ch - 'a');
    if (ch >= 'A' && ch <= 'F')
        return 10 + (int)(ch - 'A');
    return -1;
}

static uint64_t mint_mod_u64(const mint_t *mint, uint64_t divisor)
{
    __uint128_t rem = 0;
    size_t i;

    if (!mint || divisor == 0)
        return 0;

    for (i = mint->length; i > 0; --i) {
        __uint128_t cur = (rem << 64) | mint->storage[i - 1];

        rem = cur % divisor;
    }

    return (uint64_t)rem;
}

static int mint_mod_positive_inplace(mint_t *mint, const mint_t *modulus)
{
    if (!mint || !modulus)
        return -1;
    if (mint_mod_inplace(mint, modulus) != 0)
        return -1;
    while (mint_is_negative(mint))
        if (mint_add_inplace(mint, modulus) != 0)
            return -1;
    return 0;
}

static int mint_half_mod_odd_inplace(mint_t *mint, const mint_t *modulus)
{
    if (!mint || !modulus || mint_is_even_internal(modulus))
        return -1;
    if (mint_is_odd_internal(mint))
        if (mint_add_inplace(mint, modulus) != 0)
            return -1;
    return mint_shr_inplace(mint, 1);
}

static int mint_jacobi_u64(uint64_t a, uint64_t n)
{
    int result = 1;

    if ((n & 1u) == 0 || n == 0)
        return 0;

    a %= n;
    while (a != 0) {
        while ((a & 1u) == 0) {
            a >>= 1;
            if ((n & 7u) == 3u || (n & 7u) == 5u)
                result = -result;
        }

        {
            uint64_t tmp = a;

            a = n;
            n = tmp;
        }

        if ((a & 3u) == 3u && (n & 3u) == 3u)
            result = -result;
        a %= n;
    }

    return n == 1 ? result : 0;
}

static int mint_jacobi_small_over_mint(long a, const mint_t *n)
{
    unsigned long abs_a;
    int result = 1;
    uint64_t rem;

    if (!n || n->sign <= 0 || mint_is_even_internal(n))
        return 0;
    if (a == 0)
        return mint_cmp_long(n, 1) == 0 ? 1 : 0;

    if (a < 0) {
        if (mint_mod_u64(n, 4u) == 3u)
            result = -result;
        abs_a = (unsigned long)(-(a + 1)) + 1ul;
    } else {
        abs_a = (unsigned long)a;
    }

    while ((abs_a & 1ul) == 0) {
        abs_a >>= 1;
        rem = mint_mod_u64(n, 8u);
        if (rem == 3u || rem == 5u)
            result = -result;
    }

    if (abs_a == 1ul)
        return result;

    rem = mint_mod_u64(n, abs_a);
    if (rem == 0)
        return 0;

    if ((abs_a & 3ul) == 3ul && mint_mod_u64(n, 4u) == 3u)
        result = -result;

    return result * mint_jacobi_u64(rem, abs_a);
}

static void mint_ec_point_clear(mint_ec_point_t *point)
{
    if (!point)
        return;
    mint_free(point->x);
    mint_free(point->y);
    point->x = NULL;
    point->y = NULL;
    point->infinity = 1;
}

static int mint_ec_point_init(mint_ec_point_t *point)
{
    if (!point)
        return -1;
    point->x = mint_new();
    point->y = mint_new();
    if (!point->x || !point->y) {
        mint_ec_point_clear(point);
        return -1;
    }
    point->infinity = 1;
    return 0;
}

static int mint_ec_point_copy(mint_ec_point_t *dst, const mint_ec_point_t *src)
{
    if (!dst || !src || !dst->x || !dst->y || !src->x || !src->y)
        return -1;
    dst->infinity = src->infinity;
    if (src->infinity)
        return 0;
    if (mint_copy_value(dst->x, src->x) != 0 ||
        mint_copy_value(dst->y, src->y) != 0)
        return -1;
    return 0;
}

static mint_ec_step_result_t mint_ec_make_slope(const mint_t *numerator,
                                                const mint_t *denominator,
                                                const mint_t *modulus,
                                                mint_t *slope)
{
    mint_t *den = NULL;
    mint_t *g = NULL;
    int rc = MINT_EC_STEP_ERROR;

    if (!numerator || !denominator || !modulus || !slope)
        return MINT_EC_STEP_ERROR;

    den = mint_dup_value(denominator);
    g = mint_dup_value(denominator);
    if (!den || !g)
        goto cleanup;

    if (mint_mod_positive_inplace(den, modulus) != 0 ||
        mint_copy_value(g, den) != 0 ||
        mint_gcd(g, modulus) != 0)
        goto cleanup;

    if (mint_cmp(g, MNT_ONE) != 0) {
        if (mint_cmp(g, modulus) < 0)
            rc = MINT_EC_STEP_COMPOSITE;
        else
            rc = MINT_EC_STEP_ERROR;
        goto cleanup;
    }

    if (mint_modinv(den, modulus) != 0)
        goto cleanup;
    if (mint_copy_value(slope, numerator) != 0 ||
        mint_mod_positive_inplace(slope, modulus) != 0 ||
        mint_mul(slope, den) != 0 ||
        mint_mod_positive_inplace(slope, modulus) != 0)
        goto cleanup;

    rc = MINT_EC_STEP_OK;

cleanup:
    mint_free(den);
    mint_free(g);
    return (mint_ec_step_result_t)rc;
}

static mint_ec_step_result_t mint_ec_add(const mint_ec_point_t *p,
                                         const mint_ec_point_t *q,
                                         const mint_t *a,
                                         const mint_t *modulus,
                                         mint_ec_point_t *out)
{
    mint_t *tmp = NULL;
    mint_t *num = NULL;
    mint_t *den = NULL;
    mint_t *slope = NULL;
    mint_t *x3 = NULL;
    mint_t *y3 = NULL;
    mint_ec_step_result_t rc = MINT_EC_STEP_ERROR;

    if (!p || !q || !a || !modulus || !out)
        return MINT_EC_STEP_ERROR;
    if (p->infinity)
        return mint_ec_point_copy(out, q) == 0 ? MINT_EC_STEP_OK : MINT_EC_STEP_ERROR;
    if (q->infinity)
        return mint_ec_point_copy(out, p) == 0 ? MINT_EC_STEP_OK : MINT_EC_STEP_ERROR;

    tmp = mint_dup_value(p->x);
    if (!tmp)
        goto cleanup;
    if (mint_sub(tmp, q->x) != 0 || mint_mod_positive_inplace(tmp, modulus) != 0)
        goto cleanup;
    if (mint_is_zero(tmp)) {
        if (mint_copy_value(tmp, p->y) != 0 ||
            mint_add(tmp, q->y) != 0 ||
            mint_mod_positive_inplace(tmp, modulus) != 0)
            goto cleanup;
        if (mint_is_zero(tmp)) {
            out->infinity = 1;
            rc = MINT_EC_STEP_INFINITY;
            goto cleanup;
        }
    }

    num = mint_new();
    den = mint_new();
    slope = mint_new();
    x3 = mint_new();
    y3 = mint_new();
    if (!num || !den || !slope || !x3 || !y3)
        goto cleanup;

    if (mint_cmp(p->x, q->x) == 0 && mint_cmp(p->y, q->y) == 0) {
        if (mint_copy_value(den, p->y) != 0 ||
            mint_mul_long(den, 2) != 0 ||
            mint_mod_positive_inplace(den, modulus) != 0)
            goto cleanup;
        if (mint_is_zero(den)) {
            out->infinity = 1;
            rc = MINT_EC_STEP_INFINITY;
            goto cleanup;
        }

        if (mint_copy_value(num, p->x) != 0 ||
            mint_square(num) != 0 ||
            mint_mul_long(num, 3) != 0 ||
            mint_add(num, a) != 0 ||
            mint_mod_positive_inplace(num, modulus) != 0)
            goto cleanup;
    } else {
        if (mint_copy_value(num, q->y) != 0 ||
            mint_sub(num, p->y) != 0 ||
            mint_mod_positive_inplace(num, modulus) != 0 ||
            mint_copy_value(den, q->x) != 0 ||
            mint_sub(den, p->x) != 0 ||
            mint_mod_positive_inplace(den, modulus) != 0)
            goto cleanup;
    }

    rc = mint_ec_make_slope(num, den, modulus, slope);
    if (rc != MINT_EC_STEP_OK)
        goto cleanup;

    if (mint_copy_value(x3, slope) != 0 ||
        mint_square(x3) != 0 ||
        mint_sub(x3, p->x) != 0 ||
        mint_sub(x3, q->x) != 0 ||
        mint_mod_positive_inplace(x3, modulus) != 0)
        goto cleanup;

    if (mint_copy_value(y3, p->x) != 0 ||
        mint_sub(y3, x3) != 0 ||
        mint_mod_positive_inplace(y3, modulus) != 0 ||
        mint_mul(y3, slope) != 0 ||
        mint_sub(y3, p->y) != 0 ||
        mint_mod_positive_inplace(y3, modulus) != 0)
        goto cleanup;

    if (mint_copy_value(out->x, x3) != 0 ||
        mint_copy_value(out->y, y3) != 0)
        goto cleanup;
    out->infinity = 0;
    rc = MINT_EC_STEP_OK;

cleanup:
    mint_free(tmp);
    mint_free(num);
    mint_free(den);
    mint_free(slope);
    mint_free(x3);
    mint_free(y3);
    return rc;
}

static mint_ec_step_result_t mint_ec_scalar_mul(const mint_ec_point_t *point,
                                                unsigned long scalar,
                                                const mint_t *a,
                                                const mint_t *modulus,
                                                mint_ec_point_t *out)
{
    mint_ec_point_t acc = { 0 };
    mint_ec_point_t cur = { 0 };
    mint_ec_point_t next = { 0 };
    mint_ec_step_result_t rc = MINT_EC_STEP_ERROR;

    if (!point || !a || !modulus || !out)
        return MINT_EC_STEP_ERROR;
    if (mint_ec_point_init(&acc) != 0 ||
        mint_ec_point_init(&cur) != 0 ||
        mint_ec_point_init(&next) != 0) {
        mint_ec_point_clear(&acc);
        mint_ec_point_clear(&cur);
        mint_ec_point_clear(&next);
        return MINT_EC_STEP_ERROR;
    }

    acc.infinity = 1;
    if (mint_ec_point_copy(&cur, point) != 0)
        goto cleanup;

    while (scalar > 0) {
        if (scalar & 1ul) {
            rc = mint_ec_add(&acc, &cur, a, modulus, &next);
            if (rc == MINT_EC_STEP_COMPOSITE || rc == MINT_EC_STEP_ERROR)
                goto cleanup;
            mint_ec_point_clear(&acc);
            if (mint_ec_point_init(&acc) != 0 || mint_ec_point_copy(&acc, &next) != 0)
                goto cleanup;
            mint_ec_point_clear(&next);
            if (mint_ec_point_init(&next) != 0)
                goto cleanup;
        }

        scalar >>= 1;
        if (scalar == 0)
            break;

        rc = mint_ec_add(&cur, &cur, a, modulus, &next);
        if (rc == MINT_EC_STEP_COMPOSITE || rc == MINT_EC_STEP_ERROR)
            goto cleanup;
        mint_ec_point_clear(&cur);
        if (mint_ec_point_init(&cur) != 0 || mint_ec_point_copy(&cur, &next) != 0)
            goto cleanup;
        mint_ec_point_clear(&next);
        if (mint_ec_point_init(&next) != 0)
            goto cleanup;
    }

    if (mint_ec_point_copy(out, &acc) != 0)
        goto cleanup;
    rc = MINT_EC_STEP_OK;

cleanup:
    mint_ec_point_clear(&acc);
    mint_ec_point_clear(&cur);
    mint_ec_point_clear(&next);
    return rc;
}

static mint_ec_step_result_t mint_ec_scalar_mul_big(const mint_ec_point_t *point,
                                                    const mint_t *scalar,
                                                    const mint_t *a,
                                                    const mint_t *modulus,
                                                    mint_ec_point_t *out)
{
    mint_ec_point_t acc = { 0 };
    mint_ec_point_t cur = { 0 };
    mint_ec_point_t next = { 0 };
    mint_ec_step_result_t rc = MINT_EC_STEP_ERROR;
    size_t bitlen;
    size_t i;

    if (!point || !scalar || !a || !modulus || !out || scalar->sign < 0)
        return MINT_EC_STEP_ERROR;
    if (mint_ec_point_init(&acc) != 0 ||
        mint_ec_point_init(&cur) != 0 ||
        mint_ec_point_init(&next) != 0) {
        mint_ec_point_clear(&acc);
        mint_ec_point_clear(&cur);
        mint_ec_point_clear(&next);
        return MINT_EC_STEP_ERROR;
    }

    acc.infinity = 1;
    if (mint_ec_point_copy(&cur, point) != 0)
        goto cleanup;

    bitlen = mint_bit_length_internal(scalar);
    for (i = 0; i < bitlen; ++i) {
        if (mint_get_bit(scalar, i)) {
            rc = mint_ec_add(&acc, &cur, a, modulus, &next);
            if (rc == MINT_EC_STEP_COMPOSITE || rc == MINT_EC_STEP_ERROR)
                goto cleanup;
            mint_ec_point_clear(&acc);
            if (mint_ec_point_init(&acc) != 0 || mint_ec_point_copy(&acc, &next) != 0)
                goto cleanup;
            mint_ec_point_clear(&next);
            if (mint_ec_point_init(&next) != 0)
                goto cleanup;
        }

        if (i + 1 == bitlen)
            break;

        rc = mint_ec_add(&cur, &cur, a, modulus, &next);
        if (rc == MINT_EC_STEP_COMPOSITE || rc == MINT_EC_STEP_ERROR)
            goto cleanup;
        mint_ec_point_clear(&cur);
        if (mint_ec_point_init(&cur) != 0 || mint_ec_point_copy(&cur, &next) != 0)
            goto cleanup;
        mint_ec_point_clear(&next);
        if (mint_ec_point_init(&next) != 0)
            goto cleanup;
    }

    if (mint_ec_point_copy(out, &acc) != 0)
        goto cleanup;
    rc = MINT_EC_STEP_OK;

cleanup:
    mint_ec_point_clear(&acc);
    mint_ec_point_clear(&cur);
    mint_ec_point_clear(&next);
    return rc;
}

static int mint_abs_diff(mint_t *dst, const mint_t *a, const mint_t *b)
{
    const mint_t *larger;
    const mint_t *smaller;

    if (!dst || !a || !b || mint_is_immortal(dst))
        return -1;

    if (mint_cmp(a, b) >= 0) {
        larger = a;
        smaller = b;
    } else {
        larger = b;
        smaller = a;
    }

    if (mint_copy_value(dst, larger) != 0)
        return -1;
    dst->sign = mint_is_zero_internal(dst) ? 0 : 1;
    return mint_sub(dst, smaller);
}

static int mint_copy_value(mint_t *dst, const mint_t *src)
{
    if (!dst || !src)
        return -1;
    if (mint_is_immortal(dst))
        return -1;

    if (src->length == 0) {
        mint_clear(dst);
        return 0;
    }

    if (mint_ensure_capacity(dst, src->length) != 0)
        return -1;

    memcpy(dst->storage, src->storage, src->length * sizeof(*src->storage));
    dst->length = src->length;
    dst->sign = src->sign;
    return 0;
}

static mint_t *mint_dup_value(const mint_t *src)
{
    mint_t *copy;

    if (!src)
        return NULL;

    copy = mint_new();
    if (!copy)
        return NULL;
    if (mint_copy_value(copy, src) != 0) {
        mint_free(copy);
        return NULL;
    }
    return copy;
}

static size_t mint_bit_length_internal(const mint_t *mint)
{
    uint64_t top;
    size_t bits;

    if (mint_is_zero_internal(mint))
        return 0;

    top = mint->storage[mint->length - 1];
    bits = (mint->length - 1) * 64;
    while (top != 0) {
        bits++;
        top >>= 1;
    }
    return bits;
}

static int mint_get_bit(const mint_t *mint, size_t bit_index)
{
    size_t limb = bit_index / 64;
    unsigned shift = (unsigned)(bit_index % 64);

    if (!mint || limb >= mint->length)
        return 0;
    return (int)((mint->storage[limb] >> shift) & 1u);
}

static __uint128_t mint_extract_top_bits(const mint_t *mint, size_t prefix_bits)
{
    __uint128_t prefix = 0;
    size_t bitlen, start, i;

    if (!mint || prefix_bits == 0)
        return 0;

    bitlen = mint_bit_length_internal(mint);
    if (bitlen == 0)
        return 0;
    if (prefix_bits > bitlen)
        prefix_bits = bitlen;

    start = bitlen - prefix_bits;
    for (i = 0; i < prefix_bits; ++i) {
        prefix <<= 1;
        prefix |= (unsigned)mint_get_bit(mint, start + (prefix_bits - 1 - i));
    }
    return prefix;
}

static uint64_t mint_isqrt_u128(__uint128_t value)
{
    uint64_t lo = 0, hi = UINT64_MAX;
    uint64_t best = 0;

    while (lo <= hi) {
        uint64_t mid = lo + ((hi - lo) >> 1);
        __uint128_t sq = (__uint128_t)mid * mid;

        if (sq <= value) {
            best = mid;
            if (mid == UINT64_MAX)
                break;
            lo = mid + 1;
        } else {
            if (mid == 0)
                break;
            hi = mid - 1;
        }
    }

    return best;
}

static int mint_set_bit_internal(mint_t *mint, size_t bit_index)
{
    size_t limb = bit_index / 64;
    unsigned shift = (unsigned)(bit_index % 64);

    if (!mint)
        return -1;
    if (mint_ensure_capacity(mint, limb + 1) != 0)
        return -1;
    while (mint->length < limb + 1)
        mint->storage[mint->length++] = 0;
    mint->storage[limb] |= ((uint64_t)1u << shift);
    if (mint->sign == 0)
        mint->sign = 1;
    return 0;
}

static void mint_keep_low_bits(mint_t *mint, size_t bits)
{
    size_t keep_limbs;
    unsigned rem_bits;

    if (!mint)
        return;

    keep_limbs = (bits + 63u) / 64u;
    rem_bits = (unsigned)(bits % 64u);

    if (mint->length > keep_limbs)
        mint->length = keep_limbs;
    if (rem_bits != 0 && mint->length == keep_limbs)
        mint->storage[keep_limbs - 1] &= ((((uint64_t)1u) << rem_bits) - 1u);

    mint_normalise(mint);
    if (!mint_is_zero_internal(mint))
        mint->sign = 1;
}

static int mint_shift_right_bits_to(const mint_t *src, size_t bits, mint_t *dst)
{
    size_t limb_shift, out_len, i;
    unsigned rem_bits;

    if (!src || !dst)
        return -1;

    if (mint_is_zero_internal(src)) {
        mint_clear(dst);
        return 0;
    }

    limb_shift = bits / 64u;
    rem_bits = (unsigned)(bits % 64u);
    if (limb_shift >= src->length) {
        mint_clear(dst);
        return 0;
    }

    out_len = src->length - limb_shift;
    if (mint_ensure_capacity(dst, out_len) != 0)
        return -1;
    memset(dst->storage, 0, out_len * sizeof(*dst->storage));

    if (rem_bits == 0) {
        memcpy(dst->storage, src->storage + limb_shift,
               out_len * sizeof(*dst->storage));
        dst->length = out_len;
    } else {
        for (i = 0; i < out_len; ++i) {
            uint64_t lo = src->storage[limb_shift + i] >> rem_bits;
            uint64_t hi = 0;

            if (limb_shift + i + 1 < src->length)
                hi = src->storage[limb_shift + i + 1] << (64u - rem_bits);
            dst->storage[i] = lo | hi;
        }
        dst->length = out_len;
    }

    dst->sign = 1;
    mint_normalise(dst);
    return 0;
}

static int mint_mod_mersenne_with_scratch(mint_t *mint, size_t exponent,
                                          const mint_t *modulus, mint_t *scratch)
{
    if (!mint || !modulus || !scratch || modulus->sign <= 0)
        return -1;

    while (mint_bit_length_internal(mint) > exponent) {
        if (mint_shift_right_bits_to(mint, exponent, scratch) != 0)
            return -1;
        mint_keep_low_bits(mint, exponent);
        if (mint_abs_add_inplace(mint, scratch) != 0)
            return -1;
        mint->sign = mint_is_zero_internal(mint) ? 0 : 1;
    }

    while (mint_cmp_abs(mint, modulus) >= 0)
        mint_abs_sub_inplace(mint, modulus);

    if (!mint_is_zero_internal(mint))
        mint->sign = 1;
    return 0;
}

static int mint_div_abs(const mint_t *numerator, const mint_t *denominator,
                        mint_t *quotient, mint_t *remainder)
{
    size_t bits, i;

    if (!numerator || !denominator || !quotient || !remainder)
        return -1;
    if (mint_is_zero_internal(denominator))
        return -1;

    mint_clear(quotient);
    mint_clear(remainder);

    if (mint_cmp_abs(numerator, denominator) < 0) {
        if (mint_copy_value(remainder, numerator) != 0)
            return -1;
        remainder->sign = remainder->length == 0 ? 0 : 1;
        return 0;
    }

    bits = mint_bit_length_internal(numerator);
    for (i = bits; i > 0; --i) {
        if (mint_shift_left_one(remainder) != 0)
            return -1;
        if (mint_get_bit(numerator, i - 1)) {
            if (mint_add_small(remainder, 1) != 0)
                return -1;
        }
        if (mint_cmp_abs(remainder, denominator) >= 0) {
            mint_abs_sub_inplace(remainder, denominator);
            if (mint_set_bit_internal(quotient, i - 1) != 0)
                return -1;
        }
    }

    mint_normalise(quotient);
    mint_normalise(remainder);
    if (!mint_is_zero_internal(quotient))
        quotient->sign = 1;
    if (!mint_is_zero_internal(remainder))
        remainder->sign = 1;
    return 0;
}

static int mint_sqrt_initial_guess(mint_t *guess, const mint_t *value)
{
    size_t bitlen;
    size_t prefix_bits;
    long shift_bits;
    __uint128_t prefix;
    uint64_t root;

    bitlen = mint_bit_length_internal(value);
    if (bitlen == 0) {
        mint_clear(guess);
        return 0;
    }

    prefix_bits = bitlen < 128 ? bitlen : 128;
    if (((bitlen - prefix_bits) & 1u) != 0 && prefix_bits > 1)
        prefix_bits--;

    prefix = mint_extract_top_bits(value, prefix_bits);
    root = mint_isqrt_u128(prefix);
    if (root == 0)
        root = 1;

    if (mint_set_magnitude_u64(guess, root, 1) != 0)
        return -1;

    shift_bits = (long)((bitlen - prefix_bits) / 2);
    return mint_shl_inplace(guess, shift_bits);
}

static int mint_get_ulong_if_fits(const mint_t *mint, unsigned long *out)
{
    if (!mint || !out || mint->sign < 0)
        return 0;
    if (mint->length == 0) {
        *out = 0;
        return 1;
    }
    if (mint->length > 1)
        return 0;
    if (mint->storage[0] > ULONG_MAX)
        return 0;
    *out = (unsigned long)mint->storage[0];
    return 1;
}

static int mint_isprime_sieved_upto_ulong(const mint_t *mint, unsigned long limit)
{
    unsigned long root_limit, i, j, base_count = 0;
    unsigned long *base_primes = NULL;
    unsigned char *base_mark = NULL;
    unsigned char *segment = NULL;
    int prime = 1;

    if (limit < 3)
        return 1;

    root_limit = 1;
    while ((root_limit + 1) <= limit / (root_limit + 1))
        root_limit++;

    if (root_limit >= 3) {
        unsigned long odd_count = ((root_limit - 3) / 2) + 1;

        base_mark = calloc(odd_count, sizeof(*base_mark));
        base_primes = malloc((odd_count + 1) * sizeof(*base_primes));
        if (!base_mark || !base_primes) {
            free(base_mark);
            free(base_primes);
            return -1;
        }

        for (i = 0; i < odd_count; ++i) {
            unsigned long p = 2 * i + 3;

            if (base_mark[i])
                continue;
            base_primes[base_count++] = p;
            if (p > root_limit / p)
                continue;
            for (j = (p * p - 3) / 2; j < odd_count; j += p)
                base_mark[j] = 1;
        }
    }

    segment = malloc(MINT_SIEVE_SEGMENT_ODDS);
    if (!segment) {
        free(base_mark);
        free(base_primes);
        return -1;
    }

    for (unsigned long low = 3; low <= limit; ) {
        unsigned long high = low + 2 * (MINT_SIEVE_SEGMENT_ODDS - 1);
        size_t seg_count;

        if (high > limit)
            high = limit;
        seg_count = (size_t)(((high - low) / 2) + 1);
        memset(segment, 0, seg_count);

        for (i = 0; i < base_count; ++i) {
            unsigned long p = base_primes[i];
            unsigned long start;

            if (p > high / p)
                break;

            start = p * p;
            if (start < low) {
                unsigned long rem = low % p;

                start = rem == 0 ? low : low + (p - rem);
            }
            if ((start & 1ul) == 0)
                start += p;

            for (j = start; j <= high; j += 2 * p)
                segment[(j - low) / 2] = 1;
        }

        for (i = 0; i < seg_count; ++i) {
            unsigned long candidate = low + 2 * i;

            if (segment[i])
                continue;
            if (mint_mod_u64(mint, candidate) == 0) {
                prime = 0;
                goto done;
            }
        }

        if (high >= limit)
            break;
        low = high + 2;
    }

done:
    free(base_mark);
    free(base_primes);
    free(segment);
    return prime;
}

static int mint_detect_power_of_two_exponent(const mint_t *mint, size_t *exponent_out)
{
    size_t i, bit_index = 0;
    int seen = 0;

    if (!mint || !exponent_out || mint->sign <= 0 || mint->length == 0)
        return 0;

    for (i = 0; i < mint->length; ++i) {
        uint64_t limb = mint->storage[i];

        if (limb == 0)
            continue;
        if ((limb & (limb - 1u)) != 0)
            return 0;
        if (seen)
            return 0;
        seen = 1;
        bit_index = i * 64;
        while ((limb & 1u) == 0) {
            limb >>= 1;
            bit_index++;
        }
    }

    if (!seen)
        return 0;

    *exponent_out = bit_index;
    return 1;
}

static int mint_isprime_small_ulong(unsigned long n)
{
    unsigned long d;

    if (n < 2)
        return 0;
    if (n == 2 || n == 3)
        return 1;
    if ((n & 1ul) == 0)
        return 0;

    for (d = 3; d <= n / d; d += 2)
        if ((n % d) == 0)
            return 0;
    return 1;
}

static size_t mint_find_wheel210_index(uint64_t rem)
{
    size_t i;

    for (i = 0; i < sizeof(mint_wheel210_residues) / sizeof(mint_wheel210_residues[0]); ++i) {
        if (mint_wheel210_residues[i] == rem)
            return i;
    }

    return SIZE_MAX;
}

static int mint_adjust_to_next_wheel210(mint_t *mint)
{
    uint64_t rem;
    size_t i;

    if (!mint)
        return -1;
    if (mint_cmp_long(mint, 11) < 0)
        return 0;

    rem = mint_mod_u64(mint, 210);
    for (i = 0; i < sizeof(mint_wheel210_residues) / sizeof(mint_wheel210_residues[0]); ++i) {
        if (rem == mint_wheel210_residues[i])
            return 0;
        if (rem < mint_wheel210_residues[i])
            return mint_add_long(mint, (long)(mint_wheel210_residues[i] - rem));
    }

    return mint_add_long(mint, (long)(210u - rem + mint_wheel210_residues[0]));
}

static int mint_adjust_to_prev_wheel210(mint_t *mint)
{
    uint64_t rem;
    size_t i;
    size_t count = sizeof(mint_wheel210_residues) / sizeof(mint_wheel210_residues[0]);

    if (!mint)
        return -1;
    if (mint_cmp_long(mint, 11) < 0)
        return 0;

    rem = mint_mod_u64(mint, 210);
    for (i = count; i-- > 0;) {
        if (rem == mint_wheel210_residues[i])
            return 0;
        if (rem > mint_wheel210_residues[i])
            return mint_sub_long(mint, (long)(rem - mint_wheel210_residues[i]));
    }

    return mint_sub_long(mint,
                         (long)(rem + (210u - mint_wheel210_residues[count - 1])));
}

static long mint_nextprime_wheel210_step(uint64_t rem)
{
    size_t idx = mint_find_wheel210_index(rem);
    size_t count = sizeof(mint_wheel210_residues) / sizeof(mint_wheel210_residues[0]);

    if (idx == SIZE_MAX)
        return -1;
    if (idx + 1 < count)
        return (long)(mint_wheel210_residues[idx + 1] - mint_wheel210_residues[idx]);

    return (long)(210u - mint_wheel210_residues[idx] + mint_wheel210_residues[0]);
}

static long mint_prevprime_wheel210_step(uint64_t rem)
{
    size_t idx = mint_find_wheel210_index(rem);
    size_t count = sizeof(mint_wheel210_residues) / sizeof(mint_wheel210_residues[0]);

    if (idx == SIZE_MAX)
        return -1;
    if (idx > 0)
        return (long)(mint_wheel210_residues[idx] - mint_wheel210_residues[idx - 1]);

    return (long)(mint_wheel210_residues[0] +
                  (210u - mint_wheel210_residues[count - 1]));
}

static int mint_isprime_lucas_lehmer(const mint_t *mersenne, size_t exponent)
{
    mint_t *state = NULL;
    mint_t *scratch = NULL;
    size_t i;

    if (!mersenne || exponent < 2)
        return 0;
    if (exponent == 2)
        return 1;

    state = mint_create_long(4);
    scratch = mint_new();
    if (!state || !scratch) {
        mint_free(state);
        mint_free(scratch);
        return 0;
    }

    for (i = 0; i < exponent - 2; ++i) {
        if (mint_square(state) != 0 ||
            mint_sub_small(state, 2u) != 0 ||
            mint_mod_mersenne_with_scratch(state, exponent, mersenne, scratch) != 0) {
            mint_free(state);
            mint_free(scratch);
            return 0;
        }
    }

    i = mint_is_zero_internal(state);
    mint_free(state);
    mint_free(scratch);
    return (int)i;
}

static int mint_isprime_strong_probable_prime_base2(const mint_t *mint)
{
    mint_t *d = NULL;
    mint_t *n_minus_one = NULL;
    mint_t *x = NULL;
    size_t s = 0;

    if (!mint || mint->sign <= 0)
        return 0;

    n_minus_one = mint_dup_value(mint);
    if (!n_minus_one)
        return 0;
    if (mint_sub_small(n_minus_one, 1) != 0) {
        mint_free(n_minus_one);
        return 0;
    }

    d = mint_dup_value(n_minus_one);
    if (!d) {
        mint_free(n_minus_one);
        return 0;
    }

    while (mint_is_even_internal(d)) {
        if (mint_shr_inplace(d, 1) != 0) {
            mint_free(d);
            mint_free(n_minus_one);
            return 0;
        }
        s++;
    }

    x = mint_create_long(2);
    if (!x) {
        mint_free(d);
        mint_free(n_minus_one);
        return 0;
    }

    if (mint_powmod_inplace(x, d, mint) != 0) {
        mint_free(x);
        mint_free(d);
        mint_free(n_minus_one);
        return 0;
    }

    if (mint_cmp(x, MNT_ONE) == 0 || mint_cmp(x, n_minus_one) == 0) {
        mint_free(x);
        mint_free(d);
        mint_free(n_minus_one);
        return 1;
    }

    for (size_t r = 1; r < s; ++r) {
        if (mint_square(x) != 0 || mint_mod_inplace(x, mint) != 0) {
            mint_free(x);
            mint_free(d);
            mint_free(n_minus_one);
            return 0;
        }
        if (mint_cmp(x, n_minus_one) == 0) {
            mint_free(x);
            mint_free(d);
            mint_free(n_minus_one);
            return 1;
        }
        if (mint_cmp(x, MNT_ONE) == 0)
            break;
    }

    mint_free(x);
    mint_free(d);
    mint_free(n_minus_one);
    return 0;
}

static int mint_isprime_lucas_selfridge(const mint_t *mint)
{
    long abs_d = 5;
    int sign = 1;
    long D = 0;
    long Q = 0;
    int jacobi = 0;
    mint_t *n_plus_one = NULL, *d = NULL;
    mint_t *U = NULL, *V = NULL, *Qk = NULL;
    size_t s = 0, bitlen, i;
    int result = 0;

    if (!mint || mint->sign <= 0 || mint_is_even_internal(mint))
        return 0;

    for (;;) {
        D = sign * abs_d;
        jacobi = mint_jacobi_small_over_mint(D, mint);
        if (jacobi == -1)
            break;
        if (jacobi == 0)
            return 0;
        if (abs_d > LONG_MAX - 2)
            return 0;
        abs_d += 2;
        sign = -sign;
    }

    Q = (1 - D) / 4;

    n_plus_one = mint_clone(mint);
    d = mint_clone(mint);
    U = mint_create_long(1);
    V = mint_create_long(1);
    Qk = mint_create_long(Q);
    if (!n_plus_one || !d || !U || !V || !Qk)
        goto cleanup;

    if (mint_inc(n_plus_one) != 0)
        goto cleanup;
    if (mint_copy_value(d, n_plus_one) != 0)
        goto cleanup;

    while (mint_is_even_internal(d)) {
        if (mint_shr_inplace(d, 1) != 0)
            goto cleanup;
        s++;
    }

    if (mint_mod_positive_inplace(Qk, mint) != 0)
        goto cleanup;

    bitlen = mint_bit_length_internal(d);
    for (i = bitlen; i-- > 1;) {
        mint_t *u2 = mint_clone(U);
        mint_t *v2 = mint_clone(V);
        mint_t *q2 = mint_clone(Qk);
        mint_t *tmp = NULL;
        mint_t *new_u = NULL;
        mint_t *new_v = NULL;

        if (!u2 || !v2 || !q2)
            goto loop_fail;

        if (mint_mul(u2, V) != 0 || mint_mod_positive_inplace(u2, mint) != 0)
            goto loop_fail;

        if (mint_square(v2) != 0 || mint_mod_positive_inplace(v2, mint) != 0)
            goto loop_fail;
        tmp = mint_clone(Qk);
        if (!tmp)
            goto loop_fail;
        if (mint_mul_long(tmp, 2) != 0 || mint_sub(v2, tmp) != 0 ||
            mint_mod_positive_inplace(v2, mint) != 0)
            goto loop_fail;
        mint_free(tmp);
        tmp = NULL;

        if (mint_square(q2) != 0 || mint_mod_positive_inplace(q2, mint) != 0)
            goto loop_fail;

        mint_free(U);
        mint_free(V);
        mint_free(Qk);
        U = u2;
        V = v2;
        Qk = q2;
        u2 = v2 = q2 = NULL;

        if (mint_get_bit(d, i - 1)) {
            new_u = mint_clone(U);
            new_v = mint_clone(U);

            if (!new_u || !new_v) {
                mint_free(new_u);
                mint_free(new_v);
                goto cleanup;
            }

            if (mint_add(new_u, V) != 0 ||
                mint_mod_positive_inplace(new_u, mint) != 0 ||
                mint_half_mod_odd_inplace(new_u, mint) != 0)
                goto loop_fail2;

            if (mint_mul_long(new_v, D) != 0 ||
                mint_add(new_v, V) != 0 ||
                mint_mod_positive_inplace(new_v, mint) != 0 ||
                mint_half_mod_odd_inplace(new_v, mint) != 0)
                goto loop_fail2;

            if (mint_mul_long(Qk, Q) != 0 ||
                mint_mod_positive_inplace(Qk, mint) != 0)
                goto loop_fail2;

            mint_free(U);
            mint_free(V);
            U = new_u;
            V = new_v;
        }
        continue;

loop_fail2:
        mint_free(new_u);
        mint_free(new_v);
        goto cleanup;

loop_fail:
        mint_free(u2);
        mint_free(v2);
        mint_free(q2);
        mint_free(tmp);
        goto cleanup;
    }

    if (mint_is_zero_internal(U) || mint_is_zero_internal(V)) {
        result = 1;
        goto cleanup;
    }

    for (i = 0; i < s; ++i) {
        mint_t *tmp = mint_clone(Qk);

        if (!tmp)
            goto cleanup;
        if (mint_square(V) != 0 || mint_mod_positive_inplace(V, mint) != 0)
            goto cleanup_loop;
        if (mint_mul_long(tmp, 2) != 0 || mint_sub(V, tmp) != 0 ||
            mint_mod_positive_inplace(V, mint) != 0)
            goto cleanup_loop;
        mint_free(tmp);
        tmp = NULL;

        if (mint_is_zero_internal(V)) {
            result = 1;
            goto cleanup;
        }

        if (mint_square(Qk) != 0 || mint_mod_positive_inplace(Qk, mint) != 0)
            goto cleanup;
        continue;

cleanup_loop:
        mint_free(tmp);
        goto cleanup;
    }

cleanup:
    mint_free(n_plus_one);
    mint_free(d);
    mint_free(U);
    mint_free(V);
    mint_free(Qk);
    return result;
}

static mint_primality_result_t mint_prove_prime_pocklington(const mint_t *mint)
{
    mint_t *n_minus_one = NULL;
    mint_factors_t *factors = NULL;
    mint_t *proven_part = NULL;
    mint_t *proven_sq = NULL;
    mint_t *remaining = NULL;
    size_t i;
    mint_primality_result_t result = MINT_PRIMALITY_UNKNOWN;
    static const unsigned long bases[] = {
        2ul, 3ul, 5ul, 7ul, 11ul, 13ul, 17ul, 19ul,
        23ul, 29ul, 31ul, 37ul, 41ul, 43ul, 47ul
    };

    if (!mint || mint->sign <= 0 || mint_cmp(mint, MNT_ONE) <= 0)
        return MINT_PRIMALITY_COMPOSITE;

    n_minus_one = mint_dup_value(mint);
    if (!n_minus_one)
        goto cleanup;
    if (mint_sub_small(n_minus_one, 1) != 0)
        goto cleanup;

    factors = calloc(1, sizeof(*factors));
    proven_part = mint_create_long(1);
    proven_sq = mint_new();
    remaining = mint_new();
    if (!factors || !proven_part || !proven_sq || !remaining)
        goto cleanup;
    if (mint_collect_proven_factors_partial(n_minus_one, factors, remaining) != 0 ||
        factors->count == 0)
        goto cleanup;
    for (i = 0; i < factors->count; ++i) {
        mint_t *pow = mint_clone(factors->items[i].prime);

        if (!pow)
            goto cleanup;
        if (mint_pow(pow, factors->items[i].exponent) != 0 ||
            mint_mul(proven_part, pow) != 0) {
            mint_free(pow);
            goto cleanup;
        }
        mint_free(pow);
    }

    if (mint_copy_value(proven_sq, proven_part) != 0 ||
        mint_square(proven_sq) != 0 ||
        mint_cmp(proven_sq, mint) <= 0)
        goto cleanup;

    for (i = 0; i < sizeof(bases) / sizeof(bases[0]); ++i) {
        mint_t *a = mint_create_ulong(bases[i]);
        mint_t *check = NULL;
        size_t j;
        int all_good = 1;

        if (!a)
            goto cleanup;
        if (mint_cmp(a, mint) >= 0) {
            mint_free(a);
            continue;
        }

        check = mint_clone(a);
        if (!check) {
            mint_free(a);
            goto cleanup;
        }
        if (mint_powmod(check, n_minus_one, mint) != 0) {
            mint_free(check);
            mint_free(a);
            goto cleanup;
        }
        if (mint_cmp(check, MNT_ONE) != 0) {
            mint_free(check);
            mint_free(a);
            continue;
        }
        mint_free(check);
        check = NULL;

        for (j = 0; j < factors->count; ++j) {
            mint_t *exp = mint_clone(n_minus_one);
            mint_t *term = NULL;
            mint_t *g = NULL;

            if (!exp) {
                mint_free(a);
                goto cleanup;
            }
            if (mint_div(exp, factors->items[j].prime, NULL) != 0) {
                mint_free(exp);
                mint_free(a);
                goto cleanup;
            }

            term = mint_clone(a);
            g = mint_new();
            if (!term || !g) {
                mint_free(exp);
                mint_free(term);
                mint_free(g);
                mint_free(a);
                goto cleanup;
            }
            if (mint_powmod(term, exp, mint) != 0 ||
                mint_sub_long(term, 1) != 0 ||
                mint_copy_value(g, term) != 0 ||
                mint_gcd(g, mint) != 0) {
                mint_free(exp);
                mint_free(term);
                mint_free(g);
                mint_free(a);
                goto cleanup;
            }
            if (mint_cmp(g, MNT_ONE) != 0)
                all_good = 0;

            mint_free(exp);
            mint_free(term);
            mint_free(g);
            if (!all_good)
                break;
        }

        mint_free(a);
        if (all_good) {
            result = MINT_PRIMALITY_PRIME;
            goto cleanup;
        }
    }

cleanup:
    mint_free(n_minus_one);
    mint_factors_free(factors);
    mint_free(proven_part);
    mint_free(proven_sq);
    mint_free(remaining);
    return result;
}

static mint_primality_result_t mint_prove_prime_ec_nplus1_supersingular(const mint_t *mint)
{
    static const struct {
        long x;
        long y;
    } seeds[] = {
        { 0, 1 }, { 0, -1 }, { 2, 3 }, { 2, -3 }
    };
    mint_t *n_plus_one = NULL;
    mint_t *bound = NULL;
    mint_t *remaining = NULL;
    mint_factors_t *factors = NULL;
    size_t i, j;

    if (!mint || mint->sign <= 0 || mint_cmp(mint, MNT_ONE) <= 0)
        return MINT_PRIMALITY_COMPOSITE;
    if (mint_mod_u64(mint, 3u) != 2u)
        return MINT_PRIMALITY_UNKNOWN;

    n_plus_one = mint_dup_value(mint);
    bound = mint_new();
    remaining = mint_new();
    factors = calloc(1, sizeof(*factors));
    if (!n_plus_one || !bound || !remaining || !factors)
        goto cleanup;
    if (mint_add_small(n_plus_one, 1) != 0 ||
        mint_copy_value(bound, mint) != 0 ||
        mint_sqrt(bound) != 0 ||
        mint_sqrt(bound) != 0 ||
        mint_add_small(bound, 1) != 0 ||
        mint_square(bound) != 0)
        goto cleanup;
    if (mint_collect_proven_factors_partial(n_plus_one, factors, remaining) != 0)
        goto cleanup;

    for (i = 0; i < factors->count; ++i) {
        mint_t *q = factors->items[i].prime;

        if (mint_cmp(q, bound) <= 0)
            continue;

        for (j = 0; j < sizeof(seeds) / sizeof(seeds[0]); ++j) {
            mint_t *a = mint_new();
            mint_t *disc = mint_new();
            mint_t *g = mint_new();
            mint_t *h = mint_dup_value(n_plus_one);
            mint_ec_point_t p = { 0 };
            mint_ec_point_t r = { 0 };
            mint_ec_point_t s = { 0 };
            mint_primality_result_t result = MINT_PRIMALITY_UNKNOWN;
            mint_ec_step_result_t step;

            if (!a || !disc || !g || !h)
                goto stage_cleanup;
            if (mint_ec_point_init(&p) != 0 ||
                mint_ec_point_init(&r) != 0 ||
                mint_ec_point_init(&s) != 0)
                goto stage_cleanup;

            if (mint_set_long(a, 0) != 0 ||
                mint_set_long(disc, 27) != 0 ||
                mint_copy_value(g, disc) != 0 ||
                mint_gcd(g, mint) != 0)
                goto stage_cleanup;
            if (mint_cmp(g, MNT_ONE) != 0) {
                result = mint_cmp(g, mint) < 0
                    ? MINT_PRIMALITY_COMPOSITE
                    : MINT_PRIMALITY_UNKNOWN;
                goto stage_cleanup;
            }

            if (mint_set_long(p.x, seeds[j].x) != 0 ||
                mint_set_long(p.y, seeds[j].y) != 0 ||
                mint_mod_positive_inplace(p.x, mint) != 0 ||
                mint_mod_positive_inplace(p.y, mint) != 0)
                goto stage_cleanup;
            p.infinity = 0;

            if (mint_div(h, q, NULL) != 0)
                goto stage_cleanup;

            step = mint_ec_scalar_mul_big(&p, h, a, mint, &r);
            if (step == MINT_EC_STEP_COMPOSITE) {
                result = MINT_PRIMALITY_COMPOSITE;
                goto stage_cleanup;
            }
            if (step != MINT_EC_STEP_OK || r.infinity)
                goto stage_cleanup;

            step = mint_ec_scalar_mul_big(&r, q, a, mint, &s);
            if (step == MINT_EC_STEP_COMPOSITE) {
                result = MINT_PRIMALITY_COMPOSITE;
                goto stage_cleanup;
            }
            if (step != MINT_EC_STEP_OK)
                goto stage_cleanup;

            if (s.infinity) {
                result = MINT_PRIMALITY_PRIME;
                goto stage_cleanup;
            }

stage_cleanup:
            mint_free(a);
            mint_free(disc);
            mint_free(g);
            mint_free(h);
            mint_ec_point_clear(&p);
            mint_ec_point_clear(&r);
            mint_ec_point_clear(&s);
            if (result != MINT_PRIMALITY_UNKNOWN) {
                mint_free(n_plus_one);
                mint_free(bound);
                mint_free(remaining);
                mint_factors_free(factors);
                return result;
            }
        }
    }

cleanup:
    mint_free(n_plus_one);
    mint_free(bound);
    mint_free(remaining);
    mint_factors_free(factors);
    return MINT_PRIMALITY_UNKNOWN;
}

static mint_primality_result_t mint_prove_prime_ec_witness(const mint_t *mint)
{
    static const long a_values[] = { 1, 2, 3, 5, 7, 11, 13 };
    static const struct {
        long x;
        long y;
    } seeds[] = {
        { 0, 1 }, { 1, 1 }, { 2, 1 }, { 3, 1 }, { 5, 1 }, { 7, 1 }, { 2, 3 }
    };
    static const unsigned long scalars[] = {
        2ul, 3ul, 5ul, 7ul, 11ul, 13ul, 17ul, 19ul, 23ul, 29ul, 31ul
    };
    size_t i, j, k;

    if (!mint || mint->sign <= 0 || mint_cmp(mint, MNT_ONE) <= 0)
        return MINT_PRIMALITY_COMPOSITE;

    for (i = 0; i < sizeof(a_values) / sizeof(a_values[0]); ++i) {
        for (j = 0; j < sizeof(seeds) / sizeof(seeds[0]); ++j) {
            mint_t *a = mint_create_long(a_values[i]);
            mint_t *b = NULL;
            mint_t *disc = NULL;
            mint_t *g = NULL;
            mint_t *xv = NULL;
            mint_ec_point_t p = { 0 };
            mint_ec_point_t q = { 0 };
            mint_primality_result_t result = MINT_PRIMALITY_UNKNOWN;

            if (!a)
                return MINT_PRIMALITY_UNKNOWN;
            if (mint_mod_positive_inplace(a, mint) != 0)
                goto ec_cleanup;

            b = mint_create_long(seeds[j].y);
            disc = mint_create_long(seeds[j].x);
            g = mint_new();
            xv = mint_create_long(seeds[j].x);
            if (!b || !disc || !g || !xv)
                goto ec_cleanup;

            if (mint_square(b) != 0 ||
                mint_pow(disc, 3) != 0 ||
                mint_mul_long(disc, -1) != 0)
                goto ec_cleanup;

            if (mint_mul(xv, a) != 0 ||
                mint_sub(b, xv) != 0 ||
                mint_sub(b, disc) != 0)
                goto ec_cleanup;

            if (mint_mod_positive_inplace(b, mint) != 0)
                goto ec_cleanup;

            if (mint_copy_value(disc, a) != 0 ||
                mint_pow(disc, 3) != 0 ||
                mint_mul_long(disc, 4) != 0)
                goto ec_cleanup;
            if (mint_copy_value(g, b) != 0 ||
                mint_square(g) != 0 ||
                mint_mul_long(g, 27) != 0 ||
                mint_add(disc, g) != 0 ||
                mint_mod_positive_inplace(disc, mint) != 0 ||
                mint_copy_value(g, disc) != 0 ||
                mint_gcd(g, mint) != 0)
                goto ec_cleanup;

            if (mint_cmp(g, MNT_ONE) != 0) {
                result = mint_cmp(g, mint) < 0
                    ? MINT_PRIMALITY_COMPOSITE
                    : MINT_PRIMALITY_UNKNOWN;
                if (result != MINT_PRIMALITY_UNKNOWN)
                    goto ec_cleanup;
                goto ec_cleanup;
            }

            if (mint_ec_point_init(&p) != 0 || mint_ec_point_init(&q) != 0)
                goto ec_cleanup;
            if (mint_set_long(p.x, seeds[j].x) != 0 ||
                mint_set_long(p.y, seeds[j].y) != 0 ||
                mint_mod_positive_inplace(p.x, mint) != 0 ||
                mint_mod_positive_inplace(p.y, mint) != 0)
                goto ec_cleanup;
            p.infinity = 0;

            for (k = 0; k < sizeof(scalars) / sizeof(scalars[0]); ++k) {
                mint_ec_step_result_t step =
                    mint_ec_scalar_mul(&p, scalars[k], a, mint, &q);

                if (step == MINT_EC_STEP_COMPOSITE) {
                    result = MINT_PRIMALITY_COMPOSITE;
                    goto ec_cleanup;
                }
                if (step == MINT_EC_STEP_ERROR)
                    break;
                if (mint_ec_point_copy(&p, &q) != 0)
                    break;
            }

ec_cleanup:
            mint_free(a);
            mint_free(b);
            mint_free(disc);
            mint_free(g);
            mint_free(xv);
            mint_ec_point_clear(&p);
            mint_ec_point_clear(&q);
            if (result != MINT_PRIMALITY_UNKNOWN)
                return result;
        }
    }

    return MINT_PRIMALITY_UNKNOWN;
}

static int mint_collect_proven_factors_partial(const mint_t *n,
                                               mint_factors_t *proven,
                                               mint_t *remaining)
{
    mint_t *work = NULL;
    mint_t *factor = NULL;
    mint_t *other = NULL;
    mint_t *rem_factor = NULL;
    mint_t *rem_other = NULL;
    size_t i;

    if (!n || !proven || !remaining)
        return -1;
    if (mint_set_long(remaining, 1) != 0)
        return -1;
    if (mint_cmp(n, MNT_ONE) <= 0)
        return 0;

    work = mint_dup_value(n);
    if (!work)
        goto fail;
    work->sign = mint_is_zero_internal(work) ? 0 : 1;

    for (i = 0; i < sizeof(mint_small_primes) / sizeof(mint_small_primes[0]); ++i) {
        unsigned long exponent = 0;
        mint_t *prime = mint_create_ulong(mint_small_primes[i]);

        if (!prime)
            goto fail;

        while (mint_cmp(work, prime) >= 0) {
            mint_t *rem = mint_clone(work);
            int divisible;

            if (!rem) {
                mint_free(prime);
                goto fail;
            }
            if (mint_mod(rem, prime) != 0) {
                mint_free(rem);
                mint_free(prime);
                goto fail;
            }
            divisible = mint_is_zero(rem);
            mint_free(rem);
            if (!divisible)
                break;
            if (mint_div(work, prime, NULL) != 0) {
                mint_free(prime);
                goto fail;
            }
            exponent++;
        }

        if (exponent > 0 && mint_factors_append(proven, prime, exponent) != 0) {
            mint_free(prime);
            goto fail;
        }
        mint_free(prime);
    }

    if (mint_cmp(work, MNT_ONE) <= 0) {
        mint_free(work);
        return 0;
    }

    if (mint_prove_prime_internal(work) == MINT_PRIMALITY_PRIME) {
        int rc = mint_factors_append(proven, work, 1);

        mint_free(work);
        return rc;
    }

    factor = mint_new();
    other = mint_clone(work);
    rem_factor = mint_new();
    rem_other = mint_new();
    if (!factor || !other || !rem_factor || !rem_other)
        goto fail;
    if (mint_pollard_rho_factor(work, factor) != 0 ||
        mint_div(other, factor, NULL) != 0)
        goto keep_remaining;

    if (mint_collect_proven_factors_partial(factor, proven, rem_factor) != 0 ||
        mint_collect_proven_factors_partial(other, proven, rem_other) != 0)
        goto fail;
    if (mint_copy_value(remaining, rem_factor) != 0 ||
        mint_mul(remaining, rem_other) != 0)
        goto fail;

    mint_free(work);
    mint_free(factor);
    mint_free(other);
    mint_free(rem_factor);
    mint_free(rem_other);
    return 0;

keep_remaining:
    if (mint_copy_value(remaining, work) != 0)
        goto fail;
    mint_free(work);
    mint_free(factor);
    mint_free(other);
    mint_free(rem_factor);
    mint_free(rem_other);
    return 0;

fail:
    mint_free(work);
    mint_free(factor);
    mint_free(other);
    mint_free(rem_factor);
    mint_free(rem_other);
    return -1;
}

static mint_primality_result_t mint_prove_prime_internal(const mint_t *mint)
{
    unsigned long ulong_limit;
    size_t mersenne_exponent;
    size_t i;
    uint64_t rem;

    if (!mint || mint->sign <= 0)
        return MINT_PRIMALITY_COMPOSITE;
    if (mint_cmp(mint, MNT_ONE) <= 0)
        return MINT_PRIMALITY_COMPOSITE;
    if (mint->length == 1) {
        uint64_t n = mint->storage[0];

        if (n == 2 || n == 3)
            return MINT_PRIMALITY_PRIME;
    }
    if (mint_is_even_internal(mint))
        return MINT_PRIMALITY_COMPOSITE;

    for (i = 0; i < sizeof(mint_small_primes) / sizeof(mint_small_primes[0]); ++i) {
        if (mint->length == 1 && mint->storage[0] == mint_small_primes[i])
            return MINT_PRIMALITY_PRIME;
        rem = mint_mod_u64(mint, mint_small_primes[i]);
        if (rem == 0)
            return MINT_PRIMALITY_COMPOSITE;
    }

    if (mint_get_ulong_if_fits(mint, &ulong_limit)) {
        unsigned long root_limit = 1;

        while ((root_limit + 1) <= ulong_limit / (root_limit + 1))
            root_limit++;
        return mint_isprime_sieved_upto_ulong(mint, root_limit) > 0
            ? MINT_PRIMALITY_PRIME
            : MINT_PRIMALITY_COMPOSITE;
    }

    {
        mint_t *plus_one = mint_dup_value(mint);

        if (!plus_one)
            return MINT_PRIMALITY_UNKNOWN;
        if (mint_add_small(plus_one, 1) != 0) {
            mint_free(plus_one);
            return MINT_PRIMALITY_UNKNOWN;
        }
        if (mint_detect_power_of_two_exponent(plus_one, &mersenne_exponent) &&
            mint_isprime_small_ulong((unsigned long)mersenne_exponent)) {
            mint_primality_result_t rr =
                mint_isprime_lucas_lehmer(mint, mersenne_exponent)
                    ? MINT_PRIMALITY_PRIME
                    : MINT_PRIMALITY_COMPOSITE;

            mint_free(plus_one);
            return rr;
        }
        mint_free(plus_one);
    }

    if (mint_isprime_strong_probable_prime_base2(mint) <= 0)
        return MINT_PRIMALITY_COMPOSITE;
    if (mint_isprime_lucas_selfridge(mint) <= 0)
        return MINT_PRIMALITY_COMPOSITE;

    {
        mint_primality_result_t exact = mint_prove_prime_pocklington(mint);

        if (exact != MINT_PRIMALITY_UNKNOWN)
            return exact;
    }

    {
        mint_primality_result_t exact =
            mint_prove_prime_ec_nplus1_supersingular(mint);

        if (exact != MINT_PRIMALITY_UNKNOWN)
            return exact;
    }

    return mint_prove_prime_ec_witness(mint);
}

static int mint_factors_append(mint_factors_t *factors,
                               const mint_t *prime,
                               unsigned long exponent)
{
    mint_factor_t *grown;
    mint_t *prime_copy;

    if (!factors || !prime || exponent == 0)
        return -1;

    grown = realloc(factors->items, (factors->count + 1) * sizeof(*grown));
    if (!grown)
        return -1;
    factors->items = grown;

    prime_copy = mint_dup_value(prime);
    if (!prime_copy)
        return -1;

    factors->items[factors->count].prime = prime_copy;
    factors->items[factors->count].exponent = exponent;
    factors->count++;
    return 0;
}

static int mint_factor_item_cmp(const void *lhs, const void *rhs)
{
    const mint_factor_t *a = (const mint_factor_t *)lhs;
    const mint_factor_t *b = (const mint_factor_t *)rhs;

    return mint_cmp(a->prime, b->prime);
}

static void mint_factors_sort_and_compress(mint_factors_t *factors)
{
    size_t read_idx, write_idx;

    if (!factors || factors->count == 0)
        return;

    qsort(factors->items, factors->count, sizeof(*factors->items),
          mint_factor_item_cmp);

    write_idx = 0;
    for (read_idx = 0; read_idx < factors->count; ++read_idx) {
        if (write_idx > 0 &&
            mint_cmp(factors->items[write_idx - 1].prime,
                     factors->items[read_idx].prime) == 0) {
            factors->items[write_idx - 1].exponent +=
                factors->items[read_idx].exponent;
            mint_free(factors->items[read_idx].prime);
            continue;
        }

        if (write_idx != read_idx)
            factors->items[write_idx] = factors->items[read_idx];
        write_idx++;
    }

    factors->count = write_idx;
}

static int mint_pollard_rho_step(mint_t *state, const mint_t *c,
                                 const mint_t *modulus)
{
    if (mint_square(state) != 0)
        return -1;
    if (mint_add(state, c) != 0)
        return -1;
    return mint_mod(state, modulus);
}

static int mint_pollard_rho_factor(const mint_t *n, mint_t *factor)
{
    unsigned long c_value;

    if (!n || !factor || mint_is_immortal(factor) || n->sign <= 0)
        return -1;
    if (mint_is_even(n))
        return mint_set_long(factor, 2);

    for (c_value = 1; c_value <= 16; ++c_value) {
        mint_t *x = mint_create_long(2);
        mint_t *y = mint_create_long(2);
        mint_t *c = mint_create_long((long)c_value);
        mint_t *d = mint_create_long(1);
        mint_t *diff = mint_new();

        if (!x || !y || !c || !d || !diff) {
            mint_free(x);
            mint_free(y);
            mint_free(c);
            mint_free(d);
            mint_free(diff);
            return -1;
        }

        while (mint_cmp(d, MNT_ONE) == 0) {
            if (mint_pollard_rho_step(x, c, n) != 0 ||
                mint_pollard_rho_step(y, c, n) != 0 ||
                mint_pollard_rho_step(y, c, n) != 0 ||
                mint_abs_diff(diff, x, y) != 0 ||
                mint_gcd(diff, n) != 0) {
                mint_free(x);
                mint_free(y);
                mint_free(c);
                mint_free(d);
                mint_free(diff);
                return -1;
            }

            if (mint_copy_value(d, diff) != 0) {
                mint_free(x);
                mint_free(y);
                mint_free(c);
                mint_free(d);
                mint_free(diff);
                return -1;
            }
            d->sign = mint_is_zero_internal(d) ? 0 : 1;
        }

        if (mint_cmp(d, n) != 0 && mint_cmp(d, MNT_ONE) > 0) {
            int rc = mint_copy_value(factor, d);

            if (rc == 0)
                factor->sign = mint_is_zero_internal(factor) ? 0 : 1;
            mint_free(x);
            mint_free(y);
            mint_free(c);
            mint_free(d);
            mint_free(diff);
            return rc;
        }

        mint_free(x);
        mint_free(y);
        mint_free(c);
        mint_free(d);
        mint_free(diff);
    }

    return -1;
}

static int mint_factor_recursive(const mint_t *n, mint_factors_t *factors)
{
    mint_t *work = NULL;
    mint_t *prime = NULL;
    mint_t *factor = NULL;
    mint_t *other = NULL;
    size_t i;

    if (!n || !factors)
        return -1;
    if (mint_cmp(n, MNT_ONE) <= 0)
        return 0;
    if (mint_isprime(n))
        return mint_factors_append(factors, n, 1);

    work = mint_dup_value(n);
    if (!work)
        return -1;
    work->sign = mint_is_zero_internal(work) ? 0 : 1;

    for (i = 0; i < sizeof(mint_small_primes) / sizeof(mint_small_primes[0]); ++i) {
        unsigned long exponent = 0;

        prime = mint_create_ulong(mint_small_primes[i]);
        if (!prime) {
            mint_free(work);
            return -1;
        }

        while (mint_cmp(work, prime) >= 0) {
            mint_t *rem = mint_clone(work);
            int divisible;

            if (!rem) {
                mint_free(prime);
                mint_free(work);
                return -1;
            }
            if (mint_mod(rem, prime) != 0) {
                mint_free(rem);
                mint_free(prime);
                mint_free(work);
                return -1;
            }
            divisible = mint_is_zero(rem);
            mint_free(rem);
            if (!divisible)
                break;
            if (mint_div(work, prime, NULL) != 0) {
                mint_free(prime);
                mint_free(work);
                return -1;
            }
            exponent++;
        }

        if (exponent > 0 &&
            mint_factors_append(factors, prime, exponent) != 0) {
            mint_free(prime);
            mint_free(work);
            return -1;
        }
        mint_free(prime);
        prime = NULL;
    }

    if (mint_cmp(work, MNT_ONE) <= 0) {
        mint_free(work);
        return 0;
    }
    if (mint_isprime(work)) {
        int rc = mint_factors_append(factors, work, 1);

        mint_free(work);
        return rc;
    }

    factor = mint_new();
    if (!factor) {
        mint_free(work);
        return -1;
    }
    if (mint_pollard_rho_factor(work, factor) != 0) {
        mint_free(factor);
        mint_free(work);
        return -1;
    }

    other = mint_clone(work);
    if (!other) {
        mint_free(factor);
        mint_free(work);
        return -1;
    }
    if (mint_div(other, factor, NULL) != 0) {
        mint_free(other);
        mint_free(factor);
        mint_free(work);
        return -1;
    }

    if (mint_factor_recursive(factor, factors) != 0 ||
        mint_factor_recursive(other, factors) != 0) {
        mint_free(other);
        mint_free(factor);
        mint_free(work);
        return -1;
    }

    mint_free(other);
    mint_free(factor);
    mint_free(work);
    return 0;
}

mint_t *mint_new(void)
{
    mint_t *mint = calloc(1, sizeof(*mint));

    if (!mint)
        return NULL;

    mint->sign = 0;
    mint->length = 0;
    mint->capacity = 0;
    mint->storage = NULL;
    return mint;
}

mint_t *mint_create_long(long value)
{
    mint_t *mint = mint_new();

    if (!mint)
        return NULL;

    if (mint_set_long(mint, value) != 0) {
        mint_free(mint);
        return NULL;
    }

    return mint;
}

mint_t *mint_create_ulong(unsigned long value)
{
    mint_t *mint = mint_new();

    if (!mint)
        return NULL;

    if (mint_set_ulong(mint, value) != 0) {
        mint_free(mint);
        return NULL;
    }

    return mint;
}

mint_t *mint_create_2pow(uint64_t n)
{
    mint_t *mint = mint_new();
    size_t limb = (size_t)(n / 64);
    unsigned shift = (unsigned)(n % 64);

    if (!mint)
        return NULL;

    if (mint_ensure_capacity(mint, limb + 1) != 0) {
        mint_free(mint);
        return NULL;
    }

    mint->storage[limb] = ((uint64_t)1u) << shift;
    mint->length = limb + 1;
    mint->sign = 1;
    return mint;
}

mint_t *mint_create_string(const char *text)
{
    mint_t *mint = mint_new();

    if (!mint)
        return NULL;

    if (mint_set_string(mint, text) != 0) {
        mint_free(mint);
        return NULL;
    }

    return mint;
}

mint_t *mint_create_hex(const char *text)
{
    mint_t *mint = mint_new();

    if (!mint)
        return NULL;

    if (mint_set_hex(mint, text) != 0) {
        mint_free(mint);
        return NULL;
    }

    return mint;
}

mint_t *mint_clone(const mint_t *mint)
{
    return mint_dup_value(mint);
}

void mint_clear(mint_t *mint)
{
    if (!mint || mint_is_immortal(mint))
        return;

    free(mint->storage);
    mint->storage = NULL;
    mint->sign = 0;
    mint->length = 0;
    mint->capacity = 0;
}

int mint_inc(mint_t *mint)
{
    if (!mint)
        return -1;
    return mint_add_inplace(mint, MNT_ONE);
}

int mint_dec(mint_t *mint)
{
    if (!mint)
        return -1;
    return mint_sub(mint, MNT_ONE);
}

int mint_add_long(mint_t *mint, long value)
{
    mint_t *tmp;
    int rc;

    if (!mint)
        return -1;
    if (value == 0)
        return 0;

    tmp = mint_create_long(value);
    if (!tmp)
        return -1;

    rc = mint_add(mint, tmp);
    mint_free(tmp);
    return rc;
}

int mint_sub_long(mint_t *mint, long value)
{
    mint_t *tmp;
    int rc;

    if (!mint)
        return -1;
    if (value == 0)
        return 0;

    tmp = mint_create_long(value);
    if (!tmp)
        return -1;

    rc = mint_sub(mint, tmp);
    mint_free(tmp);
    return rc;
}

int mint_mul_long(mint_t *mint, long value)
{
    mint_t *tmp;
    int rc;

    if (!mint)
        return -1;
    if (value == 0) {
        mint_clear(mint);
        return 0;
    }
    if (value == 1)
        return 0;

    tmp = mint_create_long(value);
    if (!tmp)
        return -1;

    rc = mint_mul(mint, tmp);
    mint_free(tmp);
    return rc;
}

int mint_div_long(mint_t *mint, long value, long *rem)
{
    mint_t *tmp;
    mint_t *rem_mint = NULL;
    int rc;

    if (!mint || value == 0)
        return -1;

    tmp = mint_create_long(value);
    if (!tmp)
        return -1;

    if (rem) {
        rem_mint = mint_new();
        if (!rem_mint) {
            mint_free(tmp);
            return -1;
        }
    }

    rc = mint_div(mint, tmp, rem_mint);
    if (rc == 0 && rem) {
        if (!mint_get_long(rem_mint, rem))
            rc = -1;
    }

    mint_free(rem_mint);
    mint_free(tmp);
    return rc;
}

int mint_mod_long(mint_t *mint, long value)
{
    mint_t *tmp;
    int rc;

    if (!mint || value == 0)
        return -1;

    tmp = mint_create_long(value);
    if (!tmp)
        return -1;

    rc = mint_mod(mint, tmp);
    mint_free(tmp);
    return rc;
}

void mint_free(mint_t *mint)
{
    if (!mint || mint_is_immortal(mint))
        return;
    mint_clear(mint);
    free(mint);
}

static int mint_add_inplace(mint_t *mint, const mint_t *other)
{
    mint_t *tmp;
    int cmp;

    if (!mint || !other || mint_is_immortal(mint))
        return -1;
    if (mint_is_zero_internal(other))
        return 0;
    if (mint_is_zero_internal(mint))
        return mint_copy_value(mint, other);

    if (mint->sign == other->sign) {
        if (mint_abs_add_inplace(mint, other) != 0)
            return -1;
        mint->sign = other->sign;
        return 0;
    }

    cmp = mint_cmp_abs(mint, other);
    if (cmp == 0) {
        mint_clear(mint);
        return 0;
    }
    if (cmp > 0) {
        mint_abs_sub_inplace(mint, other);
        return 0;
    }

    tmp = mint_dup_value(other);
    if (!tmp)
        return -1;
    mint_abs_sub_inplace(tmp, mint);
    if (mint_copy_value(mint, tmp) != 0) {
        mint_free(tmp);
        return -1;
    }
    mint->sign = other->sign;
    mint_free(tmp);
    return 0;
}

static int mint_and_inplace(mint_t *mint, const mint_t *other)
{
    size_t i, keep;

    if (!mint || !other || mint_is_immortal(mint))
        return -1;
    if (mint_has_negative_operand(mint, other))
        return -1;
    if (mint_is_zero_internal(mint) || mint_is_zero_internal(other)) {
        mint_clear(mint);
        return 0;
    }

    keep = mint->length < other->length ? mint->length : other->length;
    for (i = 0; i < keep; ++i)
        mint->storage[i] &= other->storage[i];
    for (i = keep; i < mint->length; ++i)
        mint->storage[i] = 0;

    mint_normalise(mint);
    mint_zero_spare_limbs(mint, mint->length);
    return 0;
}

int mint_cmp(const mint_t *a, const mint_t *b)
{
    if (mint_is_zero_internal(a) && mint_is_zero_internal(b))
        return 0;
    if (mint_is_zero_internal(a))
        return b->sign < 0 ? 1 : -1;
    if (mint_is_zero_internal(b))
        return a->sign < 0 ? -1 : 1;
    if (a->sign < b->sign)
        return -1;
    if (a->sign > b->sign)
        return 1;
    if (a->sign < 0)
        return -mint_cmp_abs(a, b);
    return mint_cmp_abs(a, b);
}

int mint_cmp_long(const mint_t *mint, long value)
{
    mint_t *tmp;
    int rc;

    if (!mint)
        return 1;

    tmp = mint_create_long(value);
    if (!tmp)
        return 1;

    rc = mint_cmp(mint, tmp);
    mint_free(tmp);
    return rc;
}

int mint_sub(mint_t *mint, const mint_t *other)
{
    mint_t *tmp;
    int rc;

    if (!mint || !other || mint_is_immortal(mint))
        return -1;
    if (mint_is_zero_internal(other))
        return 0;

    tmp = mint_dup_value(other);
    if (!tmp)
        return -1;
    if (!mint_is_zero_internal(tmp))
        tmp->sign = (short)-tmp->sign;

    rc = mint_add_inplace(mint, tmp);
    mint_free(tmp);
    return rc;
}

int mint_divmod(const mint_t *numerator, const mint_t *denominator,
                mint_t *quotient, mint_t *remainder)
{
    if (!quotient || !remainder || !numerator || !denominator)
        return -1;
    if (quotient == remainder)
        return -1;
    if (mint_is_immortal(quotient) || mint_is_immortal(remainder))
        return -1;
    if (mint_copy_value(quotient, numerator) != 0)
        return -1;
    return mint_div_inplace(quotient, denominator, remainder);
}

int mint_abs(mint_t *mint)
{
    if (!mint || mint_is_immortal(mint))
        return -1;
    if (!mint_is_zero_internal(mint))
        mint->sign = 1;
    return 0;
}

bool mint_is_zero(const mint_t *mint)
{
    return mint_is_zero_internal(mint) != 0;
}

bool mint_is_negative(const mint_t *mint)
{
    return mint && mint->sign < 0;
}

bool mint_is_even(const mint_t *mint)
{
    return mint_is_even_internal(mint) != 0;
}

bool mint_is_odd(const mint_t *mint)
{
    return mint_is_odd_internal(mint) != 0;
}

size_t mint_bit_length(const mint_t *mint)
{
    return mint_bit_length_internal(mint);
}

bool mint_test_bit(const mint_t *mint, size_t bit_index)
{
    if (!mint)
        return false;
    return mint_get_bit(mint, bit_index) != 0;
}

bool mint_fits_long(const mint_t *mint)
{
    unsigned long magnitude;

    if (!mint)
        return false;
    if (mint->length == 0)
        return true;
    if (mint->length > 1)
        return false;
    if (mint->storage[0] > ULONG_MAX)
        return false;

    magnitude = (unsigned long)mint->storage[0];
    if (mint->sign >= 0)
        return magnitude <= (unsigned long)LONG_MAX;
    return magnitude <= ((unsigned long)LONG_MAX + 1ul);
}

bool mint_get_long(const mint_t *mint, long *out)
{
    unsigned long magnitude;

    if (!mint || !out || !mint_fits_long(mint))
        return false;
    if (mint->length == 0) {
        *out = 0;
        return true;
    }

    magnitude = (unsigned long)mint->storage[0];
    if (mint->sign >= 0) {
        *out = (long)magnitude;
    } else if (magnitude == ((unsigned long)LONG_MAX + 1ul)) {
        *out = LONG_MIN;
    } else {
        *out = -(long)magnitude;
    }
    return true;
}

int mint_square(mint_t *mint)
{
    return mint_mul_inplace(mint, mint);
}

mint_factors_t *mint_factors(const mint_t *mint)
{
    mint_factors_t *factors;

    if (!mint || mint->sign <= 0)
        return NULL;

    factors = calloc(1, sizeof(*factors));
    if (!factors)
        return NULL;

    if (mint_factor_recursive(mint, factors) != 0) {
        mint_factors_free(factors);
        return NULL;
    }

    mint_factors_sort_and_compress(factors);
    return factors;
}

void mint_factors_free(mint_factors_t *factors)
{
    size_t i;

    if (!factors)
        return;
    for (i = 0; i < factors->count; ++i)
        mint_free(factors->items[i].prime);
    free(factors->items);
    free(factors);
}

bool mint_isprime(const mint_t *mint)
{
    unsigned long ulong_limit;
    size_t mersenne_exponent;
    size_t i;
    uint64_t rem;

    if (!mint || mint->sign <= 0)
        return 0;
    if (mint_cmp(mint, MNT_ONE) <= 0)
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
            mint_free(plus_one);
            return false;
        }
        if (mint_detect_power_of_two_exponent(plus_one, &mersenne_exponent) &&
            mint_isprime_small_ulong((unsigned long)mersenne_exponent)) {
            is_mersenne_prime = mint_isprime_lucas_lehmer(mint, mersenne_exponent);
            mint_free(plus_one);
            return is_mersenne_prime;
        }
        mint_free(plus_one);
    }

    if (mint_isprime_strong_probable_prime_base2(mint) <= 0)
        return false;

    return mint_isprime_lucas_selfridge(mint) > 0;
}

mint_primality_result_t mint_prove_prime(const mint_t *mint)
{
    return mint_prove_prime_internal(mint);
}

int mint_nextprime(mint_t *mint)
{
    if (!mint || mint_is_immortal(mint))
        return -1;

    if (mint_cmp_long(mint, 2) < 0)
        return mint_set_long(mint, 2);
    if (mint_cmp_long(mint, 2) == 0)
        return 0;
    if (mint_cmp_long(mint, 3) == 0)
        return 0;
    if (mint_cmp_long(mint, 5) < 0)
        return mint_set_long(mint, 5);
    if (mint_cmp_long(mint, 5) == 0)
        return 0;
    if (mint_cmp_long(mint, 7) < 0)
        return mint_set_long(mint, 7);
    if (mint_cmp_long(mint, 7) == 0)
        return 0;

    if (mint_adjust_to_next_wheel210(mint) != 0)
        return -1;

    while (!mint_isprime(mint)) {
        uint64_t rem = mint_mod_u64(mint, 210);
        long step = mint_nextprime_wheel210_step(rem);

        if (step < 0 || mint_add_long(mint, step) != 0)
            return -1;
    }

    return 0;
}

int mint_prevprime(mint_t *mint)
{
    if (!mint || mint_is_immortal(mint))
        return -1;

    if (mint_cmp_long(mint, 2) < 0)
        return -1;
    if (mint_cmp_long(mint, 2) == 0)
        return 0;
    if (mint_cmp_long(mint, 3) == 0)
        return 0;
    if (mint_cmp_long(mint, 5) < 0)
        return mint_set_long(mint, 3);
    if (mint_cmp_long(mint, 5) == 0)
        return 0;
    if (mint_cmp_long(mint, 7) < 0)
        return mint_set_long(mint, 5);
    if (mint_cmp_long(mint, 7) == 0)
        return 0;

    if (mint_adjust_to_prev_wheel210(mint) != 0)
        return -1;

    while (mint_cmp_long(mint, 11) >= 0 && !mint_isprime(mint)) {
        uint64_t rem = mint_mod_u64(mint, 210);
        long step = mint_prevprime_wheel210_step(rem);

        if (step < 0 || mint_sub_long(mint, step) != 0)
            return -1;
    }

    return 0;
}

int mint_gcd(mint_t *mint, const mint_t *other)
{
    mint_t *a, *b, *r;
    int rc = -1;

    if (!mint || !other || mint_is_immortal(mint))
        return -1;

    a = mint_dup_value(mint);
    b = mint_dup_value(other);
    r = mint_new();
    if (!a || !b || !r)
        goto cleanup;

    a->sign = mint_is_zero_internal(a) ? 0 : 1;
    b->sign = mint_is_zero_internal(b) ? 0 : 1;

    while (!mint_is_zero_internal(b)) {
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
    mint_free(a);
    mint_free(b);
    mint_free(r);
    return rc;
}

int mint_lcm(mint_t *mint, const mint_t *other)
{
    mint_t *a, *b, *g;
    int rc = -1;

    if (!mint || !other || mint_is_immortal(mint))
        return -1;
    if (mint_is_zero_internal(mint) || mint_is_zero_internal(other)) {
        mint_clear(mint);
        return 0;
    }

    a = mint_dup_value(mint);
    b = mint_dup_value(other);
    g = mint_dup_value(mint);
    if (!a || !b || !g)
        goto cleanup;

    a->sign = 1;
    b->sign = 1;

    if (mint_gcd(g, other) != 0)
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
    mint_free(a);
    mint_free(b);
    mint_free(g);
    return rc;
}

int mint_gcdext(mint_t *g, mint_t *x, mint_t *y,
                const mint_t *a, const mint_t *b)
{
    mint_t *old_r = NULL, *r = NULL, *q = NULL, *rem = NULL;
    mint_t *old_s = NULL, *s = NULL, *old_t = NULL, *t = NULL;
    mint_t *tmp = NULL, *next_s = NULL, *next_t = NULL;
    int rc = -1;

    if (!a || !b)
        return -1;
    if ((g && mint_is_immortal(g)) ||
        (x && mint_is_immortal(x)) ||
        (y && mint_is_immortal(y)))
        return -1;

    old_r = mint_dup_value(a);
    r = mint_dup_value(b);
    q = mint_new();
    rem = mint_new();
    old_s = mint_create_long(1);
    s = mint_create_long(0);
    old_t = mint_create_long(0);
    t = mint_create_long(1);
    if (!old_r || !r || !q || !rem || !old_s || !s || !old_t || !t)
        goto cleanup;

    if (mint_abs(old_r) != 0 || mint_abs(r) != 0)
        goto cleanup;

    while (!mint_is_zero_internal(r)) {
        mint_t *swap;

        if (mint_divmod(old_r, r, q, rem) != 0)
            goto cleanup;

        tmp = mint_clone(s);
        next_s = mint_clone(old_s);
        if (!tmp || !next_s)
            goto cleanup;
        if (mint_mul(tmp, q) != 0 || mint_sub(next_s, tmp) != 0)
            goto cleanup;
        mint_free(tmp);
        tmp = NULL;

        tmp = mint_clone(t);
        next_t = mint_clone(old_t);
        if (!tmp || !next_t)
            goto cleanup;
        if (mint_mul(tmp, q) != 0 || mint_sub(next_t, tmp) != 0)
            goto cleanup;
        mint_free(tmp);
        tmp = NULL;

        swap = old_r;
        old_r = r;
        r = rem;
        rem = swap;

        swap = old_s;
        old_s = s;
        s = next_s;
        next_s = swap;
        mint_free(next_s);
        next_s = NULL;

        swap = old_t;
        old_t = t;
        t = next_t;
        next_t = swap;
        mint_free(next_t);
        next_t = NULL;
    }

    if (a->sign < 0 && mint_neg(old_s) != 0)
        goto cleanup;
    if (b->sign < 0 && mint_neg(old_t) != 0)
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
    mint_free(old_r);
    mint_free(r);
    mint_free(q);
    mint_free(rem);
    mint_free(old_s);
    mint_free(s);
    mint_free(old_t);
    mint_free(t);
    mint_free(tmp);
    mint_free(next_s);
    mint_free(next_t);
    return rc;
}

int mint_modinv(mint_t *mint, const mint_t *modulus)
{
    mint_t *g = NULL, *x = NULL;
    int rc = -1;

    if (!mint || !modulus || mint_is_immortal(mint))
        return -1;
    if (modulus->sign <= 0)
        return -1;

    g = mint_new();
    x = mint_new();
    if (!g || !x)
        goto cleanup;

    if (mint_gcdext(g, x, NULL, mint, modulus) != 0)
        goto cleanup;
    if (mint_cmp(g, MNT_ONE) != 0)
        goto cleanup;

    while (mint_is_negative(x))
        if (mint_add(x, modulus) != 0)
            goto cleanup;
    while (mint_cmp(x, modulus) >= 0)
        if (mint_sub(x, modulus) != 0)
            goto cleanup;

    if (mint_copy_value(mint, x) != 0)
        goto cleanup;
    mint->sign = mint_is_zero_internal(mint) ? 0 : 1;
    rc = 0;

cleanup:
    mint_free(g);
    mint_free(x);
    return rc;
}

static int mint_not_inplace(mint_t *mint)
{
    size_t bitlen, top_bits, i;
    uint64_t mask;

    if (!mint || mint_is_immortal(mint))
        return -1;
    if (mint->sign < 0)
        return -1;
    if (mint_is_zero_internal(mint))
        return 0;

    bitlen = mint_bit_length_internal(mint);
    for (i = 0; i < mint->length; ++i)
        mint->storage[i] = ~mint->storage[i];

    top_bits = bitlen - (mint->length - 1) * 64;
    if (top_bits < 64) {
        mask = (((uint64_t)1u) << top_bits) - 1u;
        mint->storage[mint->length - 1] &= mask;
    }

    mint_normalise(mint);
    mint_zero_spare_limbs(mint, mint->length);
    return 0;
}

static int mint_or_inplace(mint_t *mint, const mint_t *other)
{
    size_t i, max_len, min_len, old_len;

    if (!mint || !other || mint_is_immortal(mint))
        return -1;
    if (mint_has_negative_operand(mint, other))
        return -1;
    if (mint_is_zero_internal(other))
        return 0;
    if (mint_is_zero_internal(mint))
        return mint_copy_value(mint, other);

    old_len = mint->length;
    min_len = mint->length < other->length ? mint->length : other->length;
    max_len = mint->length > other->length ? mint->length : other->length;
    if (mint_ensure_capacity(mint, max_len) != 0)
        return -1;

    for (i = 0; i < min_len; ++i)
        mint->storage[i] |= other->storage[i];
    if (old_len < other->length) {
        for (i = old_len; i < other->length; ++i)
            mint->storage[i] = other->storage[i];
        mint->length = max_len;
    }

    mint->sign = mint->length == 0 ? 0 : 1;
    mint_normalise(mint);
    mint_zero_spare_limbs(mint, mint->length);
    return 0;
}

static int mint_mul_inplace(mint_t *mint, const mint_t *other)
{
    mint_t *result;
    size_t i, j;

    if (!mint || !other || mint_is_immortal(mint))
        return -1;
    if (mint_is_zero_internal(mint) || mint_is_zero_internal(other)) {
        mint_clear(mint);
        return 0;
    }

    result = mint_new();
    if (!result)
        return -1;
    if (mint_ensure_capacity(result, mint->length + other->length + 1) != 0) {
        mint_free(result);
        return -1;
    }

    for (i = 0; i < mint->length + other->length + 1; ++i)
        result->storage[i] = 0;
    result->length = mint->length + other->length + 1;

    for (i = 0; i < mint->length; ++i) {
        __uint128_t carry = 0;

        for (j = 0; j < other->length; ++j) {
            __uint128_t acc =
                (__uint128_t)mint->storage[i] * other->storage[j] +
                result->storage[i + j] + carry;

            result->storage[i + j] = (uint64_t)acc;
            carry = acc >> 64;
        }

        j = i + other->length;
        while (carry != 0) {
            __uint128_t acc = (__uint128_t)result->storage[j] + carry;

            result->storage[j] = (uint64_t)acc;
            carry = acc >> 64;
            j++;
        }
    }

    result->sign = mint->sign == other->sign ? 1 : -1;
    mint_normalise(result);

    if (mint_copy_value(mint, result) != 0) {
        mint_free(result);
        return -1;
    }
    mint->sign = result->sign;
    mint_free(result);
    return 0;
}

static int mint_div_inplace(mint_t *mint, const mint_t *other, mint_t *rem)
{
    mint_t *numerator, *denominator, *quotient, *remainder;
    short qsign, rsign;

    if (!mint || !other || mint_is_immortal(mint) || rem == mint)
        return -1;
    if (rem && mint_is_immortal(rem))
        return -1;
    if (mint_is_zero_internal(other))
        return -1;
    if (mint_is_zero_internal(mint)) {
        if (rem)
            mint_clear(rem);
        return 0;
    }

    numerator = mint_dup_value(mint);
    denominator = mint_dup_value(other);
    quotient = mint_new();
    remainder = mint_new();
    if (!numerator || !denominator || !quotient || !remainder) {
        mint_free(numerator);
        mint_free(denominator);
        mint_free(quotient);
        mint_free(remainder);
        return -1;
    }

    numerator->sign = numerator->length == 0 ? 0 : 1;
    denominator->sign = denominator->length == 0 ? 0 : 1;

    if (mint_div_abs(numerator, denominator, quotient, remainder) != 0) {
        mint_free(numerator);
        mint_free(denominator);
        mint_free(quotient);
        mint_free(remainder);
        return -1;
    }

    qsign = mint_is_zero_internal(quotient) ? 0 : (mint->sign == other->sign ? 1 : -1);
    rsign = mint_is_zero_internal(remainder) ? 0 : mint->sign;

    if (mint_copy_value(mint, quotient) != 0) {
        mint_free(numerator);
        mint_free(denominator);
        mint_free(quotient);
        mint_free(remainder);
        return -1;
    }
    mint->sign = qsign;

    if (rem) {
        if (mint_copy_value(rem, remainder) != 0) {
            mint_free(numerator);
            mint_free(denominator);
            mint_free(quotient);
            mint_free(remainder);
            return -1;
        }
        rem->sign = rsign;
    }

    mint_free(numerator);
    mint_free(denominator);
    mint_free(quotient);
    mint_free(remainder);
    return 0;
}

static int mint_mod_inplace(mint_t *mint, const mint_t *other)
{
    mint_t *remainder;
    int rc;

    if (!mint || !other || mint_is_immortal(mint))
        return -1;

    remainder = mint_new();
    if (!remainder)
        return -1;

    rc = mint_div_inplace(mint, other, remainder);
    if (rc == 0) {
        if (mint_copy_value(mint, remainder) != 0)
            rc = -1;
        else
            mint->sign = remainder->sign;
    }

    mint_free(remainder);
    return rc;
}

static int mint_neg_inplace(mint_t *mint)
{
    if (!mint || mint_is_immortal(mint))
        return -1;
    if (!mint_is_zero_internal(mint))
        mint->sign = (short)-mint->sign;
    return 0;
}

static int mint_pow_inplace(mint_t *mint, unsigned long exponent)
{
    mint_t *base, *result;

    if (!mint || mint_is_immortal(mint))
        return -1;

    result = mint_create_long(1);
    base = mint_dup_value(mint);
    if (!result || !base) {
        mint_free(result);
        mint_free(base);
        return -1;
    }

    while (exponent > 0) {
        if (exponent & 1ul) {
            if (mint_mul_inplace(result, base) != 0)
                goto fail;
        }
        exponent >>= 1;
        if (exponent == 0)
            break;
        if (mint_mul_inplace(base, base) != 0)
            goto fail;
    }

    if (mint_copy_value(mint, result) != 0)
        goto fail;
    mint->sign = result->sign;

    mint_free(result);
    mint_free(base);
    return 0;

fail:
    mint_free(result);
    mint_free(base);
    return -1;
}

static int mint_powmod_inplace(mint_t *mint, const mint_t *exponent, const mint_t *modulus)
{
    mint_t *base, *exp_copy, *result;

    if (!mint || !exponent || !modulus || mint_is_immortal(mint))
        return -1;
    if (mint->sign < 0 || exponent->sign < 0 || modulus->sign <= 0)
        return -1;
    if (mint_is_zero_internal(modulus))
        return -1;

    base = mint_dup_value(mint);
    exp_copy = mint_dup_value(exponent);
    result = mint_create_long(1);
    if (!base || !exp_copy || !result) {
        mint_free(base);
        mint_free(exp_copy);
        mint_free(result);
        return -1;
    }

    if (mint_mod_inplace(base, modulus) != 0)
        goto fail;

    while (!mint_is_zero_internal(exp_copy)) {
        if (mint_is_odd_internal(exp_copy)) {
            if (mint_mul_inplace(result, base) != 0)
                goto fail;
            if (mint_mod_inplace(result, modulus) != 0)
                goto fail;
        }
        if (mint_shr_inplace(exp_copy, 1) != 0)
            goto fail;
        if (mint_is_zero_internal(exp_copy))
            break;
        if (mint_mul_inplace(base, base) != 0)
            goto fail;
        if (mint_mod_inplace(base, modulus) != 0)
            goto fail;
    }

    if (mint_copy_value(mint, result) != 0)
        goto fail;
    mint->sign = mint_is_zero_internal(mint) ? 0 : 1;

    mint_free(base);
    mint_free(exp_copy);
    mint_free(result);
    return 0;

fail:
    mint_free(base);
    mint_free(exp_copy);
    mint_free(result);
    return -1;
}

static int mint_pow10_inplace(mint_t *mint, long exponent)
{
    long i;

    if (!mint || mint_is_immortal(mint) || exponent < 0)
        return -1;
    if (mint_is_zero_internal(mint))
        return 0;

    for (i = 0; i < exponent; ++i)
        if (mint_mul_small(mint, 10) != 0)
            return -1;

    return 0;
}

static int mint_shl_inplace(mint_t *mint, long bits)
{
    size_t limb_shift, i;
    unsigned bit_shift;
    uint64_t carry = 0;

    if (!mint || mint_is_immortal(mint))
        return -1;
    if (bits < 0)
        return mint_shr_inplace(mint, -bits);
    if (bits == 0 || mint_is_zero_internal(mint))
        return 0;

    limb_shift = (size_t)(bits / 64);
    bit_shift = (unsigned)(bits % 64);

    if (mint_ensure_capacity(mint, mint->length + limb_shift + 1) != 0)
        return -1;

    if (limb_shift > 0) {
        for (i = mint->length; i > 0; --i)
            mint->storage[i - 1 + limb_shift] = mint->storage[i - 1];
        for (i = 0; i < limb_shift; ++i)
            mint->storage[i] = 0;
        mint->length += limb_shift;
    }

    if (bit_shift > 0) {
        for (i = limb_shift; i < mint->length; ++i) {
            uint64_t next = mint->storage[i] >> (64 - bit_shift);

            mint->storage[i] = (mint->storage[i] << bit_shift) | carry;
            carry = next;
        }
        if (carry != 0)
            mint->storage[mint->length++] = carry;
    }

    return 0;
}

static int mint_shr_inplace(mint_t *mint, long bits)
{
    size_t limb_shift, i, new_len;
    unsigned bit_shift;
    uint64_t carry = 0;

    if (!mint || mint_is_immortal(mint))
        return -1;
    if (bits < 0)
        return mint_shl_inplace(mint, -bits);
    if (bits == 0 || mint_is_zero_internal(mint))
        return 0;

    limb_shift = (size_t)(bits / 64);
    bit_shift = (unsigned)(bits % 64);

    if (limb_shift >= mint->length) {
        mint_clear(mint);
        return 0;
    }

    new_len = mint->length - limb_shift;
    if (limb_shift > 0) {
        for (i = 0; i < new_len; ++i)
            mint->storage[i] = mint->storage[i + limb_shift];
        mint->length = new_len;
    }

    if (bit_shift > 0) {
        carry = 0;
        for (i = mint->length; i > 0; --i) {
            uint64_t cur = mint->storage[i - 1];
            uint64_t next_carry = cur << (64 - bit_shift);

            mint->storage[i - 1] = (cur >> bit_shift) | carry;
            carry = next_carry;
        }
    }

    mint_normalise(mint);
    mint_zero_spare_limbs(mint, mint->length);
    return 0;
}

static int mint_sqrt_inplace(mint_t *mint)
{
    mint_t *guess, *q, *next, *one, *sq, *sq_plus;
    int cmp;

    if (!mint || mint_is_immortal(mint))
        return -1;
    if (mint_is_zero_internal(mint))
        return 0;
    if (mint->sign < 0)
        return -1;
    if (mint->length == 1 && mint->storage[0] < 2)
        return 0;

    guess = mint_new();
    q = mint_new();
    next = mint_new();
    one = mint_create_long(1);
    sq = mint_new();
    sq_plus = mint_new();
    if (!guess || !q || !next || !one || !sq || !sq_plus) {
        mint_free(guess);
        mint_free(q);
        mint_free(next);
        mint_free(one);
        mint_free(sq);
        mint_free(sq_plus);
        return -1;
    }

    if (mint_sqrt_initial_guess(guess, mint) != 0)
        goto fail;

    for (;;) {
        if (mint_copy_value(q, mint) != 0)
            goto fail;
        if (mint_div_inplace(q, guess, NULL) != 0)
            goto fail;
        if (mint_copy_value(next, guess) != 0)
            goto fail;
        if (mint_add_inplace(next, q) != 0)
            goto fail;
        if (mint_shr_inplace(next, 1) != 0)
            goto fail;

        cmp = mint_cmp(next, guess);
        if (cmp == 0)
            break;
        if (cmp > 0) {
            if (mint_copy_value(next, guess) != 0)
                goto fail;
            break;
        }
        if (mint_copy_value(guess, next) != 0)
            goto fail;
    }

    for (;;) {
        if (mint_copy_value(sq, guess) != 0)
            goto fail;
        if (mint_mul_inplace(sq, guess) != 0)
            goto fail;
        if (mint_cmp(sq, mint) <= 0)
            break;
        if (mint_sub_small(guess, 1) != 0)
            goto fail;
    }

    for (;;) {
        if (mint_copy_value(sq_plus, guess) != 0)
            goto fail;
        if (mint_add_inplace(sq_plus, one) != 0)
            goto fail;
        if (mint_copy_value(sq, sq_plus) != 0)
            goto fail;
        if (mint_mul_inplace(sq, sq_plus) != 0)
            goto fail;
        if (mint_cmp(sq, mint) > 0)
            break;
        if (mint_copy_value(guess, sq_plus) != 0)
            goto fail;
    }

    if (mint_copy_value(mint, guess) != 0)
        goto fail;
    mint->sign = mint_is_zero_internal(mint) ? 0 : 1;

    mint_free(guess);
    mint_free(q);
    mint_free(next);
    mint_free(one);
    mint_free(sq);
    mint_free(sq_plus);
    return 0;

fail:
    mint_free(guess);
    mint_free(q);
    mint_free(next);
    mint_free(one);
    mint_free(sq);
    mint_free(sq_plus);
    return -1;
}

static int mint_xor_inplace(mint_t *mint, const mint_t *other)
{
    size_t i, max_len, min_len, old_len;

    if (!mint || !other || mint_is_immortal(mint))
        return -1;
    if (mint_has_negative_operand(mint, other))
        return -1;
    if (other == mint) {
        mint_clear(mint);
        return 0;
    }
    if (mint_is_zero_internal(other))
        return 0;
    if (mint_is_zero_internal(mint))
        return mint_copy_value(mint, other);

    old_len = mint->length;
    min_len = mint->length < other->length ? mint->length : other->length;
    max_len = mint->length > other->length ? mint->length : other->length;
    if (mint_ensure_capacity(mint, max_len) != 0)
        return -1;

    for (i = 0; i < min_len; ++i)
        mint->storage[i] ^= other->storage[i];
    for (i = old_len; i < other->length; ++i)
        mint->storage[i] = other->storage[i];

    if (mint->length < max_len)
        mint->length = max_len;

    mint->sign = mint->length == 0 ? 0 : 1;
    mint_normalise(mint);
    mint_zero_spare_limbs(mint, mint->length);
    return 0;
}

int mint_add(mint_t *mint, const mint_t *other)
{
    return mint_add_inplace(mint, other);
}

int mint_and(mint_t *mint, const mint_t *other)
{
    return mint_and_inplace(mint, other);
}

int mint_set_bit(mint_t *mint, size_t bit_index)
{
    if (!mint || mint_is_immortal(mint) || mint->sign < 0)
        return -1;
    return mint_set_bit_internal(mint, bit_index);
}

int mint_clear_bit(mint_t *mint, size_t bit_index)
{
    size_t limb;
    unsigned shift;

    if (!mint || mint_is_immortal(mint) || mint->sign < 0)
        return -1;
    if (mint_is_zero_internal(mint))
        return 0;

    limb = bit_index / 64;
    shift = (unsigned)(bit_index % 64);
    if (limb >= mint->length)
        return 0;

    mint->storage[limb] &= ~(((uint64_t)1u) << shift);
    mint_normalise(mint);
    mint_zero_spare_limbs(mint, mint->length);
    return 0;
}

int mint_mul(mint_t *mint, const mint_t *other)
{
    return mint_mul_inplace(mint, other);
}

int mint_div(mint_t *mint, const mint_t *other, mint_t *rem)
{
    return mint_div_inplace(mint, other, rem);
}

int mint_mod(mint_t *mint, const mint_t *other)
{
    return mint_mod_inplace(mint, other);
}

int mint_neg(mint_t *mint)
{
    return mint_neg_inplace(mint);
}

int mint_not(mint_t *mint)
{
    return mint_not_inplace(mint);
}

int mint_or(mint_t *mint, const mint_t *other)
{
    return mint_or_inplace(mint, other);
}

int mint_pow(mint_t *mint, unsigned long exponent)
{
    return mint_pow_inplace(mint, exponent);
}

int mint_powmod(mint_t *mint, const mint_t *exponent, const mint_t *modulus)
{
    return mint_powmod_inplace(mint, exponent, modulus);
}

int mint_pow10(mint_t *mint, long exponent)
{
    return mint_pow10_inplace(mint, exponent);
}

int mint_shl(mint_t *mint, long bits)
{
    return mint_shl_inplace(mint, bits);
}

int mint_shr(mint_t *mint, long bits)
{
    return mint_shr_inplace(mint, bits);
}

int mint_sqrt(mint_t *mint)
{
    return mint_sqrt_inplace(mint);
}

int mint_xor(mint_t *mint, const mint_t *other)
{
    return mint_xor_inplace(mint, other);
}

int mint_set_long(mint_t *mint, long value)
{
    uint64_t magnitude;
    short sign;

    if (!mint || mint_is_immortal(mint))
        return -1;

    if (value == 0) {
        mint_clear(mint);
        return 0;
    }

    sign = value < 0 ? (short)-1 : (short)1;
    if (value < 0)
        magnitude = (uint64_t)(-(value + 1)) + 1u;
    else
        magnitude = (uint64_t)value;

    return mint_set_magnitude_u64(mint, magnitude, sign);
}

int mint_set_ulong(mint_t *mint, unsigned long value)
{
    if (!mint || mint_is_immortal(mint))
        return -1;
    if (value == 0) {
        mint_clear(mint);
        return 0;
    }
    return mint_set_magnitude_u64(mint, (uint64_t)value, 1);
}

int mint_set_string(mint_t *mint, const char *text)
{
    const unsigned char *p;
    short sign = 1;
    int saw_digit = 0;

    if (!mint || !text || mint_is_immortal(mint))
        return -1;

    mint_clear(mint);
    p = (const unsigned char *)text;

    while (*p && isspace(*p))
        p++;

    if (*p == '+' || *p == '-') {
        if (*p == '-')
            sign = -1;
        p++;
    }

    while (*p == '0')
        p++;

    for (; *p && isdigit(*p); ++p) {
        if (mint_mul_small(mint, 10) != 0)
            goto fail;
        if (mint_add_small(mint, (uint32_t)(*p - '0')) != 0)
            goto fail;
        saw_digit = 1;
    }

    while (*p && isspace(*p))
        p++;

    if (*p != '\0')
        goto fail;

    if (!saw_digit) {
        mint_clear(mint);
        return 0;
    }

    mint->sign = mint->length == 0 ? 0 : sign;
    return 0;

fail:
    mint_clear(mint);
    return -1;
}

int mint_set_hex(mint_t *mint, const char *text)
{
    const unsigned char *p;
    short sign = 1;
    int saw_digit = 0;

    if (!mint || !text || mint_is_immortal(mint))
        return -1;

    mint_clear(mint);
    p = (const unsigned char *)text;

    while (*p && isspace(*p))
        p++;

    if (*p == '+' || *p == '-') {
        if (*p == '-')
            sign = -1;
        p++;
    }

    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
        p += 2;

    while (*p) {
        int digit;

        if (isspace(*p))
            break;

        digit = mint_hex_digit_value(*p);
        if (digit < 0)
            goto fail;

        if (mint_shl_inplace(mint, 4) != 0)
            goto fail;
        if (digit != 0 && mint_add_small(mint, (uint32_t)digit) != 0)
            goto fail;
        saw_digit = 1;
        p++;
    }

    while (*p && isspace(*p))
        p++;

    if (*p != '\0')
        goto fail;

    if (!saw_digit) {
        mint_clear(mint);
        return 0;
    }

    mint->sign = mint->length == 0 ? 0 : sign;
    return 0;

fail:
    mint_clear(mint);
    return -1;
}

char *mint_to_string(const mint_t *mint)
{
    mint_t *tmp;
    char *buf;
    size_t buf_len, pos;

    if (!mint)
        return NULL;
    if (mint->sign == 0) {
        buf = malloc(2);
        if (!buf)
            return NULL;
        buf[0] = '0';
        buf[1] = '\0';
        return buf;
    }

    tmp = mint_new();
    if (!tmp)
        return NULL;
    if (mint_copy_value(tmp, mint) != 0) {
        mint_free(tmp);
        return NULL;
    }
    tmp->sign = 1;

    buf_len = mint->length * 20 + 2;
    buf = malloc(buf_len);
    if (!buf) {
        mint_free(tmp);
        return NULL;
    }

    pos = buf_len;
    buf[--pos] = '\0';

    while (tmp->sign != 0)
        buf[--pos] = (char)('0' + mint_div_small_inplace(tmp, 10));

    if (mint->sign < 0)
        buf[--pos] = '-';

    memmove(buf, buf + pos, buf_len - pos);
    mint_free(tmp);
    return buf;
}

char *mint_to_hex(const mint_t *mint)
{
    static const char digits[] = "0123456789abcdef";
    size_t bitlen, digit_count, first_bits, i, pos = 0;
    char *out;

    if (!mint)
        return NULL;
    if (mint->sign == 0) {
        out = malloc(2);
        if (!out)
            return NULL;
        out[0] = '0';
        out[1] = '\0';
        return out;
    }

    bitlen = mint_bit_length_internal(mint);
    digit_count = (bitlen + 3) / 4;
    out = malloc((mint->sign < 0 ? 1u : 0u) + digit_count + 1u);
    if (!out)
        return NULL;

    if (mint->sign < 0)
        out[pos++] = '-';

    first_bits = bitlen % 4;
    if (first_bits == 0)
        first_bits = 4;

    for (i = bitlen; i > 0;) {
        size_t chunk = (i == bitlen) ? first_bits : 4;
        unsigned value = 0;
        size_t j;

        i -= chunk;
        for (j = 0; j < chunk; ++j) {
            value <<= 1;
            value |= (unsigned)mint_get_bit(mint, i + (chunk - 1 - j));
        }
        out[pos++] = digits[value];
    }

    out[pos] = '\0';
    return out;
}
