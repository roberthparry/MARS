#include <stdbool.h>

#define MARS_DIFFEQUATION_SOLVE_INTERNAL_ACCESS
#include "diffequ_solve_internal.h"

typedef struct {
    expr_t *x_coefficient;
    expr_t *y_coefficient;
    expr_t *constant;
} de_affine_t;

static bool de_expr_evaluates_zero(const expr_t *expr)
{
    number_t value;
    bool zero;

    if (!expr)
        return false;
    value = expr_eval(expr);
    zero = num_eq(value, NUM_ZERO);
    num_destroy(&value);
    return zero;
}

static void de_affine_clear(de_affine_t *affine)
{
    if (!affine)
        return;
    expr_free(affine->constant);
    expr_free(affine->y_coefficient);
    expr_free(affine->x_coefficient);
    affine->constant = NULL;
    affine->y_coefficient = NULL;
    affine->x_coefficient = NULL;
}

static bool de_extract_affine(const expr_t *expr,
                              const expr_t *independent,
                              const expr_t *dependent,
                              de_affine_t *affine)
{
    expr_t *remainder = NULL;

    affine->x_coefficient = NULL;
    affine->y_coefficient = NULL;
    affine->constant = NULL;
    if (!de_linear_decompose(
            expr,
            independent,
            &affine->x_coefficient,
            &remainder) ||
        !de_linear_decompose(
            remainder,
            dependent,
            &affine->y_coefficient,
            &affine->constant))
        goto fail;
    expr_free(remainder);
    remainder = NULL;

    if (de_expr_uses(affine->x_coefficient, independent) ||
        de_expr_uses(affine->x_coefficient, dependent) ||
        de_expr_uses(affine->y_coefficient, independent) ||
        de_expr_uses(affine->y_coefficient, dependent) ||
        de_expr_uses(affine->constant, independent) ||
        de_expr_uses(affine->constant, dependent))
        goto fail;
    return true;

fail:
    expr_free(remainder);
    de_affine_clear(affine);
    return false;
}

static expr_t *de_log_abs(const expr_t *expr)
{
    number_t value;
    expr_t *absolute;

    if (!expr)
        return NULL;
    value = expr_eval(expr);
    if (!num_is_nan(value) && num_is_real(value) &&
        num_sign(value) > 0) {
        num_destroy(&value);
        return de_simplify_unary_owned(
            expr_clone(expr), expr_log);
    }
    num_destroy(&value);
    absolute = de_simplify_unary_owned(
        expr_clone(expr), expr_abs);

    return de_simplify_unary_owned(absolute, expr_log);
}

static expr_t *de_affine_condition_constant(
    const diffequ_t *de,
    const expr_t *dependent,
    const expr_t *substitution,
    const de_affine_t *affine,
    const expr_t *integral)
{
    const expr_t *point = NULL;
    const expr_t *value = NULL;
    expr_t *x_term = NULL;
    expr_t *y_term = NULL;
    expr_t *substitution_value = NULL;
    expr_t *integral_at_condition = NULL;
    expr_t *constant = NULL;

    if (!de_find_initial_condition(
            de, dependent, &point, &value))
        return de_arbitrary_constant();

    x_term = expr_mul_simplify_owned(
        expr_clone(affine->x_coefficient), expr_clone(point));
    y_term = expr_mul_simplify_owned(
        expr_clone(affine->y_coefficient), expr_clone(value));
    substitution_value = expr_add_simplify_owned(x_term, y_term);
    x_term = NULL;
    y_term = NULL;
    substitution_value = substitution_value
        ? expr_add_simplify_owned(
              substitution_value, expr_clone(affine->constant))
        : NULL;
    integral_at_condition = substitution_value
        ? expr_substitute(
              integral, substitution, substitution_value)
        : NULL;
    integral_at_condition =
        expr_simplify_owned(integral_at_condition);
    if (integral_at_condition &&
        de_expr_evaluates_zero(point)) {
        constant = integral_at_condition;
        integral_at_condition = NULL;
    } else {
        constant = integral_at_condition
        ? expr_sub_simplify_owned(
              integral_at_condition, expr_clone(point))
        : NULL;
        if (integral_at_condition)
            integral_at_condition = NULL;
    }

    expr_free(integral_at_condition);
    expr_free(substitution_value);
    expr_free(y_term);
    expr_free(x_term);
    return constant;
}

