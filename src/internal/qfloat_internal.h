/**
 * @file qfloat_internal.h
 * @brief Controlled cross-module double-double support.
 *
 * Exposes selected coefficient tables and numerical helpers shared with other native numeric implementations. It
 * avoids exposing the whole qfloat private implementation; application code should include qfloat-t.h. Define
 * MARS_SHARED_QFLOAT_INTERNAL_ACCESS immediately before inclusion. Coefficient arrays and constants have static
 * lifetime and must not be modified or freed. qfloat_t arguments and results are ordinary by-value scalars.
 *
 * This header is an implementation detail under src/, not an installed public API. Keep its consumers within the
 * documented module boundary and preserve any explicit internal-access guards.
 */

#ifndef QFLOAT_SHARED_INTERNAL_H
#define QFLOAT_SHARED_INTERNAL_H

#if !defined(MARS_SHARED_QFLOAT_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_SHARED_QFLOAT_INTERNAL_ACCESS."
#endif

#include <stddef.h>

#include "qfloat-t.h"

/** @brief Number of entries in each immutable Faddeeva coefficient table. */
extern const size_t QFI_FADDEEVA_TERM_COUNT;

/** @brief Number of entries in the immutable Lanczos coefficient table. */
extern const size_t QFI_LANCZOS_COEFF_COUNT;

/** @brief Immutable Faddeeva coefficients shared with native complex arithmetic. */
extern const qfloat_t QFI_FADDEEVA_AK[];

/** @brief Immutable companion Faddeeva coefficients shared with native complex arithmetic. */
extern const qfloat_t QFI_FADDEEVA_CK[];

/** @brief Immutable Lanczos coefficients used by native gamma-related implementations. */
extern const qfloat_t QFI_LANCZOS_C[];

/** @brief Signed rational coefficient in the immutable even-Bernoulli table. */
typedef struct qfloat_bernoulli_even_term_t {
    double num; /**< Unsigned rational numerator. */
    double den; /**< Positive rational denominator. */
    int sign;   /**< Coefficient sign, positive or negative. */
} qfloat_bernoulli_even_term_t;

/** @brief Number of immutable even-Bernoulli rational terms. */
extern const size_t QFI_BERNOULLI_EVEN_TERM_COUNT;

/** @brief Immutable signed rational coefficients for the even Bernoulli sequence. */
extern const qfloat_bernoulli_even_term_t QFI_BERNOULLI_EVEN_TERMS[];

/** @brief Immutable Lanczos shift used with the shared coefficient table. */
extern const qfloat_t QFI_LANCZOS_SHIFT;

/** @brief Immutable double-double value of eleven divided by seventy-two. */
extern const qfloat_t QFI_ELEVEN_OVER_SEVENTY_TWO;

/** @brief Immutable double-double Bernoulli coefficient B_2. */
extern const qfloat_t QFI_BERNOULLI_B2;

/** @brief Immutable double-double Bernoulli coefficient B_4. */
extern const qfloat_t QFI_BERNOULLI_B4;

/** @brief Immutable double-double Bernoulli coefficient B_6. */
extern const qfloat_t QFI_BERNOULLI_B6;

/** @brief Immutable double-double Bernoulli coefficient B_8. */
extern const qfloat_t QFI_BERNOULLI_B8;

/** @brief Immutable double-double Bernoulli coefficient B_10. */
extern const qfloat_t QFI_BERNOULLI_B10;

/**
 * @brief Evaluate the native zeta-derivative difference near one without exposing qfloat internals.
 * @param a Double-double argument, passed by value without separate storage to retain or release.
 * @return Double-double numeric result, with no separate storage to release.
 */
qfloat_t qfi_zetap_difference_at_one(qfloat_t a);

/**
 * @brief Reduce a finite Clausen angle to [-pi, pi] without a machine-integer quotient.
 * @param theta Finite double-double angle in radians to reduce to the principal interval.
 * @return Double-double numeric result, with no separate storage to release.
 */
qfloat_t qf_clausen_reduce(qfloat_t theta);

/**
 * @brief Evaluate the argument derivative of the Lommel function using double-double arithmetic.
 * @param mu Double-double mathematical order parameter, passed by value.
 * @param nu Double-double mathematical order parameter, passed by value.
 * @param argument Double-double function argument, passed by value.
 * @return Double-double numeric result, with no separate storage to release.
 */
qfloat_t qf_lommel_s_derivative_internal(qfloat_t mu, qfloat_t nu, qfloat_t argument);

#endif
