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

/** @brief Identify the parameters of a recognised finite zeta difference. */
bool expr_series_zeta_difference_parts(const expr_t *value, const expr_t **order, const expr_t **endpoint);

/** @brief Render a series and its recognised closed form; inputs remain owned by the caller. */
string_t *expr_series_derivation_TeX(const string_t *series, const expr_t *result);

#endif
