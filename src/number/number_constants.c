#include <math.h>

#include "number_internal.h"
#include "internal/number_internal.h"
#include "internal/mcomplex_internal.h"
#include "internal/mfloat_internal.h"
#include "internal/mint_internal.h"
#include "internal/mrational_internal.h"

static const number_private_t number_zero_value = {
    .kind = NUMBER_MINT,
    .value.mi = (mint_t *)&MI_ZERO_VALUE
};

static const number_private_t number_one_value = {
    .kind = NUMBER_MINT,
    .value.mi = (mint_t *)&MI_ONE_VALUE
};

static const number_private_t number_neg_one_value = {
    .kind = NUMBER_MINT,
    .value.mi = (mint_t *)&MI_NEG_ONE_VALUE
};

static const number_private_t number_pi_value = {
    .kind = NUMBER_MFLOAT,
    .value.mf = (mfloat_t *)&MF_PI_VALUE
};

static const number_private_t number_2pi_value = {
    .kind = NUMBER_MFLOAT,
    .value.mf = (mfloat_t *)&MF_2PI_VALUE
};

static const number_private_t number_pi_2_value = {
    .kind = NUMBER_MFLOAT,
    .value.mf = (mfloat_t *)&MF_PI_2_VALUE
};

static const number_private_t number_pi_4_value = {
    .kind = NUMBER_MFLOAT,
    .value.mf = (mfloat_t *)&MF_PI_4_VALUE
};

static const number_private_t number_3pi_4_value = {
    .kind = NUMBER_MFLOAT,
    .value.mf = (mfloat_t *)&MF_3PI_4_VALUE
};

static const number_private_t number_pi_6_value = {
    .kind = NUMBER_MFLOAT,
    .value.mf = (mfloat_t *)&MF_PI_6_VALUE
};

static const number_private_t number_pi_3_value = {
    .kind = NUMBER_MFLOAT,
    .value.mf = (mfloat_t *)&MF_PI_3_VALUE
};

static const number_private_t number_2_pi_value = {
    .kind = NUMBER_MFLOAT,
    .value.mf = (mfloat_t *)&MF_2_PI_VALUE
};

static const number_private_t number_e_value = {
    .kind = NUMBER_MFLOAT,
    .value.mf = (mfloat_t *)&MF_E_VALUE
};

static const number_private_t number_inv_e_value = {
    .kind = NUMBER_MFLOAT,
    .value.mf = (mfloat_t *)&MF_INV_E_VALUE
};

static const number_private_t number_ln2_value = {
    .kind = NUMBER_MFLOAT,
    .value.mf = (mfloat_t *)&MF_LN2_VALUE
};

static const number_private_t number_invln2_value = {
    .kind = NUMBER_MFLOAT,
    .value.mf = (mfloat_t *)&MF_INVLN2_VALUE
};

static const number_private_t number_euler_mascheroni_value = {
    .kind = NUMBER_MFLOAT,
    .value.mf = (mfloat_t *)&MF_EULER_MASCHERONI_VALUE
};

static const number_private_t number_phi_value = {
    .kind = NUMBER_MFLOAT,
    .value.mf = (mfloat_t *)&MF_PHI_VALUE
};

static const number_private_t number_sqrt_half_value = {
    .kind = NUMBER_MFLOAT,
    .value.mf = (mfloat_t *)&MF_SQRT_HALF_VALUE
};

static const number_private_t number_sqrt2_value = {
    .kind = NUMBER_MFLOAT,
    .value.mf = (mfloat_t *)&MF_SQRT2_VALUE
};

static const number_private_t number_sqrt3_value = {
    .kind = NUMBER_MFLOAT,
    .value.mf = (mfloat_t *)&MF_SQRT3_VALUE
};

static const number_private_t number_sqrt2_over_two_value = {
    .kind = NUMBER_MFLOAT,
    .value.mf = (mfloat_t *)&MF_SQRT2_OVER_TWO_VALUE
};

static const number_private_t number_sqrt3_over_two_value = {
    .kind = NUMBER_MFLOAT,
    .value.mf = (mfloat_t *)&MF_SQRT3_OVER_TWO_VALUE
};

