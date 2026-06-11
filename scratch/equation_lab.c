#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "equation.h"
#include "expression/expr_stringin_internal.h"
#include "expression.h"
#include "internal/expr_internal.h"
#include "number.h"
#include "ustring.h"

static char *dup_string(const char *text)
{
    size_t n;
    char *copy;

    if (!text)
        text = "";
    n = strlen(text);
    copy = malloc(n + 1u);
    if (copy)
        memcpy(copy, text, n + 1u);
    return copy;
}

static char *equation_text_dup(const equation_t *equation, style_t style)
{
    string_t *text = equation_to_text(equation, style);
    char *copy = text ? dup_string(string_c_str(text)) : NULL;

    string_free(text);
    return copy;
}

static char *expr_text_dup(const expr_t *expr, style_t style)
{
    string_t *text = expr_to_text(expr, style);
    char *copy = text ? dup_string(string_c_str(text)) : NULL;

    string_free(text);
    return copy;
}

static char *number_text_dup(number_t value)
{
    string_t *text = num_to_string(value);
    char *copy = text ? dup_string(string_c_str(text)) : NULL;

    string_free(text);
    return copy;
}

static char *expr_tex_body_dup(const expr_t *expr)
{
    char *body = NULL;
    char *bindings = NULL;

    if (!expr)
        return NULL;
    if (expr_to_tex_parts(expr, &body, &bindings) == 0) {
        free(bindings);
        return body;
    }
    free(body);
    free(bindings);
    return expr_text_dup(expr, style_TEX);
}

static expr_t *clone_expr_local(const expr_t *expr)
{
    number_t seed = num_new();
    expr_t *needle = expr_new_named_var(seed, "__mars_equation_clone__");
    expr_t *replacement = expr_new_const(NUM_ZERO);
    expr_t *copy = NULL;

    num_destroy(&seed);
    if (needle && replacement)
        copy = expr_substitute(expr, needle, replacement);

    expr_free(replacement);
    expr_free(needle);
    return copy;
}

static expr_t *expanded_display_expr(const expr_t *expr);

static expr_t *expanded_display_product(const expr_t *left, const expr_t *right)
{
    expr_t *left_expr;
    expr_t *right_expr;
    expr_t *out;

    if (!left || !right)
        return NULL;

    if (expr_is_addsub(left) && expr_is_addsub(right)) {
        left_expr = expanded_display_expr(left);
        right_expr = expanded_display_expr(right);
        if (!left_expr || !right_expr) {
            expr_free(left_expr);
            expr_free(right_expr);
            return NULL;
        }

        out = expr_mul(left_expr, right_expr);
        expr_free(left_expr);
        expr_free(right_expr);
        return out;
    }

    if (expr_is_op(left, &ops_add)) {
        expr_t *first = expanded_display_product(left->a, right);
        expr_t *second = expanded_display_product(left->b, right);

        if (!first || !second) {
            expr_free(first);
            expr_free(second);
            return NULL;
        }

        out = expr_add(first, second);
        expr_free(first);
        expr_free(second);
        return out;
    }

    if (expr_is_op(left, &ops_sub)) {
        expr_t *first = expanded_display_product(left->a, right);
        expr_t *second = expanded_display_product(left->b, right);

        if (!first || !second) {
            expr_free(first);
            expr_free(second);
            return NULL;
        }

        out = expr_sub(first, second);
        expr_free(first);
        expr_free(second);
        return out;
    }

    if (expr_is_addsub(right))
        return expanded_display_product(right, left);

    left_expr = expanded_display_expr(left);
    right_expr = expanded_display_expr(right);
    if (!left_expr || !right_expr) {
        expr_free(left_expr);
        expr_free(right_expr);
        return NULL;
    }

    out = expr_mul(left_expr, right_expr);
    expr_free(left_expr);
    expr_free(right_expr);
    return out;
}

static expr_t *expanded_display_expr(const expr_t *expr)
{
    expr_t *left;
    expr_t *right;
    expr_t *out;

    if (!expr)
        return NULL;

    if (expr_is_op(expr, &ops_add)) {
        left = expanded_display_expr(expr->a);
        right = expanded_display_expr(expr->b);
        if (!left || !right) {
            expr_free(left);
            expr_free(right);
            return NULL;
        }
        out = expr_add(left, right);
        expr_free(left);
        expr_free(right);
        return out;
    }

    if (expr_is_op(expr, &ops_sub)) {
        left = expanded_display_expr(expr->a);
        right = expanded_display_expr(expr->b);
        if (!left || !right) {
            expr_free(left);
            expr_free(right);
            return NULL;
        }
        out = expr_sub(left, right);
        expr_free(left);
        expr_free(right);
        return out;
    }

    if (expr_is_op(expr, &ops_mul))
        return expanded_display_product(expr->a, expr->b);

    return clone_expr_local(expr);
}

