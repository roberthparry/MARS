#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "diffequation.h"
#include "equation.h"
#include "expression.h"
#include "ustring.h"

static const char *solve_status_name(de_solve_status_t status)
{
    switch (status) {
        case DE_SOLVE_STATUS_SOLVED:
            return "solved";
        case DE_SOLVE_STATUS_UNSUPPORTED:
            return "unsupported";
        case DE_SOLVE_STATUS_INVALID:
            return "invalid";
        case DE_SOLVE_STATUS_FAILED:
            return "failed";
    }
    return "invalid";
}

static const char *solver_name(de_solver_t solver)
{
    switch (solver) {
        case DE_SOLVER_NONE:
            return "none";
        case DE_SOLVER_SEPARABLE:
            return "separable";
        case DE_SOLVER_LINEAR:
            return "first-order linear";
        case DE_SOLVER_BERNOULLI:
            return "Bernoulli";
        case DE_SOLVER_HOMOGENEOUS:
            return "first-order homogeneous";
        case DE_SOLVER_LINEAR_SUBSTITUTION:
            return "linear substitution";
        case DE_SOLVER_LINEAR_TRANSFORMATION:
            return "linear transformation";
        case DE_SOLVER_STURM_LIOUVILLE:
            return "Sturm-Liouville";
        case DE_SOLVER_CONSTANT_COEFFICIENT_LINEAR:
            return "constant-coefficient linear";
        case DE_SOLVER_DERIVATIVE_QUADRATIC:
            return "derivative-quadratic";
        case DE_SOLVER_EXACT_DERIVATIVE_LINEARIZATION:
            return "exact-derivative linearization";
        case DE_SOLVER_CONSTANT_COEFFICIENT_TRANSPORT:
            return "constant-coefficient transport";
        case DE_SOLVER_CHARACTERISTICS:
            return "characteristics";
        case DE_SOLVER_PARAMETER_LINEAR_PDE:
            return "parameter-dependent linear PDE";
        case DE_SOLVER_HYDROGEN_MATRIX:
            return "hydrogen matrix eigenproblem";
        case DE_SOLVER_EXACT_FIRST_ORDER:
            return "exact first-order";
    }
    return "none";
}

static void print_solution_field(
    const char *key,
    const diffequ_solve_result_t *result,
    style_t style)
{
    size_t count = de_solve_result_count(result);

    for (size_t i = 0u; i < count; ++i) {
        const equation_t *solution = de_solve_result_at(result, i);
        string_t *text = solution ? equ_to_text(solution, style) : NULL;

        if (text) {
            printf("%s %s\n", key, string_c_str(text));
            string_free(text);
        }
    }
}

static void print_solution_tex(const char *key,
                               const diffequ_solve_result_t *result,
                               size_t line_limit)
{
    size_t count = de_solve_result_count(result);

    if (!key || count == 0u)
        return;

    printf("%s \\begin{aligned}[t]\n", key);
    for (size_t i = 0u; i < count; ++i) {
        const equation_t *solution = de_solve_result_at(result, i);
        char *lhs = solution
            ? expr_to_tex_body_wrapped(equ_lhs(solution), line_limit)
            : NULL;
        char *rhs = solution
            ? expr_to_tex_body_wrapped(equ_rhs(solution), line_limit)
            : NULL;

        printf("%s &= %s%s\n",
               lhs ? lhs : "\\text{null}",
               rhs ? rhs : "\\text{null}",
               i + 1u < count ? " \\\\" : "");
        free(rhs);
        free(lhs);
    }
    printf("\\end{aligned}\n");
}

static void print_solver_steps(const diffequ_solve_result_t *result)
{
    const char *steps = de_solve_result_steps(result);
    const char *steps_tex = de_solve_result_steps_tex(result);

    if (steps)
        printf("steps %s\n", steps);
    if (steps_tex)
        printf("steps_tex %s\n", steps_tex);
}

int main(int argc, char **argv)
{
    const char *source;
    diffequ_t *de;
    diffequ_solve_result_t *result;
    char *problem;
    char *problem_tex;
    const char *diagnostic;

    if (argc < 2) {
        fprintf(stderr, "usage: %s differential-equation\n", argv[0]);
        return 2;
    }

    source = argv[1];
    de = de_from_string(source);
    if (!de) {
        fprintf(stderr, "error could not parse differential equation\n");
        return 2;
    }

    result = de_solve(de);
    if (!result) {
        de_free(de);
        fprintf(stderr, "error could not allocate differential-equation result\n");
        return 2;
    }

    problem = de_to_string(de, style_EXPRESSION);
    problem_tex = de_to_string(de, style_TEX);
    diagnostic = de_solve_result_diagnostic(result);

    printf("input %s\n", source);
    printf("problem %s\n", problem ? problem : "");
    printf("problem_tex %s\n", problem_tex ? problem_tex : "");
    printf("status %s\n", solve_status_name(de_solve_result_status(result)));
    printf("solver %s\n", solver_name(de_solve_result_solver(result)));
    printf("diagnostic %s\n", diagnostic ? diagnostic : "");
    if (de_solve_result_symmetry(result))
        printf("symmetry %s\n", de_solve_result_symmetry(result));
    print_solver_steps(result);
    print_solution_field("solutions", result, style_UNBOUND);
    print_solution_tex("solutions_tex", result, SIZE_MAX);
    print_solution_tex("solutions_wrapped_tex", result, 1u);

    free(problem_tex);
    free(problem);
    de_solve_result_free(result);
    de_free(de);
    return 0;
}
