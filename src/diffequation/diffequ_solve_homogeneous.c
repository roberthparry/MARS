#include <limits.h>
#include <math.h>
#include <string.h>

#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"
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
    if (!isfinite(approximate) || approximate <= (double)LONG_MIN || approximate >= (double)LONG_MAX)
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
    bool is_one = expr && expr_match_const_value(expr, &value) && num_eq(value, NUM_ONE);

    num_destroy(&value);
    return is_one;
}

static bool de_homogeneous_add_degrees(long left, long right, bool subtract, long *degree_out)
{
    if (!degree_out)
        return false;
    if ((!subtract && ((right > 0L && left > LONG_MAX - right) || (right < 0L && left < LONG_MIN - right))) ||
        (subtract && ((right > 0L && left < LONG_MIN + right) || (right < 0L && left > LONG_MAX + right))))
        return false;
    *degree_out = subtract ? left - right : left + right;
    return true;
}

static bool de_homogeneous_multiply_degrees(long left, long right, long *degree_out)
{
    if (!degree_out)
        return false;
    if (left == 0L || right == 0L) {
        *degree_out = 0L;
        return true;
    }
    if ((left == -1L && right == LONG_MIN) || (right == -1L && left == LONG_MIN) ||
        (left > 0L && right > 0L && left > LONG_MAX / right) || (left > 0L && right < 0L && right < LONG_MIN / left) ||
        (left < 0L && right > 0L && left < LONG_MIN / right) || (left < 0L && right < 0L && right < LONG_MAX / left))
        return false;
    *degree_out = left * right;
    return true;
}

static bool de_homogeneous_degree(const expr_t *expr, const expr_t *independent, const expr_t *dependent,
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
    if (expr_struct_eq(expr, independent) || expr_struct_eq(expr, dependent)) {
        *degree_out = 1L;
        ok = true;
        goto cleanup;
    }
    if (!de_expr_uses(expr, independent) && !de_expr_uses(expr, dependent)) {
        *degree_out = 0L;
        ok = true;
        goto cleanup;
    }
    if (expr_match_neg_expr(expr, &left)) {
        ok = de_homogeneous_degree(left, independent, dependent, degree_out);
        goto cleanup;
    }
    if (expr_match_add_sub_expr(expr, &left, &right, &is_sub)) {
        (void)is_sub;
        ok = de_homogeneous_degree(left, independent, dependent, &left_degree) &&
             de_homogeneous_degree(right, independent, dependent, &right_degree) && left_degree == right_degree;
        if (ok)
            *degree_out = left_degree;
        goto cleanup;
    }
    if (expr_match_mul_expr(expr, &left, &right) || (division = expr_match_div_expr(expr, &left, &right))) {
        ok = de_homogeneous_degree(left, independent, dependent, &left_degree) &&
             de_homogeneous_degree(right, independent, dependent, &right_degree);
        if (!ok)
            goto cleanup;
        ok = de_homogeneous_add_degrees(left_degree, right_degree, division, degree_out);
        goto cleanup;
    }
    if (expr_match_pow_const(expr, &base, &exponent) && de_homogeneous_exact_long(exponent, &integer_exponent) &&
        de_homogeneous_degree(base, independent, dependent, &left_degree)) {
        ok = de_homogeneous_multiply_degrees(left_degree, integer_exponent, degree_out);
        goto cleanup;
    }
    if (expr_match_unary_expr(expr, &left) && de_homogeneous_degree(left, independent, dependent, &left_degree) &&
        left_degree == 0L) {
        *degree_out = 0L;
        ok = true;
        goto cleanup;
    }
    if (expr_child_exprs(expr, &left, &right) && right &&
        de_homogeneous_degree(left, independent, dependent, &left_degree) &&
        de_homogeneous_degree(right, independent, dependent, &right_degree) && left_degree == 0L &&
        right_degree == 0L) {
        *degree_out = 0L;
        ok = true;
    }

cleanup:
    num_destroy(&exponent);
    return ok;
}

