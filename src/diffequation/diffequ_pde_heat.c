#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MARS_DIFFEQUATION_PDE_INTERNAL_ACCESS
#include "diffequ_pde_internal.h"
#define MARS_SHARED_EQUATION_INTERNAL_ACCESS
#include "internal/equation_internal.h"

/* The derivative orders determine time and space independently of spelling and term order. */
static bool de_heat_derivatives(const expr_t *expr, const expr_t **field, const expr_t **dt, const expr_t **dxx)
{
    if (expr_is_formal_derivative(expr)) {
        const expr_t *dependent = expr_formal_derivative_dependent(expr);
        size_t order = expr_formal_derivative_order(expr);
        if (!expr_is_variable(dependent) || (*field && !expr_struct_eq(*field, dependent)) ||
            (order != 1u && order != 2u))
            return false;
        *field = dependent;
        const expr_t **slot = order == 1u ? dt : dxx;
        if ((*slot && !expr_struct_eq(*slot, expr)) ||
            (order == 2u && !expr_struct_eq(expr_formal_derivative_wrt_at(expr, 0u),
                                           expr_formal_derivative_wrt_at(expr, 1u))))
            return false;
        *slot = expr;
        return true;
    }
    const expr_t *left = NULL, *right = NULL;
    return !expr_child_exprs(expr, &left, &right) ||
           (de_heat_derivatives(left, field, dt, dxx) && de_heat_derivatives(right, field, dt, dxx));
}

/* Unknown scalar parameters are real and finite under the explicit result assumptions.
   Do not interpret an unevaluated function or a nonfinite literal as a scalar parameter. */
static bool de_heat_constant(const expr_t *expr, const expr_t *field, const expr_t *time, const expr_t *space)
{
    if (!expr || de_expr_uses(expr, field) || de_expr_uses(expr, time) || de_expr_uses(expr, space))
        return false;
    number_t value = expr_eval(expr);
    bool valid = num_is_real(value) && num_is_finite(value);
    if (!valid && num_is_nan(value)) {
        if (expr_symbol_name(expr) && (expr_is_variable(expr) || expr_is_named_const(expr))) {
            valid = true;
        } else {
            const expr_t *left = NULL, *right = NULL;
            bool subtract = false;
            if (expr_match_add_sub_expr(expr, &left, &right, &subtract) ||
                expr_match_mul_expr(expr, &left, &right))
                valid = de_heat_constant(left, field, time, space) && de_heat_constant(right, field, time, space);
            else if (expr_match_neg_expr(expr, &left))
                valid = de_heat_constant(left, field, time, space);
        }
    }
    num_destroy(&value);
    return valid;
}

/* Coordinate slots in the two data calls must agree; the parser's independent-variable order may differ. */
static const expr_t *de_heat_data(const diffequ_t *de, const expr_t *field, const expr_t *time, const expr_t *space)
{
    for (size_t i = 0u; i < 2u; ++i) {
        if (de->condition_point_counts[i] != 2u || !expr_struct_eq(equ_lhs(de->conditions[i]), field))
            return NULL;
    }
    for (size_t initial = 0u; initial < 2u; ++initial) {
        size_t boundary = 1u - initial;
        if (!de_pde_is_symbolically_zero(equ_rhs(de->conditions[initial])))
            continue;
        for (size_t slot = 0u; slot < 2u; ++slot) {
            if (expr_struct_eq(de->condition_points[initial][slot], space) &&
                de_pde_is_symbolically_zero(de->condition_points[initial][1u-slot]) &&
                de_pde_is_symbolically_zero(de->condition_points[boundary][slot]) &&
                expr_struct_eq(de->condition_points[boundary][1u-slot], time)) {
                const expr_t *data = equ_rhs(de->conditions[boundary]);
                if (de_expr_uses(data, field) || de_expr_uses(data, space))
                    return NULL;
                expr_t *zero = expr_const_zero(), *corner = expr_substitute(data, time, zero);
                number_t value = corner ? expr_eval(corner) : num_clone(NUM_NAN);
                bool compatible = corner && (num_is_nan(value) || (num_is_real(value) && num_is_zero(value)));
                num_destroy(&value); expr_free(corner); expr_free(zero);
                return compatible ? data : NULL;
            }
        }
    }
    return NULL;
}

