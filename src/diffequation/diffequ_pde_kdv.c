#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MARS_DIFFEQUATION_PDE_INTERNAL_ACCESS
#include "diffequ_pde_internal.h"
#define MARS_SHARED_EQUATION_INTERNAL_ACCESS
#include "internal/equation_internal.h"

/* The third derivative identifies the spatial coordinate without relying on its name or input order. */
static const expr_t *de_kdv_third(const expr_t *expr)
{
    if (expr_is_formal_derivative(expr))
        return expr_formal_derivative_order(expr) == 3u ? expr : NULL;
    const expr_t *left = NULL, *right = NULL;
    if (!expr_child_exprs(expr, &left, &right))
        return NULL;
    const expr_t *found = de_kdv_third(left);
    return found ? found : de_kdv_third(right);
}

/* Only the first temporal and first/third spatial derivatives of the same field are permitted. */
static bool de_kdv_derivatives(const expr_t *expr, const expr_t *field, const expr_t *time, const expr_t *space,
                               const expr_t **dt, const expr_t **dx)
{
    if (expr_is_formal_derivative(expr)) {
        size_t order = expr_formal_derivative_order(expr);
        if (!expr_struct_eq(expr_formal_derivative_dependent(expr), field))
            return false;
        if (order == 1u && expr_struct_eq(expr_formal_derivative_wrt_at(expr, 0u), time)) {
            *dt = expr;
            return true;
        }
        if (order != 1u && order != 3u)
            return false;
        for (size_t i = 0u; i < order; ++i)
            if (!expr_struct_eq(expr_formal_derivative_wrt_at(expr, i), space))
                return false;
        if (order == 1u)
            *dx = expr;
        return true;
    }
    const expr_t *left = NULL, *right = NULL;
    return !expr_child_exprs(expr, &left, &right) ||
           (de_kdv_derivatives(left, field, time, space, dt, dx) &&
            de_kdv_derivatives(right, field, time, space, dt, dx));
}

static bool de_kdv_constant(const expr_t *expr, const expr_t *field, const expr_t *time, const expr_t *space)
{
    if (!expr || de_expr_uses(expr, field) || de_expr_uses(expr, time) || de_expr_uses(expr, space))
        return false;
    number_t value = expr_eval(expr);
    bool valid = num_is_real(value) && num_is_finite(value) && !num_is_zero(value);
    num_destroy(&value);
    return valid;
}

static bool de_kdv_has_name(const expr_t *expr, const char *name)
{
    if (expr_is_formal_derivative(expr)) {
        if (de_kdv_has_name(expr_formal_derivative_dependent(expr), name))
            return true;
        for (size_t i = 0u; i < expr_formal_derivative_order(expr); ++i)
            if (de_kdv_has_name(expr_formal_derivative_wrt_at(expr, i), name))
                return true;
    }
    const char *symbol = expr ? expr_symbol_name(expr) : NULL;
    const expr_t *left = NULL, *right = NULL;
    return (symbol && strcmp(symbol, name) == 0) ||
           (expr_child_exprs(expr, &left, &right) &&
            (de_kdv_has_name(left, name) || de_kdv_has_name(right, name)));
}

expr_t *de_pde_kdv_parameter(const diffequ_t *de, const expr_t *residual, const char *base)
{
    for (size_t i = 0u; ; ++i) {
        char name[64];
        if (i)
            snprintf(name, sizeof(name), "%s_%zu", base, i);
        else
            snprintf(name, sizeof(name), "%s", base);
        expr_t *symbol = expr_new_named_var(NUM_NAN, name);
        if (!symbol)
            return NULL;
        const char *canonical = expr_symbol_name(symbol);
        bool used = de_constant(de, canonical) || de_kdv_has_name(residual, canonical);
        /* The two independent names may live only inside formal derivative metadata. */
        for (size_t j = 0u; j < 2u; ++j)
            used = used || de_kdv_has_name(de->independent_vars[j], canonical);
        if (!used)
            return symbol;
        expr_free(symbol);
    }
}