static expr_t *de_homogeneous_normalize_transformed(const expr_t *derivative_right, const expr_t *independent,
                                                    const expr_t *dependent, const expr_t *substitution)
{
    expr_t *scaled_substitution = NULL;
    expr_t *transformed_raw = NULL;
    expr_t *transformed = NULL;
    expr_t *one = NULL;
    expr_t *normalized_raw = NULL;
    expr_t *normalized = NULL;
    long degree;

    scaled_substitution = expr_mul_simplify_owned(expr_clone(independent), expr_clone(substitution));
    transformed_raw = scaled_substitution ? expr_substitute(derivative_right, dependent, scaled_substitution) : NULL;
    transformed = transformed_raw ? expr_simplify(transformed_raw) : NULL;
    if (!transformed || !de_expr_uses(transformed, independent))
        goto cleanup;
    if (!de_homogeneous_degree(derivative_right, independent, dependent, &degree) || degree != 0L)
        goto cleanup;

    one = expr_const_one();
    normalized_raw = one ? expr_substitute(transformed, independent, one) : NULL;
    normalized = normalized_raw ? expr_simplify(normalized_raw) : NULL;
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

static expr_t *de_homogeneous_expand_polynomial(const expr_t *expr, const expr_t *wrt)
{
    expr_t *zero = NULL;
    expr_t *expanded_expr = NULL;
    equation_t *equation = NULL;
    equation_t *expanded = NULL;

    zero = expr_const_zero();
    equation = zero ? equ_new(expr, zero) : NULL;
    expanded = equation ? equ_display_expanded(equation, wrt) : NULL;
    expanded_expr = expanded ? expr_clone(equ_lhs(expanded)) : NULL;

    equ_free(expanded);
    equ_free(equation);
    expr_free(zero);
    return expanded_expr;
}

static expr_t *de_homogeneous_divide_sum_terms(const expr_t *numerator, const expr_t *denominator)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    expr_t *left_quotient = NULL;
    expr_t *right_quotient = NULL;
    expr_t *quotient = NULL;
    bool is_sub = false;

    if (!numerator || !denominator)
        return NULL;
    if (!expr_match_add_sub_expr(numerator, &left, &right, &is_sub)) {
        expr_t *inverse = expr_pow_long(denominator, -1L);

        return inverse ? expr_mul_simplify_owned(expr_clone(numerator), inverse) : NULL;
    }

    left_quotient = de_homogeneous_divide_sum_terms(left, denominator);
    right_quotient = de_homogeneous_divide_sum_terms(right, denominator);
    quotient = left_quotient && right_quotient
                   ? (is_sub ? expr_sub(left_quotient, right_quotient) : expr_add(left_quotient, right_quotient))
                   : NULL;

    expr_free(right_quotient);
    expr_free(left_quotient);
    return quotient;
}

static bool de_homogeneous_match_reciprocal(const expr_t *expr, const expr_t **denominator_out)
{
    const expr_t *base = NULL;
    number_t exponent = num_new();
    long integer_exponent;
    bool matches = expr && denominator_out && expr_match_pow_const(expr, &base, &exponent) &&
                   de_homogeneous_exact_long(exponent, &integer_exponent) && integer_exponent == -1L;

    if (matches)
        *denominator_out = base;
    num_destroy(&exponent);
    return matches;
}

static expr_t *de_homogeneous_inverse_product_as_quotient(const expr_t *expr)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    const expr_t *denominator = NULL;
    expr_t *one = NULL;
    expr_t *quotient = NULL;

    if (!expr)
        return NULL;
    if (de_homogeneous_match_reciprocal(expr, &denominator)) {
        one = expr_const_one();
        quotient = one ? expr_div(one, denominator) : NULL;
        expr_free(one);
        return quotient;
    }
    if (!expr_match_mul_expr(expr, &left, &right))
        return NULL;
    if (de_homogeneous_match_reciprocal(right, &denominator))
        return expr_div(left, denominator);
    if (de_homogeneous_match_reciprocal(left, &denominator))
        return expr_div(right, denominator);
    return NULL;
}

