#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "equation.h"
#include "expression.h"
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

static char *equ_text_dup(const equation_t *equation, style_t style)
{
    string_t *text = equ_to_text(equation, style);
    char *copy = text ? dup_string(string_c_str(text)) : NULL;

    string_free(text);
    return copy;
}

static char *expr_text_dup(const expr_t *expr, style_t style)
{
    return expr_to_string(expr, style);
}

static char *number_text_dup(number_t value)
{
    string_t *text = num_to_string(value);
    char *copy = text ? dup_string(string_c_str(text)) : NULL;

    string_free(text);
    return copy;
}

static char *number_precision_text_dup(number_t value, int precision)
{
    char fmt[32];
    int digits = precision > 0 ? precision - 1 : 0;
    int needed;
    char *out;

    if (precision <= 0)
        return number_text_dup(value);

    snprintf(fmt, sizeof(fmt), "%%.%dN", digits);
    needed = num_sprintf(NULL, 0u, fmt, value);
    if (needed < 0)
        return NULL;

    out = malloc((size_t)needed + 1u);
    if (!out)
        return NULL;
    if (num_sprintf(out, (size_t)needed + 1u, fmt, value) < 0) {
        free(out);
        return NULL;
    }

    return out;
}

static char *expr_tex_body_dup(const expr_t *expr)
{
    char *body = expr_to_tex_body(expr);

    return body ? body : expr_text_dup(expr, style_TEX);
}

static equation_t *display_simplified_equation(const equation_t *equation)
{
    expr_t *lhs;
    expr_t *rhs;
    equation_t *out;

    if (!equation)
        return NULL;

    lhs = expr_display_simplified(equ_lhs(equation));
    rhs = expr_display_simplified(equ_rhs(equation));
    if (!lhs || !rhs) {
        expr_free(lhs);
        expr_free(rhs);
        return NULL;
    }

    out = equ_new(lhs, rhs);
    expr_free(rhs);
    expr_free(lhs);
    return out;
}

