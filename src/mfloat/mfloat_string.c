#include "mfloat_internal.h"

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

double mf_to_double(const mfloat_t *mfloat)
{
    char *text;
    double value;

    if (!mfloat)
        return NAN;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return NAN;
    if (mfloat->kind == MFLOAT_KIND_POSINF)
        return INFINITY;
    if (mfloat->kind == MFLOAT_KIND_NEGINF)
        return -INFINITY;

    text = mf_to_string(mfloat);
    if (!text)
        return NAN;
    value = strtod(text, NULL);
    free(text);
    return value;
}

qfloat_t mf_to_qfloat(const mfloat_t *mfloat)
{
    char *text;
    qfloat_t value;

    if (!mfloat)
        return QF_NAN;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return QF_NAN;
    if (mfloat->kind == MFLOAT_KIND_POSINF)
        return QF_INF;
    if (mfloat->kind == MFLOAT_KIND_NEGINF)
        return QF_NINF;

    text = mf_to_string(mfloat);
    if (!text)
        return QF_NAN;
    value = qf_from_string(text);
    free(text);
    return value;
}
