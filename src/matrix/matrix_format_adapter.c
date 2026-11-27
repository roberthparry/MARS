/**
 * @file matrix_format_adapter.c
 * @brief Module-owned matrix presentations for typed brace formatting.
 *
 * Borrows matrix pointers, validates native style names and delegates to the matrix module's public renderer.
 * The style table is sorted for binary lookup; entries are shared by validation and rendering so they cannot drift.
 * No entry values, bindings or symbolic algebra are evaluated or rewritten here.
 */
#include <string.h>

#include "format-t.h"
#include "matrix-t.h"

static int mat_fmt_style(const char *name)
{
    static const struct {
        const char *name;
        mat_string_style_t style;
    } styles[] = {
        {"expression", MAT_STRING_EXPRESSION},
        {"function",   MAT_STRING_FUNCTION},
        {"latex",      MAT_STRING_LATEX},
        {"layout",     MAT_STRING_EXPRESSION_LAYOUT},
        {"pretty",     MAT_STRING_INLINE_PRETTY},
        {"scientific", MAT_STRING_INLINE_SCIENTIFIC},
    };
    if (!*name)
        return MAT_STRING_EXPRESSION;
    size_t first = 0, last = sizeof(styles) / sizeof(*styles);
    while (first < last) {
        size_t middle = first + (last - first) / 2u;
        int order = strcmp(name, styles[middle].name);
        if (!order)
            return styles[middle].style;
        if (order < 0)
            last = middle;
        else
            first = middle + 1u;
    }
    return -1;
}

static bool mat_fmt_validate(const fmt_options_t *options, fmt_error_t *error)
{
    if (mat_fmt_style(options->presentation) < 0 || options->precision >= 0) {
        if (error)
            *error = (fmt_error_t){FMT_ERROR_TYPE, SIZE_MAX, SIZE_MAX, "unsupported matrix presentation or precision"};
        return false;
    }
    return true;
}

static string_t *mat_fmt_render(const void *data, const fmt_options_t *options, fmt_error_t *error)
{
    const matrix_t *matrix;
    memcpy(&matrix, data, sizeof(matrix));
    if (!matrix) {
        if (error)
            *error = (fmt_error_t){FMT_ERROR_ARGUMENT, SIZE_MAX, SIZE_MAX, "matrix pointer is NULL"};
        return NULL;
    }
    return mat_to_text(matrix, (mat_string_style_t)mat_fmt_style(options->presentation));
}

static const fmt_adapter_t mat_fmt_adapter = {
    .validate = mat_fmt_validate,
    .render = mat_fmt_render,
    .numeric = false,
};

/* Borrow a matrix with its native presentation adapter, retaining no entries or binding values. */
fmt_arg_t fmt_arg_matrix(const matrix_t *value)
{
    return fmt_arg_adapter(&mat_fmt_adapter, &value, sizeof(value));
}
