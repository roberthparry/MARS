#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diffequation.h"
#include "test_harness.h"
#include "test_diffequ_gradient.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"

static const expr_t *gradient_dependent(const expr_t *expr)
{
    const expr_t *left = NULL, *right = NULL;
    if (expr_is_formal_derivative(expr))
        return expr_formal_derivative_dependent(expr);
    if (!expr || !expr_child_exprs(expr, &left, &right))
        return NULL;
    const expr_t *found = gradient_dependent(left);
    return found ? found : gradient_dependent(right);
}

static const expr_t *gradient_profile(const expr_t *expr)
{
    const expr_t *left = NULL, *right = NULL;
    if (expr_is_arbitrary_function(expr))
        return expr;
    if (!expr || !expr_child_exprs(expr, &left, &right))
        return NULL;
    const expr_t *found = gradient_profile(left);
    return found ? found : gradient_profile(right);
}

/* Independent rational samples allow equivalent but differently collected derivative expressions. */
static bool gradient_zero(const expr_t *expr, const diffequ_t *de, const expr_t *parameter)
{
    if (!expr)
        return false;
    for (long sample = 2L; sample <= 5L; ++sample) {
        expr_t *at = expr_clone(expr);
        const expr_t *symbols[] = {de_independent_at(de, 0u), de_independent_at(de, 1u), parameter};
        for (size_t i = 0u; at && i < 3u; ++i) {
            if (!symbols[i])
                continue;
            number_t value = num_create_from_frac(sample + (long)i, 2L);
            expr_t *point = expr_new_const(value), *next = expr_substitute(at, symbols[i], point);
            num_destroy(&value);
            expr_free(point); expr_free(at);
            at = next;
        }
        number_t value = at ? expr_eval(at) : num_clone(NUM_NAN);
        number_t magnitude = num_abs(value), tolerance = num_create_from_string("1e-24");
        bool valid = num_is_finite(value) && num_lt(magnitude, tolerance);
        if (!valid) {
            char *text = at ? expr_to_string(at, style_UNBOUND) : NULL;
            printf("  nonzero sample: %s\n", text ? text : "NULL");
            free(text);
        }
        num_destroy(&tolerance); num_destroy(&magnitude); num_destroy(&value);
        expr_free(at);
        if (!valid)
            return false;
    }
    return true;
}

static bool gradient_verify(const diffequ_t *de, const expr_t *surface, const expr_t *parameter)
{
    const expr_t *dependent = gradient_dependent(equ_lhs(de_equation(de)));
    expr_t *residual = equ_residual(de_equation(de));
    for (size_t i = 0u; residual && surface && dependent && i < 2u; ++i) {
        expr_t *coordinate = (expr_t *)de_independent_at(de, i);
        expr_t *formal = expr_new_formal_derivative(dependent, 1u, &coordinate);
        expr_t *derivative = expr_create_deriv(surface, coordinate);
        expr_t *next = formal && derivative ? expr_substitute(residual, formal, derivative) : NULL;
        expr_free(derivative); expr_free(formal); expr_free(residual);
        residual = next;
    }
    bool valid = dependent && surface && gradient_zero(residual, de, parameter);
    expr_free(residual);
    return valid;
}

void test_diffequ_gradient_envelope(void)
{
    static const char *const sources[] = {
        "u_x + u_x*u_y = 1", "3*u_x + 3*u_x*u_y = 3", "u_y*u_x + u_x = 1",
        "2*w_r*w_s + 3*w_r + 5*w_s = 7", "v_a + v_a*v_b = 1", "F_x + F_x*F_y = 1",
    };
    for (size_t i = 0u; i < sizeof(sources) / sizeof(*sources); ++i) {
        diffequ_t *de = de_from_string(sources[i]);
        diffequ_solve_result_t *result = de ? de_solve_with_options(de, DE_SOLVE_OPTION_STEPS) : NULL;
        const equation_t *envelope = de_solve_result_at(result, 0u), *plane = de_solve_result_at(result, 1u);
        const equation_t *constraint = de_solve_result_parameter_constraint(result);
        const expr_t *parameter = de_solve_result_parameter(result);
        bool solved = de_solve_result_status(result) == DE_SOLVE_STATUS_SOLVED &&
                      de_solve_result_count(result) == 2u && constraint && parameter;
        bool verified = envelope && plane && gradient_verify(de, equ_rhs(envelope), parameter) &&
                        gradient_verify(de, equ_rhs(plane), parameter);
        expr_t *derivative = envelope && parameter ? expr_create_deriv(equ_rhs(envelope), parameter) : NULL;
        expr_t *difference = derivative && constraint ? expr_sub(derivative, equ_lhs(constraint)) : NULL;
        expr_t *simplified = difference ? expr_simplify(difference) : NULL;
        bool stationary = simplified && gradient_zero(simplified, de, parameter);
        const char *steps = de_solve_result_steps(result), *steps_TeX = de_solve_result_steps_TeX(result);
        bool explained = steps && strstr(steps, "chain rule") && steps_TeX && !strstr(steps_TeX, "NAN");
        expr_free(simplified); expr_free(difference); expr_free(derivative);
        de_free(de);
        string_t *text = envelope ? equ_to_text(envelope, style_UNBOUND) : NULL;
        bool retained = text && strstr(string_c_str(text), "subject to H =") &&
                        strstr(string_c_str(text), "!= 0") && !strstr(string_c_str(text), "NAN") &&
                        !strstr(string_c_str(text), "291/");
        printf("  %s\n  %s\n", sources[i], text ? string_c_str(text) : "NULL");
        string_free(text);
        de_solve_result_free(result);
        ASSERT_TRUE(solved);
        ASSERT_TRUE(verified);
        ASSERT_TRUE(stationary);
        ASSERT_TRUE(explained);
        ASSERT_TRUE(retained);
    }
}

