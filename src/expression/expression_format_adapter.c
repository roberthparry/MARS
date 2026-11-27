/**
 * @file expression_format_adapter.c
 * @brief Module-owned symbolic expression presentations for typed brace formatting.
 *
 * Borrows expression pointers and delegates to the expression module's native renderers without evaluation,
 * substitution or mathematical rewriting. A sorted style table provides binary lookup during validation and rendering.
 */
#include <string.h>

#include "format-t.h"
#include "expression-t.h"

static int expr_fmt_style(const char *name)
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

static bool expr_fmt_validate(const fmt_options_t *options, fmt_error_t *error)
{
    if (expr_fmt_style(options->presentation) < 0 || options->precision >= 0) {
        if (error)
            *error = (fmt_error_t){FMT_ERROR_TYPE, SIZE_MAX, SIZE_MAX, "unsupported expression presentation or precision"};
        return false;
    }
    return true;
}

static string_t *expr_fmt_render(const void *data, const fmt_options_t *options, fmt_error_t *error)
{
    const expr_t *expression;
    memcpy(&expression, data, sizeof(expression));
    if (!expression) {
        if (error)
            *error = (fmt_error_t){FMT_ERROR_ARGUMENT, SIZE_MAX, SIZE_MAX, "expression pointer is NULL"};
        return NULL;
    }
    return expr_to_text(expression, (style_t)expr_fmt_style(options->presentation));
}

static const fmt_adapter_t expr_fmt_adapter = {
    .validate = expr_fmt_validate,
    .render = expr_fmt_render,
    .numeric = false,
};

/* Borrow an expression with its native symbolic rendering adapter, retaining no expression nodes. */
fmt_arg_t fmt_arg_expression(const expr_t *value)
{
    return fmt_arg_adapter(&expr_fmt_adapter, &value, sizeof(value));
}
