#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diffequation.h"
#include "test_harness.h"
#include "test_diffequ_affine.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"

static const expr_t *affine_find_family(const expr_t *expr)
{
    const expr_t *left = NULL, *right = NULL;
    if (expr_is_arbitrary_function(expr))
        return expr;
    if (!expr || !expr_child_exprs(expr, &left, &right))
        return NULL;
    const expr_t *found = affine_find_family(left);
    return found ? found : affine_find_family(right);
}

/* Independent residual samples cover cancellations that the display simplifier leaves uncollected. */
static bool affine_zero_at_samples(const expr_t *expr, const diffequ_t *de)
{
    for (long sample = 1L; sample <= 4L; ++sample) {
        expr_t *at = expr_clone(expr);
        for (size_t i = 0u; at && i < de_independent_count(de); ++i) {
            number_t value = num_create_from_frac(sample + (long)i, 3L);
            expr_t *point = expr_new_const(value);
            expr_t *next = expr_substitute(at, de_independent_at(de, i), point);
            expr_free(point);
            num_destroy(&value);
            expr_free(at);
            at = next;
        }
        number_t value = at ? expr_eval(at) : num_clone(NUM_NAN);
        number_t magnitude = num_abs(value), tolerance = num_create_from_string("1e-24");
        bool valid = num_is_finite(value) && num_lt(magnitude, tolerance);
        num_destroy(&tolerance); num_destroy(&magnitude); num_destroy(&value);
        expr_free(at);
        if (!valid)
            return false;
    }
    return true;
}

bool test_diffequ_affine_solution_verified(const diffequ_t *de, const equation_t *solution)
{
    /* Substitute independently computed derivatives, rather than trusting the solver's verification. */
    expr_t *residual = equ_residual(de_equation(de));
    const expr_t *family = affine_find_family(equ_rhs(solution)), *args = NULL;
    expr_t *profile = expr_const_zero(), *product = expr_const_one();
    if (family)
        expr_child_exprs(family, &args, NULL);
    for (size_t i = de_independent_count(de) - 1u; args && i > 0u; --i) {
        const expr_t *argument = args;
        if (i > 1u)
            expr_child_exprs(args, &args, &argument);
        profile = expr_add_simplify_owned(profile, expr_mul(argument, argument));
        product = expr_mul_simplify_owned(product, expr_clone(argument));
    }
    profile = expr_add_simplify_owned(profile, product);
    expr_t *candidate = family && profile ? expr_substitute(equ_rhs(solution), family, profile) : NULL;
    expr_free(profile);
    const expr_t *dependent = NULL;
    const expr_t *left = equ_lhs(de_equation(de));
    /* The original dependent symbol is recovered from a derivative in the input tree. */
    const expr_t *stack[128] = {left, equ_rhs(de_equation(de))};
    size_t count = 2u;
    while (count && !dependent) {
        const expr_t *node = stack[--count], *a = NULL, *b = NULL;
        if (expr_is_formal_derivative(node))
            dependent = expr_formal_derivative_dependent(node);
        else if (node && expr_child_exprs(node, &a, &b) && count + 2u <= 128u) {
            if (a) stack[count++] = a;
            if (b) stack[count++] = b;
        }
    }
    for (size_t i = 0u; dependent && residual && i < de_independent_count(de); ++i) {
        expr_t *coordinate = (expr_t *)de_independent_at(de, i);
        expr_t *formal = expr_new_formal_derivative(dependent, 1u, &coordinate);
        expr_t *derivative = candidate ? expr_create_deriv(candidate, coordinate) : NULL;
        expr_t *next = formal && derivative ? expr_substitute(residual, formal, derivative) : NULL;
        expr_free(formal);
        expr_free(derivative);
        expr_free(residual);
        residual = next;
    }
    /* Avoid combinatorial expansion of the nonlinear test profile in higher dimensions.
       The native solver separately proves each invariant symbolically before accepting it. */
    bool sampled = dependent && residual && de_independent_count(de) > 3u && affine_zero_at_samples(residual, de);
    expr_t *zero = residual && !sampled ? expr_simplify(residual) : NULL;
    if (zero && !expr_is_exact_zero(zero)) {
        expr_t *expanded = expr_display_expanded(zero);
        expr_free(zero);
        zero = expanded ? expr_simplify(expanded) : NULL;
        expr_free(expanded);
    }
    bool valid = sampled || (dependent && zero && (expr_is_exact_zero(zero) || affine_zero_at_samples(zero, de)));
    if (!valid) {
        char *debug = zero ? expr_to_string(zero, style_UNBOUND) : NULL;
        printf("  substitution residual: %s\n", debug ? debug : "NULL");
        free(debug);
    }
    expr_free(zero);
    expr_free(residual);
    expr_free(candidate);
    return valid;
}

