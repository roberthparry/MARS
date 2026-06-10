#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "equation.h"

static string_t *equation_extract_binding_section(string_t *expr_text)
{
    const char *raw = expr_text ? string_c_str(expr_text) : NULL;
    const char *start;
    const char *end;
    char *copy;
    string_t *out;
    size_t len;

    if (!raw)
        return NULL;

    start = strchr(raw, '|');
    end = strrchr(raw, '}');
    if (!start || !end || start >= end)
        return NULL;

    ++start;
    while (start < end && (*start == ' ' || *start == '\t' || *start == '\n'))
        ++start;
    while (end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n'))
        --end;

    len = (size_t)(end - start);
    if (len == 0u)
        return NULL;

    copy = malloc(len + 1u);
    if (!copy)
        return NULL;
    memcpy(copy, start, len);
    copy[len] = '\0';
    out = string_new_with(copy);
    free(copy);
    return out;
}

static string_t *equation_binding_section(const equation_t *equation)
{
    expr_t *combined;
    string_t *combined_text;
    string_t *bindings;

    combined = expr_add(equation_lhs(equation), equation_rhs(equation));
    if (!combined)
        return NULL;

    combined_text = expr_to_text(combined, style_EXPRESSION);
    expr_free(combined);
    if (!combined_text)
        return NULL;

    bindings = equation_extract_binding_section(combined_text);
    string_free(combined_text);
    return bindings;
}

static string_t *equation_to_text_expression(const equation_t *equation)
{
    string_t *lhs = expr_to_text(equation_lhs(equation), style_UNBOUND);
    string_t *rhs = expr_to_text(equation_rhs(equation), style_UNBOUND);
    string_t *bindings = equation_binding_section(equation);
    string_t *out = NULL;

    if (!lhs || !rhs)
        goto cleanup;

    out = bindings ? string_sprintf("{ %S = %S | %S }", lhs, rhs, bindings)
                   : string_sprintf("{ %S = %S }", lhs, rhs);

cleanup:
    string_free(bindings);
    string_free(rhs);
    string_free(lhs);
    return out;
}

static string_t *equation_to_text_unbound(const equation_t *equation)
{
    string_t *lhs = expr_to_text(equation_lhs(equation), style_UNBOUND);
    string_t *rhs = expr_to_text(equation_rhs(equation), style_UNBOUND);
    string_t *out = NULL;

    if (!lhs || !rhs)
        goto cleanup;

    out = string_sprintf("%S = %S", lhs, rhs);

cleanup:
    string_free(rhs);
    string_free(lhs);
    return out;
}

static string_t *equation_to_text_tex(const equation_t *equation)
{
    string_t *lhs = expr_to_text(equation_lhs(equation), style_TEX);
    string_t *rhs = expr_to_text(equation_rhs(equation), style_TEX);
    string_t *out = NULL;

    if (!lhs || !rhs)
        goto cleanup;

    out = string_sprintf("%S = %S", lhs, rhs);

cleanup:
    string_free(rhs);
    string_free(lhs);
    return out;
}

string_t *equation_to_text(const equation_t *equation, style_t style)
{
    if (!equation)
        return string_new_with("NULL");

    if (style == style_TEX)
        return equation_to_text_tex(equation);
    if (style == style_UNBOUND)
        return equation_to_text_unbound(equation);

    return equation_to_text_expression(equation);
}

void equation_print(const equation_t *equation)
{
    string_t *text = equation_to_text(equation, style_EXPRESSION);

    fputs(text ? string_c_str(text) : "NULL", stdout);
    fputc('\n', stdout);
    string_free(text);
}
