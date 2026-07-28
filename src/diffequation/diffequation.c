#include <stdlib.h>

#include "diffequation.h"
#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_internal.h"

diffequ_t *de_new_owned(equation_t *equation)
{
    diffequ_t *de;

    if (!equation)
        return NULL;

    de = calloc(1u, sizeof(*de));
    if (!de)
        return NULL;
    de->equation = equation;
    de->equation_text = string_new();
    de->independent_text = string_new();
    de->constant_text = string_new();
    if (!de->equation_text ||
        !de->independent_text ||
        !de->constant_text) {
        string_free(de->constant_text);
        string_free(de->independent_text);
        string_free(de->equation_text);
        free(de);
        return NULL;
    }
    return de;
}

diffequ_t *de_new(const equation_t *equation)
{
    equation_t *copy;
    diffequ_t *de;

    if (!equation)
        return NULL;

    copy = equ_new(equ_lhs(equation), equ_rhs(equation));
    if (!copy)
        return NULL;

    de = de_new_owned(copy);
    if (!de)
        equ_free(copy);
    return de;
}

void de_free(diffequ_t *de)
{
    if (!de)
        return;

    for (size_t i = 0u; i < de->condition_count; ++i) {
        expr_free(de->condition_points[i]);
        string_free(de->condition_texts[i]);
        equ_free(de->conditions[i]);
    }
    free(de->condition_points);
    free(de->condition_texts);
    free(de->conditions);

    for (size_t i = 0u; i < de->independent_count; ++i)
        expr_free(de->independent_vars[i]);
    free(de->independent_vars);

    string_free(de->constant_text);
    string_free(de->independent_text);
    string_free(de->equation_text);
    expr_bindings_free(de->constants);
    equ_free(de->equation);
    free(de);
}

const equation_t *de_equation(const diffequ_t *de)
{
    return de ? de->equation : NULL;
}

size_t de_independent_count(const diffequ_t *de)
{
    return de ? de->independent_count : 0u;
}

const expr_t *de_independent_at(const diffequ_t *de, size_t index)
{
    if (!de || index >= de->independent_count)
        return NULL;
    return de->independent_vars[index];
}

expr_bindings_t *de_constants(const diffequ_t *de)
{
    return de ? de->constants : NULL;
}

expr_t *de_constant(const diffequ_t *de, const char *name)
{
    return de && de->constants
        ? expr_bindings_get(de->constants, name)
        : NULL;
}

size_t de_condition_count(const diffequ_t *de)
{
    return de ? de->condition_count : 0u;
}

const equation_t *de_condition_at(const diffequ_t *de, size_t index)
{
    if (!de || index >= de->condition_count)
        return NULL;
    return de->conditions[index];
}
