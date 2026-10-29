/**
 * @file matrix_notation_output.c
 * @brief Prepare symbolic matrix output without inspecting rendered notation.
 *
 * Matrix selects common factors, additive constants and its deduplicated native bindings. Expression supplies
 * semantic entry and binding snapshots; TeX owns every delimiter, separator and layout command. This adapter
 * serves the existing MAT_STRING_LATEX output mode without exposing rendering contexts to applications.
 */
#include <stdint.h>
#include <stdlib.h>

#include "matrix-t.h"
#include "TeX-t.h"

#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"
#define MARS_MATRIX_INTERNAL_ACCESS
#include "matrix_notation_internal.h"
#define MARS_MATRIX_INTERNAL_ACCESS
#include "matrix_internal.h"

static const TeX_model_node_t *matrix_node(notation_build_t *build, expr_t *const *values,
                                          size_t rows, size_t columns)
{
    size_t count = rows * columns;
    const TeX_model_node_t **entries = calloc(count, sizeof *entries);
    if (!entries)
        return NULL;
    for (size_t i = 0; i < count; ++i)
        entries[i] = notation_build_expr(build, values[i]);
    const TeX_model_node_t *node = notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_MATRIX, .rows = rows, .columns = columns, .child_count = count}, entries);
    free(entries);
    return node;
}

static const TeX_model_node_t *pair(notation_build_t *build, TeX_node_kind_t kind,
                                    const TeX_model_node_t *left, const TeX_model_node_t *right)
{
    const TeX_model_node_t *parts[] = {left, right};
    return notation_build_node(build, (TeX_node_view_t){.kind = kind, .child_count = 2u}, parts);
}

static const TeX_model_node_t *matrix_bindings(notation_build_t *build, mat_bindings_t *bindings, bool *all_unset)
{
    size_t count = mat_bindings_count(bindings), variables = 0u;
    *all_unset = count != 0u;
    if (!count)
        return NULL;
    const TeX_model_node_t **assignments = calloc(count, sizeof *assignments);
    if (!assignments)
        return NULL;
    /* Stable partition into variables and constants preserves each native binding group's order. */
    for (size_t i = 0u; i < count; ++i)
        variables += !mat_bindings_is_constant_at(bindings, i);
    size_t variable = 0u, constant = variables;
    for (size_t i = 0u; i < count; ++i) {
        size_t index = mat_bindings_is_constant_at(bindings, i) ? constant++ : variable++;
        bool unset = false;
        assignments[index] = notation_build_assignment(build, mat_bindings_expr_at(bindings, i), &unset);
        *all_unset &= unset;
    }
    const TeX_model_node_t *result = notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_BINDINGS, .rows = variables, .child_count = count}, assignments);
    free(assignments);
    return result;
}

/* Snapshot mathematical display decisions before asking TeX to render them. */
string_t *mat_symbolic_notation_TeX(const matrix_t *matrix, bool include_bindings)
{
    size_t rows = mat_get_row_count(matrix), columns = mat_get_col_count(matrix);
    if (!rows || !columns || rows > SIZE_MAX / columns || rows * columns > 65536u)
        return NULL;
    mat_expr_beautification_t view = {0};
    if (mat_beautify_expression_matrix(matrix, &view))
        return NULL;
    notation_build_t *build = notation_build_new();
    const TeX_model_node_t *body = matrix_node(build, view.entries, rows, columns);
    if (view.common_factor)
        body = pair(build, TeX_NODE_SCALED, notation_build_expr(build, view.common_factor), body);
    if (view.additive_constants)
        body = pair(build, TeX_NODE_SUM, body, matrix_node(build, view.additive_constants, rows, columns));
    mat_bindings_t *bindings = include_bindings ? mat_bindings_from_matrix(matrix) : NULL;
    bool all_unset = false;
    const TeX_model_node_t *assignments = include_bindings ? matrix_bindings(build, bindings, &all_unset) : NULL;
    if (include_bindings && !all_unset) {
        const TeX_model_node_t *parts[] = {body, assignments};
        body = notation_build_node(build,
            (TeX_node_view_t){.kind = TeX_NODE_BOUND, .child_count = assignments ? 2u : 1u}, parts);
    }
    string_t *result = mat_bindings_count(bindings) && !assignments ? NULL : notation_build_render(build, body);
    mat_bindings_free(bindings);
    notation_build_free(build);
    mat_expr_beautification_clear(&view);
    return result;
}
