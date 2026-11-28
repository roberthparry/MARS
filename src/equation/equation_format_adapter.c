/**
 * @file equation_format_adapter.c
 * @brief Module-owned algebraic equation presentations for typed brace formatting.
 *
 * Borrows equation pointers and delegates to the equation module's native renderers without evaluation,
 * substitution or mathematical rewriting. Style-name lengths form a minimal perfect hash with no empty slots.
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
