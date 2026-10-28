/**
 * @file diffequation_notation_internal.h
 * @brief Controlled diffequation notation adapter.
 *
 * Exposes diffequation-owned mathematical interpretation to native notation consumers.
 * Applications request notation through the public diffequation-t.h API.
 */
#ifndef MARS_SHARED_DIFFEQUATION_NOTATION_INTERNAL_H
#define MARS_SHARED_DIFFEQUATION_NOTATION_INTERNAL_H
#if !defined(MARS_SHARED_DIFFEQUATION_NOTATION_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_SHARED_DIFFEQUATION_NOTATION_INTERNAL_ACCESS."
#endif
#include "diffequation-t.h"
#define MARS_SHARED_EXPR_NOTATION_INTERNAL_ACCESS
#include "internal/expr_notation_internal.h"

/** @brief Create an owned snapshot, released with TeX_context_free. */
TeX_context_t *de_TeX_context(const diffequ_t *value);


#endif
