#include <stdlib.h>

#define MARS_DIFFEQUATION_PDE_INTERNAL_ACCESS
#include "diffequ_pde_internal.h"

/* A finite numerical constant makes the zero/nonzero branch unambiguous. */
static bool de_affine_constant(const expr_t *expr, const diffequ_t *de, const expr_t *dependent)
{
    if (!expr || de_expr_uses(expr, dependent))
        return false;
    for (size_t i = 0u; i < de->independent_count; ++i)
        if (de_expr_uses(expr, de->independent_vars[i]))
            return false;
    number_t value = expr_eval(expr);
    bool finite = num_is_finite(value);
    num_destroy(&value);
    return finite;
}

/* Verify D(expression) = forcing, including every coordinate of the original operator. */
static bool de_affine_verify(const expr_t *expr, const expr_t *forcing, const diffequ_t *de,
                             expr_t *const *coefficients)
{
    expr_t *sum = expr_neg(forcing);
    for (size_t i = 0u; sum && i < de->independent_count; ++i) {
        expr_t *derivative = expr_create_deriv(expr, de->independent_vars[i]);
        expr_t *term = derivative ? expr_mul(coefficients[i], derivative) : NULL;
        expr_free(derivative);
        sum = expr_add_simplify_owned(sum, term);
    }
    bool valid = sum && de_pde_is_symbolically_zero(sum);
    expr_free(sum);
    return valid;
}

/* Reject dependence on other coordinates or the unknown field before integrating in the chosen clock. */
static bool de_affine_time_only(const expr_t *expr, const expr_t *time, const diffequ_t *de,
                                const expr_t *dependent)
{
    if (!expr || de_expr_uses(expr, dependent))
        return false;
    for (size_t i = 0u; i < de->independent_count; ++i)
        if (de->independent_vars[i] != time && de_expr_uses(expr, de->independent_vars[i]))
            return false;
    return de_expr_uses(expr, time) || de_affine_constant(expr, de, dependent);
}

/* For dz/dt=A(t)*z+B(t), the integrating factor gives I=mu*z-integral(mu*B dt). */
static expr_t *de_affine_time_invariant(const expr_t *coordinate, const expr_t *time, const expr_t *slope,
                                       const expr_t *offset, const expr_t *clock)
{
    expr_t *rate = expr_div_simplify_owned(expr_clone(slope), expr_clone(clock));
    expr_t *primitive = rate ? expr_integrate(rate, time) : NULL;
    expr_t *exponent = expr_negate_owned(primitive);
    expr_t *decay = exponent ? expr_simplify_owned(expr_exp(exponent)) : NULL;
    expr_t *drift = expr_div_simplify_owned(expr_clone(offset), expr_clone(clock));
    expr_t *weighted = decay && drift ? expr_mul_simplify_owned(expr_clone(decay), expr_clone(drift)) : NULL;
    expr_t *shift = weighted ? expr_integrate(weighted, time) : NULL;
    expr_t *invariant = decay && !expr_is_exact_zero(decay) && shift
        ? expr_sub_simplify_owned(expr_mul(decay, coordinate), expr_clone(shift)) : NULL;
    expr_free(shift);
    expr_free(weighted);
    expr_free(drift);
    expr_free(decay);
    expr_free(exponent);
    expr_free(rate);
    return invariant;
}

/* Retain the compact constant-rate forms; use integrating factors for time-dependent rates and offsets. */
static expr_t *de_affine_invariant(const expr_t *coordinate, const expr_t *time, const expr_t *coefficient,
                                   const expr_t *clock, const diffequ_t *de, const expr_t *dependent)
{
    expr_t *slope = NULL, *offset = NULL, *invariant = NULL;
    if (!de_linear_decompose(coefficient, coordinate, &slope, &offset) ||
        !de_affine_time_only(slope, time, de, dependent) || !de_affine_time_only(offset, time, de, dependent))
        goto cleanup;
    if (!de_affine_constant(slope, de, dependent) || !de_affine_constant(offset, de, dependent)) {
        invariant = de_affine_time_invariant(coordinate, time, slope, offset, clock);
        goto cleanup;
    }
    if (expr_is_exact_zero(slope)) {
        expr_t *rate = expr_div_simplify_owned(expr_clone(offset), expr_clone(clock));
        invariant = expr_sub_simplify_owned(expr_clone(coordinate), expr_mul_simplify_owned(rate, expr_clone(time)));
    } else {
        expr_t *shift = expr_div_simplify_owned(expr_clone(offset), expr_clone(slope));
        expr_t *base = expr_add_simplify_owned(expr_clone(coordinate), shift);
        expr_t *rate = expr_div_simplify_owned(expr_clone(slope), expr_clone(clock));
        expr_t *exponent = expr_negate_owned(expr_mul_simplify_owned(rate, expr_clone(time)));
        expr_t *decay = exponent ? expr_exp(exponent) : NULL;
        expr_free(exponent);
        invariant = expr_mul_simplify_owned(base, decay);
    }
cleanup:
    expr_free(offset);
    expr_free(slope);
    return invariant;
}

