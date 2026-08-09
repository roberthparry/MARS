#include <limits.h>
#include <math.h>

#define MARS_DIFFEQUATION_PDE_INTERNAL_ACCESS
#include "diffequ_pde_internal.h"

static equation_t *de_pde_quadratic_boundary_solution(const diffequ_t *de, const expr_t *x, const expr_t *y,
                                                      const expr_t *dependent, const expr_t *invariant,
                                                      const expr_t *scaled_potential);

static equation_t *de_pde_dependent_square_boundary_solution(const diffequ_t *de, const expr_t *x, const expr_t *y,
                                                             const expr_t *dependent, const expr_t *invariant,
                                                             const expr_t *particular);

static bool de_pde_exact_long(const expr_t *expr, long *value_out)
{
    number_t value = num_new();
    number_t exact;
    double approximate;
    long candidate;
    bool matches;

    if (!value_out || !expr_match_const_value(expr, &value)) {
        num_destroy(&value);
        return false;
    }
    if (!num_is_real(value) || !num_is_integer(value)) {
        num_destroy(&value);
        return false;
    }
    approximate = num_to_double(value);
    if (!isfinite(approximate) || approximate <= (double)LONG_MIN || approximate >= (double)LONG_MAX) {
        num_destroy(&value);
        return false;
    }
    candidate = (long)approximate;
    exact = num_create_from_long(candidate);
    matches = num_eq(value, exact);
    num_destroy(&exact);
    num_destroy(&value);
    if (matches)
        *value_out = candidate;
    return matches;
}

static bool de_pde_is_characteristic_constant(const expr_t *expr, const expr_t *x, const expr_t *y,
                                              const expr_t *dependent)
{
    return expr && !de_expr_uses(expr, x) && !de_expr_uses(expr, y) && !de_expr_uses(expr, dependent);
}

static expr_t *de_pde_monomial_invariant(const expr_t *x, const expr_t *y, const expr_t *dependent,
                                         const expr_t *x_coefficient, const expr_t *y_coefficient)
{
    expr_t *numerator = expr_mul_simplify_owned(expr_clone(y_coefficient), expr_clone(x));
    expr_t *denominator = expr_mul_simplify_owned(expr_clone(x_coefficient), expr_clone(y));
    expr_t *power_ratio = numerator && denominator ? expr_div_simplify_owned(numerator, denominator) : NULL;
    expr_t *negative_power = NULL;
    expr_t *x_power = NULL;
    expr_t *invariant = NULL;
    long integer_power;

    numerator = NULL;
    denominator = NULL;
    if (!de_pde_is_characteristic_constant(power_ratio, x, y, dependent))
        goto cleanup;
    negative_power = expr_negate_owned(power_ratio);
    power_ratio = NULL;
    if (negative_power && de_pde_exact_long(negative_power, &integer_power) && integer_power == -1L) {
        invariant = expr_div_simplify_owned(expr_clone(y), expr_clone(x));
    } else {
        x_power = negative_power ? (de_pde_exact_long(negative_power, &integer_power) ? expr_pow_long(x, integer_power)
                                                                                      : expr_pow_xp(x, negative_power))
                                 : NULL;
        invariant = x_power ? expr_mul_simplify_owned(expr_clone(y), x_power) : NULL;
        x_power = NULL;
    }

cleanup:
    expr_free(x_power);
    expr_free(negative_power);
    expr_free(power_ratio);
    expr_free(denominator);
    expr_free(numerator);
    return invariant;
}

static expr_t *de_pde_radial_invariant(const expr_t *x, const expr_t *y, const expr_t *x_coefficient,
                                       const expr_t *y_coefficient)
{
    expr_t *x_squared = expr_pow_long(x, 2L);
    expr_t *y_squared = expr_pow_long(y, 2L);
    expr_t *invariant = x_squared && y_squared ? expr_add_simplify_owned(x_squared, y_squared) : NULL;
    expr_t *invariant_x;
    expr_t *invariant_y;
    expr_t *along_x;
    expr_t *along_y;
    expr_t *check_raw;
    expr_t *check;

    if (invariant) {
        x_squared = NULL;
        y_squared = NULL;
    }
    invariant_x = invariant ? expr_create_deriv(invariant, x) : NULL;
    invariant_y = invariant ? expr_create_deriv(invariant, y) : NULL;
    along_x = invariant_x ? expr_mul(x_coefficient, invariant_x) : NULL;
    along_y = invariant_y ? expr_mul(y_coefficient, invariant_y) : NULL;
    check_raw = along_x && along_y ? expr_add(along_x, along_y) : NULL;
    check = check_raw ? expr_simplify(check_raw) : NULL;

    if (!check || !expr_is_exact_zero(check)) {
        expr_free(invariant);
        invariant = NULL;
    }
    expr_free(check);
    expr_free(check_raw);
    expr_free(along_y);
    expr_free(along_x);
    expr_free(invariant_y);
    expr_free(invariant_x);
    expr_free(y_squared);
    expr_free(x_squared);
    return invariant;
}

static expr_t *de_pde_scaled_coordinate_invariant_candidate(const expr_t *x, const expr_t *y,
                                                            const expr_t *x_coefficient, const expr_t *y_coefficient,
                                                            const expr_t *numerator, const expr_t *denominator)
{
    expr_t *candidate = expr_div_simplify_owned(expr_clone(numerator), expr_clone(denominator));
    expr_t *candidate_x = candidate ? expr_create_deriv(candidate, x) : NULL;
    expr_t *candidate_y = candidate ? expr_create_deriv(candidate, y) : NULL;
    expr_t *along_x = candidate_x ? expr_mul(x_coefficient, candidate_x) : NULL;
    expr_t *along_y = candidate_y ? expr_mul(y_coefficient, candidate_y) : NULL;
    expr_t *check_raw = along_x && along_y ? expr_add(along_x, along_y) : NULL;
    expr_t *check = check_raw ? expr_simplify(check_raw) : NULL;

    if (!check || !expr_is_exact_zero(check)) {
        expr_free(candidate);
        candidate = NULL;
    }
    expr_free(check);
    expr_free(check_raw);
    expr_free(along_y);
    expr_free(along_x);
    expr_free(candidate_y);
    expr_free(candidate_x);
    return candidate;
}

static expr_t *de_pde_scaled_coordinate_invariant(const expr_t *x, const expr_t *y, const expr_t *x_coefficient,
                                                  const expr_t *y_coefficient)
{
    expr_t *derivative = !de_expr_uses(x_coefficient, y) ? expr_create_deriv(x_coefficient, x) : NULL;
    expr_t *expected = derivative ? expr_mul_simplify_owned(expr_clone(y), expr_clone(derivative)) : NULL;
    expr_t *invariant = expected && de_pde_same_symbolic_form(expected, y_coefficient)
                            ? expr_div_simplify_owned(expr_clone(y), expr_clone(x_coefficient))
                            : NULL;

    expr_free(expected);
    expr_free(derivative);
    if (invariant)
        return invariant;

    derivative = !de_expr_uses(y_coefficient, x) ? expr_create_deriv(y_coefficient, y) : NULL;
    expected = derivative ? expr_mul_simplify_owned(expr_clone(x), expr_clone(derivative)) : NULL;
    invariant = expected && de_pde_same_symbolic_form(expected, x_coefficient)
                    ? expr_div_simplify_owned(expr_clone(x), expr_clone(y_coefficient))
                    : NULL;
    expr_free(expected);
    expr_free(derivative);
    if (invariant)
        return invariant;

    invariant = de_pde_scaled_coordinate_invariant_candidate(x, y, x_coefficient, y_coefficient, y, x_coefficient);

    if (!invariant)
        invariant = de_pde_scaled_coordinate_invariant_candidate(x, y, x_coefficient, y_coefficient, x, y_coefficient);
    return invariant;
}

static bool de_pde_linear_field_coefficients(const expr_t *field, const expr_t *x, const expr_t *y,
                                             const expr_t *dependent, expr_t **x_coefficient_out,
                                             expr_t **y_coefficient_out)
{
    expr_t *without_x = NULL;
    expr_t *constant = NULL;
    bool valid = de_linear_decompose(field, x, x_coefficient_out, &without_x) &&
                 de_linear_decompose(without_x, y, y_coefficient_out, &constant) &&
                 de_pde_is_characteristic_constant(*x_coefficient_out, x, y, dependent) &&
                 de_pde_is_characteristic_constant(*y_coefficient_out, x, y, dependent) && expr_is_exact_zero(constant);

    expr_free(constant);
    expr_free(without_x);
    if (!valid) {
        expr_free(*y_coefficient_out);
        expr_free(*x_coefficient_out);
        *x_coefficient_out = NULL;
        *y_coefficient_out = NULL;
    }
    return valid;
}

static expr_t *de_pde_trace_zero_linear_invariant(const expr_t *x, const expr_t *y, const expr_t *dependent,
                                                  const expr_t *x_field, const expr_t *y_field)
{
    expr_t *a = NULL;
    expr_t *b = NULL;
    expr_t *c = NULL;
    expr_t *d = NULL;
    expr_t *trace_raw = NULL;
    expr_t *trace = NULL;
    expr_t *x_squared = NULL;
    expr_t *y_squared = NULL;
    expr_t *xy = NULL;
    expr_t *cx_squared = NULL;
    expr_t *two_a_xy = NULL;
    expr_t *b_y_squared = NULL;
    expr_t *first_difference = NULL;
    expr_t *invariant = NULL;

    if (!de_pde_linear_field_coefficients(x_field, x, y, dependent, &a, &b) ||
        !de_pde_linear_field_coefficients(y_field, x, y, dependent, &c, &d))
        goto cleanup;
    trace_raw = expr_add(a, d);
    trace = trace_raw ? expr_simplify(trace_raw) : NULL;
    if (!trace || !expr_is_exact_zero(trace))
        goto cleanup;

    x_squared = expr_pow_long(x, 2L);
    y_squared = expr_pow_long(y, 2L);
    xy = expr_mul_simplify_owned(expr_clone(x), expr_clone(y));
    cx_squared = c && x_squared ? expr_mul_simplify_owned(expr_clone(c), x_squared) : NULL;
    x_squared = NULL;
    two_a_xy = a && xy ? expr_mul_simplify_owned(expr_mul_long(a, 2L), xy) : NULL;
    xy = NULL;
    b_y_squared = b && y_squared ? expr_mul_simplify_owned(expr_clone(b), y_squared) : NULL;
    y_squared = NULL;
    first_difference = cx_squared && two_a_xy ? expr_sub_simplify_owned(cx_squared, two_a_xy) : NULL;
    cx_squared = NULL;
    two_a_xy = NULL;
    invariant = first_difference && b_y_squared ? expr_sub_simplify_owned(first_difference, b_y_squared) : NULL;
    first_difference = NULL;
    b_y_squared = NULL;
    if (invariant) {
        expr_t *simplified = expr_simplify(invariant);

        expr_free(invariant);
        invariant = simplified;
    }
    if (invariant && !de_expr_uses(invariant, x) && !de_expr_uses(invariant, y)) {
        expr_free(invariant);
        invariant = NULL;
    }

cleanup:
    expr_free(first_difference);
    expr_free(b_y_squared);
    expr_free(two_a_xy);
    expr_free(cx_squared);
    expr_free(xy);
    expr_free(y_squared);
    expr_free(x_squared);
    expr_free(trace);
    expr_free(trace_raw);
    expr_free(d);
    expr_free(c);
    expr_free(b);
    expr_free(a);
    return invariant;
}

