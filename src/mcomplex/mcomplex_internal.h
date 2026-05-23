#ifndef MCOMPLEX_INTERNAL_H
#define MCOMPLEX_INTERNAL_H

#include <mpc.h>

#include "mcomplex.h"

typedef enum mcomplex_constant_id_t {
    MCCONST_NONE = 0,
    MCCONST_ZERO,
    MCCONST_ONE,
    MCCONST_HALF,
    MCCONST_TENTH,
    MCCONST_TEN,
    MCCONST_PI,
    MCCONST_E,
    MCCONST_LN10,
    MCCONST_EULER_MASCHERONI,
    MCCONST_SQRT2,
    MCCONST_SQRT_PI,
    MCCONST_NAN,
    MCCONST_INF,
    MCCONST_NINF,
    MCCONST_I,
    MCCONST_NEG_I,
    MCCONST_COUNT
} mcomplex_constant_id_t;

struct _mcomplex_t {
    mcomplex_constant_id_t constant_id;
    mpc_t value;
    mfloat_t *real_view;
    mfloat_t *imag_view;
};

void mcomplex_constant_ensure(const mcomplex_t *mcomplex, mpfr_prec_t precision);
void mcomplex_constants_ensure_init(void);
int mcomplex_prepare_mutable(mcomplex_t *mcomplex);
int mcomplex_sync_views(mcomplex_t *mcomplex);

#endif