static de_attempt_t de_try_affine_candidate(
    const diffequ_t *de,
    const expr_t *independent,
    const expr_t *dependent,
    const expr_t *derivative_right,
    const expr_t *candidate,
    equation_t **solution_out)
{
    de_affine_t affine = { 0 };
    expr_t *substitution = NULL;
    expr_t *negative_x_term = NULL;
    expr_t *negative_constant = NULL;
    expr_t *dependent_replacement = NULL;
    expr_t *transformed_raw = NULL;
    expr_t *transformed = NULL;
    expr_t *scaled_rhs = NULL;
    expr_t *substitution_derivative = NULL;
    expr_t *reciprocal = NULL;
    expr_t *integral = NULL;
    expr_t *left = NULL;
    expr_t *constant = NULL;
    expr_t *right = NULL;
    de_attempt_t attempt = DE_ATTEMPT_NOT_MATCHED;

    if (!de_extract_affine(
            candidate, independent, dependent, &affine) ||
        expr_is_exact_zero(affine.x_coefficient) ||
        expr_is_exact_zero(affine.y_coefficient))
        goto cleanup;

    substitution = expr_new_named_var(NUM_NAN, "__de_u");
    negative_x_term = expr_mul_simplify_owned(
        de_simplify_unary_owned(
            expr_clone(affine.x_coefficient), expr_neg),
        expr_clone(independent));
    negative_constant = de_simplify_unary_owned(
        expr_clone(affine.constant), expr_neg);
    dependent_replacement = expr_add_simplify_owned(
        expr_add_simplify_owned(
            expr_clone(substitution), negative_x_term),
        negative_constant);
    negative_x_term = NULL;
    negative_constant = NULL;
    dependent_replacement = dependent_replacement
        ? expr_div_simplify_owned(
              dependent_replacement,
              expr_clone(affine.y_coefficient))
        : NULL;
    transformed_raw = dependent_replacement
        ? expr_substitute(
              derivative_right,
              dependent,
              dependent_replacement)
        : NULL;
    transformed = transformed_raw
        ? expr_simplify(transformed_raw)
        : NULL;
    if (!transformed ||
        de_expr_uses(transformed, independent) ||
        de_expr_uses(transformed, dependent) ||
        !de_expr_uses(transformed, substitution))
        goto cleanup;
    attempt = DE_ATTEMPT_FAILED;

    scaled_rhs = expr_mul_simplify_owned(
        expr_clone(affine.y_coefficient), transformed);
    transformed = NULL;
    substitution_derivative = expr_add_simplify_owned(
        expr_clone(affine.x_coefficient), scaled_rhs);
    scaled_rhs = NULL;
    reciprocal = substitution_derivative
        ? expr_div_simplify_owned(
              expr_const_one(), substitution_derivative)
        : NULL;
    substitution_derivative = NULL;
    integral = reciprocal
        ? de_integrate_or_formal(reciprocal, substitution)
        : NULL;
    left = integral
        ? expr_substitute(integral, substitution, candidate)
        : NULL;
    if (left) {
        expr_t *display_left = expr_display_simplified(left);

        if (display_left) {
            expr_free(left);
            left = display_left;
        }
    }
    constant = integral
        ? de_affine_condition_constant(
              de,
              dependent,
              substitution,
              &affine,
              integral)
        : NULL;
    if (constant && de_expr_evaluates_zero(constant)) {
        right = expr_new_named_var(
            NUM_NAN, expr_symbol_name(independent));
        expr_free(constant);
        constant = NULL;
    } else {
        expr_t *right_independent =
            constant ? expr_clone(independent) : NULL;

        right = right_independent
            ? expr_add(right_independent, constant)
            : NULL;
        expr_free(right_independent);
        expr_free(constant);
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
    expr_free(left);
    expr_free(integral);
    expr_free(reciprocal);
    expr_free(substitution_derivative);
    expr_free(scaled_rhs);
    expr_free(transformed);
    expr_free(transformed_raw);
    expr_free(dependent_replacement);
    expr_free(negative_constant);
    expr_free(negative_x_term);
    expr_free(substitution);
    de_affine_clear(&affine);
    return attempt;
}

static de_attempt_t de_find_affine_substitution(
    const diffequ_t *de,
    const expr_t *independent,
    const expr_t *dependent,
    const expr_t *derivative_right,
    const expr_t *candidate,
    equation_t **solution_out)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    de_attempt_t attempt;

    if (!candidate || expr_is_formal_derivative(candidate))
        return DE_ATTEMPT_NOT_MATCHED;

    attempt = de_try_affine_candidate(
        de,
        independent,
        dependent,
        derivative_right,
        candidate,
        solution_out);
    if (attempt != DE_ATTEMPT_NOT_MATCHED)
        return attempt;
    if (!expr_child_exprs(candidate, &left, &right))
        return DE_ATTEMPT_NOT_MATCHED;

    attempt = de_find_affine_substitution(
        de,
        independent,
        dependent,
        derivative_right,
        left,
        solution_out);
    if (attempt != DE_ATTEMPT_NOT_MATCHED)
        return attempt;
    return de_find_affine_substitution(
        de,
        independent,
        dependent,
        derivative_right,
        right,
        solution_out);
}

