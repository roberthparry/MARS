#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diffequation.h"
#include "test_harness.h"
#include "test_diffequ_gkdv.h"

static const char gkdv_source[] = "∂u/∂t = 1/2(n + 1)(n + 2)u^n ∂u/∂x - ∂^3u/∂x^3";

void test_diffequ_gkdv_family(void)
{
    static const char *const sources[] = {
        gkdv_source,
        "u_t - (n*n+3*n+2)/2*u^n*u_x + u_xxx = 0",
        "2*u_t - (n+1)*(n+2)*u^n*u_x + 2*u_xxx = 0",
        "w_s = (m+1)*(m+2)/2*w^m*w_r - w_rrr",
        "u_t - 6*u^2*u_x + u_xxx = 0",
        "u_t - 10*u^3*u_x + u_xxx = 0",
        "3*u_t - 14*u^4*u_x + 2*u_xxx = 0",
        "u_t + (n+1)*(n+2)/2*u^n*u_x + u_xxx = 0",
        "u_t + 6*u^2*u_x + u_xxx = 0",
        "u_t - (v+1)*(v+2)/2*u^v*u_x + u_xxx = 0",
    };
    for (size_t i = 0u; i < sizeof(sources) / sizeof(*sources); ++i) {
        diffequ_t *de = de_from_string(sources[i]);
        diffequ_solve_result_t *result = de ? de_solve_with_options(de, DE_SOLVE_OPTION_STEPS) : NULL;
        const equation_t *solution = de_solve_result_at(result, 0u);
        bool solved = de_solve_result_status(result) == DE_SOLVE_STATUS_SOLVED &&
                      de_solve_result_solver(result) == DE_SOLVER_GKDV_TRAVELLING_WAVE;
        de_free(de);
        string_t *text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
        char *TeX = solution ? equ_to_TeX_body_wrapped(solution, SIZE_MAX) : NULL;
        bool valid = text && TeX && strstr(string_c_str(text), "not the general solution") &&
                     strstr(string_c_str(text), "positive integer") && !strstr(TeX, "NAN");
        if (i < 7u || i == 9u)
            valid = valid && strstr(string_c_str(text), "moving singularity excluded");
        if (i == 9u)
            valid = valid && strstr(string_c_str(text), "v₁");
        printf("  %s\n  %s\n", sources[i], text ? string_c_str(text) : "NULL");
        free(TeX); string_free(text); de_solve_result_free(result);
        ASSERT_TRUE(solved);
        ASSERT_TRUE(valid);
    }
}

void test_diffequ_gkdv_scope(void)
{
    static const char *const sources[] = {
        "u_t - (n+1)*(n+2)/2*u^n*u_x + u_xxx = 1",
        "u_t - (n+1)*(n+2)/2*u^n*u_x + u_xxx = 0; u(x,0)=f(x)",
        "u_t - (x+1)*(x+2)/2*u^x*u_x + u_xxx = 0",
        "u_t - (n+1)*(n+2)/2*u^n*u_x + x*u_xxx = 0",
        "u_t - (n+1)*(n+2)/2*u^n*u_x + u_xxt = 0",
        "u_t - (n+1)*(n+2)/2*u^n*u_x + u_xx + u_xxx = 0",
        "u_t - 2*u^(1/2)*u_x + u_xxx = 0",
        "u_t - 2*u^(-2)*u_x + u_xxx = 0",
        "u_t - 6*U^2*u_x + u_xxx = 0",
        "u_t - a*u^n*u_x + u_xxx = 0",
    };
    for (size_t i = 0u; i < sizeof(sources) / sizeof(*sources); ++i) {
        diffequ_t *de = de_from_string(sources[i]);
        diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
        bool declined = de && result && de_solve_result_solver(result) != DE_SOLVER_GKDV_TRAVELLING_WAVE;
        de_solve_result_free(result); de_free(de);
        ASSERT_TRUE(declined);
    }
}

static expr_t *gkdv_bind(const expr_t *expr, const char *name, double value)
{
    number_t number = num_create_from_double(value);
    expr_t *variable = expr_new_named_var(NUM_NAN, name), *point = expr_new_const(number);
    expr_t *result = expr_substitute(expr, variable, point);
    expr_free(point); expr_free(variable); num_destroy(&number);
    return result;
}

