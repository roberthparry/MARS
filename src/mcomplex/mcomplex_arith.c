#include "mcomplex_internal.h"

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

int mc_abs(mcomplex_t *mcomplex)
{
    qfloat_t absval;
    qcomplex_t result;

    if (!mcomplex)
        return -1;
    absval = qc_abs(mc_to_qcomplex(mcomplex));
    result = qc_make(absval, QF_ZERO);
    return mc_set_qcomplex(mcomplex, result);
}

int mc_neg(mcomplex_t *mcomplex) { return mcomplex_apply_unary(mcomplex, qc_neg); }
int mc_conj(mcomplex_t *mcomplex) { return mcomplex_apply_unary(mcomplex, qc_conj); }
int mc_add(mcomplex_t *mcomplex, const mcomplex_t *other) { return mcomplex_apply_binary(mcomplex, other, qc_add); }
int mc_sub(mcomplex_t *mcomplex, const mcomplex_t *other) { return mcomplex_apply_binary(mcomplex, other, qc_sub); }
int mc_mul(mcomplex_t *mcomplex, const mcomplex_t *other) { return mcomplex_apply_binary(mcomplex, other, qc_mul); }
int mc_div(mcomplex_t *mcomplex, const mcomplex_t *other) { return mcomplex_apply_binary(mcomplex, other, qc_div); }

int mc_inv(mcomplex_t *mcomplex)
{
    if (!mcomplex)
        return -1;
    return mc_set_qcomplex(mcomplex, qc_div(QC_ONE, mc_to_qcomplex(mcomplex)));
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
int mc_sqrt(mcomplex_t *mcomplex) { return mcomplex_apply_unary(mcomplex, qc_sqrt); }
int mc_floor(mcomplex_t *mcomplex) { return mcomplex_apply_unary(mcomplex, qc_floor); }
int mc_hypot(mcomplex_t *mcomplex, const mcomplex_t *other) { return mcomplex_apply_binary(mcomplex, other, qc_hypot); }
