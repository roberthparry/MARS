#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diffequation.h"
#include "test_harness.h"
#include "test_diffequ_fourier.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"

static const expr_t *fourier_mode(const expr_t *expr)
{
    const expr_t *body = NULL, *left = NULL, *right = NULL;
    if (expr_match_integral_expr(expr, &body, NULL)) {
        const expr_t *inner = fourier_mode(body);
        return inner ? inner : body;
    }
    if (!expr_child_exprs(expr, &left, &right))
        return NULL;
    const expr_t *found = fourier_mode(left);
    return found ? found : fourier_mode(right);
}

static double fourier_sample(const expr_t *expr, long sample)
{
    static const char *const names[] = {"t", "x", "k", "ξ", "U"};
    expr_t *at = expr_clone(expr);
    for (size_t i = 0u; at && i < sizeof(names) / sizeof(*names); ++i) {
        expr_t *variable = expr_new_named_var(NUM_NAN, names[i]);
        number_t value = num_create_from_frac(sample + (long)i + 1L, 4L);
        expr_t *point = expr_new_const(value);
        expr_t *next = expr_substitute(at, variable, point);
        num_destroy(&value);
        expr_free(point); expr_free(variable); expr_free(at);
        at = next;
    }
    number_t value = at ? expr_eval(at) : num_clone(NUM_NAN);
    double result = num_to_double(value);
    num_destroy(&value);
    expr_free(at);
    return result;
}

void test_diffequ_fourier_evolution(void)
{
    static const char *const sources[] = {
        "u_t + Uu_x + u_xx + u_xxxx = 0; u(x,0) = f(x)",
        "2*u_t + 2*U*u_x + 2*u_xx + 2*u_xxxx = 0; u(x,0) = f(x)",
        "u_xxxx + u_xx + U*u_x + u_t = 0; u(x,0) = f(x)",
        "w_s + A*w_r - w_rr = 0; w(r,2) = g(r)",
        "u_t + u_xxx - u_xxxxxx = 0; u(x,0) = f(x)",
        "u_t + u_xxxxxxxx = 0; u(x,0) = f(x)",
        "u_t + k*u_x + u_xxxx = 0; u(x,0) = f(x)",
        "u_t + ξ*u_x + u_xxxx = 0; u(x,0) = f(x)",
        "u_t + (k+k_1)*u_x + u_xxxx = 0; u(x,0) = f(x)",
    };
    for (size_t i = 0u; i < sizeof(sources) / sizeof(*sources); ++i) {
        diffequ_t *de = de_from_string(sources[i]);
        diffequ_solve_result_t *result = de ? de_solve_with_options(de, DE_SOLVE_OPTION_STEPS) : NULL;
        bool solved = de_solve_result_status(result) == DE_SOLVE_STATUS_SOLVED &&
                      de_solve_result_solver(result) == DE_SOLVER_FOURIER_EVOLUTION;
        const equation_t *solution = de_solve_result_at(result, 0u);
        const char *steps = de_solve_result_steps(result);
        const char *TeX = de_solve_result_steps_TeX(result);
        bool explained = steps && strstr(steps, "Schwartz") && strstr(steps, "Fourier inversion") &&
                         TeX && !strstr(TeX, "NAN");
        de_free(de);
        string_t *text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
        printf("  %s\n  %s\n", sources[i], text ? string_c_str(text) : "NULL");
        bool retained = text && strstr(string_c_str(text), "∫") && strstr(string_c_str(text), "π") &&
                        !strstr(string_c_str(text), "NAN") && !strstr(string_c_str(text), "0.318309");
        string_free(text);
        de_solve_result_free(result);
        ASSERT_TRUE(solved);
        ASSERT_TRUE(explained);
        ASSERT_TRUE(retained);
    }
}