static const number_private_t number_sqrt_2pi_value = {
    .kind = NUMBER_MFLOAT,
    .value.mf = (mfloat_t *)&MF_SQRT_2PI_VALUE
};

static const number_private_t number_sqrt_pi_value = {
    .kind = NUMBER_MFLOAT,
    .value.mf = (mfloat_t *)&MF_SQRT_PI_VALUE
};

static const number_private_t number_sqrt_pi_over_two_value = {
    .kind = NUMBER_MFLOAT,
    .value.mf = (mfloat_t *)&MF_SQRT_PI_OVER_TWO_VALUE
};

static const number_private_t number_sqrt1onpi_value = {
    .kind = NUMBER_MFLOAT,
    .value.mf = (mfloat_t *)&MF_SQRT1ONPI_VALUE
};

static const number_private_t number_2_sqrtpi_value = {
    .kind = NUMBER_MFLOAT,
    .value.mf = (mfloat_t *)&MF_2_SQRTPI_VALUE
};

static const number_private_t number_neg_two_over_sqrt_pi_value = {
    .kind = NUMBER_MFLOAT,
    .value.mf = (mfloat_t *)&MF_NEG_TWO_OVER_SQRT_PI_VALUE
};

static const number_private_t number_inv_sqrt_2pi_value = {
    .kind = NUMBER_MFLOAT,
    .value.mf = (mfloat_t *)&MF_INV_SQRT_2PI_VALUE
};

static const number_private_t number_log_sqrt_2pi_value = {
    .kind = NUMBER_MFLOAT,
    .value.mf = (mfloat_t *)&MF_LOG_SQRT_2PI_VALUE
};

static const number_private_t number_ln_2pi_value = {
    .kind = NUMBER_MFLOAT,
    .value.mf = (mfloat_t *)&MF_LN_2PI_VALUE
};

static const number_private_t number_pi_squared_value = {
    .kind = NUMBER_MFLOAT,
    .value.mf = (mfloat_t *)&MF_PI_SQUARED_VALUE
};

static const number_private_t number_2pi_cubed_value = {
    .kind = NUMBER_MFLOAT,
    .value.mf = (mfloat_t *)&MF_2PI_CUBED_VALUE
};

static const number_private_t number_half_value = {
    .kind = NUMBER_MRATIONAL,
    .value.mr = (mrational_t *)&MR_HALF_VALUE
};

static const number_private_t number_one_and_half_value = {
    .kind = NUMBER_MRATIONAL,
    .value.mr = (mrational_t *)&MR_ONE_AND_HALF_VALUE
};

static const number_private_t number_one_third_value = {
    .kind = NUMBER_MRATIONAL,
    .value.mr = (mrational_t *)&MR_ONE_THIRD_VALUE
};

static const number_private_t number_quarter_value = {
    .kind = NUMBER_MRATIONAL,
    .value.mr = (mrational_t *)&MR_QUARTER_VALUE
};

static const number_private_t number_one_sixth_value = {
    .kind = NUMBER_MRATIONAL,
    .value.mr = (mrational_t *)&MR_ONE_SIXTH_VALUE
};

static const number_private_t number_one_eighth_value = {
    .kind = NUMBER_MRATIONAL,
    .value.mr = (mrational_t *)&MR_ONE_EIGHTH_VALUE
};

static const number_private_t number_one_tenth_value = {
    .kind = NUMBER_MRATIONAL,
    .value.mr = (mrational_t *)&MR_ONE_TENTH_VALUE
};

static const number_private_t number_two_value = {
    .kind = NUMBER_MINT,
    .value.mi = (mint_t *)&MI_TWO_VALUE
};

static const number_private_t number_ten_value = {
    .kind = NUMBER_MINT,
    .value.mi = (mint_t *)&MI_TEN_VALUE
};

static const number_private_t number_nan_value = {
    .kind = NUMBER_MFLOAT,
    .value.mf = (mfloat_t *)&MF_NAN_VALUE
};

static const number_private_t number_inf_value = {
    .kind = NUMBER_MFLOAT,
    .value.mf = (mfloat_t *)&MF_INF_VALUE
};

static const number_private_t number_ninf_value = {
    .kind = NUMBER_MFLOAT,
    .value.mf = (mfloat_t *)&MF_NINF_VALUE
};

