#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mcomplex_internal.h"
#include "../mfloat/mfloat_internal.h"

static struct _mcomplex_t mcomplex_zero_static = {
    .real = (mfloat_t *)MF_ZERO,
    .imag = (mfloat_t *)MF_ZERO,
    .immortal = true
};

static struct _mcomplex_t mcomplex_one_static = {
    .real = (mfloat_t *)MF_ONE,
    .imag = (mfloat_t *)MF_ZERO,
    .immortal = true
};

static struct _mcomplex_t mcomplex_half_static = {
    .real = (mfloat_t *)MF_HALF,
    .imag = (mfloat_t *)MF_ZERO,
    .immortal = true
};

static struct _mcomplex_t mcomplex_tenth_static = {
    .real = (mfloat_t *)MF_TENTH,
    .imag = (mfloat_t *)MF_ZERO,
    .immortal = true
};

static struct _mcomplex_t mcomplex_ten_static = {
    .real = (mfloat_t *)MF_TEN,
    .imag = (mfloat_t *)MF_ZERO,
    .immortal = true
};

static struct _mcomplex_t mcomplex_pi_static = {
    .real = (mfloat_t *)MF_PI,
    .imag = (mfloat_t *)MF_ZERO,
    .immortal = true
};

static struct _mcomplex_t mcomplex_e_static = {
    .real = (mfloat_t *)MF_E,
    .imag = (mfloat_t *)MF_ZERO,
    .immortal = true
};

static struct _mcomplex_t mcomplex_gamma_static = {
    .real = (mfloat_t *)MF_EULER_MASCHERONI,
    .imag = (mfloat_t *)MF_ZERO,
    .immortal = true
};

static struct _mcomplex_t mcomplex_sqrt2_static = {
    .real = (mfloat_t *)MF_SQRT2,
    .imag = (mfloat_t *)MF_ZERO,
    .immortal = true
};

static struct _mcomplex_t mcomplex_sqrt_pi_static = {
    .real = (mfloat_t *)MF_SQRT_PI,
    .imag = (mfloat_t *)MF_ZERO,
    .immortal = true
};

static struct _mcomplex_t mcomplex_nan_static = {
    .real = (mfloat_t *)MF_NAN,
    .imag = (mfloat_t *)MF_NAN,
    .immortal = true
};

static struct _mcomplex_t mcomplex_inf_static = {
    .real = (mfloat_t *)MF_INF,
    .imag = (mfloat_t *)MF_ZERO,
    .immortal = true
};

static struct _mcomplex_t mcomplex_ninf_static = {
    .real = (mfloat_t *)MF_NINF,
    .imag = (mfloat_t *)MF_ZERO,
    .immortal = true
};

static struct _mcomplex_t mcomplex_i_static = {
    .real = (mfloat_t *)MF_ZERO,
    .imag = (mfloat_t *)MF_ONE,
    .immortal = true
};

const mcomplex_t * const MC_ZERO = &mcomplex_zero_static;
const mcomplex_t * const MC_ONE = &mcomplex_one_static;
const mcomplex_t * const MC_HALF = &mcomplex_half_static;
const mcomplex_t * const MC_TENTH = &mcomplex_tenth_static;
const mcomplex_t * const MC_TEN = &mcomplex_ten_static;
const mcomplex_t * const MC_PI = &mcomplex_pi_static;
const mcomplex_t * const MC_E = &mcomplex_e_static;
const mcomplex_t * const MC_EULER_MASCHERONI = &mcomplex_gamma_static;
const mcomplex_t * const MC_SQRT2 = &mcomplex_sqrt2_static;
const mcomplex_t * const MC_SQRT_PI = &mcomplex_sqrt_pi_static;
const mcomplex_t * const MC_NAN = &mcomplex_nan_static;
const mcomplex_t * const MC_INF = &mcomplex_inf_static;
const mcomplex_t * const MC_NINF = &mcomplex_ninf_static;
const mcomplex_t * const MC_I = &mcomplex_i_static;