/* Put the scope and parameter restrictions in the solution itself, even when steps are disabled. */
static bool de_kdv_present(equation_t *solution, const expr_t *k, const expr_t *position)
{
    expr_t *field = expr_new_named_var(NUM_NAN, expr_symbol_name(equ_lhs(solution)));
    char *u = field ? expr_to_TeX_body(field) : NULL, *rhs = expr_to_TeX_body(equ_rhs(solution));
    char *kt = expr_to_TeX_body(k), *pt = expr_to_TeX_body(position);
    char *plain = expr_to_string(equ_rhs(solution), style_UNBOUND);
    string_t *lhs = string_sprintf("Solitary-wave family (not the general solution): %s",
                                   expr_symbol_name(equ_lhs(solution)));
    string_t *right = plain ? string_sprintf("%s; %s > 0, %s real (arbitrary constants).", plain,
                                             expr_symbol_name(k), expr_symbol_name(position)) : NULL;
    string_t *lhs_TeX = u ? string_sprintf("%s", u) : NULL;
    string_t *rhs_TeX = rhs && kt && pt ? string_sprintf(
        "\\begin{aligned}[t]&%s\\\\&%s>0,\\quad %s\\in\\mathbb{R}\\quad\\text{(arbitrary constants)}"
        "\\\\&\\text{Solitary-wave family; not the general solution.}\\end{aligned}", rhs, kt, pt) : NULL;
    bool valid = lhs && right && lhs_TeX && rhs_TeX && equ_set_display_unbound(solution, lhs, right) == 0 &&
                 equ_set_display_TeX(solution, lhs_TeX, rhs_TeX) == 0;
    string_free(rhs_TeX); string_free(lhs_TeX); string_free(right); string_free(lhs);
    free(plain); free(pt); free(kt); free(rhs); free(u); expr_free(field);
    return valid;
}