static expr_t *de_pde_spiral_linear_invariant(const expr_t *x, const expr_t *y, const expr_t *dependent,
                                              const expr_t *x_field, const expr_t *y_field)
{
    expr_t *a = NULL;
    expr_t *b = NULL;
    expr_t *c = NULL;
    expr_t *d = NULL;
    expr_t *d_minus_a = NULL;
    expr_t *c_plus_b = NULL;
    expr_t *angle = NULL;
    expr_t *x_squared = NULL;
    expr_t *y_squared = NULL;
    expr_t *radius_squared = NULL;
    expr_t *log_radius_squared = NULL;
    expr_t *log_radius = NULL;
    expr_t *ratio = NULL;
    expr_t *scaled_log = NULL;
    expr_t *candidate = NULL;
    expr_t *candidate_x = NULL;
    expr_t *candidate_y = NULL;
    expr_t *along_x = NULL;
    expr_t *along_y = NULL;
    expr_t *check_raw = NULL;
    expr_t *check = NULL;
    expr_t *invariant = NULL;

    if (!de_pde_linear_field_coefficients(x_field, x, y, dependent, &a, &b) ||
        !de_pde_linear_field_coefficients(y_field, x, y, dependent, &c, &d))
        goto cleanup;
    d_minus_a = expr_sub_simplify_owned(expr_clone(d), expr_clone(a));
    c_plus_b = expr_add_simplify_owned(expr_clone(c), expr_clone(b));
    if (!d_minus_a || !c_plus_b || !expr_is_exact_zero(d_minus_a) || !expr_is_exact_zero(c_plus_b) ||
        expr_is_exact_zero(a))
        goto cleanup;

    angle = expr_atan2(y, x);
    x_squared = expr_pow_long(x, 2L);
    y_squared = expr_pow_long(y, 2L);
    radius_squared = x_squared && y_squared ? expr_add_simplify_owned(x_squared, y_squared) : NULL;
    x_squared = NULL;
    y_squared = NULL;
    log_radius_squared = radius_squared ? expr_log(radius_squared) : NULL;
    log_radius = log_radius_squared ? expr_div_long(log_radius_squared, 2L) : NULL;
    ratio = expr_div_simplify_owned(expr_clone(b), expr_clone(a));
    scaled_log = ratio && log_radius ? expr_mul_simplify_owned(ratio, expr_clone(log_radius)) : NULL;
    ratio = NULL;
    candidate = angle && scaled_log ? expr_add_simplify_owned(expr_clone(angle), scaled_log) : NULL;
    scaled_log = NULL;
    candidate_x = candidate ? expr_create_deriv(candidate, x) : NULL;
    candidate_y = candidate ? expr_create_deriv(candidate, y) : NULL;
    along_x = candidate_x ? expr_mul(x_field, candidate_x) : NULL;
    along_y = candidate_y ? expr_mul(y_field, candidate_y) : NULL;
    check_raw = along_x && along_y ? expr_add(along_x, along_y) : NULL;
    check = check_raw ? expr_simplify(check_raw) : NULL;
    if (check && expr_is_exact_zero(check)) {
        invariant = candidate;
        candidate = NULL;
    }

cleanup:
    expr_free(check);
    expr_free(check_raw);
    expr_free(along_y);
    expr_free(along_x);
    expr_free(candidate_y);
    expr_free(candidate_x);
    expr_free(candidate);
    expr_free(scaled_log);
    expr_free(ratio);
    expr_free(log_radius);
    expr_free(log_radius_squared);
    expr_free(radius_squared);
    expr_free(y_squared);
    expr_free(x_squared);
    expr_free(angle);
    expr_free(c_plus_b);
    expr_free(d_minus_a);
    expr_free(d);
    expr_free(c);
    expr_free(b);
    expr_free(a);
    return invariant;
}

static expr_t *de_pde_unit_characteristic_potential(const expr_t *x, const expr_t *y,
                                                    const expr_t *integration_variable,
                                                    const expr_t *integration_coefficient);

static expr_t *de_pde_separable_invariant(const expr_t *x, const expr_t *y, const expr_t *x_coefficient,
                                          const expr_t *y_coefficient)
{
    expr_t *x_potential = de_pde_unit_characteristic_potential(x, y, x, x_coefficient);
    expr_t *y_potential = de_pde_unit_characteristic_potential(x, y, y, y_coefficient);
    expr_t *invariant = x_potential && y_potential ? expr_sub_simplify_owned(y_potential, x_potential) : NULL;

    if (invariant) {
        x_potential = NULL;
        y_potential = NULL;
    }
    expr_free(y_potential);
    expr_free(x_potential);
    return invariant;
}

static expr_t *de_pde_cross_coordinate_invariant_candidate(const expr_t *x, const expr_t *y,
                                                           const expr_t *x_coefficient, const expr_t *y_coefficient,
                                                           const expr_t *leading_coordinate,
                                                           const expr_t *integration_variable, const expr_t *numerator,
                                                           const expr_t *denominator)
{
    expr_t *rate = expr_div_simplify_owned(expr_clone(numerator), expr_clone(denominator));
    expr_t *primitive = rate && de_expr_uses(rate, integration_variable) && !de_expr_uses(rate, leading_coordinate)
                            ? expr_integrate(rate, integration_variable)
                            : NULL;
    expr_t *candidate = primitive ? expr_sub_simplify_owned(expr_clone(leading_coordinate), primitive) : NULL;
    expr_t *candidate_x = candidate ? expr_create_deriv(candidate, x) : NULL;
    expr_t *candidate_y = candidate ? expr_create_deriv(candidate, y) : NULL;
    expr_t *along_x = candidate_x ? expr_mul(x_coefficient, candidate_x) : NULL;
    expr_t *along_y = candidate_y ? expr_mul(y_coefficient, candidate_y) : NULL;
    expr_t *check_raw = along_x && along_y ? expr_add(along_x, along_y) : NULL;
    expr_t *check = check_raw ? expr_simplify(check_raw) : NULL;

    primitive = NULL;

    if (!check || !expr_is_exact_zero(check)) {
        expr_free(candidate);
        candidate = NULL;
    }
    expr_free(check);
    expr_free(check_raw);
    expr_free(along_y);
    expr_free(along_x);
    expr_free(candidate_y);
    expr_free(candidate_x);
    expr_free(primitive);
    expr_free(rate);
    return candidate;
}

static expr_t *de_pde_cross_coordinate_invariant(const expr_t *x, const expr_t *y, const expr_t *x_coefficient,
                                                 const expr_t *y_coefficient)
{
    expr_t *invariant = de_pde_cross_coordinate_invariant_candidate(x, y, x_coefficient, y_coefficient, x, y,
                                                                    x_coefficient, y_coefficient);

    if (!invariant)
        invariant = de_pde_cross_coordinate_invariant_candidate(x, y, x_coefficient, y_coefficient, y, x, y_coefficient,
                                                                x_coefficient);
    return invariant;
}

static expr_t *de_pde_linear_characteristic_invariant(const expr_t *x, const expr_t *y, const expr_t *dependent,
                                                      const expr_t *x_coefficient, const expr_t *y_coefficient)
{
    expr_t *invariant = de_pde_monomial_invariant(x, y, dependent, x_coefficient, y_coefficient);

    if (!invariant)
        invariant = de_pde_spiral_linear_invariant(x, y, dependent, x_coefficient, y_coefficient);
    if (!invariant)
        invariant = de_pde_trace_zero_linear_invariant(x, y, dependent, x_coefficient, y_coefficient);
    if (!invariant)
        invariant = de_pde_radial_invariant(x, y, x_coefficient, y_coefficient);
    if (!invariant)
        invariant = de_pde_scaled_coordinate_invariant(x, y, x_coefficient, y_coefficient);
    if (!invariant)
        invariant = de_pde_cross_coordinate_invariant(x, y, x_coefficient, y_coefficient);
    if (!invariant)
        invariant = de_pde_separable_invariant(x, y, x_coefficient, y_coefficient);
    return invariant;
}

static expr_t *de_pde_characteristic_exponent(const expr_t *x, const expr_t *y, const expr_t *dependent,
                                              const expr_t *x_coefficient, const expr_t *y_coefficient,
                                              const expr_t *reaction_coefficient)
{
    expr_t *target = expr_neg(reaction_coefficient);
    expr_t *target_x = target ? expr_create_deriv(target, x) : NULL;
    expr_t *target_y = target ? expr_create_deriv(target, y) : NULL;
    expr_t *target_along_x = target_x ? expr_mul(x_coefficient, target_x) : NULL;
    expr_t *target_along_y = target_y ? expr_mul(y_coefficient, target_y) : NULL;
    expr_t *target_directional_raw = target_along_x && target_along_y ? expr_add(target_along_x, target_along_y) : NULL;
    expr_t *target_directional = target_directional_raw ? expr_simplify(target_directional_raw) : NULL;
    expr_t *target_square = target && target_directional &&
                                    de_pde_is_characteristic_constant(target_directional, x, y, dependent) &&
                                    !expr_is_exact_zero(target_directional)
                                ? expr_pow_long(target, 2L)
                                : NULL;
    expr_t *twice_target_directional = target_directional ? expr_mul_long(target_directional, 2L) : NULL;
    expr_t *phase_exponent = NULL;

    if (target_square && twice_target_directional) {
        phase_exponent = expr_div_simplify_owned(target_square, twice_target_directional);
        target_square = NULL;
        twice_target_directional = NULL;
    }
    expr_t *rate_raw = target ? expr_div(target, x_coefficient) : NULL;
    expr_t *rate = rate_raw ? expr_simplify(rate_raw) : NULL;
    expr_t *primitive = rate ? expr_integrate(rate, x) : NULL;
    expr_t *primitive_x = primitive ? expr_create_deriv(primitive, x) : NULL;
    expr_t *primitive_y = primitive ? expr_create_deriv(primitive, y) : NULL;
    expr_t *along_x = primitive_x ? expr_mul(x_coefficient, primitive_x) : NULL;
    expr_t *along_y = primitive_y ? expr_mul(y_coefficient, primitive_y) : NULL;
    expr_t *directional_raw = along_x && along_y ? expr_add(along_x, along_y) : NULL;
    expr_t *directional = directional_raw ? expr_display_expanded(directional_raw) : NULL;
    expr_t *scale_raw = target && directional ? expr_div(target, directional) : NULL;
    expr_t *scale = scale_raw ? expr_simplify(scale_raw) : NULL;
    expr_t *exponent_raw = scale && de_pde_is_characteristic_constant(scale, x, y, dependent)
                               ? expr_mul_simplify_owned(expr_clone(scale), expr_clone(primitive))
                               : NULL;
    expr_t *exponent =
        phase_exponent ? expr_simplify(phase_exponent) : (exponent_raw ? expr_simplify(exponent_raw) : NULL);
    expr_t *exponent_x = exponent ? expr_create_deriv(exponent, x) : NULL;
    expr_t *exponent_y = exponent ? expr_create_deriv(exponent, y) : NULL;
    expr_t *check_x = exponent_x ? expr_mul(x_coefficient, exponent_x) : NULL;
    expr_t *check_y = exponent_y ? expr_mul(y_coefficient, exponent_y) : NULL;
    expr_t *check_sum = check_x && check_y ? expr_add(check_x, check_y) : NULL;
    expr_t *check_raw = check_sum ? expr_add(check_sum, reaction_coefficient) : NULL;
    expr_t *check = check_raw ? expr_simplify(check_raw) : NULL;

    if (!check || !expr_is_exact_zero(check)) {
        expr_free(exponent);
        exponent = NULL;
    }
    expr_free(check);
    expr_free(check_raw);
    expr_free(check_sum);
    expr_free(check_y);
    expr_free(check_x);
    expr_free(exponent_y);
    expr_free(exponent_x);
    expr_free(exponent_raw);
    expr_free(scale);
    expr_free(scale_raw);
    expr_free(directional);
    expr_free(directional_raw);
    expr_free(along_y);
    expr_free(along_x);
    expr_free(primitive_y);
    expr_free(primitive_x);
    expr_free(primitive);
    expr_free(rate);
    expr_free(rate_raw);
    expr_free(phase_exponent);
    expr_free(twice_target_directional);
    expr_free(target_square);
    expr_free(target_directional);
    expr_free(target_directional_raw);
    expr_free(target_along_y);
    expr_free(target_along_x);
    expr_free(target_y);
    expr_free(target_x);
    expr_free(target);
    return exponent;
}

static expr_t *de_pde_characteristic_factor(const expr_t *exponent)
{
    const expr_t *log_argument = NULL;
    const expr_t *scaled_base = NULL;
    number_t scale = num_new();
    expr_t *factor = NULL;

    if (expr_match_log_expr(exponent, &log_argument)) {
        factor = expr_clone(log_argument);
    } else if (expr_match_scaled_expr(exponent, &scale, &scaled_base) &&
               expr_match_log_expr(scaled_base, &log_argument) && num_is_real(scale) && num_is_integer(scale)) {
        factor = expr_pow(log_argument, &scale);
    } else {
        expr_t *raw = expr_exp(exponent);

        factor = raw ? expr_simplify(raw) : NULL;
        expr_free(raw);
    }
    num_destroy(&scale);
    return factor;
}

static expr_t *de_pde_apply_characteristic_factor(const expr_t *factor, const expr_t *arbitrary)
{
    const expr_t *numerator = NULL;
    const expr_t *denominator = NULL;
    const expr_t *power_base = NULL;
    number_t value = num_new();
    number_t exponent;
    bool reciprocal = false;
    expr_t *raw;
    expr_t *result;

    if (expr_match_div_expr(factor, &numerator, &denominator) && expr_match_const_value(numerator, &value)) {
        reciprocal = num_eq(value, NUM_ONE);
    }
    num_destroy(&value);
    exponent = num_new();
    if (!reciprocal && expr_match_pow_const(factor, &power_base, &exponent) && num_eq(exponent, NUM_NEG_ONE)) {
        reciprocal = true;
        denominator = power_base;
    }
    num_destroy(&exponent);
    raw = reciprocal ? expr_div(arbitrary, denominator) : expr_mul(factor, arbitrary);
    result = raw ? expr_simplify(raw) : NULL;
    expr_free(raw);
    return result;
}

static expr_t *de_pde_apply_linear_characteristic_operator(const expr_t *x, const expr_t *y,
                                                           const expr_t *x_coefficient, const expr_t *y_coefficient,
                                                           const expr_t *reaction_coefficient, const expr_t *candidate)
{
    expr_t *candidate_x = expr_create_deriv(candidate, x);
    expr_t *candidate_y = expr_create_deriv(candidate, y);
    expr_t *along_x = candidate_x ? expr_mul(x_coefficient, candidate_x) : NULL;
    expr_t *along_y = candidate_y ? expr_mul(y_coefficient, candidate_y) : NULL;
    expr_t *transport = along_x && along_y ? expr_add(along_x, along_y) : NULL;
    expr_t *reaction = candidate ? expr_mul(reaction_coefficient, candidate) : NULL;
    expr_t *applied_raw = transport && reaction ? expr_add(transport, reaction) : NULL;
    expr_t *applied = applied_raw ? expr_simplify(applied_raw) : NULL;

    expr_free(applied_raw);
    expr_free(reaction);
    expr_free(transport);
    expr_free(along_y);
    expr_free(along_x);
    expr_free(candidate_y);
    expr_free(candidate_x);
    return applied;
}