static expr_t *display_simplified_expr(const expr_t *expr)
{
    expr_t *expanded;
    expr_t *simplified;

    if (!expr)
        return NULL;

    expanded = expanded_display_expr(expr);
    if (!expanded)
        return expr_simplify((expr_t *)expr);

    simplified = expr_simplify(expanded);
    if (!simplified)
        return expanded;

    expr_free(expanded);
    return simplified;
}

static equation_t *display_simplified_equation(const equation_t *equation)
{
    expr_t *lhs;
    expr_t *rhs;
    equation_t *out;

    if (!equation)
        return NULL;

    lhs = display_simplified_expr(equation_lhs(equation));
    rhs = display_simplified_expr(equation_rhs(equation));
    if (!lhs || !rhs) {
        expr_free(lhs);
        expr_free(rhs);
        return NULL;
    }

    out = equation_new(lhs, rhs);
    expr_free(rhs);
    expr_free(lhs);
    return out;
}

static expr_t *substitute_bound_constants(const expr_t *expr,
                                          expr_bindings_t *bindings)
{
    expr_t *current;

    if (!expr)
        return NULL;

    current = expr_simplify((expr_t *)expr);
    if (!current)
        return NULL;

    if (!bindings)
        return current;

    for (size_t i = 0u; i < bindings->count; ++i) {
        expr_binding_entry_t *entry = &bindings->entries[i];
        number_t value;
        number_t needle_seed;
        expr_t *needle;
        expr_t *replacement;
        expr_t *next;
        const char *name;

        if (!entry->is_constant)
            continue;

        name = entry->name ? string_c_str(entry->name) : NULL;
        if (!name || !name[0])
            continue;

        value = expr_eval(entry->expr);
        if (!num_is_finite(value)) {
            num_destroy(&value);
            continue;
        }

        needle_seed = num_new();
        needle = expr_new_named_var(needle_seed, name);
        num_destroy(&needle_seed);
        replacement = expr_new_const(value);
        num_destroy(&value);
        if (!needle || !replacement) {
            expr_free(needle);
            expr_free(replacement);
            continue;
        }

        next = expr_substitute(current, needle, replacement);
        expr_free(needle);
        expr_free(replacement);
        if (!next)
            continue;

        expr_free(current);
        current = expr_simplify(next);
        expr_free(next);
        if (!current)
            return NULL;
    }

    return current;
}

static equation_t *solution_with_bound_constants(const equation_t *solution,
                                                 expr_bindings_t *bindings)
{
    expr_t *lhs;
    expr_t *rhs;
    equation_t *out;

    if (!solution)
        return NULL;

    lhs = substitute_bound_constants(equation_lhs(solution), bindings);
    rhs = substitute_bound_constants(equation_rhs(solution), bindings);
    if (!lhs || !rhs) {
        expr_free(lhs);
        expr_free(rhs);
        return NULL;
    }

    out = equation_new(lhs, rhs);
    expr_free(rhs);
    expr_free(lhs);
    return out;
}

static const char *nth_variable_binding_name(expr_bindings_t *bindings,
                                             size_t wanted_index)
{
    size_t seen = 0u;

    if (!bindings)
        return NULL;

    for (size_t i = 0u; i < bindings->count; ++i) {
        expr_binding_entry_t *entry = &bindings->entries[i];

        if (entry->is_constant)
            continue;
        if (seen == wanted_index)
            return string_c_str(entry->name);
        ++seen;
    }

    return NULL;
}

static const char *solution_display_name(expr_bindings_t *bindings,
                                         const char *symbolic_variable,
                                         const equation_solutions_t *solutions,
                                         const char *status,
                                         size_t index)
{
    const char *binding_name;

    if (status && strcmp(status, "symbolic") == 0 &&
        equation_solutions_count(solutions) > 0u &&
        symbolic_variable && symbolic_variable[0])
        return symbolic_variable;

    binding_name = nth_variable_binding_name(bindings, index);
    return binding_name && binding_name[0] ? binding_name : NULL;
}

static void print_solutions(const equation_solutions_t *solutions,
                            expr_bindings_t *bindings,
                            const char *symbolic_variable,
                            const char *status)
{
    size_t count = equation_solutions_count(solutions);

    if (count == 0u) {
        printf("solutions   \n");
        return;
    }

    for (size_t i = 0u; i < count; ++i) {
        const char *name = solution_display_name(bindings, symbolic_variable, solutions, status, i);
        const equation_t *solution = equation_solutions_at(solutions, i);
        equation_t *display = solution_with_bound_constants(solution, bindings);
        const equation_t *shown = display ? display : solution;
        char *rhs_text = expr_text_dup(equation_rhs(shown), style_UNBOUND);
        char *text = equation_text_dup(shown, style_UNBOUND);

        if (name && rhs_text)
            printf("%s%s = %s\n", i == 0u ? "solutions   " : "            ",
                   name, rhs_text);
        else
            printf("%s%s\n", i == 0u ? "solutions   " : "            ",
                   text ? text : "(null)");
        equation_free(display);
        free(text);
        free(rhs_text);
    }
}

