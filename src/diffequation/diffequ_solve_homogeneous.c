#include <limits.h>
#include <math.h>
#include <string.h>

#define MARS_DIFFEQUATION_SOLVE_INTERNAL_ACCESS
#include "diffequ_solve_internal.h"

static bool de_homogeneous_exact_long(number_t value, long *value_out)
{
    number_t exact;
    double approximate;
    long candidate;
    bool matches;

    if (!value_out || !num_is_real(value) || !num_is_integer(value))
        return false;
    approximate = num_to_double(value);
    if (!isfinite(approximate) ||
        approximate <= (double)LONG_MIN ||
        approximate >= (double)LONG_MAX)
        return false;
    candidate = (long)approximate;
    exact = num_create_from_long(candidate);
    matches = num_eq(value, exact);
    num_destroy(&exact);
    if (matches)
        *value_out = candidate;
    return matches;
}

static bool de_homogeneous_is_one(const expr_t *expr)
{
    number_t value = num_new();
    bool is_one = expr &&
        expr_match_const_value(expr, &value) &&
        num_eq(value, NUM_ONE);

    num_destroy(&value);
    return is_one;
}

static bool de_homogeneous_add_degrees(
    long left,
    long right,
    bool subtract,
    long *degree_out)
{
    if (!degree_out)
        return false;
    if ((!subtract &&
         ((right > 0L && left > LONG_MAX - right) ||
          (right < 0L && left < LONG_MIN - right))) ||
        (subtract &&
         ((right > 0L && left < LONG_MIN + right) ||
          (right < 0L && left > LONG_MAX + right))))
        return false;
    *degree_out = subtract ? left - right : left + right;
    return true;
}

static bool de_homogeneous_multiply_degrees(
    long left,
    long right,
    long *degree_out)
{
    if (!degree_out)
        return false;
    if (left == 0L || right == 0L) {
        *degree_out = 0L;
        return true;
    }
    if ((left == -1L && right == LONG_MIN) ||
        (right == -1L && left == LONG_MIN) ||
        (left > 0L && right > 0L && left > LONG_MAX / right) ||
        (left > 0L && right < 0L && right < LONG_MIN / left) ||
        (left < 0L && right > 0L && left < LONG_MIN / right) ||
        (left < 0L && right < 0L && right < LONG_MAX / left))
        return false;
    *degree_out = left * right;
    return true;
}

static bool de_homogeneous_degree(
    const expr_t *expr,
    const expr_t *independent,
    const expr_t *dependent,
    long *degree_out)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    const expr_t *base = NULL;
    number_t exponent = num_new();
    long left_degree;
    long right_degree;
    long integer_exponent;
    bool is_sub = false;
    bool division = false;
    bool ok = false;

    if (!expr || !independent || !dependent || !degree_out)
        goto cleanup;
    if (expr_struct_eq(expr, independent) ||
        expr_struct_eq(expr, dependent)) {
        *degree_out = 1L;
        ok = true;
        goto cleanup;
    }
    if (!de_expr_uses(expr, independent) &&
        !de_expr_uses(expr, dependent)) {
        *degree_out = 0L;
        ok = true;
        goto cleanup;
    }
    if (expr_match_neg_expr(expr, &left)) {
        ok = de_homogeneous_degree(
            left, independent, dependent, degree_out);
        goto cleanup;
    }
    if (expr_match_add_sub_expr(expr, &left, &right, &is_sub)) {
        (void)is_sub;
        ok = de_homogeneous_degree(
                 left, independent, dependent, &left_degree) &&
             de_homogeneous_degree(
                 right, independent, dependent, &right_degree) &&
             left_degree == right_degree;
        if (ok)
            *degree_out = left_degree;
        goto cleanup;
    }
    if (expr_match_mul_expr(expr, &left, &right) ||
        (division = expr_match_div_expr(expr, &left, &right))) {
        ok = de_homogeneous_degree(
                 left, independent, dependent, &left_degree) &&
             de_homogeneous_degree(
                 right, independent, dependent, &right_degree);
        if (!ok)
            goto cleanup;
        ok = de_homogeneous_add_degrees(
            left_degree, right_degree, division, degree_out);
        goto cleanup;
    }
    if (expr_match_pow_const(expr, &base, &exponent) &&
        de_homogeneous_exact_long(exponent, &integer_exponent) &&
        de_homogeneous_degree(
            base, independent, dependent, &left_degree)) {
        ok = de_homogeneous_multiply_degrees(
            left_degree, integer_exponent, degree_out);
        goto cleanup;
    }
    if (expr_match_unary_expr(expr, &left) &&
        de_homogeneous_degree(
            left, independent, dependent, &left_degree) &&
        left_degree == 0L) {
        *degree_out = 0L;
        ok = true;
        goto cleanup;
    }
    if (expr_child_exprs(expr, &left, &right) && right &&
        de_homogeneous_degree(
            left, independent, dependent, &left_degree) &&
        de_homogeneous_degree(
            right, independent, dependent, &right_degree) &&
        left_degree == 0L && right_degree == 0L) {
        *degree_out = 0L;
        ok = true;
    }

