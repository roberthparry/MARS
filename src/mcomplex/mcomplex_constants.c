#include "mcomplex_internal.h"

#include "internal/mfloat_internal.h"

/* Canonical immortal wrappers around mfloat constants. */
static struct _mcomplex_t mcomplex_zero_static = {
    .real = (mfloat_t *)&MF_ZERO_VALUE,
    .imag = (mfloat_t *)&MF_ZERO_VALUE,
    .immortal = true
};

static struct _mcomplex_t mcomplex_one_static = {
    .real = (mfloat_t *)&MF_ONE_VALUE,
    .imag = (mfloat_t *)&MF_ZERO_VALUE,
    .immortal = true
};

static struct _mcomplex_t mcomplex_half_static = {
    .real = (mfloat_t *)&MF_HALF_VALUE,
    .imag = (mfloat_t *)&MF_ZERO_VALUE,
    .immortal = true
};

static struct _mcomplex_t mcomplex_tenth_static = {
    .real = (mfloat_t *)&MF_TENTH_VALUE,
    .imag = (mfloat_t *)&MF_ZERO_VALUE,
    .immortal = true
};

static struct _mcomplex_t mcomplex_ten_static = {
    .real = (mfloat_t *)&MF_TEN_VALUE,
    .imag = (mfloat_t *)&MF_ZERO_VALUE,
    .immortal = true
};

static struct _mcomplex_t mcomplex_pi_static = {
    .real = (mfloat_t *)&MF_PI_VALUE,
    .imag = (mfloat_t *)&MF_ZERO_VALUE,
    .immortal = true
};

static struct _mcomplex_t mcomplex_e_static = {
    .real = (mfloat_t *)&MF_E_VALUE,
    .imag = (mfloat_t *)&MF_ZERO_VALUE,
    .immortal = true
};

static struct _mcomplex_t mcomplex_gamma_static = {
    .real = (mfloat_t *)&MF_EULER_MASCHERONI_VALUE,
    .imag = (mfloat_t *)&MF_ZERO_VALUE,
    .immortal = true
};

static struct _mcomplex_t mcomplex_sqrt2_static = {
    .real = (mfloat_t *)&MF_SQRT2_VALUE,
    .imag = (mfloat_t *)&MF_ZERO_VALUE,
    .immortal = true
};

static struct _mcomplex_t mcomplex_sqrt_pi_static = {
    .real = (mfloat_t *)&MF_SQRT_PI_VALUE,
    .imag = (mfloat_t *)&MF_ZERO_VALUE,
    .immortal = true
};

static struct _mcomplex_t mcomplex_nan_static = {
    .real = (mfloat_t *)&MF_NAN_VALUE,
    .imag = (mfloat_t *)&MF_NAN_VALUE,
    .immortal = true
};

static struct _mcomplex_t mcomplex_inf_static = {
    .real = (mfloat_t *)&MF_INF_VALUE,
    .imag = (mfloat_t *)&MF_ZERO_VALUE,
    .immortal = true
};

static struct _mcomplex_t mcomplex_ninf_static = {
    .real = (mfloat_t *)&MF_NINF_VALUE,
    .imag = (mfloat_t *)&MF_ZERO_VALUE,
    .immortal = true
};

/* Imaginary unit has no dedicated mfloat-backed wrapper object. */
const mcomplex_t MC_I_VALUE = {
    .real = (mfloat_t *)&MF_ZERO_VALUE,
    .imag = (mfloat_t *)&MF_ONE_VALUE,
    .immortal = true
};

/* Exported canonical handles. */
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
const mcomplex_t * const MC_I = &MC_I_VALUE;
