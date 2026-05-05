#include "mfloat_internal.h"

#include <limits.h>
#include <stddef.h>

#define MFLOAT_ARITH_SLACK_BITS 64u

static int mfloat_set_special(mfloat_t *mfloat, mfloat_kind_t kind)
{
    if (!mfloat || !mfloat->mantissa || mfloat_is_immortal(mfloat))
        return -1;
    mi_clear(mfloat->mantissa);
    mfloat->kind = kind;
    mfloat->exponent2 = 0;
    if (kind == MFLOAT_KIND_POSINF)
        mfloat->sign = 1;
    else if (kind == MFLOAT_KIND_NEGINF)
        mfloat->sign = -1;
    else
        mfloat->sign = 0;
    return 0;
}

static int mfloat_trim_oversized_inplace(mfloat_t *mfloat)
{
    size_t bitlen, target_precision;

    if (!mfloat || !mfloat->mantissa || !mfloat_is_finite(mfloat) || mf_is_zero(mfloat))
        return 0;
    bitlen = mi_bit_length(mfloat->mantissa);
    target_precision = mfloat->precision + MFLOAT_ARITH_SLACK_BITS;
    if (bitlen <= target_precision)
        return 0;
    return mfloat_round_to_precision_internal(mfloat, target_precision);
}

static mfloat_t *mfloat_clone_trimmed_operand(const mfloat_t *src)
{
    mfloat_t *tmp;

    if (!src)
        return NULL;
    tmp = mf_clone(src);
    if (!tmp)
        return NULL;
    if (mfloat_trim_oversized_inplace(tmp) != 0) {
        mf_free(tmp);
        return NULL;
    }
    return tmp;
}

int mf_cmp(const mfloat_t *a, const mfloat_t *b)
{
    mfloat_t *ta = NULL, *tb = NULL;
    mint_t *lhs = NULL, *rhs = NULL;
    long common_exp;
    int cmp = 0;

    if (!a || !b)
        return 0;
    if (!mfloat_is_finite(a) || !mfloat_is_finite(b)) {
        if (a && b && a->kind == b->kind) {
            if (a->kind == MFLOAT_KIND_NAN)
                return 1;
            return 0;
        }
        if (!a || a->kind == MFLOAT_KIND_NEGINF)
            return -1;
        if (!b || b->kind == MFLOAT_KIND_NEGINF)
            return 1;
        if (a->kind == MFLOAT_KIND_NAN || b->kind == MFLOAT_KIND_NAN)
            return 1;
        if (a->kind == MFLOAT_KIND_POSINF)
            return 1;
        if (b->kind == MFLOAT_KIND_POSINF)
            return -1;
    }
    if (a->sign != b->sign)
        return a->sign < b->sign ? -1 : 1;
    if (a->sign == 0)
        return 0;

    ta = mfloat_clone_trimmed_operand(a);
    tb = mfloat_clone_trimmed_operand(b);
    if (!ta || !tb)
        goto cleanup;
    a = ta;
    b = tb;

    common_exp = a->exponent2 < b->exponent2 ? a->exponent2 : b->exponent2;
    lhs = mfloat_to_scaled_mint(a, common_exp);
    rhs = mfloat_to_scaled_mint(b, common_exp);
    if (!lhs || !rhs)
        goto cleanup;

    cmp = mi_cmp(lhs, rhs);

cleanup:
    mf_free(ta);
    mf_free(tb);
    mi_free(lhs);
    mi_free(rhs);
    return cmp;
}

bool mf_eq(const mfloat_t *a, const mfloat_t *b)
{
    if (!a || !b || a->kind == MFLOAT_KIND_NAN || b->kind == MFLOAT_KIND_NAN)
        return false;
    return mf_cmp(a, b) == 0;
}
bool mf_lt(const mfloat_t *a, const mfloat_t *b)
{
    if (!a || !b || a->kind == MFLOAT_KIND_NAN || b->kind == MFLOAT_KIND_NAN)
        return false;
    return mf_cmp(a, b) < 0;
}
bool mf_le(const mfloat_t *a, const mfloat_t *b)
{
    if (!a || !b || a->kind == MFLOAT_KIND_NAN || b->kind == MFLOAT_KIND_NAN)
        return false;
    return mf_cmp(a, b) <= 0;
}
bool mf_gt(const mfloat_t *a, const mfloat_t *b)
{
    if (!a || !b || a->kind == MFLOAT_KIND_NAN || b->kind == MFLOAT_KIND_NAN)
        return false;
    return mf_cmp(a, b) > 0;
}
bool mf_ge(const mfloat_t *a, const mfloat_t *b)
{
    if (!a || !b || a->kind == MFLOAT_KIND_NAN || b->kind == MFLOAT_KIND_NAN)
        return false;
    return mf_cmp(a, b) >= 0;
}

