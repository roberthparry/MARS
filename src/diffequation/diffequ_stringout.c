#include <stdlib.h>
#include <string.h>

#include "diffequation.h"
#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_internal.h"

static string_t *de_conditions_to_text(const diffequ_t *de)
{
    string_t *out = string_new();

    if (!out)
        return NULL;

    for (size_t i = 0u; i < de->condition_count; ++i) {
        if (i > 0u && string_append_cstr(out, ", ") != 0)
            goto fail;
        if (string_append_string(out, de->condition_texts[i]) != 0)
            goto fail;
    }
    return out;

fail:
    string_free(out);
    return NULL;
}

static string_t *de_to_expression_text(const diffequ_t *de)
{
    string_t *equation;
    string_t *conditions;
    string_t *out;

    if (!de || !de->equation)
        return string_new_with("NULL");

    equation = string_length(de->equation_text) > 0u
        ? string_new_with(string_c_str(de->equation_text))
        : equ_to_text(de->equation, style_UNBOUND);
    conditions = de_conditions_to_text(de);
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

string_t *de_to_text(const diffequ_t *de, style_t style)
{
    if (!de)
        return string_new_with("NULL");
    if (style == style_UNBOUND)
        return equ_to_text(de->equation, style_UNBOUND);
    if (style == style_TEX)
        return equ_to_text(de->equation, style_TEX);
    return de_to_expression_text(de);
}

char *de_to_string(const diffequ_t *de, style_t style)
{
    string_t *text = de_to_text(de, style);
    char *out = text ? strdup(string_c_str(text)) : NULL;

    string_free(text);
    return out;
}
