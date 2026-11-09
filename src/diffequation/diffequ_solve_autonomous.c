/**
 * @file diffequ_solve_autonomous.c
 * @brief Complete the energy reduction for real autonomous exponential-force ODEs.
 *
 * A constant logarithmic derivative identifies A*exp(B*y). The conserved energy
 * gives hyperbolic, trigonometric and zero-energy real families, or a single local
 * solution for finite real value and slope data. Native domain guards retain poles;
 * semantic notation supplies the derivation without embedding TeX syntax here.
 */
#include <stdlib.h>
#include <string.h>

#define MARS_DIFFEQUATION_SOLVE_INTERNAL_ACCESS
#include "diffequ_solve_internal.h"
#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_notation_internal.h"

static bool finite_real(const expr_t *expression, bool nonzero)
{
    number_t value = expression ? expr_eval(expression) : num_clone(NUM_NAN);
    bool valid = num_is_finite(value) && num_is_real(value) && (!nonzero || !num_is_zero(value));
    num_destroy(&value);
    return valid;
}

/* Symbolic differentiation must prove a constant logarithmic derivative, not just fit sample points. */
static bool exponential_force(const expr_t *residual, const expr_t *x, const expr_t *y,
    const expr_t *second, const expr_t *first, expr_t **force_out, expr_t **slope_out, expr_t **amplitude_out)
{
    expr_t *leading = NULL, *remainder = NULL;
    if (!de_linear_decompose(residual, second, &leading, &remainder))
        return false;
    bool valid = !de_expr_uses(leading, x) && !de_expr_uses(leading, y) &&
                 (!first || !de_expr_contains(leading, first)) && finite_real(leading, true);
    expr_t *negative = valid ? expr_neg(remainder) : NULL;
    expr_t *quotient = negative ? expr_div(negative, leading) : NULL;
    expr_t *force = quotient ? expr_simplify(quotient) : NULL;
    valid = force && !de_expr_uses(force, x) && (!first || !de_expr_contains(force, first));
    expr_t *derivative = valid ? expr_create_deriv(force, y) : NULL;
    expr_t *ratio = derivative ? expr_div(derivative, force) : NULL;
    expr_t *slope = ratio ? expr_simplify(ratio) : NULL;
    valid = slope && !de_expr_uses(slope, y) && !de_expr_uses(slope, x) && finite_real(slope, true);
    expr_t *zero = expr_const_zero();
    expr_t *at_zero = valid ? expr_substitute(force, y, zero) : NULL;
    expr_t *amplitude = at_zero ? expr_simplify(at_zero) : NULL;
    valid = amplitude && finite_real(amplitude, true);
    expr_free(at_zero); expr_free(zero); expr_free(ratio); expr_free(derivative);
    expr_free(quotient); expr_free(negative); expr_free(remainder); expr_free(leading);
    if (!valid) {
        expr_free(amplitude); expr_free(slope); expr_free(force);
        return false;
    }
    *force_out = force; *slope_out = slope; *amplitude_out = amplitude;
    return true;
}

static expr_t *free_constant(const diffequ_t *de, size_t *index)
{
    /* Enumerate fresh labels only; each authored-name membership query uses the binding dictionary. */
    size_t limit = expr_bindings_count(equ_bindings(de->equation)) + *index + 1u;
    while (*index < limit) {
        string_t *name = string_sprintf("C%zu", (*index)++);
        bool unused = name && !expr_bindings_get_text(equ_bindings(de->equation), name);
        expr_t *constant = unused ? expr_new_named_const(NUM_NAN, string_c_str(name)) : NULL;
        string_free(name);
        if (unused)
            return constant;
    }
    return NULL;
}