static const number_private_t number_i_value = {
    .kind = NUMBER_MCOMPLEX,
    .value.mc = (mcomplex_t *)&MC_I_VALUE
};

typedef union {
    number_private_t priv;
    number_t pub;
} number_const_u;

static const number_const_u num_zero_storage = { .priv = number_zero_value };
static const number_const_u num_one_storage = { .priv = number_one_value };
static const number_const_u num_neg_one_storage = { .priv = number_neg_one_value };
static const number_const_u num_half_storage = { .priv = number_half_value };
static const number_const_u num_one_and_half_storage = { .priv = number_one_and_half_value };
static const number_const_u num_one_third_storage = { .priv = number_one_third_value };
static const number_const_u num_quarter_storage = { .priv = number_quarter_value };
static const number_const_u num_one_sixth_storage = { .priv = number_one_sixth_value };
static const number_const_u num_one_eighth_storage = { .priv = number_one_eighth_value };
static const number_const_u num_one_tenth_storage = { .priv = number_one_tenth_value };
static const number_const_u num_two_storage = { .priv = number_two_value };
static const number_const_u num_ten_storage = { .priv = number_ten_value };
static const number_const_u num_nan_storage = { .priv = number_nan_value };
static const number_const_u num_inf_storage = { .priv = number_inf_value };
static const number_const_u num_ninf_storage = { .priv = number_ninf_value };
static const number_const_u num_pi_storage = { .priv = number_pi_value };
static const number_const_u num_2pi_storage = { .priv = number_2pi_value };
static const number_const_u num_pi_2_storage = { .priv = number_pi_2_value };
static const number_const_u num_pi_4_storage = { .priv = number_pi_4_value };
static const number_const_u num_3pi_4_storage = { .priv = number_3pi_4_value };
static const number_const_u num_pi_6_storage = { .priv = number_pi_6_value };
static const number_const_u num_pi_3_storage = { .priv = number_pi_3_value };
static const number_const_u num_2_pi_storage = { .priv = number_2_pi_value };
static const number_const_u num_e_storage = { .priv = number_e_value };
static const number_const_u num_inv_e_storage = { .priv = number_inv_e_value };
static const number_const_u num_ln2_storage = { .priv = number_ln2_value };
static const number_const_u num_invln2_storage = { .priv = number_invln2_value };
static const number_const_u num_euler_mascheroni_storage = { .priv = number_euler_mascheroni_value };
static const number_const_u num_phi_storage = { .priv = number_phi_value };
static const number_const_u num_sqrt_half_storage = { .priv = number_sqrt_half_value };
static const number_const_u num_sqrt2_storage = { .priv = number_sqrt2_value };
static const number_const_u num_sqrt3_storage = { .priv = number_sqrt3_value };
static const number_const_u num_sqrt2_over_two_storage = { .priv = number_sqrt2_over_two_value };
static const number_const_u num_sqrt3_over_two_storage = { .priv = number_sqrt3_over_two_value };
static const number_const_u num_sqrt_2pi_storage = { .priv = number_sqrt_2pi_value };
static const number_const_u num_sqrt_pi_storage = { .priv = number_sqrt_pi_value };
static const number_const_u num_sqrt_pi_over_two_storage = { .priv = number_sqrt_pi_over_two_value };
static const number_const_u num_sqrt1onpi_storage = { .priv = number_sqrt1onpi_value };
static const number_const_u num_2_sqrtpi_storage = { .priv = number_2_sqrtpi_value };
static const number_const_u num_neg_two_over_sqrt_pi_storage = { .priv = number_neg_two_over_sqrt_pi_value };
static const number_const_u num_inv_sqrt_2pi_storage = { .priv = number_inv_sqrt_2pi_value };
static const number_const_u num_log_sqrt_2pi_storage = { .priv = number_log_sqrt_2pi_value };
static const number_const_u num_ln_2pi_storage = { .priv = number_ln_2pi_value };
static const number_const_u num_pi_squared_storage = { .priv = number_pi_squared_value };
static const number_const_u num_2pi_cubed_storage = { .priv = number_2pi_cubed_value };
static const number_const_u num_i_storage = { .priv = number_i_value };

