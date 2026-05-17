#include <stdbool.h>

#include "mfloat_internal.h"

#define MFLOAT_CONST_INIT(id_) { .constant_id = (id_) }

mfloat_t MF_ZERO_VALUE = MFLOAT_CONST_INIT(MFCONST_ZERO);
mfloat_t MF_ONE_VALUE = MFLOAT_CONST_INIT(MFCONST_ONE);
mfloat_t MF_HALF_VALUE = MFLOAT_CONST_INIT(MFCONST_HALF);
mfloat_t MF_TENTH_VALUE = MFLOAT_CONST_INIT(MFCONST_TENTH);
mfloat_t MF_TEN_VALUE = MFLOAT_CONST_INIT(MFCONST_TEN);
mfloat_t MF_PI_VALUE = MFLOAT_CONST_INIT(MFCONST_PI);
mfloat_t MF_2PI_VALUE = MFLOAT_CONST_INIT(MFCONST_2PI);
mfloat_t MF_PI_2_VALUE = MFLOAT_CONST_INIT(MFCONST_PI_2);
mfloat_t MF_PI_4_VALUE = MFLOAT_CONST_INIT(MFCONST_PI_4);
mfloat_t MF_3PI_4_VALUE = MFLOAT_CONST_INIT(MFCONST_3PI_4);
mfloat_t MF_PI_6_VALUE = MFLOAT_CONST_INIT(MFCONST_PI_6);
mfloat_t MF_PI_3_VALUE = MFLOAT_CONST_INIT(MFCONST_PI_3);
mfloat_t MF_2_PI_VALUE = MFLOAT_CONST_INIT(MFCONST_2_PI);
mfloat_t MF_E_VALUE = MFLOAT_CONST_INIT(MFCONST_E);
mfloat_t MF_INV_E_VALUE = MFLOAT_CONST_INIT(MFCONST_INV_E);
mfloat_t MF_LN2_VALUE = MFLOAT_CONST_INIT(MFCONST_LN2);
mfloat_t MF_LN10_VALUE = MFLOAT_CONST_INIT(MFCONST_LN10);
mfloat_t MF_INVLN2_VALUE = MFLOAT_CONST_INIT(MFCONST_INVLN2);
mfloat_t MF_EULER_MASCHERONI_VALUE = MFLOAT_CONST_INIT(MFCONST_EULER_MASCHERONI);
mfloat_t MF_PHI_VALUE = MFLOAT_CONST_INIT(MFCONST_PHI);
mfloat_t MF_SQRT_HALF_VALUE = MFLOAT_CONST_INIT(MFCONST_SQRT_HALF);
mfloat_t MF_SQRT2_VALUE = MFLOAT_CONST_INIT(MFCONST_SQRT2);
mfloat_t MF_SQRT3_VALUE = MFLOAT_CONST_INIT(MFCONST_SQRT3);
mfloat_t MF_SQRT2_OVER_TWO_VALUE = MFLOAT_CONST_INIT(MFCONST_SQRT2_OVER_TWO);
mfloat_t MF_SQRT3_OVER_TWO_VALUE = MFLOAT_CONST_INIT(MFCONST_SQRT3_OVER_TWO);
mfloat_t MF_SQRT_2PI_VALUE = MFLOAT_CONST_INIT(MFCONST_SQRT_2PI);
mfloat_t MF_SQRT_PI_VALUE = MFLOAT_CONST_INIT(MFCONST_SQRT_PI);
mfloat_t MF_SQRT_PI_OVER_TWO_VALUE = MFLOAT_CONST_INIT(MFCONST_SQRT_PI_OVER_TWO);
mfloat_t MF_SQRT1ONPI_VALUE = MFLOAT_CONST_INIT(MFCONST_SQRT1ONPI);
mfloat_t MF_2_SQRTPI_VALUE = MFLOAT_CONST_INIT(MFCONST_2_SQRTPI);
mfloat_t MF_NEG_TWO_OVER_SQRT_PI_VALUE = MFLOAT_CONST_INIT(MFCONST_NEG_TWO_OVER_SQRT_PI);
mfloat_t MF_INV_SQRT_2PI_VALUE = MFLOAT_CONST_INIT(MFCONST_INV_SQRT_2PI);
mfloat_t MF_LOG_SQRT_2PI_VALUE = MFLOAT_CONST_INIT(MFCONST_LOG_SQRT_2PI);
mfloat_t MF_LN_2PI_VALUE = MFLOAT_CONST_INIT(MFCONST_LN_2PI);
mfloat_t MF_PI_SQUARED_VALUE = MFLOAT_CONST_INIT(MFCONST_PI_SQUARED);
mfloat_t MF_2PI_CUBED_VALUE = MFLOAT_CONST_INIT(MFCONST_2PI_CUBED);
mfloat_t MF_NAN_VALUE = MFLOAT_CONST_INIT(MFCONST_NAN);
mfloat_t MF_INF_VALUE = MFLOAT_CONST_INIT(MFCONST_INF);
mfloat_t MF_NINF_VALUE = MFLOAT_CONST_INIT(MFCONST_NINF);