static bool de_heat_has_name(const expr_t *expr, const char *name)
{
    const char *symbol = expr ? expr_symbol_name(expr) : NULL;
    const expr_t *left = NULL, *right = NULL;
    return (symbol && strcmp(symbol, name) == 0) ||
           (expr_child_exprs(expr, &left, &right) &&
            (de_heat_has_name(left, name) || de_heat_has_name(right, name)));
}

static expr_t *de_heat_dummy(const diffequ_t *de, const expr_t *residual, const expr_t *data,
                            const expr_t *field, const expr_t *time, const expr_t *space)
{
    for (size_t suffix = 0u; ; ++suffix) {
        char name[64];
        if (suffix) snprintf(name, sizeof(name), "s_%zu", suffix);
        else snprintf(name, sizeof(name), "s");
        expr_t *symbol = expr_new_named_var(NUM_NAN, name);
        if (!symbol)
            return NULL;
        const char *canonical = expr_symbol_name(symbol);
        if (!de_constant(de, canonical) && !de_heat_has_name(residual, canonical) &&
            !de_heat_has_name(data, canonical) && !de_heat_has_name(field, canonical) &&
            !de_heat_has_name(time, canonical) && !de_heat_has_name(space, canonical))
            return symbol;
        expr_free(symbol);
    }
}

/* Domain and limiting boundary interpretation remain visible when optional steps are disabled. */
static bool de_heat_present(equation_t *solution, const expr_t *space, const expr_t *time,
                            const expr_t *diffusivity, const expr_t *reaction)
{
    expr_t *x = expr_new_named_var(NUM_NAN, expr_symbol_name(space));
    expr_t *t = expr_new_named_var(NUM_NAN, expr_symbol_name(time));
    const expr_t *values[] = {equ_lhs(solution), equ_rhs(solution), x, t, diffusivity, reaction};
    char *plain[6] = {NULL}, *TeX[6] = {NULL};
    string_t *lhs = NULL, *rhs = NULL, *lhs_TeX = NULL, *rhs_TeX = NULL;
    bool valid = false;
    for (size_t i = 0u; i < 6u; ++i) {
        plain[i] = values[i] ? expr_to_string(values[i], style_UNBOUND) : NULL;
        TeX[i] = values[i] ? expr_to_TeX_body(values[i]) : NULL;
        if (!plain[i] || !TeX[i]) goto cleanup;
    }
    lhs = string_sprintf("%s", plain[0]);
    rhs = string_sprintf("%s; %s > 0, %s > 0; right half-line, decay at infinity; "
        "continuous locally bounded boundary data, zero at the corner; boundary value by the right-hand spatial limit.",
        plain[1], plain[2], plain[3]);
    lhs_TeX = string_sprintf("%s", TeX[0]);
    rhs_TeX = string_sprintf(
        "\\begin{aligned}[t]&%s\\\\&%s>0,\\quad %s>0,\\quad\\kappa=%s>0,\\quad %s\\in\\mathbb{R}"
        "\\\\&\\text{Right half-line; decay at spatial infinity.}"
        "\\\\&\\text{Continuous, locally bounded boundary data; zero at the corner.}"
        "\\\\&\\text{Boundary value is the right-hand spatial limit, not substitution at zero.}\\end{aligned}",
        TeX[1], TeX[2], TeX[3], TeX[4], TeX[5]);
    valid = lhs && rhs && lhs_TeX && rhs_TeX && equ_set_display_unbound(solution, lhs, rhs) == 0 &&
            equ_set_display_TeX(solution, lhs_TeX, rhs_TeX) == 0;
cleanup:
    string_free(rhs_TeX); string_free(lhs_TeX); string_free(rhs); string_free(lhs);
    for (size_t i = 0u; i < 6u; ++i) { free(TeX[i]); free(plain[i]); }
    expr_free(t); expr_free(x);
    return valid;
}

