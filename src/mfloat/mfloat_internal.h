#ifndef MFLOAT_INTERNAL_H
#define MFLOAT_INTERNAL_H

#include <stdbool.h>
#include <mpfr.h>

#include "mfloat.h"

#define MFLOAT_DEFAULT_PRECISION_BITS 256u
#define MFLOAT_PARSE_GUARD_BITS 4u

typedef enum mfloat_constant_id_t {
    MFCONST_NONE = 0,
    MFCONST_ZERO,
    MFCONST_ONE,
    MFCONST_HALF,
    MFCONST_TENTH,
    MFCONST_TEN,
    MFCONST_PI,
    MFCONST_2PI,
    MFCONST_PI_2,
    MFCONST_NEG_PI_2,
    MFCONST_PI_4,
    MFCONST_3PI_4,
    MFCONST_PI_6,
    MFCONST_PI_3,
    MFCONST_2_PI,
    MFCONST_E,
    MFCONST_INV_E,
    MFCONST_LN2,
    MFCONST_LN10,
    MFCONST_INVLN2,
    MFCONST_EULER_MASCHERONI,
    MFCONST_PHI,
    MFCONST_SQRT_HALF,
    MFCONST_SQRT2,
    MFCONST_SQRT3,
    MFCONST_SQRT2_OVER_TWO,
    MFCONST_SQRT3_OVER_TWO,
    MFCONST_SQRT_2PI,
    MFCONST_SQRT_PI,
    MFCONST_SQRT_PI_OVER_TWO,
    MFCONST_SQRT1ONPI,
    MFCONST_2_SQRTPI,
    MFCONST_NEG_TWO_OVER_SQRT_PI,
    MFCONST_INV_SQRT_2PI,
    MFCONST_LOG_SQRT_2PI,
    MFCONST_LN_2PI,
    MFCONST_PI_SQUARED,
    MFCONST_2PI_CUBED,
    MFCONST_NAN,
    MFCONST_INF,
    MFCONST_NINF,
    MFCONST_COUNT
} mfloat_constant_id_t;

struct _mfloat_t {
    mfloat_constant_id_t constant_id; /* internal named-constant identity */
    mpfr_t value;                     /* authoritative representation */
};

/* Constant initialisation hook. */
void mfloat_constants_ensure_init(void);
void mfloat_constants_ensure_precision(mpfr_prec_t precision);
void mfloat_constant_ensure(const mfloat_t *constant, mpfr_prec_t precision);
int mfloat_set_mpfr_from_mrational(mpfr_t out, const mrational_t *value);

#endif