static expr_t *de_homogeneous_integrate_terms(const expr_t *integrand, const expr_t *wrt)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    expr_t *left_integral = NULL;
    expr_t *right_integral = NULL;
    expr_t *integral = NULL;
    expr_t *quotient = NULL;
    bool is_sub = false;

    if (!integrand || !wrt)
        return NULL;
    if (!expr_match_add_sub_expr(integrand, &left, &right, &is_sub)) {
        integral = expr_integrate(integrand, wrt);
        if (integral)
            return integral;
        quotient = de_homogeneous_inverse_product_as_quotient(integrand);
        integral = quotient ? expr_integrate(quotient, wrt) : NULL;
        expr_free(quotient);
        return integral ? integral : expr_integral(integrand, wrt);
    }

    left_integral = de_homogeneous_integrate_terms(left, wrt);
    right_integral = de_homogeneous_integrate_terms(right, wrt);
    integral = left_integral && right_integral
                   ? (is_sub ? expr_sub(left_integral, right_integral) : expr_add(left_integral, right_integral))
                   : NULL;

    expr_free(right_integral);
    expr_free(left_integral);
    return integral;
}

static expr_t *de_homogeneous_reciprocal_difference(const expr_t *transformed, const expr_t *substitution)
{
    const expr_t *numerator = NULL;
    const expr_t *denominator = NULL;
    expr_t *scaled_denominator = NULL;
    expr_t *difference_raw = NULL;
    expr_t *difference_expanded = NULL;
    expr_t *difference = NULL;
    expr_t *reciprocal = NULL;

    if (expr_match_div_expr(transformed, &numerator, &denominator)) {
        scaled_denominator = expr_mul_simplify_owned(expr_clone(substitution), expr_clone(denominator));
        difference_raw = scaled_denominator ? expr_sub_simplify_owned(expr_clone(numerator), scaled_denominator) : NULL;
        scaled_denominator = NULL;
        difference_expanded = difference_raw ? de_homogeneous_expand_polynomial(difference_raw, substitution) : NULL;
        difference = difference_expanded ? expr_simplify(difference_expanded) : NULL;
        reciprocal = difference ? de_homogeneous_divide_sum_terms(denominator, difference) : NULL;
    } else {
        difference = expr_sub_simplify_owned(expr_clone(transformed), expr_clone(substitution));
        reciprocal = difference ? expr_div_simplify_owned(expr_const_one(), difference) : NULL;
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
        bool unit = num_eq(value, NUM_ONE) || num_eq(value, NUM_NEG_ONE);

        num_destroy(&value);
        if (unit)
            return expr_const_zero();
    } else {
        num_destroy(&value);
    }

    absolute = de_simplify_unary_owned(expr_clone(expr), expr_abs);

    return de_simplify_unary_owned(absolute, expr_log);
}

static unsigned long de_homogeneous_unsigned_magnitude(long value)
{
    return value < 0L ? (unsigned long)(-(value + 1L)) + 1u : (unsigned long)value;
}

static unsigned long de_homogeneous_gcd(unsigned long left, unsigned long right)
{
    while (right != 0u) {
        unsigned long remainder = left % right;

        left = right;
        right = remainder;
    }
    return left;
}

static bool de_homogeneous_primitive_affine_coefficients(const expr_t *argument, const expr_t *substitution,
                                                         long coefficients_out[2])
{
    number_t coefficients[5];
    long numerators[2];
    long denominators[2];
    unsigned long denominator_gcd;
    unsigned long coefficient_gcd;
    long common_denominator;
    long integer_coefficients[2];
    bool coefficients_ready = false;
    bool ok = false;

    if (!coefficients_out)
        return false;

    for (size_t i = 0u; i < 5u; ++i)
        coefficients[i] = num_new();
    coefficients_ready = true;
    if (!expr_collect_poly_deg4(argument, substitution, coefficients))
        goto cleanup;
    if (num_eq(coefficients[1], NUM_ZERO) || !num_eq(coefficients[2], NUM_ZERO) || !num_eq(coefficients[3], NUM_ZERO) ||
        !num_eq(coefficients[4], NUM_ZERO))
        goto cleanup;
    for (size_t i = 0u; i < 2u; ++i) {
        if (!num_get_small_rational(coefficients[i], &numerators[i], &denominators[i]) || denominators[i] <= 0L)
            goto cleanup;
    }

    denominator_gcd = de_homogeneous_gcd((unsigned long)denominators[0], (unsigned long)denominators[1]);
    if (denominator_gcd == 0u ||
        (unsigned long)denominators[0] / denominator_gcd > (unsigned long)LONG_MAX / (unsigned long)denominators[1])
        goto cleanup;
    common_denominator = (long)((unsigned long)denominators[0] / denominator_gcd * (unsigned long)denominators[1]);
    for (size_t i = 0u; i < 2u; ++i) {
        long scale = common_denominator / denominators[i];

        if (!de_homogeneous_multiply_degrees(numerators[i], scale, &integer_coefficients[i]))
            goto cleanup;
    }

    coefficient_gcd = de_homogeneous_gcd(de_homogeneous_unsigned_magnitude(integer_coefficients[0]),
                                         de_homogeneous_unsigned_magnitude(integer_coefficients[1]));
    if (coefficient_gcd == 0u || coefficient_gcd > (unsigned long)LONG_MAX)
        goto cleanup;
    for (size_t i = 0u; i < 2u; ++i)
        integer_coefficients[i] /= (long)coefficient_gcd;
    if (integer_coefficients[0] < 0L || (integer_coefficients[0] == 0L && integer_coefficients[1] < 0L)) {
        if (integer_coefficients[0] == LONG_MIN || integer_coefficients[1] == LONG_MIN)
            goto cleanup;
        integer_coefficients[0] = -integer_coefficients[0];
        integer_coefficients[1] = -integer_coefficients[1];
    }

    coefficients_out[0] = integer_coefficients[0];
    coefficients_out[1] = integer_coefficients[1];
    ok = true;

cleanup:
    if (coefficients_ready) {
        for (size_t i = 0u; i < 5u; ++i)
            num_destroy(&coefficients[i]);
    }
    return ok;
}

