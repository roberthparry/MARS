/**
 * @file equation_format_adapter.c
 * @brief Module-owned algebraic equation presentations for typed brace formatting.
 *
 * Borrows equation pointers and delegates to the equation module's native renderers without evaluation,
 * substitution or mathematical rewriting. A sorted style table provides binary lookup during validation and rendering.
 */
#include <string.h>

#include "format-t.h"
#include "equation-t.h"

static int equ_fmt_style(const char *name)
{
    static const struct {
        const char *name;
        style_t style;
    } styles[] = {
        {"expression", style_EXPRESSION},
        {"function",   style_FUNCTION},
        {"latex",      style_LATEX},
        {"unbound",    style_UNBOUND},
    };
    if (!*name)
        return style_EXPRESSION;
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

static bool equ_fmt_validate(const fmt_options_t *options, fmt_error_t *error)
{
    if (equ_fmt_style(options->presentation) < 0 || options->precision >= 0) {
        if (error)
            *error = (fmt_error_t){FMT_ERROR_TYPE, SIZE_MAX, SIZE_MAX, "unsupported equation presentation or precision"};
        return false;
    }
    return true;
}

static string_t *equ_fmt_render(const void *data, const fmt_options_t *options, fmt_error_t *error)
{
    const equation_t *equation;
    memcpy(&equation, data, sizeof(equation));
    if (!equation) {
        if (error)
            *error = (fmt_error_t){FMT_ERROR_ARGUMENT, SIZE_MAX, SIZE_MAX, "equation pointer is NULL"};
        return NULL;
    }
    return equ_to_text(equation, (style_t)equ_fmt_style(options->presentation));
}

static const fmt_adapter_t equ_fmt_adapter = {
    .validate = equ_fmt_validate,
    .render = equ_fmt_render,
    .numeric = false,
};

/* Borrow an equation with its native symbolic rendering adapter, retaining no equation nodes. */
fmt_arg_t fmt_arg_equation(const equation_t *value)
{
    return fmt_arg_adapter(&equ_fmt_adapter, &value, sizeof(value));
}
