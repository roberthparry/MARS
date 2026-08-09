#ifndef EXPR_STRINGIN_INTERNAL_H
#define EXPR_STRINGIN_INTERNAL_H

#if !defined(MARS_EXPR_STRINGIN_INTERNAL_ACCESS) &&                                                                    \
    (!defined(__INTELLISENSE__) || (defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__ > 0))
#error "expr_stringin_internal.h is private to the expression parser; include expression.h instead."
#endif

#include <stdbool.h>
#include <stddef.h>

#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"

bool expr_stringin_function_hash_is_valid(void);

#endif /* EXPR_STRINGIN_INTERNAL_H */
