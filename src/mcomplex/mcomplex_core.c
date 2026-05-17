#include <math.h>
#include <stdlib.h>

#include "mcomplex_internal.h"
#include "mfloat/mfloat_internal.h"
#include "mrational.h"

static const double MCOMPLEX_LOG10_2 = 0.3010299956639812;
static const double MCOMPLEX_LOG2_10 = 3.3219280948873626;

static void mcomplex_prepare_constant(const mcomplex_t *mcomplex, mpfr_prec_t precision)
{
    if (mcomplex && mcomplex->constant_id != MCCONST_NONE)
        mcomplex_constant_ensure(mcomplex, precision);
}

static void mcomplex_prepare_mfloat(const mfloat_t *mfloat, mpfr_prec_t precision)
{
    if (mfloat && precision > 0)
        mfloat_constant_ensure(mfloat, precision);
}

int mcomplex_prepare_mutable(mcomplex_t *mcomplex)
{
    if (!mcomplex || mcomplex->constant_id != MCCONST_NONE)
        return -1;
    return 0;
}

static int mcomplex_alloc_views(mcomplex_t *mcomplex, mpfr_prec_t precision)
{
    if (!mcomplex)
        return -1;
    if (!mcomplex->real_view) {
        mcomplex->real_view = mf_new_prec((size_t)precision);
        if (!mcomplex->real_view)
            return -1;
    }
    if (!mcomplex->imag_view) {
        mcomplex->imag_view = mf_new_prec((size_t)precision);
        if (!mcomplex->imag_view)
            return -1;
    }
    return 0;
}

int mcomplex_sync_views(mcomplex_t *mcomplex)
{
    mpfr_prec_t precision;

    if (!mcomplex)
        return -1;
    mcomplex_prepare_constant(mcomplex, (mpfr_prec_t)mf_get_default_precision());
    precision = mpc_get_prec(mcomplex->value);
    if (mcomplex_alloc_views(mcomplex, precision) != 0)
        return -1;
    mpfr_set_prec(mcomplex->real_view->value, precision);
    mpfr_set_prec(mcomplex->imag_view->value, precision);
    mpfr_set(mcomplex->real_view->value, mpc_realref(mcomplex->value), MPFR_RNDN);
    mpfr_set(mcomplex->imag_view->value, mpc_imagref(mcomplex->value), MPFR_RNDN);
    return 0;
}

static int mcomplex_resize_value(mcomplex_t *mcomplex, size_t precision_bits)
{
    mpc_t tmp;

    if (mcomplex_prepare_mutable(mcomplex) != 0 || precision_bits == 0)
        return -1;

    mpc_init2(tmp, (mpfr_prec_t)precision_bits);
    mpc_set(tmp, mcomplex->value, MPC_RNDNN);
    mpc_clear(mcomplex->value);
    mpc_init2(mcomplex->value, (mpfr_prec_t)precision_bits);
    mpc_set(mcomplex->value, tmp, MPC_RNDNN);
    mpc_clear(tmp);
    return 0;
}

static int mcomplex_set_from_mfloats(mcomplex_t *mcomplex,
                                     const mfloat_t *real,
                                     const mfloat_t *imag)
{
    mpfr_prec_t precision;

    if (mcomplex_prepare_mutable(mcomplex) != 0 || !real || !imag)
        return -1;

    precision = mpc_get_prec(mcomplex->value);
    mcomplex_prepare_mfloat(real, precision);
    mcomplex_prepare_mfloat(imag, precision);
    mpc_set_fr_fr(mcomplex->value, real->value, imag->value, MPC_RNDNN);
    return 0;
}

mcomplex_t *mc_new(void)
{
    return mc_new_prec(mf_get_default_precision());
}

mcomplex_t *mc_new_prec(size_t precision_bits)
{
    mcomplex_t *mcomplex;
    mpfr_prec_t precision;

    precision = (mpfr_prec_t)(precision_bits > 0 ? precision_bits
                                                  : mf_get_default_precision());
    mcomplex = calloc(1u, sizeof(*mcomplex));
    if (!mcomplex)
        return NULL;

    mcomplex->constant_id = MCCONST_NONE;
    mpc_init2(mcomplex->value, precision);
    mpc_set_ui_ui(mcomplex->value, 0u, 0u, MPC_RNDNN);
    return mcomplex;
}