const mfloat_t * const MF_ZERO = &MF_ZERO_VALUE;
const mfloat_t * const MF_ONE = &MF_ONE_VALUE;
const mfloat_t * const MF_HALF = &MF_HALF_VALUE;
const mfloat_t * const MF_TENTH = &MF_TENTH_VALUE;
const mfloat_t * const MF_TEN = &MF_TEN_VALUE;
const mfloat_t * const MF_PI = &MF_PI_VALUE;
const mfloat_t * const MF_2PI = &MF_2PI_VALUE;
const mfloat_t * const MF_PI_2 = &MF_PI_2_VALUE;
const mfloat_t * const MF_PI_4 = &MF_PI_4_VALUE;
const mfloat_t * const MF_3PI_4 = &MF_3PI_4_VALUE;
const mfloat_t * const MF_PI_6 = &MF_PI_6_VALUE;
const mfloat_t * const MF_PI_3 = &MF_PI_3_VALUE;
const mfloat_t * const MF_2_PI = &MF_2_PI_VALUE;
const mfloat_t * const MF_E = &MF_E_VALUE;
const mfloat_t * const MF_INV_E = &MF_INV_E_VALUE;
const mfloat_t * const MF_LN2 = &MF_LN2_VALUE;
const mfloat_t * const MF_LN10 = &MF_LN10_VALUE;
const mfloat_t * const MF_INVLN2 = &MF_INVLN2_VALUE;
const mfloat_t * const MF_EULER_MASCHERONI = &MF_EULER_MASCHERONI_VALUE;
const mfloat_t * const MF_PHI = &MF_PHI_VALUE;
const mfloat_t * const MF_SQRT_HALF = &MF_SQRT_HALF_VALUE;
const mfloat_t * const MF_SQRT2 = &MF_SQRT2_VALUE;
const mfloat_t * const MF_SQRT3 = &MF_SQRT3_VALUE;
const mfloat_t * const MF_SQRT2_OVER_TWO = &MF_SQRT2_OVER_TWO_VALUE;
const mfloat_t * const MF_SQRT3_OVER_TWO = &MF_SQRT3_OVER_TWO_VALUE;
const mfloat_t * const MF_SQRT_2PI = &MF_SQRT_2PI_VALUE;
const mfloat_t * const MF_SQRT_PI = &MF_SQRT_PI_VALUE;
const mfloat_t * const MF_SQRT_PI_OVER_TWO = &MF_SQRT_PI_OVER_TWO_VALUE;
const mfloat_t * const MF_SQRT1ONPI = &MF_SQRT1ONPI_VALUE;
const mfloat_t * const MF_2_SQRTPI = &MF_2_SQRTPI_VALUE;
const mfloat_t * const MF_NEG_TWO_OVER_SQRT_PI = &MF_NEG_TWO_OVER_SQRT_PI_VALUE;
const mfloat_t * const MF_INV_SQRT_2PI = &MF_INV_SQRT_2PI_VALUE;
const mfloat_t * const MF_LOG_SQRT_2PI = &MF_LOG_SQRT_2PI_VALUE;
const mfloat_t * const MF_LN_2PI = &MF_LN_2PI_VALUE;
const mfloat_t * const MF_PI_SQUARED = &MF_PI_SQUARED_VALUE;
const mfloat_t * const MF_2PI_CUBED = &MF_2PI_CUBED_VALUE;
const mfloat_t * const MF_NAN = &MF_NAN_VALUE;
const mfloat_t * const MF_INF = &MF_INF_VALUE;
const mfloat_t * const MF_NINF = &MF_NINF_VALUE;

static mpfr_prec_t mf_zero_prec;
static mpfr_prec_t mf_one_prec;
static mpfr_prec_t mf_half_prec;
static mpfr_prec_t mf_tenth_prec;
static mpfr_prec_t mf_ten_prec;
static mpfr_prec_t mf_pi_prec;
static mpfr_prec_t mf_2pi_prec;
static mpfr_prec_t mf_pi_2_prec;
static mpfr_prec_t mf_pi_4_prec;
static mpfr_prec_t mf_3pi_4_prec;
static mpfr_prec_t mf_pi_6_prec;
static mpfr_prec_t mf_pi_3_prec;
static mpfr_prec_t mf_2_pi_prec;
static mpfr_prec_t mf_e_prec;
static mpfr_prec_t mf_inv_e_prec;
static mpfr_prec_t mf_ln2_prec;
static mpfr_prec_t mf_ln10_prec;
static mpfr_prec_t mf_invln2_prec;
static mpfr_prec_t mf_euler_prec;
static mpfr_prec_t mf_phi_prec;
static mpfr_prec_t mf_sqrt_half_prec;
static mpfr_prec_t mf_sqrt2_prec;
static mpfr_prec_t mf_sqrt3_prec;
static mpfr_prec_t mf_sqrt2_over_two_prec;
static mpfr_prec_t mf_sqrt3_over_two_prec;
static mpfr_prec_t mf_sqrt_2pi_prec;
static mpfr_prec_t mf_sqrt_pi_prec;
static mpfr_prec_t mf_sqrt_pi_over_two_prec;
static mpfr_prec_t mf_sqrt1onpi_prec;
static mpfr_prec_t mf_2_sqrtpi_prec;
static mpfr_prec_t mf_neg_two_over_sqrt_pi_prec;
static mpfr_prec_t mf_inv_sqrt_2pi_prec;
static mpfr_prec_t mf_log_sqrt_2pi_prec;
static mpfr_prec_t mf_ln_2pi_prec;
static mpfr_prec_t mf_pi_squared_prec;
static mpfr_prec_t mf_2pi_cubed_prec;
static mpfr_prec_t mf_nan_prec;
static mpfr_prec_t mf_inf_prec;
static mpfr_prec_t mf_ninf_prec;
static bool mfloat_runtime_initialised;

