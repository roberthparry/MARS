/**
 * @file diffequ_solve_emden_quadrature.c
 * @brief Exact local quadratures from the translation and scaling symmetries of modified-Emden equations.
 *
 * For y'' + a*y*y' + b*y^3 = 0, r = y'/y^2 reduces the problem to
 * d(log|y|)/dr = -r/(2*r^2+a*r+b). Two parametric quadratures retain independent coefficients.
 * Equilibria and constant-ratio branches are checked separately so division by y or the quadratic loses none.
 * This private strategy handles unconditioned real families; conditioned problems retain existing solver paths.
 */
#include <stdlib.h>
#include <string.h>

#define MARS_SHARED_EQUATION_INTERNAL_ACCESS
#include "internal/equation_internal.h"
#define MARS_DIFFEQUATION_SOLVE_INTERNAL_ACCESS
#include "diffequ_solve_internal.h"
#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_notation_internal.h"

static expr_t *fresh_symbol(expr_bindings_t *bindings, const expr_t *t, const expr_t *y,
                            const char *base, bool constant)
{
    /* Each membership check uses the binding dictionary; one more candidate than authored names suffices. */
    for (size_t i = 0; i <= expr_bindings_count(bindings) + 2u; ++i) {
        string_t *name = i ? string_sprintf("%s%zu", base, i) : string_new_with(base);
        bool unused = name && !expr_bindings_get_text(bindings, name) &&
            (!expr_symbol_name(t) || strcmp(string_c_str(name), expr_symbol_name(t))) &&
            (!expr_symbol_name(y) || strcmp(string_c_str(name), expr_symbol_name(y)));
        expr_t *symbol = unused ? (constant ? expr_new_named_const(NUM_NAN, string_c_str(name))
                                           : expr_new_named_var(NUM_NAN, string_c_str(name))) : NULL;
        string_free(name);
        if (unused)
            return symbol;
    }
    return NULL;
}

static bool real_or_unset(const expr_t *expression)
{
    number_t value = expr_eval(expression);
    bool valid = num_is_nan(value) || (num_is_real(value) && num_is_finite(value));
    num_destroy(&value);
    return valid;
}

static expr_t *quadratic(const expr_t *r, const expr_t *a, const expr_t *b)
{
    return expr_add_simplify_owned(expr_add_simplify_owned(
        expr_mul_simplify_owned(expr_const_long(2), expr_pow_long(r, 2)), expr_mul(a, r)), expr_clone(b));
}

static expr_t *restricted(const expr_t *body, const expr_t *const *real, size_t real_count,
                          const expr_t *const *nonzero, size_t nonzero_count, const expr_t *zero_required)
{
    expr_t *arguments[33] = {(expr_t *)body}, *owned[16] = {NULL}, *zero = expr_const_zero();
    size_t count = 1u, used = 0u;
    bool valid = body && zero && real_count + nonzero_count + (zero_required != NULL) <= 16u;
    for (size_t i = 0; valid && i < real_count; ++i) {
        owned[used] = expr_real_parameter(real[i]);
        arguments[count++] = owned[used++]; arguments[count++] = zero;
    }
    for (size_t i = 0; valid && i < nonzero_count; ++i) {
        owned[used] = expr_abs(nonzero[i]);
        arguments[count++] = owned[used++]; arguments[count++] = zero;
    }
    if (valid && zero_required) {
        expr_t *magnitude = expr_abs(zero_required);
        owned[used] = magnitude ? expr_real_comparison(zero, magnitude, true) : NULL;
        expr_free(magnitude);
        arguments[count++] = owned[used++]; arguments[count++] = zero;
    }
    for (size_t i = 0; valid && i < used; ++i)
        valid = owned[i] != NULL;
    expr_t *result = valid ? expr_real_domain_from_args(count, arguments) : NULL;
    for (size_t i = 0; i < used; ++i) expr_free(owned[i]);
    expr_free(zero);
    return result;
}

static bool append(diffequ_solve_result_t *result, const expr_t *left, const expr_t *right)
{
    equation_t *equation = right ? equ_new(left, right) : NULL;
    if (equation && de_solve_result_append(result, equation) == 0)
        return true;
    equ_free(equation);
    return false;
}