static bool de_affine_steps(diffequ_solve_result_t *result, const expr_t *time, const expr_t *clock,
                            const expr_t *arbitrary, const expr_t *particular, bool coupled)
{
    char *family = expr_to_TeX_body(arbitrary), *primitive = expr_to_TeX_body(particular);
    char *scale = expr_to_TeX_body(clock);
    const char *name = expr_symbol_name(time);
    expr_t *symbol = name ? expr_new_named_var(NUM_NAN, name) : NULL;
    char *parameter = symbol ? expr_to_TeX_body(symbol) : NULL;
    expr_free(symbol);
    string_t *plain = name ? string_sprintf(
        "Affine transport by characteristics, using %s as parameter.\n"
        "Divide the original transport coefficients by the constant coefficient of its derivative.\n"
        "%s"
        "Each remaining coordinate obeys dz/d%s = A(%s)*z+B(%s).\n"
        "Integrate A in that parameter and negate its primitive to form mu = exp(-primitive).\n"
        "The invariant is I = mu*z minus a primitive of mu*B in the same parameter.\n"
        "Constant rates retain their equivalent compact shifted-exponential forms.\n"
        "F is an arbitrary differentiable function of all these invariants.\n"
        "Integrate the normalised forcing in %s for the particular solution.\n"
        "Every invariant and the complete solution are verified against the original PDE.\n"
        "The solution is local to regions where the coefficient functions and primitives are defined.\n"
        "No initial or boundary data are imposed.\n", name,
        coupled ? "Resolve upstream coordinates first and substitute their flows into each remaining drift.\n"
                  "Eliminate the integration parameters using the earlier invariants.\n" : "",
        name, name, name, name) : NULL;
    string_t *TeX = family && primitive && scale && parameter ? string_sprintf(
        "\\begin{aligned}&\\text{Affine transport by characteristics; parameter }%s"
        "\\\\&\\text{Normalise the transport operator by }%s."
        "%s"
        "\\\\&\\frac{dz}{d%s}=A(%s)z+B(%s)"
        "\\\\&\\mu=\\exp\\left(-\\int A(%s)\\,d%s\\right),\\quad "
        "I=\\mu z-\\int\\mu B(%s)\\,d%s"
        "\\\\&\\text{Homogeneous family: }%s"
        "\\\\&\\text{Particular solution: }%s"
        "\\\\&F\\text{ is arbitrary and differentiable.}"
        "\\\\&\\text{All invariants and the complete solution satisfy the original operator.}"
        "\\\\&\\text{Local where coefficients and primitives are defined; no initial or boundary data.}\\end{aligned}",
        parameter, scale,
        coupled ? "\\\\&\\text{Resolve upstream flows first; evaluate each drift along those flows.}"
                  "\\\\&\\text{Eliminate their integration parameters using the earlier invariants.}" : "",
        parameter, parameter, parameter, parameter, parameter, parameter, parameter,
        family, primitive) : NULL;
    bool valid = plain && TeX && de_solve_result_set_steps(result, string_c_str(plain)) == 0 &&
                 de_solve_result_set_steps_TeX(result, string_c_str(TeX)) == 0;
    string_free(TeX);
    string_free(plain);
    free(parameter);
    free(scale);
    free(primitive);
    free(family);
    return valid;
}