cleanup:
    num_destroy(&exponent);
    return ok;
}

static expr_t *de_homogeneous_normalize_transformed(
    const expr_t *derivative_right,
    const expr_t *independent,
    const expr_t *dependent,
    const expr_t *substitution)
{
    expr_t *scaled_substitution = NULL;
    expr_t *transformed_raw = NULL;
    expr_t *transformed = NULL;
    expr_t *one = NULL;
    expr_t *normalized_raw = NULL;
    expr_t *normalized = NULL;
    long degree;

    scaled_substitution = expr_mul_simplify_owned(
        expr_clone(independent), expr_clone(substitution));
    transformed_raw = scaled_substitution
        ? expr_substitute(
              derivative_right, dependent, scaled_substitution)
        : NULL;
    transformed = transformed_raw
        ? expr_simplify(transformed_raw)
        : NULL;
    if (!transformed || !de_expr_uses(transformed, independent))
        goto cleanup;
    if (!de_homogeneous_degree(
            derivative_right,
            independent,
            dependent,
            &degree) ||
        degree != 0L)
        goto cleanup;

    one = expr_const_one();
    normalized_raw = one
        ? expr_substitute(transformed, independent, one)
        : NULL;
    normalized = normalized_raw
        ? expr_simplify(normalized_raw)
        : NULL;
    if (normalized) {
        expr_free(transformed);
        transformed = normalized;
        normalized = NULL;
    }

cleanup:
    expr_free(normalized);
    expr_free(normalized_raw);
    expr_free(one);
    expr_free(transformed_raw);
    expr_free(scaled_substitution);
    return transformed;
}

static expr_t *de_homogeneous_expand_polynomial(
    const expr_t *expr,
    const expr_t *wrt)
{
    expr_t *zero = NULL;
    expr_t *expanded_expr = NULL;
    equation_t *equation = NULL;
    equation_t *expanded = NULL;

    zero = expr_const_zero();
    equation = zero ? equ_new(expr, zero) : NULL;
    expanded = equation ? equ_display_expanded(equation, wrt) : NULL;
    expanded_expr = expanded
        ? expr_clone(equ_lhs(expanded))
        : NULL;

    equ_free(expanded);
    equ_free(equation);
    expr_free(zero);
    return expanded_expr;
}

static expr_t *de_homogeneous_divide_sum_terms(
    const expr_t *numerator,
    const expr_t *denominator)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    expr_t *left_quotient = NULL;
    expr_t *right_quotient = NULL;
    expr_t *quotient = NULL;
    bool is_sub = false;

    if (!numerator || !denominator)
        return NULL;
    if (!expr_match_add_sub_expr(
            numerator, &left, &right, &is_sub)) {
        expr_t *inverse = expr_pow_long(denominator, -1L);

        return inverse
            ? expr_mul_simplify_owned(
                  expr_clone(numerator), inverse)
            : NULL;
    }

    left_quotient = de_homogeneous_divide_sum_terms(
        left, denominator);
    right_quotient = de_homogeneous_divide_sum_terms(
        right, denominator);
    quotient = left_quotient && right_quotient
        ? (is_sub
              ? expr_sub(
                    left_quotient, right_quotient)
              : expr_add(
                    left_quotient, right_quotient))
        : NULL;

    expr_free(right_quotient);
    expr_free(left_quotient);
    return quotient;
}

static expr_t *de_homogeneous_integrate_terms(
    const expr_t *integrand,
    const expr_t *wrt)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    expr_t *left_integral = NULL;
    expr_t *right_integral = NULL;
    expr_t *integral = NULL;
    bool is_sub = false;

    if (!integrand || !wrt)
        return NULL;
    if (!expr_match_add_sub_expr(
            integrand, &left, &right, &is_sub))
        return de_integrate_or_formal(integrand, wrt);

    left_integral = de_homogeneous_integrate_terms(left, wrt);
    right_integral = de_homogeneous_integrate_terms(right, wrt);
    integral = left_integral && right_integral
        ? (is_sub
              ? expr_sub(left_integral, right_integral)
              : expr_add(left_integral, right_integral))
        : NULL;

    expr_free(right_integral);
    expr_free(left_integral);
    return integral;
}