static bool de_pde_matches_expanded_cotangent(const expr_t *candidate, const expr_t *cotangent)
{
    const expr_t *cot_argument = NULL;
    expr_t *cos_argument;
    expr_t *sin_argument;
    expr_t *quotient_raw;
    expr_t *quotient;
    expr_t *difference_raw;
    expr_t *difference;
    bool matches;

    if (!expr_match_cot_expr(cotangent, &cot_argument))
        return false;
    cos_argument = expr_cos(cot_argument);
    sin_argument = expr_sin(cot_argument);
    quotient_raw = cos_argument && sin_argument ? expr_div(cos_argument, sin_argument) : NULL;
    quotient = quotient_raw ? expr_simplify(quotient_raw) : NULL;
    difference_raw = quotient ? expr_sub(candidate, quotient) : NULL;
    difference = difference_raw ? expr_simplify(difference_raw) : NULL;
    matches = difference && expr_is_exact_zero(difference);

    expr_free(difference);
    expr_free(difference_raw);
    expr_free(quotient);
    expr_free(quotient_raw);
    expr_free(sin_argument);
    expr_free(cos_argument);
    return matches;
}

static bool de_pde_same_transport_value(const expr_t *left, const expr_t *right)
{
    return de_pde_same_symbolic_form(left, right) || de_pde_matches_expanded_cotangent(left, right) ||
           de_pde_matches_expanded_cotangent(right, left);
}

static expr_t *de_pde_unit_characteristic_potential(const expr_t *x, const expr_t *y,
                                                    const expr_t *integration_variable,
                                                    const expr_t *integration_coefficient)
{
    const expr_t *other_variable = expr_struct_eq(integration_variable, x) ? y : x;
    expr_t *reciprocal = expr_div_simplify_owned(expr_const_one(), expr_clone(integration_coefficient));
    expr_t *potential = reciprocal && !de_expr_uses(reciprocal, x) && !de_expr_uses(reciprocal, y)
                            ? expr_mul_simplify_owned(expr_clone(reciprocal), expr_clone(integration_variable))
                            : (reciprocal ? expr_integrate(reciprocal, integration_variable) : NULL);
    expr_t *derivative = potential ? expr_create_deriv(potential, integration_variable) : NULL;
    expr_t *ratio_raw = derivative && reciprocal ? expr_div(derivative, reciprocal) : NULL;
    expr_t *ratio = ratio_raw ? expr_simplify(ratio_raw) : NULL;
    expr_t *check = derivative && reciprocal ? expr_sub_simplify_owned(derivative, expr_clone(reciprocal)) : NULL;
    long ratio_value = 0L;
    bool derivative_matches =
        (check && expr_is_exact_zero(check)) || (ratio && de_pde_exact_long(ratio, &ratio_value) && ratio_value == 1L);

    derivative = NULL;
    if (!derivative_matches || de_expr_uses(potential, other_variable)) {
        expr_free(potential);
        potential = NULL;
    }
    expr_free(check);
    expr_free(ratio);
    expr_free(ratio_raw);
    expr_free(derivative);
    expr_free(reciprocal);
    return potential;
}

static bool de_pde_quadratic_characteristic_solutions(const diffequ_t *de, const expr_t *x, const expr_t *y,
                                                      const expr_t *dependent, const expr_t *x_coefficient,
                                                      const expr_t *y_coefficient, const expr_t *remainder,
                                                      equation_t **solutions_out, size_t *solution_count_out)
{
    expr_t *dependent_squared = expr_pow_long(dependent, 2L);
    expr_t *quadratic_coefficient = NULL;
    expr_t *forcing = NULL;
    expr_t *x_potential = NULL;
    expr_t *y_potential = NULL;
    expr_t *invariant = NULL;
    expr_t *arbitrary = NULL;
    expr_t *scaled_potential = NULL;
    expr_t *denominator = NULL;
    expr_t *right = NULL;
    expr_t *zero = NULL;
    bool solved = false;

    *solution_count_out = 0u;

    if (!dependent_squared || !de_linear_decompose(remainder, dependent_squared, &quadratic_coefficient, &forcing) ||
        !expr_is_exact_zero(forcing) || !de_pde_is_characteristic_constant(quadratic_coefficient, x, y, dependent) ||
        expr_is_exact_zero(quadratic_coefficient))
        goto cleanup;

    x_potential = de_pde_unit_characteristic_potential(x, y, x, x_coefficient);
    y_potential = de_pde_unit_characteristic_potential(x, y, y, y_coefficient);
    invariant =
        x_potential && y_potential ? expr_sub_simplify_owned(expr_clone(y_potential), expr_clone(x_potential)) : NULL;
    arbitrary = invariant ? expr_new_arbitrary_function("F", invariant) : NULL;
    scaled_potential =
        x_potential ? expr_mul_simplify_owned(expr_clone(quadratic_coefficient), expr_clone(x_potential)) : NULL;
    if (de && de->condition_count > 0u) {
        solutions_out[0] = de_pde_quadratic_boundary_solution(de, x, y, dependent, invariant, scaled_potential);
        solved = solutions_out[0] != NULL;
        if (solved)
            *solution_count_out = 1u;
        goto cleanup;
    }
    denominator = scaled_potential && arbitrary ? expr_add_simplify_owned(scaled_potential, arbitrary) : NULL;
    scaled_potential = NULL;
    arbitrary = NULL;
    right = denominator ? expr_div_simplify_owned(expr_const_one(), denominator) : NULL;
    denominator = NULL;
    if (!right)
        goto cleanup;

    solutions_out[0] = de_pde_solution_equation(dependent, right);
    zero = expr_const_zero();
    solutions_out[1] = zero ? de_pde_solution_equation(dependent, zero) : NULL;
    solved = solutions_out[0] && solutions_out[1];
    if (solved)
        *solution_count_out = 2u;

cleanup:
    if (!solved) {
        equ_free(solutions_out[0]);
        equ_free(solutions_out[1]);
        solutions_out[0] = NULL;
        solutions_out[1] = NULL;
    }
    expr_free(zero);
    expr_free(right);
    expr_free(denominator);
    expr_free(scaled_potential);
    expr_free(arbitrary);
    expr_free(invariant);
    expr_free(y_potential);
    expr_free(x_potential);
    expr_free(forcing);
    expr_free(quadratic_coefficient);
    expr_free(dependent_squared);
    return solved;
}

static expr_t *de_pde_characteristic_particular_along(const expr_t *x, const expr_t *y, const expr_t *dependent,
                                                      const expr_t *x_coefficient, const expr_t *y_coefficient,
                                                      const expr_t *reaction_coefficient, const expr_t *forcing,
                                                      const expr_t *integration_variable,
                                                      const expr_t *integration_coefficient)
{
    expr_t *target = expr_negate_owned(expr_clone(forcing));
    expr_t *rate_raw = target ? expr_div(target, integration_coefficient) : NULL;
    expr_t *rate = rate_raw ? expr_simplify(rate_raw) : NULL;
    expr_t *primitive = rate ? expr_integrate(rate, integration_variable) : NULL;
    expr_t *applied = primitive ? de_pde_apply_linear_characteristic_operator(x, y, x_coefficient, y_coefficient,
                                                                              reaction_coefficient, primitive)
                                : NULL;
    bool primitive_matches = target && applied && de_pde_same_transport_value(applied, target);
    expr_t *scale_raw = primitive_matches
                            ? expr_const_one()
                            : (target && applied && !expr_is_exact_zero(applied) ? expr_div(target, applied) : NULL);
    expr_t *scale = scale_raw ? expr_simplify(scale_raw) : NULL;
    expr_t *candidate_raw =
        scale && de_pde_is_characteristic_constant(scale, x, y, dependent) ? expr_mul(scale, primitive) : NULL;
    expr_t *candidate = candidate_raw ? expr_simplify(candidate_raw) : NULL;
    expr_t *candidate_applied = candidate ? de_pde_apply_linear_characteristic_operator(
                                                x, y, x_coefficient, y_coefficient, reaction_coefficient, candidate)
                                          : NULL;
    bool verified = candidate_applied && target && de_pde_same_transport_value(candidate_applied, target);

    if (!verified) {
        expr_free(candidate);
        candidate = NULL;
    }
    expr_free(candidate_applied);
    expr_free(candidate_raw);
    expr_free(scale);
    expr_free(scale_raw);
    expr_free(applied);
    expr_free(primitive);
    expr_free(rate);
    expr_free(rate_raw);
    expr_free(target);
    return candidate;
}

static expr_t *de_pde_characteristic_eigen_particular(const expr_t *x, const expr_t *y, const expr_t *dependent,
                                                      const expr_t *x_coefficient, const expr_t *y_coefficient,
                                                      const expr_t *reaction_coefficient, const expr_t *forcing)
{
    expr_t *target = expr_negate_owned(expr_clone(forcing));
    expr_t *applied =
        de_pde_apply_linear_characteristic_operator(x, y, x_coefficient, y_coefficient, reaction_coefficient, forcing);
    expr_t *eigenvalue_raw = applied && forcing ? expr_div(applied, forcing) : NULL;
    expr_t *eigenvalue = eigenvalue_raw ? expr_simplify(eigenvalue_raw) : NULL;
    expr_t *candidate_raw = target && eigenvalue && !expr_is_exact_zero(eigenvalue) &&
                                    de_pde_is_characteristic_constant(eigenvalue, x, y, dependent)
                                ? expr_div(target, eigenvalue)
                                : NULL;
    expr_t *candidate = candidate_raw ? expr_simplify(candidate_raw) : NULL;
    expr_t *candidate_applied = candidate ? de_pde_apply_linear_characteristic_operator(
                                                x, y, x_coefficient, y_coefficient, reaction_coefficient, candidate)
                                          : NULL;
    bool verified = candidate_applied && target && de_pde_same_transport_value(candidate_applied, target);

    if (!verified) {
        expr_free(candidate);
        candidate = NULL;
    }
    expr_free(candidate_applied);
    expr_free(candidate_raw);
    expr_free(eigenvalue);
    expr_free(eigenvalue_raw);
    expr_free(applied);
    expr_free(target);
    return candidate;
}

enum {
    DE_PDE_QUAD_ONE,
    DE_PDE_QUAD_X,
    DE_PDE_QUAD_Y,
    DE_PDE_QUAD_X2,
    DE_PDE_QUAD_XY,
    DE_PDE_QUAD_Y2,
    DE_PDE_QUAD_COUNT
};

static void de_pde_quadratic_zero(number_t *coefficients)
{
    for (size_t i = 0u; i < DE_PDE_QUAD_COUNT; ++i)
        coefficients[i] = num_clone(NUM_ZERO);
}

static void de_pde_quadratic_clear(number_t *coefficients)
{
    for (size_t i = 0u; i < DE_PDE_QUAD_COUNT; ++i)
        num_destroy(&coefficients[i]);
}

static int de_pde_quadratic_product_index(size_t left, size_t right)
{
    static const unsigned char x_power[DE_PDE_QUAD_COUNT] = {0u, 1u, 0u, 2u, 1u, 0u};
    static const unsigned char y_power[DE_PDE_QUAD_COUNT] = {0u, 0u, 1u, 0u, 1u, 2u};
    unsigned int product_x = x_power[left] + x_power[right];
    unsigned int product_y = y_power[left] + y_power[right];

    if (product_x + product_y > 2u)
        return -1;
    for (size_t i = 0u; i < DE_PDE_QUAD_COUNT; ++i) {
        if (x_power[i] == product_x && y_power[i] == product_y)
            return (int)i;
    }
    return -1;
}

static bool de_pde_quadratic_multiply(const number_t *left, const number_t *right, number_t *out)
{
    number_t product[DE_PDE_QUAD_COUNT];
    bool fits = true;

    de_pde_quadratic_zero(product);
    for (size_t i = 0u; i < DE_PDE_QUAD_COUNT; ++i) {
        for (size_t j = 0u; j < DE_PDE_QUAD_COUNT; ++j) {
            int index;
            number_t term;
            number_t sum;

            if (num_is_zero(left[i]) || num_is_zero(right[j]))
                continue;
            index = de_pde_quadratic_product_index(i, j);
            if (index < 0) {
                fits = false;
                goto cleanup;
            }
            term = num_mul(left[i], right[j]);
            sum = num_add(product[(size_t)index], term);
            num_destroy(&product[(size_t)index]);
            product[(size_t)index] = sum;
            num_destroy(&term);
        }
    }

    for (size_t i = 0u; i < DE_PDE_QUAD_COUNT; ++i) {
        num_destroy(&out[i]);
        out[i] = num_clone(product[i]);
    }

cleanup:
    de_pde_quadratic_clear(product);
    return fits;
}

