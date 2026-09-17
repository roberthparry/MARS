#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MARS_DIFFEQUATION_PDE_INTERNAL_ACCESS
#include "diffequ_pde_internal.h"
#define MARS_SHARED_EQUATION_INTERNAL_ACCESS
#include "internal/equation_internal.h"

static bool de_gradient_constant(const expr_t *expr, const diffequ_t *de, const expr_t *dependent)
{
    if (!expr || de_expr_uses(expr, dependent))
        return false;
    for (size_t i = 0u; i < de->independent_count; ++i)
        if (de_expr_uses(expr, de->independent_vars[i]))
            return false;
    number_t value = expr_eval(expr);
    bool valid = num_is_real(value) && num_is_finite(value);
    num_destroy(&value);
    return valid;
}

/* There are only two coordinates and one dependent name; constant bindings use the native name lookup. */
static expr_t *de_gradient_parameter(const diffequ_t *de, const expr_t *dependent, const char *base, bool constant)
{
    for (size_t i = 0u; ; ++i) {
        char name[64];
        if (i == 0u)
            snprintf(name, sizeof(name), "%s", base);
        else
            snprintf(name, sizeof(name), "%s%zu", base, i);
        bool used = de_constant(de, name) != NULL;
        const expr_t *symbols[] = {dependent, de->independent_vars[0], de->independent_vars[1]};
        for (size_t j = 0u; j < 3u; ++j) {
            const char *symbol = expr_symbol_name(symbols[j]);
            used = used || (symbol && strcmp(name, symbol) == 0);
        }
        if (!used)
            return constant ? expr_new_named_const(NUM_NAN, name) : expr_new_named_var(NUM_NAN, name);
    }
}

/* Keep the coupled condition visible even without optional derivation steps. */
static bool de_gradient_present(diffequ_solve_result_t *result, const expr_t *dependent, const expr_t *parameter,
                                const expr_t *denominator, const expr_t *slope, const expr_t *offset,
                                const expr_t *profile_symbol, bool include_steps)
{
    const expr_t *values[] = {dependent, parameter, denominator, slope, offset,
                              equ_rhs(result->solutions[0]), equ_lhs(result->parameter_constraint),
                              equ_rhs(result->solutions[1]), profile_symbol};
    char *plain[9] = {NULL}, *TeX[9] = {NULL};
    string_t *lhs = NULL, *rhs = NULL, *lhs_TeX = NULL, *rhs_TeX = NULL;
    bool valid = false;
    for (size_t i = 0u; i < 9u; ++i) {
        const char *name = expr_symbol_name(values[i]);
        expr_t *symbol = name && (expr_is_variable(values[i]) || expr_is_named_const(values[i]))
                             ? expr_new_named_var(NUM_NAN, name) : NULL;
        plain[i] = expr_to_string(symbol ? symbol : values[i], style_UNBOUND);
        TeX[i] = expr_to_TeX_body(symbol ? symbol : values[i]);
        expr_free(symbol);
        if (!plain[i] || !TeX[i])
            goto cleanup;
    }
    lhs = string_sprintf("Local envelope: %s", plain[0]);
    rhs = string_sprintf("%s\n  subject to H = %s = 0; %s != 0; dH/d%s != 0.\n"
                         "  %s is arbitrary and twice continuously differentiable; solve jointly for %s.",
                         plain[5], plain[6], plain[2], plain[1], plain[8], plain[1]);
    lhs_TeX = string_sprintf("\\text{Local envelope: }%s", TeX[0]);
    rhs_TeX = string_sprintf("\\begin{aligned}[t]&%s\\\\"
        "&\\text{subject to }H=%s=0\\\\&%s\\ne0,\\quad\\frac{\\partial H}{\\partial %s}\\ne0\\\\"
        "&%s\\text{ arbitrary and twice continuously differentiable; solve locally for }%s\\end{aligned}",
        TeX[5], TeX[6], TeX[2], TeX[1], TeX[8], TeX[1]);
    if (!lhs || !rhs || !lhs_TeX || !rhs_TeX ||
        equ_set_display_unbound(result->solutions[0], lhs, rhs) != 0 ||
        equ_set_display_TeX(result->solutions[0], lhs_TeX, rhs_TeX) != 0)
        goto cleanup;
    string_free(lhs); string_free(rhs); string_free(lhs_TeX); string_free(rhs_TeX);
    lhs = string_sprintf("Affine complete integral: %s", plain[0]);
    rhs = string_sprintf("%s; %s != 0; %s and %s are arbitrary constants.", plain[7], plain[2], plain[1], plain[4]);
    lhs_TeX = string_sprintf("\\text{Affine complete integral: }%s", TeX[0]);
    rhs_TeX = string_sprintf("%s,\\quad %s\\ne0,\\quad %s,%s\\text{ arbitrary constants}",
                             TeX[7], TeX[2], TeX[1], TeX[4]);
    if (!lhs || !rhs || !lhs_TeX || !rhs_TeX ||
        equ_set_display_unbound(result->solutions[1], lhs, rhs) != 0 ||
        equ_set_display_TeX(result->solutions[1], lhs_TeX, rhs_TeX) != 0)
        goto cleanup;
    if (include_steps) {
        string_t *steps = string_sprintf(
            "Gradient-only equation: set p and q equal to the two first partial derivatives.\n"
            "The characteristic equations keep p and q constant along each characteristic.\n"
            "Parameterise the gradient curve by p = %s, q = %s, with %s != 0.\n"
            "Integrating constant gradients gives the affine complete integral.\n"
            "Replace its additive constant by %s(%s) and differentiate the resulting expression with respect to %s.\n"
            "Setting that derivative H to zero gives the envelope constraint.\n"
            "On H=0 the chain rule gives the original gradients p and q; the native solver verifies their PDE identity.\n"
            "dH/d%s != 0 determines a regular local branch. Affine solutions are listed separately.\n"
            "No initial data, boundary data or continuation through envelope singularities are asserted.",
            plain[1], plain[3], plain[2], plain[8], plain[1], plain[1], plain[1]);
        string_t *steps_TeX = string_sprintf(
            "\\begin{aligned}&\\text{Gradient-only PDE: characteristic slopes are constant.}"
            "\\\\&p=%s,\\quad q=%s,\\quad %s\\ne0"
            "\\\\&\\text{Replace the affine offset by }%s(%s)\\text{ and impose stationarity in }%s."
            "\\\\&H=%s=0"
            "\\\\&\\text{On }H=0\\text{, the chain rule recovers }p,q\\text{ satisfying the original PDE.}"
            "\\\\&\\frac{\\partial H}{\\partial %s}\\ne0\\text{ defines a regular local envelope.}"
            "\\\\&\\text{Affine families are separate; no initial or boundary data imposed.}"
            "\\\\&\\text{No continuation through envelope singularities asserted.}\\end{aligned}",
            TeX[1], TeX[3], TeX[2], TeX[8], TeX[1], TeX[1], TeX[6], TeX[1]);
        valid = steps && steps_TeX && de_solve_result_set_steps(result, string_c_str(steps)) == 0 &&
                de_solve_result_set_steps_TeX(result, string_c_str(steps_TeX)) == 0;
        string_free(steps_TeX);
        string_free(steps);
    } else {
        valid = true;
    }
cleanup:
    string_free(rhs_TeX); string_free(lhs_TeX); string_free(rhs); string_free(lhs);
    for (size_t i = 0u; i < 9u; ++i) {
        free(TeX[i]); free(plain[i]);
    }
    return valid;
}