/* A*u_t+B*u*u_x+C*u_xxx=0 has zero-background solitary waves for real nonzero constant coefficients. */
diffequ_solve_result_t *de_pde_solve_kdv(const diffequ_t *de, const expr_t *residual, bool include_steps)
{
    if (!de || !residual || de->independent_count != 2u || de->condition_count)
        return NULL;
    const expr_t *third = de_kdv_third(residual);
    if (!third)
        return NULL;
    const expr_t *field = expr_formal_derivative_dependent(third);
    if (!expr_is_variable(field))
        return NULL;
    const expr_t *space = expr_formal_derivative_wrt_at(third, 0u), *time = NULL, *dt = NULL, *dx = NULL;
    if (expr_struct_eq(space, de->independent_vars[0]))
        time = de->independent_vars[1];
    else if (expr_struct_eq(space, de->independent_vars[1]))
        time = de->independent_vars[0];
    if (!time || !de_kdv_derivatives(residual, field, time, space, &dt, &dx) || !dt || !dx)
        return NULL;
    expr_t *a = NULL, *b = NULL, *c = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL, *r4 = NULL, *nonlinear = NULL;
    expr_t *k = NULL, *position = NULL, *amplitude = NULL, *speed = NULL, *phase = NULL, *right = NULL;
    equation_t *solution = NULL;
    diffequ_solve_result_t *result = NULL;
    if (!de_linear_decompose(residual, dt, &a, &r1) || !de_linear_decompose(r1, third, &c, &r2) ||
        !de_linear_decompose(r2, dx, &nonlinear, &r3) || !de_pde_is_symbolically_zero(r3) ||
        !de_kdv_constant(a, field, time, space) ||
        !de_kdv_constant(c, field, time, space))
        goto cleanup;
    if (!de_linear_decompose(nonlinear, field, &b, &r4) || !de_pde_is_symbolically_zero(r4) ||
        !de_kdv_constant(b, field, time, space)) {
        result = de_pde_solve_gkdv(de, residual, field, time, space, a, c, nonlinear, include_steps);
        goto cleanup;
    }
    k = de_pde_kdv_parameter(de, residual, "k");
    position = de_pde_kdv_parameter(de, residual, "x0");
    if (!k || !position)
        goto cleanup;
    expr_t *four = expr_const_long(4L), *twelve = expr_const_long(12L);
    amplitude = expr_mul_simplify_owned(expr_div_simplify_owned(expr_mul(twelve, c), expr_clone(b)),
                                       expr_pow_long(k, 2L));
    speed = expr_mul_simplify_owned(expr_div_simplify_owned(expr_mul(four, c), expr_clone(a)),
                                   expr_pow_long(k, 2L));
    expr_free(twelve); expr_free(four);
    phase = speed ? expr_mul_simplify_owned(expr_clone(k),
        expr_sub_simplify_owned(expr_sub(space, position), expr_mul(speed, time))) : NULL;
    expr_t *profile = phase ? expr_sech(phase) : NULL;
    right = profile && amplitude ? expr_mul_simplify_owned(expr_clone(amplitude), expr_pow_long(profile, 2L)) : NULL;
    expr_free(profile);
    solution = right ? de_pde_solution_equation(field, right) : NULL;
    if (!solution || !de_kdv_present(solution, k, position))
        goto cleanup;
    result = de_solve_result_new(DE_SOLVE_STATUS_SOLVED, DE_SOLVER_KDV_SOLITARY_WAVE,
        "zero-background solitary-wave family; not the general solution; "
        "real coordinates, arbitrary positive inverse width and arbitrary real position; "
        "no initial or boundary data imposed");
    if (!result)
        goto cleanup;
    if (include_steps && (de_solve_result_set_steps(result,
        "Recognise A*u_t+B*u*u_x+C*u_xxx=0 with real nonzero constant coefficients.\n"
        "Here x, t, u, k and x0 denote the spatial, temporal, field, inverse-width and position roles.\n"
        "Set u=V(z), z=x-v*t-x0. Integrate once with zero background: C*V''-A*v*V+(B/2)*V^2=0.\n"
        "For V=M*sech(k*z)^2, V''=4*k^2*V-(6*k^2/M)*V^2.\n"
        "Matching coefficients gives v=4*C*k^2/A and M=12*C*k^2/B.\n"
        "k>0 and x0 is real. This is a solitary-wave family, not the general solution.\n"
        "No arbitrary initial or boundary data, multi-soliton or periodic solutions are claimed.") != 0 ||
        de_solve_result_set_steps_TeX(result,
        "\\begin{aligned}&A u_t+B u u_x+C u_{xxx}=0,\\quad A,B,C\\in\\mathbb{R}\\setminus\\{0\\}"
        "\\\\&\\text{Here }x,t,u\\text{ denote the recognised space, time and field.}"
        "\\\\&u=V(z),\\quad z=x-vt-x_0,\\quad C V''-AvV+\\tfrac{B}{2}V^2=0"
        "\\\\&V=M\\operatorname{sech}^2(kz),\\quad V''=4k^2V-\\tfrac{6k^2}{M}V^2"
        "\\\\&v=\\tfrac{4C}{A}k^2,\\quad M=\\tfrac{12C}{B}k^2,\\quad k>0,\\quad x_0\\in\\mathbb{R}"
        "\\\\&\\text{Zero-background solitary-wave family; not the general solution.}\\end{aligned}") != 0))
        goto failure;
    if (de_solve_result_append(result, solution) != 0)
        goto failure;
    solution = NULL;
    goto cleanup;
failure:
    de_solve_result_free(result);
    result = NULL;
cleanup:
    equ_free(solution); expr_free(right); expr_free(phase); expr_free(speed); expr_free(amplitude);
    expr_free(position); expr_free(k); expr_free(nonlinear);
    expr_free(r4); expr_free(r3); expr_free(r2); expr_free(r1); expr_free(c); expr_free(b); expr_free(a);
    return result;
}