static expr_t *de_homogeneous_scaled_power(const expr_t *base, long coefficient, long exponent)
{
    expr_t *power;

    if (!base || exponent < 0L)
        return NULL;
    power = exponent == 0L ? expr_const_one() : (exponent == 1L ? expr_clone(base) : expr_pow_long(base, exponent));
    if (coefficient == 1L)
        return power;
    return power ? expr_mul_simplify_owned(expr_const_long(coefficient), power) : NULL;
}

static expr_t *de_homogeneous_scale_expression_exact(const expr_t *expr, long denominator)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    const expr_t *rest = expr;
    number_t factor_value = num_new();
    number_t value;
    expr_t *coefficient_expr = NULL;
    expr_t *scaled = NULL;
    long numerator = 1L;
    unsigned long divisor;

    if (!expr || denominator == 0L)
        goto cleanup;
    if (expr_match_mul_expr(expr, &left, &right)) {
        if (expr_match_const_value(left, &factor_value) && de_homogeneous_exact_long(factor_value, &numerator))
            rest = right;
        else if (expr_match_const_value(right, &factor_value) && de_homogeneous_exact_long(factor_value, &numerator))
            rest = left;
    }
    divisor = de_homogeneous_gcd(de_homogeneous_unsigned_magnitude(numerator),
                                 de_homogeneous_unsigned_magnitude(denominator));
    if (divisor == 0u || divisor > (unsigned long)LONG_MAX)
        goto cleanup;
    numerator /= (long)divisor;
    denominator /= (long)divisor;
    if (denominator < 0L) {
        if (denominator == LONG_MIN || numerator == LONG_MIN)
            goto cleanup;
        denominator = -denominator;
        numerator = -numerator;
    }
    if (numerator < 0L) {
        if (numerator == LONG_MIN)
            goto cleanup;
        numerator = -numerator;
        scaled = de_simplify_unary_owned(expr_clone(rest), expr_neg);
        rest = scaled;
    }
    if (numerator == denominator) {
        if (!scaled)
            scaled = expr_clone(rest);
        goto cleanup;
    }
    if (numerator == -denominator) {
        expr_t *negative = expr_neg(rest);

        expr_free(scaled);
        scaled = negative;
        goto cleanup;
    }
    value = num_create_from_frac(numerator, denominator);
    coefficient_expr = expr_new_const(value);
    num_destroy(&value);
    if (coefficient_expr) {
        expr_t *product = expr_mul(coefficient_expr, rest);

        expr_free(scaled);
        scaled = product;
    }

cleanup:
    expr_free(coefficient_expr);
    num_destroy(&factor_value);
    return scaled;
}