static long de_pde_quadratic_exponent(number_t value)
{
    if (!num_is_real(value) || !num_is_integer(value))
        return -1L;
    for (long exponent = 0L; exponent <= 2L; ++exponent) {
        number_t candidate = num_create_from_long(exponent);
        bool equal = num_eq(value, candidate);

        num_destroy(&candidate);
        if (equal)
            return exponent;
    }
    return -1L;
}

static bool de_pde_collect_quadratic(const expr_t *expr, const expr_t *x, const expr_t *y, number_t *out)
{
    number_t value = num_new();
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    bool is_sub = false;

    if (!expr || !x || !y || !out)
        goto no_match;

    if (expr_struct_eq(expr, x) || expr_struct_eq(expr, y)) {
        size_t index = expr_struct_eq(expr, x) ? DE_PDE_QUAD_X : DE_PDE_QUAD_Y;

        for (size_t i = 0u; i < DE_PDE_QUAD_COUNT; ++i) {
            num_destroy(&out[i]);
            out[i] = num_clone(i == index ? NUM_ONE : NUM_ZERO);
        }
        num_destroy(&value);
        return true;
    }

    if (expr_match_const_value(expr, &value)) {
        for (size_t i = 0u; i < DE_PDE_QUAD_COUNT; ++i) {
            num_destroy(&out[i]);
            out[i] = num_clone(i == DE_PDE_QUAD_ONE ? value : NUM_ZERO);
        }
        num_destroy(&value);
        return true;
    }

    if (expr_match_add_sub_expr(expr, &left, &right, &is_sub)) {
        number_t left_poly[DE_PDE_QUAD_COUNT];
        number_t right_poly[DE_PDE_QUAD_COUNT];
        bool ok;

        de_pde_quadratic_zero(left_poly);
        de_pde_quadratic_zero(right_poly);
        ok = de_pde_collect_quadratic(left, x, y, left_poly) && de_pde_collect_quadratic(right, x, y, right_poly);
        if (ok) {
            for (size_t i = 0u; i < DE_PDE_QUAD_COUNT; ++i) {
                number_t sum = is_sub ? num_sub(left_poly[i], right_poly[i]) : num_add(left_poly[i], right_poly[i]);

                num_destroy(&out[i]);
                out[i] = sum;
            }
        }
        de_pde_quadratic_clear(right_poly);
        de_pde_quadratic_clear(left_poly);
        num_destroy(&value);
        return ok;
    }

    if (expr_match_neg_expr(expr, &left)) {
        number_t inner[DE_PDE_QUAD_COUNT];
        bool ok;

        de_pde_quadratic_zero(inner);
        ok = de_pde_collect_quadratic(left, x, y, inner);
        if (ok) {
            for (size_t i = 0u; i < DE_PDE_QUAD_COUNT; ++i) {
                number_t negative = num_neg(inner[i]);

                num_destroy(&out[i]);
                out[i] = negative;
            }
        }
        de_pde_quadratic_clear(inner);
        num_destroy(&value);
        return ok;
    }

    if (expr_match_mul_expr(expr, &left, &right)) {
        number_t left_poly[DE_PDE_QUAD_COUNT];
        number_t right_poly[DE_PDE_QUAD_COUNT];
        bool ok;

        de_pde_quadratic_zero(left_poly);
        de_pde_quadratic_zero(right_poly);
        ok = de_pde_collect_quadratic(left, x, y, left_poly) && de_pde_collect_quadratic(right, x, y, right_poly) &&
             de_pde_quadratic_multiply(left_poly, right_poly, out);
        de_pde_quadratic_clear(right_poly);
        de_pde_quadratic_clear(left_poly);
        num_destroy(&value);
        return ok;
    }

    {
        const expr_t *base = NULL;

        if (expr_match_pow_const(expr, &base, &value)) {
            long exponent = de_pde_quadratic_exponent(value);
            number_t base_poly[DE_PDE_QUAD_COUNT];
            number_t result[DE_PDE_QUAD_COUNT];
            bool ok = exponent >= 0L;

            de_pde_quadratic_zero(base_poly);
            de_pde_quadratic_zero(result);
            num_destroy(&result[DE_PDE_QUAD_ONE]);
            result[DE_PDE_QUAD_ONE] = num_clone(NUM_ONE);
            if (ok)
                ok = de_pde_collect_quadratic(base, x, y, base_poly);
            for (long i = 0L; ok && i < exponent; ++i) {
                number_t next[DE_PDE_QUAD_COUNT];

                de_pde_quadratic_zero(next);
                ok = de_pde_quadratic_multiply(result, base_poly, next);
                if (ok) {
                    for (size_t j = 0u; j < DE_PDE_QUAD_COUNT; ++j) {
                        num_destroy(&result[j]);
                        result[j] = num_clone(next[j]);
                    }
                }
                de_pde_quadratic_clear(next);
            }
            if (ok) {
                for (size_t i = 0u; i < DE_PDE_QUAD_COUNT; ++i) {
                    num_destroy(&out[i]);
                    out[i] = num_clone(result[i]);
                }
            }
            de_pde_quadratic_clear(result);
            de_pde_quadratic_clear(base_poly);
            num_destroy(&value);
            return ok;
        }
    }

no_match:
    num_destroy(&value);
    return false;
}

static bool de_pde_quadratic_scale(const expr_t *target, const expr_t *applied, const expr_t *x, const expr_t *y,
                                   number_t *scale_out)
{
    number_t target_poly[DE_PDE_QUAD_COUNT];
    number_t applied_poly[DE_PDE_QUAD_COUNT];
    number_t scale = num_new();
    bool has_scale = false;
    bool matches;

    de_pde_quadratic_zero(target_poly);
    de_pde_quadratic_zero(applied_poly);
    matches =
        de_pde_collect_quadratic(target, x, y, target_poly) && de_pde_collect_quadratic(applied, x, y, applied_poly);
    for (size_t i = 0u; matches && i < DE_PDE_QUAD_COUNT; ++i) {
        if (num_is_zero(applied_poly[i])) {
            matches = num_is_zero(target_poly[i]);
            continue;
        }
        if (!has_scale) {
            num_destroy(&scale);
            scale = num_div(target_poly[i], applied_poly[i]);
            has_scale = true;
        } else {
            number_t scaled = num_mul(applied_poly[i], scale);

            matches = num_eq(target_poly[i], scaled);
            num_destroy(&scaled);
        }
    }
    matches = matches && has_scale;
    if (matches) {
        num_destroy(scale_out);
        *scale_out = num_clone(scale);
    }
    num_destroy(&scale);
    de_pde_quadratic_clear(applied_poly);
    de_pde_quadratic_clear(target_poly);
    return matches;
}

static expr_t *de_pde_characteristic_scaled_trial(const expr_t *x, const expr_t *y, const expr_t *dependent,
                                                  const expr_t *x_coefficient, const expr_t *y_coefficient,
                                                  const expr_t *reaction_coefficient, const expr_t *target,
                                                  const expr_t *trial)
{
    expr_t *applied =
        de_pde_apply_linear_characteristic_operator(x, y, x_coefficient, y_coefficient, reaction_coefficient, trial);
    expr_t *scale_raw = applied && !expr_is_exact_zero(applied) ? expr_div(target, applied) : NULL;
    expr_t *scale = scale_raw ? expr_simplify(scale_raw) : NULL;
    number_t polynomial_scale = num_new();
    expr_t *polynomial_scale_expr = NULL;
    expr_t *candidate_raw;
    expr_t *candidate;

    if ((!scale || !de_pde_is_characteristic_constant(scale, x, y, dependent)) && applied &&
        de_pde_quadratic_scale(target, applied, x, y, &polynomial_scale)) {
        polynomial_scale_expr = expr_new_const(polynomial_scale);
    }
    candidate_raw = (polynomial_scale_expr || (scale && de_pde_is_characteristic_constant(scale, x, y, dependent)))
                        ? expr_mul(polynomial_scale_expr ? polynomial_scale_expr : scale, trial)
                        : NULL;
    candidate = candidate_raw ? expr_simplify(candidate_raw) : NULL;

    expr_free(polynomial_scale_expr);
    num_destroy(&polynomial_scale);
    expr_free(candidate_raw);
    expr_free(scale);
    expr_free(scale_raw);
    expr_free(applied);
    return candidate;
}

static expr_t *de_pde_characteristic_basis_particular(const expr_t *x, const expr_t *y, const expr_t *dependent,
                                                      const expr_t *x_coefficient, const expr_t *y_coefficient,
                                                      const expr_t *reaction_coefficient, const expr_t *forcing)
{
    expr_t *target = expr_negate_owned(expr_clone(forcing));
    expr_t *trials[5] = {expr_clone(x), expr_clone(y), expr_pow_long(x, 2L),
                         expr_mul_simplify_owned(expr_clone(x), expr_clone(y)), expr_pow_long(y, 2L)};
    expr_t *particular = NULL;

    for (size_t i = 0u; target && i < 5u && !particular; ++i) {
        if (!trials[i])
            continue;
        particular = de_pde_characteristic_scaled_trial(x, y, dependent, x_coefficient, y_coefficient,
                                                        reaction_coefficient, target, trials[i]);
    }
    for (size_t i = 0u; i < 5u; ++i)
        expr_free(trials[i]);
    expr_free(target);
    return particular;
}

static expr_t *de_pde_characteristic_invariant_particular_with_potential(const expr_t *x, const expr_t *y,
                                                                         const expr_t *x_coefficient,
                                                                         const expr_t *y_coefficient,
                                                                         const expr_t *reaction_coefficient,
                                                                         const expr_t *target, const expr_t *potential)
{
    expr_t *zero = expr_const_zero();
    expr_t *target_directional =
        zero && target ? de_pde_apply_linear_characteristic_operator(x, y, x_coefficient, y_coefficient, zero, target)
                       : NULL;
    expr_t *candidate_raw = potential && target ? expr_mul(potential, target) : NULL;
    expr_t *candidate = candidate_raw ? expr_simplify(candidate_raw) : NULL;
    bool verified =
        expr_is_exact_zero(reaction_coefficient) && target_directional && expr_is_exact_zero(target_directional);

    if (!verified) {
        expr_free(candidate);
        candidate = NULL;
    }
    expr_free(candidate_raw);
    expr_free(target_directional);
    expr_free(zero);
    return candidate;
}

static expr_t *
de_pde_characteristic_invariant_particular_along(const expr_t *x, const expr_t *y, const expr_t *x_coefficient,
                                                 const expr_t *y_coefficient, const expr_t *reaction_coefficient,
                                                 const expr_t *target, const expr_t *integration_variable,
                                                 const expr_t *integration_coefficient)
{
    expr_t *potential = de_pde_unit_characteristic_potential(x, y, integration_variable, integration_coefficient);
    expr_t *candidate = potential ? de_pde_characteristic_invariant_particular_with_potential(
                                        x, y, x_coefficient, y_coefficient, reaction_coefficient, target, potential)
                                  : NULL;

    expr_free(potential);
    return candidate;
}

static expr_t *de_pde_characteristic_invariant_particular(const expr_t *x, const expr_t *y, const expr_t *x_coefficient,
                                                          const expr_t *y_coefficient,
                                                          const expr_t *reaction_coefficient, const expr_t *forcing)
{
    expr_t *target = expr_negate_owned(expr_clone(forcing));
    expr_t *x_potential = de_pde_unit_characteristic_potential(x, y, x, x_coefficient);
    expr_t *y_potential = de_pde_unit_characteristic_potential(x, y, y, y_coefficient);
    expr_t *potential_sum =
        x_potential && y_potential ? expr_add_simplify_owned(expr_clone(x_potential), expr_clone(y_potential)) : NULL;
    expr_t *average = potential_sum ? expr_div_long(potential_sum, 2L) : NULL;
    expr_t *particular = target && average
                             ? de_pde_characteristic_invariant_particular_with_potential(
                                   x, y, x_coefficient, y_coefficient, reaction_coefficient, target, average)
                             : NULL;

    if (!particular && target && x_potential)
        particular = de_pde_characteristic_invariant_particular_along(x, y, x_coefficient, y_coefficient,
                                                                      reaction_coefficient, target, x, x_coefficient);
    if (!particular && target && y_potential)
        particular = de_pde_characteristic_invariant_particular_along(x, y, x_coefficient, y_coefficient,
                                                                      reaction_coefficient, target, y, y_coefficient);
    expr_free(average);
    expr_free(potential_sum);
    expr_free(y_potential);
    expr_free(x_potential);
    expr_free(target);
    return particular;
}

static expr_t *de_pde_characteristic_particular(const expr_t *x, const expr_t *y, const expr_t *dependent,
                                                const expr_t *x_coefficient, const expr_t *y_coefficient,
                                                const expr_t *reaction_coefficient, const expr_t *forcing)
{
    expr_t *particular = de_pde_characteristic_eigen_particular(x, y, dependent, x_coefficient, y_coefficient,
                                                                reaction_coefficient, forcing);

    if (!particular)
        particular = de_pde_characteristic_basis_particular(x, y, dependent, x_coefficient, y_coefficient,
                                                            reaction_coefficient, forcing);
    if (!particular)
        particular = de_pde_characteristic_invariant_particular(x, y, x_coefficient, y_coefficient,
                                                                reaction_coefficient, forcing);
    if (!particular)
        particular = de_pde_characteristic_particular_along(x, y, dependent, x_coefficient, y_coefficient,
                                                            reaction_coefficient, forcing, x, x_coefficient);

    if (!particular)
        particular = de_pde_characteristic_particular_along(x, y, dependent, x_coefficient, y_coefficient,
                                                            reaction_coefficient, forcing, y, y_coefficient);
    return particular;
}