int mf_abs(mfloat_t *mfloat)
{
    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NEGINF)
        return mfloat_set_special(mfloat, MFLOAT_KIND_POSINF);
    if (mfloat->kind == MFLOAT_KIND_POSINF || mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (!mf_is_zero(mfloat))
        mfloat->sign = 1;
    return 0;
}

int mf_neg(mfloat_t *mfloat)
{
    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_POSINF)
        return mfloat_set_special(mfloat, MFLOAT_KIND_NEGINF);
    if (mfloat->kind == MFLOAT_KIND_NEGINF)
        return mfloat_set_special(mfloat, MFLOAT_KIND_POSINF);
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (!mf_is_zero(mfloat))
        mfloat->sign = (short)-mfloat->sign;
    return 0;
}

int mf_add(mfloat_t *mfloat, const mfloat_t *other)
{
    mfloat_t *other_trimmed = NULL;
    mint_t *lhs = NULL, *rhs = NULL;
    long common_exp;
    int rc = -1;

    if (!mfloat || !other)
        return -1;
    if (!mfloat_is_finite(mfloat) || !mfloat_is_finite(other)) {
        if (mfloat->kind == MFLOAT_KIND_NAN || other->kind == MFLOAT_KIND_NAN)
            return mfloat_set_special(mfloat, MFLOAT_KIND_NAN);
        if ((mfloat->kind == MFLOAT_KIND_POSINF && other->kind == MFLOAT_KIND_NEGINF) ||
            (mfloat->kind == MFLOAT_KIND_NEGINF && other->kind == MFLOAT_KIND_POSINF))
            return mfloat_set_special(mfloat, MFLOAT_KIND_NAN);
        if (other->kind == MFLOAT_KIND_POSINF || other->kind == MFLOAT_KIND_NEGINF)
            return mfloat_set_special(mfloat, other->kind);
        return 0;
    }
    if (mf_is_zero(other))
        return 0;
    if (mf_is_zero(mfloat))
        return mfloat_copy_value(mfloat, other);
    if (mfloat_trim_oversized_inplace(mfloat) != 0)
        return -1;
    other_trimmed = mfloat_clone_trimmed_operand(other);
    if (!other_trimmed)
        return -1;
    other = other_trimmed;

    common_exp = mfloat->exponent2 < other->exponent2
               ? mfloat->exponent2 : other->exponent2;
    lhs = mfloat_to_scaled_mint(mfloat, common_exp);
    rhs = mfloat_to_scaled_mint(other, common_exp);
    if (!lhs || !rhs)
        goto cleanup;

    if (mi_add(lhs, rhs) != 0)
        goto cleanup;

    rc = mfloat_set_from_signed_mint(mfloat, lhs, common_exp);

cleanup:
    mf_free(other_trimmed);
    mi_free(lhs);
    mi_free(rhs);
    return rc;
}

int mf_add_long(mfloat_t *mfloat, long value)
{
    mint_t *lhs = NULL, *rhs = NULL;
    long common_exp;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (value == 0)
        return 0;
    if (!mfloat_is_finite(mfloat)) {
        if (mfloat->kind == MFLOAT_KIND_NAN)
            return mfloat_set_special(mfloat, MFLOAT_KIND_NAN);
        return 0;
    }
    if (value == LONG_MIN) {
        mfloat_t *tmp = mf_create_long(value);

        if (!tmp)
            return -1;
        tmp->precision = mfloat->precision;
        rc = mf_add(mfloat, tmp);
        mf_free(tmp);
        return rc;
    }
    if (mf_is_zero(mfloat))
        return mf_set_long(mfloat, value);

    if (mfloat_trim_oversized_inplace(mfloat) != 0)
        return -1;

    common_exp = mfloat->exponent2 < 0 ? mfloat->exponent2 : 0;
    lhs = mfloat_to_scaled_mint(mfloat, common_exp);
    rhs = mi_create_long(value < 0 ? -value : value);
    if (!lhs || !rhs)
        goto cleanup;
    if (common_exp < 0 && mi_shl(rhs, -common_exp) != 0)
        goto cleanup;
    if (value < 0 && mi_neg(rhs) != 0)
        goto cleanup;
    if (mi_add(lhs, rhs) != 0)
        goto cleanup;

    rc = mfloat_set_from_signed_mint(mfloat, lhs, common_exp);

cleanup:
    mi_free(lhs);
    mi_free(rhs);
    return rc;
}