static bool de_homogeneous_algebraic_log_solutions(const expr_t *independent, const expr_t *dependent,
                                                   const expr_t *substitution, const expr_t *integral,
                                                   equation_t **solutions_out, size_t *solution_count_out)
{
    const expr_t *product_left = NULL;
    const expr_t *product_right = NULL;
    const expr_t *sum = NULL;
    const expr_t *first_argument = NULL;
    const expr_t *second_argument = NULL;
    expr_t *simplified = NULL;
    expr_t *constant = NULL;
    expr_t *quadratic = NULL;
    expr_t *linear = NULL;
    expr_t *constant_term = NULL;
    expr_t *quadratic_constant = NULL;
    expr_t *constant_denominator = NULL;
    expr_t *scaled_constant = NULL;
    expr_t *b_squared = NULL;
    expr_t *a_times_c = NULL;
    expr_t *four_a_c = NULL;
    expr_t *discriminant = NULL;
    expr_t *root = NULL;
    expr_t *negative_linear = NULL;
    expr_t *positive_numerator = NULL;
    expr_t *negative_numerator = NULL;
    expr_t *positive_solution = NULL;
    expr_t *negative_solution = NULL;
    number_t coefficient = num_new();
    long first_coefficients[2];
    long second_coefficients[2];
    long numerator;
    long denominator;
    long reciprocal;
    long independent_exponent;
    long quadratic_coefficient;
    long first_linear_product;
    long second_linear_product;
    long linear_coefficient;
    long constant_coefficient;
    long denominator_coefficient;
    bool is_sub = false;
    bool solved = false;

    if (!solutions_out || !solution_count_out)
        goto cleanup;
    *solution_count_out = 0u;

    simplified = integral ? expr_simplify(integral) : NULL;
    if (!simplified || !expr_match_mul_expr(simplified, &product_left, &product_right))
        goto cleanup;
    if (expr_match_const_value(product_left, &coefficient))
        sum = product_right;
    else if (expr_match_const_value(product_right, &coefficient))
        sum = product_left;
    else
        goto cleanup;
    if (!num_get_small_rational(coefficient, &numerator, &denominator) || numerator == 0L || denominator <= 0L ||
        denominator % de_homogeneous_unsigned_magnitude(numerator) != 0u)
        goto cleanup;
    reciprocal =
        numerator < 0L ? -(denominator / (long)de_homogeneous_unsigned_magnitude(numerator)) : denominator / numerator;
    if (!expr_match_add_sub_expr(sum, &product_left, &product_right, &is_sub) || is_sub ||
        !expr_match_log_expr(product_left, &first_argument) || !expr_match_log_expr(product_right, &second_argument))
        goto cleanup;

    if (!de_homogeneous_primitive_affine_coefficients(first_argument, substitution, first_coefficients) ||
        !de_homogeneous_primitive_affine_coefficients(second_argument, substitution, second_coefficients) ||
        reciprocal > -2L)
        goto cleanup;
    independent_exponent = -reciprocal - 2L;
    if (!de_homogeneous_multiply_degrees(first_coefficients[1], second_coefficients[1], &quadratic_coefficient) ||
        quadratic_coefficient == 0L ||
        !de_homogeneous_multiply_degrees(first_coefficients[0], second_coefficients[1], &first_linear_product) ||
        !de_homogeneous_multiply_degrees(first_coefficients[1], second_coefficients[0], &second_linear_product) ||
        !de_homogeneous_add_degrees(first_linear_product, second_linear_product, false, &linear_coefficient) ||
        !de_homogeneous_multiply_degrees(first_coefficients[0], second_coefficients[0], &constant_coefficient) ||
        !de_homogeneous_multiply_degrees(2L, quadratic_coefficient, &denominator_coefficient))
        goto cleanup;

    quadratic = expr_const_long(quadratic_coefficient);
    linear = de_homogeneous_scaled_power(independent, linear_coefficient, 1L);
    quadratic_constant = de_homogeneous_scaled_power(independent, constant_coefficient, 2L);
    constant = de_arbitrary_constant();
    if (!quadratic || !linear || !constant)
        goto cleanup;
    constant_denominator =
        independent_exponent == 0L ? expr_const_one() : expr_pow_long(independent, independent_exponent);
    scaled_constant = constant_denominator ? expr_div_simplify_owned(constant, constant_denominator) : NULL;
    constant = NULL;
    constant_denominator = NULL;
    constant_term = quadratic_constant ? expr_sub_simplify_owned(quadratic_constant, scaled_constant)
                                       : de_simplify_unary_owned(scaled_constant, expr_neg);
    quadratic_constant = NULL;
    scaled_constant = NULL;
    b_squared = expr_pow_long(linear, 2L);
    a_times_c = constant_term ? expr_mul_simplify_owned(expr_clone(quadratic), expr_clone(constant_term)) : NULL;
    four_a_c = a_times_c ? expr_mul_simplify_owned(expr_const_long(4L), a_times_c) : NULL;
    a_times_c = NULL;
    discriminant = b_squared && four_a_c ? expr_sub_simplify_owned(b_squared, four_a_c) : NULL;
    b_squared = NULL;
    four_a_c = NULL;
    root = discriminant ? de_simplify_unary_owned(discriminant, expr_sqrt) : NULL;
    discriminant = NULL;
    negative_linear = de_simplify_unary_owned(expr_clone(linear), expr_neg);
    positive_numerator =
        negative_linear && root ? expr_add_simplify_owned(expr_clone(negative_linear), expr_clone(root)) : NULL;
    negative_numerator = negative_linear && root ? expr_sub_simplify_owned(negative_linear, root) : NULL;
    negative_linear = NULL;
    root = NULL;
    positive_solution =
        positive_numerator ? de_homogeneous_scale_expression_exact(positive_numerator, denominator_coefficient) : NULL;
    expr_free(positive_numerator);
    positive_numerator = NULL;
    negative_solution =
        negative_numerator ? de_homogeneous_scale_expression_exact(negative_numerator, denominator_coefficient) : NULL;
    expr_free(negative_numerator);
    negative_numerator = NULL;
    solutions_out[0] = positive_solution ? equ_new(dependent, positive_solution) : NULL;
    solutions_out[1] = negative_solution ? equ_new(dependent, negative_solution) : NULL;
    if (!solutions_out[0] || !solutions_out[1]) {
        equ_free(solutions_out[1]);
        equ_free(solutions_out[0]);
        solutions_out[0] = NULL;
        solutions_out[1] = NULL;
        goto cleanup;
    }
    *solution_count_out = 2u;
    solved = true;

cleanup:
    expr_free(negative_solution);
    expr_free(positive_solution);
    expr_free(negative_numerator);
    expr_free(positive_numerator);
    expr_free(negative_linear);
    expr_free(root);
    expr_free(discriminant);
    expr_free(four_a_c);
    expr_free(a_times_c);
    expr_free(b_squared);
    expr_free(scaled_constant);
    expr_free(constant_denominator);
    expr_free(quadratic_constant);
    expr_free(constant_term);
    expr_free(linear);
    expr_free(quadratic);
    expr_free(constant);
    expr_free(simplified);
    num_destroy(&coefficient);
    return solved;
}

