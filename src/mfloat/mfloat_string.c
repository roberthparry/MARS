#include "mfloat_internal.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static char *mfloat_format_signed_digits(short sign, const char *digits, size_t frac_digits)
{
    size_t digits_len;
    size_t sign_len;
    size_t out_len;
    char *out;
    char *p;

    if (!digits)
        return NULL;

    digits_len = strlen(digits);
    sign_len = sign < 0 ? 1u : 0u;

    if (frac_digits == 0) {
        out_len = sign_len + digits_len + 1u;
        out = malloc(out_len);
        if (!out)
            return NULL;
        p = out;
        if (sign < 0)
            *p++ = '-';
        memcpy(p, digits, digits_len + 1u);
        return out;
    }

    if (digits_len > frac_digits) {
        size_t int_len = digits_len - frac_digits;

        out_len = sign_len + int_len + 1u + frac_digits + 1u;
        out = malloc(out_len);
        if (!out)
            return NULL;
        p = out;
        if (sign < 0)
            *p++ = '-';
        memcpy(p, digits, int_len);
        p += int_len;
        *p++ = '.';
        memcpy(p, digits + int_len, frac_digits);
        p += frac_digits;
        *p = '\0';
        return out;
    }

    out_len = sign_len + 2u + (frac_digits - digits_len) + digits_len + 1u;
    out = malloc(out_len);
    if (!out)
        return NULL;
    p = out;
    if (sign < 0)
        *p++ = '-';
    *p++ = '0';
    *p++ = '.';
    memset(p, '0', frac_digits - digits_len);
    p += frac_digits - digits_len;
    memcpy(p, digits, digits_len);
    p += digits_len;
    *p = '\0';
    return out;
}

mfloat_t *mf_create_string(const char *text)
{
    mfloat_t *mfloat = mf_new();

    if (!mfloat)
        return NULL;
    if (mf_set_string(mfloat, text) != 0) {
        mf_free(mfloat);
        return NULL;
    }
    return mfloat;
}

char *mf_to_string(const mfloat_t *mfloat)
{
    mint_t *work = NULL;
    mint_t *factor = NULL;
    char *digits = NULL;
    char *out = NULL;
    long exp2;

    if (!mfloat || !mfloat->mantissa)
        return NULL;

    if (mfloat->kind == MFLOAT_KIND_NAN) {
        out = malloc(4u);
        if (!out)
            return NULL;
        memcpy(out, "NAN", 4u);
        return out;
    }
    if (mfloat->kind == MFLOAT_KIND_POSINF) {
        out = malloc(4u);
        if (!out)
            return NULL;
        memcpy(out, "INF", 4u);
        return out;
    }
    if (mfloat->kind == MFLOAT_KIND_NEGINF) {
        out = malloc(5u);
        if (!out)
            return NULL;
        memcpy(out, "-INF", 5u);
        return out;
    }

    if (mf_is_zero(mfloat)) {
        out = malloc(2u);
        if (!out)
            return NULL;
        out[0] = '0';
        out[1] = '\0';
        return out;
    }

    exp2 = mfloat->exponent2;
    work = mi_clone(mfloat->mantissa);
    if (!work)
        goto cleanup;

    if (exp2 >= 0) {
        if (mi_shl(work, exp2) != 0)
            goto cleanup;
        digits = mi_to_string(work);
        if (!digits)
            goto cleanup;
        out = mfloat_format_signed_digits(mfloat->sign, digits, 0u);
        goto cleanup;
    }

    factor = mi_create_long(5);
    if (!factor || mi_pow(factor, (unsigned long)(-exp2)) != 0)
        goto cleanup;
    if (mi_mul(work, factor) != 0)
        goto cleanup;

    digits = mi_to_string(work);
    if (!digits)
        goto cleanup;

    out = mfloat_format_signed_digits(mfloat->sign, digits, (size_t)(-exp2));

cleanup:
    mi_free(work);
    mi_free(factor);
    free(digits);
    return out;
}

static uint64_t mfloat_extract_bits_u64(const mint_t *mantissa,
                                        size_t start_bit,
                                        size_t count)
{
    uint64_t value = 0u;

    for (size_t i = 0; i < count; i++) {
        value <<= 1;
        if (mi_test_bit(mantissa, start_bit + (count - 1u - i)))
            value |= 1u;
    }

    return value;
}

static int mfloat_any_low_bits(const mint_t *mantissa, size_t bit_count)
{
    size_t bitlen = mi_bit_length(mantissa);
    size_t limit = bit_count < bitlen ? bit_count : bitlen;

    for (size_t i = 0; i < limit; i++) {
        if (mi_test_bit(mantissa, i))
            return 1;
    }
    return 0;
}