static expr_t *restrict_real(const expr_t *body, const expr_t *x, const expr_t *centre,
                              const expr_t *frequency, const expr_t *denominator)
{
    expr_t *zero = expr_const_zero(), *real_x = expr_real_parameter(x);
    expr_t *real_centre = centre ? expr_real_parameter(centre) : NULL;
    expr_t *positive = frequency ? expr_real_comparison(frequency, zero, false) : NULL;
    expr_t *simple_denominator = denominator ? expr_simplify(denominator) : NULL;
    expr_t *magnitude = simple_denominator ? expr_abs(simple_denominator) : NULL;
    expr_t *arguments[9] = {(expr_t *)body, real_x, zero};
    size_t count = 3u;
    if (centre) { arguments[count++] = real_centre; arguments[count++] = zero; }
    if (frequency) { arguments[count++] = positive; arguments[count++] = zero; }
    if (denominator) { arguments[count++] = magnitude; arguments[count++] = zero; }
    expr_t *guarded = body ? expr_real_domain_from_args(count, arguments) : NULL;
    expr_t *result = guarded ? expr_simplify(guarded) : NULL;
    expr_free(guarded); expr_free(magnitude); expr_free(simple_denominator); expr_free(positive); expr_free(real_centre);
    expr_free(real_x); expr_free(zero);
    return result;
}

/* The frequency is positive; the three kernels encode positive, negative and zero energy. */
static equation_t *energy_family(const expr_t *x, const expr_t *y, const expr_t *amplitude,
    const expr_t *slope, const expr_t *frequency, const expr_t *centre, expr_t *(*kernel)(const expr_t *))
{
    expr_t *shift = expr_sub(x, centre);
    expr_t *argument = kernel && shift ? expr_mul(frequency, shift) : NULL;
    expr_t *factor = kernel && argument ? kernel(argument) : shift ? expr_clone(shift) : NULL;
    expr_t *product = expr_mul(amplitude, slope);
    expr_t *magnitude = product ? expr_abs(product) : NULL;
    expr_t *square = factor ? expr_pow_long(factor, 2L) : NULL;
    expr_t *denominator = magnitude && square ? expr_mul(magnitude, square) : NULL;
    expr_t *frequency_square = kernel ? expr_pow_long(frequency, 2L) : NULL;
    expr_t *numerator = kernel ? expr_mul_long(frequency_square, 2L) : expr_const_long(2L);
    expr_t *quotient = numerator && denominator ? expr_div(numerator, denominator) : NULL;
    expr_t *logarithm = quotient ? expr_log(quotient) : NULL;
    expr_t *body = logarithm ? expr_div(logarithm, slope) : NULL;
    expr_t *guarded = body ? restrict_real(body, x, centre, kernel ? frequency : NULL,
                                          kernel == expr_cosh ? NULL : factor) : NULL;
    equation_t *solution = guarded ? equ_new(y, guarded) : NULL;
    expr_free(guarded); expr_free(body); expr_free(logarithm); expr_free(quotient); expr_free(numerator);
    expr_free(frequency_square); expr_free(denominator); expr_free(square); expr_free(magnitude); expr_free(product);
    expr_free(factor);
    expr_free(argument); expr_free(shift);
    return solution;
}

