#include <stdlib.h>

#define MARS_DIFFEQUATION_PDE_INTERNAL_ACCESS
#include "diffequ_pde_internal.h"

static bool de_triangular_constant(const expr_t *expr, const expr_t *x, const expr_t *y, const expr_t *dependent)
{
    if (!expr || de_expr_uses(expr, x) || de_expr_uses(expr, y) || de_expr_uses(expr, dependent))
        return false;
    number_t value = expr_eval(expr);
    bool valid = num_is_real(value) && num_is_finite(value);
    num_destroy(&value);
    return valid;
}

/* On either real chart, |x| is respectively x or -x; this avoids assuming complex analyticity of abs. */
static bool de_triangular_verify(const expr_t *candidate, const expr_t *forcing, const expr_t *absolute,
                                 const expr_t *x, const expr_t *y, const expr_t *first, const expr_t *second)
{
    for (int sign = 1; sign >= -1; sign -= 2) {
        expr_t *chart = sign > 0 ? expr_clone(x) : expr_neg(x);
        expr_t *local = chart ? expr_substitute(candidate, absolute, chart) : NULL;
        expr_t *dx = local ? expr_create_deriv(local, x) : NULL;
        expr_t *dy = local ? expr_create_deriv(local, y) : NULL;
        expr_t *sum = dx && dy ? expr_add_simplify_owned(expr_mul(first, dx), expr_mul(second, dy)) : NULL;
        expr_t *error = sum ? expr_sub(sum, forcing) : NULL;
        bool valid = error && de_pde_is_symbolically_zero(error);
        expr_free(error);
        expr_free(sum);
        expr_free(dy);
        expr_free(dx);
        expr_free(local);
        expr_free(chart);
        if (!valid)
            return false;
    }
    return true;
}

static bool de_triangular_steps(diffequ_solve_result_t *result, const expr_t *dependent, const expr_t *x, const expr_t *y,
                                const expr_t *first, const expr_t *second, const expr_t *forcing,
                                const expr_t *invariant, const expr_t *particular, bool repeated)
{
    const expr_t *values[] = {x, y, first, second, forcing, invariant, particular, dependent};
    char *text[8] = {NULL}, *TeX[8] = {NULL};
    string_t *plain = NULL, *display = NULL;
    bool valid = false;
    for (size_t i = 0u; i < 8u; ++i) {
        const char *name = expr_symbol_name(values[i]);
        expr_t *symbol = name && (expr_is_variable(values[i]) || expr_is_named_const(values[i]))
                             ? expr_new_named_var(NUM_NAN, name) : NULL;
        text[i] = expr_to_string(symbol ? symbol : values[i], style_UNBOUND);
        TeX[i] = expr_to_TeX_body(symbol ? symbol : values[i]);
        expr_free(symbol);
        if (!text[i] || !TeX[i])
            goto cleanup;
    }
    plain = string_sprintf(
        "Triangular linear characteristic field.\n"
        "d%s/ds = %s, d%s/ds = %s, d%s/ds = %s.\n"
        "%s\nInvariant: I = %s. Particular solution: P = %s.\n"
        "Thus %s = P + F(I), with F an arbitrary differentiable function.\n"
        "The invariant and complete solution are verified on both real charts %s>0 and %s<0.\n"
        "This representation requires %s != 0; no extension across that axis is asserted.\n"
        "No initial or boundary data are imposed.\n", text[0], text[2], text[1], text[3], text[7], text[4],
        repeated ? "Equal diagonal rates produce a logarithmic invariant." : "Distinct diagonal rates give a scaled invariant.",
        text[5], text[6], text[7], text[0], text[0], text[0]);
    display = string_sprintf(
        "\\begin{aligned}&\\text{Triangular linear characteristic field}"
        "\\\\&\\frac{d%s}{ds}=%s,\\qquad\\frac{d%s}{ds}=%s,\\qquad\\frac{d%s}{ds}=%s"
        "\\\\&I=%s\\quad\\text{is constant along characteristics.}"
        "\\\\&P=%s\\quad\\text{is a particular solution.}"
        "\\\\&%s=P+F(I),\\quad F\\text{ arbitrary and differentiable}"
        "\\\\&\\text{Verified separately on }%s>0\\text{ and }%s<0."
        "\\\\&%s\\ne0;\\quad\\text{no extension across that axis asserted.}"
        "\\\\&\\text{No initial or boundary data imposed.}\\end{aligned}",
        TeX[0], TeX[2], TeX[1], TeX[3], TeX[7], TeX[4], TeX[5], TeX[6], TeX[7], TeX[0], TeX[0], TeX[0]);
    valid = plain && display && de_solve_result_set_steps(result, string_c_str(plain)) == 0 &&
            de_solve_result_set_steps_TeX(result, string_c_str(display)) == 0;
cleanup:
    string_free(display);
    string_free(plain);
    for (size_t i = 0u; i < 8u; ++i) {
        free(TeX[i]);
        free(text[i]);
    }
    return valid;
}

