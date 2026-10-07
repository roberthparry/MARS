/**
 * @file number_internal.h
 * @brief Controlled cross-module numeric interface.
 *
 * Declares selected representation queries, precision conversions and scoped numeric support needed by native MARS
 * modules. Callers should prefer number.h and use this guarded interface only where the public API is
 * insufficient.
 *
 * This header is an implementation detail under src/, not an installed public API. Keep its consumers within the
 * documented module boundary and preserve any explicit internal-access guards.
 */

#ifndef NUMBER_SHARED_INTERNAL_H
#define NUMBER_SHARED_INTERNAL_H

#if !defined(MARS_SHARED_NUMBER_INTERNAL_ACCESS) &&                                                                    \
    (!defined(__INTELLISENSE__) || (defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__ > 0))
#error "internal/number_internal.h is private to the MARS implementation; include number.h instead."
#endif

#include "number.h"

number_t number_invalid(void);
bool num_is_immortal(number_t number);
bool num_is_inexact_real_backend(number_t number);
bool num_is_complex_backend(number_t number);
bool num_get_small_rational(number_t number, long *numerator, long *denominator);
number_t num_as_inexact_real_prec(number_t number, size_t precision_bits);
number_t num_as_complex_prec(number_t number, size_t precision_bits);
num_scope_t *number_scope_suspend(void);
void number_scope_resume(num_scope_t *scope);
void num_scope_resume_cleanup(num_scope_t **scope);
number_t num_lommel_s_derivative_internal(const number_t mu, const number_t nu, const number_t argument);

#define NUM_SCOPE_SUSPEND(name)                                                                                        \
    __attribute__((cleanup(num_scope_resume_cleanup))) num_scope_t *(name) = number_scope_suspend()

#endif
