/**
 * @file number_internal.h
 * @brief Controlled cross-module numeric interface.
 *
 * Declares selected representation queries, precision conversions and scoped numeric support needed by native MARS
 * modules. Callers should prefer number_t and use this guarded interface only where the public API is
 * insufficient. Define MARS_SHARED_NUMBER_INTERNAL_ACCESS immediately before inclusion. Numeric inputs are
 * borrowed unless explicitly stated otherwise: passing a number_t by value does not transfer its storage.
 * Release owned numeric results and successfully populated numeric outputs with num_destroy. Scope tokens
 * remain owned by the scope mechanism and must never be destroyed by consumers of this interface.
 *
 * This header is an implementation detail under src/, not an installed public API. Keep its consumers within the
 * documented module boundary and preserve any explicit internal-access guards.
 */

#ifndef NUMBER_SHARED_INTERNAL_H
#define NUMBER_SHARED_INTERNAL_H

#if !defined(MARS_SHARED_NUMBER_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_SHARED_NUMBER_INTERNAL_ACCESS."
#endif

#include "number-t.h"

/** @brief Symbolic identities carried by number's shared mathematical and physical constants. */
typedef enum number_const_id_t {
    NUMBER_CONST_ZERO,
    NUMBER_CONST_ONE,
    NUMBER_CONST_NEG_ONE,
    NUMBER_CONST_HALF,
    NUMBER_CONST_ONE_AND_HALF,
    NUMBER_CONST_ONE_THIRD,
    NUMBER_CONST_QUARTER,
    NUMBER_CONST_ONE_SIXTH,
    NUMBER_CONST_ONE_EIGHTH,
    NUMBER_CONST_ONE_TENTH,
    NUMBER_CONST_TWO,
    NUMBER_CONST_TEN,
    NUMBER_CONST_PI,
    NUMBER_CONST_2PI,
    NUMBER_CONST_PI_2,
    NUMBER_CONST_NEG_PI_2,
    NUMBER_CONST_PI_4,
    NUMBER_CONST_3PI_4,
    NUMBER_CONST_PI_6,
    NUMBER_CONST_PI_3,
    NUMBER_CONST_2_PI,
    NUMBER_CONST_E,
    NUMBER_CONST_INV_E,
    NUMBER_CONST_NEG_INV_E,
    NUMBER_CONST_LN2,
    NUMBER_CONST_LN10,
    NUMBER_CONST_INVLN2,
    NUMBER_CONST_EULER_MASCHERONI,
    NUMBER_CONST_PHI,
    NUMBER_CONST_SQRT_HALF,
    NUMBER_CONST_SQRT2,
    NUMBER_CONST_SQRT3,
    NUMBER_CONST_SQRT2_OVER_TWO,
    NUMBER_CONST_SQRT3_OVER_TWO,
    NUMBER_CONST_SQRT_2PI,
    NUMBER_CONST_SQRT_PI,
    NUMBER_CONST_SQRT_PI_OVER_TWO,
    NUMBER_CONST_SQRT1ONPI,
    NUMBER_CONST_2_SQRTPI,
    NUMBER_CONST_NEG_TWO_OVER_SQRT_PI,
    NUMBER_CONST_INV_SQRT_2PI,
    NUMBER_CONST_LOG_SQRT_2PI,
    NUMBER_CONST_LN_2PI,
    NUMBER_CONST_PI_SQUARED,
    NUMBER_CONST_2PI_CUBED,
    NUMBER_CONST_NAN,
    NUMBER_CONST_INF,
    NUMBER_CONST_NINF,
    NUMBER_CONST_I,
    NUMBER_CONST_NEG_I,
    NUMBER_CONST_SPEED_OF_LIGHT,
    NUMBER_CONST_ELEMENTARY_CHARGE,
    NUMBER_CONST_PLANCK,
    NUMBER_CONST_BOLTZMANN,
    NUMBER_CONST_AVOGADRO,
    NUMBER_CONST_STANDARD_GRAVITY,
    NUMBER_CONST_ASTRONOMICAL_UNIT,
    NUMBER_CONST_JULIAN_YEAR,
    NUMBER_CONST_COUNT
} number_const_id_t;

/**
 * @brief Read a constant's retained symbolic identity without numeric comparison or materialisation.
 * @param number Borrowed value; no ownership or lifetime changes occur.
 * @param id_out Receives its identity, or NUMBER_CONST_COUNT when no identity is retained.
 * @return True when a symbolic identity is available; false for invalid or untagged values.
 * @note The historical function name also covers materialised constants that retain their identity.
 */
