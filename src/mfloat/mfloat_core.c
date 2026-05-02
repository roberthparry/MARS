#include "mfloat_internal.h"

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>

static struct _mfloat_t mfloat_zero_static = {
    .kind = MFLOAT_KIND_FINITE,
    .sign = 0,
    .exponent2 = 0,
    .precision = MFLOAT_DEFAULT_PRECISION_BITS,
    .mantissa = NULL
};

static struct _mfloat_t mfloat_one_static = {
    .kind = MFLOAT_KIND_FINITE,
    .sign = 1,
    .exponent2 = 0,
    .precision = MFLOAT_DEFAULT_PRECISION_BITS,
    .mantissa = NULL
};

static struct _mfloat_t mfloat_half_static = {
    .kind = MFLOAT_KIND_FINITE,
    .sign = 1,
    .exponent2 = -1,
    .precision = MFLOAT_DEFAULT_PRECISION_BITS,
    .mantissa = NULL
};

static struct _mfloat_t mfloat_tenth_static = {
    .kind = MFLOAT_KIND_FINITE,
    .sign = 1,
    .exponent2 = -1,
    .precision = MFLOAT_DEFAULT_PRECISION_BITS,
    .mantissa = NULL
};

static struct _mfloat_t mfloat_ten_static = {
    .kind = MFLOAT_KIND_FINITE,
    .sign = 1,
    .exponent2 = 0,
    .precision = MFLOAT_DEFAULT_PRECISION_BITS,
    .mantissa = NULL
};

static struct _mfloat_t mfloat_nan_static = {
    .kind = MFLOAT_KIND_NAN,
    .sign = 0,
    .exponent2 = 0,
    .precision = MFLOAT_DEFAULT_PRECISION_BITS,
    .mantissa = NULL
};

static struct _mfloat_t mfloat_inf_static = {
    .kind = MFLOAT_KIND_POSINF,
    .sign = 1,
    .exponent2 = 0,
    .precision = MFLOAT_DEFAULT_PRECISION_BITS,
    .mantissa = NULL
};

static struct _mfloat_t mfloat_ninf_static = {
    .kind = MFLOAT_KIND_NEGINF,
    .sign = -1,
    .exponent2 = 0,
    .precision = MFLOAT_DEFAULT_PRECISION_BITS,
    .mantissa = NULL
};

static size_t mfloat_default_precision_bits = MFLOAT_DEFAULT_PRECISION_BITS;

const mfloat_t * const MF_ZERO = &mfloat_zero_static;
const mfloat_t * const MF_ONE = &mfloat_one_static;
const mfloat_t * const MF_HALF = &mfloat_half_static;
const mfloat_t * const MF_TENTH = &mfloat_tenth_static;
const mfloat_t * const MF_TEN = &mfloat_ten_static;
const mfloat_t * const MF_NAN = &mfloat_nan_static;
const mfloat_t * const MF_INF = &mfloat_inf_static;
const mfloat_t * const MF_NINF = &mfloat_ninf_static;

static void mfloat_init_constants(void)
{
    static int initialised = 0;
    mfloat_t *tmp_tenth = NULL;

    if (initialised)
        return;
    initialised = 1;

    mfloat_zero_static.mantissa = (mint_t *)MI_ZERO;
    mfloat_one_static.mantissa = (mint_t *)MI_ONE;
    mfloat_half_static.mantissa = (mint_t *)MI_ONE;
    mfloat_ten_static.mantissa = (mint_t *)MI_TEN;
    mfloat_nan_static.mantissa = (mint_t *)MI_ZERO;
    mfloat_inf_static.mantissa = (mint_t *)MI_ZERO;
    mfloat_ninf_static.mantissa = (mint_t *)MI_ZERO;

    tmp_tenth = mf_create_string("0.1");
    if (tmp_tenth) {
        mfloat_tenth_static.sign = tmp_tenth->sign;
        mfloat_tenth_static.exponent2 = tmp_tenth->exponent2;
        mfloat_tenth_static.precision = tmp_tenth->precision;
        mfloat_tenth_static.mantissa = tmp_tenth->mantissa;
        tmp_tenth->mantissa = NULL;
        free(tmp_tenth);
    } else {
        mfloat_tenth_static.mantissa = (mint_t *)MI_ZERO;
        mfloat_tenth_static.sign = 0;
        mfloat_tenth_static.exponent2 = 0;
    }
}

__attribute__((constructor))
static void mfloat_init_constants_ctor(void)
{
    mfloat_init_constants();
}

