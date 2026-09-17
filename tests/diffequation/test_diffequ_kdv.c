#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diffequation.h"
#include "test_harness.h"
#include "test_diffequ_kdv.h"

void test_diffequ_kdv_family(void)
{
    static const char *const sources[] = {
        "u_t + 6uu_x + u_xxx = 0",
        "u_xxx + 6*u_x*u + u_t = 0",
        "2*u_t + 12*u*u_x + 2*u_xxx = 0",
        "w_s - 3*w*w_r + 2*w_rrr = 0",
        "u_t + 6*u*u_k + u_kkk = 0",
        "{ u_t + 6*u*u_x + u_xxx = 0 | t = ?, x = ?; x0 = 3; }",
    };
    for (size_t i = 0u; i < sizeof(sources) / sizeof(*sources); ++i) {
        diffequ_t *de = de_from_string(sources[i]);
        diffequ_solve_result_t *result = de ? de_solve_with_options(de, DE_SOLVE_OPTION_STEPS) : NULL;
        bool solved = de_solve_result_status(result) == DE_SOLVE_STATUS_SOLVED &&
                      de_solve_result_solver(result) == DE_SOLVER_KDV_SOLITARY_WAVE;
        const equation_t *solution = de_solve_result_at(result, 0u);
        bool explained = de_solve_result_steps(result) && de_solve_result_steps_TeX(result) &&
                         strstr(de_solve_result_diagnostic(result), "not the general solution");
        de_free(de);
        string_t *text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
        char *TeX = solution ? equ_to_TeX_body_wrapped(solution, SIZE_MAX) : NULL;
        printf("  %s\n  %s\n", sources[i], text ? string_c_str(text) : "NULL");
        bool retained = text && TeX && strstr(string_c_str(text), "not the general solution") &&
                        strstr(TeX, "not the general solution") && !strstr(TeX, "NAN");
        if (i == 4u)
            retained = retained && strstr(string_c_str(text), "k₁");
        free(TeX); string_free(text); de_solve_result_free(result);
        ASSERT_TRUE(solved);
        ASSERT_TRUE(explained);
        ASSERT_TRUE(retained);
    }
}

void test_diffequ_kdv_scope(void)
{
    static const char *const sources[] = {
        "u_t + 6*u*u_x + u_xxx = 1",
        "u_t + 6*u^2*u_x + u_xxx = 0",
        "u_t + 6*U*u_x + u_xxx = 0",
        "u_t + 6*x*u*u_x + u_xxx = 0",
        "u_t + 6*u*u_x + u_xxx + u_xx = 0",
        "u_tt + 6*u*u_x + u_xxx = 0",
        "u_t + 6*u*u_x + u_xxt = 0",
        "u_t + 6*u*u_x + i*u_xxx = 0",
        "u_t + 6*u*u_x + a*u_xxx = 0",
        "u_t + 6*u*u_x + u_xxx = 0; u(x,0)=f(x)",
        "u_t + 6*u*u_x + u_xxx = 0; u(0,t)=0",
    };
    for (size_t i = 0u; i < sizeof(sources) / sizeof(*sources); ++i) {
        diffequ_t *de = de_from_string(sources[i]);
        diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
        bool declined = de && result && de_solve_result_solver(result) != DE_SOLVER_KDV_SOLITARY_WAVE;
        de_solve_result_free(result); de_free(de);
        ASSERT_TRUE(declined);
    }
}

static double kdv_sample(const expr_t *expr, long sample)
{
    static const char *const names[] = {"t", "x", "k", "x0"};
    expr_t *at = expr_clone(expr);
    for (size_t i = 0u; at && i < 4u; ++i) {
        expr_t *variable = expr_new_named_var(NUM_NAN, names[i]);
        number_t value = num_create_from_frac(sample + (long)i + 1L, 8L);
        expr_t *point = expr_new_const(value), *next = expr_substitute(at, variable, point);
        num_destroy(&value); expr_free(point); expr_free(variable); expr_free(at);
        at = next;
    }
    number_t value = at ? expr_eval(at) : num_clone(NUM_NAN);
    double result = num_to_double(value);
    num_destroy(&value); expr_free(at);
    return result;
}

/* Differentiate the returned native expression and substitute it into the original, unsimplified PDE. */
void test_diffequ_kdv_residual(void)
{
    static const struct { const char *source; double a, b, c; } cases[] = {
        {"u_t + 6uu_x + u_xxx = 0", 1, 6, 1},
        {"2*u_t + 12*u*u_x + 2*u_xxx = 0", 2, 12, 2},
        {"3*u_t - 2*u*u_x + 5*u_xxx = 0", 3, -2, 5},
        {"-2*u_t + 3*u*u_x - u_xxx = 0", -2, 3, -1},
    };
    for (size_t i = 0u; i < sizeof(cases) / sizeof(*cases); ++i) {
        diffequ_t *de = de_from_string(cases[i].source);
        diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
        const equation_t *solution = de_solve_result_at(result, 0u);
        expr_t *residual = de ? equ_residual(de_equation(de)) : NULL;
        expr_t *applied = solution ? expr_substitute(residual, equ_lhs(solution), equ_rhs(solution)) : NULL;
        bool verified = applied && de_solve_result_solver(result) == DE_SOLVER_KDV_SOLITARY_WAVE;
        for (long sample = 0L; verified && sample < 4L; ++sample) {
            double t = (sample + 1.0) / 8.0, x = (sample + 2.0) / 8.0;
            double k = (sample + 3.0) / 8.0, position = (sample + 4.0) / 8.0;
            double phase = k * (x - 4.0 * cases[i].c * k*k * t / cases[i].a - position);
            double expected = 12.0 * cases[i].c * k*k / (cases[i].b * pow(cosh(phase), 2));
            double actual = kdv_sample(equ_rhs(solution), sample), error = kdv_sample(applied, sample);
            verified = isfinite(actual) && fabs(actual - expected) < 1e-12 && isfinite(error) && fabs(error) < 1e-12;
        }
        expr_free(applied); expr_free(residual); de_solve_result_free(result); de_free(de);
        ASSERT_TRUE(verified);
    }
}

/* README example from docs/diffequation.md: explicitly a particular family, even without derivation steps. */
void example_diffequation_kdv(void)
{
    const char *source = "u_t + 6uu_x + u_xxx = 0";
    diffequ_t *de = de_from_string(source);
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    const equation_t *solution = de_solve_result_at(result, 0u);
    string_t *text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    const char *expected = "Solitary-wave family (not the general solution): u = "
                           "2k²·sech²(k·(x - x₀ - 4k²t)); k > 0, x₀ real (arbitrary constants).";
    bool valid = de_solve_result_solver(result) == DE_SOLVER_KDV_SOLITARY_WAVE && text &&
                 strcmp(string_c_str(text), expected) == 0;
    printf("  %s\n  %s\n", source, text ? string_c_str(text) : "NULL");
    string_free(text); de_solve_result_free(result); de_free(de);
    ASSERT_TRUE(valid);
}
