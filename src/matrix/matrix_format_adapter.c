/**
 * @file matrix_format_adapter.c
 * @brief Module-owned matrix presentations for typed brace formatting.
 *
 * Borrows matrix pointers, validates native style names and delegates to the matrix module's public renderer.
 * A minimal perfect hash shares eight style entries between validation and rendering without empty slots.
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
        {"pretty_layout",     MAT_STRING_LAYOUT_PRETTY},
        {"scientific_layout", MAT_STRING_LAYOUT_SCIENTIFIC},
        {"pretty",            MAT_STRING_INLINE_PRETTY},
        {"layout",            MAT_STRING_EXPRESSION_LAYOUT},
        {"expression",        MAT_STRING_EXPRESSION},
        {"function",          MAT_STRING_FUNCTION},
        {"scientific",        MAT_STRING_INLINE_SCIENTIFIC},
        {"latex",             MAT_STRING_LATEX},
    };
    _Static_assert(sizeof(styles) / sizeof(*styles) == 8u, "Regenerate the minimal perfect hash when adding styles.");
    if (!*name)
        return MAT_STRING_EXPRESSION;
    uint32_t hash = 95u;
    for (const unsigned char *at = (const unsigned char *)name; *at; ++at)
        hash = (hash ^ *at) * UINT32_C(16777619);
    size_t slot = (hash ^ (hash >> 16u)) & 7u;
    return strcmp(name, styles[slot].name) ? -1 : (int)styles[slot].style;
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