int mfloat_is_immortal(const mfloat_t *mfloat)
{
    return mfloat == MF_ZERO || mfloat == MF_ONE || mfloat == MF_HALF ||
           mfloat == MF_TENTH || mfloat == MF_TEN || mfloat == MF_NAN ||
           mfloat == MF_INF || mfloat == MF_NINF;
}

int mfloat_is_finite(const mfloat_t *mfloat)
{
    return mfloat && mfloat->kind == MFLOAT_KIND_FINITE;
}

int mfloat_normalise(mfloat_t *mfloat)
{
    if (!mfloat || !mfloat->mantissa)
        return -1;
    if (!mfloat_is_finite(mfloat))
        return -1;

    if (mi_is_zero(mfloat->mantissa)) {
        mfloat->sign = 0;
        mfloat->exponent2 = 0;
        mfloat->kind = MFLOAT_KIND_FINITE;
        return 0;
    }

    while (mi_is_even(mfloat->mantissa)) {
        if (mi_shr(mfloat->mantissa, 1) != 0)
            return -1;
        mfloat->exponent2++;
    }

    if (mfloat->sign == 0)
        mfloat->sign = 1;
    return 0;
}

static mfloat_t *mfloat_alloc(size_t precision_bits)
{
    mfloat_t *mfloat = calloc(1, sizeof(*mfloat));

    mfloat_init_constants();

    if (!mfloat)
        return NULL;

    mfloat->mantissa = mi_new();
    if (!mfloat->mantissa) {
        free(mfloat);
        return NULL;
    }

    mfloat->precision = precision_bits > 0 ? precision_bits
                                           : mfloat_default_precision_bits;
    mfloat->kind = MFLOAT_KIND_FINITE;
    return mfloat;
}

static int mfloat_set_double_exact(mfloat_t *mfloat, double value)
{
    union {
        double d;
        uint64_t u;
    } bits;
    uint64_t frac;
    uint64_t mantissa_u64;
    int exp_bits;
    long exponent2;

    if (!mfloat || !mfloat->mantissa)
        return -1;
    if (mfloat_is_immortal(mfloat))
        return -1;

    if (isnan(value)) {
        mi_clear(mfloat->mantissa);
        mfloat->kind = MFLOAT_KIND_NAN;
        mfloat->sign = 0;
        mfloat->exponent2 = 0;
        return 0;
    }
    if (isinf(value)) {
        mi_clear(mfloat->mantissa);
        mfloat->kind = value < 0.0 ? MFLOAT_KIND_NEGINF : MFLOAT_KIND_POSINF;
        mfloat->sign = value < 0.0 ? (short)-1 : (short)1;
        mfloat->exponent2 = 0;
        return 0;
    }
    if (value == 0.0) {
        mf_clear(mfloat);
        if (signbit(value))
            mfloat->sign = -0;
        return 0;
    }

    bits.d = value;
    frac = bits.u & ((UINT64_C(1) << 52) - 1u);
    exp_bits = (int)((bits.u >> 52) & 0x7ffu);

    if (exp_bits == 0) {
        mantissa_u64 = frac;
        exponent2 = -1074l;
    } else {
        mantissa_u64 = (UINT64_C(1) << 52) | frac;
        exponent2 = (long)exp_bits - 1023l - 52l;
    }

    if (mi_set_ulong(mfloat->mantissa, (unsigned long)mantissa_u64) != 0)
        return -1;
    mfloat->kind = MFLOAT_KIND_FINITE;
    mfloat->sign = (bits.u >> 63) ? (short)-1 : (short)1;
    mfloat->exponent2 = exponent2;
    return mfloat_normalise(mfloat);
}

int mfloat_copy_value(mfloat_t *dst, const mfloat_t *src)
{
    if (!dst || !src || !dst->mantissa || !src->mantissa)
        return -1;
    if (mi_clear(dst->mantissa), mi_add(dst->mantissa, src->mantissa) != 0)
        return -1;
    dst->kind = src->kind;
    dst->sign = src->sign;
    dst->exponent2 = src->exponent2;
    dst->precision = src->precision;
    return 0;
}

