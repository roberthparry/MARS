#include <complex.h>
#include <math.h>

#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"

static number_mpz_t number_zero_mpz_value = {.constant_id = NUMBER_CONST_ZERO, .immortal = true, .initialised = false};
static number_mpz_t number_one_mpz_value = {.constant_id = NUMBER_CONST_ONE, .immortal = true, .initialised = false};
static number_mpz_t number_neg_one_mpz_value = {
    .constant_id = NUMBER_CONST_NEG_ONE, .immortal = true, .initialised = false};
static number_mpz_t number_two_mpz_value = {.constant_id = NUMBER_CONST_TWO, .immortal = true, .initialised = false};
static number_mpz_t number_ten_mpz_value = {.constant_id = NUMBER_CONST_TEN, .immortal = true, .initialised = false};

static number_mpq_t number_half_mpq_value = {.constant_id = NUMBER_CONST_HALF, .immortal = true, .initialised = false};
static number_mpq_t number_one_and_half_mpq_value = {
    .constant_id = NUMBER_CONST_ONE_AND_HALF, .immortal = true, .initialised = false};
static number_mpq_t number_one_third_mpq_value = {
    .constant_id = NUMBER_CONST_ONE_THIRD, .immortal = true, .initialised = false};
static number_mpq_t number_quarter_mpq_value = {
    .constant_id = NUMBER_CONST_QUARTER, .immortal = true, .initialised = false};
static number_mpq_t number_one_sixth_mpq_value = {
    .constant_id = NUMBER_CONST_ONE_SIXTH, .immortal = true, .initialised = false};
static number_mpq_t number_one_eighth_mpq_value = {
    .constant_id = NUMBER_CONST_ONE_EIGHTH, .immortal = true, .initialised = false};
static number_mpq_t number_one_tenth_mpq_value = {
    .constant_id = NUMBER_CONST_ONE_TENTH, .immortal = true, .initialised = false};

static number_mpfr_t number_pi_mpfr_value = {.constant_id = NUMBER_CONST_PI, .immortal = true, .initialised = false};
static number_mpfr_t number_2pi_mpfr_value = {.constant_id = NUMBER_CONST_2PI, .immortal = true, .initialised = false};
static number_mpfr_t number_pi_2_mpfr_value = {
    .constant_id = NUMBER_CONST_PI_2, .immortal = true, .initialised = false};
static number_mpfr_t number_neg_pi_2_mpfr_value = {
    .constant_id = NUMBER_CONST_NEG_PI_2, .immortal = true, .initialised = false};
static number_mpfr_t number_pi_4_mpfr_value = {
    .constant_id = NUMBER_CONST_PI_4, .immortal = true, .initialised = false};
static number_mpfr_t number_3pi_4_mpfr_value = {
    .constant_id = NUMBER_CONST_3PI_4, .immortal = true, .initialised = false};
static number_mpfr_t number_pi_6_mpfr_value = {
    .constant_id = NUMBER_CONST_PI_6, .immortal = true, .initialised = false};
static number_mpfr_t number_pi_3_mpfr_value = {
    .constant_id = NUMBER_CONST_PI_3, .immortal = true, .initialised = false};
static number_mpfr_t number_2_pi_mpfr_value = {
    .constant_id = NUMBER_CONST_2_PI, .immortal = true, .initialised = false};
static number_mpfr_t number_e_mpfr_value = {.constant_id = NUMBER_CONST_E, .immortal = true, .initialised = false};
static number_mpfr_t number_inv_e_mpfr_value = {
    .constant_id = NUMBER_CONST_INV_E, .immortal = true, .initialised = false};
static number_mpfr_t number_neg_inv_e_mpfr_value = {
    .constant_id = NUMBER_CONST_NEG_INV_E, .immortal = true, .initialised = false};
static number_mpfr_t number_ln2_mpfr_value = {.constant_id = NUMBER_CONST_LN2, .immortal = true, .initialised = false};
static number_mpfr_t number_ln10_mpfr_value = {
    .constant_id = NUMBER_CONST_LN10, .immortal = true, .initialised = false};
static number_mpfr_t number_invln2_mpfr_value = {
    .constant_id = NUMBER_CONST_INVLN2, .immortal = true, .initialised = false};
static number_mpfr_t number_euler_mascheroni_mpfr_value = {
    .constant_id = NUMBER_CONST_EULER_MASCHERONI, .immortal = true, .initialised = false};
static number_mpfr_t number_phi_mpfr_value = {.constant_id = NUMBER_CONST_PHI, .immortal = true, .initialised = false};
static number_mpfr_t number_sqrt_half_mpfr_value = {
    .constant_id = NUMBER_CONST_SQRT_HALF, .immortal = true, .initialised = false};
static number_mpfr_t number_sqrt2_mpfr_value = {
    .constant_id = NUMBER_CONST_SQRT2, .immortal = true, .initialised = false};
static number_mpfr_t number_sqrt3_mpfr_value = {
    .constant_id = NUMBER_CONST_SQRT3, .immortal = true, .initialised = false};
static number_mpfr_t number_sqrt2_over_two_mpfr_value = {
    .constant_id = NUMBER_CONST_SQRT2_OVER_TWO, .immortal = true, .initialised = false};
static number_mpfr_t number_sqrt3_over_two_mpfr_value = {
    .constant_id = NUMBER_CONST_SQRT3_OVER_TWO, .immortal = true, .initialised = false};
static number_mpfr_t number_sqrt_2pi_mpfr_value = {
    .constant_id = NUMBER_CONST_SQRT_2PI, .immortal = true, .initialised = false};
