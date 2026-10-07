/**
 * @file qfloat_internal.h
 * @brief Private fixed-precision real algorithm helpers.
 *
 * Declares implementation-only numerical support and coefficient access used by the qfloat source files. Other
 * native modules should use the narrower src/internal/qfloat_internal.h interface where public operations are
 * insufficient.
 *
 * This header is an implementation detail under src/, not an installed public API. Keep its consumers within the
 * documented module boundary and preserve any explicit internal-access guards.
 */

#ifndef QFLOAT_INTERNAL_H
#define QFLOAT_INTERNAL_H

#if !defined(MARS_QFLOAT_INTERNAL_ACCESS) &&                                                                           \
    (!defined(__INTELLISENSE__) || (defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__ > 0))
#error "qfloat_internal.h is private to the qfloat module; include qfloat.h instead."
#endif

#include <math.h>

#include "qfloat.h"

#define QF_SPLIT 134217729.0

void qf_two_sum(double a, double b, double *s, double *e);
void qf_two_prod(double a, double b, double *p, double *e);
void qf_quick_two_sum(double a, double b, double *s, double *e);
void qf_split_double(double x, double *hi, double *lo);

static inline int qf_to_int(qfloat_t x) { return (int)(x.hi + x.lo); }

qfloat_t qf_renorm(double hi, double lo);
string_t *qf_decimal_digits_text(qfloat_t x, int ndigits, int *out_exp10);

typedef struct qfloat_bernoulli_even_term_t {
    double num;
    double den;
    int sign;
} qfloat_bernoulli_even_term_t;

extern const size_t QFI_BERNOULLI_EVEN_TERM_COUNT;
extern const qfloat_bernoulli_even_term_t QFI_BERNOULLI_EVEN_TERMS[];

#endif