/* Solve diagonal or triangular characteristic flow with a constant clock and forcing in that clock alone. */
diffequ_solve_result_t *de_pde_solve_affine_transport(const diffequ_t *de, const expr_t *residual, bool include_steps)
{
    if (!de || !residual || de->independent_count < 2u || de->condition_count)
        return NULL;
    size_t n = de->independent_count;
    const expr_t *dependent = NULL;
    const expr_t **derivatives = calloc(n, sizeof(*derivatives));
    expr_t **coefficients = calloc(n, sizeof(*coefficients)), **invariants = calloc(n, sizeof(*invariants));
    expr_t *remainder = expr_clone(residual), *forcing = NULL;
    diffequ_solve_result_t *result = NULL;
    if (!derivatives || !coefficients || !invariants || !remainder ||
        !de_pde_find_first_derivatives_n(residual, n, de->independent_vars, &dependent, derivatives) || !dependent)
        goto cleanup;
    for (size_t i = 0u; i < n; ++i) {
        expr_t *next = NULL;
        if (!derivatives[i] || !de_linear_decompose(remainder, derivatives[i], &coefficients[i], &next)) {
            expr_free(next);
            goto cleanup;
        }
        expr_free(remainder);
        remainder = next;
    }
    if (de_expr_uses(remainder, dependent))
        goto cleanup;
    forcing = expr_neg(remainder);
    /* At most n clock candidates: their suitability depends on the forcing and the other coefficient fields. */
    for (size_t clock = 0u; forcing && !result && clock < n; ++clock) {
        if (!de_affine_constant(coefficients[clock], de, dependent) || expr_is_exact_zero(coefficients[clock]))
            continue;
        if (!de_affine_time_only(forcing, de->independent_vars[clock], de, dependent))
            continue;
        bool valid = true;
        bool coupled = false;
        size_t count = 0u;
        for (size_t i = 0u; valid && i < n; ++i) {
            if (i == clock)
                continue;
            if (de_expr_uses(forcing, de->independent_vars[i])) {
                valid = false;
                break;
            }
            invariants[count] = de_affine_invariant(de->independent_vars[i], de->independent_vars[clock],
                                                    coefficients[i], coefficients[clock], de, dependent);
            expr_t *zero = expr_const_zero();
            valid = invariants[count] && de_affine_verify(invariants[count], zero, de, coefficients);
            expr_free(zero);
            ++count;
        }
        if (!valid) {
            for (size_t i = 0u; i < count; ++i) {
                expr_free(invariants[i]);
                invariants[i] = NULL;
            }
            count = 0u;
            valid = de_pde_affine_flow_invariants(de, dependent, clock, coefficients, invariants);
            if (valid) {
                coupled = true;
                count = n - 1u;
                expr_t *zero = expr_const_zero();
                for (size_t i = 0u; valid && i < count; ++i)
                    valid = de_affine_verify(invariants[i], zero, de, coefficients);
                expr_free(zero);
            }
        }
        expr_t *rate = valid ? expr_div_simplify_owned(expr_clone(forcing), expr_clone(coefficients[clock])) : NULL;
        expr_t *particular = rate ? expr_integrate(rate, de->independent_vars[clock]) : NULL;
        expr_t *arbitrary = particular ? expr_new_arbitrary_function_n("F", count, invariants) : NULL;
        expr_t *right = arbitrary ? expr_add_simplify_owned(expr_clone(arbitrary), expr_clone(particular)) : NULL;
        /* The chain rule gives D(F(I_1,...,I_n))=0 because each D(I_j) was verified above. */
        equation_t *solution = right && de_affine_verify(particular, forcing, de, coefficients)
                                   ? de_pde_solution_equation(dependent, right) : NULL;
        if (solution) {
            result = de_solve_result_new(DE_SOLVE_STATUS_SOLVED, DE_SOLVER_CHARACTERISTICS,
                                         coupled ? "solved by triangular characteristic flow" :
                                                   "solved by affine transport characteristics");
            if (!result || (include_steps && !de_affine_steps(result, de->independent_vars[clock],
                                                             coefficients[clock], arbitrary, particular, coupled)) ||
                de_solve_result_append(result, solution) != 0) {
                de_solve_result_free(result);
                result = NULL;
                equ_free(solution);
            }
        }
        expr_free(right);
        expr_free(arbitrary);
        expr_free(particular);
        expr_free(rate);
        for (size_t i = 0u; i < count; ++i) {
            expr_free(invariants[i]);
            invariants[i] = NULL;
        }
    }
cleanup:
    expr_free(forcing);
    expr_free(remainder);
    for (size_t i = 0u; coefficients && i < n; ++i)
        expr_free(coefficients[i]);
    free(invariants);
    free(coefficients);
    free(derivatives);
    return result;
}