static equation_t *initial_solution(const diffequ_t *de, const expr_t *x, const expr_t *y,
    const expr_t *force, const expr_t *slope)
{
    const expr_t *point = NULL, *value = NULL, *slope_point = NULL, *velocity = NULL;
    if (de->condition_count != 2u || !de_find_initial_condition(de, y, &point, &value) ||
        !de_find_derivative_condition(de, y, x, 1u, &slope_point, &velocity) ||
        !finite_real(point, false) || !finite_real(slope_point, false) ||
        !finite_real(value, false) || !finite_real(velocity, false))
        return NULL;
    number_t a = expr_eval(point), b = expr_eval(slope_point);
    bool same_point = num_eq(a, b);
    num_destroy(&b); num_destroy(&a);
    if (!same_point)
        return NULL;
    expr_t *at_value = expr_substitute(force, y, value);
    expr_t *velocity_square = expr_pow_long(velocity, 2L);
    expr_t *slope_square = expr_pow_long(slope, 2L);
    expr_t *first_term = expr_mul(slope_square, velocity_square);
    expr_t *second_term = at_value ? expr_mul(slope, at_value) : NULL;
    expr_t *twice_second = second_term ? expr_mul_long(second_term, 2L) : NULL;
    expr_t *difference = first_term && twice_second ? expr_sub(first_term, twice_second) : NULL;
    expr_t *quarter = difference ? expr_div_long(difference, 4L) : NULL;
    expr_t *energy = quarter ? expr_simplify(quarter) : NULL;
    number_t energy_value = energy ? expr_eval(energy) : num_clone(NUM_NAN);
    bool valid = num_is_real(energy_value) && num_is_finite(energy_value);
    bool zero_energy = energy && expr_is_exact_zero(energy);
    bool positive = valid && num_gt(energy_value, NUM_ZERO);
    num_destroy(&energy_value);
    expr_t *magnitude = energy ? expr_abs(energy) : NULL;
    expr_t *frequency = valid && !zero_energy ? expr_sqrt(magnitude) : NULL;
    expr_t *shift = expr_sub(x, point);
    expr_t *argument = frequency ? expr_mul(frequency, shift) : NULL;
    expr_t *cosine = zero_energy ? expr_const_one() : argument ? (positive ? expr_cosh(argument) : expr_cos(argument)) : NULL;
    expr_t *sine = zero_energy ? expr_clone(shift) : argument ? (positive ? expr_sinh(argument) : expr_sin(argument)) : NULL;
    expr_t *scale_product = expr_mul(slope, velocity);
    expr_t *half_scale = scale_product ? expr_div_long(scale_product, 2L) : NULL;
    expr_t *scale = zero_energy ? expr_clone(half_scale) : frequency ? expr_div(half_scale, frequency) : NULL;
    expr_t *term = scale && sine ? expr_mul(scale, sine) : NULL;
    expr_t *kernel = cosine && term ? expr_sub(cosine, term) : NULL;
    expr_t *square = kernel ? expr_pow_long(kernel, 2L) : NULL;
    expr_t *logarithm = square ? expr_log(square) : NULL;
    expr_t *quotient = logarithm ? expr_div(logarithm, slope) : NULL;
    expr_t *body = quotient ? expr_sub(value, quotient) : NULL;
    expr_t *guarded = body ? restrict_real(body, x, NULL, NULL, kernel) : NULL;
    equation_t *solution = guarded ? equ_new(y, guarded) : NULL;
    expr_free(guarded); expr_free(body); expr_free(quotient); expr_free(logarithm); expr_free(square);
    expr_free(kernel); expr_free(term); expr_free(scale); expr_free(half_scale); expr_free(scale_product);
    expr_free(sine); expr_free(cosine); expr_free(argument); expr_free(shift); expr_free(frequency);
    expr_free(magnitude); expr_free(energy); expr_free(quarter); expr_free(difference); expr_free(twice_second);
    expr_free(second_term); expr_free(first_term); expr_free(slope_square); expr_free(velocity_square); expr_free(at_value);
    return solution;
}