/* Heat Poisson kernel for zero initial data on x>0 with a prescribed Dirichlet history at x=0. */
diffequ_solve_result_t *de_pde_solve_half_line_heat(const diffequ_t *de, const expr_t *residual, bool include_steps)
{
    if (!de || !residual || de->independent_count != 2u || de->condition_count != 2u)
        return NULL;
    const expr_t *field = NULL, *dt = NULL, *dxx = NULL;
    if (!de_heat_derivatives(residual, &field, &dt, &dxx) || !dt || !dxx)
        return NULL;
    const expr_t *time = expr_formal_derivative_wrt_at(dt, 0u), *space = expr_formal_derivative_wrt_at(dxx, 0u);
    if (expr_struct_eq(time, space))
        return NULL;
    const expr_t *data = de_heat_data(de, field, time, space);
    if (!data)
        return NULL;
    expr_t *clock = NULL, *spatial = NULL, *reaction = NULL, *r1 = NULL, *r2 = NULL, *rest = NULL;
    expr_t *diffusivity = NULL, *dummy = NULL, *tau = NULL, *kernel = NULL, *right = NULL;
    equation_t *solution = NULL;
    diffequ_solve_result_t *result = NULL;
    if (!de_linear_decompose(residual, dt, &clock, &r1) || !de_linear_decompose(r1, dxx, &spatial, &r2) ||
        !de_linear_decompose(r2, field, &reaction, &rest) || !de_pde_is_symbolically_zero(rest) ||
        !de_heat_constant(clock, field, time, space) || !de_heat_constant(spatial, field, time, space) ||
        !de_heat_constant(reaction, field, time, space))
        goto cleanup;
    number_t clock_value = expr_eval(clock);
    bool valid = num_is_real(clock_value) && num_is_finite(clock_value) && !num_is_zero(clock_value);
    num_destroy(&clock_value);
    if (!valid) goto cleanup;
    diffusivity = expr_div_simplify_owned(expr_neg(spatial), expr_clone(clock));
    number_t value = diffusivity ? expr_eval(diffusivity) : num_clone(NUM_NAN);
    valid = num_is_real(value) && num_is_finite(value) && num_gt(value, NUM_ZERO);
    bool unit_diffusivity = num_eq(value, NUM_ONE);
    num_destroy(&value);
    if (!valid) goto cleanup;
    reaction = expr_div_simplify_owned(reaction, expr_clone(clock));
    dummy = de_heat_dummy(de, residual, data, field, time, space);
    tau = dummy ? expr_sub(time, dummy) : NULL;
    expr_t *denominator = tau ? expr_mul_simplify_owned(expr_const_long(4L), expr_mul(diffusivity, tau)) : NULL;
    expr_t *exponent = denominator ? expr_negate_owned(expr_add_simplify_owned(
        expr_div_simplify_owned(expr_pow_long(space, 2L), denominator), expr_mul(reaction, tau))) : NULL;
    number_t power = num_create_from_frac(-3L, 2L);
    expr_t *decay = exponent ? expr_exp(exponent) : NULL, *time_power = tau ? expr_pow(tau, &power) : NULL;
    kernel = decay && time_power ? expr_mul(time_power, decay) : NULL;
    expr_t *history = dummy ? expr_substitute(data, time, dummy) : NULL;
    expr_t *integrand = kernel && history ? expr_mul(kernel, history) : NULL;
    expr_t *zero = expr_const_zero();
    expr_t *integral = integrand ? expr_integral_with_bounds_internal(integrand, zero, time, dummy) : NULL;
    expr_t *pi = expr_new_named_const(NUM_PI, "π");
    expr_t *product = unit_diffusivity ? expr_clone(pi) : expr_mul(pi, diffusivity);
    expr_t *root = product ? expr_sqrt(product) : NULL, *two = expr_const_long(2L);
    expr_t *normalisation = root ? expr_mul(two, root) : NULL;
    expr_t *factor = normalisation ? expr_div(space, normalisation) : NULL;
    right = integral && factor ? expr_mul(factor, integral) : NULL;
    if (de_pde_is_symbolically_zero(data)) {
        expr_free(right); right = expr_const_zero();
    }
    expr_free(factor); expr_free(normalisation); expr_free(two); expr_free(root); expr_free(product); expr_free(pi);
    expr_free(integral); expr_free(zero); expr_free(integrand); expr_free(history);
    expr_free(time_power); expr_free(decay); expr_free(exponent); num_destroy(&power);
    solution = right ? de_pde_solution_equation(field, right) : NULL;
    if (!solution || !de_heat_present(solution, space, time, diffusivity, reaction)) goto cleanup;
    result = de_solve_result_new(DE_SOLVE_STATUS_SOLVED, DE_SOLVER_HALF_LINE_HEAT,
        "exact heat boundary kernel on the right half-line; real constant reaction, known positive diffusivity; "
        "continuous locally bounded boundary data with zero corner value; decay at infinity; "
        "boundary trace by the right-hand spatial limit");
    if (!result) goto cleanup;
    if (include_steps && (de_solve_result_set_steps(result,
        "Normalise to u_t=kappa*u_xx-a*u, with constant kappa>0 and real constant a.\n"
        "On the right half-line use zero initial data, prescribed Dirichlet history and decay at infinity.\n"
        "Multiplication by exp(a*t) removes the reaction term.\n"
        "The heat Dirichlet Poisson kernel is x*exp(-x^2/(4*kappa*tau))/(2*sqrt(pi*kappa)*tau^(3/2)).\n"
        "Multiply by exp(-a*tau) and convolve with the boundary history.\n"
        "For x>0 the integrand vanishes rapidly at tau=0; it solves the PDE and has zero initial trace.\n"
        "As x tends to zero from above, the kernel is an approximate identity and recovers continuous boundary data.\n"
        "Do not substitute x=0 inside the integral: that would miss the boundary limit.\n"
        "Assume continuous locally bounded boundary data, zero at t=0 for corner compatibility.\n"
        "The result decays at spatial infinity on finite time intervals; "
        "no left half-line or finite interval is asserted.") != 0 ||
        de_solve_result_set_steps_TeX(result,
        "\\begin{aligned}&u_t=\\kappa u_{xx}-a u,\\quad\\kappa>0,\\quad a\\in\\mathbb{R}"
        "\\\\&w=e^{at}u\\quad\\Longrightarrow\\quad w_t=\\kappa w_{xx}"
        "\\\\&P(x,\\tau)=\\tfrac{x}{2\\sqrt{\\pi\\kappa}}\\tau^{-3/2}"
        "e^{-x^2/(4\\kappa\\tau)-a\\tau},\\quad x,\\tau>0"
        "\\\\&u=\\int_0^t P(x,t-s)g(s)\\,ds"
        "\\\\&\\text{The kernel solves the PDE and has zero initial trace for }x>0."
        "\\\\&\\lim_{x\\downarrow0}u(x,t)=g(t)\\quad\\text{by the approximate-identity boundary limit.}"
        "\\\\&\\text{Assume continuous locally bounded data, }g(0)=0\\text{, and decay at infinity.}"
        "\\\\&\\text{Here }x,t,g\\text{ denote the recognised space, time and boundary history.}\\end{aligned}") != 0))
        goto failure;
    if (de_solve_result_append(result, solution) != 0) goto failure;
    solution = NULL;
    goto cleanup;
failure:
    de_solve_result_free(result); result = NULL;
cleanup:
    equ_free(solution); expr_free(right); expr_free(kernel); expr_free(tau); expr_free(dummy); expr_free(diffusivity);
    expr_free(rest); expr_free(r2); expr_free(r1); expr_free(reaction); expr_free(spatial); expr_free(clock);
    return result;
}