extern const number_t NUM_ZERO __attribute__((alias("num_zero_storage")));
extern const number_t NUM_ONE __attribute__((alias("num_one_storage")));
extern const number_t NUM_NEG_ONE __attribute__((alias("num_neg_one_storage")));
extern const number_t NUM_HALF __attribute__((alias("num_half_storage")));
extern const number_t NUM_ONE_AND_HALF __attribute__((alias("num_one_and_half_storage")));
extern const number_t NUM_ONE_THIRD __attribute__((alias("num_one_third_storage")));
extern const number_t NUM_QUARTER __attribute__((alias("num_quarter_storage")));
extern const number_t NUM_ONE_SIXTH __attribute__((alias("num_one_sixth_storage")));
extern const number_t NUM_ONE_EIGHTH __attribute__((alias("num_one_eighth_storage")));
extern const number_t NUM_ONE_TENTH __attribute__((alias("num_one_tenth_storage")));
extern const number_t NUM_TWO __attribute__((alias("num_two_storage")));
extern const number_t NUM_TEN __attribute__((alias("num_ten_storage")));
extern const number_t NUM_NAN __attribute__((alias("num_nan_storage")));
extern const number_t NUM_INF __attribute__((alias("num_inf_storage")));
extern const number_t NUM_NINF __attribute__((alias("num_ninf_storage")));
extern const number_t NUM_PI __attribute__((alias("num_pi_storage")));
extern const number_t NUM_2PI __attribute__((alias("num_2pi_storage")));
extern const number_t NUM_PI_2 __attribute__((alias("num_pi_2_storage")));
extern const number_t NUM_PI_4 __attribute__((alias("num_pi_4_storage")));
extern const number_t NUM_3PI_4 __attribute__((alias("num_3pi_4_storage")));
extern const number_t NUM_PI_6 __attribute__((alias("num_pi_6_storage")));
extern const number_t NUM_PI_3 __attribute__((alias("num_pi_3_storage")));
extern const number_t NUM_2_PI __attribute__((alias("num_2_pi_storage")));
extern const number_t NUM_E __attribute__((alias("num_e_storage")));
extern const number_t NUM_INV_E __attribute__((alias("num_inv_e_storage")));
extern const number_t NUM_LN2 __attribute__((alias("num_ln2_storage")));
extern const number_t NUM_INVLN2 __attribute__((alias("num_invln2_storage")));
extern const number_t NUM_EULER_MASCHERONI __attribute__((alias("num_euler_mascheroni_storage")));
extern const number_t NUM_PHI __attribute__((alias("num_phi_storage")));
extern const number_t NUM_SQRT_HALF __attribute__((alias("num_sqrt_half_storage")));
extern const number_t NUM_SQRT2 __attribute__((alias("num_sqrt2_storage")));
extern const number_t NUM_SQRT3 __attribute__((alias("num_sqrt3_storage")));
extern const number_t NUM_SQRT2_OVER_TWO __attribute__((alias("num_sqrt2_over_two_storage")));
extern const number_t NUM_SQRT3_OVER_TWO __attribute__((alias("num_sqrt3_over_two_storage")));
extern const number_t NUM_SQRT_2PI __attribute__((alias("num_sqrt_2pi_storage")));
extern const number_t NUM_SQRT_PI __attribute__((alias("num_sqrt_pi_storage")));
extern const number_t NUM_SQRT_PI_OVER_TWO __attribute__((alias("num_sqrt_pi_over_two_storage")));
extern const number_t NUM_SQRT1ONPI __attribute__((alias("num_sqrt1onpi_storage")));
extern const number_t NUM_2_SQRTPI __attribute__((alias("num_2_sqrtpi_storage")));
extern const number_t NUM_NEG_TWO_OVER_SQRT_PI __attribute__((alias("num_neg_two_over_sqrt_pi_storage")));
extern const number_t NUM_INV_SQRT_2PI __attribute__((alias("num_inv_sqrt_2pi_storage")));
extern const number_t NUM_LOG_SQRT_2PI __attribute__((alias("num_log_sqrt_2pi_storage")));
extern const number_t NUM_LN_2PI __attribute__((alias("num_ln_2pi_storage")));
extern const number_t NUM_PI_SQUARED __attribute__((alias("num_pi_squared_storage")));
extern const number_t NUM_2PI_CUBED __attribute__((alias("num_2pi_cubed_storage")));
extern const number_t NUM_I __attribute__((alias("num_i_storage")));

