/**
 * @file matrix_context.c
 * @brief Independent context-aware snapshots of symbolic matrix entries.
 *
 * Shares scalar sign and domain proofs with equation and calculus results without
 * changing matrix storage policy, numeric entries or pivot-selection semantics.
 */
#include "expr_context-t.h"
#define MARS_MATRIX_INTERNAL_ACCESS
#include "matrix_internal.h"

/* Copy first so failure cannot partially change the caller's matrix. */
matrix_t *mat_simplify_with_context(const matrix_t *A, const expr_context_t *context)
{
    matrix_t *result = A ? mat_copy_preserving_store(A) : NULL;
    if (!result || !matrix_is_symbolic(result))
        return result;
    for (size_t row = 0; row < result->rows; ++row) {
        for (size_t col = 0; col < result->cols; ++col) {
            expr_t *entry = NULL;
            mat_get(result, row, col, &entry);
            if (!entry)
                continue;
            expr_context_t *captured = context ? NULL : expr_context_from_expression(entry, NULL);
            expr_t *simplified = expr_context_refine(context ? context : captured, entry);
            expr_context_free(captured);
            if (!simplified) {
                mat_free(result);
                return NULL;
            }
            mat_set(result, row, col, &simplified);
            expr_free(simplified);
        }
    }
    return result;
}
