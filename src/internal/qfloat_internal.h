#ifndef QFLOAT_SHARED_INTERNAL_H
#define QFLOAT_SHARED_INTERNAL_H

typedef struct _mfloat_t mfloat_t;

#include "qfloat.h"

int qf_to_mfloat_exact(mfloat_t *mfloat, qfloat_t value);

extern const qfloat_t QFI_FADDEEVA_AK[32];
extern const qfloat_t QFI_FADDEEVA_CK[32];
extern const qfloat_t QFI_LANCZOS_C[9];

extern const qfloat_t QFI_LANCZOS_SHIFT;
extern const qfloat_t QFI_ELEVEN_OVER_SEVENTY_TWO;
extern const qfloat_t QFI_BERNOULLI_B2;
extern const qfloat_t QFI_BERNOULLI_B4;
extern const qfloat_t QFI_BERNOULLI_B6;
extern const qfloat_t QFI_BERNOULLI_B8;
extern const qfloat_t QFI_BERNOULLI_B10;

#endif