void test_diffequ_affine_transport(void)
{
    static const char *const sources[] = {
        "u_t + u_x + y*u_y = sin(t)",
        "y*u_y + u_x + u_t = sin(t)",
        "2*u_t + 2*u_x + 2*y*u_y = 2*sin(t)",
        "w_s + 3*w_r + (2*z+4)*w_z = s^2",
        "u_t - u_x - 2*y*u_y + 3*z*u_z = cos(t)",
        "u_t + y*u_y = sin(t)",
        "u_t + u_x + y*u_y = 0",
    };
    for (size_t i = 0u; i < sizeof(sources) / sizeof(*sources); ++i) {
        diffequ_t *de = de_from_string(sources[i]);
        diffequ_solve_result_t *result = de ? de_solve_with_options(de, DE_SOLVE_OPTION_STEPS) : NULL;
        const equation_t *solution = de_solve_result_at(result, 0u);
        bool solved = de_solve_result_status(result) == DE_SOLVE_STATUS_SOLVED;
        bool verified = solution && test_diffequ_affine_solution_verified(de, solution);
        const char *steps = de_solve_result_steps(result), *TeX = de_solve_result_steps_TeX(result);
        bool explained = steps && strstr(steps, "characteristics") && TeX && !strstr(TeX, "NAN");
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

void test_diffequ_affine_transport_scope(void)
{
    static const char *const sources[] = {
        "u_t + x*y*u_x + y*u_y = sin(t)",
        "u_t + u_x + y^2*u_y = sin(t)",
        "u_t + u_x + y*u_y = u",
        "u_t + u_x + y*u_y = sin(t+x)",
        "u_t + u_x + y*u_y = sin(t); u(x,y,0) = x",
        "u_t + t*x*y*u_x + 3*t^2*u_y = 0",
        "u_t + x*u_x + 3*t^2*u_y = 0; u(x,y,0) = x",
    };
    for (size_t i = 0u; i < sizeof(sources) / sizeof(*sources); ++i) {
        diffequ_t *de = de_from_string(sources[i]);
        diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
        const char *diagnostic = de_solve_result_diagnostic(result);
        bool skipped = diagnostic && !strstr(diagnostic, "solved by affine transport");
        de_solve_result_free(result);
        de_free(de);
        ASSERT_TRUE(skipped);
    }
}

void test_diffequ_time_affine_transport(void)
{
    static const char *const sources[] = {
        "f_t + xf_x + 3t^2 f_y = 0",
        "3*t^2*f_y + x*f_x + f_t = 0",
        "2*f_t + 2*x*f_x + 6*t^2*f_y = 0",
        "w_s + (r+exp(s))*w_r + 3*s^2*w_z = cos(s)",
        "u_t + t*x*u_x + 3*t^2*u_y = sin(t)",
        "u_t + x*u_x + cos(t)*u_y = 0",
        "u_t + (x+2*t)*u_x + 3*t^2*u_y = 0",
        "u_t + 3*t^2*u_x = 0",
        "u_t +(α + βt)u_x + γe^tu_y = 0",
    };
    for (size_t i = 0u; i < sizeof(sources) / sizeof(*sources); ++i) {
        diffequ_t *de = de_from_string(sources[i]);
        diffequ_solve_result_t *result = de ? de_solve_with_options(de, DE_SOLVE_OPTION_STEPS) : NULL;
        const equation_t *solution = de_solve_result_at(result, 0u);
        bool solved = de_solve_result_status(result) == DE_SOLVE_STATUS_SOLVED;
        bool verified = solution && test_diffequ_affine_solution_verified(de, solution);
        const char *steps = de_solve_result_steps(result), *TeX = de_solve_result_steps_TeX(result);
        bool explained = steps && *steps && TeX && !strstr(TeX, "NAN");
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

void test_diffequ_ordered_equation_TeX(void)
{
    static const struct { const char *source; const char *first; const char *second; const char *third; } cases[] = {
        {"u_t + u_x + y*u_y = sin(t)", "{\\partial t}", "{\\partial x}", "{\\partial y}"},
        {"y*u_y + u_x + u_t = sin(t)", "{\\partial y}", "{\\partial x}", "{\\partial t}"},
        {"u_t - (u_x - y*u_y) = sin(t)", "{\\partial t}", "{\\partial x}", "{\\partial y}"},
    };
    for (size_t i = 0u; i < sizeof(cases) / sizeof(*cases); ++i) {
        diffequ_t *de = de_from_string(cases[i].source);
        char *TeX = de ? de_to_string(de, style_LATEX) : NULL;
        const char *a = TeX ? strstr(TeX, cases[i].first) : NULL;
        const char *b = TeX ? strstr(TeX, cases[i].second) : NULL;
        const char *c = TeX ? strstr(TeX, cases[i].third) : NULL;
        bool ordered = a && b && c && a < b && b < c;
        bool ungrouped = TeX && !strstr(TeX, "\\left(");
        bool signs = i != 2u || (TeX && strstr(TeX, " - ") && strstr(TeX, " + "));
        printf("  %s\n", TeX ? TeX : "NULL");
        free(TeX);
        de_free(de);
        ASSERT_TRUE(ordered);
        ASSERT_TRUE(ungrouped);
        ASSERT_TRUE(signs);
    }
    diffequ_t *de = de_from_string("(x+y)*u_x + u_y = 0");
    char *TeX = de ? de_to_string(de, style_LATEX) : NULL;
    bool grouped = TeX && strstr(TeX, "\\left(") && strstr(TeX, "\\right)");
    free(TeX);
    de_free(de);
    ASSERT_TRUE(grouped);
}

/* README example from docs/diffequation.md: multidimensional affine characteristic flow. */
void example_diffequation_affine_transport(void)
{
    const char *source = "u_t + u_x + y*u_y = sin(t)";
    diffequ_t *de = de_from_string(source);
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    const equation_t *solution = de_solve_result_at(result, 0u);
    string_t *text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    printf("  %s\n  %s\n", source, text ? string_c_str(text) : "NULL");
    bool valid = text && strcmp(string_c_str(text), "u = F(x - t, y·exp(-t)) - cos(t)") == 0 &&
                 solution && test_diffequ_affine_solution_verified(de, solution);
    string_free(text);
    de_solve_result_free(result);
    de_free(de);
    ASSERT_TRUE(valid);
}

/* README example from docs/diffequation.md: time-dependent diagonal affine transport. */
void example_diffequation_time_affine_transport(void)
{
    const char *source = "f_t + xf_x + 3t^2 f_y = 0";
    diffequ_t *de = de_from_string(source);
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    const equation_t *solution = de_solve_result_at(result, 0u);
    string_t *text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    printf("  %s\n  %s\n", source, text ? string_c_str(text) : "NULL");
    bool valid = text && strcmp(string_c_str(text), "f = F(x·exp(-t), y - t³)") == 0 &&
                 solution && test_diffequ_affine_solution_verified(de, solution);
    string_free(text);
    de_solve_result_free(result);
    de_free(de);
    ASSERT_TRUE(valid);
}

/* README example from docs/diffequation.md: symbolic parameters in time-dependent transport. */
void example_diffequation_symbolic_affine_transport(void)
{
    const char *source = "u_t +(α + βt)u_x + γe^tu_y = 0";
    diffequ_t *de = de_from_string(source);
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    const equation_t *solution = de_solve_result_at(result, 0u);
    string_t *text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    char *TeX = de ? de_to_string(de, style_LATEX) : NULL;
    printf("  %s\n  %s\n", source, text ? string_c_str(text) : "NULL");
    bool valid = de_solve_result_status(result) == DE_SOLVE_STATUS_SOLVED && text &&
                 strcmp(string_c_str(text), "u = F(½·(2x - 2αt - βt²), y - γ·exp(t))") == 0 &&
                 solution && test_diffequ_affine_solution_verified(de, solution);
    bool ordered = TeX && strstr(TeX, "\\left(\\alpha + \\beta\\mkern-2mu t\\right)");
    free(TeX);
    string_free(text);
    de_solve_result_free(result);
    de_free(de);
    ASSERT_TRUE(valid);
    ASSERT_TRUE(ordered);
}
