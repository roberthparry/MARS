#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diffequation.h"
#include "test_harness.h"
#include "test_diffequ_triangular.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"

static const expr_t *triangular_dependent(const expr_t *expr)
{
    const expr_t *left = NULL, *right = NULL;
    if (expr_is_formal_derivative(expr))
        return expr_formal_derivative_dependent(expr);
    if (!expr || !expr_child_exprs(expr, &left, &right))
        return NULL;
    const expr_t *found = triangular_dependent(left);
    return found ? found : triangular_dependent(right);
}

static const expr_t *triangular_family(const expr_t *expr)
{
    const expr_t *left = NULL, *right = NULL;
    if (expr_is_arbitrary_function(expr))
        return expr;
    if (!expr || !expr_child_exprs(expr, &left, &right))
        return NULL;
    const expr_t *found = triangular_family(left);
    return found ? found : triangular_family(right);
}

/* Independent numerical checks avoid depending on the solver's symbolic factor-collection strategy. */
static bool triangular_zero_at_samples(const expr_t *expr, const diffequ_t *de, unsigned mask)
{
    for (long sample = 1L; sample <= 3L; ++sample) {
        expr_t *at = expr_clone(expr);
        for (size_t i = 0u; at && i < 2u; ++i) {
            long sign = (mask & (1u << i)) ? -1L : 1L;
            number_t value = num_create_from_frac(sign * (sample + (long)i), 2L);
            expr_t *point = expr_new_const(value);
            num_destroy(&value);
            expr_t *next = expr_substitute(at, de_independent_at(de, i), point);
            expr_free(point);
            expr_free(at);
            at = next;
        }
        number_t value = at ? expr_eval(at) : num_clone(NUM_NAN);
        number_t magnitude = num_abs(value), tolerance = num_create_from_string("1e-24");
        bool valid = num_is_finite(value) && num_lt(magnitude, tolerance);
        num_destroy(&tolerance);
        num_destroy(&magnitude);
        num_destroy(&value);
        expr_free(at);
        if (!valid)
            return false;
    }
    return true;
}

/* Substitute the solution derivatives into the input PDE on all four real coordinate-sign charts. */
static bool triangular_verify(const diffequ_t *de, const equation_t *solution)
{
    const expr_t *dependent = triangular_dependent(equ_lhs(de_equation(de)));
    if (!dependent)
        return false;
    for (unsigned mask = 0u; mask < 4u; ++mask) {
        expr_t *candidate = expr_clone(equ_rhs(solution));
        const expr_t *family = triangular_family(candidate), *argument = NULL;
        if (family && expr_child_exprs(family, &argument, NULL)) {
            /* A nonlinear profile exercises all chain-rule terms without relying on formal F' collection. */
            expr_t *square = expr_mul(argument, argument);
            expr_t *profile = expr_add_simplify_owned(expr_mul(square, argument), expr_clone(argument));
            expr_t *next = expr_substitute(candidate, family, profile);
            expr_free(profile);
            expr_free(square);
            expr_free(candidate);
            candidate = next;
        }
        for (size_t i = 0u; candidate && i < 2u; ++i) {
            const expr_t *coordinate = de_independent_at(de, i);
            expr_t *absolute = expr_abs(coordinate);
            expr_t *signed_coordinate = (mask & (1u << i)) ? expr_neg(coordinate) : expr_clone(coordinate);
            expr_t *next = expr_substitute(candidate, absolute, signed_coordinate);
            expr_free(signed_coordinate);
            expr_free(absolute);
            expr_free(candidate);
            candidate = next;
        }
        expr_t *residual = equ_residual(de_equation(de));
        for (size_t i = 0u; residual && candidate && i < 2u; ++i) {
            expr_t *coordinate = (expr_t *)de_independent_at(de, i);
            expr_t *formal = expr_new_formal_derivative(dependent, 1u, &coordinate);
            expr_t *derivative = expr_create_deriv(candidate, coordinate);
            expr_t *next = formal && derivative ? expr_substitute(residual, formal, derivative) : NULL;
            expr_free(derivative);
            expr_free(formal);
            expr_free(residual);
            residual = next;
        }
        expr_t *simplified = residual ? expr_simplify(residual) : NULL;
        expr_t *expanded = simplified ? expr_display_expanded(simplified) : NULL;
        expr_t *zero = expanded ? expr_simplify(expanded) : NULL;
        bool valid = candidate && zero &&
                     (expr_is_exact_zero(zero) || triangular_zero_at_samples(zero, de, mask));
        if (!valid) {
            char *text = zero ? expr_to_string(zero, style_UNBOUND) : NULL;
            printf("  residual: %s\n", text ? text : "NULL");
            free(text);
        }
        expr_free(zero);
        expr_free(expanded);
        expr_free(simplified);
        expr_free(residual);
        expr_free(candidate);
        if (!valid)
            return false;
    }
    return true;
}

