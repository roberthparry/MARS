/**
 * @file equation_solution_display.c
 * @brief Compact presentation of equation solution families.
 *
 * Orders solutions and produces compact text with shared structure retained where possible. It works on solver
 * results rather than altering the mathematical equation to fit an output format.
 *
 * Structurally symmetric roots stay together even when numerical sorting separates their companion root.
 * The equation module plans paired roots and shared definitions. TeX owns the
 * layout syntax when the caller requests typeset output.
 */

#include <stdlib.h>
#include <string.h>

#include "equation-t.h"
#include "string-t.h"

#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"
#define MARS_SHARED_TeX_INTERNAL_ACCESS
#include "internal/TeX_internal.h"
#define MARS_EQUATION_INTERNAL_ACCESS
#include "equation_internal.h"

static const equation_t *ordered_solution_at(const equation_solutions_t *solutions, const size_t *order, size_t i)
{
    return equ_solutions_at(solutions, order ? order[i] : i);
}

static bool display_pair_parts(const expr_t *a, const expr_t *b, const expr_t **centre, const expr_t **offset)
{
    const expr_t *b_centre = NULL, *b_offset = NULL;
    bool a_sub = false, b_sub = false;
    return expr_match_add_sub_expr(a, centre, offset, &a_sub) &&
           expr_match_add_sub_expr(b, &b_centre, &b_offset, &b_sub) && a_sub != b_sub &&
           expr_struct_eq(*centre, b_centre) && expr_struct_eq(*offset, b_offset);
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
    const equation_t *ordered[16] = {0}, *arranged[16] = {0};
    bool used[16] = {false};
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
        ordered[i] = solution;
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
    /* At most sixteen displayed roots: bounded structural matching preserves genuine pairs before compaction. */
    for (size_t slot = count % 2u; slot < count; slot += 2u) {
        bool found = false;
        for (size_t a = 0u; !found && a < count; ++a) {
            if (used[a])
                continue;
            for (size_t b = a + 1u; !found && b < count; ++b) {
                const expr_t *centre = NULL, *offset = NULL;
                if (!used[b] && display_pair_parts(equ_rhs(ordered[a]), equ_rhs(ordered[b]), &centre, &offset)) {
                    arranged[slot] = ordered[a];
                    arranged[slot + 1u] = ordered[b];
                    used[a] = used[b] = found = true;
                }
            }
        }
        if (!found)
            break;
    }
    for (size_t i = 0u, slot = 0u; i < count; ++i) {
        if (used[i])
            continue;
        while (arranged[slot])
            ++slot;
        arranged[slot++] = ordered[i];
    }
    if (count % 2u) {
        rows[0] = expr_clone(equ_rhs(arranged[0]));
        if (!rows[0])
            goto cleanup;
    }
    for (size_t i = count % 2u; i < count; i += 2u) {
        const expr_t *a = equ_rhs(arranged[i]);
        const expr_t *b = equ_rhs(arranged[i + 1u]);
        const expr_t *a_centre = NULL, *a_offset = NULL;
        if (display_pair_parts(a, b, &a_centre, &a_offset)) {
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
    if (!equ_solution_pairs_text(plan, roots, count, style, rendered))
        goto cleanup;
    if (TeX) {
        const string_t *rows_to_align[16] = {0};
        size_t row_count = 0u;
        for (size_t i = 0u; i < count;) {
            rows_to_align[row_count++] = rendered[i];
            i += (i != 0u || count % 2u == 0u) ? 2u : 1u;
        }
        output = TeX_align_equation_rows(definitions, lhs, rows_to_align, row_count);
        complete = output != NULL;
        goto cleanup;
    }
    output = string_new();
    if (!output)
        goto cleanup;
    if (string_append_cstr(output, bound ? "{ " : "") < 0)
        goto cleanup;
    if (!bound && string_append_string(output, definitions) < 0)
        goto cleanup;
    for (size_t i = 0u; i < count;) {
        bool paired = i != 0u || count % 2u == 0u;
        const char *separator = bound ? (i ? ";\n  " : "") : "\n";
        if (string_append_format(output, "%s%s%s%s", separator, string_c_str(lhs),
                                 " = ", string_c_str(rendered[i])) < 0)
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
    if (string_append_cstr(output, bound ? " }" : "") < 0)
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