static const qfloat_t *const number_const_qfloat_table[NUMBER_CONST_COUNT] = {
    [NUMBER_CONST_ZERO] = &QF_ZERO,
    [NUMBER_CONST_ONE] = &QF_ONE,
    [NUMBER_CONST_NEG_ONE] = &QF_NEG_ONE,
    [NUMBER_CONST_HALF] = &QF_HALF,
    [NUMBER_CONST_QUARTER] = &QF_QUARTER,
    [NUMBER_CONST_ONE_EIGHTH] = &QF_ONE_EIGHTH,
    [NUMBER_CONST_TWO] = &QF_TWO,
    [NUMBER_CONST_PI] = &QF_PI,
    [NUMBER_CONST_2PI] = &QF_2PI,
    [NUMBER_CONST_PI_2] = &QF_PI_2,
    [NUMBER_CONST_PI_4] = &QF_PI_4,
    [NUMBER_CONST_3PI_4] = &QF_3PI_4,
    [NUMBER_CONST_PI_6] = &QF_PI_6,
    [NUMBER_CONST_PI_3] = &QF_PI_3,
    [NUMBER_CONST_E] = &QF_E,
    [NUMBER_CONST_INV_E] = &QF_INV_E,
    [NUMBER_CONST_LN2] = &QF_LN2,
    [NUMBER_CONST_SQRT2] = &QF_SQRT2,
    [NUMBER_CONST_SQRT3] = &QF_SQRT3,
    [NUMBER_CONST_SQRT2_OVER_TWO] = &QF_SQRT2_OVER_TWO,
    [NUMBER_CONST_SQRT3_OVER_TWO] = &QF_SQRT3_OVER_TWO
};

static const mfloat_t *const number_const_mfloat_table[NUMBER_CONST_COUNT] = {
    [NUMBER_CONST_ZERO] = &MF_ZERO_VALUE,
    [NUMBER_CONST_ONE] = &MF_ONE_VALUE,
    [NUMBER_CONST_HALF] = &MF_HALF_VALUE,
    [NUMBER_CONST_PI] = &MF_PI_VALUE,
    [NUMBER_CONST_2PI] = &MF_2PI_VALUE,
    [NUMBER_CONST_PI_2] = &MF_PI_2_VALUE,
    [NUMBER_CONST_PI_4] = &MF_PI_4_VALUE,
    [NUMBER_CONST_3PI_4] = &MF_3PI_4_VALUE,
    [NUMBER_CONST_PI_6] = &MF_PI_6_VALUE,
    [NUMBER_CONST_PI_3] = &MF_PI_3_VALUE,
    [NUMBER_CONST_E] = &MF_E_VALUE,
    [NUMBER_CONST_INV_E] = &MF_INV_E_VALUE,
    [NUMBER_CONST_LN2] = &MF_LN2_VALUE,
    [NUMBER_CONST_SQRT2] = &MF_SQRT2_VALUE,
    [NUMBER_CONST_SQRT3] = &MF_SQRT3_VALUE,
    [NUMBER_CONST_SQRT2_OVER_TWO] = &MF_SQRT2_OVER_TWO_VALUE,
    [NUMBER_CONST_SQRT3_OVER_TWO] = &MF_SQRT3_OVER_TWO_VALUE
};

