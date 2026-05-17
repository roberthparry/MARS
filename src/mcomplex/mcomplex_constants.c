#include <stdbool.h>

#include "mcomplex_internal.h"

#define MCOMPLEX_CONST_INIT(id_) { .constant_id = (id_) }

mcomplex_t MC_ZERO_VALUE = MCOMPLEX_CONST_INIT(MCCONST_ZERO);
mcomplex_t MC_ONE_VALUE = MCOMPLEX_CONST_INIT(MCCONST_ONE);
mcomplex_t MC_HALF_VALUE = MCOMPLEX_CONST_INIT(MCCONST_HALF);
mcomplex_t MC_TENTH_VALUE = MCOMPLEX_CONST_INIT(MCCONST_TENTH);
mcomplex_t MC_TEN_VALUE = MCOMPLEX_CONST_INIT(MCCONST_TEN);
mcomplex_t MC_PI_VALUE = MCOMPLEX_CONST_INIT(MCCONST_PI);
mcomplex_t MC_E_VALUE = MCOMPLEX_CONST_INIT(MCCONST_E);
mcomplex_t MC_LN10_VALUE = MCOMPLEX_CONST_INIT(MCCONST_LN10);
mcomplex_t MC_EULER_MASCHERONI_VALUE = MCOMPLEX_CONST_INIT(MCCONST_EULER_MASCHERONI);
mcomplex_t MC_SQRT2_VALUE = MCOMPLEX_CONST_INIT(MCCONST_SQRT2);
mcomplex_t MC_SQRT_PI_VALUE = MCOMPLEX_CONST_INIT(MCCONST_SQRT_PI);
mcomplex_t MC_NAN_VALUE = MCOMPLEX_CONST_INIT(MCCONST_NAN);
mcomplex_t MC_INF_VALUE = MCOMPLEX_CONST_INIT(MCCONST_INF);
mcomplex_t MC_NINF_VALUE = MCOMPLEX_CONST_INIT(MCCONST_NINF);
mcomplex_t MC_I_VALUE = MCOMPLEX_CONST_INIT(MCCONST_I);

const mcomplex_t * const MC_ZERO = &MC_ZERO_VALUE;
const mcomplex_t * const MC_ONE = &MC_ONE_VALUE;
const mcomplex_t * const MC_HALF = &MC_HALF_VALUE;
const mcomplex_t * const MC_TENTH = &MC_TENTH_VALUE;
const mcomplex_t * const MC_TEN = &MC_TEN_VALUE;
const mcomplex_t * const MC_PI = &MC_PI_VALUE;
const mcomplex_t * const MC_E = &MC_E_VALUE;
const mcomplex_t * const MC_LN10 = &MC_LN10_VALUE;
const mcomplex_t * const MC_EULER_MASCHERONI = &MC_EULER_MASCHERONI_VALUE;
const mcomplex_t * const MC_SQRT2 = &MC_SQRT2_VALUE;
const mcomplex_t * const MC_SQRT_PI = &MC_SQRT_PI_VALUE;
const mcomplex_t * const MC_NAN = &MC_NAN_VALUE;
const mcomplex_t * const MC_INF = &MC_INF_VALUE;
const mcomplex_t * const MC_NINF = &MC_NINF_VALUE;
const mcomplex_t * const MC_I = &MC_I_VALUE;

static mpfr_prec_t mc_zero_prec;
static mpfr_prec_t mc_one_prec;
static mpfr_prec_t mc_half_prec;
static mpfr_prec_t mc_tenth_prec;
static mpfr_prec_t mc_ten_prec;
static mpfr_prec_t mc_pi_prec;
static mpfr_prec_t mc_e_prec;
static mpfr_prec_t mc_ln10_prec;
static mpfr_prec_t mc_euler_prec;
static mpfr_prec_t mc_sqrt2_prec;
static mpfr_prec_t mc_sqrt_pi_prec;
static mpfr_prec_t mc_nan_prec;
static mpfr_prec_t mc_inf_prec;
static mpfr_prec_t mc_ninf_prec;
static mpfr_prec_t mc_i_prec;
static bool mcomplex_runtime_initialised;

typedef enum mcomplex_fixed_kind_t {
    MCFIX_ZERO,
    MCFIX_ONE,
    MCFIX_HALF,
    MCFIX_TEN,
    MCFIX_NAN,
    MCFIX_INF,
    MCFIX_NINF,
    MCFIX_I
} mcomplex_fixed_kind_t;

