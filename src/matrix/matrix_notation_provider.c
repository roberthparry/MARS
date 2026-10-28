/**
 * @file matrix_notation_provider.c
 * @brief Expose native matrix dimensions and entries as semantic callback data.
 *
 * Matrix storage and element interpretation stay in this module. The resulting
 * context owns an immutable semantic snapshot and does not retain the matrix.
 */
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#include "matrix-t.h"
#include "TeX-t.h"

#define MARS_SHARED_EXPR_NOTATION_INTERNAL_ACCESS
#include "internal/expr_notation_internal.h"
#define MARS_SHARED_MATRIX_NOTATION_INTERNAL_ACCESS
#include "internal/matrix_notation_internal.h"

/* Traverse row-major entries once and preserve their native scalar semantics. */
const TeX_model_node_t *notation_build_matrix(notation_build_t *build, const matrix_t *matrix)
{
    size_t rows = matrix ? mat_get_row_count(matrix) : 0u, columns = matrix ? mat_get_col_count(matrix) : 0u;
    if (!build || !rows || !columns || rows > SIZE_MAX / columns || rows * columns > 65536u) {
        errno = EINVAL;
        return NULL;
    }
    size_t count = rows * columns;
    const TeX_model_node_t **entries = calloc(count, sizeof *entries);
    if (!entries)
        return NULL;
    bool valid = true;
    for (size_t i = 0; i < count && valid; ++i) {
        if (mat_typeof(matrix) == MAT_TYPE_EXPR) {
            expr_t *entry = NULL;
            mat_get(matrix, i / columns, i % columns, &entry);
            expr_t *display = entry ? expr_beautify(entry) : NULL;
            entries[i] = entry ? (display ? notation_build_expr(build, display) : NULL)
                               : notation_build_number(build, NUM_ZERO);
            expr_free(display);
        } else {
            number_t entry = mat_get_num(matrix, i / columns, i % columns);
            entries[i] = notation_build_number(build, entry);
            num_destroy(&entry);
        }
        valid = entries[i] != NULL;
    }
    const TeX_model_node_t *node = valid ? notation_build_node(build, (TeX_node_view_t){
        .kind = TeX_NODE_MATRIX, .child_count = count, .rows = rows, .columns = columns}, entries) : NULL;
    free(entries);
    return node;
}

/* Return a semantic matrix snapshot with ownership independent of the source. */
TeX_context_t *mat_TeX_context(const matrix_t *matrix)
{
    notation_build_t *build = notation_build_new();
    const TeX_model_node_t *root = notation_build_matrix(build, matrix);
    TeX_context_t *context = notation_build_context(build, root);
    notation_build_free(build);
    return context;
}