static expr_t *de_homogeneous_substitute_simplified(const expr_t *expr, const expr_t *substitution, const expr_t *value)
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
    if (expr_name && substitution_name && strcmp(expr_name, substitution_name) == 0) {
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
        left_value = de_homogeneous_substitute_simplified(left, substitution, value);
        substituted = de_simplify_unary_owned(left_value, expr_neg);
        left_value = NULL;
        goto cleanup;
    }
    if (expr_match_add_sub_expr(expr, &left, &right, &is_sub)) {
        left_value = de_homogeneous_substitute_simplified(left, substitution, value);
        right_value = de_homogeneous_substitute_simplified(right, substitution, value);
        substituted = left_value && right_value ? (is_sub ? expr_sub_simplify_owned(left_value, right_value)
                                                          : expr_add_simplify_owned(left_value, right_value))
                                                : NULL;
        if (left_value && right_value) {
            left_value = NULL;
            right_value = NULL;
        }
        goto cleanup;
    }
    if (expr_match_mul_expr(expr, &left, &right) || expr_match_div_expr(expr, &left, &right)) {
        bool division = expr_match_div_expr(expr, &left, &right);

        left_value = de_homogeneous_substitute_simplified(left, substitution, value);
        right_value = de_homogeneous_substitute_simplified(right, substitution, value);
        if (left_value && right_value && division && de_homogeneous_is_one(right_value)) {
            substituted = left_value;
            left_value = NULL;
        } else if (left_value && right_value && !division && de_homogeneous_is_one(left_value)) {
            substituted = right_value;
            right_value = NULL;
        } else if (left_value && right_value && !division && de_homogeneous_is_one(right_value)) {
            substituted = left_value;
            left_value = NULL;
        } else {
            substituted = left_value && right_value ? (division ? expr_div_simplify_owned(left_value, right_value)
                                                                : expr_mul_simplify_owned(left_value, right_value))
                                                    : NULL;
        }
        if (left_value && right_value) {
            left_value = NULL;
            right_value = NULL;
        }
        goto cleanup;
    }
    if (expr_match_pow_const(expr, &left, &exponent)) {
        left_value = de_homogeneous_substitute_simplified(left, substitution, value);
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

        left_value = de_homogeneous_substitute_simplified(left, substitution, value);
        if (left_value && expr_match_const_value(left_value, &argument) && num_eq(argument, NUM_ONE)) {
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

static expr_t *de_homogeneous_constant(const diffequ_t *de, const expr_t *dependent, const expr_t *substitution,
                                       const expr_t *integral)
{
    const expr_t *point = NULL;
    const expr_t *value = NULL;
    expr_t *ratio_at_point = NULL;
    expr_t *integral_at_point = NULL;
    expr_t *log_point = NULL;
    expr_t *constant = NULL;
    number_t ratio_value;

    if (!de_find_initial_condition(de, dependent, &point, &value))
        return de_arbitrary_constant();
    if (expr_is_exact_zero(point))
        return NULL;

    ratio_value = num_new();
    ratio_at_point = expr_div_simplify_owned(expr_clone(value), expr_clone(point));
    if (ratio_at_point && expr_match_const_value(ratio_at_point, &ratio_value) && num_eq(ratio_value, NUM_ONE)) {
        expr_free(ratio_at_point);
        ratio_at_point = expr_const_one();
    }
    num_destroy(&ratio_value);
    integral_at_point =
        ratio_at_point ? de_homogeneous_substitute_simplified(integral, substitution, ratio_at_point) : NULL;
    log_point = de_log_abs(point);
    if (integral_at_point && log_point && expr_is_exact_zero(log_point)) {
        constant = integral_at_point;
        integral_at_point = NULL;
    } else {
        constant = integral_at_point && log_point ? expr_sub_simplify_owned(integral_at_point, log_point) : NULL;
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

de_attempt_t de_attempt_homogeneous(const diffequ_t *de, const expr_t *independent, const expr_t *dependent,
                                    const expr_t *derivative_right, equation_t **solutions_out,
                                    size_t *solution_count_out)
{
    const expr_t *condition_point = NULL;
    const expr_t *condition_value = NULL;
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

    if (!solutions_out || !solution_count_out)
        return DE_ATTEMPT_FAILED;
    *solution_count_out = 0u;

    substitution = expr_new_named_var(NUM_NAN, "__de_u");
    transformed = substitution
                      ? de_homogeneous_normalize_transformed(derivative_right, independent, dependent, substitution)
                      : NULL;
    if (!transformed || de_expr_uses(transformed, independent))
        goto cleanup;
    attempt = DE_ATTEMPT_FAILED;

    reciprocal = de_homogeneous_reciprocal_difference(transformed, substitution);
    expr_free(transformed);
    transformed = NULL;
    integral = reciprocal ? de_homogeneous_integrate_terms(reciprocal, substitution) : NULL;
    if (integral && !de_find_initial_condition(de, dependent, &condition_point, &condition_value)) {
        if (de_homogeneous_algebraic_log_solutions(independent, dependent, substitution, integral, solutions_out,
                                                   solution_count_out)) {
            attempt = DE_ATTEMPT_SOLVED;
            goto cleanup;
        }
    }
    ratio = expr_div_simplify_owned(expr_clone(dependent), expr_clone(independent));
    left = integral && ratio ? expr_substitute(integral, substitution, ratio) : NULL;
    log_independent = de_log_abs(independent);
    constant = integral ? de_homogeneous_constant(de, dependent, substitution, integral) : NULL;
    right = log_independent && constant ? expr_add(log_independent, constant) : NULL;
    if (!left || !right)
        goto cleanup;

    solutions_out[0] = equ_new(left, right);
    if (solutions_out[0]) {
        *solution_count_out = 1u;
        attempt = DE_ATTEMPT_SOLVED;
    }

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