typedef struct mcomplex_const_cache_t {
    mcomplex_t *value;
    mpfr_prec_t *cached_prec;
} mcomplex_const_cache_t;

static void mcomplex_const_prepare(mcomplex_t *value, mpfr_prec_t *cached_prec,
                                   mpfr_prec_t precision)
{
    if (*cached_prec == 0)
        mpc_init2(value->value, precision);
    else if (*cached_prec < precision)
        mpc_set_prec(value->value, precision);
    else
        return;

    *cached_prec = precision;
}

static void mcomplex_const_prepare_fixed(mcomplex_t *value, mpfr_prec_t *cached_prec,
                                         mpfr_prec_t precision)
{
    if (*cached_prec != 0)
        return;

    mpc_init2(value->value, precision);
    *cached_prec = precision;
}

static void mcomplex_init_fixed_constant_once(mcomplex_t *value,
                                              mpfr_prec_t *cached_prec,
                                              mpfr_prec_t init_prec,
                                              mcomplex_fixed_kind_t kind)
{
    mcomplex_const_prepare_fixed(value, cached_prec, init_prec);

    switch (kind) {
        case MCFIX_ZERO:
            mpc_set_ui_ui(value->value, 0u, 0u, MPC_RNDNN);
            break;
        case MCFIX_ONE:
            mpc_set_ui_ui(value->value, 1u, 0u, MPC_RNDNN);
            break;
        case MCFIX_HALF:
            mpc_set_ui_ui(value->value, 1u, 0u, MPC_RNDNN);
            mpc_div_2ui(value->value, value->value, 1u, MPC_RNDNN);
            break;
        case MCFIX_TEN:
            mpc_set_ui_ui(value->value, 10u, 0u, MPC_RNDNN);
            break;
        case MCFIX_NAN:
            mpc_set_nan(value->value);
            break;
        case MCFIX_INF:
            mpfr_set_inf(mpc_realref(value->value), 1);
            mpfr_set_zero(mpc_imagref(value->value), 0);
            break;
        case MCFIX_NINF:
            mpfr_set_inf(mpc_realref(value->value), -1);
            mpfr_set_zero(mpc_imagref(value->value), 0);
            break;
        case MCFIX_I:
            mpc_set_ui_ui(value->value, 0u, 1u, MPC_RNDNN);
            break;
    }
}

static void mcomplex_ensure_tenth(mpfr_prec_t precision)
{
    if (mc_tenth_prec >= precision)
        return;
    mcomplex_const_prepare(&MC_TENTH_VALUE, &mc_tenth_prec, precision);
    mpc_set_ui_ui(MC_TENTH_VALUE.value, 1u, 0u, MPC_RNDNN);
    mpc_div_ui(MC_TENTH_VALUE.value, MC_TENTH_VALUE.value, 10u, MPC_RNDNN);
}

static void mcomplex_ensure_pi(mpfr_prec_t precision)
{
    if (mc_pi_prec >= precision)
        return;
    mcomplex_const_prepare(&MC_PI_VALUE, &mc_pi_prec, precision);
    mpfr_const_pi(mpc_realref(MC_PI_VALUE.value), MPFR_RNDN);
    mpfr_set_zero(mpc_imagref(MC_PI_VALUE.value), 0);
}

static void mcomplex_ensure_e(mpfr_prec_t precision)
{
    if (mc_e_prec >= precision)
        return;
    mcomplex_const_prepare(&MC_E_VALUE, &mc_e_prec, precision);
    mpfr_set_ui(mpc_realref(MC_E_VALUE.value), 1u, MPFR_RNDN);
    mpfr_exp(mpc_realref(MC_E_VALUE.value), mpc_realref(MC_E_VALUE.value), MPFR_RNDN);
    mpfr_set_zero(mpc_imagref(MC_E_VALUE.value), 0);
}

static void mcomplex_ensure_ln10(mpfr_prec_t precision)
{
    if (mc_ln10_prec >= precision)
        return;
    mcomplex_const_prepare(&MC_LN10_VALUE, &mc_ln10_prec, precision);
    mpfr_set_ui(mpc_realref(MC_LN10_VALUE.value), 10u, MPFR_RNDN);
    mpfr_log(mpc_realref(MC_LN10_VALUE.value), mpc_realref(MC_LN10_VALUE.value), MPFR_RNDN);
    mpfr_set_zero(mpc_imagref(MC_LN10_VALUE.value), 0);
}