static expr_t *substitute_bound_constants(const expr_t *expr,
                                          expr_bindings_t *bindings)
{
    expr_t *current;
    bool have_constant_bindings = false;

    if (!expr)
        return NULL;

    if (!bindings) {
        current = (expr_t *)expr;
        expr_retain(current);
        return current;
    }

    for (size_t i = 0u; i < expr_bindings_count(bindings); ++i) {
        const char *name = expr_bindings_name_at(bindings, i);

        if (!expr_bindings_is_constant_at(bindings, i))
            continue;
        if (!name || !name[0])
            continue;
        have_constant_bindings = true;
        break;
    }

    current = (expr_t *)expr;
    expr_retain(current);
    if (!have_constant_bindings)
        return current;

    for (size_t i = 0u; i < expr_bindings_count(bindings); ++i) {
        number_t value;
        number_t needle_seed;
        expr_t *needle;
        expr_t *replacement;
        expr_t *next;
        const char *name = expr_bindings_name_at(bindings, i);
        expr_t *binding_expr = expr_bindings_expr_at(bindings, i);

        if (!expr_bindings_is_constant_at(bindings, i))
            continue;
        if (!name || !name[0])
            continue;

        value = expr_eval(binding_expr);
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
        current = next;
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

    lhs = substitute_bound_constants(equ_lhs(solution), bindings);
    rhs = substitute_bound_constants(equ_rhs(solution), bindings);
    if (!lhs || !rhs) {
        expr_free(lhs);
        expr_free(rhs);
        return NULL;
    }

    out = equ_new(lhs, rhs);
    expr_free(rhs);
    expr_free(lhs);
    return out;
}

static const char *solution_binding_name(expr_bindings_t *bindings,
                                         const equation_t *solution)
{
    const expr_t *lhs = equ_lhs(solution);

    if (!bindings || !lhs)
        return NULL;

    for (size_t i = 0u; i < expr_bindings_count(bindings); ++i) {
        const char *name = expr_bindings_name_at(bindings, i);
        expr_t *binding_expr = expr_bindings_expr_at(bindings, i);

        if (expr_bindings_is_constant_at(bindings, i) || binding_expr != lhs || !name)
            continue;
        return name;
    }

    return NULL;
}

static void print_solutions(const equation_solutions_t *solutions,
                            expr_bindings_t *bindings)
{
    size_t count = equ_solutions_count(solutions);

    if (count == 0u) {
        printf("solutions   \n");
        return;
    }

    for (size_t i = 0u; i < count; ++i) {
        const equation_t *solution = equ_solutions_at(solutions, i);
        const char *name = solution_binding_name(bindings, solution);
        equation_t *display = solution_with_bound_constants(solution, bindings);
        const equation_t *shown = display ? display : solution;
        char *rhs_text = expr_text_dup(equ_rhs(shown), style_UNBOUND);
        char *text = equ_text_dup(shown, style_UNBOUND);

        if (name && rhs_text)
            printf("%s%s = %s\n", i == 0u ? "solutions   " : "            ",
                   name, rhs_text);
        else
            printf("%s%s\n", i == 0u ? "solutions   " : "            ",
                   text ? text : "(null)");
        equ_free(display);
        free(text);
        free(rhs_text);
    }
}

static void print_solutions_tex(const equation_solutions_t *solutions,
                                expr_bindings_t *bindings)
{
    size_t count = equ_solutions_count(solutions);

    if (count == 0u) {
        printf("solutions_tex \n");
        return;
    }

    printf("solutions_tex \\begin{aligned}");
    for (size_t i = 0u; i < count; ++i) {
        const equation_t *solution = equ_solutions_at(solutions, i);
        const char *name = solution_binding_name(bindings, solution);
        equation_t *display = solution_with_bound_constants(solution, bindings);
        const equation_t *shown = display ? display : solution;
        char *rhs_tex = expr_tex_body_dup(equ_rhs(shown));
        char *tex = equ_text_dup(shown, style_TEX);

        if (name && rhs_tex)
            printf("%s%s &= %s", i == 0u ? " " : " \\\\ ", name, rhs_tex);
        else
            printf("%s%s", i == 0u ? " " : " \\\\ ", tex ? tex : "\\text{null}");
        equ_free(display);
        free(tex);
        free(rhs_tex);
    }
    printf(" \\end{aligned}\n");
}

static number_t eval_solution_rhs_with_sampled_n(const expr_t *rhs,
                                                 bool *sampled_out)
{
    number_t value;

    if (sampled_out)
        *sampled_out = false;

    value = expr_eval(rhs);
    if (num_is_finite(value) && !num_is_nan(value))
        return value;

    {
        number_t zero_value = num_create_from_long(0L);
        expr_t *n = expr_new_named_var(NUM_NAN, "n");
        expr_t *zero = expr_new_const(zero_value);
        expr_t *sampled = (n && zero) ? expr_substitute(rhs, n, zero) : NULL;
        expr_t *simplified = sampled ? expr_simplify(sampled) : NULL;
        number_t sampled_value = simplified ? expr_eval(simplified) : num_new();

        num_destroy(&zero_value);
        expr_free(simplified);
        expr_free(sampled);
        expr_free(zero);
        expr_free(n);

        if (num_is_finite(sampled_value) && !num_is_nan(sampled_value)) {
            num_destroy(&value);
            if (sampled_out)
                *sampled_out = true;
            return sampled_value;
        }

        num_destroy(&sampled_value);
    }

    return value;
}

static void print_solution_numerics(const equation_solutions_t *solutions,
                                    expr_bindings_t *bindings,
                                    int precision)
{
    size_t count = equ_solutions_count(solutions);

    if (count == 0u) {
        printf("numeric     \n");
        return;
    }

    for (size_t i = 0u; i < count; ++i) {
        const equation_t *solution = equ_solutions_at(solutions, i);
        const char *name = solution_binding_name(bindings, solution);
        equation_t *display = solution_with_bound_constants(solution, bindings);
        const equation_t *shown = display ? display : solution;
        bool sampled_n = false;
        number_t value = eval_solution_rhs_with_sampled_n(equ_rhs(shown),
                                                          &sampled_n);
        char *value_text = number_precision_text_dup(value, precision);

        if (name && value_text)
            printf("%s%s ≈ %s%s\n", i == 0u ? "numeric     " : "            ",
                   name, value_text, sampled_n ? "  (n = 0)" : "");
        else
            printf("%s%s%s\n", i == 0u ? "numeric     " : "            ",
                   value_text ? value_text : "(null)",
                   sampled_n ? "  (n = 0)" : "");

        free(value_text);
        num_destroy(&value);
        equ_free(display);
    }
}

static void print_equation_fields(const equation_t *equation,
                                  const equation_solutions_t *solutions,
                                  expr_bindings_t *bindings,
                                  const char *input,
                                  const char *status,
                                  int precision)
{
    expr_t *residual = equ_residual(equation);
    expr_t *display_residual = residual ? expr_display_simplified(residual) : NULL;
    equation_t *display_equation = display_simplified_equation(equation);
    const equation_t *shown_equation = display_equation ? display_equation : equation;
    number_t residual_value = residual ? expr_eval(residual) : num_new();
    char *equation_text = equ_text_dup(equation, style_EXPRESSION);
    char *unbound_text = equ_text_dup(shown_equation, style_UNBOUND);
    char *tex = equ_text_dup(shown_equation, style_TEX);
    char *residual_text = display_residual ? expr_text_dup(display_residual, style_UNBOUND) : NULL;
    char *residual_value_text = number_text_dup(residual_value);

    printf("input       %s\n", input ? input : "");
    printf("equation    %s\n", equation_text ? equation_text : "");
    printf("unbound     %s\n", unbound_text ? unbound_text : "");
    printf("tex         %s\n", tex ? tex : "");
    printf("residual    %s\n", residual_text ? residual_text : "");
    printf("error       %s\n", residual_value_text ? residual_value_text : "");
    printf("status      %s\n", status ? status : "unsolved");
    print_solutions(solutions, bindings);
    print_solutions_tex(solutions, bindings);
    print_solution_numerics(solutions, bindings, precision);

    free(residual_value_text);
    free(residual_text);
    free(tex);
    free(unbound_text);
    free(equation_text);
    equ_free(display_equation);
    expr_free(display_residual);
    num_destroy(&residual_value);
    expr_free(residual);
}

int main(int argc, char **argv)
{
    const char *input = argc > 1 ? argv[1] : "{ 2*x + 3 = 7 | x = NAN }";
    int precision = argc > 2 ? atoi(argv[2]) : 64;
    equation_t *equation;
    equation_solutions_t *solutions = NULL;
    const char *status = "unsolved";
    int rc = 0;

    if (precision > 0)
        num_set_default_prec_digits((size_t)precision + 8u);

    equation = equ_from_string(input);
    if (!equation) {
        fprintf(stderr, "could not parse equation\n");
        return 1;
    }

    solutions = equ_derive_solutions(equation);
    if (!solutions)
        rc = 1;
    else if (equ_solutions_count(solutions) > 0u)
        status = "solved";

    if (rc == 0)
        print_equation_fields(equation, solutions, equ_bindings(equation),
                              input, status, precision);
    else
        fprintf(stderr, "could not solve equation\n");

    equ_solutions_free(solutions);
    equ_free(equation);
    return rc;
}
