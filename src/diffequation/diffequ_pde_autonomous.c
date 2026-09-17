#include <stdlib.h>

#define MARS_DIFFEQUATION_PDE_INTERNAL_ACCESS
#include "diffequ_pde_internal.h"

/* The normalising coefficient is constant in both coordinates and in the dependent variable. */
static bool de_autonomous_constant(const expr_t *coefficient, const expr_t *dependent, bool *assumption)
{
    if (!coefficient || de_expr_uses(coefficient, dependent) || expr_is_exact_zero(coefficient))
        return false;
    number_t value = expr_eval(coefficient);
    *assumption = num_is_nan(value);
    bool valid = *assumption || (num_is_finite(value) && !num_is_zero(value));
    num_destroy(&value);
    return valid;
}

/* Partial derivatives hold the dependent symbol fixed: this verifies the implicit characteristic relation. */
static bool de_autonomous_invariant(const expr_t *relation, const expr_t *x, const expr_t *y,
                                    const expr_t *a, const expr_t *b)
{
    expr_t *dx = expr_create_deriv(relation, x);
    expr_t *dy = expr_create_deriv(relation, y);
    expr_t *first = dx ? expr_mul(a, dx) : NULL;
    expr_t *second = dy ? expr_mul(b, dy) : NULL;
    expr_t *residual = first && second ? expr_add(first, second) : NULL;
    bool valid = residual && de_pde_is_symbolically_zero(residual);
    expr_free(residual);
    expr_free(second);
    expr_free(first);
    expr_free(dy);
    expr_free(dx);
    return valid;
}

static bool de_autonomous_steps(diffequ_solve_result_t *result, const expr_t *dependent, const expr_t *along,
                                const expr_t *across, const expr_t *coefficient, const expr_t *speed,
                                const expr_t *invariant, const expr_t *relation, bool assumption)
{
    const expr_t *values[] = {dependent, along, across, coefficient, speed, invariant, relation};
    char *text[7] = {NULL}, *TeX[7] = {NULL};
    string_t *steps = NULL, *steps_TeX = NULL;
    bool valid = false;

    for (size_t i = 0u; i < 7u; ++i) {
        expr_t *symbol = i < 3u ? expr_new_named_var(NUM_NAN, expr_symbol_name(values[i])) : NULL;
        text[i] = expr_to_string(symbol ? symbol : values[i], style_UNBOUND);
        TeX[i] = expr_to_TeX_body(symbol ? symbol : values[i]);
        expr_free(symbol);
        if (!text[i] || !TeX[i])
            goto cleanup;
    }
    steps = string_sprintf(
        "Autonomous transport: implicit characteristic solution.\n"
        "Along characteristics, d%s/d%s = %s and d%s/d%s = 0.\n"
        "Thus %s and %s are constant along each characteristic.\n"
        "Relating these constants gives %s = F(%s), with F an arbitrary differentiable function.\n"
        "Let H = %s. Partial derivatives of H hold %s independent of the coordinates.\n"
        "The native solver verifies the original transport operator applied to H is zero.\n"
        "Local smooth branches require the partial derivative of H with respect to %s to be nonzero.\n"
        "Coefficients must be defined; no initial or boundary data are imposed.\n"
        "This is a classical local solution, not a continuation through characteristic crossings.\n",
        text[2], text[1], text[4], text[0], text[1], text[0], text[5], text[0], text[5], text[6], text[0], text[0]);
    steps_TeX = string_sprintf(
        "\\begin{aligned}&\\text{Autonomous transport: implicit characteristic solution}"
        "\\\\&\\frac{d %s}{d %s}=%s,\\qquad\\frac{d %s}{d %s}=0"
        "\\\\&%s\\text{ and }%s\\text{ are constant along characteristics.}"
        "\\\\&%s=F\\left(%s\\right),\\qquad F\\text{ arbitrary and differentiable}"
        "\\\\&H=%s"
        "\\\\&\\text{The original transport operator annihilates }H\\text{ with }%s\\text{ held fixed.}"
        "\\\\&\\frac{\\partial H}{\\partial %s}\\ne0\\quad\\text{defines a local smooth branch.}"
        "\\\\&\\text{Coefficients defined; no initial or boundary data imposed.}"
        "\\\\&\\text{No continuation through characteristic crossings is asserted.}",
        TeX[2], TeX[1], TeX[4], TeX[0], TeX[1], TeX[0], TeX[5], TeX[0], TeX[5], TeX[6], TeX[0], TeX[0]);
    if (!steps || !steps_TeX)
        goto cleanup;
    if (assumption && (string_append_format(steps, "Normalisation requires %s != 0.\n", text[3]) < 0 ||
                       string_append_format(steps_TeX, "\\\\&%s\\ne0\\quad\\text{(normalisation)}", TeX[3]) < 0))
        goto cleanup;
    if (string_append_cstr(steps_TeX, "\\end{aligned}") != 0)
        goto cleanup;
    valid = de_solve_result_set_steps(result, string_c_str(steps)) == 0 &&
            de_solve_result_set_steps_TeX(result, string_c_str(steps_TeX)) == 0;

cleanup:
    string_free(steps_TeX);
    string_free(steps);
    for (size_t i = 0u; i < 7u; ++i) {
        free(TeX[i]);
        free(text[i]);
    }
    return valid;
}

