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

#if !defined(MARS_QFLOAT_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_QFLOAT_INTERNAL_ACCESS."
#endif

#include <math.h>

#include "qfloat-t.h"
#define MARS_SHARED_QFLOAT_INTERNAL_ACCESS
#include "internal/qfloat_internal.h"

/** @brief Dekker splitting factor for the binary64 significand. */
#define QF_SPLIT 134217729.0

/**
 * @brief Split a finite addition into its rounded sum and residual, without allocating storage.
 * @param a First addend, passed by value.
 * @param b Second addend, passed by value.
 * @param[out] s Required caller-owned destination for the rounded sum.
 * @param[out] e Required distinct caller-owned destination for the residual.
 * @return No value; callers must avoid overflow for an error-free result. No pointers are retained.
 */
void qf_two_sum(double a, double b, double *s, double *e);

/**
 * @brief Split a finite multiplication using a fused multiply-add residual, without allocating storage.
 * @param a First factor, passed by value.
 * @param b Second factor, passed by value.
 * @param[out] p Required caller-owned destination for the rounded product.
 * @param[out] e Required distinct caller-owned destination for the residual.
 * @return No value; callers must avoid overflow and residual underflow for an error-free result. No pointers are retained.
 */
void qf_two_prod(double a, double b, double *p, double *e);

/**
 * @brief Split an addition whose first addend has at least the second addend's magnitude.
 * @param a Larger-magnitude finite addend, passed by value.
 * @param b Smaller-magnitude finite addend, passed by value.
 * @param[out] s Required caller-owned destination for the rounded sum.
 * @param[out] e Required distinct caller-owned destination for the residual.
 * @return No value; callers must avoid overflow for an error-free result. Allocates nothing and retains no pointers.
 */
void qf_quick_two_sum(double a, double b, double *s, double *e);

/**
 * @brief Split a finite binary64 value into leading and trailing components for Dekker arithmetic.
 * @param x Value whose product with QF_SPLIT remains finite, passed by value.
 * @param[out] hi Required caller-owned destination for the leading component.
 * @param[out] lo Required distinct caller-owned destination for the trailing component.
 * @return No value. Allocates nothing and retains no pointers; inputs outside the stated range are not corrected.
 */
void qf_split_double(double x, double *hi, double *lo);

/**
 * @brief Convert an internal finite scalar to int by adding its components and truncating towards zero.
 * @param x Value whose component sum is representable as int, passed by value.
 * @return Truncated integer; callers must validate the input range. Allocates nothing and has no shared state.
 */
static inline int qf_to_int(qfloat_t x) { return (int)(x.hi + x.lo); }

/**
 * @brief Renormalise finite leading and trailing components using QuickTwoSum.
 * @param hi Leading component with at least the trailing component's magnitude, passed by value.
 * @param lo Trailing component, passed by value.
 * @return By-value scalar requiring no release; callers must avoid overflowing the component sum.
 */
qfloat_t qf_renorm(double hi, double lo);

/**
 * @brief Generate unrounded decimal digits for native scalar formatting.
 * @param x Finite non-negative magnitude, passed by value.
 * @param ndigits Number of digits to generate; zero requests an empty string and negative values are rejected.
 * @param[out] out_exp10 Optional caller-owned destination for the leading digit's decimal exponent, set on success.
 * @return Owned digit string released with string_free(), or NULL for a negative count or allocation failure.
 * @note Retains no pointers or shared state. The caller handles signs, non-finite values and final rounding.
 */
string_t *qf_decimal_digits_text(qfloat_t x, int ndigits, int *out_exp10);

#endif
