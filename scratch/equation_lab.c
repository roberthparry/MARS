#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "equation.h"
#include "expression/expr_stringin_internal.h"
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
                                         const equation_solve_result_t *result,
                                         const char *status,
                                         size_t index)
{
    const char *binding_name;

    if (status && strcmp(status, "symbolic") == 0 &&
        result && result->count > 0u &&
        symbolic_variable && symbolic_variable[0])
        return symbolic_variable;

    binding_name = nth_variable_binding_name(bindings, index);
    return binding_name && binding_name[0] ? binding_name : NULL;
}

static void print_solutions(const equation_solve_result_t *result,
                            expr_bindings_t *bindings,
                            const char *symbolic_variable,
                            const char *status)
{
    if (!result || result->count == 0u) {
        printf("solutions   \n");
        return;
    }

    for (size_t i = 0u; i < result->count; ++i) {
        const char *name = solution_display_name(bindings, symbolic_variable, result, status, i);
        char *rhs_text = expr_text_dup(equation_rhs(result->solutions[i]), style_UNBOUND);
        char *text = equation_text_dup(result->solutions[i], style_UNBOUND);

        if (name && rhs_text)
            printf("%s%s = %s\n", i == 0u ? "solutions   " : "            ",
                   name, rhs_text);
        else
            printf("%s%s\n", i == 0u ? "solutions   " : "            ",
                   text ? text : "(null)");
        free(text);
        free(rhs_text);
    }
}

static void print_solutions_tex(const equation_solve_result_t *result,
                                expr_bindings_t *bindings,
                                const char *symbolic_variable,
                                const char *status)
{
    if (!result || result->count == 0u) {
        printf("solutions_tex \n");
        return;
    }

    printf("solutions_tex \\begin{aligned}");
    for (size_t i = 0u; i < result->count; ++i) {
        const char *name = solution_display_name(bindings, symbolic_variable, result, status, i);
        char *rhs_tex = expr_text_dup(equation_rhs(result->solutions[i]), style_TEX);
        char *tex = equation_text_dup(result->solutions[i], style_TEX);

        if (name && rhs_tex)
            printf("%s%s &= %s", i == 0u ? " " : " \\\\ ", name, rhs_tex);
        else
            printf("%s%s", i == 0u ? " " : " \\\\ ", tex ? tex : "\\text{null}");
        free(tex);
        free(rhs_tex);
    }
    printf(" \\end{aligned}\n");
}

static void print_equation_fields(const equation_t *equation,
                                  const equation_solve_result_t *result,
                                  expr_bindings_t *bindings,
                                  const char *input,
                                  const char *symbolic_variable,
                                  const char *status)
{
    expr_t *residual = equation_residual(equation);
    number_t residual_value = residual ? expr_eval(residual) : num_new();
    char *equation_text = equation_text_dup(equation, style_EXPRESSION);
    char *unbound_text = equation_text_dup(equation, style_UNBOUND);
    char *tex = equation_text_dup(equation, style_TEX);
    char *residual_text = residual ? expr_text_dup(residual, style_UNBOUND) : NULL;
    char *residual_value_text = number_text_dup(residual_value);

    printf("input       %s\n", input ? input : "");
    printf("equation    %s\n", equation_text ? equation_text : "");
    printf("unbound     %s\n", unbound_text ? unbound_text : "");
    printf("tex         %s\n", tex ? tex : "");
    printf("residual    %s\n", residual_text ? residual_text : "");
    printf("value       %s\n", residual_value_text ? residual_value_text : "");
    printf("status      %s\n", status ? status : "unsolved");
    print_solutions(result, bindings, symbolic_variable, status);
    print_solutions_tex(result, bindings, symbolic_variable, status);

    free(residual_value_text);
    free(residual_text);
    free(tex);
    free(unbound_text);
    free(equation_text);
    num_destroy(&residual_value);
    expr_free(residual);
}

static int solve_symbolically(equation_t *equation,
                              expr_bindings_t *bindings,
                              const char *variable,
                              equation_solve_result_t *result)
{
    expr_t *wrt;

    if (!variable || !variable[0] || !bindings)
        return 1;

    wrt = expr_bindings_get(bindings, variable);
    if (!wrt)
        return 1;

    if (equation_solve_for(equation, wrt, result) != 0)
        return -1;

    return result->status == EQUATION_SOLVE_SOLVED ? 0 : 1;
}

static int solve_numerically(equation_t *equation,
                             expr_bindings_t *bindings,
                             int precision,
                             equation_solve_result_t *result)
{
    expr_goal_seek_options_t options = {
        .precision_digits = precision > 0 ? (size_t)precision : 64u,
        .max_iterations = precision > 0 ? (size_t)precision * 4u : 0u,
        .allow_complex = true,
        .simplify_result = false
    };

    if (equation_solve_numeric(equation, bindings, &options, result) != 0)
        return -1;

    return result->status == EQUATION_SOLVE_SOLVED ? 0 : 1;
}

int main(int argc, char **argv)
{
    const char *input = argc > 1 ? argv[1] : "{ 2*x + 3 = 7 | x = NAN }";
    int precision = argc > 2 ? atoi(argv[2]) : 64;
    const char *variable = argc > 3 ? argv[3] : "x";
    expr_bindings_t *bindings = NULL;
    equation_t *equation;
    equation_solve_result_t result;
    const char *status = "unsolved";
    int rc = 0;
    int solve_rc;

    if (precision > 0)
        num_set_default_prec_digits((size_t)precision + 8u);

    equation = equation_from_string(input, &bindings);
    if (!equation) {
        fprintf(stderr, "could not parse equation\n");
        return 1;
    }

    solve_rc = solve_symbolically(equation, bindings, variable, &result);
    if (solve_rc == 0) {
        status = "symbolic";
    } else if (solve_rc < 0) {
        rc = 1;
    } else {
        solve_rc = solve_numerically(equation, bindings, precision, &result);
        if (solve_rc == 0)
            status = "numeric";
        else if (solve_rc < 0)
            rc = 1;
    }

    if (rc == 0)
        print_equation_fields(equation, &result, bindings, input, variable, status);
    else
        fprintf(stderr, "could not solve equation\n");

    equation_solve_result_clear(&result);
    expr_bindings_free(bindings);
    equation_free(equation);
    return rc;
}