static void print_solutions_tex(const equation_solutions_t *solutions,
                                expr_bindings_t *bindings,
                                const char *symbolic_variable,
                                const char *status)
{
    size_t count = equation_solutions_count(solutions);

    if (count == 0u) {
        printf("solutions_tex \n");
        return;
    }

    printf("solutions_tex \\begin{aligned}");
    for (size_t i = 0u; i < count; ++i) {
        const char *name = solution_display_name(bindings, symbolic_variable, solutions, status, i);
        const equation_t *solution = equation_solutions_at(solutions, i);
        equation_t *display = solution_with_bound_constants(solution, bindings);
        const equation_t *shown = display ? display : solution;
        char *rhs_tex = expr_tex_body_dup(equation_rhs(shown));
        char *tex = equation_text_dup(shown, style_TEX);

        if (name && rhs_tex)
            printf("%s%s &= %s", i == 0u ? " " : " \\\\ ", name, rhs_tex);
        else
            printf("%s%s", i == 0u ? " " : " \\\\ ", tex ? tex : "\\text{null}");
        equation_free(display);
        free(tex);
        free(rhs_tex);
    }
    printf(" \\end{aligned}\n");
}

static void print_equation_fields(const equation_t *equation,
                                  const equation_solutions_t *solutions,
                                  expr_bindings_t *bindings,
                                  const char *input,
                                  const char *symbolic_variable,
                                  const char *status)
{
    expr_t *residual = equation_residual(equation);
    expr_t *display_residual = residual ? display_simplified_expr(residual) : NULL;
    equation_t *display_equation = display_simplified_equation(equation);
    const equation_t *shown_equation = display_equation ? display_equation : equation;
    number_t residual_value = residual ? expr_eval(residual) : num_new();
    char *equation_text = equation_text_dup(equation, style_EXPRESSION);
    char *unbound_text = equation_text_dup(shown_equation, style_UNBOUND);
    char *tex = equation_text_dup(shown_equation, style_TEX);
    char *residual_text = display_residual ? expr_text_dup(display_residual, style_UNBOUND) : NULL;
    char *residual_value_text = number_text_dup(residual_value);

    printf("input       %s\n", input ? input : "");
    printf("equation    %s\n", equation_text ? equation_text : "");
    printf("unbound     %s\n", unbound_text ? unbound_text : "");
    printf("tex         %s\n", tex ? tex : "");
    printf("residual    %s\n", residual_text ? residual_text : "");
    printf("value       %s\n", residual_value_text ? residual_value_text : "");
    printf("status      %s\n", status ? status : "unsolved");
    print_solutions(solutions, bindings, symbolic_variable, status);
    print_solutions_tex(solutions, bindings, symbolic_variable, status);

    free(residual_value_text);
    free(residual_text);
    free(tex);
    free(unbound_text);
    free(equation_text);
    equation_free(display_equation);
    expr_free(display_residual);
    num_destroy(&residual_value);
    expr_free(residual);
}

static equation_solutions_t *solve_symbolically(equation_t *equation,
                                                expr_bindings_t *bindings,
                                                const char *variable)
{
    expr_t *wrt;

    if (!variable || !variable[0] || !bindings)
        return NULL;

    wrt = expr_bindings_get(bindings, variable);
    if (!wrt)
        return NULL;

    return equation_create_solutions_for(equation, wrt);
}

static equation_solutions_t *solve_numerically(equation_t *equation,
                                               expr_bindings_t *bindings,
                                               int precision)
{
    expr_goal_seek_options_t options = {
        .precision_digits = precision > 0 ? (size_t)precision : 64u,
        .max_iterations = precision > 0 ? (size_t)precision * 4u : 0u,
        .allow_complex = true,
        .simplify_result = false
    };

    return equation_create_numeric_solutions(equation, bindings, &options);
}

int main(int argc, char **argv)
{
    const char *input = argc > 1 ? argv[1] : "{ 2*x + 3 = 7 | x = NAN }";
    int precision = argc > 2 ? atoi(argv[2]) : 64;
    const char *variable = argc > 3 ? argv[3] : "x";
    expr_bindings_t *bindings = NULL;
    equation_t *equation;
    equation_solutions_t *solutions = NULL;
    const char *status = "unsolved";
    int rc = 0;

    if (precision > 0)
        num_set_default_prec_digits((size_t)precision + 8u);

    equation = equation_from_string(input, &bindings);
    if (!equation) {
        fprintf(stderr, "could not parse equation\n");
        return 1;
    }

    solutions = solve_symbolically(equation, bindings, variable);
    if (solutions && equation_solutions_has_any(solutions)) {
        status = "symbolic";
    } else {
        equation_solutions_free(solutions);
        solutions = solve_numerically(equation, bindings, precision);
        if (solutions && equation_solutions_has_any(solutions))
            status = "numeric";
        else if (!solutions)
            rc = 1;
    }

    if (rc == 0)
        print_equation_fields(equation, solutions, bindings, input, variable, status);
    else
        fprintf(stderr, "could not solve equation\n");

    equation_solutions_free(solutions);
    expr_bindings_free(bindings);
    equation_free(equation);
    return rc;
}