mcomplex_t *mc_const_prec(const mcomplex_t *constant, size_t precision_bits)
{
    mcomplex_t *copy;

    if (!constant || precision_bits == 0)
        return NULL;
    mcomplex_prepare_constant(constant, (mpfr_prec_t)precision_bits);
    copy = mc_new_prec(precision_bits);
    if (!copy)
        return NULL;
    mpc_set(copy->value, constant->value, MPC_RNDNN);
    return copy;
}

mcomplex_t *mc_const(const mcomplex_t *constant)
{
    return mc_const_prec(constant, mc_get_precision(constant));
}

mcomplex_t *mc_create(const mfloat_t *real, const mfloat_t *imag)
{
    mcomplex_t *mcomplex;
    size_t precision_bits;

    if (!real || !imag)
        return NULL;
    precision_bits = mf_get_precision(real);
    if (mf_get_precision(imag) > precision_bits)
        precision_bits = mf_get_precision(imag);

    mcomplex = mc_new_prec(precision_bits);
    if (!mcomplex || mcomplex_set_from_mfloats(mcomplex, real, imag) != 0) {
        mc_free(mcomplex);
        return NULL;
    }
    return mcomplex;
}

mcomplex_t *mc_create_long(long real)
{
    mcomplex_t *mcomplex = mc_new();
    if (!mcomplex)
        return NULL;
    mpc_set_si_si(mcomplex->value, real, 0, MPC_RNDNN);
    return mcomplex;
}

mcomplex_t *mc_create_qcomplex(qcomplex_t value)
{
    mcomplex_t *mcomplex = mc_new();
    if (!mcomplex || mc_set_qcomplex(mcomplex, value) != 0) {
        mc_free(mcomplex);
        return NULL;
    }
    return mcomplex;
}

mcomplex_t *mc_clone(const mcomplex_t *mcomplex)
{
    return mcomplex ? mc_const_prec(mcomplex, mc_get_precision(mcomplex)) : NULL;
}

void mc_free(mcomplex_t *mcomplex)
{
    if (!mcomplex || mcomplex->constant_id != MCCONST_NONE)
        return;
    mf_free(mcomplex->real_view);
    mf_free(mcomplex->imag_view);
    mpc_clear(mcomplex->value);
    free(mcomplex);
}

void mc_clear(mcomplex_t *mcomplex)
{
    if (mcomplex_prepare_mutable(mcomplex) != 0)
        return;
    mpc_set_ui_ui(mcomplex->value, 0u, 0u, MPC_RNDNN);
}

int mc_set_precision(mcomplex_t *mcomplex, size_t precision_bits)
{
    return mcomplex_resize_value(mcomplex, precision_bits);
}

size_t mc_get_precision(const mcomplex_t *mcomplex)
{
    mcomplex_prepare_constant(mcomplex, (mpfr_prec_t)mf_get_default_precision());
    return mcomplex ? (size_t)mpc_get_prec(mcomplex->value) : 0u;
}

int mc_set_precision_digits(mcomplex_t *mcomplex, size_t significant_digits)
{
    if (significant_digits == 0u)
        return -1;
    return mc_set_precision(
        mcomplex,
        (size_t)ceil((double)significant_digits * MCOMPLEX_LOG2_10));
}

size_t mc_get_precision_digits(const mcomplex_t *mcomplex)
{
    size_t precision_bits = mc_get_precision(mcomplex);
    size_t digits;

    if (precision_bits == 0u)
        return 0u;
    digits = (size_t)floor((double)precision_bits * MCOMPLEX_LOG10_2);
    return digits > 0u ? digits : 1u;
}

int mc_set(mcomplex_t *mcomplex, const mfloat_t *real, const mfloat_t *imag)
{
    return mcomplex_set_from_mfloats(mcomplex, real, imag);
}