static expr_t *de_pde_remove_dependent_factor(const expr_t *coefficient, const expr_t *dependent)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    bool left_uses;
    bool right_uses;
    expr_t *reduced;

    if (expr_struct_eq(coefficient, dependent))
        return expr_const_one();
    if (expr_match_neg_expr(coefficient, &left)) {
        reduced = de_pde_remove_dependent_factor(left, dependent);
        return reduced ? expr_negate_owned(reduced) : NULL;
    }
    if (!expr_match_mul_expr(coefficient, &left, &right))
        return NULL;
    left_uses = de_expr_uses(left, dependent);
    right_uses = de_expr_uses(right, dependent);
    if (left_uses == right_uses)
        return NULL;
    reduced = de_pde_remove_dependent_factor(left_uses ? left : right, dependent);
    return reduced ? expr_mul_simplify_owned(reduced, expr_clone(left_uses ? right : left)) : NULL;
}

static bool de_pde_dependent_square_solutions(const diffequ_t *de, const expr_t *x, const expr_t *y,
                                              const expr_t *dependent, const expr_t *x_coefficient,
                                              const expr_t *y_coefficient, const expr_t *remainder,
                                              equation_t **solutions_out, size_t *solution_count_out)
{
    expr_t *base_x = NULL;
    expr_t *base_y = NULL;
    expr_t *scaled_remainder_raw = NULL;
    expr_t *scaled_remainder = NULL;
    expr_t *zero = expr_const_zero();
    expr_t *transformed_forcing = NULL;
    expr_t *invariant;
    expr_t *arbitrary;
    expr_t *particular;
    expr_t *squared_right;
    expr_t *root_raw = NULL;
    expr_t *root = NULL;
    expr_t *negative_root = NULL;
    bool solved = false;

    if (!de_expr_uses(remainder, dependent)) {
        base_x = de_pde_remove_dependent_factor(x_coefficient, dependent);
        base_y = de_pde_remove_dependent_factor(y_coefficient, dependent);
        transformed_forcing = base_x && base_y ? expr_mul_long(remainder, 2L) : NULL;
    } else if (!de_expr_uses(x_coefficient, dependent) && !de_expr_uses(y_coefficient, dependent)) {
        scaled_remainder_raw = expr_mul(remainder, dependent);
        scaled_remainder = scaled_remainder_raw ? expr_simplify(scaled_remainder_raw) : NULL;
        if (scaled_remainder && !de_expr_uses(scaled_remainder, dependent)) {
            base_x = expr_clone(x_coefficient);
            base_y = expr_clone(y_coefficient);
            transformed_forcing = expr_mul_long(scaled_remainder, 2L);
        }
    }
    invariant = base_x && base_y && transformed_forcing
                    ? de_pde_linear_characteristic_invariant(x, y, dependent, base_x, base_y)
                    : NULL;
    arbitrary = invariant ? expr_new_arbitrary_function("F", invariant) : NULL;
    particular = arbitrary && transformed_forcing && zero && !expr_is_exact_zero(transformed_forcing)
                     ? de_pde_characteristic_particular(x, y, dependent, base_x, base_y, zero, transformed_forcing)
                     : NULL;
    squared_right =
        arbitrary && expr_is_exact_zero(transformed_forcing)
            ? expr_clone(arbitrary)
            : (arbitrary && particular ? expr_add_simplify_owned(expr_clone(arbitrary), expr_clone(particular)) : NULL);
    if (de && de->condition_count > 0u) {
        expr_t *zero_particular = particular ? NULL : expr_const_zero();

        solutions_out[0] = de_pde_dependent_square_boundary_solution(de, x, y, dependent, invariant,
                                                                     particular ? particular : zero_particular);
        expr_free(zero_particular);
        if (solutions_out[0]) {
            *solution_count_out = 1u;
            solved = true;
        }
        goto cleanup;
    }
    root_raw = squared_right ? expr_sqrt(squared_right) : NULL;
    root = root_raw ? expr_simplify(root_raw) : NULL;
    negative_root = root ? expr_negate_owned(expr_clone(root)) : NULL;

    if (root && negative_root) {
        solutions_out[0] = de_pde_solution_equation(dependent, root);
        solutions_out[1] = de_pde_solution_equation(dependent, negative_root);
        solved = solutions_out[0] && solutions_out[1];
        if (solved)
            *solution_count_out = 2u;
    }

cleanup:
    if (!solved) {
        equ_free(solutions_out[0]);
        equ_free(solutions_out[1]);
        solutions_out[0] = NULL;
        solutions_out[1] = NULL;
    }
    expr_free(negative_root);
    expr_free(root);
    expr_free(root_raw);
    expr_free(squared_right);
    expr_free(particular);
    expr_free(arbitrary);
    expr_free(invariant);
    expr_free(transformed_forcing);
    expr_free(zero);
    expr_free(scaled_remainder);
    expr_free(scaled_remainder_raw);
    expr_free(base_y);
    expr_free(base_x);
    return solved;
}

static expr_t *de_pde_substitute_characteristic_curve(const expr_t *expr, const expr_t *x, const expr_t *y,
                                                      const expr_t *curve_x, const expr_t *curve_y)
{
    expr_t *after_x = expr_substitute(expr, x, curve_x);
    expr_t *after_y = after_x ? expr_substitute(after_x, y, curve_y) : NULL;

    expr_free(after_x);
    return after_y;
}

static bool de_pde_affine_parameter_components(const expr_t *expr, const expr_t *parameter, expr_t **coefficient_out,
                                               expr_t **constant_out)
{
    expr_t *coefficient = NULL;
    expr_t *constant = NULL;
    expr_t *one = NULL;
    expr_t *at_one = NULL;
    expr_t *constant_raw = NULL;
    bool matches = false;

    *coefficient_out = NULL;
    *constant_out = NULL;
    if (!expr || !parameter)
        return false;
    if (de_linear_decompose(expr, parameter, &coefficient, &constant) && !de_expr_uses(coefficient, parameter) &&
        !de_expr_uses(constant, parameter)) {
        matches = true;
        goto cleanup;
    }
    expr_free(constant);
    expr_free(coefficient);
    constant = NULL;
    coefficient = expr_create_deriv(expr, parameter);
    if (!coefficient || de_expr_uses(coefficient, parameter))
        goto cleanup;
    one = expr_const_one();
    at_one = one ? expr_substitute(expr, parameter, one) : NULL;
    constant_raw = at_one ? expr_sub(at_one, coefficient) : NULL;
    constant = constant_raw ? expr_simplify(constant_raw) : NULL;
    matches = constant && !de_expr_uses(constant, parameter);

cleanup:
    if (matches) {
        *coefficient_out = coefficient;
        *constant_out = constant;
        coefficient = NULL;
        constant = NULL;
    }
    expr_free(constant_raw);
    expr_free(at_one);
    expr_free(one);
    expr_free(constant);
    expr_free(coefficient);
    return matches;
}

static expr_t *de_pde_boundary_parameter_value(const expr_t *boundary_invariant, const expr_t *parameter,
                                               const expr_t *current_invariant)
{
    expr_t *coefficient = NULL;
    expr_t *constant = NULL;
    expr_t *numerator = NULL;
    expr_t *value = NULL;
    expr_t *reciprocal_coordinate = NULL;
    expr_t *parameter_from_reciprocal = NULL;
    expr_t *transformed_raw = NULL;
    expr_t *transformed = NULL;
    expr_t *logarithm = NULL;
    const expr_t *exponent = NULL;
    long exponent_coefficient = 0L;

    if (!boundary_invariant || !parameter || !current_invariant)
        return NULL;
    if (de_pde_affine_parameter_components(boundary_invariant, parameter, &coefficient, &constant) &&
        !expr_is_exact_zero(coefficient) && !de_expr_uses(coefficient, parameter)) {
        if (de_pde_exact_long(coefficient, &exponent_coefficient) && exponent_coefficient == -1L) {
            value = expr_sub_simplify_owned(constant, expr_clone(current_invariant));
            constant = NULL;
            goto cleanup;
        }
        numerator = expr_sub_simplify_owned(expr_clone(current_invariant), constant);
        constant = NULL;
        if (numerator && de_pde_exact_long(coefficient, &exponent_coefficient) && exponent_coefficient == 1L) {
            value = numerator;
            numerator = NULL;
        } else {
            value = numerator ? expr_div_simplify_owned(numerator, coefficient) : NULL;
            numerator = NULL;
            coefficient = NULL;
        }
        goto cleanup;
    }
    expr_free(constant);
    expr_free(coefficient);
    constant = NULL;
    coefficient = NULL;

    if (expr_match_exp_expr(boundary_invariant, &exponent) &&
        de_pde_affine_parameter_components(exponent, parameter, &coefficient, &constant) &&
        !expr_is_exact_zero(coefficient)) {
        logarithm = expr_log(current_invariant);
        if (logarithm && de_pde_exact_long(coefficient, &exponent_coefficient) && exponent_coefficient == -1L) {
            value = expr_sub_simplify_owned(constant, logarithm);
            constant = NULL;
            logarithm = NULL;
            goto cleanup;
        }
        numerator = logarithm ? expr_sub_simplify_owned(logarithm, constant) : NULL;
        logarithm = NULL;
        constant = NULL;
        if (numerator && de_pde_exact_long(coefficient, &exponent_coefficient) && exponent_coefficient == 1L) {
            value = numerator;
            numerator = NULL;
        } else {
            value = numerator ? expr_div_simplify_owned(numerator, coefficient) : NULL;
            numerator = NULL;
            coefficient = NULL;
        }
        goto cleanup;
    }
    expr_free(constant);
    expr_free(coefficient);
    constant = NULL;
    coefficient = NULL;

    reciprocal_coordinate = expr_new_named_var(NUM_NAN, "ρ");
    parameter_from_reciprocal =
        reciprocal_coordinate ? expr_div_simplify_owned(expr_const_one(), expr_clone(reciprocal_coordinate)) : NULL;
    transformed_raw =
        parameter_from_reciprocal ? expr_substitute(boundary_invariant, parameter, parameter_from_reciprocal) : NULL;
    transformed = transformed_raw ? expr_display_expanded(transformed_raw) : NULL;
    if (reciprocal_coordinate && transformed &&
        de_pde_affine_parameter_components(transformed, reciprocal_coordinate, &coefficient, &constant) &&
        !expr_is_exact_zero(coefficient) && !de_expr_uses(coefficient, parameter)) {
        numerator = expr_sub_simplify_owned(expr_clone(current_invariant), constant);
        constant = NULL;
        value = numerator ? expr_div_simplify_owned(coefficient, numerator) : NULL;
        coefficient = NULL;
        numerator = NULL;
        goto cleanup;
    }
    expr_free(constant);
    expr_free(coefficient);
    expr_free(transformed);
    expr_free(transformed_raw);
    constant = NULL;
    coefficient = NULL;
    transformed = NULL;
    transformed_raw = NULL;

cleanup:
    expr_free(logarithm);
    expr_free(transformed);
    expr_free(transformed_raw);
    expr_free(parameter_from_reciprocal);
    expr_free(reciprocal_coordinate);
    expr_free(numerator);
    expr_free(constant);
    expr_free(coefficient);
    return value;
}

