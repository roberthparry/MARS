#include <stdlib.h>
#include <string.h>

#include "equation.h"
#include "ustring.h"
#define MARS_SHARED_EQUATION_INTERNAL_ACCESS
#include "internal/equation_internal.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"

static const equation_t *ordered_solution_at(const equation_solutions_t *solutions, const size_t *order, size_t i)
{
    return equ_solutions_at(solutions, order ? order[i] : i);
}

/* Render lengthy exact root sets with shared definitions and symmetric root pairs. */
string_t *equ_solutions_compact_text(const equation_solutions_t *solutions, const size_t *order, style_t output_style)
{
    bool TeX = output_style == style_LATEX;
    bool bound = output_style == style_EXPRESSION;
    size_t count = equ_solutions_count(solutions);
    size_t length = 0u;
    expr_t *rows[16] = {0};
    const expr_t *roots[32] = {0};
    string_t *rendered[16] = {0};
    expr_function_temporaries_t *plan = NULL;
    string_t *definitions = NULL;
    string_t *lhs = NULL;
    string_t *output = NULL;
    bool complete = false;
    style_t style = TeX ? style_LATEX : style_UNBOUND;

    if (count < 2u || count > 16u)
        return NULL;
    const expr_t *variable = equ_lhs(ordered_solution_at(solutions, order, 0u));
    for (size_t i = 0u; i < count; ++i) {
        const equation_t *solution = ordered_solution_at(solutions, order, i);
        if (!expr_struct_eq(variable, equ_lhs(solution)))
            return NULL;
        number_t value = expr_eval(equ_rhs(solution));
        bool finite = num_is_finite(value);
        num_destroy(&value);
        if (!finite)
            return NULL;
        char *text = expr_to_string(equ_rhs(solution), style_UNBOUND);
        length += text ? strlen(text) : 0u;
        free(text);
    }
    if (length < (count % 2u ? 240u : 600u))
        return NULL;
    if (count % 2u) {
        rows[0] = expr_clone(equ_rhs(ordered_solution_at(solutions, order, 0u)));
        if (!rows[0])
            goto cleanup;
    }
    for (size_t i = count % 2u; i < count; i += 2u) {
        const expr_t *a = equ_rhs(ordered_solution_at(solutions, order, i));
        const expr_t *b = equ_rhs(ordered_solution_at(solutions, order, i + 1u));
        const expr_t *a_centre = NULL, *a_offset = NULL, *b_centre = NULL, *b_offset = NULL;
        bool a_sub = false, b_sub = false;
        if (expr_match_add_sub_expr(a, &a_centre, &a_offset, &a_sub) &&
            expr_match_add_sub_expr(b, &b_centre, &b_offset, &b_sub) && a_sub != b_sub &&
            expr_struct_eq(a_centre, b_centre) && expr_struct_eq(a_offset, b_offset)) {
            rows[i] = expr_clone(a_centre);
            rows[i + 1u] = expr_clone(a_offset);
            if (!rows[i] || !rows[i + 1u])
                goto cleanup;
            continue;
        }
        expr_t *sum = expr_simplify_owned(expr_add(a, b));
        expr_t *difference = expr_simplify_owned(expr_sub(a, b));
        rows[i] = expr_simplify_owned(expr_div_long(sum, 2L));
        rows[i + 1u] = expr_simplify_owned(expr_div_long(difference, 2L));
        expr_free(difference);
        expr_free(sum);
        /* Negate the entire offset, not its text: either sign describes the same pair. */
        char *offset_text = expr_to_string(rows[i + 1u], style_UNBOUND);
        if (offset_text && offset_text[0] == '-') {
            expr_t *positive = expr_simplify_owned(expr_neg(rows[i + 1u]));
            expr_free(rows[i + 1u]);
            rows[i + 1u] = positive;
        }
        free(offset_text);
        if (!rows[i] || !rows[i + 1u])
            goto cleanup;
    }
    for (size_t i = 0u; i < count; ++i) {
        roots[i] = rows[i];
        roots[count + i] = variable; /* Reserve the solution coordinate's name too. */
    }
    plan = expr_mathematical_temporaries_new(roots, 2u * count);
    definitions = expr_temporaries_math_definitions(plan, style);
    lhs = expr_temporaries_math_expression(plan, variable, style);
    if (!definitions || !string_length(definitions) || !lhs)
        goto cleanup;
    for (size_t i = 0u; i < count; ++i) {
        rendered[i] = expr_temporaries_math_expression(plan, rows[i], style);
        if (!rendered[i])
            goto cleanup;
    }
    output = string_new();
    if (!output)
        goto cleanup;
    if (string_append_cstr(output, TeX ? "\\begin{aligned} " : bound ? "{ " : "") < 0)
        goto cleanup;
    if (!bound && string_append_string(output, definitions) < 0)
        goto cleanup;
    for (size_t i = 0u; i < count;) {
        bool paired = i != 0u || count % 2u == 0u;
        const char *separator = TeX ? " \\\\\n" : bound ? (i ? ";\n  " : "") : "\n";
        if (string_append_format(output, "%s%s%s%s", separator, string_c_str(lhs),
                                 TeX ? " &= " : " = ", string_c_str(rendered[i])) < 0)
            goto cleanup;
        if (paired && string_append_format(output, "%s%s%s",
                                 TeX ? " \\pm \\left(" : " ± (", string_c_str(rendered[i + 1u]),
                                 TeX ? "\\right)" : ")") < 0)
            goto cleanup;
        i += paired ? 2u : 1u;
    }
    if (bound) {
        if (string_append_cstr(output, "\n  | ") < 0)
            goto cleanup;
        const char *cursor = string_c_str(definitions);
        while (*cursor) {
            const char *newline = strchr(cursor, '\n');
            size_t length = newline ? (size_t)(newline - cursor) : strlen(cursor);
            if (length && string_append_chars(output, cursor, length) < 0)
                goto cleanup;
            if (!newline)
                break;
            if (string_append_cstr(output, ",\n    ") < 0)
                goto cleanup;
            cursor = newline + 1;
        }
    }
    if (string_append_cstr(output, TeX ? " \\end{aligned}" : bound ? " }" : "") < 0)
        goto cleanup;
    complete = true;

cleanup:
    string_free(lhs);
    string_free(definitions);
    expr_function_temporaries_free(plan);
    for (size_t i = 0u; i < count; ++i) {
        string_free(rendered[i]);
        expr_free(rows[i]);
    }
    if (!complete) {
        string_free(output);
        output = NULL;
    }
    return output;
}
