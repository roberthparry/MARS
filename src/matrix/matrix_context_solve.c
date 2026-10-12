/**
 * @file matrix_context_solve.c
 * @brief Symbolic elimination whose divisions require explicit context evidence.
 *
 * Implements opt-in solving and inversion without assuming an unknown pivot is
 * non-zero. A dense, owned working array makes row exchanges independent of input
 * storage. Missing evidence is distinct from structural singularity. These routines
 * do not change the existing generic matrix algorithms or enumerate parameter cases.
 */
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#include "matrix-t.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"

static void mat_context_release(expr_t **entries, size_t count)
{
    if (entries)
        for (size_t i = 0; i < count; ++i)
            expr_free(entries[i]);
    free(entries);
}

static expr_t *mat_context_entry(const matrix_t *matrix, size_t row, size_t col)
{
    if (mat_typeof(matrix) == MAT_TYPE_EXPR) {
        expr_t *entry = NULL;
        mat_get(matrix, row, col, &entry);
        return entry ? expr_clone(entry) : expr_new_const(NUM_ZERO);
    }
    number_t value = mat_get_num(matrix, row, col);
    expr_t *entry = expr_new_const(value);
    num_destroy(&value);
    return entry;
}

static bool mat_context_divide(expr_t **entry, const expr_t *pivot)
{
    expr_t *quotient = expr_div(*entry, pivot);
    expr_t *simplified = quotient ? expr_simplify(quotient) : NULL;
    expr_free(quotient);
    if (!simplified)
        return false;
    expr_free(*entry);
    *entry = simplified;
    return true;
}

static bool mat_context_eliminate(expr_t **entry, const expr_t *factor, const expr_t *pivot_entry)
{
    expr_t *product = expr_mul(factor, pivot_entry);
    expr_t *difference = product ? expr_sub(*entry, product) : NULL;
    expr_t *simplified = difference ? expr_simplify(difference) : NULL;
    expr_free(difference);
    expr_free(product);
    if (!simplified)
        return false;
    expr_free(*entry);
    *entry = simplified;
    return true;
}

/* Solve using context-proven pivots without mutating either input matrix. */
matrix_t *mat_solve_with_context(const matrix_t *A, const matrix_t *B, const expr_context_t *context)
{
    size_t n = A ? mat_get_row_count(A) : 0, columns = B ? mat_get_col_count(B) : 0;
    if (!A || !B || !context || !n || !columns || mat_get_col_count(A) != n || mat_get_row_count(B) != n) {
        errno = EINVAL;
        return NULL;
    }
    if (columns > SIZE_MAX - n || n > SIZE_MAX / (n + columns) / sizeof(expr_t *)) {
        errno = ENOMEM;
        return NULL;
    }
    size_t width = n + columns, count = n * width;
    expr_t **work = calloc(count, sizeof(*work));
    expr_t **answers = NULL;
    matrix_t *result = NULL;
    int error = ENOMEM;
    if (!work)
        return NULL;
    for (size_t row = 0; row < n; ++row)
        for (size_t col = 0; col < width; ++col) {
            work[row * width + col] = mat_context_entry(col < n ? A : B, row, col < n ? col : col - n);
            if (!work[row * width + col])
                goto cleanup;
        }
    for (size_t col = 0; col < n; ++col) {
        size_t pivot_row = n;
        bool all_zero = true;
        /* Every remaining row is a distinct pivot candidate; this bounded scan is essential to row exchange. */
        for (size_t row = col; row < n; ++row) {
            const expr_t *candidate = work[row * width + col];
            if (!expr_is_exact_zero(candidate))
                all_zero = false;
            if (expr_context_proves(context, candidate, EXPR_CONTEXT_NONZERO)) {
                pivot_row = row;
                break;
            }
        }
        if (pivot_row == n) {
            error = all_zero ? EDOM : EAGAIN;
            goto cleanup;
        }
        if (pivot_row != col)
            for (size_t j = 0; j < width; ++j) {
                expr_t *swap = work[col * width + j];
                work[col * width + j] = work[pivot_row * width + j];
                work[pivot_row * width + j] = swap;
            }
        expr_t *pivot = work[col * width + col];
        expr_retain(pivot);
        bool divided = true;
        for (size_t j = col; j < width && divided; ++j)
            divided = mat_context_divide(&work[col * width + j], pivot);
        expr_free(pivot);
        if (!divided)
            goto cleanup;
        for (size_t row = 0; row < n; ++row) {
            if (row == col || expr_is_exact_zero(work[row * width + col]))
                continue;
            expr_t *factor = work[row * width + col];
            expr_retain(factor);
            bool eliminated = true;
            for (size_t j = col; j < width && eliminated; ++j)
                eliminated = mat_context_eliminate(&work[row * width + j], factor, work[col * width + j]);
            expr_free(factor);
            if (!eliminated)
                goto cleanup;
        }
    }
    answers = calloc(n * columns, sizeof(*answers));
    if (!answers)
        goto cleanup;
    for (size_t row = 0; row < n; ++row)
        for (size_t col = 0; col < columns; ++col) {
            size_t index = row * columns + col;
            answers[index] = expr_context_refine(context, work[row * width + n + col]);
            if (!answers[index])
                goto cleanup;
        }
    result = mat_create_expr(n, columns, answers);

cleanup:
    mat_context_release(answers, n * columns);
    mat_context_release(work, count);
    if (!result)
        errno = error;
    return result;
}

/* Inversion shares the guarded solver rather than maintaining a second pivot policy. */
matrix_t *mat_inverse_with_context(const matrix_t *A, const expr_context_t *context)
{
    size_t n = A ? mat_get_row_count(A) : 0;
    if (!A || !context || !n || mat_get_col_count(A) != n) {
        errno = EINVAL;
        return NULL;
    }
    matrix_t *identity = mat_create_identity_expr(n);
    if (!identity) {
        errno = ENOMEM;
        return NULL;
    }
    matrix_t *result = mat_solve_with_context(A, identity, context);
    int error = errno;
    mat_free(identity);
    errno = error;
    return result;
}
