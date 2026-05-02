#include "mint_internal.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
int mi_inc(mint_t *mint)
{
    if (!mint)
        return -1;
    return mint_add_inplace(mint, MI_ONE);
}

int mi_dec(mint_t *mint)
{
    if (!mint)
        return -1;
    return mi_sub(mint, MI_ONE);
}

int mi_add_long(mint_t *mint, long value)
{
    uint64_t magnitude;
    short sign;

    if (!mint)
        return -1;
    if (value == 0)
        return 0;
    sign = value < 0 ? -1 : 1;
    magnitude = value < 0 ? (uint64_t)((unsigned long)(-(value + 1)) + 1ul)
                          : (uint64_t)(unsigned long)value;
    return mint_add_signed_word_inplace(mint, magnitude, sign);
}

int mi_sub_long(mint_t *mint, long value)
{
    uint64_t magnitude;
    short sign;

    if (!mint)
        return -1;
    if (value == 0)
        return 0;
    sign = value < 0 ? 1 : -1;
    magnitude = value < 0 ? (uint64_t)((unsigned long)(-(value + 1)) + 1ul)
                          : (uint64_t)(unsigned long)value;
    return mint_add_signed_word_inplace(mint, magnitude, sign);
}

int mi_mul_long(mint_t *mint, long value)
{
    unsigned long magnitude;
    short sign;

    if (!mint)
        return -1;
    if (value == 0) {
        mi_clear(mint);
        return 0;
    }
    if (value == 1)
        return 0;
    if (value == -1)
        return mi_neg(mint);

    sign = value < 0 ? -1 : 1;
    magnitude = value < 0 ? (unsigned long)(-(value + 1)) + 1ul
                          : (unsigned long)value;

    if (mint->sign == 0)
        return 0;
    if (mint_mul_word_inplace(mint, (uint64_t)magnitude) != 0)
        return -1;
    if (!mint_is_zero_internal(mint))
        mint->sign = (short)(mint->sign * sign);
    return 0;
}
int mint_add_inplace(mint_t *mint, const mint_t *other)
{
    int cmp;

    if (!mint || !other || mint_is_immortal(mint))
        return -1;
    if (mint_is_zero_internal(other))
        return 0;
    if (other->length == 1)
        return mint_add_signed_word_inplace(mint, other->storage[0], other->sign);
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
        mi_clear(mint);
        return 0;
    }
    if (cmp > 0) {
        mint_abs_sub_inplace(mint, other);
        return 0;
    }

    {
        uint64_t *scratch = NULL;
        size_t scratch_len = mint->length;

        if (scratch_len > 0) {
            scratch = malloc(scratch_len * sizeof(*scratch));
            if (!scratch)
                return -1;
            memcpy(scratch, mint->storage, scratch_len * sizeof(*scratch));
        }

        if (mint_ensure_capacity(mint, other->length) != 0) {
            free(scratch);
            return -1;
        }
        memcpy(mint->storage, other->storage,
               other->length * sizeof(*mint->storage));
        mint->length = other->length;
        mint_abs_sub_raw_inplace(mint->storage, &mint->length,
                                 scratch, scratch_len);
        mint->sign = mint->length == 0 ? 0 : other->sign;
        free(scratch);
        return 0;
    }
}

