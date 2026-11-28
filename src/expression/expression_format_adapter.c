/**
 * @file expression_format_adapter.c
 * @brief Module-owned symbolic expression presentations for typed brace formatting.
 *
 * Borrows expression pointers and delegates to the expression module's native renderers without evaluation,
 * substitution or mathematical rewriting. Style-name lengths form a minimal perfect hash with no empty slots.
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
        {"function",   style_FUNCTION},
        {"latex",      style_LATEX},
        {"expression", style_EXPRESSION},
        {"unbound",    style_UNBOUND},
    };
    if (!*name)
        return style_EXPRESSION;
    _Static_assert(sizeof(styles) / sizeof(*styles) == 4u, "Regenerate the minimal perfect hash when adding styles.");
    size_t slot = strlen(name) & 3u;
    return strcmp(name, styles[slot].name) ? -1 : (int)styles[slot].style;
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