static void mcomplex_ensure_euler(mpfr_prec_t precision)
{
    if (mc_euler_prec >= precision)
        return;
    mcomplex_const_prepare(&MC_EULER_MASCHERONI_VALUE, &mc_euler_prec, precision);
    mpfr_const_euler(mpc_realref(MC_EULER_MASCHERONI_VALUE.value), MPFR_RNDN);
    mpfr_set_zero(mpc_imagref(MC_EULER_MASCHERONI_VALUE.value), 0);
}

static void mcomplex_ensure_sqrt2(mpfr_prec_t precision)
{
    if (mc_sqrt2_prec >= precision)
        return;
    mcomplex_const_prepare(&MC_SQRT2_VALUE, &mc_sqrt2_prec, precision);
    mpfr_set_ui(mpc_realref(MC_SQRT2_VALUE.value), 2u, MPFR_RNDN);
    mpfr_sqrt(mpc_realref(MC_SQRT2_VALUE.value), mpc_realref(MC_SQRT2_VALUE.value), MPFR_RNDN);
    mpfr_set_zero(mpc_imagref(MC_SQRT2_VALUE.value), 0);
}

static void mcomplex_ensure_sqrt_pi(mpfr_prec_t precision)
{
    if (mc_sqrt_pi_prec >= precision)
        return;
    mcomplex_ensure_pi(precision);
    mcomplex_const_prepare(&MC_SQRT_PI_VALUE, &mc_sqrt_pi_prec, precision);
    mpfr_sqrt(mpc_realref(MC_SQRT_PI_VALUE.value), mpc_realref(MC_PI_VALUE.value), MPFR_RNDN);
    mpfr_set_zero(mpc_imagref(MC_SQRT_PI_VALUE.value), 0);
}

static void mcomplex_ensure_zero_fixed(mpfr_prec_t precision)
{
    (void)precision;
    mcomplex_init_fixed_constant_once(&MC_ZERO_VALUE, &mc_zero_prec, 1, MCFIX_ZERO);
}

static void mcomplex_ensure_one_fixed(mpfr_prec_t precision)
{
    (void)precision;
    mcomplex_init_fixed_constant_once(&MC_ONE_VALUE, &mc_one_prec, 1, MCFIX_ONE);
}

static void mcomplex_ensure_half_fixed(mpfr_prec_t precision)
{
    (void)precision;
    mcomplex_init_fixed_constant_once(&MC_HALF_VALUE, &mc_half_prec, 1, MCFIX_HALF);
}

static void mcomplex_ensure_ten_fixed(mpfr_prec_t precision)
{
    (void)precision;
    mcomplex_init_fixed_constant_once(&MC_TEN_VALUE, &mc_ten_prec, 4, MCFIX_TEN);
}

static void mcomplex_ensure_nan_fixed(mpfr_prec_t precision)
{
    (void)precision;
    mcomplex_init_fixed_constant_once(&MC_NAN_VALUE, &mc_nan_prec, 1, MCFIX_NAN);
}

static void mcomplex_ensure_inf_fixed(mpfr_prec_t precision)
{
    (void)precision;
    mcomplex_init_fixed_constant_once(&MC_INF_VALUE, &mc_inf_prec, 1, MCFIX_INF);
}

static void mcomplex_ensure_ninf_fixed(mpfr_prec_t precision)
{
    (void)precision;
    mcomplex_init_fixed_constant_once(&MC_NINF_VALUE, &mc_ninf_prec, 1, MCFIX_NINF);
}

static void mcomplex_ensure_i_fixed(mpfr_prec_t precision)
{
    (void)precision;
    mcomplex_init_fixed_constant_once(&MC_I_VALUE, &mc_i_prec, 1, MCFIX_I);
}

typedef void (*mcomplex_const_ensure_fn_t)(mpfr_prec_t precision);