int mint_and_inplace(mint_t *mint, const mint_t *other)
{
    size_t i, keep;

    if (!mint || !other || mint_is_immortal(mint))
        return -1;
    if (mint_has_negative_operand(mint, other))
        return -1;
    if (mint_is_zero_internal(mint) || mint_is_zero_internal(other)) {
        mi_clear(mint);
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

int mi_cmp(const mint_t *a, const mint_t *b)
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

int mi_cmp_long(const mint_t *mint, long value)
{
    if (!mint)
        return 1;
    if (value == 0) {
        if (mint_is_zero_internal(mint))
            return 0;
        return mint->sign < 0 ? -1 : 1;
    }
    if (mint_is_zero_internal(mint))
        return value < 0 ? 1 : -1;
    if (mint->sign < 0 && value >= 0)
        return -1;
    if (mint->sign > 0 && value < 0)
        return 1;

    {
        uint64_t magnitude = value < 0
                                 ? (uint64_t)((unsigned long)(-(value + 1)) + 1ul)
                                 : (uint64_t)(unsigned long)value;
        int cmp = mint_cmp_abs_u64(mint, magnitude);

        return mint->sign < 0 ? -cmp : cmp;
    }
}

int mi_sub(mint_t *mint, const mint_t *other)
{
    int cmp;

    if (!mint || !other || mint_is_immortal(mint))
        return -1;
    if (mint_is_zero_internal(other))
        return 0;
    if (other->length == 1)
        return mint_add_signed_word_inplace(mint, other->storage[0],
                                            (short)-other->sign);
    if (mint_is_zero_internal(mint)) {
        if (mint_copy_value(mint, other) != 0)
            return -1;
        if (!mint_is_zero_internal(mint))
            mint->sign = (short)-mint->sign;
        return 0;
    }

    if (mint->sign != other->sign) {
        if (mint_abs_add_inplace(mint, other) != 0)
            return -1;
        return 0;
    }

    cmp = mint_cmp_abs(mint, other);
    if (cmp == 0) {
        mi_clear(mint);
        return 0;
    }
    if (cmp > 0) {
        mint_abs_sub_inplace(mint, other);
        return 0;
    }

    {
        uint64_t *scratch = NULL;
        size_t scratch_len = mint->length;

        if (scratch_len > 0) {
            scratch = malloc(scratch_len * sizeof(*scratch));
            if (!scratch)
                return -1;
            memcpy(scratch, mint->storage, scratch_len * sizeof(*scratch));
        }

        if (mint_ensure_capacity(mint, other->length) != 0) {
            free(scratch);
            return -1;
        }
        memcpy(mint->storage, other->storage,
               other->length * sizeof(*mint->storage));
        mint->length = other->length;
        mint_abs_sub_raw_inplace(mint->storage, &mint->length,
                                 scratch, scratch_len);
        mint->sign = mint->length == 0 ? 0 : (short)-other->sign;
        free(scratch);
        return 0;
    }
}
int mi_abs(mint_t *mint)
{
    if (!mint || mint_is_immortal(mint))
        return -1;
    if (!mint_is_zero_internal(mint))
        mint->sign = 1;
    return 0;
}

bool mi_is_zero(const mint_t *mint)
{
    return mint_is_zero_internal(mint) != 0;
}

bool mi_is_negative(const mint_t *mint)
{
    return mint && mint->sign < 0;
}

bool mi_is_even(const mint_t *mint)
{
    return mint_is_even_internal(mint) != 0;
}

bool mi_is_odd(const mint_t *mint)
{
    return mint_is_odd_internal(mint) != 0;
}

size_t mi_bit_length(const mint_t *mint)
{
    return mint_bit_length_internal(mint);
}

bool mi_test_bit(const mint_t *mint, size_t bit_index)
{
    if (!mint)
        return false;
    return mint_get_bit(mint, bit_index) != 0;
}

bool mi_fits_long(const mint_t *mint)
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

bool mi_get_long(const mint_t *mint, long *out)
{
    unsigned long magnitude;

    if (!mint || !out || !mi_fits_long(mint))
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

int mi_square(mint_t *mint)
{
    uint64_t *out;
    const uint64_t *src;
    size_t len, needed;
    size_t i, j;

    if (!mint || mint_is_immortal(mint))
        return -1;
    if (mint_is_zero_internal(mint))
        return 0;
    if (mint->length == 1) {
        if (mint_mul_word_inplace(mint, mint->storage[0]) != 0)
            return -1;
        if (!mint_is_zero_internal(mint))
            mint->sign = 1;
        return 0;
    }

    src = mint->storage;
    len = mint->length;
    needed = len * 2;
    out = calloc(needed, sizeof(*out));
    if (!out)
        return -1;

    if (len <= 4) {
        mint_mul_schoolbook_raw(out, src, len, src, len);
        free(mint->storage);
        mint->storage = out;
        mint->capacity = needed;
        mint->length = needed;
        mint_normalise(mint);
        if (!mint_is_zero_internal(mint))
            mint->sign = 1;
        return 0;
    }

    for (i = 0; i < len; ++i) {
        __uint128_t prod = (__uint128_t)src[i] * src[i];
        size_t k = i * 2;
        __uint128_t acc = (__uint128_t)out[k] + (uint64_t)prod;
        uint64_t carry = (uint64_t)(acc >> 64);

        out[k] = (uint64_t)acc;
        acc = (__uint128_t)out[k + 1] + (uint64_t)(prod >> 64) + carry;
        out[k + 1] = (uint64_t)acc;
        carry = (uint64_t)(acc >> 64);
        k += 2;
        while (carry != 0) {
            acc = (__uint128_t)out[k] + carry;
            out[k] = (uint64_t)acc;
            carry = (uint64_t)(acc >> 64);
            ++k;
        }
    }

    for (i = 0; i < len; ++i) {
        for (j = i + 1; j < len; ++j) {
            __uint128_t prod = (__uint128_t)src[i] * src[j];
            uint64_t lo = (uint64_t)prod;
            uint64_t hi = (uint64_t)(prod >> 64);
            __uint128_t mid;
            __uint128_t acc;
            uint64_t carry;
            size_t k = i + j;

            acc = (__uint128_t)out[k] + (lo << 1);
            out[k] = (uint64_t)acc;
            carry = (uint64_t)(acc >> 64);

            mid = ((__uint128_t)hi << 1) | (lo >> 63);
            acc = (__uint128_t)out[k + 1] + mid + carry;
            out[k + 1] = (uint64_t)acc;
            carry = (uint64_t)(acc >> 64);
            k += 2;

            while (carry != 0) {
                acc = (__uint128_t)out[k] + carry;
                out[k] = (uint64_t)acc;
                carry = (uint64_t)(acc >> 64);
                ++k;
            }
        }
    }

    free(mint->storage);
    mint->storage = out;
    mint->capacity = needed;
    mint->length = needed;
    mint_normalise(mint);
    if (!mint_is_zero_internal(mint))
        mint->sign = 1;
    return 0;
}
int mint_not_inplace(mint_t *mint)
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

int mint_or_inplace(mint_t *mint, const mint_t *other)
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

int mint_mul_inplace(mint_t *mint, const mint_t *other)
{
    uint64_t *out;
    const uint64_t *lhs_storage;
    const uint64_t *rhs_storage;
    size_t lhs_length;
    size_t rhs_length;
    size_t needed;
    short lhs_sign;
    short sign;

    if (!mint || !other || mint_is_immortal(mint))
        return -1;
    if (other == mint)
        return mi_square(mint);
    if (mint_is_zero_internal(mint) || mint_is_zero_internal(other)) {
        mi_clear(mint);
        return 0;
    }
    lhs_sign = mint->sign;
    if (mint_is_abs_one(other)) {
        if (other->sign < 0)
            mint->sign = (short)-mint->sign;
        return 0;
    }
    if (mint_is_abs_one(mint)) {
        if (mint_copy_value(mint, other) != 0)
            return -1;
        if (mint->sign != 0 && lhs_sign < 0)
            mint->sign = (short)-mint->sign;
        return 0;
    }
    sign = mint->sign == other->sign ? 1 : -1;

    if (other->length == 1) {
        if (mint_mul_word_inplace(mint, other->storage[0]) != 0)
            return -1;
        mint->sign = sign;
        return 0;
    }
    if (mint->length == 1) {
        uint64_t factor = mint->storage[0];
        __uint128_t carry = 0;
        size_t i;

        if (mint_ensure_capacity(mint, other->length + 1) != 0)
            return -1;

        for (i = 0; i < other->length; ++i) {
            __uint128_t prod = (__uint128_t)other->storage[i] * factor + carry;

            mint->storage[i] = (uint64_t)prod;
            carry = prod >> 64;
        }

        mint->length = other->length;
        if (carry != 0)
            mint->storage[mint->length++] = (uint64_t)carry;
        mint->sign = sign;
        mint_normalise(mint);
        mint_zero_spare_limbs(mint, mint->length);
        return 0;
    }

    lhs_length = mint->length;
    rhs_length = other->length;
    needed = lhs_length + rhs_length;
    lhs_storage = mint->storage;
    rhs_storage = other->storage;
    out = calloc(needed, sizeof(*out));
    if (!out)
        return -1;
    mint_mul_schoolbook_raw(out, lhs_storage, lhs_length, rhs_storage, rhs_length);

    free(mint->storage);
    mint->storage = out;
    mint->capacity = needed;
    mint->length = needed;
    mint->sign = sign;
    mint_normalise(mint);
    return 0;
}

int mint_neg_inplace(mint_t *mint)
{
    if (!mint || mint_is_immortal(mint))
        return -1;
    if (!mint_is_zero_internal(mint))
        mint->sign = (short)-mint->sign;
    return 0;
}

int mint_pow_inplace(mint_t *mint, unsigned long exponent)
{
    mint_t *base, *result;

    if (!mint || mint_is_immortal(mint))
        return -1;

    result = mi_create_long(1);
    base = mint_dup_value(mint);
    if (!result || !base) {
        mi_free(result);
        mi_free(base);
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

    mi_free(result);
    mi_free(base);
    return 0;

fail:
    mi_free(result);
    mi_free(base);
    return -1;
}

int mint_powmod_inplace(mint_t *mint, const mint_t *exponent, const mint_t *modulus)
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
    result = mi_create_long(1);
    if (!base || !exp_copy || !result) {
        mi_free(base);
        mi_free(exp_copy);
        mi_free(result);
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

    mi_free(base);
    mi_free(exp_copy);
    mi_free(result);
    return 0;

fail:
    mi_free(base);
    mi_free(exp_copy);
    mi_free(result);
    return -1;
}

int mint_pow10_inplace(mint_t *mint, long exponent)
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

int mint_shl_inplace(mint_t *mint, long bits)
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

int mint_shr_inplace(mint_t *mint, long bits)
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
        mi_clear(mint);
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

int mint_sqrt_inplace(mint_t *mint)
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

    guess = mi_new();
    q = mi_new();
    next = mi_new();
    one = mi_create_long(1);
    sq = mi_new();
    sq_plus = mi_new();
    if (!guess || !q || !next || !one || !sq || !sq_plus) {
        mi_free(guess);
        mi_free(q);
        mi_free(next);
        mi_free(one);
        mi_free(sq);
        mi_free(sq_plus);
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

        cmp = mi_cmp(next, guess);
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
        if (mi_cmp(sq, mint) <= 0)
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
        if (mi_cmp(sq, mint) > 0)
            break;
        if (mint_copy_value(guess, sq_plus) != 0)
            goto fail;
    }

    if (mint_copy_value(mint, guess) != 0)
        goto fail;
    mint->sign = mint_is_zero_internal(mint) ? 0 : 1;

    mi_free(guess);
    mi_free(q);
    mi_free(next);
    mi_free(one);
    mi_free(sq);
    mi_free(sq_plus);
    return 0;

fail:
    mi_free(guess);
    mi_free(q);
    mi_free(next);
    mi_free(one);
    mi_free(sq);
    mi_free(sq_plus);
    return -1;
}

int mint_xor_inplace(mint_t *mint, const mint_t *other)
{
    size_t i, max_len, min_len, old_len;

    if (!mint || !other || mint_is_immortal(mint))
        return -1;
    if (mint_has_negative_operand(mint, other))
        return -1;
    if (other == mint) {
        mi_clear(mint);
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

int mi_add(mint_t *mint, const mint_t *other)
{
    return mint_add_inplace(mint, other);
}

int mi_and(mint_t *mint, const mint_t *other)
{
    return mint_and_inplace(mint, other);
}

int mi_set_bit(mint_t *mint, size_t bit_index)
{
    if (!mint || mint_is_immortal(mint) || mint->sign < 0)
        return -1;
    return mint_set_bit_internal(mint, bit_index);
}

int mi_clear_bit(mint_t *mint, size_t bit_index)
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

int mi_mul(mint_t *mint, const mint_t *other)
{
    return mint_mul_inplace(mint, other);
}

int mi_neg(mint_t *mint)
{
    return mint_neg_inplace(mint);
}

int mi_not(mint_t *mint)
{
    return mint_not_inplace(mint);
}

int mi_or(mint_t *mint, const mint_t *other)
{
    return mint_or_inplace(mint, other);
}

int mi_pow(mint_t *mint, unsigned long exponent)
{
    return mint_pow_inplace(mint, exponent);
}

int mi_powmod(mint_t *mint, const mint_t *exponent, const mint_t *modulus)
{
    return mint_powmod_inplace(mint, exponent, modulus);
}

int mi_pow10(mint_t *mint, long exponent)
{
    return mint_pow10_inplace(mint, exponent);
}

int mi_shl(mint_t *mint, long bits)
{
    return mint_shl_inplace(mint, bits);
}

int mi_shr(mint_t *mint, long bits)
{
    return mint_shr_inplace(mint, bits);
}

int mi_sqrt(mint_t *mint)
{
    return mint_sqrt_inplace(mint);
}

int mi_xor(mint_t *mint, const mint_t *other)
{
    return mint_xor_inplace(mint, other);
}