void test_diffequ_triangular_transport(void)
{
    static const char *const sources[] = {
        "x*u_x + (x+y)*u_y = 1",
        "(x+y)*u_y + x*u_x = 1",
        "3*x*u_x + (3*x+3*y)*u_y = 3",
        "2*r*w_r + (3*r+2*s)*w_s = 5",
        "y*u_y + (y+x)*u_x = -2",
        "x*u_x + (2*x+3*y)*u_y = 1",
        "2*x*u_x + (x-y)*u_y = 3",
    };
    for (size_t i = 0u; i < sizeof(sources) / sizeof(*sources); ++i) {
        diffequ_t *de = de_from_string(sources[i]);
        diffequ_solve_result_t *result = de ? de_solve_with_options(de, DE_SOLVE_OPTION_STEPS) : NULL;
        const equation_t *solution = de_solve_result_at(result, 0u);
        bool solved = de_solve_result_status(result) == DE_SOLVE_STATUS_SOLVED;
        bool verified = solution && triangular_verify(de, solution);
        const char *steps = de_solve_result_steps(result), *TeX = de_solve_result_steps_TeX(result);
        bool explained = steps && strstr(steps, "real charts") && strstr(steps, "!= 0") &&
                         !strstr(steps, "NAN") && TeX && !strstr(TeX, "NAN");
        de_free(de);
        string_t *text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
        printf("  %s\n  %s\n", sources[i], text ? string_c_str(text) : "NULL");
        bool retained = text && strstr(string_c_str(text), "F(");
        string_free(text);
        de_solve_result_free(result);
        ASSERT_TRUE(solved);
        ASSERT_TRUE(verified);
        ASSERT_TRUE(explained);
        ASSERT_TRUE(retained);
    }
}

void test_diffequ_triangular_scope(void)
{
    static const char *const sources[] = {
        "x*u_x + (x+y^2)*u_y = 1",
        "(x+y)*u_x + (x-y)*u_y = 1",
        "x*u_x + (x+y)*u_y = u",
        "x*u_x + (x+y)*u_y = sin(y)",
        "x*u_x + (x+y)*u_y = 1; u(1,y) = 0",
        "x*u_x + (x+y)*u_y + u_xx = 1",
    };
    for (size_t i = 0u; i < sizeof(sources) / sizeof(*sources); ++i) {
        diffequ_t *de = de_from_string(sources[i]);
        diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
        const char *diagnostic = de_solve_result_diagnostic(result);
        bool skipped = diagnostic && !strstr(diagnostic, "triangular linear characteristics");
        de_solve_result_free(result);
        de_free(de);
        ASSERT_TRUE(skipped);
    }
}

void test_diffequ_calculus_coefficients_TeX(void)
{
    static const struct { const char *source; const char *derivative; } cases[] = {
        {"x*u_x + (x+y)*u_y = 1", "\\frac{\\partial u}{\\partial y}"},
        {"x*u_x - (x+y)*u_y = 1", "\\frac{\\partial u}{\\partial y}"},
        {"u_y*(x+y) + x*u_x = 1", "\\frac{\\partial u}{\\partial y}"},
        {"(x+y)*u_yy + u_x = 0", "\\frac{\\partial^{2} u}{\\partial y^{2}}"},
        {"u_x + (x+y)*u_xy = 0", "\\frac{\\partial^{2} u}"},
        {"(x+1)*y'' + y = 0", "\\frac{d^{2} y}{d x^{2}}"},
    };
    for (size_t i = 0u; i < sizeof(cases) / sizeof(*cases); ++i) {
        diffequ_t *de = de_from_string(cases[i].source);
        char *TeX = de ? de_to_string(de, style_LATEX) : NULL;
        const char *coefficient = TeX ? strstr(TeX, "\\right)") : NULL;
        const char *derivative = TeX ? strstr(TeX, cases[i].derivative) : NULL;
        bool ordered = coefficient && derivative && coefficient < derivative;
        printf("  %s\n", TeX ? TeX : "NULL");
        free(TeX);
        de_free(de);
        ASSERT_TRUE(ordered);
    }
    expr_t *x = expr_new_named_var(NUM_NAN, "x"), *one = expr_const_one();
    expr_t *coefficient = expr_add(x, one), *hyperbolic = expr_cosh(x);
    expr_t *integrand = expr_exp(hyperbolic), *integral = expr_integral(integrand, x);
    expr_t *product = expr_mul(integral, coefficient);
    char *TeX = expr_to_TeX_body(product);
    const char *before = TeX ? strstr(TeX, "\\right)") : NULL;
    const char *after = TeX ? strstr(TeX, "\\int") : NULL;
    bool ordered = before && after && before < after;
    printf("  %s\n", TeX ? TeX : "NULL");
    free(TeX);
    expr_free(product);
    expr_free(integral);
    expr_free(integrand);
    expr_free(hyperbolic);
    expr_free(coefficient);
    expr_free(one);
    expr_free(x);
    ASSERT_TRUE(ordered);
}

/* README example from docs/diffequation.md: the repeated-rate triangular characteristic field. */
void example_diffequation_triangular_transport(void)
{
    const char *source = "x*u_x + (x+y)*u_y = 1";
    diffequ_t *de = de_from_string(source);
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    const equation_t *solution = de_solve_result_at(result, 0u);
    string_t *text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    printf("  %s\n  %s\n", source, text ? string_c_str(text) : "NULL");
    bool verified = solution && triangular_verify(de, solution);
    bool matched = text && strcmp(string_c_str(text), "u = F(y/x - ln(|x|)) + ln(|x|)") == 0;
    string_free(text);
    de_solve_result_free(result);
    de_free(de);
    ASSERT_TRUE(verified);
    ASSERT_TRUE(matched);
}
