#include "mcomplex_internal.h"
#include "../mfloat/mfloat_internal.h"

int mcomplex_apply_unary(mcomplex_t *mcomplex, qcomplex_t (*fn)(qcomplex_t))
{
    if (!mcomplex || !fn)
        return -1;
    return mc_set_qcomplex(mcomplex, fn(mc_to_qcomplex(mcomplex)));
}

int mcomplex_apply_binary(mcomplex_t *mcomplex,
                          const mcomplex_t *other,
                          qcomplex_t (*fn)(qcomplex_t, qcomplex_t))
{
    if (!mcomplex || !other || !fn)
        return -1;
    return mc_set_qcomplex(
        mcomplex, fn(mc_to_qcomplex(mcomplex), mc_to_qcomplex(other)));
}

static qcomplex_t mcomplex_pow_int_local(qcomplex_t base, int exponent)
{
    qcomplex_t result = QC_ONE;

    if (exponent < 0)
        return qc_div(QC_ONE, mcomplex_pow_int_local(base, -exponent));
    while (exponent > 0) {
        if ((exponent & 1) != 0)
            result = qc_mul(result, base);
        base = qc_mul(base, base);
        exponent >>= 1;
    }
    return result;
}

static int mcomplex_round_parts(mcomplex_t *mcomplex, size_t precision_bits)
{
    if (!mcomplex)
        return -1;
    if (mcomplex->real && mfloat_is_finite(mcomplex->real) &&
        mfloat_round_to_precision_internal(mcomplex->real, precision_bits) != 0)
        return -1;
    if (mcomplex->imag && mfloat_is_finite(mcomplex->imag) &&
        mfloat_round_to_precision_internal(mcomplex->imag, precision_bits) != 0)
        return -1;
    if (mf_set_precision(mcomplex->real, precision_bits) != 0 ||
        mf_set_precision(mcomplex->imag, precision_bits) != 0)
        return -1;
    return 0;
}

int mc_abs(mcomplex_t *mcomplex)
{
    mfloat_t *mag2 = NULL, *tmp = NULL, *root = NULL;
    size_t precision_bits;
    size_t work_prec;
    qfloat_t seed;
    int iter;

    if (!mcomplex)
        return -1;
    precision_bits = mc_get_precision(mcomplex);
    work_prec = precision_bits * 2u;
    if (mcomplex_ensure_mutable(mcomplex) != 0)
        return -1;
    mag2 = mf_clone(mcomplex->real);
    tmp = mf_clone(mcomplex->imag);
    if (!mag2 || !tmp)
        goto fail;
    if (mf_set_precision(mag2, work_prec) != 0 ||
        mf_set_precision(tmp, work_prec) != 0)
        goto fail;
    if (mf_mul(mag2, mcomplex->real) != 0 ||
        mf_mul(tmp, mcomplex->imag) != 0 ||
        mf_add(mag2, tmp) != 0)
        goto fail;

    seed = qf_sqrt(mf_to_qfloat(mag2));
    root = mf_new_prec(work_prec);
    if (!root || mf_set_qfloat(root, seed) != 0)
        goto fail;
    if (mf_is_zero(root)) {
        if (mfloat_copy_value(root, mag2) != 0)
            goto fail;
    }

    for (iter = 0; iter < 10; ++iter) {
        mf_free(tmp);
        tmp = mf_clone(mag2);
        if (!tmp)
            goto fail;
        if (mf_set_precision(tmp, work_prec) != 0 ||
            mf_div(tmp, root) != 0 ||
            mf_add(tmp, root) != 0 ||
            mf_ldexp(tmp, -1) != 0)
            goto fail;
        mf_free(root);
        root = tmp;
        tmp = NULL;
    }

    if (mfloat_copy_value(mcomplex->real, root) != 0)
        goto fail;
    if (precision_bits < mf_get_precision(root) &&
        mfloat_round_to_precision_internal(mcomplex->real, precision_bits) != 0)
        goto fail;
    mf_clear(mcomplex->imag);
    if (mcomplex_round_parts(mcomplex, precision_bits) != 0)
        goto fail;

    mf_free(root);
    mf_free(tmp);
    mf_free(mag2);
    return 0;

fail:
    mf_free(root);
    mf_free(tmp);
    mf_free(mag2);
    return -1;
}

int mc_neg(mcomplex_t *mcomplex)
{
    if (!mcomplex)
        return -1;
    if (mcomplex_ensure_mutable(mcomplex) != 0)
        return -1;
    if (mf_neg(mcomplex->real) != 0 || mf_neg(mcomplex->imag) != 0)
        return -1;
    return 0;
}

int mc_conj(mcomplex_t *mcomplex)
{
    if (!mcomplex)
        return -1;
    if (mcomplex_ensure_mutable(mcomplex) != 0)
        return -1;
    return mf_neg(mcomplex->imag);
}