static expr_t *de_affine_intersection_x(const de_affine_t *numerator,
                                        const de_affine_t *denominator,
                                        const expr_t *determinant)
{
    expr_t *first = expr_mul_simplify_owned(
        expr_clone(numerator->y_coefficient),
        expr_clone(denominator->constant));
    expr_t *second = expr_mul_simplify_owned(
        expr_clone(denominator->y_coefficient),
        expr_clone(numerator->constant));
    expr_t *difference = expr_sub_simplify_owned(first, second);

    return difference
        ? expr_div_simplify_owned(
              difference, expr_clone(determinant))
        : NULL;
}

static expr_t *de_affine_intersection_y(const de_affine_t *numerator,
                                        const de_affine_t *denominator,
                                        const expr_t *determinant)
{
    expr_t *first = expr_mul_simplify_owned(
        expr_clone(denominator->x_coefficient),
        expr_clone(numerator->constant));
    expr_t *second = expr_mul_simplify_owned(
        expr_clone(numerator->x_coefficient),
        expr_clone(denominator->constant));
    expr_t *difference = expr_sub_simplify_owned(first, second);

    return difference
        ? expr_div_simplify_owned(
              difference, expr_clone(determinant))
        : NULL;
}

static expr_t *de_shifted_condition_constant(
    const diffequ_t *de,
    const expr_t *dependent,
    const expr_t *substitution,
    const expr_t *x_shift,
    const expr_t *y_shift,
    const expr_t *integral)
{
    const expr_t *point = NULL;
    const expr_t *value = NULL;
    expr_t *shifted_point = NULL;
    expr_t *shifted_value = NULL;
    expr_t *ratio = NULL;
    expr_t *integral_at_condition = NULL;
    expr_t *log_point = NULL;
    expr_t *constant = NULL;

    if (!de_find_initial_condition(
            de, dependent, &point, &value))
        return de_arbitrary_constant();

    shifted_point = expr_sub_simplify_owned(
        expr_clone(point), expr_clone(x_shift));
    if (!shifted_point || expr_is_exact_zero(shifted_point))
        goto cleanup;
    shifted_value = expr_sub_simplify_owned(
        expr_clone(value), expr_clone(y_shift));
    ratio = shifted_value
        ? expr_div_simplify_owned(
              shifted_value, expr_clone(shifted_point))
        : NULL;
    shifted_value = NULL;
    integral_at_condition = ratio
        ? expr_substitute(integral, substitution, ratio)
        : NULL;
    log_point = de_log_abs(shifted_point);
    constant = integral_at_condition && log_point
        ? expr_sub_simplify_owned(
              integral_at_condition, log_point)
        : NULL;
    if (integral_at_condition && log_point) {
        integral_at_condition = NULL;
        log_point = NULL;
    }

cleanup:
    expr_free(log_point);
    expr_free(integral_at_condition);
    expr_free(ratio);
    expr_free(shifted_value);
    expr_free(shifted_point);
    return constant;
}