static double gkdv_at(const expr_t *expr, double x)
{
    const char *names[] = {"t", "x", "v", "x0"};
    double values[] = {0.125, x, 1.0, 0.25};
    expr_t *at = expr_clone(expr);
    for (size_t i = 0u; at && i < 4u; ++i) {
        expr_t *next = gkdv_bind(at, names[i], values[i]);
        expr_free(at); at = next;
    }
    number_t value = at ? expr_eval(at) : num_clone(NUM_NAN);
    double result = num_to_double(value);
    num_destroy(&value); expr_free(at);
    return result;
}

/* Verify both parity classes, both sides of the excluded pole, and the focusing sign independently. */
void test_diffequ_gkdv_residual(void)
{
    const char *sources[] = {gkdv_source, "u_t + (n+1)*(n+2)/2*u^n*u_x + u_xxx = 0"};
    for (size_t sign = 0u; sign < 2u; ++sign) {
        diffequ_t *de = de_from_string(sources[sign]);
        diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
        const equation_t *solution = de_solve_result_at(result, 0u);
        expr_t *residual = de ? equ_residual(de_equation(de)) : NULL;
        bool valid = solution && residual;
        for (long n = 1L; valid && n <= 5L; ++n) {
            expr_t *rhs = gkdv_bind(equ_rhs(solution), "n", (double)n);
            expr_t *pde = gkdv_bind(residual, "n", (double)n);
            expr_t *applied = rhs && pde ? expr_substitute(pde, equ_lhs(solution), rhs) : NULL;
            valid = applied != NULL;
            for (long side = -1L; valid && side <= 1L; side += 2L) {
                double x = 0.375 + side * 1.25, phase = n * side * 1.25 / 2.0;
                double denominator = sign ? cosh(phase) : sinh(phase);
                double expected = pow(1.0 / (denominator*denominator), 1.0/n);
                double actual = gkdv_at(rhs, x), error = gkdv_at(applied, x);
                valid = isfinite(actual) && fabs(actual-expected) < 1e-10 && isfinite(error) && fabs(error) < 1e-9;
                printf("  sign %zu, n=%ld, side=%ld: value %.12g, residual %.4g\n", sign, n, side, actual, error);
            }
            expr_free(applied); expr_free(pde); expr_free(rhs);
            if (valid && n > 1L) {
                char source[128];
                snprintf(source, sizeof(source), "u_t %c %ld*u^%ld*u_x + u_xxx = 0",
                          sign ? '+' : '-', (n+1L)*(n+2L)/2L, n);
                diffequ_t *literal = de_from_string(source);
                diffequ_solve_result_t *lr = literal ? de_solve(literal) : NULL;
                const equation_t *ls = de_solve_result_at(lr, 0u);
                expr_t *equation = literal ? equ_residual(de_equation(literal)) : NULL;
                expr_t *error = ls ? expr_substitute(equation, equ_lhs(ls), equ_rhs(ls)) : NULL;
                valid = error && de_solve_result_solver(lr) == DE_SOLVER_GKDV_TRAVELLING_WAVE;
                for (long side = -1L; valid && side <= 1L; side += 2L) {
                    double x = 0.375 + side*1.25, phase = n*side*1.25/2.0;
                    double denominator = sign ? cosh(phase) : sinh(phase);
                    double expected = pow(1.0/(denominator*denominator), 1.0/n);
                    double actual = gkdv_at(equ_rhs(ls), x), residual_value = gkdv_at(error, x);
                    valid = isfinite(actual) && fabs(actual-expected) < 1e-10 &&
                            isfinite(residual_value) && fabs(residual_value) < 1e-9;
                }
                expr_free(error); expr_free(equation); de_solve_result_free(lr); de_free(literal);
            }
        }
        expr_free(residual); de_solve_result_free(result); de_free(de);
        ASSERT_TRUE(valid);
    }
}

/* README example: the exact positive-integer-n input has a real singular travelling-wave family. */
void example_diffequation_gkdv(void)
{
    diffequ_t *de = de_from_string(gkdv_source);
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    const equation_t *solution = de_solve_result_at(result, 0u);
    string_t *text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    const char *expected = "Singular travelling-wave family (not the general solution): u = "
        "(v·cosech²(½n·√(v)·(x - x₀ - vt)))^(1/n); n is a positive integer; v > 0, x₀ real; "
        "positive real root; x - x₀ - vt != 0 (moving singularity excluded).";
    bool valid = text && de_solve_result_solver(result) == DE_SOLVER_GKDV_TRAVELLING_WAVE &&
                 strcmp(string_c_str(text), expected) == 0;
    printf("  %s\n  %s\n", gkdv_source, text ? string_c_str(text) : "NULL");
    string_free(text); de_solve_result_free(result); de_free(de);
    ASSERT_TRUE(valid);
}
