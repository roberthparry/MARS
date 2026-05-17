#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "mfloat_internal.h"

static void mfloat_prepare_constant(const mfloat_t *mfloat, mpfr_prec_t precision)
{
    if (mfloat && mfloat->constant_id != MFCONST_NONE)
        mfloat_constant_ensure(mfloat, precision);
}

static char *mfloat_dup_cstr(const char *text)
{
    size_t len;
    char *out;

    if (!text)
        return NULL;
    len = strlen(text) + 1u;
    out = malloc(len);
    if (!out)
        return NULL;
    memcpy(out, text, len);
    return out;
}

static char *mfloat_special_string(const mfloat_t *mfloat)
{
    if (mpfr_nan_p(mfloat->value))
        return mfloat_dup_cstr("NAN");
    if (mpfr_inf_p(mfloat->value))
        return mfloat_dup_cstr(mpfr_sgn(mfloat->value) < 0 ? "-INF" : "INF");
    if (mpfr_zero_p(mfloat->value))
        return mfloat_dup_cstr("0");
    return NULL;
}

static char *mfloat_format_mpfr_digits(const char *digits, mpfr_exp_t exponent10)
{
    size_t digits_len;
    size_t first_digit;
    size_t last_digit;
    size_t int_digits;
    size_t frac_digits;
    size_t out_len;
    char *out;
    char *p;

    if (!digits)
        return NULL;

    first_digit = (digits[0] == '-') ? 1u : 0u;
    digits_len = strlen(digits);
    last_digit = digits_len;
    while (last_digit > first_digit + 1u && digits[last_digit - 1u] == '0')
        last_digit--;

    if (last_digit == first_digit + 1u && digits[first_digit] == '0')
        return mfloat_dup_cstr("0");

    int_digits = exponent10 > 0 ? (size_t)exponent10 : 0u;
    if (int_digits >= last_digit - first_digit) {
        out_len = first_digit + int_digits + 1u;
        out = malloc(out_len);
        if (!out)
            return NULL;

        p = out;
        if (first_digit)
            *p++ = '-';
        memcpy(p, digits + first_digit, last_digit - first_digit);
        p += last_digit - first_digit;
        memset(p, '0', int_digits - (last_digit - first_digit));
        p += int_digits - (last_digit - first_digit);
        *p = '\0';
        return out;
    }

    if (exponent10 > 0) {
        frac_digits = (last_digit - first_digit) - (size_t)exponent10;
        out_len = first_digit + (size_t)exponent10 + 1u + frac_digits + 1u;
        out = malloc(out_len);
        if (!out)
            return NULL;

        p = out;
        if (first_digit)
            *p++ = '-';
        memcpy(p, digits + first_digit, (size_t)exponent10);
        p += (size_t)exponent10;
        *p++ = '.';
        memcpy(p, digits + first_digit + (size_t)exponent10, frac_digits);
        p += frac_digits;
        *p = '\0';
        return out;
    }

    frac_digits = (size_t)(-exponent10);
    out_len = first_digit + 2u + frac_digits + (last_digit - first_digit) + 1u;
    out = malloc(out_len);
    if (!out)
        return NULL;

    p = out;
    if (first_digit)
        *p++ = '-';
    *p++ = '0';
    *p++ = '.';
    memset(p, '0', frac_digits);
    p += frac_digits;
    memcpy(p, digits + first_digit, last_digit - first_digit);
    p += last_digit - first_digit;
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
    mpfr_exp_t exponent10;
    char *digits = NULL;
    char *out = NULL;
    size_t digit_count;

    if (!mfloat)
        return NULL;

    mfloat_prepare_constant(mfloat, (mpfr_prec_t)mf_get_default_precision());

    out = mfloat_special_string(mfloat);
    if (out)
        return out;

    digit_count = mpfr_get_str_ndigits(10, mpfr_get_prec(mfloat->value)) + 2u;
    digits = mpfr_get_str(NULL, &exponent10, 10, digit_count, mfloat->value, MPFR_RNDN);
    if (!digits)
        return NULL;

    out = mfloat_format_mpfr_digits(digits, exponent10);
    mpfr_free_str(digits);
    return out;
}

double mf_to_double(const mfloat_t *mfloat)
{
    if (!mfloat)
        return NAN;
    mfloat_prepare_constant(mfloat, (mpfr_prec_t)mf_get_default_precision());
    return mpfr_get_d(mfloat->value, MPFR_RNDN);
}

qfloat_t mf_to_qfloat(const mfloat_t *mfloat)
{
    mpfr_t residual;
    double hi;
    double lo;

    if (!mfloat)
        return QF_NAN;

    mfloat_prepare_constant(mfloat, (mpfr_prec_t)mf_get_default_precision());

    if (mpfr_nan_p(mfloat->value))
        return QF_NAN;
    if (mpfr_inf_p(mfloat->value))
        return mpfr_sgn(mfloat->value) < 0 ? QF_NINF : QF_INF;
    if (mpfr_zero_p(mfloat->value))
        return QF_ZERO;

    hi = mpfr_get_d(mfloat->value, MPFR_RNDN);
    if (!isfinite(hi))
        return qf_from_double(hi);

    mpfr_init2(residual, mpfr_get_prec(mfloat->value));
    mpfr_set(residual, mfloat->value, MPFR_RNDN);
    mpfr_sub_d(residual, residual, hi, MPFR_RNDN);
    lo = mpfr_get_d(residual, MPFR_RNDN);
    mpfr_clear(residual);

    return qf_add(qf_from_double(hi), qf_from_double(lo));
}