static de_attempt_t de_try_shifted_ratio(
    const diffequ_t *de,
    const expr_t *independent,
    const expr_t *dependent,
    const expr_t *derivative_right,
    const expr_t *candidate,
    equation_t **solution_out)
{
    const expr_t *numerator_expr = NULL;
    const expr_t *denominator_expr = NULL;
    de_affine_t numerator = { 0 };
    de_affine_t denominator = { 0 };
    expr_t *first_determinant = NULL;
    expr_t *second_determinant = NULL;
    expr_t *determinant = NULL;
    expr_t *x_shift = NULL;
    expr_t *y_shift = NULL;
    expr_t *shifted_independent = NULL;
    expr_t *substitution = NULL;
    expr_t *temporary_independent = NULL;
    expr_t *scaled_substitution = NULL;
    expr_t *x_replacement = NULL;
    expr_t *y_replacement = NULL;
    expr_t *after_x = NULL;
    expr_t *transformed_raw = NULL;
    expr_t *transformed = NULL;
    expr_t *difference = NULL;
    expr_t *reciprocal = NULL;
    expr_t *integral = NULL;
    expr_t *shifted_dependent = NULL;
    expr_t *ratio = NULL;
    expr_t *left = NULL;
    expr_t *log_independent = NULL;
    expr_t *constant = NULL;
    expr_t *right = NULL;
    de_attempt_t attempt = DE_ATTEMPT_NOT_MATCHED;

    if (!expr_match_div_expr(
            candidate, &numerator_expr, &denominator_expr) ||
        !de_extract_affine(
            numerator_expr,
            independent,
            dependent,
            &numerator) ||
        !de_extract_affine(
            denominator_expr,
            independent,
            dependent,
            &denominator))
        goto cleanup;

    first_determinant = expr_mul_simplify_owned(
        expr_clone(numerator.x_coefficient),
        expr_clone(denominator.y_coefficient));
    second_determinant = expr_mul_simplify_owned(
        expr_clone(denominator.x_coefficient),
        expr_clone(numerator.y_coefficient));
    determinant = expr_sub_simplify_owned(
        first_determinant, second_determinant);
    first_determinant = NULL;
    second_determinant = NULL;
    if (!determinant || expr_is_exact_zero(determinant))
        goto cleanup;

    x_shift = de_affine_intersection_x(
        &numerator, &denominator, determinant);
    y_shift = de_affine_intersection_y(
        &numerator, &denominator, determinant);
    shifted_independent = x_shift
        ? expr_sub_simplify_owned(
              expr_clone(independent), expr_clone(x_shift))
        : NULL;
    substitution = expr_new_named_var(NUM_NAN, "__de_u");
    temporary_independent =
        expr_new_named_var(NUM_NAN, "__de_X");
    scaled_substitution = temporary_independent && substitution
        ? expr_mul_simplify_owned(
              expr_clone(temporary_independent),
              expr_clone(substitution))
        : NULL;
    x_replacement = x_shift && temporary_independent
        ? expr_add_simplify_owned(
              expr_clone(temporary_independent),
              expr_clone(x_shift))
        : NULL;
    y_replacement = scaled_substitution && y_shift
        ? expr_add_simplify_owned(
              scaled_substitution, expr_clone(y_shift))
        : NULL;
    scaled_substitution = NULL;
    after_x = x_replacement
        ? expr_substitute(
              derivative_right, independent, x_replacement)
        : NULL;
    transformed_raw = after_x && y_replacement
        ? expr_substitute(after_x, dependent, y_replacement)
        : NULL;
    transformed = transformed_raw
        ? expr_simplify(transformed_raw)
        : NULL;
    if (!transformed ||
        de_expr_uses(transformed, independent) ||
        de_expr_uses(transformed, dependent) ||
        de_expr_uses(transformed, temporary_independent) ||
        !de_expr_uses(transformed, substitution))
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
    shifted_dependent = expr_sub_simplify_owned(
        expr_clone(dependent), expr_clone(y_shift));
    ratio = shifted_dependent && shifted_independent
        ? expr_div_simplify_owned(
              shifted_dependent,
              expr_clone(shifted_independent))
        : NULL;
    shifted_dependent = NULL;
    left = integral && ratio
        ? expr_substitute(integral, substitution, ratio)
        : NULL;
    if (left) {
        expr_t *display_left = expr_display_simplified(left);

        if (display_left) {
            expr_free(left);
            left = display_left;
        }
    }
    log_independent = de_log_abs(shifted_independent);
    constant = integral
        ? de_shifted_condition_constant(
              de,
              dependent,
              substitution,
              x_shift,
              y_shift,
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
    expr_free(shifted_dependent);
    expr_free(integral);
    expr_free(reciprocal);
    expr_free(difference);
    expr_free(transformed);
    expr_free(transformed_raw);
    expr_free(after_x);
    expr_free(y_replacement);
    expr_free(x_replacement);
    expr_free(scaled_substitution);
    expr_free(temporary_independent);
    expr_free(substitution);
    expr_free(shifted_independent);
    expr_free(y_shift);
    expr_free(x_shift);
    expr_free(determinant);
    expr_free(second_determinant);
    expr_free(first_determinant);
    de_affine_clear(&denominator);
    de_affine_clear(&numerator);
    return attempt;
}

static de_attempt_t de_find_shifted_ratio(
    const diffequ_t *de,
    const expr_t *independent,
    const expr_t *dependent,
    const expr_t *derivative_right,
    const expr_t *candidate,
    equation_t **solution_out)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    de_attempt_t attempt;

    if (!candidate || expr_is_formal_derivative(candidate))
        return DE_ATTEMPT_NOT_MATCHED;

    attempt = de_try_shifted_ratio(
        de,
        independent,
        dependent,
        derivative_right,
        candidate,
        solution_out);
    if (attempt != DE_ATTEMPT_NOT_MATCHED)
        return attempt;
    if (!expr_child_exprs(candidate, &left, &right))
        return DE_ATTEMPT_NOT_MATCHED;

    attempt = de_find_shifted_ratio(
        de,
        independent,
        dependent,
        derivative_right,
        left,
        solution_out);
    if (attempt != DE_ATTEMPT_NOT_MATCHED)
        return attempt;
    return de_find_shifted_ratio(
        de,
        independent,
        dependent,
        derivative_right,
        right,
        solution_out);
}

de_attempt_t de_attempt_linear_substitution(
    const diffequ_t *de,
    const expr_t *independent,
    const expr_t *dependent,
    const expr_t *derivative_right,
    equation_t **solution_out)
{
    de_attempt_t attempt = de_find_affine_substitution(
        de,
        independent,
        dependent,
        derivative_right,
        derivative_right,
        solution_out);

    if (attempt != DE_ATTEMPT_NOT_MATCHED)
        return attempt;
    return de_find_shifted_ratio(
        de,
        independent,
        dependent,
        derivative_right,
        derivative_right,
        solution_out);
}