void test_diffequ_gradient_scope(void)
{
    static const char *const sources[] = {
        "u_x*u_y = 0", "u_x + u_y = 1", "u_x + u_x*u_y = x", "u_x + u_x*u_y = u",
        "u_x^2 + u_x*u_y = 1", "u_x + u_x*u_y = 1; u(x,0) = x",
        "u_x + u_x*u_yy = 1", "u_t + u_x + u_x*u_y = 1",
    };
    for (size_t i = 0u; i < sizeof(sources) / sizeof(*sources); ++i) {
        diffequ_t *de = de_from_string(sources[i]);
        diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
        bool skipped = result && !de_solve_result_parameter(result) && !de_solve_result_parameter_constraint(result);
        de_solve_result_free(result);
        de_free(de);
        ASSERT_TRUE(skipped);
    }
    ASSERT_TRUE(de_solve_result_parameter(NULL) == NULL);
    ASSERT_TRUE(de_solve_result_parameter_constraint(NULL) == NULL);
}

/* F=0 gives two non-affine branches u=+/-2*sqrt(x*y)-y on x,y>0. */
void test_diffequ_gradient_non_affine(void)
{
    diffequ_t *de = de_from_string("u_x + u_x*u_y = 1");
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    const expr_t *parameter = de_solve_result_parameter(result);
    const equation_t *envelope = de_solve_result_at(result, 0u);
    const equation_t *constraint = de_solve_result_parameter_constraint(result);
    bool valid = parameter && envelope && constraint;
    if (valid) {
        const expr_t *profile = gradient_profile(equ_rhs(envelope));
        const expr_t *profile_derivative = gradient_profile(equ_lhs(constraint));
        expr_t *zero = expr_const_zero();
        expr_t *surface = expr_substitute(equ_rhs(envelope), profile, zero);
        expr_t *condition = expr_substitute(equ_lhs(constraint), profile_derivative, zero);
        expr_t *ratio = expr_div(de_independent_at(de, 1u), de_independent_at(de, 0u));
        expr_t *root = expr_sqrt(ratio);
        for (size_t branch = 0u; branch < 2u; ++branch) {
            expr_t *a = branch ? expr_neg(root) : expr_clone(root);
            expr_t *candidate = expr_substitute(surface, parameter, a);
            expr_t *h = expr_substitute(condition, parameter, a);
            valid = valid && gradient_zero(h, de, NULL) && gradient_verify(de, candidate, NULL);
            expr_free(h); expr_free(candidate); expr_free(a);
        }
        expr_free(root); expr_free(ratio); expr_free(condition); expr_free(surface);
        expr_free(zero);
    }
    de_solve_result_free(result);
    de_free(de);
    ASSERT_TRUE(valid);
}

/* README example from docs/diffequation.md: the constraint remains visible without optional steps. */
void example_diffequation_gradient_envelope(void)
{
    diffequ_t *de = de_from_string("u_x + u_x u_y = 1");
    char *TeX = de ? de_to_string(de, style_LATEX) : NULL;
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    bool valid = de_solve_result_count(result) == 2u && de_solve_result_parameter_constraint(result);
    bool readable = TeX && strstr(TeX, "\\cdot") && !strstr(TeX, "NAN");
    printf("  u_x + u_x u_y = 1\n");
    for (size_t i = 0u; i < de_solve_result_count(result); ++i) {
        string_t *text = equ_to_text(de_solve_result_at(result, i), style_UNBOUND);
        printf("  %s\n", text ? string_c_str(text) : "NULL");
        valid = valid && text && strstr(string_c_str(text), "a != 0");
        if (i == 0u)
            valid = valid && text && strstr(string_c_str(text), "ax + y·(1/a - 1) + F(a)") &&
                    strstr(string_c_str(text), "x - y/a² + F'(a) = 0");
        else
            valid = valid && text && strstr(string_c_str(text), "Affine complete integral:") &&
                    strstr(string_c_str(text), "a and b are arbitrary constants");
        string_free(text);
    }
    free(TeX);
    de_solve_result_free(result);
    de_free(de);
    ASSERT_TRUE(valid);
    ASSERT_TRUE(readable);
}