int mc_add(mcomplex_t *mcomplex, const mcomplex_t *other)
{
    if (!mcomplex || !other)
        return -1;
    if (mcomplex_ensure_mutable(mcomplex) != 0)
        return -1;
    if (mf_add(mcomplex->real, mc_real(other)) != 0 ||
        mf_add(mcomplex->imag, mc_imag(other)) != 0)
        return -1;
    return 0;
}

int mc_sub(mcomplex_t *mcomplex, const mcomplex_t *other)
{
    if (!mcomplex || !other)
        return -1;
    if (mcomplex_ensure_mutable(mcomplex) != 0)
        return -1;
    if (mf_sub(mcomplex->real, mc_real(other)) != 0 ||
        mf_sub(mcomplex->imag, mc_imag(other)) != 0)
        return -1;
    return 0;
}

int mc_mul(mcomplex_t *mcomplex, const mcomplex_t *other)
{
    mfloat_t *a = NULL, *b = NULL, *real_part = NULL, *imag_part = NULL, *tmp = NULL;
    size_t precision_bits;

    if (!mcomplex || !other)
        return -1;
    precision_bits = mc_get_precision(mcomplex);
    if (mcomplex_ensure_mutable(mcomplex) != 0)
        return -1;
    a = mf_clone(mcomplex->real);
    b = mf_clone(mcomplex->imag);
    real_part = mf_clone(a);
    imag_part = mf_clone(b);
    tmp = mf_clone(b);
    if (!a || !b || !real_part || !imag_part || !tmp)
        goto fail;

    if (mf_mul(real_part, mc_real(other)) != 0 ||
        mf_mul(tmp, mc_imag(other)) != 0 ||
        mf_sub(real_part, tmp) != 0)
        goto fail;

    mf_free(tmp);
    tmp = mf_clone(a);
    if (!tmp)
        goto fail;
    if (mf_mul(imag_part, mc_real(other)) != 0 ||
        mf_mul(tmp, mc_imag(other)) != 0 ||
        mf_add(imag_part, tmp) != 0)
        goto fail;

    if (mc_set(mcomplex, real_part, imag_part) != 0 ||
        mcomplex_round_parts(mcomplex, precision_bits) != 0)
        goto fail;

    mf_free(tmp);
    mf_free(imag_part);
    mf_free(real_part);
    mf_free(b);
    mf_free(a);
    return 0;

fail:
    mf_free(tmp);
    mf_free(imag_part);
    mf_free(real_part);
    mf_free(b);
    mf_free(a);
    return -1;
}

int mc_div(mcomplex_t *mcomplex, const mcomplex_t *other)
{
    mfloat_t *a = NULL, *b = NULL, *den = NULL, *real_part = NULL, *imag_part = NULL, *tmp = NULL;
    size_t precision_bits;

    if (!mcomplex || !other)
        return -1;
    precision_bits = mc_get_precision(mcomplex);
    if (mcomplex_ensure_mutable(mcomplex) != 0)
        return -1;
    a = mf_clone(mcomplex->real);
    b = mf_clone(mcomplex->imag);
    den = mf_clone(mc_real(other));
    real_part = mf_clone(a);
    imag_part = mf_clone(b);
    tmp = mf_clone(mc_imag(other));
    if (!a || !b || !den || !real_part || !imag_part || !tmp)
        goto fail;

    if (mf_mul(den, mc_real(other)) != 0 ||
        mf_mul(tmp, mc_imag(other)) != 0 ||
        mf_add(den, tmp) != 0)
        goto fail;

    mf_free(tmp);
    tmp = mf_clone(b);
    if (!tmp)
        goto fail;
    if (mf_mul(real_part, mc_real(other)) != 0 ||
        mf_mul(tmp, mc_imag(other)) != 0 ||
        mf_add(real_part, tmp) != 0 ||
        mf_div(real_part, den) != 0)
        goto fail;

    mf_free(tmp);
    tmp = mf_clone(a);
    if (!tmp)
        goto fail;
    if (mf_mul(imag_part, mc_real(other)) != 0 ||
        mf_mul(tmp, mc_imag(other)) != 0 ||
        mf_sub(imag_part, tmp) != 0 ||
        mf_div(imag_part, den) != 0)
        goto fail;

    if (mc_set(mcomplex, real_part, imag_part) != 0 ||
        mcomplex_round_parts(mcomplex, precision_bits) != 0)
        goto fail;

    mf_free(tmp);
    mf_free(imag_part);
    mf_free(real_part);
    mf_free(den);
    mf_free(b);
    mf_free(a);
    return 0;

fail:
    mf_free(tmp);
    mf_free(imag_part);
    mf_free(real_part);
    mf_free(den);
    mf_free(b);
    mf_free(a);
    return -1;
}