typedef struct mfloat_const_cache_t {
    mfloat_t *value;
    mpfr_prec_t *cached_prec;
} mfloat_const_cache_t;

static void mfloat_const_prepare(mfloat_t *value, mpfr_prec_t *cached_prec,
                                 mpfr_prec_t precision)
{
    if (*cached_prec == 0)
        mpfr_init2(value->value, precision);
    else if (*cached_prec < precision)
        mpfr_set_prec(value->value, precision);
    else
        return;

    *cached_prec = precision;
}

static void mfloat_const_prepare_fixed(mfloat_t *value, mpfr_prec_t *cached_prec,
                                       mpfr_prec_t precision)
{
    if (*cached_prec != 0)
        return;

    mpfr_init2(value->value, precision);
    *cached_prec = precision;
}

typedef enum mfloat_fixed_constant_kind_t {
    MFIX_ZERO,
    MFIX_ONE,
    MFIX_HALF,
    MFIX_TEN,
    MFIX_NAN,
    MFIX_INF,
    MFIX_NINF,
    MFIX_COUNT
} mfloat_fixed_constant_kind_t;

typedef void (*mfloat_fixed_constant_setter_t)(mfloat_t *value);

static void mfloat_set_fixed_zero(mfloat_t *value)
{
    mpfr_set_zero(value->value, 0);
}

static void mfloat_set_fixed_one(mfloat_t *value)
{
    mpfr_set_ui(value->value, 1u, MPFR_RNDN);
}

static void mfloat_set_fixed_half(mfloat_t *value)
{
    mpfr_set_ui(value->value, 1u, MPFR_RNDN);
    mpfr_div_2ui(value->value, value->value, 1u, MPFR_RNDN);
}

static void mfloat_set_fixed_ten(mfloat_t *value)
{
    mpfr_set_ui(value->value, 10u, MPFR_RNDN);
}

static void mfloat_set_fixed_nan(mfloat_t *value)
{
    mpfr_set_nan(value->value);
}

static void mfloat_set_fixed_inf(mfloat_t *value)
{
    mpfr_set_inf(value->value, 1);
}

static void mfloat_set_fixed_ninf(mfloat_t *value)
{
    mpfr_set_inf(value->value, -1);
}

static const mfloat_fixed_constant_setter_t mfloat_fixed_constant_setters[MFIX_COUNT] = {
    [MFIX_ZERO] = mfloat_set_fixed_zero,
    [MFIX_ONE] = mfloat_set_fixed_one,
    [MFIX_HALF] = mfloat_set_fixed_half,
    [MFIX_TEN] = mfloat_set_fixed_ten,
    [MFIX_NAN] = mfloat_set_fixed_nan,
    [MFIX_INF] = mfloat_set_fixed_inf,
    [MFIX_NINF] = mfloat_set_fixed_ninf
};

static void mfloat_init_fixed_constant_once(mfloat_t *value,
                                            mpfr_prec_t *cached_prec,
                                            mpfr_prec_t init_prec,
                                            mfloat_fixed_constant_kind_t kind)
{
    mfloat_fixed_constant_setter_t setter;

    mfloat_const_prepare_fixed(value, cached_prec, init_prec);

    if ((size_t)kind >= MFIX_COUNT)
        return;
    setter = mfloat_fixed_constant_setters[kind];
    if (setter)
        setter(value);
}

static void mfloat_ensure_tenth(mpfr_prec_t precision)
{
    if (mf_tenth_prec >= precision)
        return;
    mfloat_const_prepare(&MF_TENTH_VALUE, &mf_tenth_prec, precision);
    mpfr_set_ui(MF_TENTH_VALUE.value, 1u, MPFR_RNDN);
    mpfr_div_ui(MF_TENTH_VALUE.value, MF_TENTH_VALUE.value, 10u, MPFR_RNDN);
}

static void mfloat_ensure_pi(mpfr_prec_t precision)
{
    if (mf_pi_prec >= precision)
        return;
    mfloat_const_prepare(&MF_PI_VALUE, &mf_pi_prec, precision);
    mpfr_const_pi(MF_PI_VALUE.value, MPFR_RNDN);
}

static void mfloat_ensure_2pi(mpfr_prec_t precision)
{
    if (mf_2pi_prec >= precision)
        return;
    mfloat_ensure_pi(precision);
    mfloat_const_prepare(&MF_2PI_VALUE, &mf_2pi_prec, precision);
    mpfr_mul_ui(MF_2PI_VALUE.value, MF_PI_VALUE.value, 2u, MPFR_RNDN);
}

