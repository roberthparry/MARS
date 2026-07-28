#define MARS_DIFFEQUATION_SOLVE_INTERNAL_ACCESS
#include "diffequ_solve_internal.h"

static expr_t *de_log_abs(const expr_t *expr)
{
    expr_t *absolute =
        de_simplify_unary_owned(expr_clone(expr), expr_abs);

    return de_simplify_unary_owned(absolute, expr_log);
}

static expr_t *de_homogeneous_constant(
    const diffequ_t *de,
    const expr_t *dependent,
    const expr_t *substitution,
    const expr_t *integral)
{
    const expr_t *point = NULL;
    const expr_t *value = NULL;
    expr_t *ratio_at_point = NULL;
    expr_t *integral_at_point = NULL;
    expr_t *log_point = NULL;
    expr_t *constant = NULL;

    if (!de_find_initial_condition(
            de, dependent, &point, &value))
        return de_arbitrary_constant();
    if (expr_is_exact_zero(point))
        return NULL;

    ratio_at_point = expr_div_simplify_owned(
        expr_clone(value), expr_clone(point));
    integral_at_point = ratio_at_point
        ? expr_substitute(integral, substitution, ratio_at_point)
        : NULL;
    log_point = de_log_abs(point);
    constant = integral_at_point && log_point
        ? expr_sub_simplify_owned(integral_at_point, log_point)
        : NULL;
    if (integral_at_point && log_point) {
        integral_at_point = NULL;
        log_point = NULL;
    }

    expr_free(log_point);
    expr_free(integral_at_point);
    expr_free(ratio_at_point);
    return constant;
}

de_attempt_t de_attempt_homogeneous(
    const diffequ_t *de,
    const expr_t *independent,
    const expr_t *dependent,
    const expr_t *derivative_right,
    equation_t **solution_out)
{
    expr_t *substitution = NULL;
    expr_t *scaled_substitution = NULL;
    expr_t *transformed_raw = NULL;
    expr_t *transformed = NULL;
    expr_t *difference = NULL;
    expr_t *reciprocal = NULL;
    expr_t *integral = NULL;
    expr_t *ratio = NULL;
    expr_t *left = NULL;
    expr_t *log_independent = NULL;
    expr_t *constant = NULL;
    expr_t *right = NULL;
    de_attempt_t attempt = DE_ATTEMPT_NOT_MATCHED;

    substitution = expr_new_named_var(NUM_NAN, "__de_u");
    scaled_substitution = substitution
        ? expr_mul_simplify_owned(
              expr_clone(independent), expr_clone(substitution))
        : NULL;
    transformed_raw = scaled_substitution
        ? expr_substitute(
              derivative_right, dependent, scaled_substitution)
        : NULL;
    transformed = transformed_raw
        ? expr_simplify(transformed_raw)
        : NULL;
    if (!transformed ||
        de_expr_uses(transformed, independent))
        goto cleanup;
    attempt = DE_ATTEMPT_FAILED;

    difference = expr_sub_simplify_owned(
        transformed, expr_clone(substitution));
    transformed = NULL;
    reciprocal = difference
        ? expr_div_simplify_owned(expr_const_one(), difference)
        : NULL;
    difference = NULL;
    integral = reciprocal
        ? de_integrate_or_formal(reciprocal, substitution)
        : NULL;
    ratio = expr_div_simplify_owned(
        expr_clone(dependent), expr_clone(independent));
    left = integral && ratio
        ? expr_substitute(integral, substitution, ratio)
        : NULL;
    log_independent = de_log_abs(independent);
    constant = integral
        ? de_homogeneous_constant(
              de,
              dependent,
              substitution,
              integral)
        : NULL;
    right = log_independent && constant
        ? expr_add_simplify_owned(log_independent, constant)
        : NULL;
    if (log_independent && constant) {
        log_independent = NULL;
        constant = NULL;
    }
    if (!left || !right)
        goto cleanup;

    *solution_out = equ_new(left, right);
    if (*solution_out)
        attempt = DE_ATTEMPT_SOLVED;

cleanup:
    expr_free(right);
    expr_free(constant);
    expr_free(log_independent);
    expr_free(left);
    expr_free(ratio);
    expr_free(integral);
    expr_free(reciprocal);
    expr_free(difference);
    expr_free(transformed);
    expr_free(transformed_raw);
    expr_free(scaled_substitution);
    expr_free(substitution);
    return attempt;
}
