#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MARS_DIFFEQUATION_PDE_INTERNAL_ACCESS
#include "diffequ_pde_internal.h"

enum { DE_FOURIER_MAX_ORDER = 8 };

static bool de_fourier_has_name(const expr_t *expr, const char *name)
{
    const char *symbol = expr ? expr_symbol_name(expr) : NULL;
    const expr_t *left = NULL, *right = NULL;
    return (symbol && strcmp(symbol, name) == 0) ||
           (expr_child_exprs(expr, &left, &right) &&
            (de_fourier_has_name(left, name) || de_fourier_has_name(right, name)));
}

/* Integral binders must not capture a parameter or a symbol in the initial data. */
static expr_t *de_fourier_symbol(const diffequ_t *de, const expr_t *residual, const char *preferred)
{
    char name[64];
    snprintf(name, sizeof(name), "%s", preferred);
    for (size_t suffix = 1u; ; ++suffix) {
        expr_t *symbol = expr_new_named_var(NUM_NAN, name);
        if (!symbol)
            return NULL;
        const char *canonical = expr_symbol_name(symbol);
        if (!de_fourier_has_name(residual, canonical) &&
            !de_fourier_has_name(equ_rhs(de->conditions[0]), canonical) &&
            !de_fourier_has_name(de->condition_points[0][0], canonical) &&
            !de_fourier_has_name(de->condition_points[0][1], canonical) && !de_constant(de, canonical))
            return symbol;
        expr_free(symbol);
        snprintf(name, sizeof(name), "%s_%zu", preferred, suffix);
    }
}

/* Unknown scalar symbols are real constant parameters, not aliases for the dependent variable. */
static bool de_fourier_constant(const expr_t *expr, const diffequ_t *de, const expr_t *dependent)
{
    if (!expr || de_expr_uses(expr, dependent) || de_expr_uses(expr, de->independent_vars[0]) ||
        de_expr_uses(expr, de->independent_vars[1]))
        return false;
    number_t value = expr_eval(expr);
    bool valid = num_is_nan(value) || (num_is_real(value) && num_is_finite(value));
    number_t literal = num_new();
    if (!expr_symbol_name(expr) && expr_match_const_value(expr, &literal) && !num_is_finite(literal))
        valid = false;
    num_destroy(&literal);
    num_destroy(&value);
    return valid;
}

/* Accept pure spatial derivatives and exactly one first time derivative; reject mixed or nonlinear operators. */
static bool de_fourier_derivatives(const expr_t *expr, const expr_t *dependent, const expr_t *time,
                                   const expr_t *space, const expr_t **temporal, const expr_t **spatial)
{
    if (expr_is_formal_derivative(expr)) {
        if (!expr_struct_eq(expr_formal_derivative_dependent(expr), dependent))
            return false;
        size_t order = expr_formal_derivative_order(expr);
        if (order == 1u && expr_struct_eq(expr_formal_derivative_wrt_at(expr, 0u), time)) {
            *temporal = expr;
            return true;
        }
        if (!order || order > DE_FOURIER_MAX_ORDER)
            return false;
        for (size_t i = 0u; i < order; ++i)
            if (!expr_struct_eq(expr_formal_derivative_wrt_at(expr, i), space))
                return false;
        spatial[order] = expr;
        return true;
    }
    const expr_t *left = NULL, *right = NULL;
    return !expr_child_exprs(expr, &left, &right) ||
           (de_fourier_derivatives(left, dependent, time, space, temporal, spatial) &&
            de_fourier_derivatives(right, dependent, time, space, temporal, spatial));
}