static void mfloat_ensure_pi_2(mpfr_prec_t precision)
{
    if (mf_pi_2_prec >= precision)
        return;
    mfloat_ensure_pi(precision);
    mfloat_const_prepare(&MF_PI_2_VALUE, &mf_pi_2_prec, precision);
    mpfr_div_2ui(MF_PI_2_VALUE.value, MF_PI_VALUE.value, 1u, MPFR_RNDN);
}

static void mfloat_ensure_pi_4(mpfr_prec_t precision)
{
    if (mf_pi_4_prec >= precision)
        return;
    mfloat_ensure_pi(precision);
    mfloat_const_prepare(&MF_PI_4_VALUE, &mf_pi_4_prec, precision);
    mpfr_div_2ui(MF_PI_4_VALUE.value, MF_PI_VALUE.value, 2u, MPFR_RNDN);
}

static void mfloat_ensure_3pi_4(mpfr_prec_t precision)
{
    if (mf_3pi_4_prec >= precision)
        return;
    mfloat_ensure_pi(precision);
    mfloat_const_prepare(&MF_3PI_4_VALUE, &mf_3pi_4_prec, precision);
    mpfr_mul_ui(MF_3PI_4_VALUE.value, MF_PI_VALUE.value, 3u, MPFR_RNDN);
    mpfr_div_2ui(MF_3PI_4_VALUE.value, MF_3PI_4_VALUE.value, 2u, MPFR_RNDN);
}

static void mfloat_ensure_pi_6(mpfr_prec_t precision)
{
    if (mf_pi_6_prec >= precision)
        return;
    mfloat_ensure_pi(precision);
    mfloat_const_prepare(&MF_PI_6_VALUE, &mf_pi_6_prec, precision);
    mpfr_div_ui(MF_PI_6_VALUE.value, MF_PI_VALUE.value, 6u, MPFR_RNDN);
}

static void mfloat_ensure_pi_3(mpfr_prec_t precision)
{
    if (mf_pi_3_prec >= precision)
        return;
    mfloat_ensure_pi(precision);
    mfloat_const_prepare(&MF_PI_3_VALUE, &mf_pi_3_prec, precision);
    mpfr_div_ui(MF_PI_3_VALUE.value, MF_PI_VALUE.value, 3u, MPFR_RNDN);
}

static void mfloat_ensure_2_pi(mpfr_prec_t precision)
{
    if (mf_2_pi_prec >= precision)
        return;
    mfloat_ensure_pi(precision);
    mfloat_const_prepare(&MF_2_PI_VALUE, &mf_2_pi_prec, precision);
    mpfr_ui_div(MF_2_PI_VALUE.value, 2u, MF_PI_VALUE.value, MPFR_RNDN);
}

static void mfloat_ensure_e(mpfr_prec_t precision)
{
    if (mf_e_prec >= precision)
        return;
    mfloat_const_prepare(&MF_E_VALUE, &mf_e_prec, precision);
    mpfr_set_ui(MF_E_VALUE.value, 1u, MPFR_RNDN);
    mpfr_exp(MF_E_VALUE.value, MF_E_VALUE.value, MPFR_RNDN);
}

static void mfloat_ensure_inv_e(mpfr_prec_t precision)
{
    if (mf_inv_e_prec >= precision)
        return;
    mfloat_ensure_e(precision);
    mfloat_const_prepare(&MF_INV_E_VALUE, &mf_inv_e_prec, precision);
    mpfr_ui_div(MF_INV_E_VALUE.value, 1u, MF_E_VALUE.value, MPFR_RNDN);
}

static void mfloat_ensure_ln2(mpfr_prec_t precision)
{
    if (mf_ln2_prec >= precision)
        return;
    mfloat_const_prepare(&MF_LN2_VALUE, &mf_ln2_prec, precision);
    mpfr_set_ui(MF_LN2_VALUE.value, 2u, MPFR_RNDN);
    mpfr_log(MF_LN2_VALUE.value, MF_LN2_VALUE.value, MPFR_RNDN);
}

static void mfloat_ensure_ln10(mpfr_prec_t precision)
{
    if (mf_ln10_prec >= precision)
        return;
    mfloat_const_prepare(&MF_LN10_VALUE, &mf_ln10_prec, precision);
    mpfr_set_ui(MF_LN10_VALUE.value, 10u, MPFR_RNDN);
    mpfr_log(MF_LN10_VALUE.value, MF_LN10_VALUE.value, MPFR_RNDN);
}

static void mfloat_ensure_invln2(mpfr_prec_t precision)
{
    if (mf_invln2_prec >= precision)
        return;
    mfloat_ensure_ln2(precision);
    mfloat_const_prepare(&MF_INVLN2_VALUE, &mf_invln2_prec, precision);
    mpfr_ui_div(MF_INVLN2_VALUE.value, 1u, MF_LN2_VALUE.value, MPFR_RNDN);
}

static void mfloat_ensure_euler(mpfr_prec_t precision)
{
    if (mf_euler_prec >= precision)
        return;
    mfloat_const_prepare(&MF_EULER_MASCHERONI_VALUE, &mf_euler_prec, precision);
    mpfr_const_euler(MF_EULER_MASCHERONI_VALUE.value, MPFR_RNDN);
}