static number_mpfr_t number_sqrt_pi_mpfr_value = {
    .constant_id = NUMBER_CONST_SQRT_PI, .immortal = true, .initialised = false};
static number_mpfr_t number_sqrt_pi_over_two_mpfr_value = {
    .constant_id = NUMBER_CONST_SQRT_PI_OVER_TWO, .immortal = true, .initialised = false};
static number_mpfr_t number_sqrt1onpi_mpfr_value = {
    .constant_id = NUMBER_CONST_SQRT1ONPI, .immortal = true, .initialised = false};
static number_mpfr_t number_2_sqrtpi_mpfr_value = {
    .constant_id = NUMBER_CONST_2_SQRTPI, .immortal = true, .initialised = false};
static number_mpfr_t number_neg_two_over_sqrt_pi_mpfr_value = {
    .constant_id = NUMBER_CONST_NEG_TWO_OVER_SQRT_PI, .immortal = true, .initialised = false};
static number_mpfr_t number_inv_sqrt_2pi_mpfr_value = {
    .constant_id = NUMBER_CONST_INV_SQRT_2PI, .immortal = true, .initialised = false};
static number_mpfr_t number_log_sqrt_2pi_mpfr_value = {
    .constant_id = NUMBER_CONST_LOG_SQRT_2PI, .immortal = true, .initialised = false};
static number_mpfr_t number_ln_2pi_mpfr_value = {
    .constant_id = NUMBER_CONST_LN_2PI, .immortal = true, .initialised = false};
static number_mpfr_t number_pi_squared_mpfr_value = {
    .constant_id = NUMBER_CONST_PI_SQUARED, .immortal = true, .initialised = false};
static number_mpfr_t number_2pi_cubed_mpfr_value = {
    .constant_id = NUMBER_CONST_2PI_CUBED, .immortal = true, .initialised = false};
static number_mpfr_t number_nan_mpfr_value = {.constant_id = NUMBER_CONST_NAN, .immortal = true, .initialised = false};
static number_mpfr_t number_inf_mpfr_value = {.constant_id = NUMBER_CONST_INF, .immortal = true, .initialised = false};
static number_mpfr_t number_ninf_mpfr_value = {
    .constant_id = NUMBER_CONST_NINF, .immortal = true, .initialised = false};

static const number_private_t number_zero_value = {.kind = NUMBER_MPZ, .value.mpz = &number_zero_mpz_value};

static const number_private_t number_one_value = {.kind = NUMBER_MPZ, .value.mpz = &number_one_mpz_value};

static const number_private_t number_neg_one_value = {.kind = NUMBER_MPZ, .value.mpz = &number_neg_one_mpz_value};

static const number_private_t number_pi_value = {.kind = NUMBER_MPFR, .value.mpfr = &number_pi_mpfr_value};

static const number_private_t number_2pi_value = {.kind = NUMBER_MPFR, .value.mpfr = &number_2pi_mpfr_value};

static const number_private_t number_pi_2_value = {.kind = NUMBER_MPFR, .value.mpfr = &number_pi_2_mpfr_value};

static const number_private_t number_neg_pi_2_value = {.kind = NUMBER_MPFR, .value.mpfr = &number_neg_pi_2_mpfr_value};

static const number_private_t number_pi_4_value = {.kind = NUMBER_MPFR, .value.mpfr = &number_pi_4_mpfr_value};

static const number_private_t number_3pi_4_value = {.kind = NUMBER_MPFR, .value.mpfr = &number_3pi_4_mpfr_value};

static const number_private_t number_pi_6_value = {.kind = NUMBER_MPFR, .value.mpfr = &number_pi_6_mpfr_value};

static const number_private_t number_pi_3_value = {.kind = NUMBER_MPFR, .value.mpfr = &number_pi_3_mpfr_value};

static const number_private_t number_2_pi_value = {.kind = NUMBER_MPFR, .value.mpfr = &number_2_pi_mpfr_value};

static const number_private_t number_e_value = {.kind = NUMBER_MPFR, .value.mpfr = &number_e_mpfr_value};

static const number_private_t number_inv_e_value = {.kind = NUMBER_MPFR, .value.mpfr = &number_inv_e_mpfr_value};

static const number_private_t number_neg_inv_e_value = {.kind = NUMBER_MPFR,
                                                        .value.mpfr = &number_neg_inv_e_mpfr_value};

static const number_private_t number_ln2_value = {.kind = NUMBER_MPFR, .value.mpfr = &number_ln2_mpfr_value};

static const number_private_t number_ln10_value = {.kind = NUMBER_MPFR, .value.mpfr = &number_ln10_mpfr_value};

static const number_private_t number_invln2_value = {.kind = NUMBER_MPFR, .value.mpfr = &number_invln2_mpfr_value};

static const number_private_t number_euler_mascheroni_value = {.kind = NUMBER_MPFR,
                                                               .value.mpfr = &number_euler_mascheroni_mpfr_value};

static const number_private_t number_phi_value = {.kind = NUMBER_MPFR, .value.mpfr = &number_phi_mpfr_value};

static const number_private_t number_sqrt_half_value = {.kind = NUMBER_MPFR,
                                                        .value.mpfr = &number_sqrt_half_mpfr_value};

static const number_private_t number_sqrt2_value = {.kind = NUMBER_MPFR, .value.mpfr = &number_sqrt2_mpfr_value};

static const number_private_t number_sqrt3_value = {.kind = NUMBER_MPFR, .value.mpfr = &number_sqrt3_mpfr_value};

static const number_private_t number_sqrt2_over_two_value = {.kind = NUMBER_MPFR,
                                                             .value.mpfr = &number_sqrt2_over_two_mpfr_value};

