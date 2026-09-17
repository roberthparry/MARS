#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "diffequation.h"
#include "equation.h"
#include "expression.h"
#include "ustring.h"

static const char *solve_status_name(de_solve_status_t status)
{
    static const char *const names[] = {
        [DE_SOLVE_STATUS_SOLVED]      = "solved",
        [DE_SOLVE_STATUS_UNSUPPORTED] = "unsupported",
        [DE_SOLVE_STATUS_INVALID]     = "invalid",
        [DE_SOLVE_STATUS_FAILED]      = "failed",
        [DE_SOLVE_STATUS_SERIES]      = "series",
    };
    return (size_t)status < sizeof(names) / sizeof(*names) && names[status] ? names[status] : "invalid";
}

static const char *solver_name(de_solver_t solver)
{
    static const char *const names[] = {
        [DE_SOLVER_NONE]                           = "none",
        [DE_SOLVER_SEPARABLE]                      = "separable",
        [DE_SOLVER_LINEAR]                         = "first-order linear",
        [DE_SOLVER_BERNOULLI]                      = "Bernoulli",
        [DE_SOLVER_HOMOGENEOUS]                    = "first-order homogeneous",
        [DE_SOLVER_LINEAR_SUBSTITUTION]            = "linear substitution",
        [DE_SOLVER_LINEAR_TRANSFORMATION]          = "linear transformation",
        [DE_SOLVER_STURM_LIOUVILLE]                = "Sturm-Liouville",
        [DE_SOLVER_POWER_LAW_BESSEL]               = "power-law Bessel",
        [DE_SOLVER_CONSTANT_COEFFICIENT_LINEAR]    = "constant-coefficient linear",
        [DE_SOLVER_DERIVATIVE_QUADRATIC]           = "derivative-quadratic",
        [DE_SOLVER_EXACT_DERIVATIVE_LINEARIZATION] = "exact-derivative linearization",
        [DE_SOLVER_CONSTANT_COEFFICIENT_TRANSPORT] = "constant-coefficient transport",
        [DE_SOLVER_CHARACTERISTICS]                = "characteristics",
        [DE_SOLVER_PARAMETER_LINEAR_PDE]           = "parameter-dependent linear PDE",
        [DE_SOLVER_STATIONARY_EIGENFUNCTION]       = "stationary eigenfunction",
        [DE_SOLVER_EXACT_FIRST_ORDER]              = "exact first-order",
        [DE_SOLVER_LAPLACE]                        = "Laplace",
        [DE_SOLVER_TAYLOR_SERIES]                  = "local Taylor series",
        [DE_SOLVER_KIRCHHOFF]                      = "Kirchhoff spherical means",
        [DE_SOLVER_DALEMBERT_DUHAMEL]              = "d'Alembert-Duhamel",
        [DE_SOLVER_FOURIER_EVOLUTION]              = "Fourier evolution",
        [DE_SOLVER_KDV_SOLITARY_WAVE]              = "KdV solitary waves",
        [DE_SOLVER_GKDV_TRAVELLING_WAVE]           = "generalised KdV travelling waves",
        [DE_SOLVER_HALF_LINE_HEAT]                = "half-line heat boundary kernel",
    };
    return (size_t)solver < sizeof(names) / sizeof(*names) && names[solver] ? names[solver] : "none";
}

static void print_solution_field(const char *key, const diffequ_solve_result_t *result, style_t style)
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

static void print_result_TeX(const char *key, const diffequ_solve_result_t *result, const char *problem_TeX,
                             size_t line_limit)
{
    size_t count = de_solve_result_count(result);
    bool has_problem = problem_TeX && *problem_TeX;

    if (!key)
        return;
    if (count == 0u) {
        if (has_problem)
            printf("%s %s\n", key, problem_TeX);
        return;
    }

    /* Preserve the conventional factored Bessel basis in responsive output. */
    if (de_solve_result_solver(result) == DE_SOLVER_POWER_LAW_BESSEL)
        line_limit = SIZE_MAX;

    printf("%s ", key);
    if (has_problem)
        printf("\\begin{aligned}[t]\n&%s \\\\[1em]\n&", problem_TeX);
    if (count > 1u)
        printf("\\begin{aligned}[t]\n");
    for (size_t i = 0u; i < count; ++i) {
        const equation_t *solution = de_solve_result_at(result, i);
        char *TeX = solution ? equ_to_TeX_body_wrapped(solution, line_limit) : NULL;

        printf("%s%s%s\n", count > 1u ? "&" : "", TeX ? TeX : "\\text{null}",
               i + 1u < count ? " \\\\" : "");
        free(TeX);
    }
    if (count > 1u)
        printf("\\end{aligned}\n");
    if (has_problem)
        printf("\\end{aligned}\n");
}

static void print_solver_steps(const diffequ_solve_result_t *result)
{
    const char *steps = de_solve_result_steps(result);
    const char *steps_TeX = de_solve_result_steps_TeX(result);

    if (steps)
        printf("steps %s\n", steps);
    if (steps_TeX)
        printf("steps_TeX %s\n", steps_TeX);
}

int main(int argc, char **argv)
{
    const char *source;
    diffequ_t *de;
    diffequ_solve_result_t *result;
    char *problem;
    char *problem_TeX;
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

    result = de_solve_with_options(de, DE_SOLVE_OPTION_STEPS);
    if (!result) {
        de_free(de);
        fprintf(stderr, "error could not allocate differential-equation result\n");
        return 2;
    }

    problem = de_to_string(de, style_EXPRESSION);
    problem_TeX = de_to_string(de, style_LATEX);
    diagnostic = de_solve_result_diagnostic(result);

    printf("input %s\n", source);
    printf("problem %s\n", problem ? problem : "");
    printf("problem_TeX %s\n", problem_TeX ? problem_TeX : "");
    printf("status %s\n", solve_status_name(de_solve_result_status(result)));
    printf("solver %s\n", solver_name(de_solve_result_solver(result)));
    printf("diagnostic %s\n", diagnostic ? diagnostic : "");
    if (de_solve_result_symmetry(result))
        printf("symmetry %s\n", de_solve_result_symmetry(result));
    print_solver_steps(result);
    print_solution_field("solutions", result, style_UNBOUND);
    print_result_TeX("solutions_TeX", result, NULL, SIZE_MAX);
    print_result_TeX("solutions_wrapped_TeX", result, NULL, 1u);
    print_result_TeX("display_TeX", result, problem_TeX, SIZE_MAX);
    print_result_TeX("display_wrapped_TeX", result, problem_TeX, 1u);

    free(problem_TeX);
    free(problem);
    de_solve_result_free(result);
    de_free(de);
    return 0;
}