static int mcomplex_alloc_parts(mcomplex_t *mcomplex, size_t precision_bits)
{
    mcomplex->real = mf_new_prec(precision_bits);
    mcomplex->imag = mf_new_prec(precision_bits);
    if (!mcomplex->real || !mcomplex->imag) {
        mf_free(mcomplex->real);
        mf_free(mcomplex->imag);
        mcomplex->real = NULL;
        mcomplex->imag = NULL;
        return -1;
    }
    mcomplex->immortal = false;
    return 0;
}

int mcomplex_ensure_mutable(mcomplex_t *mcomplex)
{
    mfloat_t *real;
    mfloat_t *imag;

    if (!mcomplex)
        return -1;
    if (!mcomplex->immortal)
        return 0;

    real = mf_clone(mcomplex->real);
    imag = mf_clone(mcomplex->imag);
    if (!real || !imag) {
        mf_free(real);
        mf_free(imag);
        return -1;
    }

    mcomplex->real = real;
    mcomplex->imag = imag;
    mcomplex->immortal = false;
    return 0;
}

qcomplex_t mc_to_qcomplex(const mcomplex_t *mcomplex)
{
    return qc_make(mf_to_qfloat(mcomplex->real), mf_to_qfloat(mcomplex->imag));
}

int mc_set_qcomplex(mcomplex_t *mcomplex, qcomplex_t value)
{
    size_t precision_bits;

    if (!mcomplex)
        return -1;
    precision_bits = mc_get_precision(mcomplex);
    if (mcomplex_ensure_mutable(mcomplex) != 0)
        return -1;
    if (mf_set_precision(mcomplex->real, precision_bits) != 0 ||
        mf_set_precision(mcomplex->imag, precision_bits) != 0 ||
        mf_set_qfloat(mcomplex->real, value.re) != 0 ||
        mf_set_qfloat(mcomplex->imag, value.im) != 0)
        return -1;
    return 0;
}

mcomplex_t *mc_new(void)
{
    return mc_new_prec(mf_get_default_precision());
}

mcomplex_t *mc_new_prec(size_t precision_bits)
{
    mcomplex_t *mcomplex = calloc(1, sizeof(*mcomplex));

    if (!mcomplex)
        return NULL;
    if (mcomplex_alloc_parts(mcomplex, precision_bits) != 0) {
        free(mcomplex);
        return NULL;
    }
    return mcomplex;
}

mcomplex_t *mc_create(const mfloat_t *real, const mfloat_t *imag)
{
    mcomplex_t *mcomplex;
    size_t precision_bits;

    if (!real || !imag)
        return NULL;
    precision_bits = mf_get_precision(real) > mf_get_precision(imag)
        ? mf_get_precision(real) : mf_get_precision(imag);
    mcomplex = mc_new_prec(precision_bits);
    if (!mcomplex || mc_set(mcomplex, real, imag) != 0) {
        mc_free(mcomplex);
        return NULL;
    }
    return mcomplex;
}