static bool exceptional_families(diffequ_solve_result_t *result, const expr_t *t, const expr_t *y,
                                 const expr_t *a, const expr_t *b, const expr_t *amplitude, const expr_t *origin)
{
    expr_t *zero = expr_const_zero();
    bool valid = zero && append(result, y, zero);
    expr_t *discriminant = expr_sub_simplify_owned(expr_pow_long(a, 2), expr_mul_long(b, 8));
    expr_t *radical = discriminant ? expr_sqrt(discriminant) : NULL, *previous = NULL;
    valid = valid && radical;
    for (unsigned sign = 0; valid && radical && sign < 2u; ++sign) {
        expr_t *numerator = sign ? expr_sub_simplify_owned(expr_neg(a), expr_clone(radical))
                                : expr_add_simplify_owned(expr_neg(a), expr_clone(radical));
        expr_t *root = numerator ? expr_div_long(numerator, 4) : NULL;
        expr_free(numerator);
        bool distinct = !previous || !expr_struct_eq(root, previous);
        number_t value = root ? expr_eval(root) : NUM_NAN;
        if (root && distinct && !expr_is_exact_zero(root) && !num_is_zero(value) && real_or_unset(root)) {
            expr_t *denominator = expr_mul_simplify_owned(expr_clone(root), expr_sub(t, origin));
            expr_t *reciprocal = expr_div_simplify_owned(expr_const_long(-1), expr_clone(denominator));
            const expr_t *real[] = {root, t, origin}, *nonzero[] = {denominator};
            expr_t *branch = restricted(reciprocal, real, 3u, nonzero, 1u, NULL);
            valid = branch && append(result, y, branch);
            expr_free(branch); expr_free(reciprocal); expr_free(denominator);
        }
        num_destroy(&value);
        valid = valid && root;
        expr_free(previous); previous = root;
    }
    number_t beta = expr_eval(b);
    if (valid && (num_is_nan(beta) || num_is_zero(beta))) {
        const expr_t *real[] = {amplitude};
        expr_t *branch = restricted(amplitude, real, 1u, NULL, 0u, b);
        valid = branch && append(result, y, branch);
        expr_free(branch);
    }
    num_destroy(&beta); expr_free(previous); expr_free(radical); expr_free(discriminant); expr_free(zero);
    return valid;
}

