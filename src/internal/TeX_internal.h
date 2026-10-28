/**
 * @file TeX_internal.h
 * @brief Controlled native rendering interface to the TeX backend.
 *
 * Shares presentation helpers between native mathematical modules and their TeX
 * backend. Application callers use the constructors in TeX-t.h instead.
 */
#ifndef MARS_SHARED_TeX_INTERNAL_H
#define MARS_SHARED_TeX_INTERNAL_H
#if !defined(MARS_SHARED_TeX_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_SHARED_TeX_INTERNAL_ACCESS."
#endif
#define MARS_SHARED_EXPR_STRINGOUT_INTERNAL_ACCESS
#include "internal/expr_stringout_internal.h"

/** @brief Emit the requested mathematical layout element. */
void TeX_emit_definition_heading(sbuf_t *buffer, const char *name, bool separated);

/** @brief Emit the requested mathematical layout element. */
void TeX_emit_parenthesis(sbuf_t *buffer, bool closing);

/** @brief Emit the requested mathematical layout element. */
void TeX_emit_product_space(sbuf_t *buffer);

#endif
