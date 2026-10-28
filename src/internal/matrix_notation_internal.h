/**
 * @file matrix_notation_internal.h
 * @brief Controlled matrix notation adapter.
 *
 * Exposes matrix-owned mathematical interpretation to native notation consumers.
 * Applications request notation through the public matrix-t.h API.
 */
#ifndef MARS_SHARED_MATRIX_NOTATION_INTERNAL_H
#define MARS_SHARED_MATRIX_NOTATION_INTERNAL_H
#if !defined(MARS_SHARED_MATRIX_NOTATION_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_SHARED_MATRIX_NOTATION_INTERNAL_ACCESS."
#endif
#include "matrix-t.h"
#define MARS_SHARED_EXPR_NOTATION_INTERNAL_ACCESS
#include "expr_notation_internal.h"

/** @brief Create an owned snapshot, released with TeX_context_free. */
TeX_context_t *mat_TeX_context(const matrix_t *value);

/** @brief Parse a matrix body and bindings; recognised reports whether the syntax is a matrix. */
TeX_t *mat_notation_input(const string_t *text, const string_t *bindings, bool *recognised);

/** @brief Snapshot matrix dimensions and entries in a borrowed builder. */
const TeX_model_node_t *notation_build_matrix(notation_build_t *build, const matrix_t *value);

#endif