int mfloat_set_from_signed_mint(mfloat_t *dst, mint_t *value, long exponent2)
{
    if (!dst || !value || !dst->mantissa)
        return -1;
    if (mfloat_is_immortal(dst))
        return -1;

    if (mi_is_zero(value)) {
        mf_clear(dst);
        return 0;
    }

    dst->sign = mi_is_negative(value) ? (short)-1 : (short)1;
    if (dst->sign < 0 && mi_abs(value) != 0)
        return -1;

    mi_clear(dst->mantissa);
    if (mi_add(dst->mantissa, value) != 0)
        return -1;
    dst->kind = MFLOAT_KIND_FINITE;
    dst->exponent2 = exponent2;
    return mfloat_normalise(dst);
}

mint_t *mfloat_to_scaled_mint(const mfloat_t *mfloat, long target_exp)
{
    mint_t *value;
    long shift;

    if (!mfloat || !mfloat->mantissa)
        return NULL;
    if (!mfloat_is_finite(mfloat))
        return NULL;

    value = mi_clone(mfloat->mantissa);
    if (!value)
        return NULL;

    shift = mfloat->exponent2 - target_exp;
    if (shift > 0) {
        if (mi_shl(value, shift) != 0) {
            mi_free(value);
            return NULL;
        }
    } else if (shift < 0) {
        if (mi_shr(value, -shift) != 0) {
            mi_free(value);
            return NULL;
        }
    }

    if (mfloat->sign < 0 && mi_neg(value) != 0) {
        mi_free(value);
        return NULL;
    }

    return value;
}

static int mfloat_parse_decimal_components(const char *text,
                                           short *out_sign,
                                           mint_t *digits,
                                           long *out_exp10)
{
    const unsigned char *p = (const unsigned char *)text;
    short sign = 1;
    long frac_digits = 0;
    long exp10 = 0;
    bool seen_digit = false;
    bool seen_dot = false;

    if (!text || !out_sign || !digits || !out_exp10)
        return -1;

    while (isspace(*p))
        ++p;

    if (*p == '+' || *p == '-') {
        if (*p == '-')
            sign = -1;
        ++p;
    }

    if (mi_set_long(digits, 0) != 0)
        return -1;

    while (*p) {
        if (isdigit(*p)) {
            seen_digit = true;
            if (mi_mul_long(digits, 10) != 0 ||
                mi_add_long(digits, (long)(*p - '0')) != 0)
                return -1;
            if (seen_dot)
                frac_digits++;
            ++p;
            continue;
        }
        if (*p == '.' && !seen_dot) {
            seen_dot = true;
            ++p;
            continue;
        }
        break;
    }

    if (!seen_digit)
        return -1;

    if (*p == 'e' || *p == 'E') {
        bool neg_exp = false;
        long parsed = 0;

        ++p;
        if (*p == '+' || *p == '-') {
            neg_exp = (*p == '-');
            ++p;
        }
        if (!isdigit(*p))
            return -1;
        while (isdigit(*p)) {
            if (parsed > (LONG_MAX - 9) / 10)
                return -1;
            parsed = parsed * 10 + (long)(*p - '0');
            ++p;
        }
        exp10 = neg_exp ? -parsed : parsed;
    }

    while (isspace(*p))
        ++p;
    if (*p != '\0')
        return -1;

    *out_sign = sign;
    *out_exp10 = exp10 - frac_digits;
    return 0;
}

static int mfloat_set_from_decimal_parts(mfloat_t *mfloat,
                                         short sign,
                                         mint_t *digits,
                                         long exp10)
{
    mint_t *work = NULL, *factor = NULL, *q = NULL, *r = NULL, *twor = NULL;
    size_t shift_bits;
    int rc = -1;

    if (!mfloat || !digits || !mfloat->mantissa)
        return -1;

    if (mi_is_zero(digits)) {
        mf_clear(mfloat);
        return 0;
    }

    work = mi_clone(digits);
    if (!work)
        goto cleanup;

    if (exp10 >= 0) {
        factor = mi_create_long(5);
        if (!factor || mi_pow(factor, (unsigned long)exp10) != 0)
            goto cleanup;
        if (mi_mul(work, factor) != 0)
            goto cleanup;

        mi_clear(mfloat->mantissa);
        if (mi_add(mfloat->mantissa, work) != 0)
            goto cleanup;
        mfloat->kind = MFLOAT_KIND_FINITE;
        mfloat->sign = sign;
        mfloat->exponent2 = exp10;
        rc = mfloat_normalise(mfloat);
        goto cleanup;
    }

    factor = mi_create_long(5);
    if (!factor || mi_pow(factor, (unsigned long)(-exp10)) != 0)
        goto cleanup;

    shift_bits = mfloat->precision + mi_bit_length(factor) + MFLOAT_PARSE_GUARD_BITS;
    if (mi_shl(work, (long)shift_bits) != 0)
        goto cleanup;

    q = mi_new();
    r = mi_new();
    if (!q || !r)
        goto cleanup;
    if (mi_divmod(work, factor, q, r) != 0)
        goto cleanup;

    twor = mi_clone(r);
    if (!twor || mi_mul_long(twor, 2) != 0)
        goto cleanup;
    if (mi_cmp(twor, factor) >= 0) {
        if (mi_inc(q) != 0)
            goto cleanup;
    }

    mi_clear(mfloat->mantissa);
    if (mi_add(mfloat->mantissa, q) != 0)
        goto cleanup;
    mfloat->kind = MFLOAT_KIND_FINITE;
    mfloat->sign = sign;
    mfloat->exponent2 = exp10 - (long)shift_bits;
    rc = mfloat_normalise(mfloat);

cleanup:
    mi_free(work);
    mi_free(factor);
    mi_free(q);
    mi_free(r);
    mi_free(twor);
    return rc;
}