int mf_sub(mfloat_t *mfloat, const mfloat_t *other)
{
    mfloat_t *tmp;
    int rc;

    if (!mfloat || !other)
        return -1;
    tmp = mf_clone(other);
    if (!tmp)
        return -1;
    if (mf_neg(tmp) != 0) {
        mf_free(tmp);
        return -1;
    }
    rc = mf_add(mfloat, tmp);
    mf_free(tmp);
    return rc;
}

int mf_mul(mfloat_t *mfloat, const mfloat_t *other)
{
    mfloat_t *other_trimmed = NULL;

    if (!mfloat || !other || !mfloat->mantissa || !other->mantissa)
        return -1;
    if (!mfloat_is_finite(mfloat) || !mfloat_is_finite(other)) {
        if (mfloat->kind == MFLOAT_KIND_NAN || other->kind == MFLOAT_KIND_NAN)
            return mfloat_set_special(mfloat, MFLOAT_KIND_NAN);
        if ((mf_is_zero(mfloat) && !mfloat_is_finite(other)) ||
            (mf_is_zero(other) && !mfloat_is_finite(mfloat)))
            return mfloat_set_special(mfloat, MFLOAT_KIND_NAN);
        if (other->kind == MFLOAT_KIND_POSINF || other->kind == MFLOAT_KIND_NEGINF ||
            mfloat->kind == MFLOAT_KIND_POSINF || mfloat->kind == MFLOAT_KIND_NEGINF) {
            int sign = mf_get_sign(mfloat) * mf_get_sign(other);
            return mfloat_set_special(mfloat, sign < 0 ? MFLOAT_KIND_NEGINF : MFLOAT_KIND_POSINF);
        }
    }
    if (mf_is_zero(mfloat) || mf_is_zero(other)) {
        mf_clear(mfloat);
        return 0;
    }
    if (mfloat_trim_oversized_inplace(mfloat) != 0)
        return -1;
    other_trimmed = mfloat_clone_trimmed_operand(other);
    if (!other_trimmed)
        return -1;
    other = other_trimmed;

    if (mi_mul(mfloat->mantissa, other->mantissa) != 0) {
        mf_free(other_trimmed);
        return -1;
    }
    mfloat->sign = (mfloat->sign == other->sign) ? 1 : -1;
    mfloat->exponent2 += other->exponent2;
    mf_free(other_trimmed);
    return mfloat_normalise(mfloat);
}

int mf_mul_long(mfloat_t *mfloat, long value)
{
    long abs_value;

    if (!mfloat)
        return -1;
    if (value == LONG_MIN) {
        mfloat_t *tmp = mf_create_long(value);
        int rc;

        if (!tmp)
            return -1;
        tmp->precision = mfloat->precision;
        rc = mf_mul(mfloat, tmp);
        mf_free(tmp);
        return rc;
    }
    if (!mfloat_is_finite(mfloat)) {
        if (mfloat->kind == MFLOAT_KIND_NAN)
            return mfloat_set_special(mfloat, MFLOAT_KIND_NAN);
        if (value == 0)
            return mfloat_set_special(mfloat, MFLOAT_KIND_NAN);
        if (value < 0)
            return mfloat_set_special(mfloat,
                                      mfloat->kind == MFLOAT_KIND_POSINF ? MFLOAT_KIND_NEGINF
                                                                         : MFLOAT_KIND_POSINF);
        return 0;
    }
    if (mf_is_zero(mfloat) || value == 0) {
        mf_clear(mfloat);
        return 0;
    }
    if (mfloat_trim_oversized_inplace(mfloat) != 0)
        return -1;

    abs_value = value < 0 ? -value : value;
    if (mi_mul_long(mfloat->mantissa, abs_value) != 0)
        return -1;
    if (value < 0)
        mfloat->sign = (short)-mfloat->sign;
    return mfloat_normalise(mfloat);
}