/* Build the real and imaginary parts of P(ik) directly by derivative order, without case dispatch. */
static bool de_fourier_polynomials(const diffequ_t *de, const expr_t *residual, const expr_t *dependent,
                                   const expr_t *time, const expr_t *space, const expr_t *frequency,
                                   expr_t **real_out, expr_t **imaginary_out)
{
    const expr_t *temporal = NULL, *spatial[DE_FOURIER_MAX_ORDER + 1] = {dependent};
    expr_t *clock = NULL, *remainder = NULL, *parts[2] = {NULL, NULL};
    bool valid = false, damping = false;
    size_t highest_even = 0u;
    if (!de_fourier_derivatives(residual, dependent, time, space, &temporal, spatial) || !temporal ||
        !de_linear_decompose(residual, temporal, &clock, &remainder) || !de_fourier_constant(clock, de, dependent))
        goto cleanup;
    number_t clock_value = expr_eval(clock);
    bool clock_valid = num_is_real(clock_value) && num_is_finite(clock_value) && !num_eq(clock_value, NUM_ZERO);
    num_destroy(&clock_value);
    if (!clock_valid)
        goto cleanup;
    parts[0] = expr_const_zero();
    parts[1] = expr_const_zero();
    for (size_t slot = 1u; slot <= DE_FOURIER_MAX_ORDER + 1u; ++slot) {
        size_t order = slot % (DE_FOURIER_MAX_ORDER + 1u);
        if (!spatial[order])
            continue;
        expr_t *coefficient = NULL, *next = NULL;
        if (!de_linear_decompose(remainder, spatial[order], &coefficient, &next) ||
            !de_fourier_constant(coefficient, de, dependent)) {
            expr_free(coefficient);
            expr_free(next);
            goto cleanup;
        }
        expr_free(remainder);
        remainder = next;
        coefficient = expr_div_simplify_owned(coefficient, expr_clone(clock));
        if ((order / 2u) % 2u)
            coefficient = expr_negate_owned(coefficient);
        if (order && order % 2u == 0u && !expr_is_exact_zero(coefficient)) {
            number_t value = expr_eval(coefficient);
            damping = num_is_real(value) && num_is_finite(value) && num_gt(value, NUM_ZERO);
            highest_even = order;
            num_destroy(&value);
        }
        expr_t *power = expr_simplify_owned(expr_pow_long(frequency, (long)order));
        parts[order % 2u] = expr_add_simplify_owned(parts[order % 2u], expr_mul_simplify_owned(coefficient, power));
        if (!parts[order % 2u])
            goto cleanup;
    }
    if (!highest_even || !damping || !de_pde_is_symbolically_zero(remainder))
        goto cleanup;
    *real_out = parts[0];
    *imaginary_out = parts[1];
    parts[0] = parts[1] = NULL;
    valid = true;
cleanup:
    expr_free(parts[1]); expr_free(parts[0]); expr_free(remainder); expr_free(clock);
    return valid;
}

static bool de_fourier_steps(diffequ_solve_result_t *result, const expr_t *real, const expr_t *imaginary,
                             const expr_t *frequency, const expr_t *time, const expr_t *initial_time)
{
    char *r = expr_to_TeX_body(real), *s = expr_to_TeX_body(imaginary), *k = expr_to_TeX_body(frequency);
    const char *time_name = expr_symbol_name(time);
    expr_t *time_symbol = time_name ? expr_new_named_var(NUM_NAN, time_name) : NULL;
    char *t = time_symbol ? expr_to_TeX_body(time_symbol) : NULL, *t0 = expr_to_TeX_body(initial_time);
    expr_free(time_symbol);
    const char *plain =
        "Whole-line Fourier evolution with real constant coefficients.\n"
        "Distinct symbols remain distinct: a coefficient such as U is not the dependent variable u.\n"
        "Replace each spatial derivative of order n by (i*k)^n, obtaining P(i*k)=R(k)+i*S(k).\n"
        "Each Fourier mode evolves by exp(-(t-t0)*P(i*k)).\n"
        "Pair positive and negative frequencies to obtain the real cosine kernel, then convolve with the initial data.\n"
        "The highest even power in R has a known positive coefficient, ensuring high-frequency decay for t>t0.\n"
        "Assumptions: whole real spatial line, real finite coefficient parameters, t>t0, Schwartz initial data.\n"
        "Schwartz means smooth with the function and all derivatives decaying faster than every inverse power.\n"
        "The initial condition holds as t tends to t0 from above by Fourier inversion, not by an ordinary kernel at t0.\n"
        "This is an exact integral representation; no numerical integral or boundary-value problem is claimed.\n";
    string_t *TeX = r && s && k && t && t0 ? string_sprintf(
        "\\begin{aligned}&\\text{Whole-line Fourier evolution; real constant parameters.}"
        "\\\\&\\tau=%s-(%s)>0,\\quad R(%s)=%s,\\quad S(%s)=%s"
        "\\\\&\\text{The Fourier transform uses the factor }e^{-i%s\\,\\text{(spatial coordinate)}}."
        "\\\\&\\widehat{u}\\text{ denotes the spatial Fourier transform of the solution.}"
        "\\\\&\\partial_{\\tau}\\widehat{u}=-(R+iS)\\widehat{u},\\quad "
        "\\widehat{u}(\\tau)=e^{-\\tau(R+iS)}\\widehat{u}(0)."
        "\\\\&\\text{Pair opposite frequencies and convolve the resulting kernel with the supplied initial data.}"
        "\\\\&\\text{Assume smooth, rapidly decreasing initial data on the whole real line.}"
        "\\\\&\\text{All derivatives of the data must also decrease faster than every inverse power.}"
        "\\\\&\\text{Initial data are recovered as }\\tau\\downarrow0\\text{ by Fourier inversion.}"
        "\\\\&\\text{Exact integral representation; no finite-interval boundary conditions.}\\end{aligned}",
        t, t0, k, r, k, s, k) : NULL;
    bool valid = TeX && de_solve_result_set_steps(result, plain) == 0 &&
                 de_solve_result_set_steps_TeX(result, string_c_str(TeX)) == 0;
    string_free(TeX);
    free(t0); free(t); free(k); free(s); free(r);
    return valid;
}