static expr_t *de_pde_boundary_parameter_squared_value(const expr_t *boundary_invariant, const expr_t *parameter,
                                                       const expr_t *current_invariant)
{
    expr_t *parameter_squared = NULL;
    expr_t *reciprocal_square = NULL;
    expr_t *coefficient = NULL;
    expr_t *constant = NULL;
    expr_t *numerator = NULL;
    expr_t *value = NULL;
    expr_t *scaled_invariant_raw = NULL;
    expr_t *scaled_invariant = NULL;
    expr_t *reciprocal_invariant = NULL;
    expr_t *current_reciprocal = NULL;
    const expr_t *outer_numerator = NULL;
    const expr_t *outer_denominator = NULL;
    const expr_t *inner_numerator = NULL;
    const expr_t *inner_denominator = NULL;

    if (!boundary_invariant || !parameter || !current_invariant)
        return NULL;
    parameter_squared = expr_pow_long(parameter, 2L);
    if (expr_match_div_expr(boundary_invariant, &outer_numerator, &outer_denominator) &&
        expr_struct_eq(outer_denominator, parameter) &&
        expr_match_div_expr(outer_numerator, &inner_numerator, &inner_denominator) &&
        expr_struct_eq(inner_denominator, parameter) && !de_expr_uses(inner_numerator, parameter)) {
        value = expr_div_simplify_owned(expr_clone(inner_numerator), expr_clone(current_invariant));
        goto cleanup;
    }
    outer_numerator = NULL;
    outer_denominator = NULL;
    if (parameter_squared && expr_match_div_expr(boundary_invariant, &outer_numerator, &outer_denominator) &&
        !de_expr_uses(outer_numerator, parameter) &&
        de_pde_affine_parameter_components(outer_denominator, parameter_squared, &coefficient, &constant) &&
        !expr_is_exact_zero(coefficient)) {
        current_reciprocal = expr_div_simplify_owned(expr_clone(outer_numerator), expr_clone(current_invariant));
        numerator = current_reciprocal ? expr_sub_simplify_owned(current_reciprocal, constant) : NULL;
        current_reciprocal = NULL;
        constant = NULL;
        value = numerator ? expr_div_simplify_owned(numerator, coefficient) : NULL;
        numerator = NULL;
        coefficient = NULL;
        goto cleanup;
    }
    expr_free(constant);
    expr_free(coefficient);
    constant = NULL;
    coefficient = NULL;
    scaled_invariant_raw = parameter_squared ? expr_mul(boundary_invariant, parameter_squared) : NULL;
    scaled_invariant = scaled_invariant_raw ? expr_simplify(scaled_invariant_raw) : NULL;
    if (scaled_invariant && !de_expr_uses(scaled_invariant, parameter) && !expr_is_exact_zero(scaled_invariant)) {
        value = expr_div_simplify_owned(expr_clone(scaled_invariant), expr_clone(current_invariant));
        goto cleanup;
    }
    if (parameter_squared &&
        de_pde_affine_parameter_components(boundary_invariant, parameter_squared, &coefficient, &constant) &&
        !expr_is_exact_zero(coefficient)) {
        numerator = expr_sub_simplify_owned(expr_clone(current_invariant), constant);
        constant = NULL;
        value = numerator ? expr_div_simplify_owned(numerator, coefficient) : NULL;
        numerator = NULL;
        coefficient = NULL;
        goto cleanup;
    }
    expr_free(constant);
    expr_free(coefficient);
    constant = NULL;
    coefficient = NULL;

    reciprocal_invariant = expr_div_simplify_owned(expr_const_one(), expr_clone(boundary_invariant));
    if (reciprocal_invariant && parameter_squared &&
        de_pde_affine_parameter_components(reciprocal_invariant, parameter_squared, &coefficient, &constant) &&
        !expr_is_exact_zero(coefficient)) {
        current_reciprocal = expr_div_simplify_owned(expr_const_one(), expr_clone(current_invariant));
        numerator = current_reciprocal ? expr_sub_simplify_owned(current_reciprocal, constant) : NULL;
        current_reciprocal = NULL;
        constant = NULL;
        value = numerator ? expr_div_simplify_owned(numerator, coefficient) : NULL;
        numerator = NULL;
        coefficient = NULL;
        goto cleanup;
    }
    expr_free(constant);
    expr_free(coefficient);
    constant = NULL;
    coefficient = NULL;

    reciprocal_square =
        parameter_squared ? expr_div_simplify_owned(expr_const_one(), expr_clone(parameter_squared)) : NULL;
    if (!reciprocal_square ||
        !de_pde_affine_parameter_components(boundary_invariant, reciprocal_square, &coefficient, &constant) ||
        expr_is_exact_zero(coefficient))
        goto cleanup;
    numerator = expr_sub_simplify_owned(expr_clone(current_invariant), constant);
    constant = NULL;
    value = numerator ? expr_div_simplify_owned(coefficient, numerator) : NULL;
    coefficient = NULL;
    numerator = NULL;

cleanup:
    expr_free(current_reciprocal);
    expr_free(reciprocal_invariant);
    expr_free(scaled_invariant);
    expr_free(scaled_invariant_raw);
    expr_free(numerator);
    expr_free(constant);
    expr_free(coefficient);
    expr_free(reciprocal_square);
    expr_free(parameter_squared);
    return value;
}

static equation_t *de_pde_dependent_square_boundary_solution(const diffequ_t *de, const expr_t *x, const expr_t *y,
                                                             const expr_t *dependent, const expr_t *invariant,
                                                             const expr_t *particular)
{
    const expr_t *coordinates[2] = {x, y};

    if (!de || !invariant || !particular)
        return NULL;
    for (size_t condition_index = 0u; condition_index < de->condition_count; ++condition_index) {
        const equation_t *condition = de->conditions[condition_index];
        const expr_t *left = condition ? equ_lhs(condition) : NULL;
        const expr_t *value = condition ? equ_rhs(condition) : NULL;
        const expr_t *point_x;
        const expr_t *point_y;

        if (!left || !value || !expr_struct_eq(left, dependent) || de->condition_point_counts[condition_index] != 2u)
            continue;
        point_x = de->condition_points[condition_index][0];
        point_y = de->condition_points[condition_index][1];

        for (size_t parameter_index = 0u; parameter_index < 2u; ++parameter_index) {
            const expr_t *parameter = coordinates[parameter_index];
            const expr_t *other = coordinates[1u - parameter_index];
            expr_t *dummy = expr_new_named_var(NUM_NAN, "τ");
            expr_t *curve_x = dummy ? expr_substitute(point_x, parameter, dummy) : NULL;
            expr_t *curve_y = dummy ? expr_substitute(point_y, parameter, dummy) : NULL;
            expr_t *curve_value = dummy ? expr_substitute(value, parameter, dummy) : NULL;
            expr_t *boundary_invariant = NULL;
            expr_t *parameter_value = NULL;
            expr_t *parameter_squared = NULL;
            expr_t *parameter_squared_value = NULL;
            expr_t *boundary_particular_raw = NULL;
            expr_t *boundary_particular_curve = NULL;
            expr_t *boundary_particular = NULL;
            expr_t *boundary_value = NULL;
            expr_t *boundary_value_squared_curve = NULL;
            expr_t *boundary_value_squared = NULL;
            expr_t *arbitrary_value = NULL;
            expr_t *squared_right = NULL;
            expr_t *root_raw = NULL;
            expr_t *right = NULL;
            equation_t *solution = NULL;

            if (!curve_x || !curve_y || !curve_value || de_expr_uses(curve_x, other) || de_expr_uses(curve_y, other) ||
                de_expr_uses(curve_value, other) || (!de_expr_uses(curve_x, dummy) && !de_expr_uses(curve_y, dummy)))
                goto candidate_cleanup;
            boundary_invariant = de_pde_substitute_characteristic_curve(invariant, x, y, curve_x, curve_y);
            boundary_particular_raw = de_pde_substitute_characteristic_curve(particular, x, y, curve_x, curve_y);
            boundary_particular_curve = boundary_particular_raw ? expr_simplify(boundary_particular_raw) : NULL;
            parameter_squared = expr_pow_long(dummy, 2L);
            parameter_squared_value = de_pde_boundary_parameter_squared_value(boundary_invariant, dummy, invariant);
            boundary_value_squared_curve = expr_pow_long(curve_value, 2L);
            if (parameter_squared_value && parameter_squared && boundary_value_squared_curve) {
                boundary_value_squared =
                    expr_struct_eq(boundary_value_squared_curve, parameter_squared)
                        ? expr_clone(parameter_squared_value)
                        : expr_substitute(boundary_value_squared_curve, parameter_squared, parameter_squared_value);
                boundary_particular =
                    boundary_particular_curve
                        ? (!de_expr_uses(boundary_particular_curve, dummy)
                               ? expr_clone(boundary_particular_curve)
                               : expr_substitute(boundary_particular_curve, parameter_squared, parameter_squared_value))
                        : NULL;
                if ((boundary_value_squared && de_expr_uses(boundary_value_squared, dummy)) ||
                    (boundary_particular && de_expr_uses(boundary_particular, dummy))) {
                    expr_free(boundary_value_squared);
                    expr_free(boundary_particular);
                    boundary_value_squared = NULL;
                    boundary_particular = NULL;
                }
            }
            if (!boundary_value_squared || !boundary_particular) {
                parameter_value = de_pde_boundary_parameter_value(boundary_invariant, dummy, invariant);
                if (!parameter_value)
                    goto candidate_cleanup;
                boundary_particular = boundary_particular_curve
                                          ? expr_substitute(boundary_particular_curve, dummy, parameter_value)
                                          : NULL;
                boundary_value = expr_substitute(curve_value, dummy, parameter_value);
                boundary_value_squared = boundary_value ? expr_pow_long(boundary_value, 2L) : NULL;
            }
            arbitrary_value = boundary_value_squared && boundary_particular
                                  ? expr_sub_simplify_owned(boundary_value_squared, boundary_particular)
                                  : NULL;
            boundary_value_squared = NULL;
            boundary_particular = NULL;
            squared_right = arbitrary_value ? expr_add_simplify_owned(expr_clone(particular), arbitrary_value) : NULL;
            arbitrary_value = NULL;
            root_raw = squared_right ? expr_sqrt(squared_right) : NULL;
            right = root_raw ? expr_simplify(root_raw) : NULL;
            {
                const expr_t *positive_boundary_value = NULL;

                if (right && expr_match_neg_expr(curve_value, &positive_boundary_value))
                    right = expr_negate_owned(right);
            }
            solution = right ? de_pde_solution_equation(dependent, right) : NULL;

        candidate_cleanup:
            expr_free(right);
            expr_free(root_raw);
            expr_free(squared_right);
            expr_free(arbitrary_value);
            expr_free(boundary_value_squared);
            expr_free(boundary_value_squared_curve);
            expr_free(boundary_value);
            expr_free(boundary_particular);
            expr_free(boundary_particular_curve);
            expr_free(boundary_particular_raw);
            expr_free(parameter_squared_value);
            expr_free(parameter_squared);
            expr_free(parameter_value);
            expr_free(boundary_invariant);
            expr_free(curve_value);
            expr_free(curve_y);
            expr_free(curve_x);
            expr_free(dummy);
            if (solution)
                return solution;
        }
    }
    return NULL;
}

static equation_t *de_pde_quadratic_boundary_solution(const diffequ_t *de, const expr_t *x, const expr_t *y,
                                                      const expr_t *dependent, const expr_t *invariant,
                                                      const expr_t *scaled_potential)
{
    const expr_t *coordinates[2] = {x, y};

    if (!de || !invariant || !scaled_potential)
        return NULL;
    for (size_t condition_index = 0u; condition_index < de->condition_count; ++condition_index) {
        const equation_t *condition = de->conditions[condition_index];
        const expr_t *left = condition ? equ_lhs(condition) : NULL;
        const expr_t *value = condition ? equ_rhs(condition) : NULL;
        const expr_t *point_x;
        const expr_t *point_y;

        if (!left || !value || !expr_struct_eq(left, dependent) || de->condition_point_counts[condition_index] != 2u)
            continue;
        point_x = de->condition_points[condition_index][0];
        point_y = de->condition_points[condition_index][1];
        if (expr_is_exact_zero(value)) {
            expr_t *zero = expr_const_zero();
            equation_t *solution = zero ? de_pde_solution_equation(dependent, zero) : NULL;

            expr_free(zero);
            return solution;
        }

        for (size_t parameter_index = 0u; parameter_index < 2u; ++parameter_index) {
            const expr_t *parameter = coordinates[parameter_index];
            const expr_t *other = coordinates[1u - parameter_index];
            expr_t *dummy = expr_new_named_var(NUM_NAN, "τ");
            expr_t *curve_x = dummy ? expr_substitute(point_x, parameter, dummy) : NULL;
            expr_t *curve_y = dummy ? expr_substitute(point_y, parameter, dummy) : NULL;
            expr_t *curve_value = dummy ? expr_substitute(value, parameter, dummy) : NULL;
            expr_t *boundary_invariant = NULL;
            expr_t *parameter_value = NULL;
            expr_t *boundary_potential_curve = NULL;
            expr_t *boundary_potential = NULL;
            expr_t *boundary_value = NULL;
            expr_t *reciprocal_value = NULL;
            expr_t *arbitrary_value = NULL;
            expr_t *denominator = NULL;
            expr_t *right = NULL;
            equation_t *solution = NULL;

            if (!curve_x || !curve_y || !curve_value || de_expr_uses(curve_x, other) || de_expr_uses(curve_y, other) ||
                de_expr_uses(curve_value, other) || (!de_expr_uses(curve_x, dummy) && !de_expr_uses(curve_y, dummy)))
                goto candidate_cleanup;
            boundary_invariant = de_pde_substitute_characteristic_curve(invariant, x, y, curve_x, curve_y);
            parameter_value = de_pde_boundary_parameter_value(boundary_invariant, dummy, invariant);
            if (!parameter_value)
                goto candidate_cleanup;
            boundary_potential_curve = de_pde_substitute_characteristic_curve(scaled_potential, x, y, curve_x, curve_y);
            boundary_potential =
                boundary_potential_curve ? expr_substitute(boundary_potential_curve, dummy, parameter_value) : NULL;
            boundary_value = expr_substitute(curve_value, dummy, parameter_value);
            reciprocal_value = boundary_value ? expr_div_simplify_owned(expr_const_one(), boundary_value) : NULL;
            boundary_value = NULL;
            arbitrary_value = reciprocal_value && boundary_potential
                                  ? expr_sub_simplify_owned(reciprocal_value, boundary_potential)
                                  : NULL;
            reciprocal_value = NULL;
            boundary_potential = NULL;
            denominator =
                arbitrary_value ? expr_add_simplify_owned(expr_clone(scaled_potential), arbitrary_value) : NULL;
            arbitrary_value = NULL;
            right = denominator ? expr_div_simplify_owned(expr_const_one(), denominator) : NULL;
            denominator = NULL;
            solution = right ? de_pde_solution_equation(dependent, right) : NULL;

        candidate_cleanup:
            expr_free(right);
            expr_free(denominator);
            expr_free(arbitrary_value);
            expr_free(reciprocal_value);
            expr_free(boundary_value);
            expr_free(boundary_potential);
            expr_free(boundary_potential_curve);
            expr_free(parameter_value);
            expr_free(boundary_invariant);
            expr_free(curve_value);
            expr_free(curve_y);
            expr_free(curve_x);
            expr_free(dummy);
            if (solution)
                return solution;
        }
    }
    return NULL;
}