static const qcomplex_t *const number_const_qcomplex_table[NUMBER_CONST_COUNT] = {
    [NUMBER_CONST_ZERO] = &QC_ZERO,
    [NUMBER_CONST_ONE] = &QC_ONE,
    [NUMBER_CONST_NEG_ONE] = &QC_NEG_ONE,
    [NUMBER_CONST_HALF] = &QC_HALF,
    [NUMBER_CONST_QUARTER] = &QC_QUARTER,
    [NUMBER_CONST_ONE_EIGHTH] = &QC_ONE_EIGHTH,
    [NUMBER_CONST_TWO] = &QC_TWO,
    [NUMBER_CONST_PI] = &QC_PI,
    [NUMBER_CONST_2PI] = &QC_2PI,
    [NUMBER_CONST_PI_2] = &QC_PI_2,
    [NUMBER_CONST_PI_4] = &QC_PI_4,
    [NUMBER_CONST_3PI_4] = &QC_3PI_4,
    [NUMBER_CONST_PI_6] = &QC_PI_6,
    [NUMBER_CONST_PI_3] = &QC_PI_3,
    [NUMBER_CONST_E] = &QC_E,
    [NUMBER_CONST_INV_E] = &QC_INV_E,
    [NUMBER_CONST_LN2] = &QC_LN2,
    [NUMBER_CONST_SQRT2] = &QC_SQRT2,
    [NUMBER_CONST_SQRT3] = &QC_SQRT3,
    [NUMBER_CONST_SQRT2_OVER_TWO] = &QC_SQRT2_OVER_TWO,
    [NUMBER_CONST_SQRT3_OVER_TWO] = &QC_SQRT3_OVER_TWO,
    [NUMBER_CONST_I] = &QC_I
};

static const number_t *const number_const_exact_table[NUMBER_CONST_COUNT] = {
    [NUMBER_CONST_ZERO] = &NUM_ZERO,
    [NUMBER_CONST_ONE] = &NUM_ONE,
    [NUMBER_CONST_NEG_ONE] = &NUM_NEG_ONE,
    [NUMBER_CONST_HALF] = &NUM_HALF,
    [NUMBER_CONST_QUARTER] = &NUM_QUARTER,
    [NUMBER_CONST_ONE_EIGHTH] = &NUM_ONE_EIGHTH,
    [NUMBER_CONST_TWO] = &NUM_TWO
};

static const double number_const_double_table[NUMBER_CONST_COUNT] = {
    [NUMBER_CONST_ZERO] = 0.0,
    [NUMBER_CONST_ONE] = 1.0,
    [NUMBER_CONST_NEG_ONE] = -1.0,
    [NUMBER_CONST_HALF] = 0.5,
    [NUMBER_CONST_QUARTER] = 0.25,
    [NUMBER_CONST_ONE_EIGHTH] = 0.125,
    [NUMBER_CONST_TWO] = 2.0,
    [NUMBER_CONST_PI] = M_PI,
    [NUMBER_CONST_2PI] = 2.0 * M_PI,
    [NUMBER_CONST_PI_2] = M_PI_2,
    [NUMBER_CONST_PI_4] = M_PI_4,
    [NUMBER_CONST_3PI_4] = 3.0 * M_PI_4,
    [NUMBER_CONST_PI_6] = M_PI / 6.0,
    [NUMBER_CONST_PI_3] = M_PI / 3.0,
    [NUMBER_CONST_E] = M_E,
    [NUMBER_CONST_INV_E] = 1.0 / M_E,
    [NUMBER_CONST_LN2] = M_LN2,
    [NUMBER_CONST_SQRT2] = M_SQRT2,
    [NUMBER_CONST_SQRT3] = 1.73205080756887729353,
    [NUMBER_CONST_SQRT2_OVER_TWO] = M_SQRT1_2,
    [NUMBER_CONST_SQRT3_OVER_TWO] = 0.86602540378443864676
};

static const bool number_const_has_double_table[NUMBER_CONST_COUNT] = {
    [NUMBER_CONST_ZERO] = true,
    [NUMBER_CONST_ONE] = true,
    [NUMBER_CONST_NEG_ONE] = true,
    [NUMBER_CONST_HALF] = true,
    [NUMBER_CONST_QUARTER] = true,
    [NUMBER_CONST_ONE_EIGHTH] = true,
    [NUMBER_CONST_TWO] = true,
    [NUMBER_CONST_PI] = true,
    [NUMBER_CONST_2PI] = true,
    [NUMBER_CONST_PI_2] = true,
    [NUMBER_CONST_PI_4] = true,
    [NUMBER_CONST_3PI_4] = true,
    [NUMBER_CONST_PI_6] = true,
    [NUMBER_CONST_PI_3] = true,
    [NUMBER_CONST_E] = true,
    [NUMBER_CONST_INV_E] = true,
    [NUMBER_CONST_LN2] = true,
    [NUMBER_CONST_SQRT2] = true,
    [NUMBER_CONST_SQRT3] = true,
    [NUMBER_CONST_SQRT2_OVER_TWO] = true,
    [NUMBER_CONST_SQRT3_OVER_TWO] = true
};