static expr_t *de_homogeneous_reciprocal_difference(
    const expr_t *transformed,
    const expr_t *substitution)
{
    const expr_t *numerator = NULL;
    const expr_t *denominator = NULL;
    expr_t *scaled_denominator = NULL;
    expr_t *difference_raw = NULL;
    expr_t *difference_expanded = NULL;
    expr_t *difference = NULL;
    expr_t *reciprocal = NULL;

    if (expr_match_div_expr(
            transformed, &numerator, &denominator)) {
        scaled_denominator = expr_mul_simplify_owned(
            expr_clone(substitution), expr_clone(denominator));
        difference_raw = scaled_denominator
            ? expr_sub_simplify_owned(
                  expr_clone(numerator), scaled_denominator)
            : NULL;
        scaled_denominator = NULL;
        difference_expanded = difference_raw
            ? de_homogeneous_expand_polynomial(
                  difference_raw, substitution)
            : NULL;
        difference = difference_expanded
            ? expr_simplify(difference_expanded)
            : NULL;
        reciprocal = difference
            ? de_homogeneous_divide_sum_terms(
                  denominator, difference)
            : NULL;
    } else {
        difference = expr_sub_simplify_owned(
            expr_clone(transformed), expr_clone(substitution));
        reciprocal = difference
            ? expr_div_simplify_owned(expr_const_one(), difference)
            : NULL;
        difference = NULL;
    }

    expr_free(difference);
    expr_free(difference_expanded);
    expr_free(difference_raw);
    expr_free(scaled_denominator);
    return reciprocal;
}

static expr_t *de_log_abs(const expr_t *expr)
{
    number_t value = num_new();
    expr_t *absolute = NULL;

    if (expr_match_const_value(expr, &value)) {
        bool unit = num_eq(value, NUM_ONE) ||
                    num_eq(value, NUM_NEG_ONE);

        num_destroy(&value);
        if (unit)
            return expr_const_zero();
    } else {
        num_destroy(&value);
    }

    absolute = de_simplify_unary_owned(expr_clone(expr), expr_abs);

    return de_simplify_unary_owned(absolute, expr_log);
}

