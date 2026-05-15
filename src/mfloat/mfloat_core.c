#include "mfloat_internal.h"
#include "internal/number_scope_alloc.h"
#include "internal/qfloat_internal.h"
#include "internal/mint_internal.h"
#include "mrational.h"

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>

static const double MFLOAT_LOG10_2 = 0.3010299956639812;
static const double MFLOAT_LOG2_10 = 3.3219280948873626;

static size_t mfloat_default_precision_bits = MFLOAT_DEFAULT_PRECISION_BITS;

bool mfloat_is_immortal(const mfloat_t *mfloat)
{
    return mfloat && mfloat->immortal;
}

bool mfloat_is_finite(const mfloat_t *mfloat)
{
    return mfloat && mfloat->kind == MFLOAT_KIND_FINITE;
}

bool mfloat_is_nan(const mfloat_t *mfloat)
{
    return mfloat && mfloat->kind == MFLOAT_KIND_NAN;
}

bool mfloat_is_inf(const mfloat_t *mfloat)
{
    return mfloat &&
        (mfloat->kind == MFLOAT_KIND_POSINF || mfloat->kind == MFLOAT_KIND_NEGINF);
}

bool mf_is_finite(const mfloat_t *mfloat)
{
    return mfloat_is_finite(mfloat);
}

bool mf_is_nan(const mfloat_t *mfloat)
{
    return mfloat_is_nan(mfloat);
}