static const bool number_const_has_ldexp_table[NUMBER_CONST_COUNT] = {
    [NUMBER_CONST_QUARTER] = true,
    [NUMBER_CONST_ONE_EIGHTH] = true,
    [NUMBER_CONST_TWO] = true
};

static const int number_const_ldexp_table[NUMBER_CONST_COUNT] = {
    [NUMBER_CONST_QUARTER] = -2,
    [NUMBER_CONST_ONE_EIGHTH] = -3,
    [NUMBER_CONST_TWO] = 1
};

qfloat_t number_const_qfloat(number_const_id_t id)
{
    const qfloat_t *value;

    if ((unsigned)id >= NUMBER_CONST_COUNT)
        return QF_NAN;
    value = number_const_qfloat_table[id];
    return value ? *value : QF_NAN;
}

const mfloat_t *number_const_mfloat_value(number_const_id_t id)
{
    return (unsigned)id < NUMBER_CONST_COUNT ? number_const_mfloat_table[id] : NULL;
}

qcomplex_t number_const_qcomplex(number_const_id_t id)
{
    const qcomplex_t *value;

    if ((unsigned)id >= NUMBER_CONST_COUNT)
        return QC_NAN;
    value = number_const_qcomplex_table[id];
    return value ? *value : qc_make(number_const_qfloat(id), QF_ZERO);
}

number_t number_const_mreal_exact(number_const_id_t id)
{
    const number_t *value;

    if ((unsigned)id >= NUMBER_CONST_COUNT)
        return number_invalid();
    value = number_const_exact_table[id];
    return value ? *value : number_invalid();
}

bool number_const_has_double(number_const_id_t id)
{
    return (unsigned)id < NUMBER_CONST_COUNT && number_const_has_double_table[id];
}

double number_const_double_value(number_const_id_t id)
{
    return (unsigned)id < NUMBER_CONST_COUNT ? number_const_double_table[id] : 0.0;
}

bool number_const_has_ldexp(number_const_id_t id)
{
    return (unsigned)id < NUMBER_CONST_COUNT && number_const_has_ldexp_table[id];
}

int number_const_ldexp_value(number_const_id_t id)
{
    return (unsigned)id < NUMBER_CONST_COUNT ? number_const_ldexp_table[id] : 0;
}

number_t number_create_exact_mfloat_long_prec(long value, size_t precision_bits)
{
    mfloat_t *mfloat;

    if (precision_bits == 0u)
        return number_invalid();
    mfloat = mf_new_prec(precision_bits);
    if (!mfloat || mf_set_long(mfloat, value) != 0) {
        mf_free(mfloat);
        return number_invalid();
    }
    return number_take(number_wrap_mfloat(mfloat));
}

number_t number_create_exact_mfloat_dyadic_prec(long numerator,
                                                int exponent2,
                                                size_t precision_bits)
{
    mfloat_t *mfloat;

    if (precision_bits == 0u)
        return number_invalid();
    mfloat = mf_new_prec(precision_bits);
    if (!mfloat || mf_set_long(mfloat, numerator) != 0 ||
        mf_ldexp(mfloat, exponent2) != 0) {
        mf_free(mfloat);
        return number_invalid();
    }
    return number_take(number_wrap_mfloat(mfloat));
}

number_t number_const_return_like(const number_t *like, number_const_id_t id)
{
    return number_const_like(like, id);
}

number_t number_neg_const_return_like(const number_t *like, number_const_id_t id)
{
    NUM_SCOPE_SUSPEND(saved_scope);
    number_t value = number_const_like(like, id);
    number_t out = num_neg(value);

    num_destroy(&value);
    return out;
}

number_t number_imag_const_return_like(const number_t *like, number_const_id_t id)
{
    NUM_SCOPE_SUSPEND(saved_scope);
    number_t imag_unit = number_const_like(like, NUMBER_CONST_I);
    number_t value = number_const_like(like, id);
    number_t out = num_mul(imag_unit, value);

    num_destroy(&imag_unit);
    num_destroy(&value);
    return out;
}