static expr_t *de_pde_boundary_curve_value(const expr_t *curve_expression, const expr_t *parameter,
                                           const expr_t *parameter_value, const expr_t *parameter_squared,
                                           const expr_t *parameter_squared_value)
{
    expr_t *value;

    if (!curve_expression || !parameter)
        return NULL;
    if (!de_expr_uses(curve_expression, parameter))
        return expr_clone(curve_expression);
    if (parameter_value)
        return expr_substitute(curve_expression, parameter, parameter_value);
    if (!parameter_squared || !parameter_squared_value)
        return NULL;
    if (expr_struct_eq(curve_expression, parameter_squared))
        return expr_clone(parameter_squared_value);
    value = expr_substitute(curve_expression, parameter_squared, parameter_squared_value);
    if (value && de_expr_uses(value, parameter)) {
        expr_free(value);
        value = NULL;
    }
    return value;
}

static equation_t *de_pde_parametric_boundary_solution(const diffequ_t *de, const expr_t *x, const expr_t *y,
                                                       const expr_t *dependent, const expr_t *invariant,
                                                       const expr_t *factor, const expr_t *particular)
{
    const expr_t *coordinates[2] = {x, y};

    if (!de || !invariant || !factor)
        return NULL;
    for (size_t condition_index = 0u; condition_index < de->condition_count; ++condition_index) {
        const equation_t *condition = de->conditions[condition_index];
        const expr_t *left = condition ? equ_lhs(condition) : NULL;
        const expr_t *value = condition ? equ_rhs(condition) : NULL;
        const expr_t *point_x;
        const expr_t *point_y;

        if (!left || !value || !expr_struct_eq(left, dependent) || de->condition_point_counts[condition_index] != 2u)
            continue;
        point_x = de->condition_points[condition_index][0];
        point_y = de->condition_points[condition_index][1];

        for (size_t parameter_index = 0u; parameter_index < 2u; ++parameter_index) {
            const expr_t *parameter = coordinates[parameter_index];
            const expr_t *other = coordinates[1u - parameter_index];
            expr_t *dummy = expr_new_named_var(NUM_NAN, "τ");
            expr_t *curve_x = dummy ? expr_substitute(point_x, parameter, dummy) : NULL;
            expr_t *curve_y = dummy ? expr_substitute(point_y, parameter, dummy) : NULL;
            expr_t *curve_value = dummy ? expr_substitute(value, parameter, dummy) : NULL;
            expr_t *boundary_invariant = NULL;
            expr_t *parameter_value = NULL;
            expr_t *parameter_squared = NULL;
            expr_t *parameter_squared_value = NULL;
            expr_t *parameter_root_raw = NULL;
            expr_t *boundary_invariant_raw = NULL;
            expr_t *boundary_factor_curve = NULL;
            expr_t *boundary_factor = NULL;
            expr_t *boundary_particular_curve = NULL;
            expr_t *boundary_particular = NULL;
            expr_t *boundary_value = NULL;
            expr_t *factor_ratio = NULL;
            expr_t *boundary_delta = NULL;
            expr_t *arbitrary_value = NULL;
            expr_t *homogeneous = NULL;
            expr_t *right = NULL;
            equation_t *solution = NULL;

            if (!curve_x || !curve_y || !curve_value || de_expr_uses(curve_x, other) || de_expr_uses(curve_y, other) ||
                de_expr_uses(curve_value, other) || (!de_expr_uses(curve_x, dummy) && !de_expr_uses(curve_y, dummy)))
                goto candidate_cleanup;
            boundary_invariant_raw = de_pde_substitute_characteristic_curve(invariant, x, y, curve_x, curve_y);
            boundary_invariant = boundary_invariant_raw ? expr_simplify(boundary_invariant_raw) : NULL;
            parameter_value = de_pde_boundary_parameter_value(boundary_invariant, dummy, invariant);
            if (!parameter_value) {
                parameter_squared = expr_pow_long(dummy, 2L);
                parameter_squared_value = de_pde_boundary_parameter_squared_value(boundary_invariant, dummy, invariant);
                parameter_root_raw = parameter_squared_value ? expr_sqrt(parameter_squared_value) : NULL;
                parameter_value = parameter_root_raw ? expr_simplify(parameter_root_raw) : NULL;
            }
            if (!parameter_value && !parameter_squared_value)
                goto candidate_cleanup;
            boundary_factor_curve = de_pde_substitute_characteristic_curve(factor, x, y, curve_x, curve_y);
            boundary_factor = de_pde_boundary_curve_value(boundary_factor_curve, dummy, parameter_value,
                                                          parameter_squared, parameter_squared_value);
            boundary_particular_curve = particular
                                            ? de_pde_substitute_characteristic_curve(particular, x, y, curve_x, curve_y)
                                            : expr_const_zero();
            boundary_particular = de_pde_boundary_curve_value(boundary_particular_curve, dummy, parameter_value,
                                                              parameter_squared, parameter_squared_value);
            boundary_value = de_pde_boundary_curve_value(curve_value, dummy, parameter_value, parameter_squared,
                                                         parameter_squared_value);
            if (!particular) {
                factor_ratio =
                    factor && boundary_factor ? expr_div_simplify_owned(expr_clone(factor), boundary_factor) : NULL;
                boundary_factor = NULL;
                right = factor_ratio && boundary_value ? expr_mul_simplify_owned(factor_ratio, boundary_value) : NULL;
                factor_ratio = NULL;
                boundary_value = NULL;
                solution = right ? de_pde_solution_equation(dependent, right) : NULL;
                goto candidate_cleanup;
            }
            boundary_delta = boundary_value && boundary_particular
                                 ? expr_sub_simplify_owned(boundary_value, boundary_particular)
                                 : NULL;
            boundary_value = NULL;
            boundary_particular = NULL;
            arbitrary_value =
                boundary_delta && boundary_factor ? expr_div_simplify_owned(boundary_delta, boundary_factor) : NULL;
            boundary_delta = NULL;
            boundary_factor = NULL;
            homogeneous =
                factor && arbitrary_value ? expr_mul_simplify_owned(expr_clone(factor), arbitrary_value) : NULL;
            arbitrary_value = NULL;
            right =
                homogeneous && particular ? expr_add_simplify_owned(homogeneous, expr_clone(particular)) : homogeneous;
            homogeneous = NULL;
            solution = right ? de_pde_solution_equation(dependent, right) : NULL;

        candidate_cleanup:
            expr_free(right);
            expr_free(homogeneous);
            expr_free(factor_ratio);
            expr_free(arbitrary_value);
            expr_free(boundary_delta);
            expr_free(boundary_value);
            expr_free(boundary_particular);
            expr_free(boundary_particular_curve);
            expr_free(boundary_factor);
            expr_free(boundary_factor_curve);
            expr_free(parameter_squared_value);
            expr_free(parameter_squared);
            expr_free(parameter_root_raw);
            expr_free(boundary_invariant_raw);
            expr_free(parameter_value);
            expr_free(boundary_invariant);
            expr_free(curve_value);
            expr_free(curve_y);
            expr_free(curve_x);
            expr_free(dummy);
            if (solution)
                return solution;
        }
    }
    return NULL;
}

static equation_t *de_pde_variable_linear_solution(const diffequ_t *de, const expr_t *x, const expr_t *y,
                                                   const expr_t *dependent, const expr_t *x_coefficient,
                                                   const expr_t *y_coefficient, const expr_t *reaction_coefficient,
                                                   const expr_t *forcing)
{
    expr_t *invariant = de_pde_linear_characteristic_invariant(x, y, dependent, x_coefficient, y_coefficient);
    expr_t *arbitrary = invariant ? expr_new_arbitrary_function("F", invariant) : NULL;
    expr_t *exponent = NULL;
    expr_t *factor = NULL;
    expr_t *homogeneous = NULL;
    expr_t *particular = NULL;
    expr_t *right = NULL;
    equation_t *solution = NULL;

    if (!arbitrary)
        goto cleanup;
    if (expr_is_exact_zero(reaction_coefficient)) {
        factor = expr_const_one();
        homogeneous = expr_clone(arbitrary);
    } else {
        exponent = de_pde_characteristic_exponent(x, y, dependent, x_coefficient, y_coefficient, reaction_coefficient);
        factor = exponent ? de_pde_characteristic_factor(exponent) : NULL;
        homogeneous = factor ? de_pde_apply_characteristic_factor(factor, arbitrary) : NULL;
    }
    if (!homogeneous)
        goto cleanup;
    if (!expr_is_exact_zero(forcing)) {
        particular = de_pde_characteristic_particular(x, y, dependent, x_coefficient, y_coefficient,
                                                      reaction_coefficient, forcing);
        if (!particular)
            goto cleanup;
    }
    if (de && de->condition_count > 0u) {
        solution = de_pde_parametric_boundary_solution(de, x, y, dependent, invariant, factor, particular);
        goto cleanup;
    }
    if (expr_is_exact_zero(forcing)) {
        right = expr_clone(homogeneous);
    } else {
        right = particular ? expr_add_simplify_owned(expr_clone(homogeneous), expr_clone(particular)) : NULL;
    }
    solution = right ? de_pde_solution_equation(dependent, right) : NULL;

cleanup:
    expr_free(right);
    expr_free(particular);
    expr_free(homogeneous);
    expr_free(factor);
    expr_free(exponent);
    expr_free(arbitrary);
    expr_free(invariant);
    return solution;
}

static bool de_pde_is_first_integral(const expr_t *x, const expr_t *y, const expr_t *dependent,
                                     const expr_t *x_coefficient, const expr_t *y_coefficient,
                                     const expr_t *dependent_coefficient, const expr_t *candidate)
{
    expr_t *candidate_x = expr_create_deriv(candidate, x);
    expr_t *candidate_y = expr_create_deriv(candidate, y);
    expr_t *candidate_dependent = expr_create_deriv(candidate, dependent);
    expr_t *along_x = candidate_x ? expr_mul(x_coefficient, candidate_x) : NULL;
    expr_t *along_y = candidate_y ? expr_mul(y_coefficient, candidate_y) : NULL;
    expr_t *along_dependent = candidate_dependent ? expr_mul(dependent_coefficient, candidate_dependent) : NULL;
    expr_t *xy_sum = along_x && along_y ? expr_add(along_x, along_y) : NULL;
    expr_t *directional_raw = xy_sum && along_dependent ? expr_add(xy_sum, along_dependent) : NULL;
    expr_t *directional = directional_raw ? expr_simplify(directional_raw) : NULL;
    bool is_first_integral = directional && expr_is_exact_zero(directional);

    expr_free(directional);
    expr_free(directional_raw);
    expr_free(xy_sum);
    expr_free(along_dependent);
    expr_free(along_y);
    expr_free(along_x);
    expr_free(candidate_dependent);
    expr_free(candidate_y);
    expr_free(candidate_x);
    return is_first_integral;
}

static bool de_pde_is_scaled_cyclic_field(const expr_t *x, const expr_t *y, const expr_t *dependent,
                                          const expr_t *x_coefficient, const expr_t *y_coefficient,
                                          const expr_t *dependent_coefficient, long power)
{
    expr_t *x_power = expr_pow_long(x, power);
    expr_t *y_power = expr_pow_long(y, power);
    expr_t *dependent_power = expr_pow_long(dependent, power);
    expr_t *y_minus_dependent =
        y_power && dependent_power ? expr_sub_simplify_owned(expr_clone(y_power), expr_clone(dependent_power)) : NULL;
    expr_t *dependent_minus_x =
        dependent_power && x_power ? expr_sub_simplify_owned(expr_clone(dependent_power), expr_clone(x_power)) : NULL;
    expr_t *x_minus_y = x_power && y_power ? expr_sub_simplify_owned(expr_clone(x_power), expr_clone(y_power)) : NULL;
    expr_t *base_x = y_minus_dependent ? expr_mul_simplify_owned(expr_clone(x), y_minus_dependent) : NULL;
    expr_t *base_y = dependent_minus_x ? expr_mul_simplify_owned(expr_clone(y), dependent_minus_x) : NULL;
    expr_t *base_dependent = x_minus_y ? expr_mul_simplify_owned(expr_clone(dependent), x_minus_y) : NULL;
    expr_t *scale_x_raw = base_x ? expr_div(x_coefficient, base_x) : NULL;
    expr_t *scale_y_raw = base_y ? expr_div(y_coefficient, base_y) : NULL;
    expr_t *scale_dependent_raw = base_dependent ? expr_div(dependent_coefficient, base_dependent) : NULL;
    expr_t *scale_x = scale_x_raw ? expr_simplify(scale_x_raw) : NULL;
    expr_t *scale_y = scale_y_raw ? expr_simplify(scale_y_raw) : NULL;
    expr_t *scale_dependent = scale_dependent_raw ? expr_simplify(scale_dependent_raw) : NULL;
    bool matches = scale_x && !expr_is_exact_zero(scale_x) && de_pde_same_symbolic_form(scale_x, scale_y) &&
                   de_pde_same_symbolic_form(scale_x, scale_dependent);

    expr_free(scale_dependent);
    expr_free(scale_y);
    expr_free(scale_x);
    expr_free(scale_dependent_raw);
    expr_free(scale_y_raw);
    expr_free(scale_x_raw);
    expr_free(base_dependent);
    expr_free(base_y);
    expr_free(base_x);
    expr_free(dependent_power);
    expr_free(y_power);
    expr_free(x_power);
    return matches;
}