bool mf_is_inf(const mfloat_t *mfloat)
{
    return mfloat_is_inf(mfloat);
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

int mfloat_round_to_precision_internal(mfloat_t *mfloat, size_t precision)
{
    size_t bitlen, excess;
    mint_t *hi = NULL, *trunc = NULL, *low = NULL, *half = NULL;
    int rc = -1;

    if (!mfloat || !mfloat->mantissa || !mfloat_is_finite(mfloat) || precision == 0)
        return -1;

    bitlen = mi_bit_length(mfloat->mantissa);
    if (bitlen <= precision) {
        mfloat->precision = precision;
        return 0;
    }

    excess = bitlen - precision;
    hi = mi_clone(mfloat->mantissa);
    low = mi_clone(mfloat->mantissa);
    if (!hi || !low)
        goto cleanup;

    if (mi_shr(hi, (long)excess) != 0)
        goto cleanup;

    trunc = mi_clone(hi);
    if (!trunc)
        goto cleanup;
    if (mi_shl(trunc, (long)excess) != 0)
        goto cleanup;
    if (mi_sub(low, trunc) != 0)
        goto cleanup;

    half = mi_create_2pow((uint64_t)(excess - 1u));
    if (!half)
        goto cleanup;
    if (mi_cmp(low, half) >= 0) {
        if (mi_inc(hi) != 0)
            goto cleanup;
    }

    mi_clear(mfloat->mantissa);
    if (mi_add(mfloat->mantissa, hi) != 0)
        goto cleanup;
    mfloat->exponent2 += (long)excess;
    mfloat->precision = precision;
    rc = mfloat_normalise(mfloat);

cleanup:
    mi_free(hi);
    mi_free(trunc);
    mi_free(low);
    mi_free(half);
    return rc;
}

static mfloat_t *mfloat_alloc(size_t precision_bits)
{
    mfloat_t *mfloat =
        (mfloat_t *)number_scope_mem_calloc(1u, sizeof(*mfloat), _Alignof(mfloat_t));

    if (!mfloat)
        return NULL;

    mfloat->mantissa = mi_new();
    if (!mfloat->mantissa) {
        number_scope_mem_free(mfloat);
        return NULL;
    }

    mfloat->precision = precision_bits > 0 ? precision_bits
                                           : mfloat_default_precision_bits;
    mfloat->kind = MFLOAT_KIND_FINITE;
    mfloat->immortal = false;
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
    return mfloat_alloc(precision_bits);
}

size_t mfloat_get_default_precision_internal(void)
{
    return mfloat_default_precision_bits;
}

mfloat_t *mfloat_clone_immortal_prec_internal(const mfloat_t *src, size_t precision)
{
    mfloat_t *dst;

    if (!src)
        return NULL;
    dst = mf_new_prec(precision);
    if (!dst)
        return NULL;
    if (mfloat_copy_value(dst, src) != 0) {
        mf_free(dst);
        return NULL;
    }
    if (precision < src->precision &&
        mfloat_round_to_precision_internal(dst, precision) != 0) {
        mf_free(dst);
        return NULL;
    }
    dst->precision = precision;
    return dst;
}

int mfloat_set_from_immortal_internal(mfloat_t *dst, const mfloat_t *src, size_t precision)
{
    if (src && precision == src->precision) {
        int rc = mfloat_copy_value(dst, src);

        if (rc == 0)
            dst->precision = precision;
        return rc;
    }
    if (src && precision < src->precision) {
        int rc = mfloat_copy_value(dst, src);

        if (rc != 0)
            return rc;
        if (mfloat_round_to_precision_internal(dst, precision) != 0)
            return -1;
        dst->precision = precision;
        return 0;
    }
    mfloat_t *tmp = mfloat_clone_immortal_prec_internal(src, precision);
    int rc;

    if (!tmp)
        return -1;
    rc = mfloat_copy_value(dst, tmp);
    if (rc == 0)
        dst->precision = precision;
    mf_free(tmp);
    return rc;
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

mfloat_t *mf_create_mrational(const mrational_t *value)
{
    mfloat_t *mfloat = mf_new();

    if (!mfloat)
        return NULL;
    if (mf_set_mrational(mfloat, value) != 0) {
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
    number_scope_mem_free(mfloat);
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

static size_t mfloat_bits_to_decimal_digits(size_t precision_bits)
{
    size_t digits;

    if (precision_bits == 0u)
        return 0u;
    digits = (size_t)floor((double)precision_bits * MFLOAT_LOG10_2);
    return digits > 0u ? digits : 1u;
}

static size_t mfloat_decimal_digits_to_bits(size_t significant_digits)
{
    size_t bits;

    if (significant_digits == 0u)
        return 0u;
    bits = (size_t)ceil((double)significant_digits * MFLOAT_LOG2_10);
    return bits > 0u ? bits : 1u;
}

size_t mf_get_default_precision(void)
{
    return mfloat_default_precision_bits;
}

size_t mf_get_precision(const mfloat_t *mfloat)
{
    return mfloat ? mfloat->precision : 0;
}

int mf_set_default_precision_digits(size_t significant_digits)
{
    return mf_set_default_precision(
        mfloat_decimal_digits_to_bits(significant_digits));
}

size_t mf_get_default_precision_digits(void)
{
    return mfloat_bits_to_decimal_digits(mf_get_default_precision());
}

int mf_set_precision_digits(mfloat_t *mfloat, size_t significant_digits)
{
    return mf_set_precision(mfloat,
                            mfloat_decimal_digits_to_bits(significant_digits));
}

size_t mf_get_precision_digits(const mfloat_t *mfloat)
{
    return mfloat_bits_to_decimal_digits(mf_get_precision(mfloat));
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
    if (!mfloat)
        return -1;
    if (mfloat_is_immortal(mfloat))
        return -1;
    return qf_to_mfloat_exact(mfloat, value);
}

int mf_set_mrational(mfloat_t *mfloat, const mrational_t *value)
{
    if (!mfloat || !value || mfloat_is_immortal(mfloat))
        return -1;
    if (mf_set_long(mfloat, 0) != 0)
        return -1;
    return mf_add_mrational(mfloat, value);
}

int mf_add_mrational(mfloat_t *mfloat, const mrational_t *value)
{
    size_t precision, work_prec;
    const mint_t *num = NULL;
    const mint_t *den = NULL;
    mint_t *lhs_num = NULL;
    mint_t *rhs_num = NULL;
    mint_t *common_den = NULL;
    mfloat_t *tmp = NULL;
    mfloat_t *den_mf = NULL;
    int rc = -1;

    if (!mfloat || !value)
        return -1;
    if (mfloat_is_immortal(mfloat))
        return -1;

    num = mr_numerator(value);
    den = mr_denominator(value);
    if (!num || !den || mi_is_zero(den))
        goto cleanup;

    precision = mfloat->precision;
    work_prec = precision + 32u;
    tmp = mf_clone(mfloat);
    den_mf = mf_new_prec(work_prec);
    if (!tmp || !den_mf)
        goto cleanup;
    if (mf_set_precision(tmp, work_prec) != 0)
        goto cleanup;

    lhs_num = mi_clone(tmp->mantissa);
    rhs_num = mi_clone(num);
    common_den = mi_clone(den);
    if (!lhs_num || !rhs_num || !common_den)
        goto cleanup;
    if (tmp->sign < 0 && mi_neg(lhs_num) != 0)
        goto cleanup;

    if (tmp->exponent2 >= 0) {
        if (mi_shl(lhs_num, tmp->exponent2) != 0 ||
            mi_mul(lhs_num, common_den) != 0)
            goto cleanup;
    } else {
        long shift = -tmp->exponent2;
        if (mi_mul(lhs_num, common_den) != 0 ||
            mi_shl(rhs_num, shift) != 0 ||
            mi_shl(common_den, shift) != 0)
            goto cleanup;
    }

    if (mi_add(lhs_num, rhs_num) != 0)
        goto cleanup;
    if (mfloat_set_from_signed_mint(tmp, lhs_num, 0) != 0 ||
        mfloat_set_from_signed_mint(den_mf, common_den, 0) != 0 ||
        mf_div(tmp, den_mf) != 0)
        goto cleanup;
    if (mfloat_copy_value(mfloat, tmp) != 0)
        goto cleanup;
    rc = mfloat_round_to_precision_internal(mfloat, precision);

cleanup:
    mi_free(lhs_num);
    mi_free(rhs_num);
    mi_free(common_den);
    mf_free(tmp);
    mf_free(den_mf);
    return rc;
}

int mf_mul_mrational(mfloat_t *mfloat, const mrational_t *value)
{
    size_t precision, work_prec;
    const mint_t *num = NULL;
    const mint_t *den = NULL;
    mint_t *num_mag = NULL;
    mint_t *den_mag = NULL;
    mfloat_t *tmp = NULL;
    mfloat_t *den_mf = NULL;
    int rc = -1;

    if (!mfloat || !value)
        return -1;
    if (mfloat_is_immortal(mfloat))
        return -1;

    num = mr_numerator(value);
    den = mr_denominator(value);
    if (!num || !den || mi_is_zero(den))
        goto cleanup;

    precision = mfloat->precision;
    work_prec = precision + 32u;
    tmp = mf_clone(mfloat);
    den_mf = mf_new_prec(work_prec);
    if (!tmp || !den_mf)
        goto cleanup;
    if (mf_set_precision(tmp, work_prec) != 0)
        goto cleanup;

    if (mi_is_zero(num)) {
        rc = mf_set_long(mfloat, 0);
        goto cleanup;
    }

    num_mag = mi_clone(num);
    den_mag = mi_clone(den);
    if (!num_mag || !den_mag)
        goto cleanup;
    if (mi_is_negative(num)) {
        if (mf_neg(tmp) != 0 || mi_abs(num_mag) != 0)
            goto cleanup;
    }
    if (mi_mul(tmp->mantissa, num_mag) != 0 || mfloat_normalise(tmp) != 0)
        goto cleanup;
    if (mfloat_set_from_signed_mint(den_mf, den_mag, 0) != 0 || mf_div(tmp, den_mf) != 0)
        goto cleanup;
    if (mfloat_copy_value(mfloat, tmp) != 0)
        goto cleanup;
    rc = mfloat_round_to_precision_internal(mfloat, precision);

cleanup:
    mi_free(num_mag);
    mi_free(den_mag);
    mf_free(tmp);
    mf_free(den_mf);
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