static const number_private_t number_sqrt3_over_two_value = {.kind = NUMBER_MPFR,
                                                             .value.mpfr = &number_sqrt3_over_two_mpfr_value};

static const number_private_t number_sqrt_2pi_value = {.kind = NUMBER_MPFR, .value.mpfr = &number_sqrt_2pi_mpfr_value};

static const number_private_t number_sqrt_pi_value = {.kind = NUMBER_MPFR, .value.mpfr = &number_sqrt_pi_mpfr_value};

static const number_private_t number_sqrt_pi_over_two_value = {.kind = NUMBER_MPFR,
                                                               .value.mpfr = &number_sqrt_pi_over_two_mpfr_value};

static const number_private_t number_sqrt1onpi_value = {.kind = NUMBER_MPFR,
                                                        .value.mpfr = &number_sqrt1onpi_mpfr_value};

static const number_private_t number_2_sqrtpi_value = {.kind = NUMBER_MPFR, .value.mpfr = &number_2_sqrtpi_mpfr_value};

static const number_private_t number_neg_two_over_sqrt_pi_value = {
    .kind = NUMBER_MPFR, .value.mpfr = &number_neg_two_over_sqrt_pi_mpfr_value};

static const number_private_t number_inv_sqrt_2pi_value = {.kind = NUMBER_MPFR,
                                                           .value.mpfr = &number_inv_sqrt_2pi_mpfr_value};

static const number_private_t number_log_sqrt_2pi_value = {.kind = NUMBER_MPFR,
                                                           .value.mpfr = &number_log_sqrt_2pi_mpfr_value};

static const number_private_t number_ln_2pi_value = {.kind = NUMBER_MPFR, .value.mpfr = &number_ln_2pi_mpfr_value};

static const number_private_t number_pi_squared_value = {.kind = NUMBER_MPFR,
                                                         .value.mpfr = &number_pi_squared_mpfr_value};

static const number_private_t number_2pi_cubed_value = {.kind = NUMBER_MPFR,
                                                        .value.mpfr = &number_2pi_cubed_mpfr_value};

static const number_private_t number_half_value = {.kind = NUMBER_MPQ, .value.mpq = &number_half_mpq_value};

static const number_private_t number_one_and_half_value = {.kind = NUMBER_MPQ,
                                                           .value.mpq = &number_one_and_half_mpq_value};

static const number_private_t number_one_third_value = {.kind = NUMBER_MPQ, .value.mpq = &number_one_third_mpq_value};

static const number_private_t number_quarter_value = {.kind = NUMBER_MPQ, .value.mpq = &number_quarter_mpq_value};

static const number_private_t number_one_sixth_value = {.kind = NUMBER_MPQ, .value.mpq = &number_one_sixth_mpq_value};

static const number_private_t number_one_eighth_value = {.kind = NUMBER_MPQ, .value.mpq = &number_one_eighth_mpq_value};

static const number_private_t number_one_tenth_value = {.kind = NUMBER_MPQ, .value.mpq = &number_one_tenth_mpq_value};

static const number_private_t number_two_value = {.kind = NUMBER_MPZ, .value.mpz = &number_two_mpz_value};

static const number_private_t number_ten_value = {.kind = NUMBER_MPZ, .value.mpz = &number_ten_mpz_value};

static const number_private_t number_nan_value = {.kind = NUMBER_MPFR, .value.mpfr = &number_nan_mpfr_value};

static const number_private_t number_inf_value = {.kind = NUMBER_MPFR, .value.mpfr = &number_inf_mpfr_value};

static const number_private_t number_ninf_value = {.kind = NUMBER_MPFR, .value.mpfr = &number_ninf_mpfr_value};

static complex_t number_i_complex_value;
static complex_t number_neg_i_complex_value;

static number_mpz_t *const number_immortal_mpz_values[] = {&number_zero_mpz_value, &number_one_mpz_value,
                                                           &number_neg_one_mpz_value, &number_two_mpz_value,
                                                           &number_ten_mpz_value};

static number_mpq_t *const number_immortal_mpq_values[] = {
    &number_half_mpq_value,      &number_one_and_half_mpq_value, &number_one_third_mpq_value, &number_quarter_mpq_value,
    &number_one_sixth_mpq_value, &number_one_eighth_mpq_value,   &number_one_tenth_mpq_value};

static number_mpfr_t *const number_immortal_mpfr_values[] = {&number_pi_mpfr_value,
                                                             &number_2pi_mpfr_value,
                                                             &number_pi_2_mpfr_value,
                                                             &number_neg_pi_2_mpfr_value,
                                                             &number_pi_4_mpfr_value,
                                                             &number_3pi_4_mpfr_value,
                                                             &number_pi_6_mpfr_value,
                                                             &number_pi_3_mpfr_value,
                                                             &number_2_pi_mpfr_value,
                                                             &number_e_mpfr_value,
                                                             &number_inv_e_mpfr_value,
                                                             &number_neg_inv_e_mpfr_value,
                                                             &number_ln2_mpfr_value,
                                                             &number_ln10_mpfr_value,
                                                             &number_invln2_mpfr_value,
                                                             &number_euler_mascheroni_mpfr_value,
                                                             &number_phi_mpfr_value,
                                                             &number_sqrt_half_mpfr_value,
                                                             &number_sqrt2_mpfr_value,
                                                             &number_sqrt3_mpfr_value,
                                                             &number_sqrt2_over_two_mpfr_value,
                                                             &number_sqrt3_over_two_mpfr_value,
                                                             &number_sqrt_2pi_mpfr_value,
                                                             &number_sqrt_pi_mpfr_value,
                                                             &number_sqrt_pi_over_two_mpfr_value,
                                                             &number_sqrt1onpi_mpfr_value,
                                                             &number_2_sqrtpi_mpfr_value,
                                                             &number_neg_two_over_sqrt_pi_mpfr_value,
                                                             &number_inv_sqrt_2pi_mpfr_value,
                                                             &number_log_sqrt_2pi_mpfr_value,
                                                             &number_ln_2pi_mpfr_value,
                                                             &number_pi_squared_mpfr_value,
                                                             &number_2pi_cubed_mpfr_value,
                                                             &number_nan_mpfr_value,
                                                             &number_inf_mpfr_value,
                                                             &number_ninf_mpfr_value};