static void de_pde_collect_cyclic_power_candidates(const expr_t *expr, long *powers, size_t *power_count,
                                                   size_t power_capacity)
{
    const expr_t *base = NULL;
    number_t exponent = num_new();
    expr_t *exponent_expr = NULL;
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    long power = 0L;
    bool duplicate = false;

    if (!expr || !powers || !power_count || *power_count >= power_capacity)
        goto cleanup;
    if (expr_match_pow_const(expr, &base, &exponent)) {
        exponent_expr = expr_new_const(exponent);
        if (exponent_expr && de_pde_exact_long(exponent_expr, &power) && power > 0L) {
            for (size_t i = 0u; i < *power_count; ++i) {
                if (powers[i] == power) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate)
                powers[(*power_count)++] = power;
        }
    }
    if (expr_child_exprs(expr, &left, &right)) {
        de_pde_collect_cyclic_power_candidates(left, powers, power_count, power_capacity);
        de_pde_collect_cyclic_power_candidates(right, powers, power_count, power_capacity);
    }

cleanup:
    expr_free(exponent_expr);
    num_destroy(&exponent);
}

static equation_t *de_pde_cyclic_first_integral_solution(const expr_t *x, const expr_t *y, const expr_t *dependent,
                                                         const expr_t *x_coefficient, const expr_t *y_coefficient,
                                                         const expr_t *remainder)
{
    expr_t *dependent_coefficient = expr_negate_owned(expr_clone(remainder));
    expr_t *xy_product = expr_mul_simplify_owned(expr_clone(x), expr_clone(y));
    expr_t *product = xy_product ? expr_mul_simplify_owned(xy_product, expr_clone(dependent)) : NULL;
    equation_t *solution = NULL;
    long powers[16] = {1L};
    size_t power_count = 1u;

    de_pde_collect_cyclic_power_candidates(x_coefficient, powers, &power_count, 16u);
    de_pde_collect_cyclic_power_candidates(y_coefficient, powers, &power_count, 16u);
    de_pde_collect_cyclic_power_candidates(dependent_coefficient, powers, &power_count, 16u);

    for (size_t i = 0u; dependent_coefficient && product && i < power_count && !solution; ++i) {
        expr_t *x_power = expr_pow_long(x, powers[i]);
        expr_t *y_power = expr_pow_long(y, powers[i]);
        expr_t *dependent_power = expr_pow_long(dependent, powers[i]);
        expr_t *xy_sum = x_power && y_power ? expr_add_simplify_owned(x_power, y_power) : NULL;
        expr_t *sum = xy_sum && dependent_power ? expr_add_simplify_owned(xy_sum, dependent_power) : NULL;
        expr_t *arguments[2] = {sum, product};
        bool verified =
            sum &&
            ((de_pde_is_first_integral(x, y, dependent, x_coefficient, y_coefficient, dependent_coefficient, sum) &&
              de_pde_is_first_integral(x, y, dependent, x_coefficient, y_coefficient, dependent_coefficient,
                                       product)) ||
             de_pde_is_scaled_cyclic_field(x, y, dependent, x_coefficient, y_coefficient, dependent_coefficient,
                                           powers[i]));

        if (verified) {
            expr_t *arbitrary_relation = expr_new_arbitrary_function_n("F", 2u, arguments);
            expr_t *zero = expr_const_zero();

            solution = arbitrary_relation && zero ? equ_new(arbitrary_relation, zero) : NULL;
            expr_free(zero);
            expr_free(arbitrary_relation);
        }
        expr_free(sum);
    }

    expr_free(product);
    expr_free(dependent_coefficient);
    return solution;
}

static bool de_pde_extract_dependent_power(const expr_t *expression, const expr_t *dependent, long *power_out,
                                           expr_t **coefficient_out)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    const expr_t *base = NULL;
    number_t exponent = num_new();
    expr_t *exponent_expr = NULL;
    expr_t *coefficient = NULL;
    long power = 0L;
    bool matches = false;

    if (!expression || !dependent || !power_out || !coefficient_out)
        goto cleanup;
    if (expr_match_pow_const(expression, &base, &exponent) && expr_struct_eq(base, dependent)) {
        exponent_expr = expr_new_const(exponent);
        if (!exponent_expr || !de_pde_exact_long(exponent_expr, &power) || power == 0L)
            goto cleanup;
        coefficient = expr_const_one();
    } else if (expr_match_neg_expr(expression, &left)) {
        if (!de_pde_extract_dependent_power(left, dependent, &power, &coefficient))
            goto cleanup;
        coefficient = expr_negate_owned(coefficient);
    } else if (expr_match_mul_expr(expression, &left, &right)) {
        const expr_t *power_factor = NULL;
        const expr_t *other_factor = NULL;

        if (de_expr_uses(left, dependent) && !de_expr_uses(right, dependent)) {
            power_factor = left;
            other_factor = right;
        } else if (de_expr_uses(right, dependent) && !de_expr_uses(left, dependent)) {
            power_factor = right;
            other_factor = left;
        } else {
            goto cleanup;
        }
        if (!de_pde_extract_dependent_power(power_factor, dependent, &power, &coefficient))
            goto cleanup;
        coefficient = expr_mul_simplify_owned(coefficient, expr_clone(other_factor));
    }
    if (!coefficient || de_expr_uses(coefficient, dependent))
        goto cleanup;
    *power_out = power;
    *coefficient_out = coefficient;
    coefficient = NULL;
    matches = true;

cleanup:
    expr_free(coefficient);
    expr_free(exponent_expr);
    num_destroy(&exponent);
    return matches;
}

static bool de_pde_power_characteristic_solutions(const expr_t *x, const expr_t *y, const expr_t *dependent,
                                                  const expr_t *x_coefficient, const expr_t *y_coefficient,
                                                  const expr_t *remainder, equation_t **solutions_out,
                                                  size_t *solution_count_out)
{
    long power = 0L;
    expr_t *power_coefficient = NULL;
    expr_t *invariant = NULL;
    expr_t *arbitrary = NULL;
    expr_t *power_minus_one = NULL;
    expr_t *transformed_forcing = NULL;
    expr_t *zero = NULL;
    expr_t *particular = NULL;
    expr_t *transformed = NULL;
    expr_t *one_minus_power = NULL;
    expr_t *inverse_exponent = NULL;
    expr_t *right_raw = NULL;
    expr_t *right = NULL;
    expr_t *singular = NULL;
    bool solved = false;

    if (!de_pde_extract_dependent_power(remainder, dependent, &power, &power_coefficient) || power == 0L || power == 1L)
        goto cleanup;
    invariant = de_pde_linear_characteristic_invariant(x, y, dependent, x_coefficient, y_coefficient);
    arbitrary = invariant ? expr_new_arbitrary_function("F", invariant) : NULL;
    power_minus_one = expr_const_long(power - 1L);
    transformed_forcing =
        power_minus_one ? expr_negate_owned(expr_mul_simplify_owned(power_minus_one, expr_clone(power_coefficient)))
                        : NULL;
    power_minus_one = NULL;
    zero = expr_const_zero();
    particular =
        arbitrary && transformed_forcing && zero
            ? de_pde_characteristic_particular(x, y, dependent, x_coefficient, y_coefficient, zero, transformed_forcing)
            : NULL;
    transformed = arbitrary && particular ? expr_add_simplify_owned(expr_clone(arbitrary), particular) : NULL;
    particular = NULL;
    one_minus_power = expr_const_long(1L - power);
    inverse_exponent = one_minus_power ? expr_div_simplify_owned(expr_const_one(), one_minus_power) : NULL;
    one_minus_power = NULL;
    right_raw = power == 2L && transformed
                    ? expr_div_simplify_owned(expr_const_one(), expr_clone(transformed))
                    : (transformed && inverse_exponent ? expr_pow_xp(transformed, inverse_exponent) : NULL);
    right = right_raw ? expr_simplify(right_raw) : NULL;
    if (!right)
        goto cleanup;
    solutions_out[0] = de_pde_solution_equation(dependent, right);
    if (!solutions_out[0])
        goto cleanup;
    *solution_count_out = 1u;
    if (power > 1L) {
        singular = expr_const_zero();
        solutions_out[1] = singular ? de_pde_solution_equation(dependent, singular) : NULL;
        if (!solutions_out[1])
            goto cleanup;
        *solution_count_out = 2u;
    }
    solved = true;

cleanup:
    if (!solved) {
        equ_free(solutions_out[0]);
        equ_free(solutions_out[1]);
        solutions_out[0] = NULL;
        solutions_out[1] = NULL;
        *solution_count_out = 0u;
    }
    expr_free(singular);
    expr_free(right);
    expr_free(right_raw);
    expr_free(inverse_exponent);
    expr_free(one_minus_power);
    expr_free(transformed);
    expr_free(particular);
    expr_free(zero);
    expr_free(transformed_forcing);
    expr_free(power_minus_one);
    expr_free(arbitrary);
    expr_free(invariant);
    expr_free(power_coefficient);
    return solved;
}

de_attempt_t de_pde_attempt_characteristics(const diffequ_t *de, const expr_t *residual, equation_t **solutions_out,
                                            size_t *solution_count_out)
{
    const expr_t *x;
    const expr_t *y;
    const expr_t *dependent = NULL;
    const expr_t *dx = NULL;
    const expr_t *dy = NULL;
    expr_t *x_coefficient = NULL;
    expr_t *without_dx = NULL;
    expr_t *y_coefficient = NULL;
    expr_t *remainder = NULL;
    expr_t *reaction_coefficient = NULL;
    expr_t *forcing = NULL;
    de_attempt_t attempt = DE_ATTEMPT_NOT_MATCHED;

    *solution_count_out = 0u;
    if (!de || !residual || !solutions_out || de->independent_count != 2u)
        return DE_ATTEMPT_NOT_MATCHED;
    x = de->independent_vars[0];
    y = de->independent_vars[1];
    if (!de_pde_find_first_derivatives(residual, x, y, &dependent, &dx, &dy) || !dependent || !dx || !dy ||
        !de_linear_decompose(residual, dx, &x_coefficient, &without_dx) ||
        !de_linear_decompose(without_dx, dy, &y_coefficient, &remainder))
        goto cleanup;

    if (!de_expr_uses(x_coefficient, dependent) && !de_expr_uses(y_coefficient, dependent) &&
        de_linear_decompose(remainder, dependent, &reaction_coefficient, &forcing)) {
        solutions_out[0] = de_pde_variable_linear_solution(de, x, y, dependent, x_coefficient, y_coefficient,
                                                           reaction_coefficient, forcing);
        if (solutions_out[0]) {
            *solution_count_out = 1u;
            attempt = DE_ATTEMPT_SOLVED;
            goto cleanup;
        }
    }

    if (de_pde_quadratic_characteristic_solutions(de, x, y, dependent, x_coefficient, y_coefficient, remainder,
                                                  solutions_out, solution_count_out)) {
        attempt = DE_ATTEMPT_SOLVED;
        goto cleanup;
    }

    solutions_out[0] = de_pde_cyclic_first_integral_solution(x, y, dependent, x_coefficient, y_coefficient, remainder);
    if (solutions_out[0]) {
        *solution_count_out = 1u;
        attempt = DE_ATTEMPT_SOLVED;
        goto cleanup;
    }

    if (de_pde_dependent_square_solutions(de, x, y, dependent, x_coefficient, y_coefficient, remainder, solutions_out,
                                          solution_count_out)) {
        attempt = DE_ATTEMPT_SOLVED;
        goto cleanup;
    }

    if (de_pde_power_characteristic_solutions(x, y, dependent, x_coefficient, y_coefficient, remainder, solutions_out,
                                              solution_count_out)) {
        attempt = DE_ATTEMPT_SOLVED;
        goto cleanup;
    }

cleanup:
    if (attempt != DE_ATTEMPT_SOLVED) {
        equ_free(solutions_out[0]);
        equ_free(solutions_out[1]);
        solutions_out[0] = NULL;
        solutions_out[1] = NULL;
    }
    expr_free(forcing);
    expr_free(reaction_coefficient);
    expr_free(remainder);
    expr_free(y_coefficient);
    expr_free(without_dx);
    expr_free(x_coefficient);
    return attempt;
}