/* Construct a complete local parametric family after the eight-symmetry special case has declined. */
diffequ_solve_result_t *de_solve_emden_quadrature(const diffequ_t *de, const expr_t *t, const expr_t *y,
    const expr_t *second, const expr_t *first, const expr_t *residual, bool include_steps)
{
    expr_t *leading = NULL, *mixed = NULL, *cubic = NULL;
    expr_t *a = NULL, *b = NULL, *r = NULL, *reference = NULL;
    expr_t *inner = NULL, *outer_variable = NULL, *q_inner = NULL, *q_outer = NULL, *j_outer = NULL;
    expr_t *time_symbol = NULL;
    expr_bindings_t *names = NULL;
    expr_t *amplitude = NULL, *origin = NULL, *q = NULL, *q_reference = NULL, *ratio = NULL, *j = NULL;
    expr_t *negative = NULL, *exponential = NULL, *position = NULL, *outer = NULL, *clock = NULL;
    expr_t *positive = NULL, *time_integrand = NULL, *guarded_position = NULL, *guarded_clock = NULL;
    diffequ_solve_result_t *result = NULL;
    if (!de || de->condition_count || de->interval_lower || de->interval_upper ||
        !de_decompose_emden(residual, t, y, first, second, &leading, &mixed, &cubic) ||
        (expr_is_exact_zero(mixed) && expr_is_exact_zero(cubic)))
        goto cleanup;
    const expr_t *coefficients[] = {leading, mixed, cubic};
    for (size_t i = 0; i < 3u; ++i)
        if (!real_or_unset(coefficients[i]))
            goto cleanup;
    a = expr_div_simplify_owned(expr_clone(mixed), expr_clone(leading));
    b = expr_div_simplify_owned(expr_clone(cubic), expr_clone(leading));
    names = expr_bindings_from_expr_internal(residual);
    if (!names)
        goto cleanup;
    r = fresh_symbol(names, t, y, "r", false); reference = fresh_symbol(names, t, y, "r0", true);
    inner = fresh_symbol(names, t, y, "s", false); outer_variable = fresh_symbol(names, t, y, "u", false);
    amplitude = fresh_symbol(names, t, y, "C1", true); origin = fresh_symbol(names, t, y, "C2", true);
    q = a && b && r ? quadratic(r, a, b) : NULL;
    q_reference = q && reference ? expr_substitute(q, r, reference) : NULL;
    q_inner = q && inner ? expr_substitute(q, r, inner) : NULL;
    ratio = q_inner ? expr_div(inner, q_inner) : NULL;
    j = ratio && reference ? expr_integral_with_bounds_internal(ratio, reference, r, inner) : NULL;
    negative = j ? expr_neg(j) : NULL; exponential = negative ? expr_exp(negative) : NULL;
    position = exponential && amplitude ? expr_mul(amplitude, exponential) : NULL;
    j_outer = j && outer_variable ? expr_substitute(j, r, outer_variable) : NULL;
    q_outer = q && outer_variable ? expr_substitute(q, r, outer_variable) : NULL;
    positive = j_outer ? expr_exp(j_outer) : NULL;
    time_integrand = positive && q_outer ? expr_div(positive, q_outer) : NULL;
    outer = time_integrand ? expr_integral_with_bounds_internal(time_integrand, reference, r, outer_variable) : NULL;
    clock = outer && amplitude && origin ? expr_sub_simplify_owned(expr_clone(origin),
        expr_mul_simplify_owned(expr_pow_long(amplitude, -1), expr_clone(outer))) : NULL;
    const expr_t *real[] = {a, b, r, reference, amplitude, origin};
    const expr_t *nonzero[] = {leading, amplitude, q, q_reference};
    guarded_position = restricted(position, real, 6u, nonzero, 4u, NULL);
    guarded_clock = restricted(clock, real, 6u, nonzero, 4u, NULL);
    result = guarded_position && guarded_clock ? de_solve_result_new(DE_SOLVE_STATUS_QUADRATURE,
        DE_SOLVER_EMDEN_QUADRATURE, "exact local parametric quadratures from time translation and scaling; "
        "the first two equations are simultaneous, and later equations are separate exceptional families") : NULL;
    if (!result)
        goto cleanup;
    result->quadrature_parameter = expr_clone(r); result->quadrature_reference = expr_clone(reference);
    time_symbol = expr_symbol_name(t) ? expr_new_named_var(NUM_NAN, expr_symbol_name(t)) : NULL;
    bool valid = result->quadrature_parameter && result->quadrature_reference && time_symbol &&
        append(result, y, guarded_position) && append(result, time_symbol, guarded_clock) &&
        exceptional_families(result, t, y, a, b, amplitude, origin) &&
        de_emden_quadrature_display(result) &&
        de_solve_result_set_symmetry(result, "time translation and scaling (two non-commuting point symmetries)") == 0;
    if (valid && include_steps)
        valid = de_emden_quadrature_steps(result, t, y, first, a, b, r, reference, q, j);
    if (!valid) { de_solve_result_free(result); result = NULL; }
cleanup:
    expr_free(time_symbol); expr_bindings_free(names); expr_free(j_outer); expr_free(q_outer);
    expr_free(q_inner); expr_free(outer_variable); expr_free(inner);
    expr_free(guarded_clock); expr_free(guarded_position); expr_free(time_integrand); expr_free(positive);
    expr_free(clock); expr_free(outer); expr_free(position); expr_free(exponential); expr_free(negative);
    expr_free(j); expr_free(ratio); expr_free(q_reference); expr_free(q); expr_free(origin); expr_free(amplitude);
    expr_free(reference); expr_free(r); expr_free(b); expr_free(a);
    expr_free(cubic); expr_free(mixed); expr_free(leading);
    return result;
}