static const number_private_t number_i_value = {.kind = NUMBER_COMPLEX, .value.cx = &number_i_complex_value};

static const number_private_t number_neg_i_value = {.kind = NUMBER_COMPLEX, .value.cx = &number_neg_i_complex_value};

typedef union {
    number_private_t priv;
    number_t pub;
} number_const_u;

static const number_const_u num_zero_storage = {.priv = number_zero_value};
static const number_const_u num_one_storage = {.priv = number_one_value};
static const number_const_u num_neg_one_storage = {.priv = number_neg_one_value};
static const number_const_u num_half_storage = {.priv = number_half_value};
static const number_const_u num_one_and_half_storage = {.priv = number_one_and_half_value};
static const number_const_u num_one_third_storage = {.priv = number_one_third_value};
static const number_const_u num_quarter_storage = {.priv = number_quarter_value};
static const number_const_u num_one_sixth_storage = {.priv = number_one_sixth_value};
static const number_const_u num_one_eighth_storage = {.priv = number_one_eighth_value};
static const number_const_u num_one_tenth_storage = {.priv = number_one_tenth_value};
static const number_const_u num_two_storage = {.priv = number_two_value};
static const number_const_u num_ten_storage = {.priv = number_ten_value};
static const number_const_u num_nan_storage = {.priv = number_nan_value};
static const number_const_u num_inf_storage = {.priv = number_inf_value};
static const number_const_u num_ninf_storage = {.priv = number_ninf_value};
static const number_const_u num_pi_storage = {.priv = number_pi_value};
static const number_const_u num_2pi_storage = {.priv = number_2pi_value};
static const number_const_u num_pi_2_storage = {.priv = number_pi_2_value};
static const number_const_u num_neg_pi_2_storage = {.priv = number_neg_pi_2_value};
static const number_const_u num_pi_4_storage = {.priv = number_pi_4_value};
static const number_const_u num_3pi_4_storage = {.priv = number_3pi_4_value};
static const number_const_u num_pi_6_storage = {.priv = number_pi_6_value};
static const number_const_u num_pi_3_storage = {.priv = number_pi_3_value};
static const number_const_u num_2_pi_storage = {.priv = number_2_pi_value};
static const number_const_u num_e_storage = {.priv = number_e_value};
static const number_const_u num_inv_e_storage = {.priv = number_inv_e_value};
static const number_const_u num_neg_inv_e_storage = {.priv = number_neg_inv_e_value};
static const number_const_u num_ln2_storage = {.priv = number_ln2_value};
static const number_const_u num_ln10_storage = {.priv = number_ln10_value};
static const number_const_u num_invln2_storage = {.priv = number_invln2_value};
static const number_const_u num_euler_mascheroni_storage = {.priv = number_euler_mascheroni_value};
static const number_const_u num_phi_storage = {.priv = number_phi_value};
static const number_const_u num_sqrt_half_storage = {.priv = number_sqrt_half_value};
static const number_const_u num_sqrt2_storage = {.priv = number_sqrt2_value};
static const number_const_u num_sqrt3_storage = {.priv = number_sqrt3_value};
static const number_const_u num_sqrt2_over_two_storage = {.priv = number_sqrt2_over_two_value};
static const number_const_u num_sqrt3_over_two_storage = {.priv = number_sqrt3_over_two_value};
static const number_const_u num_sqrt_2pi_storage = {.priv = number_sqrt_2pi_value};
static const number_const_u num_sqrt_pi_storage = {.priv = number_sqrt_pi_value};
static const number_const_u num_sqrt_pi_over_two_storage = {.priv = number_sqrt_pi_over_two_value};
static const number_const_u num_sqrt1onpi_storage = {.priv = number_sqrt1onpi_value};
static const number_const_u num_2_sqrtpi_storage = {.priv = number_2_sqrtpi_value};
static const number_const_u num_neg_two_over_sqrt_pi_storage = {.priv = number_neg_two_over_sqrt_pi_value};
static const number_const_u num_inv_sqrt_2pi_storage = {.priv = number_inv_sqrt_2pi_value};
static const number_const_u num_log_sqrt_2pi_storage = {.priv = number_log_sqrt_2pi_value};
static const number_const_u num_ln_2pi_storage = {.priv = number_ln_2pi_value};
static const number_const_u num_pi_squared_storage = {.priv = number_pi_squared_value};
static const number_const_u num_2pi_cubed_storage = {.priv = number_2pi_cubed_value};
static const number_const_u num_i_storage = {.priv = number_i_value};
static const number_const_u num_neg_i_storage = {.priv = number_neg_i_value};

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
extern const number_t NUM_NEG_PI_2 __attribute__((alias("num_neg_pi_2_storage")));
extern const number_t NUM_PI_4 __attribute__((alias("num_pi_4_storage")));
extern const number_t NUM_3PI_4 __attribute__((alias("num_3pi_4_storage")));
extern const number_t NUM_PI_6 __attribute__((alias("num_pi_6_storage")));
extern const number_t NUM_PI_3 __attribute__((alias("num_pi_3_storage")));
extern const number_t NUM_2_PI __attribute__((alias("num_2_pi_storage")));
extern const number_t NUM_E __attribute__((alias("num_e_storage")));
extern const number_t NUM_INV_E __attribute__((alias("num_inv_e_storage")));
extern const number_t NUM_NEG_INV_E __attribute__((alias("num_neg_inv_e_storage")));
extern const number_t NUM_LN2 __attribute__((alias("num_ln2_storage")));
extern const number_t NUM_LN10 __attribute__((alias("num_ln10_storage")));
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
extern const number_t NUM_NEG_I __attribute__((alias("num_neg_i_storage")));