static void mfloat_ensure_phi(mpfr_prec_t precision)
{
    if (mf_phi_prec >= precision)
        return;
    mfloat_const_prepare(&MF_PHI_VALUE, &mf_phi_prec, precision);
    mpfr_set_ui(MF_PHI_VALUE.value, 5u, MPFR_RNDN);
    mpfr_sqrt(MF_PHI_VALUE.value, MF_PHI_VALUE.value, MPFR_RNDN);
    mpfr_add_ui(MF_PHI_VALUE.value, MF_PHI_VALUE.value, 1u, MPFR_RNDN);
    mpfr_div_2ui(MF_PHI_VALUE.value, MF_PHI_VALUE.value, 1u, MPFR_RNDN);
}

static void mfloat_ensure_sqrt_half(mpfr_prec_t precision)
{
    if (mf_sqrt_half_prec >= precision)
        return;
    mfloat_const_prepare(&MF_SQRT_HALF_VALUE, &mf_sqrt_half_prec, precision);
    mpfr_set_ui(MF_SQRT_HALF_VALUE.value, 1u, MPFR_RNDN);
    mpfr_div_2ui(MF_SQRT_HALF_VALUE.value, MF_SQRT_HALF_VALUE.value, 1u, MPFR_RNDN);
    mpfr_sqrt(MF_SQRT_HALF_VALUE.value, MF_SQRT_HALF_VALUE.value, MPFR_RNDN);
}

static void mfloat_ensure_sqrt2(mpfr_prec_t precision)
{
    if (mf_sqrt2_prec >= precision)
        return;
    mfloat_const_prepare(&MF_SQRT2_VALUE, &mf_sqrt2_prec, precision);
    mpfr_set_ui(MF_SQRT2_VALUE.value, 2u, MPFR_RNDN);
    mpfr_sqrt(MF_SQRT2_VALUE.value, MF_SQRT2_VALUE.value, MPFR_RNDN);
}

static void mfloat_ensure_sqrt3(mpfr_prec_t precision)
{
    if (mf_sqrt3_prec >= precision)
        return;
    mfloat_const_prepare(&MF_SQRT3_VALUE, &mf_sqrt3_prec, precision);
    mpfr_set_ui(MF_SQRT3_VALUE.value, 3u, MPFR_RNDN);
    mpfr_sqrt(MF_SQRT3_VALUE.value, MF_SQRT3_VALUE.value, MPFR_RNDN);
}

static void mfloat_ensure_sqrt2_over_two(mpfr_prec_t precision)
{
    if (mf_sqrt2_over_two_prec >= precision)
        return;
    mfloat_ensure_sqrt2(precision);
    mfloat_const_prepare(&MF_SQRT2_OVER_TWO_VALUE, &mf_sqrt2_over_two_prec, precision);
    mpfr_div_2ui(MF_SQRT2_OVER_TWO_VALUE.value, MF_SQRT2_VALUE.value, 1u, MPFR_RNDN);
}

static void mfloat_ensure_sqrt3_over_two(mpfr_prec_t precision)
{
    if (mf_sqrt3_over_two_prec >= precision)
        return;
    mfloat_ensure_sqrt3(precision);
    mfloat_const_prepare(&MF_SQRT3_OVER_TWO_VALUE, &mf_sqrt3_over_two_prec, precision);
    mpfr_div_2ui(MF_SQRT3_OVER_TWO_VALUE.value, MF_SQRT3_VALUE.value, 1u, MPFR_RNDN);
}

static void mfloat_ensure_sqrt_2pi(mpfr_prec_t precision)
{
    if (mf_sqrt_2pi_prec >= precision)
        return;
    mfloat_ensure_2pi(precision);
    mfloat_const_prepare(&MF_SQRT_2PI_VALUE, &mf_sqrt_2pi_prec, precision);
    mpfr_sqrt(MF_SQRT_2PI_VALUE.value, MF_2PI_VALUE.value, MPFR_RNDN);
}

static void mfloat_ensure_sqrt_pi(mpfr_prec_t precision)
{
    if (mf_sqrt_pi_prec >= precision)
        return;
    mfloat_ensure_pi(precision);
    mfloat_const_prepare(&MF_SQRT_PI_VALUE, &mf_sqrt_pi_prec, precision);
    mpfr_sqrt(MF_SQRT_PI_VALUE.value, MF_PI_VALUE.value, MPFR_RNDN);
}

static void mfloat_ensure_sqrt_pi_over_two(mpfr_prec_t precision)
{
    if (mf_sqrt_pi_over_two_prec >= precision)
        return;
    mfloat_ensure_sqrt_pi(precision);
    mfloat_const_prepare(&MF_SQRT_PI_OVER_TWO_VALUE, &mf_sqrt_pi_over_two_prec, precision);
    mpfr_div_2ui(MF_SQRT_PI_OVER_TWO_VALUE.value, MF_SQRT_PI_VALUE.value, 1u, MPFR_RNDN);
}

static void mfloat_ensure_sqrt1onpi(mpfr_prec_t precision)
{
    if (mf_sqrt1onpi_prec >= precision)
        return;
    mfloat_ensure_pi(precision);
    mfloat_const_prepare(&MF_SQRT1ONPI_VALUE, &mf_sqrt1onpi_prec, precision);
    mpfr_ui_div(MF_SQRT1ONPI_VALUE.value, 1u, MF_PI_VALUE.value, MPFR_RNDN);
    mpfr_sqrt(MF_SQRT1ONPI_VALUE.value, MF_SQRT1ONPI_VALUE.value, MPFR_RNDN);
}