int mf_div(mfloat_t *mfloat, const mfloat_t *other)
{
    mfloat_t *other_trimmed = NULL;
    mint_t *num = NULL, *den = NULL, *q = NULL, *r = NULL, *twor = NULL;
    size_t shift_bits;
    long exponent2;
    int rc = -1;

    if (!mfloat || !other || !mfloat->mantissa || !other->mantissa)
        return -1;
    if (!mfloat_is_finite(mfloat) || !mfloat_is_finite(other)) {
        if (mfloat->kind == MFLOAT_KIND_NAN || other->kind == MFLOAT_KIND_NAN)
            return mfloat_set_special(mfloat, MFLOAT_KIND_NAN);
        if ((mfloat->kind == MFLOAT_KIND_POSINF || mfloat->kind == MFLOAT_KIND_NEGINF) &&
            (other->kind == MFLOAT_KIND_POSINF || other->kind == MFLOAT_KIND_NEGINF))
            return mfloat_set_special(mfloat, MFLOAT_KIND_NAN);
        if (other->kind == MFLOAT_KIND_POSINF || other->kind == MFLOAT_KIND_NEGINF) {
            mf_clear(mfloat);
            return 0;
        }
        if (mfloat->kind == MFLOAT_KIND_POSINF || mfloat->kind == MFLOAT_KIND_NEGINF) {
            int sign = mf_get_sign(mfloat) * mf_get_sign(other);
            return mfloat_set_special(mfloat, sign < 0 ? MFLOAT_KIND_NEGINF : MFLOAT_KIND_POSINF);
        }
    }
    if (mf_is_zero(other)) {
        if (mf_is_zero(mfloat))
            return mfloat_set_special(mfloat, MFLOAT_KIND_NAN);
        return mfloat_set_special(mfloat, mf_get_sign(mfloat) * mf_get_sign(other) < 0
                                          ? MFLOAT_KIND_NEGINF : MFLOAT_KIND_POSINF);
    }
    if (mf_is_zero(mfloat))
        return 0;
    if (mfloat_trim_oversized_inplace(mfloat) != 0)
        return -1;
    other_trimmed = mfloat_clone_trimmed_operand(other);
    if (!other_trimmed)
        return -1;
    other = other_trimmed;

    num = mi_clone(mfloat->mantissa);
    den = mi_clone(other->mantissa);
    q = mi_new();
    r = mi_new();
    if (!num || !den || !q || !r)
        goto cleanup;

    shift_bits = mfloat->precision + mi_bit_length(den) + MFLOAT_PARSE_GUARD_BITS;
    if (mi_shl(num, (long)shift_bits) != 0)
        goto cleanup;
    if (mi_divmod(num, den, q, r) != 0)
        goto cleanup;

    twor = mi_clone(r);
    if (!twor || mi_mul_long(twor, 2) != 0)
        goto cleanup;
    if (mi_cmp(twor, den) >= 0) {
        if (mi_inc(q) != 0)
            goto cleanup;
    }

    mi_clear(mfloat->mantissa);
    if (mi_add(mfloat->mantissa, q) != 0)
        goto cleanup;

    exponent2 = mfloat->exponent2 - other->exponent2 - (long)shift_bits;
    mfloat->sign = (mfloat->sign == other->sign) ? 1 : -1;
    mfloat->exponent2 = exponent2;
    rc = mfloat_normalise(mfloat);

cleanup:
    mf_free(other_trimmed);
    mi_free(num);
    mi_free(den);
    mi_free(q);
    mi_free(r);
    mi_free(twor);
    return rc;
}

int mf_inv(mfloat_t *mfloat)
{
    mfloat_t *one;
    int rc;

    if (!mfloat)
        return -1;
    if (mfloat->kind == MFLOAT_KIND_NAN)
        return 0;
    if (mfloat->kind == MFLOAT_KIND_POSINF || mfloat->kind == MFLOAT_KIND_NEGINF) {
        mf_clear(mfloat);
        return 0;
    }
    if (mf_is_zero(mfloat))
        return mfloat_set_special(mfloat, mfloat->sign < 0 ? MFLOAT_KIND_NEGINF : MFLOAT_KIND_POSINF);

    one = mf_clone(MF_ONE);
    if (!one)
        return -1;
    one->precision = mfloat->precision;
    rc = mf_div(one, mfloat);
    if (rc == 0)
        rc = mfloat_copy_value(mfloat, one);
    mf_free(one);
    return rc;
}