mcomplex_t *mc_create_long(long real)
{
    mcomplex_t *mcomplex = mc_new();

    if (!mcomplex || mf_set_long(mcomplex->real, real) != 0) {
        mc_free(mcomplex);
        return NULL;
    }
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

mcomplex_t *mc_create_string(const char *text)
{
    mcomplex_t *mcomplex = mc_new();

    if (!mcomplex || mc_set_string(mcomplex, text) != 0) {
        mc_free(mcomplex);
        return NULL;
    }
    return mcomplex;
}

mcomplex_t *mc_clone(const mcomplex_t *mcomplex)
{
    return mcomplex ? mc_create(mcomplex->real, mcomplex->imag) : NULL;
}

void mc_free(mcomplex_t *mcomplex)
{
    if (!mcomplex)
        return;
    if (!mcomplex->immortal) {
        mf_free(mcomplex->real);
        mf_free(mcomplex->imag);
    }
    free(mcomplex);
}

void mc_clear(mcomplex_t *mcomplex)
{
    if (!mcomplex)
        return;
    if (mcomplex_ensure_mutable(mcomplex) != 0)
        return;
    mf_clear(mcomplex->real);
    mf_clear(mcomplex->imag);
}

int mc_set_precision(mcomplex_t *mcomplex, size_t precision_bits)
{
    if (!mcomplex || mcomplex_ensure_mutable(mcomplex) != 0)
        return -1;
    if (mf_set_precision(mcomplex->real, precision_bits) != 0 ||
        mf_set_precision(mcomplex->imag, precision_bits) != 0)
        return -1;
    return 0;
}

size_t mc_get_precision(const mcomplex_t *mcomplex)
{
    size_t real_prec, imag_prec;

    if (!mcomplex)
        return 0u;
    real_prec = mf_get_precision(mcomplex->real);
    imag_prec = mf_get_precision(mcomplex->imag);
    return real_prec > imag_prec ? real_prec : imag_prec;
}

int mc_set(mcomplex_t *mcomplex, const mfloat_t *real, const mfloat_t *imag)
{
    size_t precision_bits;

    if (!mcomplex || !real || !imag)
        return -1;
    precision_bits = mc_get_precision(mcomplex);
    if (mcomplex_ensure_mutable(mcomplex) != 0)
        return -1;
    if (mf_set_precision(mcomplex->real, precision_bits) != 0 ||
        mf_set_precision(mcomplex->imag, precision_bits) != 0)
        return -1;
    if ((mfloat_is_immortal(real)
            ? mfloat_set_from_immortal_internal(mcomplex->real, real, precision_bits)
            : mfloat_copy_value(mcomplex->real, real)) != 0 ||
        (mfloat_is_immortal(imag)
            ? mfloat_set_from_immortal_internal(mcomplex->imag, imag, precision_bits)
            : mfloat_copy_value(mcomplex->imag, imag)) != 0)
        return -1;
    if (!mfloat_is_immortal(real) && precision_bits < mf_get_precision(real) &&
        mfloat_round_to_precision_internal(mcomplex->real, precision_bits) != 0)
        return -1;
    if (!mfloat_is_immortal(imag) && precision_bits < mf_get_precision(imag) &&
        mfloat_round_to_precision_internal(mcomplex->imag, precision_bits) != 0)
        return -1;
    mcomplex->real->precision = precision_bits;
    mcomplex->imag->precision = precision_bits;
    return 0;
}

const mfloat_t *mc_real(const mcomplex_t *mcomplex)
{
    return mcomplex ? mcomplex->real : NULL;
}

const mfloat_t *mc_imag(const mcomplex_t *mcomplex)
{
    return mcomplex ? mcomplex->imag : NULL;
}

bool mc_is_zero(const mcomplex_t *mcomplex)
{
    return mcomplex && mf_is_zero(mcomplex->real) && mf_is_zero(mcomplex->imag);
}

bool mc_eq(const mcomplex_t *a, const mcomplex_t *b)
{
    return a && b && mf_eq(a->real, b->real) && mf_eq(a->imag, b->imag);
}

bool mc_isnan(const mcomplex_t *mcomplex)
{
    return mcomplex && qc_isnan(mc_to_qcomplex(mcomplex));
}

bool mc_isinf(const mcomplex_t *mcomplex)
{
    return mcomplex && qc_isinf(mc_to_qcomplex(mcomplex));
}

bool mc_isposinf(const mcomplex_t *mcomplex)
{
    return mcomplex && qc_isposinf(mc_to_qcomplex(mcomplex));
}

bool mc_isneginf(const mcomplex_t *mcomplex)
{
    return mcomplex && qc_isneginf(mc_to_qcomplex(mcomplex));
}
