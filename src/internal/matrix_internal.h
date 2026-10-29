/**
 * @file matrix_internal.h
 * @brief Controlled cross-module matrix input adapter.
 *
 * Native mathematical input dispatch uses this declaration to recognise and render matrix literals.
 * Matrix retains dimensions, storage, binding and element interpretation;
 * TeX supplies typesetting. Define MARS_SHARED_MATRIX_INTERNAL_ACCESS immediately before including this header.
 * Applications use matrix-t.h and request MAT_STRING_LATEX without managing a rendering context.
 */
#ifndef MARS_SHARED_MATRIX_INTERNAL_H
#define MARS_SHARED_MATRIX_INTERNAL_H
#if !defined(MARS_SHARED_MATRIX_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_SHARED_MATRIX_INTERNAL_ACCESS."
#endif

#include "string-t.h"
#include "TeX-t.h"

/**
 * @brief Recognise native matrix input and render its unevaluated mathematical structure.
 * @param text Borrowed input containing a literal, optional power or supported scalar product.
 * @param bindings Borrowed outer binding declarations, or NULL when absent.
 * @param recognised Required caller-owned flag, initialised to false before the call.
 * It becomes true when this adapter claims the input, including recognised input which fails to render.
 * @return Owned notation released with TeX_free, or NULL for unrecognised input, invalid input or failure.
 * @note A false recognised flag permits another native parser to try the input. No input is modified or retained.
 */
TeX_t *mat_notation_input(const string_t *text, const string_t *bindings, bool *recognised);

#endif
