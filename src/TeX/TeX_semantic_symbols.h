/**
 * @file TeX_semantic_symbols.h
 * @brief Private symbol spelling shared by TeX renderers.
 *
 * Presentation code uses this direct Unicode lookup without consulting expression
 * metadata. Application callers use TeX-t.h rather than this private interface.
 */
#ifndef MARS_TeX_SEMANTIC_SYMBOLS_H
#define MARS_TeX_SEMANTIC_SYMBOLS_H
#if !defined(MARS_TeX_SEMANTIC_SYMBOLS_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_TeX_SEMANTIC_SYMBOLS_INTERNAL_ACCESS."
#endif

#include "string-t.h"

/** @brief Return static presentation text for a Greek code point, or NULL when unknown. */
const char *TeX_semantic_greek(unsigned codepoint);

/** @brief Return a standard function's static spelling by perfect hash, or NULL for a user-defined name. */
const char *TeX_semantic_function(const string_t *name);

/** @brief Return the established TeX spelling for a plain operation name, or NULL if unknown. */
const char *TeX_operation_name(const char *name);
#endif