/* Solve c*u_t + a(u)*u_x = 0 as an implicit family, independently of coordinate names and input order. */
diffequ_solve_result_t *de_pde_solve_autonomous_transport(const diffequ_t *de, const expr_t *residual,
                                                        bool include_steps)
{
    const expr_t *dependent = NULL, *dx = NULL, *dy = NULL;
    expr_t *a = NULL, *b = NULL, *without_dx = NULL, *remainder = NULL;
    expr_t *speed = NULL, *phase = NULL, *right = NULL, *relation = NULL;
    equation_t *solution = NULL;
    diffequ_solve_result_t *result = NULL;
    bool assumption = false;

    if (!de || !residual || de->independent_count != 2u || de->condition_count != 0u)
        return NULL;
    const expr_t *x = de->independent_vars[0], *y = de->independent_vars[1];
    if (!de_pde_find_first_derivatives(residual, x, y, &dependent, &dx, &dy) || !dependent || !dx || !dy ||
        !de_linear_decompose(residual, dx, &a, &without_dx) ||
        !de_linear_decompose(without_dx, dy, &b, &remainder) || !de_pde_is_symbolically_zero(remainder) ||
        de_expr_uses(a, x) || de_expr_uses(a, y) || de_expr_uses(b, x) || de_expr_uses(b, y))
        goto cleanup;

    const expr_t *along = NULL, *across = NULL, *normaliser = NULL, *transport = NULL;
    if (de_autonomous_constant(a, dependent, &assumption) && de_expr_uses(b, dependent)) {
        along = x;
        across = y;
        normaliser = a;
        transport = b;
    } else if (de_autonomous_constant(b, dependent, &assumption) && de_expr_uses(a, dependent)) {
        along = y;
        across = x;
        normaliser = b;
        transport = a;
    } else {
        goto cleanup;
    }
    speed = expr_div_simplify_owned(expr_clone(transport), expr_clone(normaliser));
    phase = speed ? expr_sub_simplify_owned(expr_clone(across), expr_mul(speed, along)) : NULL;
    right = phase ? expr_new_arbitrary_function("F", phase) : NULL;
    relation = right ? expr_sub(dependent, right) : NULL;
    if (!relation || !de_autonomous_invariant(phase, x, y, a, b) ||
        !de_autonomous_invariant(relation, x, y, a, b))
        goto cleanup;
    /* Reuse the same dependent symbol on both sides of the implicit equation. */
    solution = equ_new(dependent, right);
    result = solution ? de_solve_result_new(DE_SOLVE_STATUS_SOLVED, DE_SOLVER_CHARACTERISTICS,
        "implicit local characteristic family; coefficients defined, normaliser nonzero, and H_u nonzero") : NULL;
    if (!result || (include_steps && !de_autonomous_steps(result, dependent, along, across, normaliser,
                                                         speed, phase, relation, assumption)) ||
        de_solve_result_append(result, solution) != 0) {
        de_solve_result_free(result);
        result = NULL;
        goto cleanup;
    }
    solution = NULL;

cleanup:
    equ_free(solution);
    expr_free(relation);
    expr_free(right);
    expr_free(phase);
    expr_free(speed);
    expr_free(remainder);
    expr_free(without_dx);
    expr_free(b);
    expr_free(a);
    return result;
}
