#ifndef QFLOAT_SHARED_INTERNAL_H
#define QFLOAT_SHARED_INTERNAL_H

#if !defined(MARS_SHARED_QFLOAT_INTERNAL_ACCESS) &&                                                                    \
    (!defined(__INTELLISENSE__) || (defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__ > 0))
#error "internal/qfloat_internal.h is private to the MARS implementation; include qfloat.h instead."
#endif

#include <stddef.h>

#include "qfloat.h"

extern const size_t QFI_FADDEEVA_TERM_COUNT;
extern const size_t QFI_LANCZOS_COEFF_COUNT;
extern const qfloat_t QFI_FADDEEVA_AK[];
extern const qfloat_t QFI_FADDEEVA_CK[];
extern const qfloat_t QFI_LANCZOS_C[];

typedef struct qfloat_bernoulli_even_term_t {
    double num;
    double den;
    int sign;
} qfloat_bernoulli_even_term_t;

extern const size_t QFI_BERNOULLI_EVEN_TERM_COUNT;
extern const qfloat_bernoulli_even_term_t QFI_BERNOULLI_EVEN_TERMS[];

extern const qfloat_t QFI_LANCZOS_SHIFT;
extern const qfloat_t QFI_ELEVEN_OVER_SEVENTY_TWO;
extern const qfloat_t QFI_BERNOULLI_B2;
extern const qfloat_t QFI_BERNOULLI_B4;
extern const qfloat_t QFI_BERNOULLI_B6;
extern const qfloat_t QFI_BERNOULLI_B8;
extern const qfloat_t QFI_BERNOULLI_B10;

#endif