int mc_set_qcomplex(mcomplex_t *mcomplex, qcomplex_t value)
{
    mfloat_t *real = NULL;
    mfloat_t *imag = NULL;
    int rc = -1;

    if (!mcomplex)
        return -1;

    real = mf_new_prec(mc_get_precision(mcomplex));
    imag = mf_new_prec(mc_get_precision(mcomplex));
    if (!real || !imag)
        goto cleanup;
    if (mf_set_qfloat(real, qc_real(value)) != 0 ||
        mf_set_qfloat(imag, qc_imag(value)) != 0)
        goto cleanup;
    rc = mcomplex_set_from_mfloats(mcomplex, real, imag);

cleanup:
    mf_free(real);
    mf_free(imag);
    return rc;
}

const mfloat_t *mc_real(const mcomplex_t *mcomplex)
{
    mcomplex_t *mutable_complex = (mcomplex_t *)mcomplex;

    if (!mutable_complex || mcomplex_sync_views(mutable_complex) != 0)
        return NULL;
    return mutable_complex->real_view;
}

const mfloat_t *mc_imag(const mcomplex_t *mcomplex)
{
    mcomplex_t *mutable_complex = (mcomplex_t *)mcomplex;

    if (!mutable_complex || mcomplex_sync_views(mutable_complex) != 0)
        return NULL;
    return mutable_complex->imag_view;
}

qcomplex_t mc_to_qcomplex(const mcomplex_t *mcomplex)
{
    return qc_make(mf_to_qfloat(mc_real(mcomplex)), mf_to_qfloat(mc_imag(mcomplex)));
}

bool mc_is_zero(const mcomplex_t *mcomplex)
{
    mcomplex_prepare_constant(mcomplex, (mpfr_prec_t)mf_get_default_precision());
    return mcomplex &&
           mpfr_zero_p(mpc_realref(mcomplex->value)) != 0 &&
           mpfr_zero_p(mpc_imagref(mcomplex->value)) != 0;
}

bool mc_eq(const mcomplex_t *a, const mcomplex_t *b)
{
    mcomplex_prepare_constant(a, (mpfr_prec_t)mf_get_default_precision());
    mcomplex_prepare_constant(b, (mpfr_prec_t)mf_get_default_precision());
    return a && b &&
           mpfr_equal_p(mpc_realref(a->value), mpc_realref(b->value)) != 0 &&
           mpfr_equal_p(mpc_imagref(a->value), mpc_imagref(b->value)) != 0;
}

bool mc_isnan(const mcomplex_t *mcomplex)
{
    mcomplex_prepare_constant(mcomplex, (mpfr_prec_t)mf_get_default_precision());
    return mcomplex &&
           (mpfr_nan_p(mpc_realref(mcomplex->value)) != 0 ||
            mpfr_nan_p(mpc_imagref(mcomplex->value)) != 0);
}

bool mc_isinf(const mcomplex_t *mcomplex)
{
    mcomplex_prepare_constant(mcomplex, (mpfr_prec_t)mf_get_default_precision());
    return mcomplex &&
           (mpfr_inf_p(mpc_realref(mcomplex->value)) != 0 ||
            mpfr_inf_p(mpc_imagref(mcomplex->value)) != 0);
}

bool mc_isposinf(const mcomplex_t *mcomplex)
{
    mcomplex_prepare_constant(mcomplex, (mpfr_prec_t)mf_get_default_precision());
    return mcomplex &&
           mpfr_inf_p(mpc_realref(mcomplex->value)) != 0 &&
           mpfr_sgn(mpc_realref(mcomplex->value)) > 0 &&
           mpfr_zero_p(mpc_imagref(mcomplex->value)) != 0;
}

bool mc_isneginf(const mcomplex_t *mcomplex)
{
    mcomplex_prepare_constant(mcomplex, (mpfr_prec_t)mf_get_default_precision());
    return mcomplex &&
           mpfr_inf_p(mpc_realref(mcomplex->value)) != 0 &&
           mpfr_sgn(mpc_realref(mcomplex->value)) < 0 &&
           mpfr_zero_p(mpc_imagref(mcomplex->value)) != 0;
}