static void number_init_complex_constant(complex_t *value, number_const_id_t id, number_t real, number_t imag)
{
    value->constant_id = id;
    value->precision_bits = 0u;
    value->real = real;
    value->imag = imag;
    value->mpc_cache_valid = false;
}

__attribute__((constructor)) static void number_init_complex_constants(void)
{
    number_init_complex_constant(&number_i_complex_value, NUMBER_CONST_I, NUM_ZERO, NUM_ONE);
    number_init_complex_constant(&number_neg_i_complex_value, NUMBER_CONST_NEG_I, NUM_ZERO, NUM_NEG_ONE);
}

static void number_clear_immortal_mpz(number_mpz_t *value)
{
    if (!value || !value->initialised)
        return;
    mpz_clear(value->value);
    value->initialised = false;
}

static void number_clear_immortal_mpq(number_mpq_t *value)
{
    if (!value || !value->initialised)
        return;
    mpq_clear(value->value);
    value->initialised = false;
}

static void number_clear_immortal_mpfr(number_mpfr_t *value)
{
    if (!value || !value->initialised)
        return;
    mpfr_clear(value->value);
    value->initialised = false;
}

void number_constants_shutdown(void)
{
    number_complex_clear_mpc_cache(&number_neg_i_complex_value);
    number_complex_clear_mpc_cache(&number_i_complex_value);

    for (size_t i = 0u; i < sizeof(number_immortal_mpfr_values) / sizeof(number_immortal_mpfr_values[0]); ++i)
        number_clear_immortal_mpfr(number_immortal_mpfr_values[i]);
    for (size_t i = 0u; i < sizeof(number_immortal_mpq_values) / sizeof(number_immortal_mpq_values[0]); ++i)
        number_clear_immortal_mpq(number_immortal_mpq_values[i]);
    for (size_t i = 0u; i < sizeof(number_immortal_mpz_values) / sizeof(number_immortal_mpz_values[0]); ++i)
        number_clear_immortal_mpz(number_immortal_mpz_values[i]);
}

static const qfloat_t *const number_const_qfloat_table[NUMBER_CONST_COUNT] = {
    [NUMBER_CONST_ZERO] = &QF_ZERO,
    [NUMBER_CONST_ONE] = &QF_ONE,
    [NUMBER_CONST_NEG_ONE] = &QF_NEG_ONE,
    [NUMBER_CONST_HALF] = &QF_HALF,
    [NUMBER_CONST_ONE_AND_HALF] = &QF_ONE_AND_HALF,
    [NUMBER_CONST_ONE_THIRD] = &QF_ONE_THIRD,
    [NUMBER_CONST_QUARTER] = &QF_QUARTER,
    [NUMBER_CONST_ONE_SIXTH] = &QF_ONE_SIXTH,
    [NUMBER_CONST_ONE_EIGHTH] = &QF_ONE_EIGHTH,
    [NUMBER_CONST_ONE_TENTH] = &QF_ONE_TENTH,
    [NUMBER_CONST_TWO] = &QF_TWO,
    [NUMBER_CONST_TEN] = &QF_TEN,
    [NUMBER_CONST_PI] = &QF_PI,
    [NUMBER_CONST_2PI] = &QF_2PI,
    [NUMBER_CONST_PI_2] = &QF_PI_2,
    [NUMBER_CONST_NEG_PI_2] = &QF_NEG_PI_2,
    [NUMBER_CONST_PI_4] = &QF_PI_4,
    [NUMBER_CONST_3PI_4] = &QF_3PI_4,
    [NUMBER_CONST_PI_6] = &QF_PI_6,
    [NUMBER_CONST_PI_3] = &QF_PI_3,
    [NUMBER_CONST_E] = &QF_E,
    [NUMBER_CONST_INV_E] = &QF_INV_E,
    [NUMBER_CONST_NEG_INV_E] = &QF_NEG_INV_E,
    [NUMBER_CONST_LN2] = &QF_LN2,
    [NUMBER_CONST_LN10] = &QF_LN10,
    [NUMBER_CONST_SQRT2] = &QF_SQRT2,
    [NUMBER_CONST_SQRT3] = &QF_SQRT3,
    [NUMBER_CONST_SQRT2_OVER_TWO] = &QF_SQRT2_OVER_TWO,
    [NUMBER_CONST_SQRT3_OVER_TWO] = &QF_SQRT3_OVER_TWO,
    [NUMBER_CONST_INF] = &QF_INF,
    [NUMBER_CONST_NINF] = &QF_NINF};

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
    [NUMBER_CONST_NEG_PI_2] = &QC_NEG_PI_2,
    [NUMBER_CONST_PI_4] = &QC_PI_4,
    [NUMBER_CONST_3PI_4] = &QC_3PI_4,
    [NUMBER_CONST_PI_6] = &QC_PI_6,
    [NUMBER_CONST_PI_3] = &QC_PI_3,
    [NUMBER_CONST_E] = &QC_E,
    [NUMBER_CONST_INV_E] = &QC_INV_E,
    [NUMBER_CONST_NEG_INV_E] = &QC_NEG_INV_E,
    [NUMBER_CONST_LN2] = &QC_LN2,
    [NUMBER_CONST_LN10] = &QC_LN10,
    [NUMBER_CONST_SQRT2] = &QC_SQRT2,
    [NUMBER_CONST_SQRT3] = &QC_SQRT3,
    [NUMBER_CONST_SQRT2_OVER_TWO] = &QC_SQRT2_OVER_TWO,
    [NUMBER_CONST_SQRT3_OVER_TWO] = &QC_SQRT3_OVER_TWO,
    [NUMBER_CONST_INF] = &QC_INF,
    [NUMBER_CONST_NINF] = &QC_NINF,
    [NUMBER_CONST_I] = &QC_I};