static expr_t *de_homogeneous_substitute_simplified(
    const expr_t *expr,
    const expr_t *substitution,
    const expr_t *value)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    expr_t *left_value = NULL;
    expr_t *right_value = NULL;
    expr_t *substituted = NULL;
    expr_t *raw = NULL;
    number_t exponent = num_new();
    const char *expr_name;
    const char *substitution_name;
    bool is_sub = false;

    if (!expr || !substitution || !value)
        goto cleanup;
    expr_name = expr_symbol_name(expr);
    substitution_name = expr_symbol_name(substitution);
    if (expr_name && substitution_name &&
        strcmp(expr_name, substitution_name) == 0) {
        substituted = expr_clone(value);
        goto cleanup;
    }
    if (expr_struct_eq(expr, substitution)) {
        substituted = expr_clone(value);
        goto cleanup;
    }
    if (!de_expr_uses(expr, substitution)) {
        substituted = expr_clone(expr);
        goto cleanup;
    }
    if (expr_match_neg_expr(expr, &left)) {
        left_value = de_homogeneous_substitute_simplified(
            left, substitution, value);
        substituted = de_simplify_unary_owned(
            left_value, expr_neg);
        left_value = NULL;
        goto cleanup;
    }
    if (expr_match_add_sub_expr(
            expr, &left, &right, &is_sub)) {
        left_value = de_homogeneous_substitute_simplified(
            left, substitution, value);
        right_value = de_homogeneous_substitute_simplified(
            right, substitution, value);
        substituted = left_value && right_value
            ? (is_sub
                  ? expr_sub_simplify_owned(
                        left_value, right_value)
                  : expr_add_simplify_owned(
                        left_value, right_value))
            : NULL;
        if (left_value && right_value) {
            left_value = NULL;
            right_value = NULL;
        }
        goto cleanup;
    }
    if (expr_match_mul_expr(expr, &left, &right) ||
        expr_match_div_expr(expr, &left, &right)) {
        bool division = expr_match_div_expr(
            expr, &left, &right);

        left_value = de_homogeneous_substitute_simplified(
            left, substitution, value);
        right_value = de_homogeneous_substitute_simplified(
            right, substitution, value);
        if (left_value && right_value && division &&
            de_homogeneous_is_one(right_value)) {
            substituted = left_value;
            left_value = NULL;
        } else if (left_value && right_value && !division &&
                   de_homogeneous_is_one(left_value)) {
            substituted = right_value;
            right_value = NULL;
        } else if (left_value && right_value && !division &&
                   de_homogeneous_is_one(right_value)) {
            substituted = left_value;
            left_value = NULL;
        } else {
            substituted = left_value && right_value
                ? (division
                      ? expr_div_simplify_owned(
                            left_value, right_value)
                      : expr_mul_simplify_owned(
                            left_value, right_value))
                : NULL;
        }
        if (left_value && right_value) {
            left_value = NULL;
            right_value = NULL;
        }
        goto cleanup;
    }
    if (expr_match_pow_const(expr, &left, &exponent)) {
        left_value = de_homogeneous_substitute_simplified(
            left, substitution, value);
        if (de_homogeneous_is_one(left_value)) {
            substituted = expr_const_one();
        } else {
            raw = left_value ? expr_pow(left_value, &exponent) : NULL;
            substituted = raw ? expr_simplify(raw) : NULL;
        }
        goto cleanup;
    }
    if (expr_match_log_expr(expr, &left)) {
        number_t argument = num_new();

        left_value = de_homogeneous_substitute_simplified(
            left, substitution, value);
        if (left_value &&
            expr_match_const_value(left_value, &argument) &&
            num_eq(argument, NUM_ONE)) {
            substituted = expr_const_zero();
        } else {
            raw = left_value ? expr_log(left_value) : NULL;
            substituted = raw ? expr_simplify(raw) : NULL;
        }
        num_destroy(&argument);
        goto cleanup;
    }

    substituted = expr_substitute(expr, substitution, value);

cleanup:
    num_destroy(&exponent);
    expr_free(raw);
    expr_free(right_value);
    expr_free(left_value);
    return substituted;
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
    number_t ratio_value = num_new();

    if (!de_find_initial_condition(
            de, dependent, &point, &value))
        return de_arbitrary_constant();
    if (expr_is_exact_zero(point))
        return NULL;

    ratio_at_point = expr_div_simplify_owned(
        expr_clone(value), expr_clone(point));
    if (ratio_at_point &&
        expr_match_const_value(ratio_at_point, &ratio_value) &&
        num_eq(ratio_value, NUM_ONE)) {
        expr_free(ratio_at_point);
        ratio_at_point = expr_const_one();
    }
    num_destroy(&ratio_value);
    integral_at_point = ratio_at_point
        ? de_homogeneous_substitute_simplified(
              integral, substitution, ratio_at_point)
        : NULL;
    log_point = de_log_abs(point);
    if (integral_at_point && log_point &&
        expr_is_exact_zero(log_point)) {
        constant = integral_at_point;
        integral_at_point = NULL;
    } else {
        constant = integral_at_point && log_point
            ? expr_sub_simplify_owned(
                  integral_at_point, log_point)
            : NULL;
        if (integral_at_point && log_point) {
            integral_at_point = NULL;
            log_point = NULL;
        }
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
    expr_t *transformed = NULL;
    expr_t *reciprocal = NULL;
    expr_t *integral = NULL;
    expr_t *ratio = NULL;
    expr_t *left = NULL;
    expr_t *log_independent = NULL;
    expr_t *constant = NULL;
    expr_t *right = NULL;
    de_attempt_t attempt = DE_ATTEMPT_NOT_MATCHED;

    substitution = expr_new_named_var(NUM_NAN, "__de_u");
    transformed = substitution
        ? de_homogeneous_normalize_transformed(
              derivative_right,
              independent,
              dependent,
              substitution)
        : NULL;
    if (!transformed ||
        de_expr_uses(transformed, independent))
        goto cleanup;
    attempt = DE_ATTEMPT_FAILED;

    reciprocal = de_homogeneous_reciprocal_difference(
        transformed, substitution);
    expr_free(transformed);
    transformed = NULL;
    integral = reciprocal
        ? de_homogeneous_integrate_terms(
              reciprocal, substitution)
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
        ? expr_add(log_independent, constant)
        : NULL;
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
    expr_free(transformed);
    expr_free(substitution);
    return attempt;
}
