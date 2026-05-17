#include <gmp.h>
#include <stdlib.h>

#include "mfloat_internal.h"
#include "mint.h"

static void mfloat_prepare_constant(const mfloat_t *mfloat, mpfr_prec_t precision)
{
    if (mfloat && mfloat->constant_id != MFCONST_NONE)
        mfloat_constant_ensure(mfloat, precision);
}

static mpfr_prec_t mfloat_binary_prec(const mfloat_t *a, const mfloat_t *b)
{
    mpfr_prec_t prec = (mpfr_prec_t)mf_get_default_precision();

    if (a && a->constant_id == MFCONST_NONE)
        prec = mpfr_get_prec(a->value);
    if (b && b->constant_id == MFCONST_NONE && mpfr_get_prec(b->value) > prec)
        prec = mpfr_get_prec(b->value);
    return prec;
}

static int mfloat_prepare_mutable(mfloat_t *mfloat)
{
    if (!mfloat || mfloat->constant_id != MFCONST_NONE)
        return -1;
    return 0;
}

static int mfloat_set_mpz_from_mint(mpz_t dst, const mint_t *value)
{
    char *text = NULL;
    int rc = -1;

    if (!dst || !value)
        return -1;

    text = mi_to_string(value);
    if (!text)
        return -1;

    if (mpz_set_str(dst, text, 10) != 0)
        goto cleanup;

    rc = 0;

cleanup:
    free(text);
    return rc;
}

static int mfloat_apply_binary_mpfr(mfloat_t *dst, const mfloat_t *rhs,
                                    int (*op)(mpfr_ptr, mpfr_srcptr, mpfr_srcptr,
                                              mpfr_rnd_t))
{
    if (mfloat_prepare_mutable(dst) != 0 || !rhs || !op)
        return -1;

    mfloat_prepare_constant(rhs, mpfr_get_prec(dst->value));
    op(dst->value, dst->value, rhs->value, MPFR_RNDN);
    return 0;
}

static int mfloat_apply_mint_mpfr(mfloat_t *dst, const mint_t *value,
                                  int subtract)
{
    mpz_t rhs_z;
    int rc = -1;

    if (mfloat_prepare_mutable(dst) != 0 || !value)
        return -1;

    mpz_init(rhs_z);
    if (mfloat_set_mpz_from_mint(rhs_z, value) != 0)
        goto cleanup;

    if (subtract)
        mpfr_sub_z(dst->value, dst->value, rhs_z, MPFR_RNDN);
    else
        mpfr_add_z(dst->value, dst->value, rhs_z, MPFR_RNDN);
    rc = 0;

cleanup:
    mpz_clear(rhs_z);
    return rc;
}

int mf_cmp(const mfloat_t *a, const mfloat_t *b)
{
    if (!a || !b)
        return 0;

    mfloat_prepare_constant(a, mfloat_binary_prec(a, b));
    mfloat_prepare_constant(b, mfloat_binary_prec(a, b));

    if (mpfr_nan_p(a->value) || mpfr_nan_p(b->value))
        return 1;
    return mpfr_cmp(a->value, b->value);
}

bool mf_eq(const mfloat_t *a, const mfloat_t *b)
{
    if (!a || !b)
        return false;
    mfloat_prepare_constant(a, mfloat_binary_prec(a, b));
    mfloat_prepare_constant(b, mfloat_binary_prec(a, b));
    if (mpfr_nan_p(a->value) || mpfr_nan_p(b->value))
        return false;
    return mpfr_equal_p(a->value, b->value) != 0;
}

bool mf_lt(const mfloat_t *a, const mfloat_t *b)
{
    if (!a || !b)
        return false;
    mfloat_prepare_constant(a, mfloat_binary_prec(a, b));
    mfloat_prepare_constant(b, mfloat_binary_prec(a, b));
    if (mpfr_nan_p(a->value) || mpfr_nan_p(b->value))
        return false;
    return mpfr_less_p(a->value, b->value) != 0;
}

bool mf_le(const mfloat_t *a, const mfloat_t *b)
{
    if (!a || !b)
        return false;
    mfloat_prepare_constant(a, mfloat_binary_prec(a, b));
    mfloat_prepare_constant(b, mfloat_binary_prec(a, b));
    if (mpfr_nan_p(a->value) || mpfr_nan_p(b->value))
        return false;
    return mpfr_lessequal_p(a->value, b->value) != 0;
}