static const mcomplex_const_ensure_fn_t mcomplex_const_dispatch[MCCONST_COUNT] = {
    [MCCONST_ZERO] = mcomplex_ensure_zero_fixed,
    [MCCONST_ONE] = mcomplex_ensure_one_fixed,
    [MCCONST_HALF] = mcomplex_ensure_half_fixed,
    [MCCONST_TENTH] = mcomplex_ensure_tenth,
    [MCCONST_TEN] = mcomplex_ensure_ten_fixed,
    [MCCONST_PI] = mcomplex_ensure_pi,
    [MCCONST_E] = mcomplex_ensure_e,
    [MCCONST_LN10] = mcomplex_ensure_ln10,
    [MCCONST_EULER_MASCHERONI] = mcomplex_ensure_euler,
    [MCCONST_SQRT2] = mcomplex_ensure_sqrt2,
    [MCCONST_SQRT_PI] = mcomplex_ensure_sqrt_pi,
    [MCCONST_NAN] = mcomplex_ensure_nan_fixed,
    [MCCONST_INF] = mcomplex_ensure_inf_fixed,
    [MCCONST_NINF] = mcomplex_ensure_ninf_fixed,
    [MCCONST_I] = mcomplex_ensure_i_fixed
};

static const mcomplex_const_cache_t mcomplex_const_cache[MCCONST_COUNT] = {
    [MCCONST_ZERO] = { &MC_ZERO_VALUE, &mc_zero_prec },
    [MCCONST_ONE] = { &MC_ONE_VALUE, &mc_one_prec },
    [MCCONST_HALF] = { &MC_HALF_VALUE, &mc_half_prec },
    [MCCONST_TENTH] = { &MC_TENTH_VALUE, &mc_tenth_prec },
    [MCCONST_TEN] = { &MC_TEN_VALUE, &mc_ten_prec },
    [MCCONST_PI] = { &MC_PI_VALUE, &mc_pi_prec },
    [MCCONST_E] = { &MC_E_VALUE, &mc_e_prec },
    [MCCONST_LN10] = { &MC_LN10_VALUE, &mc_ln10_prec },
    [MCCONST_EULER_MASCHERONI] = { &MC_EULER_MASCHERONI_VALUE, &mc_euler_prec },
    [MCCONST_SQRT2] = { &MC_SQRT2_VALUE, &mc_sqrt2_prec },
    [MCCONST_SQRT_PI] = { &MC_SQRT_PI_VALUE, &mc_sqrt_pi_prec },
    [MCCONST_NAN] = { &MC_NAN_VALUE, &mc_nan_prec },
    [MCCONST_INF] = { &MC_INF_VALUE, &mc_inf_prec },
    [MCCONST_NINF] = { &MC_NINF_VALUE, &mc_ninf_prec },
    [MCCONST_I] = { &MC_I_VALUE, &mc_i_prec }
};

static void mcomplex_ensure_runtime_init(void)
{
    if (mcomplex_runtime_initialised)
        return;
    mcomplex_runtime_initialised = true;
}

static void __attribute__((destructor)) mcomplex_constants_shutdown(void)
{
    mcomplex_constant_id_t id;

    if (!mcomplex_runtime_initialised)
        return;

    for (id = MCCONST_ZERO; id < MCCONST_COUNT; ++id) {
        if (mcomplex_const_cache[id].cached_prec &&
            *mcomplex_const_cache[id].cached_prec != 0) {
            mpc_clear(mcomplex_const_cache[id].value->value);
            *mcomplex_const_cache[id].cached_prec = 0;
        }
        mf_free(mcomplex_const_cache[id].value->real_view);
        mf_free(mcomplex_const_cache[id].value->imag_view);
        mcomplex_const_cache[id].value->real_view = NULL;
        mcomplex_const_cache[id].value->imag_view = NULL;
    }

    mpfr_free_cache();
    mcomplex_runtime_initialised = false;
}

void mcomplex_constant_ensure(const mcomplex_t *mcomplex, mpfr_prec_t precision)
{
    mcomplex_constant_id_t id;

    if (!mcomplex || mcomplex->constant_id == MCCONST_NONE)
        return;
    mcomplex_ensure_runtime_init();
    if (precision == 0)
        precision = (mpfr_prec_t)mf_get_default_precision();
    id = mcomplex->constant_id;
    if (id <= MCCONST_NONE || id >= MCCONST_COUNT)
        return;
    if (!mcomplex_const_dispatch[id])
        return;
    mcomplex_const_dispatch[id](precision);
}

void mcomplex_constants_ensure_init(void)
{
    mcomplex_constant_id_t id;

    mcomplex_ensure_runtime_init();
    for (id = MCCONST_ZERO; id < MCCONST_COUNT; ++id) {
        if (mcomplex_const_dispatch[id])
            mcomplex_const_dispatch[id]((mpfr_prec_t)mf_get_default_precision());
    }
}