static uint64_t mfloat_shift_left_to_u64(const mint_t *mantissa, size_t left_shift)
{
    size_t bitlen = mi_bit_length(mantissa);
    uint64_t value = 0u;

    for (size_t i = 0; i < bitlen; i++) {
        value <<= 1;
        if (mi_test_bit(mantissa, bitlen - 1u - i))
            value |= 1u;
    }

    return value << left_shift;
}

static uint64_t mfloat_round_shifted_to_u64(const mint_t *mantissa, long shift)
{
    size_t bitlen = mi_bit_length(mantissa);
    uint64_t value;

    if (bitlen == 0u)
        return 0u;

    if (shift < 0)
        return mfloat_shift_left_to_u64(mantissa, (size_t)(-shift));

    if ((size_t)shift < bitlen) {
        size_t kept = bitlen - (size_t)shift;
        value = mfloat_extract_bits_u64(mantissa, (size_t)shift, kept);
    } else {
        value = 0u;
    }

    if (shift > 0) {
        size_t round_bit_index = (size_t)shift - 1u;
        int round_bit = round_bit_index < bitlen ? mi_test_bit(mantissa, round_bit_index) : 0;
        int sticky = mfloat_any_low_bits(mantissa, round_bit_index);

        if (round_bit && (sticky || (value & 1u)))
            value++;
    }

    return value;
}

static double mfloat_to_double_direct(const mfloat_t *mfloat)
{
    size_t bitlen;
    long exponent;
    double value;

    if (!mfloat)
        return NAN;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return NAN;
    if (mfloat->kind == MFLOAT_KIND_POSINF)
        return INFINITY;
    if (mfloat->kind == MFLOAT_KIND_NEGINF)
        return -INFINITY;
    if (!mfloat->mantissa || mi_is_zero(mfloat->mantissa))
        return 0.0;

    bitlen = mi_bit_length(mfloat->mantissa);
    exponent = mfloat->exponent2 + (long)bitlen - 1l;

    if (exponent > DBL_MAX_EXP - 1)
        return mfloat->sign < 0 ? -INFINITY : INFINITY;

    if (exponent >= DBL_MIN_EXP) {
        long shift = (long)bitlen - 53l;
        uint64_t sig;

        if (shift >= 0) {
            sig = mfloat_extract_bits_u64(mfloat->mantissa, (size_t)shift, 53u);
            if (shift > 0) {
                size_t round_bit_index = (size_t)shift - 1u;
                int round_bit = mi_test_bit(mfloat->mantissa, round_bit_index);
                int sticky = mfloat_any_low_bits(mfloat->mantissa, round_bit_index);

                if (round_bit && (sticky || (sig & 1u)))
                    sig++;
            }
        } else {
            sig = mfloat_shift_left_to_u64(mfloat->mantissa, (size_t)(-shift));
        }

        if (sig == (UINT64_C(1) << 53)) {
            sig >>= 1;
            exponent++;
            if (exponent > DBL_MAX_EXP - 1)
                return mfloat->sign < 0 ? -INFINITY : INFINITY;
        }

        value = ldexp((double)sig, (int)exponent - 52);
    } else {
        long shift = -(mfloat->exponent2 + 1074l);
        uint64_t frac = mfloat_round_shifted_to_u64(mfloat->mantissa, shift);

        if (frac == 0u) {
            value = 0.0;
        } else {
            value = ldexp((double)frac, -1074);
        }
    }

    return mfloat->sign < 0 ? -value : value;
}

double mf_to_double(const mfloat_t *mfloat)
{
    return mfloat_to_double_direct(mfloat);
}

qfloat_t mf_to_qfloat(const mfloat_t *mfloat)
{
    double hi, lo;
    mfloat_t *residual = NULL;
    mfloat_t *head = NULL;
    qfloat_t value;

    if (!mfloat)
        return QF_NAN;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return QF_NAN;
    if (mfloat->kind == MFLOAT_KIND_POSINF)
        return QF_INF;
    if (mfloat->kind == MFLOAT_KIND_NEGINF)
        return QF_NINF;
    if (!mfloat->mantissa || mi_is_zero(mfloat->mantissa))
        return QF_ZERO;

    hi = mfloat_to_double_direct(mfloat);
    if (!isfinite(hi))
        return qf_from_double(hi);

    residual = mf_clone(mfloat);
    head = mf_create_double(hi);
    if (!residual || !head)
        goto fail;
    if (mf_set_precision(head, residual->precision) != 0)
        goto fail;
    if (mf_sub(residual, head) != 0)
        goto fail;

    lo = mfloat_to_double_direct(residual);
    value = qf_add(qf_from_double(hi), qf_from_double(lo));

    mf_free(residual);
    mf_free(head);
    return value;

fail:
    mf_free(residual);
    mf_free(head);
    return QF_NAN;
}
