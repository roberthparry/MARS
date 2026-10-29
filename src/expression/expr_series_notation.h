/**
 * @file expr_series_notation.h
 * @brief Expression-owned selection of series derivation presentation.
 *
 * This private interface is used by the expression parser. Applications use
 * expr_from_string_with_derivation_TeX from expression-t.h instead.
 */
#ifndef MARS_EXPR_SERIES_NOTATION_H
#define MARS_EXPR_SERIES_NOTATION_H
#if !defined(MARS_EXPR_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_EXPR_INTERNAL_ACCESS."
#endif
#include "expression-t.h"

/**
 * @brief Identify the parameters of a recognised finite zeta difference.
 * @param value Borrowed expression to copy into the semantic snapshot.
 * @param order Borrowed optional solution-order index array; NULL preserves the native solution order.
 * @param endpoint Borrowed native upper-endpoint expression or its input text, according to the declared type.
 * @return True when the requested operation succeeds; false when unavailable, invalid or unsuccessful.
 */
bool expr_series_zeta_difference_parts(const expr_t *value, const expr_t **order, const expr_t **endpoint);

/**
 * @brief Render a series and its recognised closed form; inputs remain owned by the caller.
 * @param series Borrowed rendered starting series; the caller retains its string storage.
 * @param result Borrowed already computed mathematical result or rendered result text, according to its declared
 * type.
 * @return Owned text released with string_free, or NULL when no representation is available or allocation fails.
 */
string_t *expr_series_derivation_TeX(const string_t *series, const expr_t *result);

/**
 * @brief Return an owned rendering of the recognised telescoping identity at a native endpoint.
 * @param endpoint Borrowed native upper-endpoint expression or its input text, according to the declared type.
 * @return Owned text released with string_free, or NULL when no representation is available or allocation fails.
 */
string_t *expr_series_telescoping_identity_TeX(const char *endpoint);

/**
 * @brief Return an owned rendering of the positive or negative reciprocal-prime identity.
 * @param positive Whether the reciprocal-prime product uses addition rather than subtraction.
 * @return Owned text released with string_free, or NULL when no representation is available or allocation fails.
 */
string_t *expr_series_prime_identity_TeX(bool positive);

/**
 * @brief Return an owned rendering of a recognised trigonometric progression; operands remain caller-owned.
 * @param endpoint Borrowed native upper-endpoint expression or its input text, according to the declared type.
 * @param function Borrowed plain mathematical function name used by the recognised progression.
 * @param angle Borrowed angle-step expression; NULL requests a unit step.
 * @param weighted Whether each summand is weighted by the reciprocal of its index.
 * @return Owned text released with string_free, or NULL when no representation is available or allocation fails.
 */
string_t *expr_series_trigonometric_sum_TeX(const expr_t *endpoint, const char *function,
                                           const expr_t *angle, bool weighted);

#endif