bool number_const_id_from_immortal(const number_t *number, number_const_id_t *id_out);

/**
 * @brief Return the native invalid-value sentinel; no numeric storage is allocated.
 * @return Native invalid-value sentinel with no allocated numeric storage.
 */
number_t number_invalid(void);

/**
 * @brief Report whether the borrowed number uses shared immortal storage.
 * @param number Borrowed numeric input; passing it by value does not transfer its storage.
 * @return True when the documented structural property holds; false otherwise.
 */
bool num_is_immortal(number_t number);

/**
 * @brief Report whether the borrowed number uses the arbitrary-precision real backend.
 * @param number Borrowed numeric input; passing it by value does not transfer its storage.
 * @return True when the documented structural property holds; false otherwise.
 */
bool num_is_inexact_real_backend(number_t number);

/**
 * @brief Report whether the borrowed number uses the arbitrary-precision complex backend.
 * @param number Borrowed numeric input; passing it by value does not transfer its storage.
 * @return True when the documented structural property holds; false otherwise.
 */
bool num_is_complex_backend(number_t number);

/**
 * @brief Read exact real numerator and denominator into caller-owned long outputs when representable.
 * @param number Borrowed numeric input; passing it by value does not transfer its storage.
 * @param numerator Required caller-owned output receiving the exact signed-long numerator on success.
 * @param denominator Required caller-owned output receiving the exact signed-long denominator on success.
 * @return True when the requested operation succeeds; false when unavailable, invalid or unsuccessful.
 */
bool num_get_small_rational(number_t number, long *numerator, long *denominator);

/**
 * @brief Split an exact fraction n/d whose reduced denominator is a non-unit integer square.
 * @param number Borrowed exact rational; inexact and non-rational values are rejected.
 * @param numerator Initialised owned output, replaced with the exact numerator on success.
 * @param denominator_root Distinct initialised owned output, replaced with the positive integer square root of d.
 * @return True on an exact match; false leaves outputs unchanged. Release successful outputs with num_destroy.
 */
bool num_extract_square_denominator(number_t number, number_t *numerator, number_t *denominator_root);

/**
 * @brief Convert a borrowed value to an owned real at the requested precision; release with num_destroy.
 * @param number Borrowed numeric input; passing it by value does not transfer its storage.
 * @param precision_bits Requested numeric precision in bits; zero retains the conversion's default precision.
 * @return Owned numeric result released with num_destroy; the native invalid-value sentinel indicates failure.
 */
number_t num_as_inexact_real_prec(number_t number, size_t precision_bits);

/**
 * @brief Convert a borrowed value to an owned complex at the requested precision; release with num_destroy.
 * @param number Borrowed numeric input; passing it by value does not transfer its storage.
 * @param precision_bits Requested numeric precision in bits; zero retains the conversion's default precision.
 * @return Owned numeric result released with num_destroy; the native invalid-value sentinel indicates failure.
 */
number_t num_as_complex_prec(number_t number, size_t precision_bits);

/**
 * @brief Suspend scoped numeric allocation; pair each call with number_scope_resume on the same thread.
 * @return Borrowed scope token paired with number_scope_resume on the same thread; never free the token.
 */
num_scope_t *number_scope_suspend(void);

/**
 * @brief Resume a suspended numeric allocation scope without freeing the borrowed scope token.
 * @param scope Borrowed scope token returned by number_scope_suspend; the scope remains caller-owned.
 */
void number_scope_resume(num_scope_t *scope);

/**
 * @brief Restore a suspended scope from a compiler cleanup callback; never destroys the scope.
 * @param scope Caller-owned address of a borrowed scope token; the pointed-to scope is never freed here.
 */
void num_scope_resume_cleanup(num_scope_t **scope);

/**
 * @brief Evaluate the argument derivative of the Lommel function; release the owned result with num_destroy.
 * @param mu Mathematical order parameter of the Lommel function, passed by value without transferring storage.
 * @param nu Mathematical order parameter of the Lommel function, passed by value without transferring storage.
 * @param argument Borrowed numeric argument of the Lommel function; its storage remains caller-owned.
 * @return Owned numeric result released with num_destroy; the native invalid-value sentinel indicates failure.
 */
number_t num_lommel_s_derivative_internal(const number_t mu, const number_t nu, const number_t argument);

/** @brief Suspend numeric scope allocation until lexical-scope cleanup resumes it automatically. */
#define NUM_SCOPE_SUSPEND(name)                                                                                        \
    __attribute__((cleanup(num_scope_resume_cleanup))) num_scope_t *(name) = number_scope_suspend()

#endif