mfloat_t *mf_new(void)
{
    return mf_new_prec(mfloat_default_precision_bits);
}

mfloat_t *mf_new_prec(size_t precision_bits)
{
    mfloat_init_constants();
    return mfloat_alloc(precision_bits);
}

size_t mfloat_get_default_precision_internal(void)
{
    return mfloat_default_precision_bits;
}

mfloat_t *mf_create_long(long value)
{
    mfloat_t *mfloat = mf_new();

    if (!mfloat)
        return NULL;
    if (mf_set_long(mfloat, value) != 0) {
        mf_free(mfloat);
        return NULL;
    }
    return mfloat;
}

mfloat_t *mf_create_double(double value)
{
    mfloat_t *mfloat = mf_new();

    if (!mfloat)
        return NULL;
    if (mf_set_double(mfloat, value) != 0) {
        mf_free(mfloat);
        return NULL;
    }
    return mfloat;
}

mfloat_t *mf_create_qfloat(qfloat_t value)
{
    mfloat_t *mfloat = mf_new();

    if (!mfloat)
        return NULL;
    if (mf_set_qfloat(mfloat, value) != 0) {
        mf_free(mfloat);
        return NULL;
    }
    return mfloat;
}

mfloat_t *mf_clone(const mfloat_t *mfloat)
{
    mfloat_t *copy;

    if (!mfloat)
        return NULL;
    copy = mf_new_prec(mfloat->precision);
    if (!copy)
        return NULL;
    if (mfloat_copy_value(copy, mfloat) != 0) {
        mf_free(copy);
        return NULL;
    }
    return copy;
}

void mf_free(mfloat_t *mfloat)
{
    if (!mfloat)
        return;
    if (mfloat_is_immortal(mfloat))
        return;
    mi_free(mfloat->mantissa);
    free(mfloat);
}

void mf_clear(mfloat_t *mfloat)
{
    if (!mfloat || !mfloat->mantissa)
        return;
    if (mfloat_is_immortal(mfloat))
        return;
    mi_clear(mfloat->mantissa);
    mfloat->kind = MFLOAT_KIND_FINITE;
    mfloat->sign = 0;
    mfloat->exponent2 = 0;
}

int mf_set_precision(mfloat_t *mfloat, size_t precision_bits)
{
    if (!mfloat || precision_bits == 0)
        return -1;
    if (mfloat_is_immortal(mfloat))
        return -1;
    mfloat->precision = precision_bits;
    return 0;
}

int mf_set_default_precision(size_t precision_bits)
{
    if (precision_bits == 0)
        return -1;
    mfloat_default_precision_bits = precision_bits;
    return 0;
}

size_t mf_get_default_precision(void)
{
    return mfloat_default_precision_bits;
}

size_t mf_get_precision(const mfloat_t *mfloat)
{
    return mfloat ? mfloat->precision : 0;
}

int mf_set_long(mfloat_t *mfloat, long value)
{
    if (!mfloat || !mfloat->mantissa)
        return -1;
    if (mfloat_is_immortal(mfloat))
        return -1;

    if (value == 0) {
        mf_clear(mfloat);
        return 0;
    }

    if (mi_set_long(mfloat->mantissa, value < 0 ? -value : value) != 0)
        return -1;
    mfloat->kind = MFLOAT_KIND_FINITE;
    mfloat->sign = value < 0 ? (short)-1 : (short)1;
    mfloat->exponent2 = 0;
    return mfloat_normalise(mfloat);
}