int mf_pow_int(mfloat_t *mfloat, int exponent)
{
    mfloat_t *base = NULL;
    mfloat_t *result = NULL;
    int exp;
    int rc = -1;

    if (!mfloat)
        return -1;
    if (!mfloat_is_finite(mfloat))
        return 0;
    if (exponent == 0)
        return mf_set_long(mfloat, 1);
    if (mf_is_zero(mfloat))
        return exponent < 0 ? -1 : 0;

    base = mf_clone(mfloat);
    result = mf_new_prec(mfloat->precision);
    if (!base || !result || mf_set_long(result, 1) != 0)
        goto cleanup;

    exp = exponent < 0 ? -exponent : exponent;
    while (exp > 0) {
        if (exp & 1) {
            if (mf_mul(result, base) != 0)
                goto cleanup;
        }
        exp >>= 1;
        if (exp > 0 && mf_mul(base, base) != 0)
            goto cleanup;
    }

    if (exponent < 0) {
        mfloat_t *one = mf_new_prec(mfloat->precision);
        if (!one || mf_set_long(one, 1) != 0 || mf_div(one, result) != 0) {
            mf_free(one);
            goto cleanup;
        }
        mf_free(result);
        result = one;
    }

    rc = mfloat_copy_value(mfloat, result);

cleanup:
    mf_free(base);
    mf_free(result);
    return rc;
}

int mf_ldexp(mfloat_t *mfloat, int exponent2)
{
    if (!mfloat)
        return -1;
    if (!mfloat_is_finite(mfloat))
        return 0;
    if (mf_is_zero(mfloat))
        return 0;
    mfloat->exponent2 += exponent2;
    return 0;
}

int mf_sqrt(mfloat_t *mfloat)
{
    mint_t *root = NULL;
    mint_t *check = NULL;
    mint_t *work = NULL;
    size_t bitlen;
    long exponent2;
    size_t shift_bits;

    if (!mfloat || !mfloat->mantissa)
        return -1;
    if (!mfloat_is_finite(mfloat))
        return mfloat->kind == MFLOAT_KIND_NAN ? 0 : -1;
    if (mfloat->sign < 0)
        return -1;
    if (mf_is_zero(mfloat))
        return 0;

    if ((mfloat->exponent2 & 1l) == 0) {
        root = mi_clone(mfloat->mantissa);
        check = mi_clone(mfloat->mantissa);
        if (!root || !check) {
            mi_free(root);
            mi_free(check);
            return -1;
        }
        if (mi_sqrt(root) != 0 || mi_square(root) != 0) {
            mi_free(root);
            mi_free(check);
            return -1;
        }
        if (mi_cmp(root, check) == 0) {
            if (mi_sqrt(check) != 0) {
                mi_free(root);
                mi_free(check);
                return -1;
            }
            mi_clear(mfloat->mantissa);
            if (mi_add(mfloat->mantissa, check) != 0) {
                mi_free(root);
                mi_free(check);
                return -1;
            }
            mfloat->sign = 1;
            mfloat->exponent2 /= 2l;
            mi_free(root);
            mi_free(check);
            return mfloat_normalise(mfloat);
        }
        mi_free(root);
        mi_free(check);
    }

    bitlen = mi_bit_length(mfloat->mantissa);
    shift_bits = mfloat->precision * 2u + MFLOAT_PARSE_GUARD_BITS;
    if (((long)shift_bits & 1l) != (mfloat->exponent2 & 1l))
        shift_bits++;

    if ((long)bitlen >= (long)(mfloat->precision * 2u))
        shift_bits = MFLOAT_PARSE_GUARD_BITS + ((mfloat->exponent2 & 1l) ? 1u : 0u);

    work = mi_clone(mfloat->mantissa);
    if (!work)
        return -1;
    if (mi_shl(work, (long)shift_bits) != 0) {
        mi_free(work);
        return -1;
    }
    if (mi_sqrt(work) != 0) {
        mi_free(work);
        return -1;
    }

    mi_clear(mfloat->mantissa);
    if (mi_add(mfloat->mantissa, work) != 0) {
        mi_free(work);
        return -1;
    }

    exponent2 = (mfloat->exponent2 - (long)shift_bits) / 2l;
    mfloat->sign = 1;
    mfloat->exponent2 = exponent2;
    mi_free(work);
    return mfloat_normalise(mfloat);
}