static bool energy_steps(diffequ_solve_result_t *result, const expr_t *x, const expr_t *y,
                         const expr_t *force, const expr_t *slope, bool initial)
{
    expr_t *wrts[] = {(expr_t *)x};
    expr_t *derivative = expr_new_formal_derivative(y, 1u, wrts);
    expr_t *square = derivative ? expr_pow_long(derivative, 2L) : NULL;
    string_t *energy_left = square ? expr_to_text(square, style_UNBOUND) : NULL;
    expr_t *potential = expr_div_simplify_owned(expr_mul_long(force, 2L), expr_clone(slope));
    string_t *potential_text = potential ? expr_to_text(potential, style_UNBOUND) : NULL;
    string_t *energy_right = potential_text ? string_sprintf("%S + E", potential_text) : NULL;
    string_t *plain = energy_left && energy_right ? string_sprintf(
        "Autonomous energy integral\nMultiply the equation by the first derivative and integrate.\n"
        "%S = %S.\nFor a non-zero exponential force there are no finite equilibrium solutions.\n"
        "E is the real conserved energy constant.\n"
        "Positive, negative and zero energy are retained when real solutions exist.\n"
        "%s\n"
        "Solutions apply on real intervals where their logarithm denominators do not vanish.\n",
        energy_left, energy_right, initial ? "The supplied value and slope fix the energy and translation."
                                          : "The arbitrary frequency is positive and the translation is real.") : NULL;
    const de_notation_step_t rows[] = {
        {.caption = "Autonomous energy integral"},
        {.caption = "Multiply by the first derivative and integrate."},
        {.caption = "E is the real conserved energy constant."},
        {.left = energy_left ? string_c_str(energy_left) : NULL, .right = energy_right ? string_c_str(energy_right) : NULL},
        {.caption = "Retain every real energy branch, including zero energy when present."},
        {.caption = "Work on real intervals away from zeros of the logarithm denominators; no finite equilibria exist."},
    };
    bool valid = plain && de_notation_steps(result, string_c_str(plain), rows, sizeof rows / sizeof *rows, false);
    string_free(plain); string_free(energy_right); string_free(potential_text); expr_free(potential);
    string_free(energy_left); expr_free(square); expr_free(derivative);
    return valid;
}

/* Complete the autonomous reduction before polynomial symmetry searches or unsupported-result diagnostics. */
diffequ_solve_result_t *de_solve_autonomous_exponential(const diffequ_t *de, const expr_t *x,
    const expr_t *y, const expr_t *second, const expr_t *first, const expr_t *residual, bool include_steps)
{
    expr_t *force = NULL, *slope = NULL, *amplitude = NULL;
    if (!de || !x || !y || !second || !residual)
        return NULL;
    if (!exponential_force(residual, x, y, second, first, &force, &slope, &amplitude))
        return NULL;
    diffequ_solve_result_t *result = de_solve_result_new(DE_SOLVE_STATUS_SOLVED, DE_SOLVER_AUTONOMOUS_ENERGY,
        "solved by the conserved energy of an autonomous exponential force");
    equation_t *solutions[3] = {NULL};
    size_t count = 0u, next_constant = 1u;
    expr_t *frequency = NULL, *centre = NULL, *product = NULL;
    bool valid = result != NULL;
    if (valid && de->condition_count) {
        solutions[count++] = initial_solution(de, x, y, force, slope);
    } else if (valid) {
        frequency = free_constant(de, &next_constant);
        centre = free_constant(de, &next_constant);
        product = expr_mul(amplitude, slope);
        number_t sign = expr_eval(product);
        bool positive = num_gt(sign, NUM_ZERO);
        num_destroy(&sign);
        solutions[count++] = frequency && centre ? energy_family(x, y, amplitude, slope, frequency, centre,
                                                                 positive ? expr_sinh : expr_cosh) : NULL;
        if (positive) {
            solutions[count++] = energy_family(x, y, amplitude, slope, frequency, centre, expr_cos);
            solutions[count++] = energy_family(x, y, amplitude, slope, frequency, centre, NULL);
        }
    }
    for (size_t i = 0; valid && i < count; ++i) {
        valid = solutions[i] && de_solve_result_append(result, solutions[i]) == 0;
        if (valid)
            solutions[i] = NULL;
    }
    valid = valid && (!include_steps || energy_steps(result, x, y, force, slope, de->condition_count != 0u));
    if (!valid) { de_solve_result_free(result); result = NULL; }
    for (size_t i = 0; i < count; ++i) equ_free(solutions[i]);
    expr_free(product); expr_free(centre); expr_free(frequency); expr_free(amplitude); expr_free(slope); expr_free(force);
    return result;
}
