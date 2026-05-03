#ifndef MFLOAT_INTERNAL_H
#define MFLOAT_INTERNAL_H

#include "mfloat.h"

#include "mint.h"

#define MFLOAT_DEFAULT_PRECISION_BITS 256u
#define MFLOAT_PARSE_GUARD_BITS 4u

/* Internal representation tags and flags. */
typedef enum mfloat_kind_t {
    MFLOAT_KIND_FINITE = 0,
    MFLOAT_KIND_NAN,
    MFLOAT_KIND_POSINF,
    MFLOAT_KIND_NEGINF
} mfloat_kind_t;

typedef enum mfloat_flags_t {
    MFLOAT_FLAG_NONE = 0u,
    MFLOAT_FLAG_IMMORTAL = 1u << 0
} mfloat_flags_t;

struct _mfloat_t {
    mfloat_kind_t kind; /* finite / NaN / infinities */
    short sign;         /* -1, 0, +1 */
    long exponent2;     /* binary exponent */
    size_t precision;   /* target rounded precision in bits */
    unsigned flags;     /* internal state flags */
    mint_t *mantissa;   /* always non-negative */
};

/* Core-owned immortal constants used only inside the mfloat module. */
extern const mfloat_t * const MFLOAT_INTERNAL_HALF_LN_PI;
extern const mfloat_t * const MFLOAT_INTERNAL_HALF_LN_2PI;
extern const mfloat_t * const MFLOAT_INTERNAL_ERF_HALF;
extern const mfloat_t * const MFLOAT_INTERNAL_ERFINV_HALF;
extern const mfloat_t * const MFLOAT_INTERNAL_TETRAGAMMA_1;
extern const mfloat_t * const MFLOAT_INTERNAL_GAMMAINV_MIN;
extern const mfloat_t * const MFLOAT_INTERNAL_GAMMAINV_ARGMIN;
extern const mfloat_t * const MFLOAT_INTERNAL_GAMMAINV_3;
extern const mfloat_t * const MFLOAT_INTERNAL_LAMBERT_W0_1;

/* Core representation helpers. */
int mfloat_is_immortal(const mfloat_t *mfloat);
int mfloat_is_finite(const mfloat_t *mfloat);
int mfloat_normalise(mfloat_t *mfloat);
int mfloat_copy_value(mfloat_t *dst, const mfloat_t *src);
int mfloat_set_from_signed_mint(mfloat_t *dst, mint_t *value, long exponent2);
mint_t *mfloat_to_scaled_mint(const mfloat_t *mfloat, long target_exp);
size_t mfloat_get_default_precision_internal(void);

/* Internal immortal-constant helpers. */
mfloat_t *mfloat_clone_immortal_prec_internal(const mfloat_t *src, size_t precision);
int mfloat_set_from_immortal_internal(mfloat_t *dst, const mfloat_t *src, size_t precision);

/* Internal coefficient-table helpers. */
int mfloat_copy_lgamma_asymptotic_term_internal(mfloat_t *dst, size_t index, size_t precision);
int mfloat_mul_euler_gamma_coeff_internal(mfloat_t *mfloat, size_t index, size_t precision);

#endif
