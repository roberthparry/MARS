/**
 * @file matrix_notation_internal.h
 * @brief Private matrix semantic snapshots and TeX output preparation.
 *
 * Matrix notation implementations and authorised white-box tests use this interface to copy entries and
 * prepare native output decisions. It exposes no matrix storage. Other modules use matrix-t.h or the minimal
 * controlled matrix input adapter in internal/matrix_internal.h. Define MARS_MATRIX_INTERNAL_ACCESS directly
 * before inclusion. Contexts and rendered strings retain the ownership documented on each declaration.
 */
#ifndef MARS_MATRIX_NOTATION_INTERNAL_H
#define MARS_MATRIX_NOTATION_INTERNAL_H
#if !defined(MARS_MATRIX_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_MATRIX_INTERNAL_ACCESS."
#endif

#include "matrix-t.h"

#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"

/**
 * @brief Copy a matrix into an independently owned semantic snapshot.
 * @param value Borrowed matrix; it may be freed after this call.
 * @return Owned context released with TeX_context_free, or NULL for unsupported dimensions or failure.
 * @note Entries remain symbolic; this function neither evaluates matrix powers nor changes bindings.
 */
TeX_context_t *mat_TeX_context(const matrix_t *value);

/**
 * @brief Copy matrix dimensions and entry semantics into a caller-owned expression snapshot builder.
 * @param build Borrowed builder owning the resulting node and all copied entry data.
 * @param value Borrowed matrix; it need not outlive the completed snapshot.
 * @return Builder-owned node, or NULL for unsupported dimensions or failure; never free the node separately.
 */
const TeX_model_node_t *notation_build_matrix(notation_build_t *build, const matrix_t *value);

/**
 * @brief Render native numeric entries through TeX layout callbacks; return an owned string.
 * @param matrix Borrowed numeric matrix whose entries and components are copied through native numeric APIs.
 * @return Owned TeX string released with string_free, or NULL on invalid dimensions or rendering failure.
 */
string_t *mat_numeric_notation_TeX(const matrix_t *matrix);

/**
 * @brief Render a symbolic matrix and optional native bindings; return an owned string.
 * @param matrix Borrowed symbolic matrix; its bindings, values and storage remain unchanged.
 * @param include_bindings Whether to append the native binding set to the symbolic body.
 * @return Owned TeX string released with string_free, or NULL on invalid dimensions or rendering failure.
 */
string_t *mat_symbolic_notation_TeX(const matrix_t *matrix, bool include_bindings);

#endif