static void mfloat_ensure_2_sqrtpi(mpfr_prec_t precision)
{
    if (mf_2_sqrtpi_prec >= precision)
        return;
    mfloat_ensure_sqrt_pi(precision);
    mfloat_const_prepare(&MF_2_SQRTPI_VALUE, &mf_2_sqrtpi_prec, precision);
    mpfr_ui_div(MF_2_SQRTPI_VALUE.value, 2u, MF_SQRT_PI_VALUE.value, MPFR_RNDN);
}

static void mfloat_ensure_neg_two_over_sqrt_pi(mpfr_prec_t precision)
{
    if (mf_neg_two_over_sqrt_pi_prec >= precision)
        return;
    mfloat_ensure_2_sqrtpi(precision);
    mfloat_const_prepare(&MF_NEG_TWO_OVER_SQRT_PI_VALUE,
                         &mf_neg_two_over_sqrt_pi_prec, precision);
    mpfr_neg(MF_NEG_TWO_OVER_SQRT_PI_VALUE.value, MF_2_SQRTPI_VALUE.value, MPFR_RNDN);
}

static void mfloat_ensure_inv_sqrt_2pi(mpfr_prec_t precision)
{
    if (mf_inv_sqrt_2pi_prec >= precision)
        return;
    mfloat_ensure_sqrt_2pi(precision);
    mfloat_const_prepare(&MF_INV_SQRT_2PI_VALUE, &mf_inv_sqrt_2pi_prec, precision);
    mpfr_ui_div(MF_INV_SQRT_2PI_VALUE.value, 1u, MF_SQRT_2PI_VALUE.value, MPFR_RNDN);
}

static void mfloat_ensure_log_sqrt_2pi(mpfr_prec_t precision)
{
    if (mf_log_sqrt_2pi_prec >= precision)
        return;
    mfloat_ensure_sqrt_2pi(precision);
    mfloat_const_prepare(&MF_LOG_SQRT_2PI_VALUE, &mf_log_sqrt_2pi_prec, precision);
    mpfr_log(MF_LOG_SQRT_2PI_VALUE.value, MF_SQRT_2PI_VALUE.value, MPFR_RNDN);
}

static void mfloat_ensure_ln_2pi(mpfr_prec_t precision)
{
    if (mf_ln_2pi_prec >= precision)
        return;
    mfloat_ensure_2pi(precision);
    mfloat_const_prepare(&MF_LN_2PI_VALUE, &mf_ln_2pi_prec, precision);
    mpfr_log(MF_LN_2PI_VALUE.value, MF_2PI_VALUE.value, MPFR_RNDN);
}

static void mfloat_ensure_pi_squared(mpfr_prec_t precision)
{
    if (mf_pi_squared_prec >= precision)
        return;
    mfloat_ensure_pi(precision);
    mfloat_const_prepare(&MF_PI_SQUARED_VALUE, &mf_pi_squared_prec, precision);
    mpfr_mul(MF_PI_SQUARED_VALUE.value, MF_PI_VALUE.value, MF_PI_VALUE.value, MPFR_RNDN);
}

static void mfloat_ensure_2pi_cubed(mpfr_prec_t precision)
{
    if (mf_2pi_cubed_prec >= precision)
        return;
    mfloat_ensure_2pi(precision);
    mfloat_const_prepare(&MF_2PI_CUBED_VALUE, &mf_2pi_cubed_prec, precision);
    mpfr_mul(MF_2PI_CUBED_VALUE.value, MF_2PI_VALUE.value, MF_2PI_VALUE.value, MPFR_RNDN);
    mpfr_mul(MF_2PI_CUBED_VALUE.value, MF_2PI_CUBED_VALUE.value, MF_2PI_VALUE.value,
             MPFR_RNDN);
}

static void mfloat_ensure_zero_fixed(mpfr_prec_t precision)
{
    (void)precision;
    mfloat_init_fixed_constant_once(&MF_ZERO_VALUE, &mf_zero_prec, 1, MFIX_ZERO);
}

static void mfloat_ensure_one_fixed(mpfr_prec_t precision)
{
    (void)precision;
    mfloat_init_fixed_constant_once(&MF_ONE_VALUE, &mf_one_prec, 1, MFIX_ONE);
}

static void mfloat_ensure_half_fixed(mpfr_prec_t precision)
{
    (void)precision;
    mfloat_init_fixed_constant_once(&MF_HALF_VALUE, &mf_half_prec, 1, MFIX_HALF);
}

static void mfloat_ensure_ten_fixed(mpfr_prec_t precision)
{
    (void)precision;
    mfloat_init_fixed_constant_once(&MF_TEN_VALUE, &mf_ten_prec, 4, MFIX_TEN);
}

static void mfloat_ensure_nan_fixed(mpfr_prec_t precision)
{
    (void)precision;
    mfloat_init_fixed_constant_once(&MF_NAN_VALUE, &mf_nan_prec, 1, MFIX_NAN);
}