/* Recognise a*x*u_x + (b*x+d*y)*u_y = q, including the repeated-rate (Jordan) case. */
static diffequ_solve_result_t *de_triangular_chart(const expr_t *dependent, const expr_t *x, const expr_t *y,
                                                 const expr_t *first, const expr_t *second, const expr_t *forcing,
                                                 bool include_steps)
{
    expr_t *a = NULL, *b = NULL, *d = NULL, *offset = NULL, *rest = NULL, *tail = NULL;
    expr_t *absolute = NULL, *logarithm = NULL, *particular = NULL, *invariant = NULL, *right = NULL;
    diffequ_solve_result_t *result = NULL;
    if (!de_linear_decompose(first, x, &a, &offset) || !de_pde_is_symbolically_zero(offset) ||
        !de_linear_decompose(second, y, &d, &rest) || !de_linear_decompose(rest, x, &b, &tail) ||
        !de_pde_is_symbolically_zero(tail) || !de_triangular_constant(a, x, y, dependent) ||
        !de_triangular_constant(b, x, y, dependent) || !de_triangular_constant(d, x, y, dependent) ||
        !de_triangular_constant(forcing, x, y, dependent))
        goto cleanup;
    number_t av = expr_eval(a), dv = expr_eval(d);
    bool nonzero = !num_is_zero(av), repeated = num_eq(av, dv);
    num_destroy(&dv);
    num_destroy(&av);
    if (!nonzero)
        goto cleanup;
    absolute = expr_abs(x);
    logarithm = absolute ? expr_log(absolute) : NULL;
    expr_t *rate = expr_div_simplify_owned(expr_clone(forcing), expr_clone(a));
    particular = logarithm ? expr_mul_simplify_owned(rate, expr_clone(logarithm)) : NULL;
    if (!logarithm)
        expr_free(rate);
    if (repeated) {
        expr_t *ratio = expr_div_simplify_owned(expr_clone(b), expr_clone(a));
        expr_t *shift = logarithm ? expr_mul_simplify_owned(ratio, expr_clone(logarithm)) : NULL;
        if (!logarithm)
            expr_free(ratio);
        /* The invariant uses y/x, independently of which coordinate was first in the input. */
        invariant = shift ? expr_sub_simplify_owned(expr_div(y, x), shift) : NULL;
    } else {
        expr_t *difference = expr_sub_simplify_owned(expr_clone(a), expr_clone(d));
        expr_t *ratio = expr_div_simplify_owned(expr_clone(b), difference);
        expr_t *base = expr_sub_simplify_owned(expr_clone(y), expr_mul_simplify_owned(ratio, expr_clone(x)));
        ratio = expr_div_simplify_owned(expr_clone(d), expr_clone(a));
        expr_t *exponent = logarithm ? expr_negate_owned(expr_mul_simplify_owned(ratio, expr_clone(logarithm))) : NULL;
        if (!logarithm)
            expr_free(ratio);
        expr_t *decay = exponent ? expr_exp(exponent) : NULL;
        expr_free(exponent);
        invariant = expr_mul_simplify_owned(base, decay);
    }
    expr_t *zero = expr_const_zero();
    bool verified = invariant && particular && de_triangular_verify(invariant, zero, absolute, x, y, first, second);
    expr_free(zero);
    expr_t *family = verified ? expr_new_arbitrary_function("F", invariant) : NULL;
    right = family ? expr_add_simplify_owned(family, expr_clone(particular)) : NULL;
    if (!right || !de_triangular_verify(right, forcing, absolute, x, y, first, second))
        goto cleanup;
    equation_t *solution = de_pde_solution_equation(dependent, right);
    result = solution ? de_solve_result_new(DE_SOLVE_STATUS_SOLVED, DE_SOLVER_CHARACTERISTICS,
                                           "triangular linear characteristics; real nonzero coordinate chart") : NULL;
    if (!result || (include_steps && !de_triangular_steps(result, dependent, x, y, first, second, forcing,
                                                         invariant, particular, repeated)) ||
        de_solve_result_append(result, solution) != 0) {
        equ_free(solution);
        de_solve_result_free(result);
        result = NULL;
    }
cleanup:
    expr_free(right);
    expr_free(invariant);
    expr_free(particular);
    expr_free(logarithm);
    expr_free(absolute);
    expr_free(tail);
    expr_free(rest);
    expr_free(offset);
    expr_free(d);
    expr_free(b);
    expr_free(a);
    return result;
}

/* Try both possible triangular orientations, preserving existing solvers and supplied conditions. */
diffequ_solve_result_t *de_pde_solve_triangular_transport(const diffequ_t *de, const expr_t *residual, bool include_steps)
{
    if (!de || !residual || de->independent_count != 2u || de->condition_count)
        return NULL;
    const expr_t *dependent = NULL, *dx = NULL, *dy = NULL;
    const expr_t *x = de->independent_vars[0], *y = de->independent_vars[1];
    expr_t *first = NULL, *second = NULL, *rest = NULL, *remainder = NULL, *forcing = NULL;
    diffequ_solve_result_t *result = NULL;
    if (!de_pde_find_first_derivatives(residual, x, y, &dependent, &dx, &dy) || !dependent || !dx || !dy ||
        !de_linear_decompose(residual, dx, &first, &rest) ||
        !de_linear_decompose(rest, dy, &second, &remainder))
        goto cleanup;
    forcing = expr_neg(remainder);
    result = de_triangular_chart(dependent, x, y, first, second, forcing, include_steps);
    if (!result)
        result = de_triangular_chart(dependent, y, x, second, first, forcing, include_steps);
cleanup:
    expr_free(forcing);
    expr_free(remainder);
    expr_free(rest);
    expr_free(second);
    expr_free(first);
    return result;
}
