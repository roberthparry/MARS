/**
 * @file diffequation_internal.h
 * @brief Controlled differential-equation semantic-snapshot interface.
 *
 * Native mathematical input adapters, plot parameter selection and notation regressions use this interface.
 * Parameter predicates retain native real-domain assumptions; presentation adapters copy a governing equation
 * and its recorded conditions. Differential equation retains mathematical interpretation; TeX receives semantic
 * nodes rather than solver structures or syntax. Define MARS_SHARED_DIFFEQUATION_INTERNAL_ACCESS immediately
 * before inclusion. Applications use diffequation-t.h and its ordinary TeX output functions.
 */
#ifndef MARS_SHARED_DIFFEQUATION_INTERNAL_H
#define MARS_SHARED_DIFFEQUATION_INTERNAL_H
#if !defined(MARS_SHARED_DIFFEQUATION_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_SHARED_DIFFEQUATION_INTERNAL_ACCESS."
#endif

#include "diffequation-t.h"

#define MARS_SHARED_TeX_INTERNAL_ACCESS
#include "TeX_internal.h"

/**
 * @brief Copy the governing equation and recorded conditions into an independently owned semantic snapshot.
 * @param value Borrowed problem; it may be freed after this call.
 * @return Owned context released with TeX_context_free, or NULL for invalid input or failure.
 * @note This is presentation only: no solver is run and no bindings or conditions are changed.
 */
TeX_context_t *de_TeX_context(const diffequ_t *value);

/**
 * @brief Construct the unit expression restricted by a differential problem's parameter assumptions.
 * @param[in] de Borrowed problem; coordinate intervals and initial conditions are excluded.
 * @return Owned expression released with expr_free(), or NULL for invalid input or allocation failure.
 * @note Native plot adapters use this predicate to select illustrative real parameter values. Parameter nodes
 * are retained, so their owner's later value changes remain visible. Use serially with the problem's bindings.
 */
expr_t *de_parameter_domain(const diffequ_t *de);

/**
 * @brief Parse differential-equation presentation input and render its authored derivative notation.
 * @param text Borrowed native source, neither modified nor retained; NULL or invalid input is rejected.
 * @return Independently owned notation released with TeX_free(), or NULL on parse, formatting or allocation failure.
 * @note A single-coordinate ODE authored with primes retains prime derivatives and function-style point conditions.
 * Explicit coordinate derivatives and PDEs retain differential fractions. No solver runs and no native problem is retained.
 */
TeX_t *de_notation_input(const string_t *text);

#endif