static void mfloat_ensure_inf_fixed(mpfr_prec_t precision)
{
    (void)precision;
    mfloat_init_fixed_constant_once(&MF_INF_VALUE, &mf_inf_prec, 1, MFIX_INF);
}

static void mfloat_ensure_ninf_fixed(mpfr_prec_t precision)
{
    (void)precision;
    mfloat_init_fixed_constant_once(&MF_NINF_VALUE, &mf_ninf_prec, 1, MFIX_NINF);
}

typedef void (*mfloat_const_ensure_fn_t)(mpfr_prec_t precision);

static const mfloat_const_ensure_fn_t mfloat_const_dispatch[MFCONST_COUNT] = {
    [MFCONST_ZERO] = mfloat_ensure_zero_fixed,
    [MFCONST_ONE] = mfloat_ensure_one_fixed,
    [MFCONST_HALF] = mfloat_ensure_half_fixed,
    [MFCONST_TENTH] = mfloat_ensure_tenth,
    [MFCONST_TEN] = mfloat_ensure_ten_fixed,
    [MFCONST_PI] = mfloat_ensure_pi,
    [MFCONST_2PI] = mfloat_ensure_2pi,
    [MFCONST_PI_2] = mfloat_ensure_pi_2,
    [MFCONST_PI_4] = mfloat_ensure_pi_4,
    [MFCONST_3PI_4] = mfloat_ensure_3pi_4,
    [MFCONST_PI_6] = mfloat_ensure_pi_6,
    [MFCONST_PI_3] = mfloat_ensure_pi_3,
    [MFCONST_2_PI] = mfloat_ensure_2_pi,
    [MFCONST_E] = mfloat_ensure_e,
    [MFCONST_INV_E] = mfloat_ensure_inv_e,
    [MFCONST_LN2] = mfloat_ensure_ln2,
    [MFCONST_LN10] = mfloat_ensure_ln10,
    [MFCONST_INVLN2] = mfloat_ensure_invln2,
    [MFCONST_EULER_MASCHERONI] = mfloat_ensure_euler,
    [MFCONST_PHI] = mfloat_ensure_phi,
    [MFCONST_SQRT_HALF] = mfloat_ensure_sqrt_half,
    [MFCONST_SQRT2] = mfloat_ensure_sqrt2,
    [MFCONST_SQRT3] = mfloat_ensure_sqrt3,
    [MFCONST_SQRT2_OVER_TWO] = mfloat_ensure_sqrt2_over_two,
    [MFCONST_SQRT3_OVER_TWO] = mfloat_ensure_sqrt3_over_two,
    [MFCONST_SQRT_2PI] = mfloat_ensure_sqrt_2pi,
    [MFCONST_SQRT_PI] = mfloat_ensure_sqrt_pi,
    [MFCONST_SQRT_PI_OVER_TWO] = mfloat_ensure_sqrt_pi_over_two,
    [MFCONST_SQRT1ONPI] = mfloat_ensure_sqrt1onpi,
    [MFCONST_2_SQRTPI] = mfloat_ensure_2_sqrtpi,
    [MFCONST_NEG_TWO_OVER_SQRT_PI] = mfloat_ensure_neg_two_over_sqrt_pi,
    [MFCONST_INV_SQRT_2PI] = mfloat_ensure_inv_sqrt_2pi,
    [MFCONST_LOG_SQRT_2PI] = mfloat_ensure_log_sqrt_2pi,
    [MFCONST_LN_2PI] = mfloat_ensure_ln_2pi,
    [MFCONST_PI_SQUARED] = mfloat_ensure_pi_squared,
    [MFCONST_2PI_CUBED] = mfloat_ensure_2pi_cubed,
    [MFCONST_NAN] = mfloat_ensure_nan_fixed,
    [MFCONST_INF] = mfloat_ensure_inf_fixed,
    [MFCONST_NINF] = mfloat_ensure_ninf_fixed
};