static const number_t *const number_const_exact_table[NUMBER_CONST_COUNT] = {
    [NUMBER_CONST_ZERO] = &NUM_ZERO,
    [NUMBER_CONST_ONE] = &NUM_ONE,
    [NUMBER_CONST_NEG_ONE] = &NUM_NEG_ONE,
    [NUMBER_CONST_HALF] = &NUM_HALF,
    [NUMBER_CONST_ONE_AND_HALF] = &NUM_ONE_AND_HALF,
    [NUMBER_CONST_ONE_THIRD] = &NUM_ONE_THIRD,
    [NUMBER_CONST_QUARTER] = &NUM_QUARTER,
    [NUMBER_CONST_ONE_SIXTH] = &NUM_ONE_SIXTH,
    [NUMBER_CONST_ONE_EIGHTH] = &NUM_ONE_EIGHTH,
    [NUMBER_CONST_ONE_TENTH] = &NUM_ONE_TENTH,
    [NUMBER_CONST_TWO] = &NUM_TWO,
    [NUMBER_CONST_TEN] = &NUM_TEN};

static const double number_const_double_table[NUMBER_CONST_COUNT] = {[NUMBER_CONST_ZERO] = 0.0,
                                                                     [NUMBER_CONST_ONE] = 1.0,
                                                                     [NUMBER_CONST_NEG_ONE] = -1.0,
                                                                     [NUMBER_CONST_HALF] = 0.5,
                                                                     [NUMBER_CONST_ONE_AND_HALF] = 1.5,
                                                                     [NUMBER_CONST_ONE_THIRD] = 1.0 / 3.0,
                                                                     [NUMBER_CONST_QUARTER] = 0.25,
                                                                     [NUMBER_CONST_ONE_SIXTH] = 1.0 / 6.0,
                                                                     [NUMBER_CONST_ONE_EIGHTH] = 0.125,
                                                                     [NUMBER_CONST_ONE_TENTH] = 0.1,
                                                                     [NUMBER_CONST_TWO] = 2.0,
                                                                     [NUMBER_CONST_TEN] = 10.0,
                                                                     [NUMBER_CONST_PI] = M_PI,
                                                                     [NUMBER_CONST_2PI] = 2.0 * M_PI,
                                                                     [NUMBER_CONST_PI_2] = M_PI_2,
                                                                     [NUMBER_CONST_NEG_PI_2] = -M_PI_2,
                                                                     [NUMBER_CONST_PI_4] = M_PI_4,
                                                                     [NUMBER_CONST_3PI_4] = 3.0 * M_PI_4,
                                                                     [NUMBER_CONST_PI_6] = M_PI / 6.0,
                                                                     [NUMBER_CONST_PI_3] = M_PI / 3.0,
                                                                     [NUMBER_CONST_E] = M_E,
                                                                     [NUMBER_CONST_INV_E] = 1.0 / M_E,
                                                                     [NUMBER_CONST_NEG_INV_E] = -1.0 / M_E,
                                                                     [NUMBER_CONST_LN2] = M_LN2,
                                                                     [NUMBER_CONST_LN10] = M_LN10,
                                                                     [NUMBER_CONST_SQRT2] = M_SQRT2,
                                                                     [NUMBER_CONST_SQRT3] = 1.73205080756887729353,
                                                                     [NUMBER_CONST_SQRT2_OVER_TWO] = M_SQRT1_2,
                                                                     [NUMBER_CONST_SQRT3_OVER_TWO] =
                                                                         0.86602540378443864676,
                                                                     [NUMBER_CONST_INF] = INFINITY,
                                                                     [NUMBER_CONST_NINF] = -INFINITY};