bool mf_gt(const mfloat_t *a, const mfloat_t *b)
{
    if (!a || !b)
        return false;
    mfloat_prepare_constant(a, mfloat_binary_prec(a, b));
    mfloat_prepare_constant(b, mfloat_binary_prec(a, b));
    if (mpfr_nan_p(a->value) || mpfr_nan_p(b->value))
        return false;
    return mpfr_greater_p(a->value, b->value) != 0;
}

bool mf_ge(const mfloat_t *a, const mfloat_t *b)
{
    if (!a || !b)
        return false;
    mfloat_prepare_constant(a, mfloat_binary_prec(a, b));
    mfloat_prepare_constant(b, mfloat_binary_prec(a, b));
    if (mpfr_nan_p(a->value) || mpfr_nan_p(b->value))
        return false;
    return mpfr_greaterequal_p(a->value, b->value) != 0;
}

int mf_abs(mfloat_t *mfloat)
{
    if (mfloat_prepare_mutable(mfloat) != 0)
        return -1;
    mpfr_abs(mfloat->value, mfloat->value, MPFR_RNDN);
    return 0;
}

int mf_neg(mfloat_t *mfloat)
{
    if (mfloat_prepare_mutable(mfloat) != 0)
        return -1;
    mpfr_neg(mfloat->value, mfloat->value, MPFR_RNDN);
    return 0;
}

int mf_add(mfloat_t *mfloat, const mfloat_t *other)
{
    return mfloat_apply_binary_mpfr(mfloat, other, mpfr_add);
}

int mf_add_long(mfloat_t *mfloat, long value)
{
    if (mfloat_prepare_mutable(mfloat) != 0)
        return -1;
    mpfr_add_si(mfloat->value, mfloat->value, value, MPFR_RNDN);
    return 0;
}

int mf_add_mint(mfloat_t *mfloat, const mint_t *value)
{
    return mfloat_apply_mint_mpfr(mfloat, value, 0);
}

int mf_sub_long(mfloat_t *mfloat, long value)
{
    if (mfloat_prepare_mutable(mfloat) != 0)
        return -1;
    mpfr_sub_si(mfloat->value, mfloat->value, value, MPFR_RNDN);
    return 0;
}

int mf_sub_mint(mfloat_t *mfloat, const mint_t *value)
{
    return mfloat_apply_mint_mpfr(mfloat, value, 1);
}

int mf_sub(mfloat_t *mfloat, const mfloat_t *other)
{
    return mfloat_apply_binary_mpfr(mfloat, other, mpfr_sub);
}

int mf_mul(mfloat_t *mfloat, const mfloat_t *other)
{
    return mfloat_apply_binary_mpfr(mfloat, other, mpfr_mul);
}

int mf_mul_long(mfloat_t *mfloat, long value)
{
    if (mfloat_prepare_mutable(mfloat) != 0)
        return -1;
    mpfr_mul_si(mfloat->value, mfloat->value, value, MPFR_RNDN);
    return 0;
}

int mf_div(mfloat_t *mfloat, const mfloat_t *other)
{
    return mfloat_apply_binary_mpfr(mfloat, other, mpfr_div);
}

int mf_inv(mfloat_t *mfloat)
{
    if (mfloat_prepare_mutable(mfloat) != 0)
        return -1;
    mpfr_ui_div(mfloat->value, 1u, mfloat->value, MPFR_RNDN);
    return 0;
}

int mf_pow_int(mfloat_t *mfloat, int exponent)
{
    if (mfloat_prepare_mutable(mfloat) != 0)
        return -1;
    mpfr_pow_si(mfloat->value, mfloat->value, (long)exponent, MPFR_RNDN);
    return 0;
}

int mf_ldexp(mfloat_t *mfloat, int exponent2)
{
    if (mfloat_prepare_mutable(mfloat) != 0)
        return -1;
    if (exponent2 >= 0)
        mpfr_mul_2si(mfloat->value, mfloat->value, (long)exponent2, MPFR_RNDN);
    else
        mpfr_div_2si(mfloat->value, mfloat->value, (long)(-exponent2), MPFR_RNDN);
    return 0;
}

int mf_sqrt(mfloat_t *mfloat)
{
    if (mfloat_prepare_mutable(mfloat) != 0)
        return -1;
    if (mpfr_nan_p(mfloat->value))
        return 0;
    if (mpfr_sgn(mfloat->value) < 0)
        return -1;
    mpfr_sqrt(mfloat->value, mfloat->value, MPFR_RNDN);
    return 0;
}
