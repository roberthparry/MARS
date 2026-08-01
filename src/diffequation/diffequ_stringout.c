#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diffequation.h"
#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_internal.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"

/* TeX commands are considerably wider in source than when rendered. */
static const size_t de_tex_line_limit = 180u;

static int de_append_superscript(string_t *out, size_t value)
{
    static const char *const digits[] = {
        "⁰", "¹", "²", "³", "⁴",
        "⁵", "⁶", "⁷", "⁸", "⁹"
    };
    char text[3u * sizeof(size_t) + 1u];

    if (snprintf(text, sizeof(text), "%zu", value) < 0)
        return -1;
    for (const char *cursor = text; *cursor; ++cursor) {
        if (string_append_cstr(out, digits[(size_t)(*cursor - '0')]) != 0)
            return -1;
    }
    return 0;
}

static int de_is_derivative_letter(char value)
{
    return (value >= 'A' && value <= 'Z') ||
           (value >= 'a' && value <= 'z');
}

static int de_append_standard_derivative(
    string_t *out,
    const char *suffix,
    size_t order,
    const char *dependent,
    size_t dependent_length,
    bool partial)
{
    if (string_append_cstr(out, partial ? "∂" : "d") != 0)
        return -1;
    if (order > 1u && de_append_superscript(out, order) != 0)
        return -1;
    if (string_append_chars(out, dependent, dependent_length) != 0 ||
        string_append_char(out, '/') != 0)
        return -1;

    if (!partial) {
        if (string_append_char(out, 'd') != 0 ||
            string_append_char(out, suffix[0]) != 0)
            return -1;
        return order > 1u ? de_append_superscript(out, order) : 0;
    }

    for (size_t index = order; index > 0u;) {
        const char variable = suffix[index - 1u];
        size_t multiplicity = 1u;

        while (index > multiplicity &&
               suffix[index - multiplicity - 1u] == variable)
            ++multiplicity;
        if (string_append_cstr(out, "∂") != 0 ||
            string_append_char(out, variable) != 0)
            return -1;
        if (multiplicity > 1u &&
            de_append_superscript(out, multiplicity) != 0)
            return -1;
        index -= multiplicity;
    }
    return 0;
}

static string_t *de_derivatives_to_standard_text(
    const string_t *source,
    bool partial)
{
    const char *text = source ? string_c_str(source) : "";
    string_t *out = string_new();
    size_t index = 0u;

    if (!out)
        return NULL;
    while (text[index]) {
        size_t suffix_end;
        size_t dependent_start;
        size_t dependent_end;

        if (text[index] != 'D' ||
            !de_is_derivative_letter(text[index + 1u])) {
            if (string_append_char(out, text[index++]) != 0)
                goto fail;
            continue;
        }

        suffix_end = index + 1u;
        while (de_is_derivative_letter(text[suffix_end]))
            ++suffix_end;
        if (text[suffix_end] != '(') {
            if (string_append_char(out, text[index++]) != 0)
                goto fail;
            continue;
        }

        dependent_start = suffix_end + 1u;
        dependent_end = dependent_start;
        while (text[dependent_end] &&
               text[dependent_end] != ')' &&
               text[dependent_end] != '(' &&
               text[dependent_end] != ' ' &&
               text[dependent_end] != '\t' &&
               text[dependent_end] != '\r' &&
               text[dependent_end] != '\n')
            ++dependent_end;
        if (dependent_end == dependent_start ||
            text[dependent_end] != ')') {
            if (string_append_char(out, text[index++]) != 0)
                goto fail;
            continue;
        }

        if (de_append_standard_derivative(
                out,
                text + index + 1u,
                suffix_end - index - 1u,
                text + dependent_start,
                dependent_end - dependent_start,
                partial) != 0)
            goto fail;
        index = dependent_end + 1u;
    }
    return out;

fail:
    string_free(out);
    return NULL;
}

static string_t *de_conditions_to_text(
    const diffequ_t *de,
    bool partial_derivatives)
{
    string_t *out = string_new();

    if (!out)
        return NULL;

    for (size_t i = 0u; i < de->condition_count; ++i) {
        string_t *condition = de_derivatives_to_standard_text(
            de->condition_texts[i], partial_derivatives);

        if (!condition)
            goto fail;
        if (i > 0u && string_append_cstr(out, ", ") != 0)
            goto condition_fail;
        if (string_append_string(out, condition) != 0)
            goto condition_fail;
        string_free(condition);
        continue;

condition_fail:
        string_free(condition);
        goto fail;
    }
    return out;

fail:
    string_free(out);
    return NULL;
}

static string_t *de_to_expression_text(const diffequ_t *de)
{
    string_t *canonical_equation;
    string_t *equation;
    string_t *conditions;
    string_t *out;
    bool partial_derivatives;

    if (!de || !de->equation)
        return string_new_with("NULL");

    partial_derivatives = de->independent_count > 1u;
    canonical_equation = string_length(de->equation_text) > 0u
        ? string_new_with(string_c_str(de->equation_text))
        : equ_to_text(de->equation, style_UNBOUND);
    equation = de_derivatives_to_standard_text(
        canonical_equation, partial_derivatives);
    string_free(canonical_equation);
    conditions = de_conditions_to_text(de, partial_derivatives);
    if (!equation || !conditions) {
        string_free(conditions);
        string_free(equation);
        return NULL;
    }

    out = string_sprintf(
        "{ %S | %S; %S; %S }",
        equation,
        de->independent_text,
        de->constant_text,
        conditions);

    string_free(conditions);
    string_free(equation);
    return out;
}

static char *de_expr_to_unbound_tex(
    const expr_t *expr,
    bool partial_derivatives)
{
    const char *symbol_name;
    char *tex;

    symbol_name = expr_symbol_name(expr);
    if (symbol_name) {
        expr_t *symbol =
            expr_new_named_var(NUM_NAN, symbol_name);

        tex = symbol
            ? expr_to_tex_body_wrapped(symbol, de_tex_line_limit)
            : NULL;
        expr_free(symbol);
        return tex;
    }
    return partial_derivatives
        ? expr_to_tex_body_wrapped_with_partials(
              expr, de_tex_line_limit)
        : expr_to_tex_body_wrapped_with_totals(
              expr, de_tex_line_limit);
}

static string_t *de_to_tex(const diffequ_t *de)
{
    char *lhs;
    char *rhs;
    string_t *out;

    if (!de || !de->equation)
        return string_new_with("NULL");

    lhs = de_expr_to_unbound_tex(
        equ_lhs(de->equation), de->independent_count > 1u);
    rhs = de_expr_to_unbound_tex(
        equ_rhs(de->equation), de->independent_count > 1u);
    out = lhs && rhs ? string_sprintf("%s = %s", lhs, rhs) : NULL;
    free(rhs);
    free(lhs);
    return out;
}

string_t *de_to_text(const diffequ_t *de, style_t style)
{
    if (!de)
        return string_new_with("NULL");
    if (style == style_UNBOUND)
        return equ_to_text(de->equation, style_UNBOUND);
    if (style == style_TEX)
        return de_to_tex(de);
    return de_to_expression_text(de);
}

char *de_to_string(const diffequ_t *de, style_t style)
{
    string_t *text = de_to_text(de, style);
    char *out = text ? strdup(string_c_str(text)) : NULL;

    string_free(text);
    return out;
}