int mc_inv(mcomplex_t *mcomplex)
{
    mfloat_t *den = NULL, *tmp = NULL;
    size_t precision_bits;

    if (!mcomplex)
        return -1;
    precision_bits = mc_get_precision(mcomplex);
    if (mcomplex_ensure_mutable(mcomplex) != 0)
        return -1;
    den = mf_clone(mcomplex->real);
    tmp = mf_clone(mcomplex->imag);
    if (!den || !tmp)
        goto fail;
    if (mf_mul(den, mcomplex->real) != 0 ||
        mf_mul(tmp, mcomplex->imag) != 0 ||
        mf_add(den, tmp) != 0 ||
        mf_div(mcomplex->real, den) != 0 ||
        mf_div(mcomplex->imag, den) != 0 ||
        mf_neg(mcomplex->imag) != 0 ||
        mcomplex_round_parts(mcomplex, precision_bits) != 0)
        goto fail;

    mf_free(tmp);
    mf_free(den);
    return 0;

fail:
    mf_free(tmp);
    mf_free(den);
    return -1;
}

int mc_pow_int(mcomplex_t *mcomplex, int exponent)
{
    if (!mcomplex)
        return -1;
    return mc_set_qcomplex(
        mcomplex, mcomplex_pow_int_local(mc_to_qcomplex(mcomplex), exponent));
}

int mc_pow(mcomplex_t *mcomplex, const mcomplex_t *exponent) { return mcomplex_apply_binary(mcomplex, exponent, qc_pow); }
int mc_ldexp(mcomplex_t *mcomplex, int exponent2)
{
    if (!mcomplex)
        return -1;
    return mc_set_qcomplex(mcomplex, qc_ldexp(mc_to_qcomplex(mcomplex), exponent2));
}
int mc_sqrt(mcomplex_t *mcomplex)
{
    mcomplex_t *mag = NULL;
    mfloat_t *real_part = NULL;
    mfloat_t *imag_part = NULL;
    mfloat_t *neg_real = NULL;
    size_t precision_bits;
    size_t work_prec;

    if (!mcomplex)
        return -1;

    precision_bits = mc_get_precision(mcomplex);
    work_prec = precision_bits + 192u;

    if (mf_is_zero(mc_imag(mcomplex))) {
        if (mcomplex_ensure_mutable(mcomplex) != 0)
            return -1;
        if (mf_ge(mc_real(mcomplex), MF_ZERO)) {
            if (mf_sqrt(mcomplex->real) != 0)
                return -1;
            mf_clear(mcomplex->imag);
            return 0;
        }

        neg_real = mf_clone(mc_real(mcomplex));
        if (!neg_real)
            return -1;
        if (mf_set_precision(neg_real, work_prec) != 0 ||
            mf_neg(neg_real) != 0 ||
            mf_sqrt(neg_real) != 0 ||
            mc_set(mcomplex, MF_ZERO, neg_real) != 0) {
            mf_free(neg_real);
            return -1;
        }
        mf_free(neg_real);
        return 0;
    }

    mag = mc_clone(mcomplex);
    if (!mag)
        return -1;
    if (mc_set_precision(mag, work_prec) != 0 ||
        mc_abs(mag) != 0) {
        mc_free(mag);
        return -1;
    }

    real_part = mf_clone(mc_real(mag));
    imag_part = mf_clone(mc_real(mag));
    if (!real_part || !imag_part)
        goto fail;
    if (mf_set_precision(real_part, work_prec) != 0 ||
        mf_set_precision(imag_part, work_prec) != 0)
        goto fail;

    if (mf_add(real_part, mc_real(mcomplex)) != 0 ||
        mf_ldexp(real_part, -1) != 0 ||
        mf_sqrt(real_part) != 0)
        goto fail;

    if (mf_sub(imag_part, mc_real(mcomplex)) != 0 ||
        mf_ldexp(imag_part, -1) != 0 ||
        mf_sqrt(imag_part) != 0)
        goto fail;

    if (mf_lt(mc_imag(mcomplex), MF_ZERO) && mf_neg(imag_part) != 0)
        goto fail;

    if (mc_set(mcomplex, real_part, imag_part) != 0)
        goto fail;

    mf_free(imag_part);
    mf_free(real_part);
    mc_free(mag);
    return 0;

fail:
    mf_free(neg_real);
    mf_free(imag_part);
    mf_free(real_part);
    mc_free(mag);
    return -1;
}
int mc_floor(mcomplex_t *mcomplex) { return mcomplex_apply_unary(mcomplex, qc_floor); }
int mc_hypot(mcomplex_t *mcomplex, const mcomplex_t *other) { return mcomplex_apply_binary(mcomplex, other, qc_hypot); }