static const mfloat_const_cache_t mfloat_const_cache[MFCONST_COUNT] = {
    [MFCONST_ZERO] = { &MF_ZERO_VALUE, &mf_zero_prec },
    [MFCONST_ONE] = { &MF_ONE_VALUE, &mf_one_prec },
    [MFCONST_HALF] = { &MF_HALF_VALUE, &mf_half_prec },
    [MFCONST_TENTH] = { &MF_TENTH_VALUE, &mf_tenth_prec },
    [MFCONST_TEN] = { &MF_TEN_VALUE, &mf_ten_prec },
    [MFCONST_PI] = { &MF_PI_VALUE, &mf_pi_prec },
    [MFCONST_2PI] = { &MF_2PI_VALUE, &mf_2pi_prec },
    [MFCONST_PI_2] = { &MF_PI_2_VALUE, &mf_pi_2_prec },
    [MFCONST_PI_4] = { &MF_PI_4_VALUE, &mf_pi_4_prec },
    [MFCONST_3PI_4] = { &MF_3PI_4_VALUE, &mf_3pi_4_prec },
    [MFCONST_PI_6] = { &MF_PI_6_VALUE, &mf_pi_6_prec },
    [MFCONST_PI_3] = { &MF_PI_3_VALUE, &mf_pi_3_prec },
    [MFCONST_2_PI] = { &MF_2_PI_VALUE, &mf_2_pi_prec },
    [MFCONST_E] = { &MF_E_VALUE, &mf_e_prec },
    [MFCONST_INV_E] = { &MF_INV_E_VALUE, &mf_inv_e_prec },
    [MFCONST_LN2] = { &MF_LN2_VALUE, &mf_ln2_prec },
    [MFCONST_LN10] = { &MF_LN10_VALUE, &mf_ln10_prec },
    [MFCONST_INVLN2] = { &MF_INVLN2_VALUE, &mf_invln2_prec },
    [MFCONST_EULER_MASCHERONI] = { &MF_EULER_MASCHERONI_VALUE, &mf_euler_prec },
    [MFCONST_PHI] = { &MF_PHI_VALUE, &mf_phi_prec },
    [MFCONST_SQRT_HALF] = { &MF_SQRT_HALF_VALUE, &mf_sqrt_half_prec },
    [MFCONST_SQRT2] = { &MF_SQRT2_VALUE, &mf_sqrt2_prec },
    [MFCONST_SQRT3] = { &MF_SQRT3_VALUE, &mf_sqrt3_prec },
    [MFCONST_SQRT2_OVER_TWO] = { &MF_SQRT2_OVER_TWO_VALUE, &mf_sqrt2_over_two_prec },
    [MFCONST_SQRT3_OVER_TWO] = { &MF_SQRT3_OVER_TWO_VALUE, &mf_sqrt3_over_two_prec },
    [MFCONST_SQRT_2PI] = { &MF_SQRT_2PI_VALUE, &mf_sqrt_2pi_prec },
    [MFCONST_SQRT_PI] = { &MF_SQRT_PI_VALUE, &mf_sqrt_pi_prec },
    [MFCONST_SQRT_PI_OVER_TWO] = { &MF_SQRT_PI_OVER_TWO_VALUE, &mf_sqrt_pi_over_two_prec },
    [MFCONST_SQRT1ONPI] = { &MF_SQRT1ONPI_VALUE, &mf_sqrt1onpi_prec },
    [MFCONST_2_SQRTPI] = { &MF_2_SQRTPI_VALUE, &mf_2_sqrtpi_prec },
    [MFCONST_NEG_TWO_OVER_SQRT_PI] = { &MF_NEG_TWO_OVER_SQRT_PI_VALUE, &mf_neg_two_over_sqrt_pi_prec },
    [MFCONST_INV_SQRT_2PI] = { &MF_INV_SQRT_2PI_VALUE, &mf_inv_sqrt_2pi_prec },
    [MFCONST_LOG_SQRT_2PI] = { &MF_LOG_SQRT_2PI_VALUE, &mf_log_sqrt_2pi_prec },
    [MFCONST_LN_2PI] = { &MF_LN_2PI_VALUE, &mf_ln_2pi_prec },
    [MFCONST_PI_SQUARED] = { &MF_PI_SQUARED_VALUE, &mf_pi_squared_prec },
    [MFCONST_2PI_CUBED] = { &MF_2PI_CUBED_VALUE, &mf_2pi_cubed_prec },
    [MFCONST_NAN] = { &MF_NAN_VALUE, &mf_nan_prec },
    [MFCONST_INF] = { &MF_INF_VALUE, &mf_inf_prec },
    [MFCONST_NINF] = { &MF_NINF_VALUE, &mf_ninf_prec }
};

static void mfloat_ensure_runtime_init(void)
{
    if (mfloat_runtime_initialised)
        return;
    mfloat_runtime_initialised = true;
}

static void __attribute__((destructor)) mfloat_constants_shutdown(void)
{
    mfloat_constant_id_t id;

    if (!mfloat_runtime_initialised)
        return;

    for (id = MFCONST_ZERO; id < MFCONST_COUNT; ++id) {
        if (mfloat_const_cache[id].cached_prec &&
            *mfloat_const_cache[id].cached_prec != 0) {
            mpfr_clear(mfloat_const_cache[id].value->value);
            *mfloat_const_cache[id].cached_prec = 0;
        }
    }

    mpfr_free_cache();
    mfloat_runtime_initialised = false;
}

void mfloat_constant_ensure(const mfloat_t *constant, mpfr_prec_t precision)
{
    mfloat_constant_id_t id;

    if (!constant || constant->constant_id == MFCONST_NONE)
        return;
    mfloat_ensure_runtime_init();
    if (precision == 0)
        precision = (mpfr_prec_t)mf_get_default_precision();
    id = constant->constant_id;
    if (id <= MFCONST_NONE || id >= MFCONST_COUNT)
        return;
    if (!mfloat_const_dispatch[id])
        return;
    mfloat_const_dispatch[id](precision);
}

void mfloat_constants_ensure_precision(mpfr_prec_t precision)
{
    mfloat_constant_id_t id;

    mfloat_ensure_runtime_init();
    if (precision == 0)
        precision = (mpfr_prec_t)mf_get_default_precision();

    for (id = MFCONST_ZERO; id < MFCONST_COUNT; ++id) {
        if (mfloat_const_dispatch[id])
            mfloat_const_dispatch[id](precision);
    }
}

void mfloat_constants_ensure_init(void)
{
    mfloat_constants_ensure_precision((mpfr_prec_t)mf_get_default_precision());
}