int mf_set_double(mfloat_t *mfloat, double value)
{
    return mfloat_set_double_exact(mfloat, value);
}

int mf_set_qfloat(mfloat_t *mfloat, qfloat_t value)
{
    mfloat_t *tmp = NULL;
    int rc;

    if (!mfloat)
        return -1;
    if (mfloat_is_immortal(mfloat))
        return -1;
    if (qf_isnan(value)) {
        mi_clear(mfloat->mantissa);
        mfloat->kind = MFLOAT_KIND_NAN;
        mfloat->sign = 0;
        mfloat->exponent2 = 0;
        return 0;
    }
    if (qf_isposinf(value)) {
        mi_clear(mfloat->mantissa);
        mfloat->kind = MFLOAT_KIND_POSINF;
        mfloat->sign = 1;
        mfloat->exponent2 = 0;
        return 0;
    }
    if (qf_isneginf(value)) {
        mi_clear(mfloat->mantissa);
        mfloat->kind = MFLOAT_KIND_NEGINF;
        mfloat->sign = -1;
        mfloat->exponent2 = 0;
        return 0;
    }

    rc = mfloat_set_double_exact(mfloat, value.hi);
    if (rc != 0)
        return rc;

    if (value.lo == 0.0)
        return 0;

    tmp = mf_create_double(value.lo);
    if (!tmp)
        return -1;
    if (mf_set_precision(tmp, mfloat->precision) != 0) {
        mf_free(tmp);
        return -1;
    }

    rc = mf_add(mfloat, tmp);
    mf_free(tmp);
    return rc;
}

int mf_set_string(mfloat_t *mfloat, const char *text)
{
    mint_t *digits = NULL;
    short sign = 1;
    long exp10 = 0;
    int rc;

    if (!mfloat || !text)
        return -1;
    if (mfloat_is_immortal(mfloat))
        return -1;

    if (text[0] == 'N' && text[1] == 'A' && text[2] == 'N' && text[3] == '\0') {
        mfloat->kind = MFLOAT_KIND_NAN;
        mfloat->sign = 0;
        mfloat->exponent2 = 0;
        mi_clear(mfloat->mantissa);
        return 0;
    }
    if (text[0] == 'I' && text[1] == 'N' && text[2] == 'F' && text[3] == '\0') {
        mfloat->kind = MFLOAT_KIND_POSINF;
        mfloat->sign = 1;
        mfloat->exponent2 = 0;
        mi_clear(mfloat->mantissa);
        return 0;
    }
    if (text[0] == '-' && text[1] == 'I' && text[2] == 'N' && text[3] == 'F' && text[4] == '\0') {
        mfloat->kind = MFLOAT_KIND_NEGINF;
        mfloat->sign = -1;
        mfloat->exponent2 = 0;
        mi_clear(mfloat->mantissa);
        return 0;
    }

    digits = mi_new();
    if (!digits)
        return -1;

    rc = mfloat_parse_decimal_components(text, &sign, digits, &exp10);
    if (rc == 0)
        rc = mfloat_set_from_decimal_parts(mfloat, sign, digits, exp10);

    mi_free(digits);
    return rc;
}

bool mf_is_zero(const mfloat_t *mfloat)
{
    if (!mfloat_is_finite(mfloat))
        return false;
    return !mfloat || mfloat->sign == 0 || !mfloat->mantissa ||
           mi_is_zero(mfloat->mantissa);
}

short mf_get_sign(const mfloat_t *mfloat)
{
    return mfloat ? mfloat->sign : 0;
}

long mf_get_exponent2(const mfloat_t *mfloat)
{
    return mfloat ? mfloat->exponent2 : 0;
}

size_t mf_get_mantissa_bits(const mfloat_t *mfloat)
{
    if (!mfloat || !mfloat->mantissa)
        return 0;
    if (!mfloat_is_finite(mfloat))
        return 0;
    return mi_bit_length(mfloat->mantissa);
}

bool mf_get_mantissa_u64(const mfloat_t *mfloat, uint64_t *out)
{
    long value;

    if (!mfloat || !out || !mfloat->mantissa || mi_is_negative(mfloat->mantissa))
        return false;
    if (!mfloat_is_finite(mfloat))
        return false;
    if (mi_bit_length(mfloat->mantissa) > (sizeof(long) * 8u - 1u))
        return false;
    if (!mi_get_long(mfloat->mantissa, &value) || value < 0)
        return false;
    *out = (uint64_t)value;
    return true;
}
