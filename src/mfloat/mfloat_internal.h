#ifndef MFLOAT_INTERNAL_H
#define MFLOAT_INTERNAL_H

#include "mfloat.h"

#include "mint.h"

#define MFLOAT_DEFAULT_PRECISION_BITS 256u
#define MFLOAT_PARSE_GUARD_BITS 4u

typedef enum mfloat_kind_t {
    MFLOAT_KIND_FINITE = 0,
    MFLOAT_KIND_NAN,
    MFLOAT_KIND_POSINF,
    MFLOAT_KIND_NEGINF
} mfloat_kind_t;

struct _mfloat_t {
    mfloat_kind_t kind; /* finite / NaN / infinities */
    short sign;         /* -1, 0, +1 */
    long exponent2;     /* binary exponent */
    size_t precision;   /* target rounded precision in bits */
    mint_t *mantissa;   /* always non-negative */
};

int mfloat_is_immortal(const mfloat_t *mfloat);
int mfloat_is_finite(const mfloat_t *mfloat);
int mfloat_normalise(mfloat_t *mfloat);
int mfloat_copy_value(mfloat_t *dst, const mfloat_t *src);
int mfloat_set_from_signed_mint(mfloat_t *dst, mint_t *value, long exponent2);
mint_t *mfloat_to_scaled_mint(const mfloat_t *mfloat, long target_exp);
size_t mfloat_get_default_precision_internal(void);

#endif
