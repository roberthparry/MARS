#include "mint_internal.h"

#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

const unsigned long mint_small_primes[MINT_SMALL_PRIMES_COUNT] = {
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

const mint_t * const MI_ZERO = &mnt_zero_static;
const mint_t * const MI_ONE = &mnt_one_static;
const mint_t * const MI_TEN = &mnt_ten_static;
int mint_is_immortal(const mint_t *mint)
{
    return mint == MI_ZERO || mint == MI_ONE || mint == MI_TEN;
}

int mint_is_zero_internal(const mint_t *mint)
{
    return !mint || mint->sign == 0 || mint->length == 0;
}

void mint_normalise(mint_t *mint)
{
    if (!mint)
        return;

    while (mint->length > 0 && mint->storage[mint->length - 1] == 0)
        mint->length--;

    if (mint->length == 0)
        mint->sign = 0;
}

int mint_cmp_abs(const mint_t *a, const mint_t *b)
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

int mint_has_negative_operand(const mint_t *a, const mint_t *b)
{
    return (a && a->sign < 0) || (b && b->sign < 0);
}

int mint_is_abs_one(const mint_t *mint)
{
    return mint && mint->length == 1 && mint->storage[0] == 1u;
}

int mint_is_even_internal(const mint_t *mint)
{
    return mint_is_zero_internal(mint) || ((mint->storage[0] & 1u) == 0);
}

int mint_is_odd_internal(const mint_t *mint)
{
    return !mint_is_zero_internal(mint) && ((mint->storage[0] & 1u) != 0);
}

void mint_zero_spare_limbs(mint_t *mint, size_t from)
{
    if (!mint || !mint->storage || from >= mint->capacity)
        return;
    memset(mint->storage + from, 0, (mint->capacity - from) * sizeof(*mint->storage));
}

int mint_ensure_capacity(mint_t *mint, size_t needed)
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

int mint_set_magnitude_u64(mint_t *mint, uint64_t magnitude, short sign)
{
    if (!mint || mint_is_immortal(mint))
        return -1;

    if (magnitude == 0) {
        mi_clear(mint);
        return 0;
    }

    if (mint_ensure_capacity(mint, 1) != 0)
        return -1;

    mint->storage[0] = magnitude;
    mint->length = 1;
    mint->sign = sign < 0 ? -1 : 1;
    return 0;
}

int mint_abs_add_inplace(mint_t *dst, const mint_t *src)
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

int mint_cmp_abs_u64(const mint_t *mint, uint64_t value)
{
    if (!mint)
        return 1;
    if (mint->length == 0)
        return value == 0 ? 0 : -1;
    if (mint->length > 1)
        return 1;
    if (mint->storage[0] < value)
        return -1;
    if (mint->storage[0] > value)
        return 1;
    return 0;
}

void mint_abs_sub_inplace(mint_t *dst, const mint_t *src)
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

void mint_abs_sub_raw_inplace(uint64_t *dst_storage,
                                     size_t *dst_length,
                                     const uint64_t *src_storage,
                                     size_t src_length)
{
    __uint128_t borrow = 0;
    size_t i;
    size_t len;

    if (!dst_storage || !dst_length)
        return;

    len = *dst_length;
    for (i = 0; i < len; ++i) {
        __uint128_t lhs = dst_storage[i];
        __uint128_t rhs = borrow;

        if (i < src_length)
            rhs += src_storage[i];

        dst_storage[i] = (uint64_t)(lhs - rhs);
        borrow = lhs < rhs ? 1 : 0;
    }

    while (len > 0 && dst_storage[len - 1] == 0)
        len--;
    *dst_length = len;
}

int mint_mul_small(mint_t *mint, uint32_t factor)
{
    __uint128_t carry = 0;
    size_t i;

    if (!mint)
        return -1;
    if (mint->sign == 0 || factor == 1)
        return 0;
    if (factor == 0) {
        mi_clear(mint);
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

int mint_mul_word_inplace(mint_t *mint, uint64_t factor)
{
    __uint128_t carry = 0;
    size_t i;

    if (!mint)
        return -1;
    if (mint->sign == 0 || factor == 1)
        return 0;
    if (factor == 0) {
        mi_clear(mint);
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

void mint_mul_schoolbook_raw(uint64_t *out,
                                    const uint64_t *lhs_storage,
                                    size_t lhs_length,
                                    const uint64_t *rhs_storage,
                                    size_t rhs_length)
{
    size_t i, j;

    for (i = 0; i < lhs_length; ++i) {
        __uint128_t carry = 0;

        for (j = 0; j < rhs_length; ++j) {
            __uint128_t acc =
                (__uint128_t)lhs_storage[i] * rhs_storage[j] +
                out[i + j] + carry;

            out[i + j] = (uint64_t)acc;
            carry = acc >> 64;
        }

        j = i + rhs_length;
        while (carry != 0) {
            __uint128_t acc = (__uint128_t)out[j] + carry;

            out[j] = (uint64_t)acc;
            carry = acc >> 64;
            ++j;
        }
    }
}

unsigned mint_clz64(uint64_t value)
{
    if (value == 0)
        return 64u;
    return (unsigned)__builtin_clzll(value);
}

uint64_t mint_shift_left_bits_raw(uint64_t *dst,
                                         const uint64_t *src,
                                         size_t len,
                                         unsigned shift)
{
    uint64_t carry = 0;
    size_t i;

    if (shift == 0) {
        memcpy(dst, src, len * sizeof(*dst));
        return 0;
    }

    for (i = 0; i < len; ++i) {
        uint64_t cur = src[i];

        dst[i] = (cur << shift) | carry;
        carry = cur >> (64u - shift);
    }

    return carry;
}

void mint_shift_right_bits_raw(uint64_t *dst,
                                      const uint64_t *src,
                                      size_t len,
                                      unsigned shift)
{
    size_t i;
    uint64_t carry = 0;

    if (shift == 0) {
        memcpy(dst, src, len * sizeof(*dst));
        return;
    }

    for (i = len; i > 0; --i) {
        uint64_t cur = src[i - 1];
        uint64_t next_carry = cur << (64u - shift);

        dst[i - 1] = (cur >> shift) | carry;
        carry = next_carry;
    }
}

int mint_sub_small(mint_t *mint, uint32_t subtrahend)
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

int mint_add_small(mint_t *mint, uint32_t addend)
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

int mint_add_word(mint_t *mint, uint64_t addend)
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

int mint_sub_word(mint_t *mint, uint64_t subtrahend)
{
    size_t i;
    uint64_t borrow = subtrahend;

    if (!mint)
        return -1;
    if (subtrahend == 0)
        return 0;
    if (mint_is_zero_internal(mint))
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

int mint_add_signed_word_inplace(mint_t *mint, uint64_t magnitude, short sign)
{
    int cmp;

    if (!mint)
        return -1;
    if (magnitude == 0)
        return 0;
    if (sign == 0)
        return 0;

    if (mint->sign == 0)
        return mint_set_magnitude_u64(mint, magnitude, sign);

    if (mint->sign == sign) {
        if (mint_add_word(mint, magnitude) != 0)
            return -1;
        mint->sign = sign;
        return 0;
    }

    cmp = mint_cmp_abs_u64(mint, magnitude);
    if (cmp == 0) {
        mi_clear(mint);
        return 0;
    }
    if (cmp > 0)
        return mint_sub_word(mint, magnitude);

    return mint_set_magnitude_u64(mint, magnitude - mint->storage[0], sign);
}

uint64_t mint_div_word_inplace(mint_t *mint, uint64_t divisor)
{
    __uint128_t rem = 0;
    size_t i;

    if (!mint || divisor == 0)
        return 0;

    for (i = mint->length; i > 0; --i) {
        __uint128_t cur = (rem << 64) | mint->storage[i - 1];

        mint->storage[i - 1] = (uint64_t)(cur / divisor);
        rem = cur % divisor;
    }

    mint_normalise(mint);
    return (uint64_t)rem;
}

uint32_t mint_div_small_inplace(mint_t *mint, uint32_t divisor)
{
    return (uint32_t)mint_div_word_inplace(mint, divisor);
}

int mint_hex_digit_value(unsigned char ch)
{
    if (ch >= '0' && ch <= '9')
        return (int)(ch - '0');
    if (ch >= 'a' && ch <= 'f')
        return 10 + (int)(ch - 'a');
    if (ch >= 'A' && ch <= 'F')
        return 10 + (int)(ch - 'A');
    return -1;
}

uint64_t mint_mod_u64(const mint_t *mint, uint64_t divisor)
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

int mint_mod_positive_inplace(mint_t *mint, const mint_t *modulus)
{
    if (!mint || !modulus)
        return -1;
    if (mint_mod_inplace(mint, modulus) != 0)
        return -1;
    while (mi_is_negative(mint))
        if (mint_add_inplace(mint, modulus) != 0)
            return -1;
    return 0;
}

int mint_half_mod_odd_inplace(mint_t *mint, const mint_t *modulus)
{
    if (!mint || !modulus || mint_is_even_internal(modulus))
        return -1;
    if (mint_is_odd_internal(mint))
        if (mint_add_inplace(mint, modulus) != 0)
            return -1;
    return mint_shr_inplace(mint, 1);
}

int mint_jacobi_u64(uint64_t a, uint64_t n)
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

int mint_jacobi_small_over_mint(long a, const mint_t *n)
{
    unsigned long abs_a;
    int result = 1;
    uint64_t rem;

    if (!n || n->sign <= 0 || mint_is_even_internal(n))
        return 0;
    if (a == 0)
        return mi_cmp_long(n, 1) == 0 ? 1 : 0;

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

void mint_ec_point_clear(mint_ec_point_t *point)
{
    if (!point)
        return;
    mi_free(point->x);
    mi_free(point->y);
    point->x = NULL;
    point->y = NULL;
    point->infinity = 1;
}

int mint_ec_point_init(mint_ec_point_t *point)
{
    if (!point)
        return -1;
    point->x = mi_new();
    point->y = mi_new();
    if (!point->x || !point->y) {
        mint_ec_point_clear(point);
        return -1;
    }
    point->infinity = 1;
    return 0;
}

int mint_ec_point_copy(mint_ec_point_t *dst, const mint_ec_point_t *src)
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

mint_ec_step_result_t mint_ec_make_slope(const mint_t *numerator,
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
        mi_gcd(g, modulus) != 0)
        goto cleanup;

    if (mi_cmp(g, MI_ONE) != 0) {
        if (mi_cmp(g, modulus) < 0)
            rc = MINT_EC_STEP_COMPOSITE;
        else
            rc = MINT_EC_STEP_ERROR;
        goto cleanup;
    }

    if (mi_modinv(den, modulus) != 0)
        goto cleanup;
    if (mint_copy_value(slope, numerator) != 0 ||
        mint_mod_positive_inplace(slope, modulus) != 0 ||
        mi_mul(slope, den) != 0 ||
        mint_mod_positive_inplace(slope, modulus) != 0)
        goto cleanup;

    rc = MINT_EC_STEP_OK;

cleanup:
    mi_free(den);
    mi_free(g);
    return (mint_ec_step_result_t)rc;
}

mint_ec_step_result_t mint_ec_add(const mint_ec_point_t *p,
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
    if (mi_sub(tmp, q->x) != 0 || mint_mod_positive_inplace(tmp, modulus) != 0)
        goto cleanup;
    if (mi_is_zero(tmp)) {
        if (mint_copy_value(tmp, p->y) != 0 ||
            mi_add(tmp, q->y) != 0 ||
            mint_mod_positive_inplace(tmp, modulus) != 0)
            goto cleanup;
        if (mi_is_zero(tmp)) {
            out->infinity = 1;
            rc = MINT_EC_STEP_INFINITY;
            goto cleanup;
        }
    }

    num = mi_new();
    den = mi_new();
    slope = mi_new();
    x3 = mi_new();
    y3 = mi_new();
    if (!num || !den || !slope || !x3 || !y3)
        goto cleanup;

    if (mi_cmp(p->x, q->x) == 0 && mi_cmp(p->y, q->y) == 0) {
        if (mint_copy_value(den, p->y) != 0 ||
            mi_mul_long(den, 2) != 0 ||
            mint_mod_positive_inplace(den, modulus) != 0)
            goto cleanup;
        if (mi_is_zero(den)) {
            out->infinity = 1;
            rc = MINT_EC_STEP_INFINITY;
            goto cleanup;
        }

        if (mint_copy_value(num, p->x) != 0 ||
            mi_square(num) != 0 ||
            mi_mul_long(num, 3) != 0 ||
            mi_add(num, a) != 0 ||
            mint_mod_positive_inplace(num, modulus) != 0)
            goto cleanup;
    } else {
        if (mint_copy_value(num, q->y) != 0 ||
            mi_sub(num, p->y) != 0 ||
            mint_mod_positive_inplace(num, modulus) != 0 ||
            mint_copy_value(den, q->x) != 0 ||
            mi_sub(den, p->x) != 0 ||
            mint_mod_positive_inplace(den, modulus) != 0)
            goto cleanup;
    }

    rc = mint_ec_make_slope(num, den, modulus, slope);
    if (rc != MINT_EC_STEP_OK)
        goto cleanup;

    if (mint_copy_value(x3, slope) != 0 ||
        mi_square(x3) != 0 ||
        mi_sub(x3, p->x) != 0 ||
        mi_sub(x3, q->x) != 0 ||
        mint_mod_positive_inplace(x3, modulus) != 0)
        goto cleanup;

    if (mint_copy_value(y3, p->x) != 0 ||
        mi_sub(y3, x3) != 0 ||
        mint_mod_positive_inplace(y3, modulus) != 0 ||
        mi_mul(y3, slope) != 0 ||
        mi_sub(y3, p->y) != 0 ||
        mint_mod_positive_inplace(y3, modulus) != 0)
        goto cleanup;

    if (mint_copy_value(out->x, x3) != 0 ||
        mint_copy_value(out->y, y3) != 0)
        goto cleanup;
    out->infinity = 0;
    rc = MINT_EC_STEP_OK;

cleanup:
    mi_free(tmp);
    mi_free(num);
    mi_free(den);
    mi_free(slope);
    mi_free(x3);
    mi_free(y3);
    return rc;
}

mint_ec_step_result_t mint_ec_scalar_mul(const mint_ec_point_t *point,
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

mint_ec_step_result_t mint_ec_scalar_mul_big(const mint_ec_point_t *point,
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

int mint_abs_diff(mint_t *dst, const mint_t *a, const mint_t *b)
{
    const mint_t *larger;
    const mint_t *smaller;

    if (!dst || !a || !b || mint_is_immortal(dst))
        return -1;

    if (mi_cmp(a, b) >= 0) {
        larger = a;
        smaller = b;
    } else {
        larger = b;
        smaller = a;
    }

    if (mint_copy_value(dst, larger) != 0)
        return -1;
    dst->sign = mint_is_zero_internal(dst) ? 0 : 1;
    return mi_sub(dst, smaller);
}

int mint_copy_value(mint_t *dst, const mint_t *src)
{
    if (!dst || !src)
        return -1;
    if (mint_is_immortal(dst))
        return -1;

    if (src->length == 0) {
        mi_clear(dst);
        return 0;
    }

    if (mint_ensure_capacity(dst, src->length) != 0)
        return -1;

    memcpy(dst->storage, src->storage, src->length * sizeof(*src->storage));
    dst->length = src->length;
    dst->sign = src->sign;
    return 0;
}

mint_t *mint_dup_value(const mint_t *src)
{
    mint_t *copy;

    if (!src)
        return NULL;

    copy = mi_new();
    if (!copy)
        return NULL;
    if (mint_copy_value(copy, src) != 0) {
        mi_free(copy);
        return NULL;
    }
    return copy;
}

size_t mint_bit_length_internal(const mint_t *mint)
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

int mint_get_bit(const mint_t *mint, size_t bit_index)
{
    size_t limb = bit_index / 64;
    unsigned shift = (unsigned)(bit_index % 64);

    if (!mint || limb >= mint->length)
        return 0;
    return (int)((mint->storage[limb] >> shift) & 1u);
}

__uint128_t mint_extract_top_bits(const mint_t *mint, size_t prefix_bits)
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

uint64_t mint_isqrt_u128(__uint128_t value)
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

size_t mint_lehmer_collect_quotients(const mint_t *a,
                                            const mint_t *b,
                                            unsigned long *qs,
                                            size_t max_qs)
{
    size_t n;
    __int128 x, y;
    __int128 A = 1, B = 0, C = 0, D = 1;
    size_t count = 0;

    if (!a || !b || !qs || max_qs == 0)
        return 0;
    if (a->length != b->length || a->length < 2)
        return 0;

    n = a->length;
    x = ((__int128)a->storage[n - 1] << 64) | a->storage[n - 2];
    y = ((__int128)b->storage[n - 1] << 64) | b->storage[n - 2];

    while (count < max_qs) {
        __int128 denom1 = y + C;
        __int128 denom2 = y + D;
        __int128 q1, q2;
        unsigned long q;

        if (denom1 <= 0 || denom2 <= 0)
            break;

        q1 = (x + A) / denom1;
        q2 = (x + B) / denom2;
        if (q1 != q2 || q1 <= 0 || q1 > (__int128)ULONG_MAX)
            break;

        q = (unsigned long)q1;
        qs[count++] = q;

        {
            __int128 nextA = C;
            __int128 nextB = D;
            __int128 nextC = A - (__int128)q * C;
            __int128 nextD = B - (__int128)q * D;
            __int128 nextX = y;
            __int128 nextY = x - (__int128)q * y;

            A = nextA;
            B = nextB;
            C = nextC;
            D = nextD;
            x = nextX;
            y = nextY;
        }
    }

    return count;
}

int mint_lehmer_apply_quotients(mint_t *a,
                                       mint_t *b,
                                       const unsigned long *qs,
                                       size_t qcount,
                                       mint_t *tmp)
{
    size_t i;

    if (!a || !b || !qs || !tmp)
        return -1;

    for (i = 0; i < qcount; ++i) {
        mint_t *swap;

        if (mint_is_zero_internal(b))
            break;
        if (mint_copy_value(tmp, b) != 0)
            return -1;
        if (mint_mul_word_inplace(tmp, (uint64_t)qs[i]) != 0)
            return -1;
        if (mi_sub(a, tmp) != 0)
            return -1;

        swap = a;
        a = b;
        b = swap;
    }

    return 0;
}

uint64_t mint_gcd_u64(uint64_t a, uint64_t b)
{
    while (b != 0) {
        uint64_t t = a % b;

        a = b;
        b = t;
    }
    return a;
}

int mint_fibonacci_pair(mint_t *fn, mint_t *fn1, unsigned long n)
{
    mint_t *a = NULL, *b = NULL, *c = NULL, *d = NULL, *tmp = NULL;
    unsigned long mask;
    int rc = -1;

    if (!fn || !fn1)
        return -1;

    a = mi_new();
    b = mi_new();
    c = mi_new();
    d = mi_new();
    tmp = mi_new();
    if (!a || !b || !c || !d || !tmp)
        goto cleanup;

    if (mi_set_long(a, 0) != 0 || mi_set_long(b, 1) != 0)
        goto cleanup;

    if (n != 0) {
        mask = 1ul << ((sizeof(n) * 8u - 1u) - (unsigned)__builtin_clzl(n));
        for (; mask != 0; mask >>= 1) {
            if (mint_copy_value(tmp, b) != 0 || mi_mul_long(tmp, 2) != 0 ||
                mi_sub(tmp, a) != 0)
                goto cleanup;
            if (mint_copy_value(c, a) != 0 || mi_mul(c, tmp) != 0)
                goto cleanup;

            if (mint_copy_value(d, a) != 0 || mi_square(d) != 0)
                goto cleanup;
            if (mint_copy_value(tmp, b) != 0 || mi_square(tmp) != 0 ||
                mi_add(d, tmp) != 0)
                goto cleanup;

            if ((n & mask) == 0) {
                if (mint_copy_value(a, c) != 0 || mint_copy_value(b, d) != 0)
                    goto cleanup;
            } else {
                if (mint_copy_value(tmp, c) != 0 || mi_add(tmp, d) != 0)
                    goto cleanup;
                if (mint_copy_value(a, d) != 0 || mint_copy_value(b, tmp) != 0)
                    goto cleanup;
            }
        }
    }

    if (mint_copy_value(fn, a) != 0 || mint_copy_value(fn1, b) != 0)
        goto cleanup;

    rc = 0;

cleanup:
    mi_free(a);
    mi_free(b);
    mi_free(c);
    mi_free(d);
    mi_free(tmp);
    return rc;
}

int mint_set_bit_internal(mint_t *mint, size_t bit_index)
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

void mint_keep_low_bits(mint_t *mint, size_t bits)
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

int mint_shift_right_bits_to(const mint_t *src, size_t bits, mint_t *dst)
{
    size_t limb_shift, out_len, i;
    unsigned rem_bits;

    if (!src || !dst)
        return -1;

    if (mint_is_zero_internal(src)) {
        mi_clear(dst);
        return 0;
    }

    limb_shift = bits / 64u;
    rem_bits = (unsigned)(bits % 64u);
    if (limb_shift >= src->length) {
        mi_clear(dst);
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

int mint_mod_mersenne_with_scratch(mint_t *mint, size_t exponent,
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

int mint_div_abs(const mint_t *numerator, const mint_t *denominator,
                        mint_t *quotient, mint_t *remainder)
{
    uint64_t *u = NULL;
    uint64_t *v = NULL;
    size_t n, m, j;
    unsigned shift;
    int rc = -1;

    if (!numerator || !denominator || !quotient || !remainder)
        return -1;
    if (mint_is_zero_internal(denominator))
        return -1;

    mi_clear(quotient);
    mi_clear(remainder);

    if (mint_cmp_abs(numerator, denominator) < 0) {
        if (mint_copy_value(remainder, numerator) != 0)
            return -1;
        remainder->sign = remainder->length == 0 ? 0 : 1;
        return 0;
    }

    if (denominator->length == 1) {
        uint64_t rem_mag;

        if (mint_copy_value(quotient, numerator) != 0)
            return -1;
        rem_mag = mint_div_word_inplace(quotient, denominator->storage[0]);
        quotient->sign = mint_is_zero_internal(quotient) ? 0 : 1;
        return mint_set_magnitude_u64(remainder, rem_mag, rem_mag == 0 ? 0 : 1);
    }

    n = denominator->length;
    m = numerator->length - n;
    u = calloc(numerator->length + 1, sizeof(*u));
    v = malloc(n * sizeof(*v));
    if (!u || !v)
        goto cleanup;

    shift = mint_clz64(denominator->storage[n - 1]);
    u[numerator->length] = mint_shift_left_bits_raw(u, numerator->storage,
                                                    numerator->length, shift);
    (void)mint_shift_left_bits_raw(v, denominator->storage, n, shift);

    if (mint_ensure_capacity(quotient, m + 1) != 0)
        goto cleanup;
    memset(quotient->storage, 0, (m + 1) * sizeof(*quotient->storage));
    quotient->length = m + 1;
    quotient->sign = 1;

    {
        uint64_t v1 = v[n - 1];
        uint64_t v2 = v[n - 2];

    for (j = m + 1; j-- > 0;) {
        __uint128_t numhat =
            ((__uint128_t)u[j + n] << 64) | u[j + n - 1];
        __uint128_t qhat_est = numhat / v1;
        __uint128_t rhat_est = numhat % v1;
        uint64_t qhat;
        size_t i;
        uint64_t carry;
        uint64_t borrow;

        if (qhat_est > UINT64_MAX) {
            qhat = UINT64_MAX;
            rhat_est = numhat - (__uint128_t)qhat * v1;
        } else {
            qhat = (uint64_t)qhat_est;
        }

        while ((__uint128_t)qhat * v2 >
               (rhat_est << 64) + u[j + n - 2]) {
            qhat--;
            rhat_est += v1;
            if (rhat_est > UINT64_MAX)
                break;
        }

        if (qhat == 0) {
            quotient->storage[j] = 0;
            continue;
        }

        carry = 0;
        borrow = 0;
        for (i = 0; i < n; ++i) {
            __uint128_t prod = (__uint128_t)qhat * v[i] + carry;
            __uint128_t rhs = (__uint128_t)(uint64_t)prod + borrow;
            uint64_t ui = u[j + i];

            u[j + i] = (uint64_t)((__uint128_t)ui - rhs);
            borrow = ((__uint128_t)ui < rhs) ? 1u : 0u;
            carry = (uint64_t)(prod >> 64);
        }

        {
            __uint128_t rhs = (__uint128_t)carry + borrow;
            uint64_t ui = u[j + n];

            u[j + n] = (uint64_t)((__uint128_t)ui - rhs);
            borrow = ((__uint128_t)ui < rhs) ? 1u : 0u;
        }

        if (borrow != 0) {
            __uint128_t add_carry = 0;
            size_t i;

            qhat--;
            for (i = 0; i < n; ++i) {
                __uint128_t sum =
                    (__uint128_t)u[j + i] + v[i] + add_carry;

                u[j + i] = (uint64_t)sum;
                add_carry = sum >> 64;
            }
            u[j + n] += (uint64_t)add_carry;
        }
        quotient->storage[j] = qhat;
    }
    }

    mint_normalise(quotient);
    if (mint_ensure_capacity(remainder, n) != 0)
        goto cleanup;
    if (shift == 0) {
        memcpy(remainder->storage, u, n * sizeof(*remainder->storage));
    } else {
        mint_shift_right_bits_raw(remainder->storage, u, n, shift);
    }
    remainder->length = n;
    remainder->sign = 1;
    mint_normalise(remainder);
    if (!mint_is_zero_internal(quotient))
        quotient->sign = 1;
    if (!mint_is_zero_internal(remainder))
        remainder->sign = 1;
    rc = 0;

cleanup:
    free(u);
    free(v);
    return rc;
}

int mint_mod_abs(const mint_t *numerator, const mint_t *denominator,
                        mint_t *remainder)
{
    uint64_t *u = NULL;
    uint64_t *v = NULL;
    size_t n, m, j;
    unsigned shift;
    int rc = -1;

    if (!numerator || !denominator || !remainder)
        return -1;
    if (mint_is_zero_internal(denominator))
        return -1;

    mi_clear(remainder);

    if (mint_cmp_abs(numerator, denominator) < 0) {
        if (mint_copy_value(remainder, numerator) != 0)
            return -1;
        remainder->sign = remainder->length == 0 ? 0 : 1;
        return 0;
    }

    if (denominator->length == 1) {
        uint64_t rem_mag = mint_mod_u64(numerator, denominator->storage[0]);

        return mint_set_magnitude_u64(remainder, rem_mag, rem_mag == 0 ? 0 : 1);
    }

    n = denominator->length;
    m = numerator->length - n;
    u = calloc(numerator->length + 1, sizeof(*u));
    v = malloc(n * sizeof(*v));
    if (!u || !v)
        goto cleanup;

    shift = mint_clz64(denominator->storage[n - 1]);
    u[numerator->length] = mint_shift_left_bits_raw(u, numerator->storage,
                                                    numerator->length, shift);
    (void)mint_shift_left_bits_raw(v, denominator->storage, n, shift);

    {
        uint64_t v1 = v[n - 1];
        uint64_t v2 = v[n - 2];

        for (j = m + 1; j-- > 0;) {
            __uint128_t numhat =
                ((__uint128_t)u[j + n] << 64) | u[j + n - 1];
            __uint128_t qhat_est = numhat / v1;
            __uint128_t rhat_est = numhat % v1;
            uint64_t qhat;
            size_t i;
            uint64_t carry;
            uint64_t borrow;

            if (qhat_est > UINT64_MAX) {
                qhat = UINT64_MAX;
                rhat_est = numhat - (__uint128_t)qhat * v1;
            } else {
                qhat = (uint64_t)qhat_est;
            }

            while ((__uint128_t)qhat * v2 >
                   (rhat_est << 64) + u[j + n - 2]) {
                qhat--;
                rhat_est += v1;
                if (rhat_est > UINT64_MAX)
                    break;
            }

            if (qhat == 0)
                continue;

            carry = 0;
            borrow = 0;
            for (i = 0; i < n; ++i) {
                __uint128_t prod = (__uint128_t)qhat * v[i] + carry;
                __uint128_t rhs = (__uint128_t)(uint64_t)prod + borrow;
                uint64_t ui = u[j + i];

                u[j + i] = (uint64_t)((__uint128_t)ui - rhs);
                borrow = ((__uint128_t)ui < rhs) ? 1u : 0u;
                carry = (uint64_t)(prod >> 64);
            }

            {
                __uint128_t rhs = (__uint128_t)carry + borrow;
                uint64_t ui = u[j + n];

                u[j + n] = (uint64_t)((__uint128_t)ui - rhs);
                borrow = ((__uint128_t)ui < rhs) ? 1u : 0u;
            }

            if (borrow != 0) {
                __uint128_t add_carry = 0;
                size_t i;

                for (i = 0; i < n; ++i) {
                    __uint128_t sum =
                        (__uint128_t)u[j + i] + v[i] + add_carry;

                    u[j + i] = (uint64_t)sum;
                    add_carry = sum >> 64;
                }
                u[j + n] += (uint64_t)add_carry;
            }
        }
    }

    if (mint_ensure_capacity(remainder, n) != 0)
        goto cleanup;
    if (shift == 0) {
        memcpy(remainder->storage, u, n * sizeof(*remainder->storage));
    } else {
        mint_shift_right_bits_raw(remainder->storage, u, n, shift);
    }
    remainder->length = n;
    remainder->sign = 1;
    mint_normalise(remainder);
    if (!mint_is_zero_internal(remainder))
        remainder->sign = 1;
    rc = 0;

cleanup:
    free(u);
    free(v);
    return rc;
}

int mint_sqrt_initial_guess(mint_t *guess, const mint_t *value)
{
    size_t bitlen;
    size_t prefix_bits;
    long shift_bits;
    __uint128_t prefix;
    uint64_t root;

    bitlen = mint_bit_length_internal(value);
    if (bitlen == 0) {
        mi_clear(guess);
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
    if (mint_add_small(guess, 1) != 0)
        return -1;

    shift_bits = (long)((bitlen - prefix_bits) / 2);
    return mint_shl_inplace(guess, shift_bits);
}

int mint_get_ulong_if_fits(const mint_t *mint, unsigned long *out)
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

int mint_isprime_sieved_upto_ulong(const mint_t *mint, unsigned long limit)
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

int mint_detect_power_of_two_exponent(const mint_t *mint, size_t *exponent_out)
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

int mint_isprime_small_ulong(unsigned long n)
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

size_t mint_find_wheel210_index(uint64_t rem)
{
    size_t i;

    for (i = 0; i < sizeof(mint_wheel210_residues) / sizeof(mint_wheel210_residues[0]); ++i) {
        if (mint_wheel210_residues[i] == rem)
            return i;
    }

    return SIZE_MAX;
}

int mint_adjust_to_next_wheel210(mint_t *mint)
{
    uint64_t rem;
    size_t i;

    if (!mint)
        return -1;
    if (mi_cmp_long(mint, 11) < 0)
        return 0;

    rem = mint_mod_u64(mint, 210);
    for (i = 0; i < sizeof(mint_wheel210_residues) / sizeof(mint_wheel210_residues[0]); ++i) {
        if (rem == mint_wheel210_residues[i])
            return 0;
        if (rem < mint_wheel210_residues[i])
            return mi_add_long(mint, (long)(mint_wheel210_residues[i] - rem));
    }

    return mi_add_long(mint, (long)(210u - rem + mint_wheel210_residues[0]));
}

int mint_adjust_to_prev_wheel210(mint_t *mint)
{
    uint64_t rem;
    size_t i;
    size_t count = sizeof(mint_wheel210_residues) / sizeof(mint_wheel210_residues[0]);

    if (!mint)
        return -1;
    if (mi_cmp_long(mint, 11) < 0)
        return 0;

    rem = mint_mod_u64(mint, 210);
    for (i = count; i-- > 0;) {
        if (rem == mint_wheel210_residues[i])
            return 0;
        if (rem > mint_wheel210_residues[i])
            return mi_sub_long(mint, (long)(rem - mint_wheel210_residues[i]));
    }

    return mi_sub_long(mint,
                         (long)(rem + (210u - mint_wheel210_residues[count - 1])));
}

long mint_nextprime_wheel210_step(uint64_t rem)
{
    size_t idx = mint_find_wheel210_index(rem);
    size_t count = sizeof(mint_wheel210_residues) / sizeof(mint_wheel210_residues[0]);

    if (idx == SIZE_MAX)
        return -1;
    if (idx + 1 < count)
        return (long)(mint_wheel210_residues[idx + 1] - mint_wheel210_residues[idx]);

    return (long)(210u - mint_wheel210_residues[idx] + mint_wheel210_residues[0]);
}

long mint_prevprime_wheel210_step(uint64_t rem)
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

int mint_isprime_lucas_lehmer(const mint_t *mersenne, size_t exponent)
{
    mint_t *state = NULL;
    mint_t *scratch = NULL;
    size_t i;

    if (!mersenne || exponent < 2)
        return 0;
    if (exponent == 2)
        return 1;

    state = mi_create_long(4);
    scratch = mi_new();
    if (!state || !scratch) {
        mi_free(state);
        mi_free(scratch);
        return 0;
    }

    for (i = 0; i < exponent - 2; ++i) {
        if (mi_square(state) != 0 ||
            mint_sub_small(state, 2u) != 0 ||
            mint_mod_mersenne_with_scratch(state, exponent, mersenne, scratch) != 0) {
            mi_free(state);
            mi_free(scratch);
            return 0;
        }
    }

    i = mint_is_zero_internal(state);
    mi_free(state);
    mi_free(scratch);
    return (int)i;
}

int mint_isprime_strong_probable_prime_base2(const mint_t *mint)
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
        mi_free(n_minus_one);
        return 0;
    }

    d = mint_dup_value(n_minus_one);
    if (!d) {
        mi_free(n_minus_one);
        return 0;
    }

    while (mint_is_even_internal(d)) {
        if (mint_shr_inplace(d, 1) != 0) {
            mi_free(d);
            mi_free(n_minus_one);
            return 0;
        }
        s++;
    }

    x = mi_create_long(2);
    if (!x) {
        mi_free(d);
        mi_free(n_minus_one);
        return 0;
    }

    if (mint_powmod_inplace(x, d, mint) != 0) {
        mi_free(x);
        mi_free(d);
        mi_free(n_minus_one);
        return 0;
    }

    if (mi_cmp(x, MI_ONE) == 0 || mi_cmp(x, n_minus_one) == 0) {
        mi_free(x);
        mi_free(d);
        mi_free(n_minus_one);
        return 1;
    }

    for (size_t r = 1; r < s; ++r) {
        if (mi_square(x) != 0 || mint_mod_inplace(x, mint) != 0) {
            mi_free(x);
            mi_free(d);
            mi_free(n_minus_one);
            return 0;
        }
        if (mi_cmp(x, n_minus_one) == 0) {
            mi_free(x);
            mi_free(d);
            mi_free(n_minus_one);
            return 1;
        }
        if (mi_cmp(x, MI_ONE) == 0)
            break;
    }

    mi_free(x);
    mi_free(d);
    mi_free(n_minus_one);
    return 0;
}

int mint_isprime_lucas_selfridge(const mint_t *mint)
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

    n_plus_one = mi_clone(mint);
    d = mi_clone(mint);
    U = mi_create_long(1);
    V = mi_create_long(1);
    Qk = mi_create_long(Q);
    if (!n_plus_one || !d || !U || !V || !Qk)
        goto cleanup;

    if (mi_inc(n_plus_one) != 0)
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
        mint_t *u2 = mi_clone(U);
        mint_t *v2 = mi_clone(V);
        mint_t *q2 = mi_clone(Qk);
        mint_t *tmp = NULL;
        mint_t *new_u = NULL;
        mint_t *new_v = NULL;

        if (!u2 || !v2 || !q2)
            goto loop_fail;

        if (mi_mul(u2, V) != 0 || mint_mod_positive_inplace(u2, mint) != 0)
            goto loop_fail;

        if (mi_square(v2) != 0 || mint_mod_positive_inplace(v2, mint) != 0)
            goto loop_fail;
        tmp = mi_clone(Qk);
        if (!tmp)
            goto loop_fail;
        if (mi_mul_long(tmp, 2) != 0 || mi_sub(v2, tmp) != 0 ||
            mint_mod_positive_inplace(v2, mint) != 0)
            goto loop_fail;
        mi_free(tmp);
        tmp = NULL;

        if (mi_square(q2) != 0 || mint_mod_positive_inplace(q2, mint) != 0)
            goto loop_fail;

        mi_free(U);
        mi_free(V);
        mi_free(Qk);
        U = u2;
        V = v2;
        Qk = q2;
        u2 = v2 = q2 = NULL;

        if (mint_get_bit(d, i - 1)) {
            new_u = mi_clone(U);
            new_v = mi_clone(U);

            if (!new_u || !new_v) {
                mi_free(new_u);
                mi_free(new_v);
                goto cleanup;
            }

            if (mi_add(new_u, V) != 0 ||
                mint_mod_positive_inplace(new_u, mint) != 0 ||
                mint_half_mod_odd_inplace(new_u, mint) != 0)
                goto loop_fail2;

            if (mi_mul_long(new_v, D) != 0 ||
                mi_add(new_v, V) != 0 ||
                mint_mod_positive_inplace(new_v, mint) != 0 ||
                mint_half_mod_odd_inplace(new_v, mint) != 0)
                goto loop_fail2;

            if (mi_mul_long(Qk, Q) != 0 ||
                mint_mod_positive_inplace(Qk, mint) != 0)
                goto loop_fail2;

            mi_free(U);
            mi_free(V);
            U = new_u;
            V = new_v;
        }
        continue;

loop_fail2:
        mi_free(new_u);
        mi_free(new_v);
        goto cleanup;

loop_fail:
        mi_free(u2);
        mi_free(v2);
        mi_free(q2);
        mi_free(tmp);
        goto cleanup;
    }

    if (mint_is_zero_internal(U) || mint_is_zero_internal(V)) {
        result = 1;
        goto cleanup;
    }

    for (i = 0; i < s; ++i) {
        mint_t *tmp = mi_clone(Qk);

        if (!tmp)
            goto cleanup;
        if (mi_square(V) != 0 || mint_mod_positive_inplace(V, mint) != 0)
            goto cleanup_loop;
        if (mi_mul_long(tmp, 2) != 0 || mi_sub(V, tmp) != 0 ||
            mint_mod_positive_inplace(V, mint) != 0)
            goto cleanup_loop;
        mi_free(tmp);
        tmp = NULL;

        if (mint_is_zero_internal(V)) {
            result = 1;
            goto cleanup;
        }

        if (mi_square(Qk) != 0 || mint_mod_positive_inplace(Qk, mint) != 0)
            goto cleanup;
        continue;

cleanup_loop:
        mi_free(tmp);
        goto cleanup;
    }

cleanup:
    mi_free(n_plus_one);
    mi_free(d);
    mi_free(U);
    mi_free(V);
    mi_free(Qk);
    return result;
}

mint_primality_result_t mint_prove_prime_pocklington(const mint_t *mint)
{
    mint_t *n_minus_one = NULL;
    mint_factors_t *factors = NULL;
    mint_t *proven_part = NULL;
    mint_t *proven_sq = NULL;
    mint_t *remaining = NULL;
    size_t i;
    mint_primality_result_t result = MI_PRIMALITY_UNKNOWN;
    static const unsigned long bases[] = {
        2ul, 3ul, 5ul, 7ul, 11ul, 13ul, 17ul, 19ul,
        23ul, 29ul, 31ul, 37ul, 41ul, 43ul, 47ul
    };

    if (!mint || mint->sign <= 0 || mi_cmp(mint, MI_ONE) <= 0)
        return MI_PRIMALITY_COMPOSITE;

    n_minus_one = mint_dup_value(mint);
    if (!n_minus_one)
        goto cleanup;
    if (mint_sub_small(n_minus_one, 1) != 0)
        goto cleanup;

    factors = calloc(1, sizeof(*factors));
    proven_part = mi_create_long(1);
    proven_sq = mi_new();
    remaining = mi_new();
    if (!factors || !proven_part || !proven_sq || !remaining)
        goto cleanup;
    if (mint_collect_proven_factors_partial(n_minus_one, factors, remaining) != 0 ||
        factors->count == 0)
        goto cleanup;
    for (i = 0; i < factors->count; ++i) {
        mint_t *pow = mi_clone(factors->items[i].prime);

        if (!pow)
            goto cleanup;
        if (mi_pow(pow, factors->items[i].exponent) != 0 ||
            mi_mul(proven_part, pow) != 0) {
            mi_free(pow);
            goto cleanup;
        }
        mi_free(pow);
    }

    if (mint_copy_value(proven_sq, proven_part) != 0 ||
        mi_square(proven_sq) != 0 ||
        mi_cmp(proven_sq, mint) <= 0)
        goto cleanup;

    for (i = 0; i < sizeof(bases) / sizeof(bases[0]); ++i) {
        mint_t *a = mi_create_ulong(bases[i]);
        mint_t *check = NULL;
        size_t j;
        int all_good = 1;

        if (!a)
            goto cleanup;
        if (mi_cmp(a, mint) >= 0) {
            mi_free(a);
            continue;
        }

        check = mi_clone(a);
        if (!check) {
            mi_free(a);
            goto cleanup;
        }
        if (mi_powmod(check, n_minus_one, mint) != 0) {
            mi_free(check);
            mi_free(a);
            goto cleanup;
        }
        if (mi_cmp(check, MI_ONE) != 0) {
            mi_free(check);
            mi_free(a);
            continue;
        }
        mi_free(check);
        check = NULL;

        for (j = 0; j < factors->count; ++j) {
            mint_t *exp = mi_clone(n_minus_one);
            mint_t *term = NULL;
            mint_t *g = NULL;

            if (!exp) {
                mi_free(a);
                goto cleanup;
            }
            if (mi_div(exp, factors->items[j].prime, NULL) != 0) {
                mi_free(exp);
                mi_free(a);
                goto cleanup;
            }

            term = mi_clone(a);
            g = mi_new();
            if (!term || !g) {
                mi_free(exp);
                mi_free(term);
                mi_free(g);
                mi_free(a);
                goto cleanup;
            }
            if (mi_powmod(term, exp, mint) != 0 ||
                mi_sub_long(term, 1) != 0 ||
                mint_copy_value(g, term) != 0 ||
                mi_gcd(g, mint) != 0) {
                mi_free(exp);
                mi_free(term);
                mi_free(g);
                mi_free(a);
                goto cleanup;
            }
            if (mi_cmp(g, MI_ONE) != 0)
                all_good = 0;

            mi_free(exp);
            mi_free(term);
            mi_free(g);
            if (!all_good)
                break;
        }

        mi_free(a);
        if (all_good) {
            result = MI_PRIMALITY_PRIME;
            goto cleanup;
        }
    }

cleanup:
    mi_free(n_minus_one);
    mi_factors_free(factors);
    mi_free(proven_part);
    mi_free(proven_sq);
    mi_free(remaining);
    return result;
}

mint_primality_result_t mint_prove_prime_ec_nplus1_supersingular(const mint_t *mint)
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

    if (!mint || mint->sign <= 0 || mi_cmp(mint, MI_ONE) <= 0)
        return MI_PRIMALITY_COMPOSITE;
    if (mint_mod_u64(mint, 3u) != 2u)
        return MI_PRIMALITY_UNKNOWN;

    n_plus_one = mint_dup_value(mint);
    bound = mi_new();
    remaining = mi_new();
    factors = calloc(1, sizeof(*factors));
    if (!n_plus_one || !bound || !remaining || !factors)
        goto cleanup;
    if (mint_add_small(n_plus_one, 1) != 0 ||
        mint_copy_value(bound, mint) != 0 ||
        mi_sqrt(bound) != 0 ||
        mi_sqrt(bound) != 0 ||
        mint_add_small(bound, 1) != 0 ||
        mi_square(bound) != 0)
        goto cleanup;
    if (mint_collect_proven_factors_partial(n_plus_one, factors, remaining) != 0)
        goto cleanup;

    for (i = 0; i < factors->count; ++i) {
        mint_t *q = factors->items[i].prime;

        if (mi_cmp(q, bound) <= 0)
            continue;

        for (j = 0; j < sizeof(seeds) / sizeof(seeds[0]); ++j) {
            mint_t *a = mi_new();
            mint_t *disc = mi_new();
            mint_t *g = mi_new();
            mint_t *h = mint_dup_value(n_plus_one);
            mint_ec_point_t p = { 0 };
            mint_ec_point_t r = { 0 };
            mint_ec_point_t s = { 0 };
            mint_primality_result_t result = MI_PRIMALITY_UNKNOWN;
            mint_ec_step_result_t step;

            if (!a || !disc || !g || !h)
                goto stage_cleanup;
            if (mint_ec_point_init(&p) != 0 ||
                mint_ec_point_init(&r) != 0 ||
                mint_ec_point_init(&s) != 0)
                goto stage_cleanup;

            if (mi_set_long(a, 0) != 0 ||
                mi_set_long(disc, 27) != 0 ||
                mint_copy_value(g, disc) != 0 ||
                mi_gcd(g, mint) != 0)
                goto stage_cleanup;
            if (mi_cmp(g, MI_ONE) != 0) {
                result = mi_cmp(g, mint) < 0
                    ? MI_PRIMALITY_COMPOSITE
                    : MI_PRIMALITY_UNKNOWN;
                goto stage_cleanup;
            }

            if (mi_set_long(p.x, seeds[j].x) != 0 ||
                mi_set_long(p.y, seeds[j].y) != 0 ||
                mint_mod_positive_inplace(p.x, mint) != 0 ||
                mint_mod_positive_inplace(p.y, mint) != 0)
                goto stage_cleanup;
            p.infinity = 0;

            if (mi_div(h, q, NULL) != 0)
                goto stage_cleanup;

            step = mint_ec_scalar_mul_big(&p, h, a, mint, &r);
            if (step == MINT_EC_STEP_COMPOSITE) {
                result = MI_PRIMALITY_COMPOSITE;
                goto stage_cleanup;
            }
            if (step != MINT_EC_STEP_OK || r.infinity)
                goto stage_cleanup;

            step = mint_ec_scalar_mul_big(&r, q, a, mint, &s);
            if (step == MINT_EC_STEP_COMPOSITE) {
                result = MI_PRIMALITY_COMPOSITE;
                goto stage_cleanup;
            }
            if (step != MINT_EC_STEP_OK)
                goto stage_cleanup;

            if (s.infinity) {
                result = MI_PRIMALITY_PRIME;
                goto stage_cleanup;
            }

stage_cleanup:
            mi_free(a);
            mi_free(disc);
            mi_free(g);
            mi_free(h);
            mint_ec_point_clear(&p);
            mint_ec_point_clear(&r);
            mint_ec_point_clear(&s);
            if (result != MI_PRIMALITY_UNKNOWN) {
                mi_free(n_plus_one);
                mi_free(bound);
                mi_free(remaining);
                mi_factors_free(factors);
                return result;
            }
        }
    }

cleanup:
    mi_free(n_plus_one);
    mi_free(bound);
    mi_free(remaining);
    mi_factors_free(factors);
    return MI_PRIMALITY_UNKNOWN;
}

mint_primality_result_t mint_prove_prime_ec_witness(const mint_t *mint)
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

    if (!mint || mint->sign <= 0 || mi_cmp(mint, MI_ONE) <= 0)
        return MI_PRIMALITY_COMPOSITE;

    for (i = 0; i < sizeof(a_values) / sizeof(a_values[0]); ++i) {
        for (j = 0; j < sizeof(seeds) / sizeof(seeds[0]); ++j) {
            mint_t *a = mi_create_long(a_values[i]);
            mint_t *b = NULL;
            mint_t *disc = NULL;
            mint_t *g = NULL;
            mint_t *xv = NULL;
            mint_ec_point_t p = { 0 };
            mint_ec_point_t q = { 0 };
            mint_primality_result_t result = MI_PRIMALITY_UNKNOWN;

            if (!a)
                return MI_PRIMALITY_UNKNOWN;
            if (mint_mod_positive_inplace(a, mint) != 0)
                goto ec_cleanup;

            b = mi_create_long(seeds[j].y);
            disc = mi_create_long(seeds[j].x);
            g = mi_new();
            xv = mi_create_long(seeds[j].x);
            if (!b || !disc || !g || !xv)
                goto ec_cleanup;

            if (mi_square(b) != 0 ||
                mi_pow(disc, 3) != 0 ||
                mi_mul_long(disc, -1) != 0)
                goto ec_cleanup;

            if (mi_mul(xv, a) != 0 ||
                mi_sub(b, xv) != 0 ||
                mi_sub(b, disc) != 0)
                goto ec_cleanup;

            if (mint_mod_positive_inplace(b, mint) != 0)
                goto ec_cleanup;

            if (mint_copy_value(disc, a) != 0 ||
                mi_pow(disc, 3) != 0 ||
                mi_mul_long(disc, 4) != 0)
                goto ec_cleanup;
            if (mint_copy_value(g, b) != 0 ||
                mi_square(g) != 0 ||
                mi_mul_long(g, 27) != 0 ||
                mi_add(disc, g) != 0 ||
                mint_mod_positive_inplace(disc, mint) != 0 ||
                mint_copy_value(g, disc) != 0 ||
                mi_gcd(g, mint) != 0)
                goto ec_cleanup;

            if (mi_cmp(g, MI_ONE) != 0) {
                result = mi_cmp(g, mint) < 0
                    ? MI_PRIMALITY_COMPOSITE
                    : MI_PRIMALITY_UNKNOWN;
                if (result != MI_PRIMALITY_UNKNOWN)
                    goto ec_cleanup;
                goto ec_cleanup;
            }

            if (mint_ec_point_init(&p) != 0 || mint_ec_point_init(&q) != 0)
                goto ec_cleanup;
            if (mi_set_long(p.x, seeds[j].x) != 0 ||
                mi_set_long(p.y, seeds[j].y) != 0 ||
                mint_mod_positive_inplace(p.x, mint) != 0 ||
                mint_mod_positive_inplace(p.y, mint) != 0)
                goto ec_cleanup;
            p.infinity = 0;

            for (k = 0; k < sizeof(scalars) / sizeof(scalars[0]); ++k) {
                mint_ec_step_result_t step =
                    mint_ec_scalar_mul(&p, scalars[k], a, mint, &q);

                if (step == MINT_EC_STEP_COMPOSITE) {
                    result = MI_PRIMALITY_COMPOSITE;
                    goto ec_cleanup;
                }
                if (step == MINT_EC_STEP_ERROR)
                    break;
                if (mint_ec_point_copy(&p, &q) != 0)
                    break;
            }

ec_cleanup:
            mi_free(a);
            mi_free(b);
            mi_free(disc);
            mi_free(g);
            mi_free(xv);
            mint_ec_point_clear(&p);
            mint_ec_point_clear(&q);
            if (result != MI_PRIMALITY_UNKNOWN)
                return result;
        }
    }

    return MI_PRIMALITY_UNKNOWN;
}

int mint_collect_proven_factors_partial(const mint_t *n,
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
    if (mi_set_long(remaining, 1) != 0)
        return -1;
    if (mi_cmp(n, MI_ONE) <= 0)
        return 0;

    work = mint_dup_value(n);
    if (!work)
        goto fail;
    work->sign = mint_is_zero_internal(work) ? 0 : 1;

    for (i = 0; i < sizeof(mint_small_primes) / sizeof(mint_small_primes[0]); ++i) {
        unsigned long exponent = 0;
        mint_t *prime = mi_create_ulong(mint_small_primes[i]);

        if (!prime)
            goto fail;

        while (mi_cmp(work, prime) >= 0) {
            mint_t *rem = mi_clone(work);
            int divisible;

            if (!rem) {
                mi_free(prime);
                goto fail;
            }
            if (mi_mod(rem, prime) != 0) {
                mi_free(rem);
                mi_free(prime);
                goto fail;
            }
            divisible = mi_is_zero(rem);
            mi_free(rem);
            if (!divisible)
                break;
            if (mi_div(work, prime, NULL) != 0) {
                mi_free(prime);
                goto fail;
            }
            exponent++;
        }

        if (exponent > 0 && mint_factors_append(proven, prime, exponent) != 0) {
            mi_free(prime);
            goto fail;
        }
        mi_free(prime);
    }

    if (mi_cmp(work, MI_ONE) <= 0) {
        mi_free(work);
        return 0;
    }

    if (mint_prove_prime_internal(work) == MI_PRIMALITY_PRIME) {
        int rc = mint_factors_append(proven, work, 1);

        mi_free(work);
        return rc;
    }

    factor = mi_new();
    other = mi_clone(work);
    rem_factor = mi_new();
    rem_other = mi_new();
    if (!factor || !other || !rem_factor || !rem_other)
        goto fail;
    if (mint_pollard_rho_factor(work, factor) != 0 ||
        mi_div(other, factor, NULL) != 0)
        goto keep_remaining;

    if (mint_collect_proven_factors_partial(factor, proven, rem_factor) != 0 ||
        mint_collect_proven_factors_partial(other, proven, rem_other) != 0)
        goto fail;
    if (mint_copy_value(remaining, rem_factor) != 0 ||
        mi_mul(remaining, rem_other) != 0)
        goto fail;

    mi_free(work);
    mi_free(factor);
    mi_free(other);
    mi_free(rem_factor);
    mi_free(rem_other);
    return 0;

keep_remaining:
    if (mint_copy_value(remaining, work) != 0)
        goto fail;
    mi_free(work);
    mi_free(factor);
    mi_free(other);
    mi_free(rem_factor);
    mi_free(rem_other);
    return 0;

fail:
    mi_free(work);
    mi_free(factor);
    mi_free(other);
    mi_free(rem_factor);
    mi_free(rem_other);
    return -1;
}

mint_primality_result_t mint_prove_prime_internal(const mint_t *mint)
{
    unsigned long ulong_limit;
    size_t mersenne_exponent;
    size_t i;
    uint64_t rem;

    if (!mint || mint->sign <= 0)
        return MI_PRIMALITY_COMPOSITE;
    if (mi_cmp(mint, MI_ONE) <= 0)
        return MI_PRIMALITY_COMPOSITE;
    if (mint->length == 1) {
        uint64_t n = mint->storage[0];

        if (n == 2 || n == 3)
            return MI_PRIMALITY_PRIME;
    }
    if (mint_is_even_internal(mint))
        return MI_PRIMALITY_COMPOSITE;

    for (i = 0; i < sizeof(mint_small_primes) / sizeof(mint_small_primes[0]); ++i) {
        if (mint->length == 1 && mint->storage[0] == mint_small_primes[i])
            return MI_PRIMALITY_PRIME;
        rem = mint_mod_u64(mint, mint_small_primes[i]);
        if (rem == 0)
            return MI_PRIMALITY_COMPOSITE;
    }

    if (mint_get_ulong_if_fits(mint, &ulong_limit)) {
        unsigned long root_limit = 1;

        while ((root_limit + 1) <= ulong_limit / (root_limit + 1))
            root_limit++;
        return mint_isprime_sieved_upto_ulong(mint, root_limit) > 0
            ? MI_PRIMALITY_PRIME
            : MI_PRIMALITY_COMPOSITE;
    }

    {
        mint_t *plus_one = mint_dup_value(mint);

        if (!plus_one)
            return MI_PRIMALITY_UNKNOWN;
        if (mint_add_small(plus_one, 1) != 0) {
            mi_free(plus_one);
            return MI_PRIMALITY_UNKNOWN;
        }
        if (mint_detect_power_of_two_exponent(plus_one, &mersenne_exponent) &&
            mint_isprime_small_ulong((unsigned long)mersenne_exponent)) {
            mint_primality_result_t rr =
                mint_isprime_lucas_lehmer(mint, mersenne_exponent)
                    ? MI_PRIMALITY_PRIME
                    : MI_PRIMALITY_COMPOSITE;

            mi_free(plus_one);
            return rr;
        }
        mi_free(plus_one);
    }

    if (mint_isprime_strong_probable_prime_base2(mint) <= 0)
        return MI_PRIMALITY_COMPOSITE;
    if (mint_isprime_lucas_selfridge(mint) <= 0)
        return MI_PRIMALITY_COMPOSITE;

    {
        mint_primality_result_t exact = mint_prove_prime_pocklington(mint);

        if (exact != MI_PRIMALITY_UNKNOWN)
            return exact;
    }

    {
        mint_primality_result_t exact =
            mint_prove_prime_ec_nplus1_supersingular(mint);

        if (exact != MI_PRIMALITY_UNKNOWN)
            return exact;
    }

    return mint_prove_prime_ec_witness(mint);
}

int mint_factors_append(mint_factors_t *factors,
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

int mint_factor_item_cmp(const void *lhs, const void *rhs)
{
    const mint_factor_t *a = (const mint_factor_t *)lhs;
    const mint_factor_t *b = (const mint_factor_t *)rhs;

    return mi_cmp(a->prime, b->prime);
}

void mint_factors_sort_and_compress(mint_factors_t *factors)
{
    size_t read_idx, write_idx;

    if (!factors || factors->count == 0)
        return;

    qsort(factors->items, factors->count, sizeof(*factors->items),
          mint_factor_item_cmp);

    write_idx = 0;
    for (read_idx = 0; read_idx < factors->count; ++read_idx) {
        if (write_idx > 0 &&
            mi_cmp(factors->items[write_idx - 1].prime,
                     factors->items[read_idx].prime) == 0) {
            factors->items[write_idx - 1].exponent +=
                factors->items[read_idx].exponent;
            mi_free(factors->items[read_idx].prime);
            continue;
        }

        if (write_idx != read_idx)
            factors->items[write_idx] = factors->items[read_idx];
        write_idx++;
    }

    factors->count = write_idx;
}

int mint_pollard_rho_step(mint_t *state, const mint_t *c,
                                 const mint_t *modulus)
{
    if (mi_square(state) != 0)
        return -1;
    if (mi_add(state, c) != 0)
        return -1;
    return mi_mod(state, modulus);
}

int mint_pollard_rho_factor(const mint_t *n, mint_t *factor)
{
    unsigned long c_value;

    if (!n || !factor || mint_is_immortal(factor) || n->sign <= 0)
        return -1;
    if (mi_is_even(n))
        return mi_set_long(factor, 2);

    for (c_value = 1; c_value <= 16; ++c_value) {
        mint_t *x = mi_create_long(2);
        mint_t *y = mi_create_long(2);
        mint_t *c = mi_create_long((long)c_value);
        mint_t *d = mi_create_long(1);
        mint_t *diff = mi_new();

        if (!x || !y || !c || !d || !diff) {
            mi_free(x);
            mi_free(y);
            mi_free(c);
            mi_free(d);
            mi_free(diff);
            return -1;
        }

        while (mi_cmp(d, MI_ONE) == 0) {
            if (mint_pollard_rho_step(x, c, n) != 0 ||
                mint_pollard_rho_step(y, c, n) != 0 ||
                mint_pollard_rho_step(y, c, n) != 0 ||
                mint_abs_diff(diff, x, y) != 0 ||
                mi_gcd(diff, n) != 0) {
                mi_free(x);
                mi_free(y);
                mi_free(c);
                mi_free(d);
                mi_free(diff);
                return -1;
            }

            if (mint_copy_value(d, diff) != 0) {
                mi_free(x);
                mi_free(y);
                mi_free(c);
                mi_free(d);
                mi_free(diff);
                return -1;
            }
            d->sign = mint_is_zero_internal(d) ? 0 : 1;
        }

        if (mi_cmp(d, n) != 0 && mi_cmp(d, MI_ONE) > 0) {
            int rc = mint_copy_value(factor, d);

            if (rc == 0)
                factor->sign = mint_is_zero_internal(factor) ? 0 : 1;
            mi_free(x);
            mi_free(y);
            mi_free(c);
            mi_free(d);
            mi_free(diff);
            return rc;
        }

        mi_free(x);
        mi_free(y);
        mi_free(c);
        mi_free(d);
        mi_free(diff);
    }

    return -1;
}

int mint_factor_recursive(const mint_t *n, mint_factors_t *factors)
{
    mint_t *work = NULL;
    mint_t *prime = NULL;
    mint_t *factor = NULL;
    mint_t *other = NULL;
    size_t i;

    if (!n || !factors)
        return -1;
    if (mi_cmp(n, MI_ONE) <= 0)
        return 0;
    if (mi_isprime(n))
        return mint_factors_append(factors, n, 1);

    work = mint_dup_value(n);
    if (!work)
        return -1;
    work->sign = mint_is_zero_internal(work) ? 0 : 1;

    for (i = 0; i < sizeof(mint_small_primes) / sizeof(mint_small_primes[0]); ++i) {
        unsigned long exponent = 0;

        prime = mi_create_ulong(mint_small_primes[i]);
        if (!prime) {
            mi_free(work);
            return -1;
        }

        while (mi_cmp(work, prime) >= 0) {
            mint_t *rem = mi_clone(work);
            int divisible;

            if (!rem) {
                mi_free(prime);
                mi_free(work);
                return -1;
            }
            if (mi_mod(rem, prime) != 0) {
                mi_free(rem);
                mi_free(prime);
                mi_free(work);
                return -1;
            }
            divisible = mi_is_zero(rem);
            mi_free(rem);
            if (!divisible)
                break;
            if (mi_div(work, prime, NULL) != 0) {
                mi_free(prime);
                mi_free(work);
                return -1;
            }
            exponent++;
        }

        if (exponent > 0 &&
            mint_factors_append(factors, prime, exponent) != 0) {
            mi_free(prime);
            mi_free(work);
            return -1;
        }
        mi_free(prime);
        prime = NULL;
    }

    if (mi_cmp(work, MI_ONE) <= 0) {
        mi_free(work);
        return 0;
    }
    if (mi_isprime(work)) {
        int rc = mint_factors_append(factors, work, 1);

        mi_free(work);
        return rc;
    }

    factor = mi_new();
    if (!factor) {
        mi_free(work);
        return -1;
    }
    if (mint_pollard_rho_factor(work, factor) != 0) {
        mi_free(factor);
        mi_free(work);
        return -1;
    }

    other = mi_clone(work);
    if (!other) {
        mi_free(factor);
        mi_free(work);
        return -1;
    }
    if (mi_div(other, factor, NULL) != 0) {
        mi_free(other);
        mi_free(factor);
        mi_free(work);
        return -1;
    }

    if (mint_factor_recursive(factor, factors) != 0 ||
        mint_factor_recursive(other, factors) != 0) {
        mi_free(other);
        mi_free(factor);
        mi_free(work);
        return -1;
    }

    mi_free(other);
    mi_free(factor);
    mi_free(work);
    return 0;
}
mint_t *mi_new(void)
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

mint_t *mi_create_long(long value)
{
    mint_t *mint = mi_new();

    if (!mint)
        return NULL;

    if (mi_set_long(mint, value) != 0) {
        mi_free(mint);
        return NULL;
    }

    return mint;
}

mint_t *mi_create_ulong(unsigned long value)
{
    mint_t *mint = mi_new();

    if (!mint)
        return NULL;

    if (mi_set_ulong(mint, value) != 0) {
        mi_free(mint);
        return NULL;
    }

    return mint;
}

mint_t *mi_create_2pow(uint64_t n)
{
    mint_t *mint = mi_new();
    size_t limb = (size_t)(n / 64);
    unsigned shift = (unsigned)(n % 64);

    if (!mint)
        return NULL;

    if (mint_ensure_capacity(mint, limb + 1) != 0) {
        mi_free(mint);
        return NULL;
    }

    mint->storage[limb] = ((uint64_t)1u) << shift;
    mint->length = limb + 1;
    mint->sign = 1;
    return mint;
}
mint_t *mi_clone(const mint_t *mint)
{
    return mint_dup_value(mint);
}

void mi_clear(mint_t *mint)
{
    if (!mint || mint_is_immortal(mint))
        return;

    free(mint->storage);
    mint->storage = NULL;
    mint->sign = 0;
    mint->length = 0;
    mint->capacity = 0;
}
void mi_free(mint_t *mint)
{
    if (!mint || mint_is_immortal(mint))
        return;
    mi_clear(mint);
    free(mint);
}
int mi_set_long(mint_t *mint, long value)
{
    uint64_t magnitude;
    short sign;

    if (!mint || mint_is_immortal(mint))
        return -1;

    if (value == 0) {
        mi_clear(mint);
        return 0;
    }

    sign = value < 0 ? (short)-1 : (short)1;
    if (value < 0)
        magnitude = (uint64_t)(-(value + 1)) + 1u;
    else
        magnitude = (uint64_t)value;

    return mint_set_magnitude_u64(mint, magnitude, sign);
}

int mi_set_ulong(mint_t *mint, unsigned long value)
{
    if (!mint || mint_is_immortal(mint))
        return -1;
    if (value == 0) {
        mi_clear(mint);
        return 0;
    }
    return mint_set_magnitude_u64(mint, (uint64_t)value, 1);
}