/* Solve a first-time-order, constant-spatial-coefficient IVP by its dissipative whole-line kernel. */
diffequ_solve_result_t *de_pde_solve_fourier_evolution(const diffequ_t *de, const expr_t *residual, bool include_steps)
{
    if (!de || !residual || de->independent_count != 2u || de->condition_count != 1u ||
        de->condition_point_counts[0] != 2u)
        return NULL;
    const expr_t *dependent = equ_lhs(de->conditions[0]), *initial = equ_rhs(de->conditions[0]);
    if (!expr_is_variable(dependent))
        return NULL;
    size_t space_index = 2u, point_index = 2u;
    for (size_t i = 0u; i < 2u; ++i)
        for (size_t j = 0u; j < 2u; ++j)
            if (expr_struct_eq(de->independent_vars[i], de->condition_points[0][j])) {
                if (space_index != 2u)
                    return NULL;
                space_index = i;
                point_index = j;
            }
    if (space_index == 2u)
        return NULL;
    const expr_t *space = de->independent_vars[space_index], *time = de->independent_vars[1u - space_index];
    const expr_t *initial_time = de->condition_points[0][1u - point_index];
    number_t value = expr_eval(initial_time);
    bool valid = num_is_real(value) && num_is_finite(value);
    num_destroy(&value);
    if (!valid || !de_fourier_constant(initial_time, de, dependent) ||
        de_expr_uses(initial, time) || de_expr_uses(initial, dependent))
        return NULL;
    expr_t *k = de_fourier_symbol(de, residual, "k"), *xi = de_fourier_symbol(de, residual, "ξ");
    expr_t *real = NULL, *imaginary = NULL, *right = NULL;
    diffequ_solve_result_t *result = NULL;
    equation_t *solution = NULL;
    if (!k || !xi || !de_fourier_polynomials(de, residual, dependent, time, space, k, &real, &imaginary))
        goto cleanup;
    expr_t *tau = expr_sub_simplify_owned(expr_clone(time), expr_clone(initial_time));
    expr_t *phase = expr_sub_simplify_owned(expr_mul_simplify_owned(expr_clone(k), expr_sub(space, xi)),
                                           expr_mul(tau, imaginary));
    expr_t *exponent = expr_negate_owned(expr_mul_simplify_owned(expr_clone(tau), expr_clone(real)));
    expr_t *mode = phase && exponent ? expr_mul_simplify_owned(expr_exp(exponent), expr_cos(phase)) : NULL;
    expr_t *zero = expr_const_zero(), *infinity = expr_new_const(NUM_INF), *negative_infinity = expr_new_const(NUM_NINF);
    expr_t *kernel = mode ? expr_integral_with_bounds_internal(mode, zero, infinity, k) : NULL;
    expr_t *data = expr_substitute(initial, space, xi);
    expr_t *integrand = data && kernel ? expr_mul(data, kernel) : NULL;
    expr_t *convolution = integrand ? expr_integral_with_bounds_internal(integrand, negative_infinity, infinity, xi) : NULL;
    expr_t *pi = expr_new_named_const(NUM_PI, "π"), *one = expr_const_one();
    expr_t *scale = expr_div(one, pi);
    right = convolution && scale ? expr_mul(scale, convolution) : NULL;
    expr_free(one); expr_free(scale); expr_free(pi); expr_free(convolution); expr_free(integrand);
    expr_free(data); expr_free(kernel);
    expr_free(negative_infinity); expr_free(infinity); expr_free(zero); expr_free(mode);
    expr_free(exponent); expr_free(phase); expr_free(tau);
    solution = right ? de_pde_solution_equation(dependent, right) : NULL;
    result = solution ? de_solve_result_new(DE_SOLVE_STATUS_SOLVED, DE_SOLVER_FOURIER_EVOLUTION,
        "exact whole-line Fourier kernel for t>t0; real constant parameters and Schwartz initial data; "
        "initial condition recovered by the right-hand limit") : NULL;
    if (!result || (include_steps && !de_fourier_steps(result, real, imaginary, k, time, initial_time)) ||
        de_solve_result_append(result, solution) != 0) {
        de_solve_result_free(result);
        result = NULL;
    } else {
        solution = NULL;
    }
cleanup:
    equ_free(solution); expr_free(right); expr_free(imaginary); expr_free(real); expr_free(xi); expr_free(k);
    return result;
}
