#include "mcomplex_internal.h"


static void mcomplex_prepare_constant(const mcomplex_t *mcomplex, mpfr_prec_t precision)
{
    if (mcomplex && mcomplex->constant_id != MCCONST_NONE)
        mcomplex_constant_ensure(mcomplex, precision);
}

static int mcomplex_apply_unary_mpc(mcomplex_t *mcomplex,
                                    int (*op)(mpc_ptr, mpc_srcptr, mpc_rnd_t))
{
    if (mcomplex_prepare_mutable(mcomplex) != 0 || !op)
        return -1;
    op(mcomplex->value, mcomplex->value, MPC_RNDNN);
    return 0;
}

static int mcomplex_apply_binary_mpc(mcomplex_t *mcomplex,
                                     const mcomplex_t *other,
                                     int (*op)(mpc_ptr, mpc_srcptr, mpc_srcptr, mpc_rnd_t))
{
    if (mcomplex_prepare_mutable(mcomplex) != 0 || !other || !op)
        return -1;
    mcomplex_prepare_constant(other, mpc_get_prec(mcomplex->value));
    op(mcomplex->value, mcomplex->value, other->value, MPC_RNDNN);
    return 0;
}

int mc_abs(mcomplex_t *mcomplex)
{
    mpfr_t mag;

    if (mcomplex_prepare_mutable(mcomplex) != 0)
        return -1;

    mpfr_init2(mag, mpc_get_prec(mcomplex->value));
    mpc_abs(mag, mcomplex->value, MPFR_RNDN);
    mpc_set_fr(mcomplex->value, mag, MPC_RNDNN);
    mpfr_clear(mag);
    return 0;
}

int mc_neg(mcomplex_t *mcomplex)
{
    return mcomplex_apply_unary_mpc(mcomplex, mpc_neg);
}

int mc_conj(mcomplex_t *mcomplex)
{
    return mcomplex_apply_unary_mpc(mcomplex, mpc_conj);
}

int mc_add(mcomplex_t *mcomplex, const mcomplex_t *other)
{
    return mcomplex_apply_binary_mpc(mcomplex, other, mpc_add);
}

int mc_sub(mcomplex_t *mcomplex, const mcomplex_t *other)
{
    return mcomplex_apply_binary_mpc(mcomplex, other, mpc_sub);
}

int mc_mul(mcomplex_t *mcomplex, const mcomplex_t *other)
{
    return mcomplex_apply_binary_mpc(mcomplex, other, mpc_mul);
}

int mc_div(mcomplex_t *mcomplex, const mcomplex_t *other)
{
    return mcomplex_apply_binary_mpc(mcomplex, other, mpc_div);
}

int mc_div_long(mcomplex_t *mcomplex, long value)
{
    mpfr_t divisor;

    if (mcomplex_prepare_mutable(mcomplex) != 0 || value == 0)
        return -1;

    mpfr_init2(divisor, mpc_get_prec(mcomplex->value));
    mpfr_set_si(divisor, value, MPFR_RNDN);
    mpc_div_fr(mcomplex->value, mcomplex->value, divisor, MPC_RNDNN);
    mpfr_clear(divisor);
    return 0;
}

int mc_inv(mcomplex_t *mcomplex)
{
    if (mcomplex_prepare_mutable(mcomplex) != 0)
        return -1;
    mpc_ui_div(mcomplex->value, 1u, mcomplex->value, MPC_RNDNN);
    return 0;
}

int mc_pow_int(mcomplex_t *mcomplex, int exponent)
{
    if (mcomplex_prepare_mutable(mcomplex) != 0)
        return -1;
    mpc_pow_si(mcomplex->value, mcomplex->value, exponent, MPC_RNDNN);
    return 0;
}

int mc_pow(mcomplex_t *mcomplex, const mcomplex_t *exponent)
{
    return mcomplex_apply_binary_mpc(mcomplex, exponent, mpc_pow);
}

int mc_ldexp(mcomplex_t *mcomplex, int exponent2)
{
    if (mcomplex_prepare_mutable(mcomplex) != 0)
        return -1;
    mpc_mul_2si(mcomplex->value, mcomplex->value, exponent2, MPC_RNDNN);
    return 0;
}

int mc_sqrt(mcomplex_t *mcomplex)
{
    return mcomplex_apply_unary_mpc(mcomplex, mpc_sqrt);
}