void test_diffequ_fourier_scope(void)
{
    static const char *const sources[] = {
        "u_t + u*u_x + u_xxxx = 0; u(x,0) = f(x)",
        "u_t + x*u_x + u_xxxx = 0; u(x,0) = f(x)",
        "u_t - u_xxxx = 0; u(x,0) = f(x)",
        "u_t + a*u_xxxx = 0; u(x,0) = f(x)",
        "u_tt + u_xxxx = 0; u(x,0) = f(x)",
        "u_t + u_tx + u_xxxx = 0; u(x,0) = f(x)",
        "u_t + u_xxxx = sin(t); u(x,0) = f(x)",
        "u_t + u_xxxx = 0; u(x,0) = f(x); u(0,t) = 0",
        "u_t + u_xxxx = 0; u(x,0) = f(t)",
        "u_t + i*u_x + u_xxxx = 0; u(x,0) = f(x)",
        "u_t - u_xxxxxxxxxx = 0; u(x,0) = f(x)",
        "u_t + U*u_x + u_xxxx = 0",
    };
    for (size_t i = 0u; i < sizeof(sources) / sizeof(*sources); ++i) {
        diffequ_t *de = de_from_string(sources[i]);
        diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
        bool declined = de && result && de_solve_result_solver(result) != DE_SOLVER_FOURIER_EVOLUTION;
        de_solve_result_free(result);
        de_free(de);
        ASSERT_TRUE(declined);
    }
}

void test_diffequ_fourier_kernel(void)
{
    static const char *const sources[] = {
        "u_t + Uu_x + u_xx + u_xxxx = 0; u(x,0) = f(x)",
        "u_t + u_xxx - u_xxxxxx = 0; u(x,0) = f(x)",
        "u_t + u_xxxxxxxx = 0; u(x,0) = f(x)",
    };
    for (size_t i = 0u; i < sizeof(sources) / sizeof(*sources); ++i) {
        diffequ_t *de = de_from_string(sources[i]);
        diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
        const equation_t *solution = de_solve_result_at(result, 0u);
        const expr_t *mode = solution ? fourier_mode(equ_rhs(solution)) : NULL;
        expr_t *residual = de ? equ_residual(de_equation(de)) : NULL;
        expr_t *applied = mode ? expr_substitute(residual, equ_lhs(de_condition_at(de, 0u)), mode) : NULL;
        bool verified = mode && applied;
        for (long sample = 0L; verified && sample < 3L; ++sample) {
            double t = (sample + 1.0) / 4.0, x = (sample + 2.0) / 4.0, k = (sample + 3.0) / 4.0;
            double xi = (sample + 4.0) / 4.0, velocity = (sample + 5.0) / 4.0;
            double expected = i == 0u ? exp((k*k - pow(k, 4))*t) * cos(k*(x-xi-velocity*t)) :
                              i == 1u ? exp(-pow(k, 6)*t) * cos(k*(x-xi) + pow(k, 3)*t) :
                                        exp(-pow(k, 8)*t) * cos(k*(x-xi));
            double actual = fourier_sample(mode, sample), error = fourier_sample(applied, sample);
            verified = isfinite(actual) && fabs(actual - expected) < 1e-12 && isfinite(error) && fabs(error) < 1e-12;
        }
        expr_free(applied); expr_free(residual);
        de_solve_result_free(result); de_free(de);
        ASSERT_TRUE(verified);
    }
}

/* README example from docs/diffequation.md: constant U is distinct from the dependent variable u. */
void example_diffequation_fourier_evolution(void)
{
    const char *source = "u_t + Uu_x + u_xx + u_xxxx = 0; u(x,0) = f(x)";
    diffequ_t *de = de_from_string(source);
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    const equation_t *solution = de_solve_result_at(result, 0u);
    string_t *text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    const char *expected = "u = 1/π·∫^∞_-∞ f(ξ)·∫^∞_0 exp(-t·(k⁴ - k²))·cos(k·(x - ξ) - Ukt)·dk·dξ";
    bool valid = de_solve_result_solver(result) == DE_SOLVER_FOURIER_EVOLUTION && text &&
                 strcmp(string_c_str(text), expected) == 0 && fourier_mode(equ_rhs(solution));
    printf("  %s\n  %s\n", source, text ? string_c_str(text) : "NULL");
    string_free(text); de_solve_result_free(result); de_free(de);
    ASSERT_TRUE(valid);
}