/* Nondegenerate bilinear gradient curves A*p*q+B*p+C*q+D=0 admit affine and regular envelope families. */
diffequ_solve_result_t *de_pde_solve_gradient_envelope(const diffequ_t *de, const expr_t *residual, bool include_steps)
{
    if (!de || !residual || de->independent_count != 2u || de->condition_count)
        return NULL;
    const expr_t *x = de->independent_vars[0], *y = de->independent_vars[1];
    const expr_t *dependent = NULL, *dx = NULL, *dy = NULL;
    expr_t *coefficient = NULL, *rest = NULL, *A = NULL, *B = NULL, *C = NULL, *D = NULL;
    expr_t *parameter = NULL, *offset = NULL, *denominator = NULL, *slope = NULL, *surface = NULL;
    expr_t *constraint = NULL, *plane = NULL, *identity = NULL, *determinant = NULL;
    expr_t *profile_symbol = NULL;
    diffequ_solve_result_t *result = NULL;
    if (!de_pde_find_first_derivatives(residual, x, y, &dependent, &dx, &dy) || !dependent || !dx || !dy ||
        !de_linear_decompose(residual, dy, &coefficient, &rest) ||
        !de_linear_decompose(coefficient, dx, &A, &C) || !de_linear_decompose(rest, dx, &B, &D))
        goto cleanup;
    const expr_t *coefficients[] = {A, B, C, D};
    for (size_t i = 0u; i < 4u; ++i)
        if (!de_gradient_constant(coefficients[i], de, dependent))
            goto cleanup;
    determinant = expr_sub_simplify_owned(expr_mul(B, C), expr_mul(A, D));
    number_t a_value = expr_eval(A), delta = determinant ? expr_eval(determinant) : num_clone(NUM_NAN);
    bool valid = !num_is_zero(a_value) && num_is_finite(delta) && !num_is_zero(delta);
    num_destroy(&delta); num_destroy(&a_value);
    if (!valid)
        goto cleanup;
    parameter = de_gradient_parameter(de, dependent, "a", false);
    offset = de_gradient_parameter(de, dependent, "b", true);
    denominator = parameter ? expr_add_simplify_owned(expr_mul(A, parameter), expr_clone(C)) : NULL;
    expr_t *constant_slope = expr_negate_owned(expr_div_simplify_owned(expr_clone(B), expr_clone(A)));
    expr_t *correction = denominator ? expr_div_simplify_owned(expr_clone(determinant), expr_mul(A, denominator)) : NULL;
    slope = expr_add_simplify_owned(constant_slope, correction);
    expr_t *base = slope ? expr_add_simplify_owned(expr_mul(parameter, x), expr_mul(slope, y)) : NULL;
    profile_symbol = de_gradient_parameter(de, dependent, "F", false);
    expr_t *profile = parameter && profile_symbol
        ? expr_new_arbitrary_function(expr_symbol_name(profile_symbol), parameter) : NULL;
    surface = base && profile ? expr_add_simplify_owned(expr_clone(base), expr_clone(profile)) : NULL;
    plane = base && offset ? expr_add_simplify_owned(expr_clone(base), expr_clone(offset)) : NULL;
    expr_t *profile_derivative = profile ? expr_create_deriv(profile, parameter) : NULL;
    expr_t *slope_derivative = denominator
        ? expr_div_simplify_owned(expr_neg(determinant), expr_mul(denominator, denominator)) : NULL;
    constraint = slope_derivative && profile_derivative
        ? expr_add_simplify_owned(expr_add_simplify_owned(expr_clone(x), expr_mul(slope_derivative, y)), profile_derivative)
        : NULL;
    if (!slope_derivative)
        expr_free(profile_derivative);
    expr_free(slope_derivative);
    expr_free(profile); expr_free(base);
    expr_t *first = slope ? expr_substitute(residual, dx, parameter) : NULL;
    identity = first ? expr_substitute(first, dy, slope) : NULL;
    expr_free(first);
    if (!surface || !plane || !constraint || !identity || !de_pde_is_symbolically_zero(identity))
        goto cleanup;
    /* H=S_a; hence S_x+H*a_x=a and S_y+H*a_y=Q(a) on H=0. Verify the fixed-parameter derivatives too. */
    expr_t *sx = expr_create_deriv(surface, x), *sy = expr_create_deriv(surface, y);
    expr_t *ex = sx ? expr_sub(sx, parameter) : NULL, *ey = sy ? expr_sub(sy, slope) : NULL;
    expr_t *sa = expr_create_deriv(surface, parameter), *eh = sa ? expr_sub(sa, constraint) : NULL;
    valid = ex && ey && eh && de_pde_is_symbolically_zero(ex) && de_pde_is_symbolically_zero(ey) &&
            de_pde_is_symbolically_zero(eh);
    expr_free(eh); expr_free(sa);
    expr_free(ey); expr_free(ex); expr_free(sy); expr_free(sx);
    if (!valid)
        goto cleanup;
    result = de_solve_result_new(DE_SOLVE_STATUS_SOLVED, DE_SOLVER_CHARACTERISTICS,
                                 "regular local envelope with a coupled parameter constraint; "
                                 "affine complete integral separately");
    if (!result)
        goto cleanup;
    result->envelope_parameter = expr_clone(parameter);
    expr_t *zero = expr_const_zero();
    result->parameter_constraint = zero ? equ_new(constraint, zero) : NULL;
    expr_free(zero);
    equation_t *envelope = de_pde_solution_equation(dependent, surface);
    equation_t *affine = de_pde_solution_equation(dependent, plane);
    if (!result->envelope_parameter || !result->parameter_constraint || !envelope || !affine ||
        de_solve_result_append(result, envelope) != 0) {
        equ_free(envelope); equ_free(affine);
        goto failure;
    }
    if (de_solve_result_append(result, affine) != 0) {
        equ_free(affine);
        goto failure;
    }
    if (!de_gradient_present(result, dependent, parameter, denominator, slope, offset,
                             profile_symbol, include_steps))
        goto failure;
    goto cleanup;
failure:
    de_solve_result_free(result);
    result = NULL;
cleanup:
    expr_free(profile_symbol);
    expr_free(determinant); expr_free(identity); expr_free(plane); expr_free(constraint); expr_free(surface);
    expr_free(slope); expr_free(denominator); expr_free(offset); expr_free(parameter);
    expr_free(D); expr_free(C); expr_free(B); expr_free(A); expr_free(rest); expr_free(coefficient);
    return result;
}