static const double _Complex number_const_cdouble_table[NUMBER_CONST_COUNT] = {
    [NUMBER_CONST_ZERO] = 0.0 + 0.0 * I,
    [NUMBER_CONST_ONE] = 1.0 + 0.0 * I,
    [NUMBER_CONST_NEG_ONE] = -1.0 + 0.0 * I,
    [NUMBER_CONST_HALF] = 0.5 + 0.0 * I,
    [NUMBER_CONST_ONE_AND_HALF] = 1.5 + 0.0 * I,
    [NUMBER_CONST_ONE_THIRD] = (1.0 / 3.0) + 0.0 * I,
    [NUMBER_CONST_QUARTER] = 0.25 + 0.0 * I,
    [NUMBER_CONST_ONE_SIXTH] = (1.0 / 6.0) + 0.0 * I,
    [NUMBER_CONST_ONE_EIGHTH] = 0.125 + 0.0 * I,
    [NUMBER_CONST_ONE_TENTH] = 0.1 + 0.0 * I,
    [NUMBER_CONST_TWO] = 2.0 + 0.0 * I,
    [NUMBER_CONST_TEN] = 10.0 + 0.0 * I,
    [NUMBER_CONST_PI] = M_PI + 0.0 * I,
    [NUMBER_CONST_2PI] = 2.0 * M_PI + 0.0 * I,
    [NUMBER_CONST_PI_2] = M_PI_2 + 0.0 * I,
    [NUMBER_CONST_NEG_PI_2] = -M_PI_2 + 0.0 * I,
    [NUMBER_CONST_PI_4] = M_PI_4 + 0.0 * I,
    [NUMBER_CONST_3PI_4] = 3.0 * M_PI_4 + 0.0 * I,
    [NUMBER_CONST_PI_6] = M_PI / 6.0 + 0.0 * I,
    [NUMBER_CONST_PI_3] = M_PI / 3.0 + 0.0 * I,
    [NUMBER_CONST_E] = M_E + 0.0 * I,
    [NUMBER_CONST_INV_E] = 1.0 / M_E + 0.0 * I,
    [NUMBER_CONST_NEG_INV_E] = -1.0 / M_E + 0.0 * I,
    [NUMBER_CONST_LN2] = M_LN2 + 0.0 * I,
    [NUMBER_CONST_LN10] = M_LN10 + 0.0 * I,
    [NUMBER_CONST_SQRT2] = M_SQRT2 + 0.0 * I,
    [NUMBER_CONST_SQRT3] = 1.73205080756887729353 + 0.0 * I,
    [NUMBER_CONST_SQRT2_OVER_TWO] = M_SQRT1_2 + 0.0 * I,
    [NUMBER_CONST_SQRT3_OVER_TWO] = 0.86602540378443864676 + 0.0 * I,
    [NUMBER_CONST_NAN] = NAN + 0.0 * I,
    [NUMBER_CONST_INF] = INFINITY + 0.0 * I,
    [NUMBER_CONST_NINF] = -INFINITY + 0.0 * I,
    [NUMBER_CONST_I] = 0.0 + 1.0 * I,
    [NUMBER_CONST_NEG_I] = 0.0 - 1.0 * I};

static const bool number_const_has_double_table[NUMBER_CONST_COUNT] = {[NUMBER_CONST_ZERO] = true,
                                                                       [NUMBER_CONST_ONE] = true,
                                                                       [NUMBER_CONST_NEG_ONE] = true,
                                                                       [NUMBER_CONST_HALF] = true,
                                                                       [NUMBER_CONST_ONE_AND_HALF] = true,
                                                                       [NUMBER_CONST_ONE_THIRD] = true,
                                                                       [NUMBER_CONST_QUARTER] = true,
                                                                       [NUMBER_CONST_ONE_SIXTH] = true,
                                                                       [NUMBER_CONST_ONE_EIGHTH] = true,
                                                                       [NUMBER_CONST_ONE_TENTH] = true,
                                                                       [NUMBER_CONST_TWO] = true,
                                                                       [NUMBER_CONST_TEN] = true,
                                                                       [NUMBER_CONST_PI] = true,
                                                                       [NUMBER_CONST_2PI] = true,
                                                                       [NUMBER_CONST_PI_2] = true,
                                                                       [NUMBER_CONST_NEG_PI_2] = true,
                                                                       [NUMBER_CONST_PI_4] = true,
                                                                       [NUMBER_CONST_3PI_4] = true,
                                                                       [NUMBER_CONST_PI_6] = true,
                                                                       [NUMBER_CONST_PI_3] = true,
                                                                       [NUMBER_CONST_E] = true,
                                                                       [NUMBER_CONST_INV_E] = true,
                                                                       [NUMBER_CONST_NEG_INV_E] = true,
                                                                       [NUMBER_CONST_LN2] = true,
                                                                       [NUMBER_CONST_LN10] = true,
                                                                       [NUMBER_CONST_SQRT2] = true,
                                                                       [NUMBER_CONST_SQRT3] = true,
                                                                       [NUMBER_CONST_SQRT2_OVER_TWO] = true,
                                                                       [NUMBER_CONST_SQRT3_OVER_TWO] = true,
                                                                       [NUMBER_CONST_INF] = true,
                                                                       [NUMBER_CONST_NINF] = true};

static const bool number_const_has_cdouble_table[NUMBER_CONST_COUNT] = {[NUMBER_CONST_ZERO] = true,
                                                                        [NUMBER_CONST_ONE] = true,
                                                                        [NUMBER_CONST_NEG_ONE] = true,
                                                                        [NUMBER_CONST_HALF] = true,
                                                                        [NUMBER_CONST_ONE_AND_HALF] = true,
                                                                        [NUMBER_CONST_ONE_THIRD] = true,
                                                                        [NUMBER_CONST_QUARTER] = true,
                                                                        [NUMBER_CONST_ONE_SIXTH] = true,
                                                                        [NUMBER_CONST_ONE_EIGHTH] = true,
                                                                        [NUMBER_CONST_ONE_TENTH] = true,
                                                                        [NUMBER_CONST_TWO] = true,
                                                                        [NUMBER_CONST_TEN] = true,
                                                                        [NUMBER_CONST_PI] = true,
                                                                        [NUMBER_CONST_2PI] = true,
                                                                        [NUMBER_CONST_PI_2] = true,
                                                                        [NUMBER_CONST_NEG_PI_2] = true,
                                                                        [NUMBER_CONST_PI_4] = true,
                                                                        [NUMBER_CONST_3PI_4] = true,
                                                                        [NUMBER_CONST_PI_6] = true,
                                                                        [NUMBER_CONST_PI_3] = true,
                                                                        [NUMBER_CONST_E] = true,
                                                                        [NUMBER_CONST_INV_E] = true,
                                                                        [NUMBER_CONST_NEG_INV_E] = true,
                                                                        [NUMBER_CONST_LN2] = true,
                                                                        [NUMBER_CONST_LN10] = true,
                                                                        [NUMBER_CONST_SQRT2] = true,
                                                                        [NUMBER_CONST_SQRT3] = true,
                                                                        [NUMBER_CONST_SQRT2_OVER_TWO] = true,
                                                                        [NUMBER_CONST_SQRT3_OVER_TWO] = true,
                                                                        [NUMBER_CONST_NAN] = true,
                                                                        [NUMBER_CONST_INF] = true,
                                                                        [NUMBER_CONST_NINF] = true,
                                                                        [NUMBER_CONST_I] = true,
                                                                        [NUMBER_CONST_NEG_I] = true};

