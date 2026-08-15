#include <stdlib.h>

#define MARS_MATRIX_INTERNAL_ACCESS
#include "matrix_internal.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"

static bool mat_beautify_is_integration_constant(const expr_t *expr)
{
    const char *name;

    if (!expr || !expr_is_named_const(expr))
        return false;
    name = expr_symbol_name(expr);
    return name && name[0] == 'C' && name[1] != '\0';
}

static bool mat_beautify_split_integration_constant(const expr_t *expr, expr_t **entry_out, expr_t **constant_out)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    bool subtraction = false;

    *entry_out = NULL;
    *constant_out = NULL;
    if (!expr_match_add_sub_expr(expr, &left, &right, &subtraction) || subtraction)
        return false;
    if (mat_beautify_is_integration_constant(right)) {
        *entry_out = expr_clone(left);
        *constant_out = expr_clone(right);
    } else if (mat_beautify_is_integration_constant(left)) {
        *entry_out = expr_clone(right);
        *constant_out = expr_clone(left);
    }
    if (*entry_out && *constant_out)
        return true;
    expr_free(*entry_out);
    expr_free(*constant_out);
    *entry_out = NULL;
    *constant_out = NULL;
    return false;
}

/* Return a copy with every symbolic entry passed through the expression beautifier. */
matrix_t *mat_beautify_symbolic(const matrix_t *A)
{
    matrix_t *C;

    if (!A)
        return NULL;
    C = mat_copy_preserving_store(A);
    if (!C || C->elem != &expr_elem)
        return C;
    for (size_t row = 0u; row < C->rows; ++row) {
        for (size_t col = 0u; col < C->cols; ++col) {
            expr_t *entry = NULL;
            expr_t *beautified;

            mat_get(C, row, col, &entry);
            beautified = entry ? expr_beautify(entry) : NULL;
            if (entry && !beautified) {
                mat_free(C);
                return NULL;
            }
            if (beautified) {
                mat_set(C, row, col, &beautified);
                expr_free(beautified);
            }
        }
    }
    return C;
}

/* Build the style-independent expression form consumed by matrix renderers. */
int mat_beautify_expression_matrix(const matrix_t *A, mat_expr_beautification_t *beautification)
{
    size_t count;

    if (!A || !beautification || A->elem != &expr_elem)
        return -1;
    *beautification = (mat_expr_beautification_t){0};
    count = A->rows * A->cols;
    beautification->entries = calloc(count ? count : 1u, sizeof(*beautification->entries));
    if (!beautification->entries)
        return -1;
    beautification->count = count;
    for (size_t row = 0u; row < A->rows; ++row) {
        for (size_t col = 0u; col < A->cols; ++col) {
            size_t index = row * A->cols + col;
            expr_t *entry = NULL;

            mat_get(A, row, col, &entry);
            beautification->entries[index] = mat_simplify_expression_for_beautification(entry);
            if (!beautification->entries[index]) {
                mat_expr_beautification_clear(beautification);
                return -1;
            }
        }
    }
    expr_t **split_entries = calloc(count ? count : 1u, sizeof(*split_entries));

    beautification->additive_constants = calloc(count ? count : 1u, sizeof(*beautification->additive_constants));
    if (!split_entries || !beautification->additive_constants) {
        free(split_entries);
        mat_expr_beautification_clear(beautification);
        return -1;
    }
    for (size_t index = 0u; index < count; ++index) {
        if (!mat_beautify_split_integration_constant(beautification->entries[index], &split_entries[index],
                                                      &beautification->additive_constants[index])) {
            for (size_t clear = 0u; clear < index; ++clear) {
                expr_free(split_entries[clear]);
                expr_free(beautification->additive_constants[clear]);
            }
            free(split_entries);
            free(beautification->additive_constants);
            beautification->additive_constants = NULL;
            break;
        }
    }
    if (beautification->additive_constants) {
        for (size_t index = 0u; index < count; ++index) {
            expr_free(beautification->entries[index]);
            beautification->entries[index] = split_entries[index];
        }
        free(split_entries);
    }
    for (size_t index = 0u; index < count; ++index) {
        expr_t *beautified = expr_beautify(beautification->entries[index]);

        if (!beautified)
            beautified = expr_clone(beautification->entries[index]);
        if (!beautified) {
            mat_expr_beautification_clear(beautification);
            return -1;
        }
        expr_free(beautification->entries[index]);
        beautification->entries[index] = beautified;
    }
    if (mat_simplify_common_expression_factor(beautification->entries, count, &beautification->common_factor) != 0) {
        mat_expr_beautification_clear(beautification);
        return -1;
    }
    {
        const expr_t *factor_numerator = NULL;
        const expr_t *factor_denominator = NULL;
        const expr_t *power_base = NULL;
        const expr_t *power_exponent = NULL;
        bool symbolic_power_factor =
            beautification->common_factor &&
            expr_match_div_expr(beautification->common_factor, &factor_numerator, &factor_denominator) &&
            expr_match_pow_expr(factor_denominator, &power_base, &power_exponent);

        if (symbolic_power_factor) {
            for (size_t index = 0u; index < count; ++index) {
                expr_t *beautified = expr_beautify(beautification->entries[index]);

                if (beautified) {
                    expr_free(beautification->entries[index]);
                    beautification->entries[index] = beautified;
                }
            }
        }
    }
    for (size_t index = 0u; index < count; ++index) {
        if (beautification->additive_constants) {
            expr_t *beautified = expr_beautify(beautification->additive_constants[index]);

            if (!beautified)
                beautified = expr_clone(beautification->additive_constants[index]);
            if (!beautified) {
                mat_expr_beautification_clear(beautification);
                return -1;
            }
            expr_free(beautification->additive_constants[index]);
            beautification->additive_constants[index] = beautified;
        }
    }
    if (beautification->common_factor) {
        expr_t *beautified = expr_beautify(beautification->common_factor);

        if (beautified) {
            expr_free(beautification->common_factor);
            beautification->common_factor = beautified;
        }
    }
    return 0;
}

/* Release a completed matrix-beautification result. */
void mat_expr_beautification_clear(mat_expr_beautification_t *beautification)
{
    if (!beautification)
        return;
    for (size_t index = 0u; index < beautification->count; ++index)
        expr_free(beautification->entries[index]);
    for (size_t index = 0u; index < beautification->count; ++index)
        expr_free(beautification->additive_constants ? beautification->additive_constants[index] : NULL);
    free(beautification->entries);
    free(beautification->additive_constants);
    expr_free(beautification->common_factor);
    *beautification = (mat_expr_beautification_t){0};
}
