#ifndef QFLOAT_SHARED_INTERNAL_H
#define QFLOAT_SHARED_INTERNAL_H

#include <stddef.h>

typedef struct _mfloat_t mfloat_t;

#include "qfloat.h"

int qf_to_mfloat_exact(mfloat_t *mfloat, qfloat_t value);

extern const size_t QFI_FADDEEVA_TERM_COUNT;
extern const size_t QFI_LANCZOS_COEFF_COUNT;
extern const qfloat_t QFI_FADDEEVA_AK[];
extern const qfloat_t QFI_FADDEEVA_CK[];
extern const qfloat_t QFI_LANCZOS_C[];

extern const qfloat_t QFI_LANCZOS_SHIFT;
extern const qfloat_t QFI_ELEVEN_OVER_SEVENTY_TWO;
extern const qfloat_t QFI_BERNOULLI_B2;
extern const qfloat_t QFI_BERNOULLI_B4;
extern const qfloat_t QFI_BERNOULLI_B6;
extern const qfloat_t QFI_BERNOULLI_B8;
extern const qfloat_t QFI_BERNOULLI_B10;

#endif