static const bool number_const_has_ldexp_table[NUMBER_CONST_COUNT] = {
    [NUMBER_CONST_QUARTER] = true, [NUMBER_CONST_ONE_EIGHTH] = true, [NUMBER_CONST_TWO] = true};

static const int number_const_ldexp_table[NUMBER_CONST_COUNT] = {
    [NUMBER_CONST_QUARTER] = -2, [NUMBER_CONST_ONE_EIGHTH] = -3, [NUMBER_CONST_TWO] = 1};

qfloat_t number_const_qfloat(number_const_id_t id)
{
    const qfloat_t *value;

    if ((unsigned)id >= NUMBER_CONST_COUNT)
        return QF_NAN;
    value = number_const_qfloat_table[id];
    return value ? *value : QF_NAN;
}

qcomplex_t number_const_qcomplex(number_const_id_t id)
{
    const qcomplex_t *value;

    if ((unsigned)id >= NUMBER_CONST_COUNT)
        return QC_NAN;
    if (id == NUMBER_CONST_NEG_I)
        return qc_make(QF_ZERO, QF_NEG_ONE);
    value = number_const_qcomplex_table[id];
    return value ? *value : qc_make(number_const_qfloat(id), QF_ZERO);
}

number_t number_const_mpfr_exact(number_const_id_t id)
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

bool number_const_has_cdouble(number_const_id_t id)
{
    return (unsigned)id < NUMBER_CONST_COUNT && number_const_has_cdouble_table[id];
}

double _Complex number_const_cdouble_value(number_const_id_t id)
{
    return (unsigned)id < NUMBER_CONST_COUNT ? number_const_cdouble_table[id] : 0.0 + 0.0 * I;
}

bool number_const_has_ldexp(number_const_id_t id)
{
    return (unsigned)id < NUMBER_CONST_COUNT && number_const_has_ldexp_table[id];
}

int number_const_ldexp_value(number_const_id_t id)
{
    return (unsigned)id < NUMBER_CONST_COUNT ? number_const_ldexp_table[id] : 0;
}

number_t number_create_exact_mpfr_long_prec(long value, size_t precision_bits)
{
    number_mpfr_t *mpfr;

    if (precision_bits == 0u)
        return number_invalid();
    mpfr = number_mpfr_new_prec(precision_bits);
    if (!mpfr) {
        return number_invalid();
    }
    (void)mpfr_set_si(mpfr->value, value, MPFR_RNDN);
    return number_take(number_wrap_mpfr(mpfr));
}

number_t number_create_exact_mpfr_dyadic_prec(long numerator, int exponent2, size_t precision_bits)
{
    number_mpfr_t *mpfr;

    if (precision_bits == 0u)
        return number_invalid();
    mpfr = number_mpfr_new_prec(precision_bits);
    if (!mpfr)
        return number_invalid();
    (void)mpfr_set_si(mpfr->value, numerator, MPFR_RNDN);
    mpfr_mul_2si(mpfr->value, mpfr->value, exponent2, MPFR_RNDN);
    return number_take(number_wrap_mpfr(mpfr));
}

static number_t number_const_imag_magnitude(number_const_id_t id, size_t precision_bits)
{
    number_mpfr_t *mpfr;

    if (id == NUMBER_CONST_NEG_ONE)
        return number_create_exact_mpfr_long_prec(-1, precision_bits);
    if (number_const_has_ldexp(id))
        return number_create_exact_mpfr_dyadic_prec(1, number_const_ldexp_value(id), precision_bits);

    mpfr = number_mpfr_from_const_id(id, precision_bits);
    return mpfr ? number_take(number_wrap_mpfr(mpfr)) : number_invalid();
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

number_t number_imag_const_like_qreal(const number_t *like, number_const_id_t id)
{
    qfloat_t imag_qf;

    (void)like;
    imag_qf = number_const_qfloat(id);
    return qf_isnan(imag_qf) ? number_invalid() : num_create_from_qcomplex(qc_make(QF_ZERO, imag_qf));
}

number_t number_imag_const_like_mpfr(const number_t *like, number_const_id_t id)
{
    const number_vtable_t *vt = number_vt(like);
    size_t precision_bits = vt && vt->get_precision ? vt->get_precision(like) : 0u;
    number_t magnitude;
    number_t real;
    complex_t *complex_value;

    if (precision_bits == 0u)
        precision_bits = number_default_precision_bits;

    magnitude = number_const_imag_magnitude(id, precision_bits);
    if (!number_is_valid_value(&magnitude))
        return number_invalid();
    real = number_create_exact_mpfr_long_prec(0, precision_bits);
    complex_value = number_complex_create(real, magnitude);
    if (complex_value)
        complex_value->precision_bits = precision_bits;
    return complex_value ? number_take(number_wrap_complex(complex_value)) : number_invalid();
}

number_t number_imag_const_return_like(const number_t *like, number_const_id_t id)
{
    const number_vtable_t *vt = number_vt(like);

    return vt && vt->imag_const_like ? vt->imag_const_like(like, id) : number_invalid();
}
