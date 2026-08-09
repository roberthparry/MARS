#include <stdbool.h>
#include <stdlib.h>

#define MARS_DIFFEQUATION_PDE_INTERNAL_ACCESS
#include "diffequ_pde_internal.h"

static bool de_transport_is_constant_coefficient(const expr_t *coefficient, const expr_t *x, const expr_t *y,
                                                 const expr_t *dependent)
{
    return coefficient && !expr_is_exact_zero(coefficient) && !de_expr_uses(coefficient, x) &&
           !de_expr_uses(coefficient, y) && !de_expr_uses(coefficient, dependent);
}

static bool de_transport_is_constant_term(const expr_t *term, const expr_t *x, const expr_t *y, const expr_t *dependent)
{
    return term && !de_expr_uses(term, x) && !de_expr_uses(term, y) && !de_expr_uses(term, dependent);
}

static expr_t *de_transport_characteristic_foot(const expr_t *along, const expr_t *transverse,
                                                const expr_t *fixed_transverse, const expr_t *along_coefficient,
                                                const expr_t *transverse_coefficient)
{
    expr_t *delta = expr_sub_simplify_owned(expr_clone(transverse), expr_clone(fixed_transverse));
    expr_t *ratio = expr_div_simplify_owned(expr_clone(along_coefficient), expr_clone(transverse_coefficient));
    expr_t *shift = NULL;
    expr_t *foot = NULL;

    if (delta && ratio) {
        shift = expr_mul_simplify_owned(ratio, delta);
        ratio = NULL;
        delta = NULL;
    }
    if (shift) {
        foot = expr_sub_simplify_owned(expr_clone(along), shift);
        shift = NULL;
    }
    expr_free(shift);
    expr_free(ratio);
    expr_free(delta);
    return foot;
}

static expr_t *de_transport_additive_forcing_solution(const expr_t *first, const expr_t *second,
                                                      const expr_t *first_coefficient, const expr_t *second_coefficient,
                                                      const expr_t *forcing, expr_t *arbitrary);

static equation_t *de_transport_boundary_solution(const diffequ_t *de, const expr_t *dependent, const expr_t *x,
                                                  const expr_t *y, const expr_t *x_coefficient,
                                                  const expr_t *y_coefficient, const expr_t *reaction_coefficient,
                                                  const expr_t *forcing)
{
    for (size_t i = 0u; i < de->condition_count; ++i) {
        const equation_t *condition = de->conditions[i];
        const expr_t *left = condition ? equ_lhs(condition) : NULL;
        const expr_t *value = condition ? equ_rhs(condition) : NULL;
        const expr_t *first;
        const expr_t *second;
        const expr_t *along;
        const expr_t *transverse;
        const expr_t *fixed_transverse;
        const expr_t *along_coefficient;
        const expr_t *transverse_coefficient;
        expr_t *foot;
        expr_t *substituted;
        expr_t *travel;
        expr_t *particular = NULL;
        expr_t *particular_at_transverse = NULL;
        expr_t *particular_at_boundary = NULL;
        expr_t *solution_right;
        equation_t *solution;

        if (!left || !value || !expr_struct_eq(left, dependent) || de->condition_point_counts[i] != 2u)
            continue;

        first = de->condition_points[i][0];
        second = de->condition_points[i][1];
        if (expr_struct_eq(first, x) && !de_expr_uses(second, x) && !de_expr_uses(second, y) &&
            !de_expr_uses(value, y) && !de_expr_uses(value, dependent)) {
            along = x;
            transverse = y;
            fixed_transverse = second;
            along_coefficient = x_coefficient;
            transverse_coefficient = y_coefficient;
        } else if (expr_struct_eq(second, y) && !de_expr_uses(first, x) && !de_expr_uses(first, y) &&
                   !de_expr_uses(value, x) && !de_expr_uses(value, dependent)) {
            along = y;
            transverse = x;
            fixed_transverse = first;
            along_coefficient = y_coefficient;
            transverse_coefficient = x_coefficient;
        } else {
            continue;
        }

        foot = de_transport_characteristic_foot(along, transverse, fixed_transverse, along_coefficient,
                                                transverse_coefficient);
        substituted = foot ? expr_substitute(value, along, foot) : NULL;
        travel = foot ? expr_sub_simplify_owned(expr_clone(along), expr_clone(foot)) : NULL;
        solution_right = NULL;
        if (substituted && travel && expr_is_exact_zero(reaction_coefficient)) {
            expr_t *zero = expr_const_zero();

            particular =
                zero ? de_transport_additive_forcing_solution(x, y, x_coefficient, y_coefficient, forcing, zero) : NULL;
            zero = NULL;
            particular_at_transverse = particular ? expr_substitute(particular, transverse, fixed_transverse) : NULL;
            particular_at_boundary =
                particular_at_transverse ? expr_substitute(particular_at_transverse, along, foot) : NULL;
            if (particular && particular_at_boundary) {
                expr_t *with_boundary_value = expr_add(particular, substituted);

                solution_right = with_boundary_value ? expr_sub(with_boundary_value, particular_at_boundary) : NULL;
                expr_free(with_boundary_value);
            }
        } else if (substituted && travel) {
            expr_t *offset = expr_div(forcing, reaction_coefficient);
            expr_t *adjusted = offset ? expr_add(substituted, offset) : NULL;
            expr_t *alpha = expr_div(reaction_coefficient, along_coefficient);
            expr_t *exponent = alpha ? expr_mul(alpha, travel) : NULL;
            expr_t *negative_exponent = exponent ? expr_neg(exponent) : NULL;
            expr_t *decay = negative_exponent ? expr_exp(negative_exponent) : NULL;
            expr_t *evolved = adjusted && decay ? expr_mul(adjusted, decay) : NULL;

            solution_right = evolved && offset ? expr_sub(evolved, offset) : NULL;
            expr_free(evolved);
            expr_free(decay);
            expr_free(negative_exponent);
            expr_free(exponent);
            expr_free(alpha);
            expr_free(adjusted);
            expr_free(offset);
        }
        solution_right = solution_right ? expr_simplify_owned(solution_right) : NULL;
        expr_free(particular_at_boundary);
        expr_free(particular_at_transverse);
        expr_free(particular);
        expr_free(travel);
        expr_free(substituted);
        expr_free(foot);
        if (!solution_right)
            return NULL;
        solution = de_pde_solution_equation(dependent, solution_right);
        expr_free(solution_right);
        return solution;
    }
    return NULL;
}

static expr_t *de_transport_invariant(const expr_t *first, const expr_t *second, const expr_t *first_coefficient,
                                      const expr_t *second_coefficient)
{
    expr_t *ratio = expr_div_simplify_owned(expr_clone(second_coefficient), expr_clone(first_coefficient));
    expr_t *shift = ratio ? expr_mul_simplify_owned(ratio, expr_clone(first)) : NULL;

    ratio = NULL;
    if (!shift)
        return NULL;
    return expr_sub_simplify_owned(expr_clone(second), shift);
}

static expr_t *de_transport_directional_derivative(const expr_t *expr, const expr_t *first, const expr_t *second,
                                                   const expr_t *first_coefficient, const expr_t *second_coefficient)
{
    expr_t *first_derivative = expr_create_deriv(expr, first);
    expr_t *second_derivative = expr_create_deriv(expr, second);
    expr_t *first_term = first_derivative ? expr_mul(first_coefficient, first_derivative) : NULL;
    expr_t *second_term = second_derivative ? expr_mul(second_coefficient, second_derivative) : NULL;
    expr_t *raw = first_term && second_term ? expr_add(first_term, second_term) : NULL;
    expr_t *directional = raw ? expr_simplify(raw) : NULL;

    expr_free(raw);
    expr_free(second_term);
    expr_free(first_term);
    expr_free(second_derivative);
    expr_free(first_derivative);
    return directional;
}

static expr_t *de_transport_closed_directional_particular(const expr_t *first, const expr_t *second,
                                                          const expr_t *first_coefficient,
                                                          const expr_t *second_coefficient,
                                                          const expr_t *reaction_coefficient, const expr_t *forcing)
{
    expr_t *first_directional =
        de_transport_directional_derivative(forcing, first, second, first_coefficient, second_coefficient);
    expr_t *second_directional = first_directional
                                     ? de_transport_directional_derivative(first_directional, first, second,
                                                                           first_coefficient, second_coefficient)
                                     : NULL;
    expr_t *derivative_scale =
        second_directional ? expr_div_simplify_owned(expr_clone(second_directional), expr_clone(forcing)) : NULL;
    expr_t *reconstructed =
        derivative_scale ? expr_mul_simplify_owned(expr_clone(derivative_scale), expr_clone(forcing)) : NULL;
    expr_t *remainder = reconstructed ? expr_sub_simplify_owned(expr_clone(second_directional), reconstructed) : NULL;
    expr_t *reaction_square = NULL;
    expr_t *denominator = NULL;
    expr_t *reaction_forcing = NULL;
    expr_t *numerator = NULL;
    expr_t *particular = NULL;
    expr_t *particular_directional = NULL;
    expr_t *particular_reaction = NULL;
    expr_t *applied = NULL;
    expr_t *check = NULL;

    reconstructed = NULL;
    if (!second_directional || !derivative_scale || !remainder || !expr_is_exact_zero(remainder) ||
        de_expr_uses(derivative_scale, first) || de_expr_uses(derivative_scale, second))
        goto cleanup;

    reaction_square = expr_mul(reaction_coefficient, reaction_coefficient);
    denominator = reaction_square ? expr_sub_simplify_owned(reaction_square, expr_clone(derivative_scale)) : NULL;
    reaction_square = NULL;
    if (!denominator || expr_is_exact_zero(denominator))
        goto cleanup;

    reaction_forcing = expr_mul(reaction_coefficient, forcing);
    numerator = reaction_forcing ? expr_sub_simplify_owned(expr_clone(first_directional), reaction_forcing) : NULL;
    reaction_forcing = NULL;
    particular = numerator ? expr_div_simplify_owned(numerator, expr_clone(denominator)) : NULL;
    numerator = NULL;
    particular_directional = particular ? de_transport_directional_derivative(particular, first, second,
                                                                              first_coefficient, second_coefficient)
                                        : NULL;
    particular_reaction = particular ? expr_mul(reaction_coefficient, particular) : NULL;
    applied = particular_directional && particular_reaction
                  ? expr_add_simplify_owned(particular_directional, particular_reaction)
                  : NULL;
    particular_directional = NULL;
    particular_reaction = NULL;
    check = applied ? expr_add_simplify_owned(applied, expr_clone(forcing)) : NULL;
    applied = NULL;
    if (!check || !expr_is_exact_zero(check)) {
        expr_free(particular);
        particular = NULL;
    }

cleanup:
    expr_free(check);
    expr_free(applied);
    expr_free(particular_reaction);
    expr_free(particular_directional);
    expr_free(numerator);
    expr_free(reaction_forcing);
    expr_free(denominator);
    expr_free(reaction_square);
    expr_free(remainder);
    expr_free(reconstructed);
    expr_free(derivative_scale);
    expr_free(second_directional);
    expr_free(first_directional);
    return particular;
}

static expr_t *de_transport_unary_phase_particular(const expr_t *first, const expr_t *second,
                                                   const expr_t *first_coefficient, const expr_t *second_coefficient,
                                                   const expr_t *forcing)
{
    expr_t *target = expr_negate_owned(expr_clone(forcing));
    number_t scale = num_new();
    const expr_t *base = target;
    const expr_t *phase = NULL;
    expr_t *phase_directional = NULL;
    expr_t *dummy = NULL;
    expr_t *dummy_base = NULL;
    expr_t *dummy_primitive = NULL;
    expr_t *phase_primitive = NULL;
    expr_t *scaled_primitive = NULL;
    expr_t *candidate = NULL;
    expr_t *candidate_directional = NULL;
    expr_t *check = NULL;
    number_t phase_rate = num_new();

    if (!target)
        goto cleanup;
    if (!expr_match_scaled_expr(target, &scale, &base)) {
        num_destroy(&scale);
        scale = num_clone(NUM_ONE);
        base = target;
    }
    if (!expr_match_unary_expr(base, &phase))
        goto cleanup;
    phase_directional =
        de_transport_directional_derivative(phase, first, second, first_coefficient, second_coefficient);
    if (!phase_directional || expr_is_exact_zero(phase_directional) || de_expr_uses(phase_directional, first) ||
        de_expr_uses(phase_directional, second))
        goto cleanup;

    dummy = expr_new_named_var(NUM_NAN, "θ");
    dummy_base = dummy ? expr_substitute(base, phase, dummy) : NULL;
    dummy_primitive = dummy_base ? expr_integrate(dummy_base, dummy) : NULL;
    phase_primitive = dummy_primitive ? expr_substitute(dummy_primitive, dummy, phase) : NULL;
    if (phase_primitive && expr_match_const_value(phase_directional, &phase_rate)) {
        number_t coefficient = num_div(scale, phase_rate);

        candidate = expr_mul_num(phase_primitive, &coefficient);
        num_destroy(&coefficient);
    } else {
        scaled_primitive = phase_primitive ? expr_mul_num(phase_primitive, &scale) : NULL;
        candidate = scaled_primitive ? expr_div_simplify_owned(scaled_primitive, expr_clone(phase_directional)) : NULL;
        scaled_primitive = NULL;
    }
    candidate_directional =
        candidate ? de_transport_directional_derivative(candidate, first, second, first_coefficient, second_coefficient)
                  : NULL;
    check = candidate_directional ? expr_add_simplify_owned(candidate_directional, expr_clone(forcing)) : NULL;
    candidate_directional = NULL;
    if (!check || !expr_is_exact_zero(check)) {
        expr_free(candidate);
        candidate = NULL;
    }

cleanup:
    expr_free(check);
    expr_free(candidate_directional);
    expr_free(scaled_primitive);
    expr_free(phase_primitive);
    expr_free(dummy_primitive);
    expr_free(dummy_base);
    expr_free(dummy);
    expr_free(phase_directional);
    num_destroy(&phase_rate);
    num_destroy(&scale);
    expr_free(target);
    return candidate;
}

static expr_t *de_transport_prepend_sum(const expr_t *prefix, const expr_t *tail);

static expr_t *de_transport_combine_particular(const expr_t *first, const expr_t *first_coefficient,
                                               const expr_t *reaction_coefficient, const expr_t *arbitrary,
                                               const expr_t *particular)
{
    expr_t *homogeneous = NULL;
    expr_t *alpha = NULL;
    expr_t *exponent = NULL;
    expr_t *decay = NULL;
    expr_t *right;

    if (expr_is_exact_zero(reaction_coefficient)) {
        homogeneous = expr_clone(arbitrary);
    } else {
        alpha = expr_div_simplify_owned(expr_clone(reaction_coefficient), expr_clone(first_coefficient));
        exponent = alpha ? expr_mul_simplify_owned(alpha, expr_clone(first)) : NULL;
        alpha = NULL;
        exponent = exponent ? expr_negate_owned(exponent) : NULL;
        decay = exponent ? expr_exp(exponent) : NULL;
        homogeneous = decay ? expr_mul(decay, arbitrary) : NULL;
    }
    right = homogeneous && particular ? de_transport_prepend_sum(homogeneous, particular) : NULL;
    expr_free(homogeneous);
    expr_free(decay);
    expr_free(exponent);
    expr_free(alpha);
    return right;
}

static expr_t *de_transport_variable_forcing_solution(const expr_t *first, const expr_t *second,
                                                      const expr_t *first_coefficient, const expr_t *second_coefficient,
                                                      const expr_t *reaction_coefficient, const expr_t *forcing,
                                                      const expr_t *invariant, expr_t *arbitrary)
{
    expr_t *eta = expr_new_named_var(NUM_NAN, "ξ");
    expr_t *ratio = expr_div_simplify_owned(expr_clone(second_coefficient), expr_clone(first_coefficient));
    expr_t *path_shift = ratio ? expr_mul_simplify_owned(ratio, expr_clone(first)) : NULL;
    expr_t *path_second = eta && path_shift ? expr_add_simplify_owned(expr_clone(eta), path_shift) : NULL;
    expr_t *forcing_path = path_second ? expr_substitute(forcing, second, path_second) : NULL;
    expr_t *negative_forcing = forcing_path ? expr_negate_owned(forcing_path) : NULL;
    expr_t *source = negative_forcing ? expr_div_simplify_owned(negative_forcing, expr_clone(first_coefficient)) : NULL;
    expr_t *alpha = expr_div_simplify_owned(expr_clone(reaction_coefficient), expr_clone(first_coefficient));
    expr_t *alpha_first = alpha ? expr_mul_simplify_owned(alpha, expr_clone(first)) : NULL;
    expr_t *factor = alpha_first ? expr_exp(alpha_first) : NULL;
    expr_t *weighted = source && factor ? expr_mul_simplify_owned(source, expr_clone(factor)) : NULL;
    expr_t *antiderivative = weighted ? de_integrate_or_formal(weighted, first) : NULL;
    expr_t *current_antiderivative = antiderivative && eta ? expr_substitute(antiderivative, eta, invariant) : NULL;
    expr_t *particular = current_antiderivative && factor
                             ? expr_div_simplify_owned(expr_clone(current_antiderivative), expr_clone(factor))
                             : NULL;
    expr_t *homogeneous =
        arbitrary && factor ? expr_div_simplify_owned(expr_clone(arbitrary), expr_clone(factor)) : NULL;
    expr_t *right = particular && homogeneous ? expr_add(particular, homogeneous) : NULL;

    ratio = NULL;
    path_shift = NULL;
    forcing_path = NULL;
    negative_forcing = NULL;
    source = NULL;
    alpha = NULL;
    expr_free(arbitrary);
    expr_free(homogeneous);
    expr_free(particular);
    expr_free(current_antiderivative);
    expr_free(antiderivative);
    expr_free(weighted);
    expr_free(factor);
    expr_free(alpha_first);
    expr_free(alpha);
    expr_free(source);
    expr_free(negative_forcing);
    expr_free(forcing_path);
    expr_free(path_second);
    expr_free(path_shift);
    expr_free(ratio);
    expr_free(eta);
    return right ? expr_simplify_owned(right) : NULL;
}

static expr_t *de_transport_additive_forcing_solution(const expr_t *first, const expr_t *second,
                                                      const expr_t *first_coefficient, const expr_t *second_coefficient,
                                                      const expr_t *forcing, expr_t *arbitrary)
{
    expr_t *zero = expr_new_const(NUM_ZERO);
    expr_t *at_second_zero = zero ? expr_substitute(forcing, second, zero) : NULL;
    expr_t *origin = at_second_zero && zero ? expr_substitute(at_second_zero, first, zero) : NULL;
    expr_t *first_part =
        at_second_zero && origin ? expr_sub_simplify_owned(expr_clone(at_second_zero), expr_clone(origin)) : NULL;
    expr_t *second_part = zero ? expr_substitute(forcing, first, zero) : NULL;
    expr_t *separated =
        first_part && second_part ? expr_add_simplify_owned(expr_clone(first_part), expr_clone(second_part)) : NULL;
    expr_t *mixed = separated ? expr_sub_simplify_owned(expr_clone(forcing), separated) : NULL;
    expr_t *first_source =
        first_part ? expr_div_simplify_owned(expr_negate_owned(expr_clone(first_part)), expr_clone(first_coefficient))
                   : NULL;
    expr_t *second_source = second_part ? expr_div_simplify_owned(expr_negate_owned(expr_clone(second_part)),
                                                                  expr_clone(second_coefficient))
                                        : NULL;
    expr_t *first_integral = NULL;
    expr_t *second_integral = NULL;
    expr_t *particular = NULL;
    expr_t *right = NULL;

    if (!mixed || !expr_is_exact_zero(mixed))
        goto cleanup;
    first_integral = first_source ? de_integrate_or_formal(first_source, first) : NULL;
    second_integral = second_source ? de_integrate_or_formal(second_source, second) : NULL;
    particular = first_integral && second_integral
                     ? expr_add_simplify_owned(expr_clone(first_integral), expr_clone(second_integral))
                     : NULL;
    right = particular && arbitrary ? expr_add_simplify_owned(expr_clone(arbitrary), expr_clone(particular)) : NULL;

cleanup:
    expr_free(particular);
    expr_free(second_integral);
    expr_free(first_integral);
    expr_free(second_source);
    expr_free(first_source);
    expr_free(mixed);
    expr_free(second_part);
    expr_free(first_part);
    expr_free(origin);
    expr_free(at_second_zero);
    expr_free(zero);
    expr_free(arbitrary);
    return right;
}

static expr_t *de_transport_prepend_sum(const expr_t *prefix, const expr_t *tail)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    bool is_sub = false;

    if (expr_match_add_sub_expr(tail, &left, &right, &is_sub)) {
        expr_t *combined_left = de_transport_prepend_sum(prefix, left);
        expr_t *combined =
            combined_left ? (is_sub ? expr_sub(combined_left, right) : expr_add(combined_left, right)) : NULL;

        expr_free(combined_left);
        return combined;
    }
    return expr_add(prefix, tail);
}

static expr_t *de_transport_closed_derivative_particular(const expr_t *variable, const expr_t *coefficient,
                                                         const expr_t *reaction_coefficient, const expr_t *forcing)
{
    expr_t *first_derivative = expr_create_deriv(forcing, variable);
    expr_t *second_derivative = first_derivative ? expr_create_deriv(first_derivative, variable) : NULL;
    expr_t *derivative_scale =
        second_derivative ? expr_div_simplify_owned(expr_clone(second_derivative), expr_clone(forcing)) : NULL;
    expr_t *reconstructed =
        derivative_scale ? expr_mul_simplify_owned(expr_clone(derivative_scale), expr_clone(forcing)) : NULL;
    expr_t *remainder = reconstructed ? expr_sub_simplify_owned(expr_clone(second_derivative), reconstructed) : NULL;
    expr_t *reaction_square = NULL;
    expr_t *coefficient_square = NULL;
    expr_t *scaled_coefficient_square = NULL;
    expr_t *denominator = NULL;
    expr_t *derivative_term = NULL;
    expr_t *forcing_term = NULL;
    expr_t *numerator = NULL;
    expr_t *particular = NULL;

    reconstructed = NULL;
    if (!second_derivative || !derivative_scale || !remainder || !expr_is_exact_zero(remainder) ||
        de_expr_uses(derivative_scale, variable))
        goto cleanup;

    reaction_square = expr_mul(reaction_coefficient, reaction_coefficient);
    coefficient_square = expr_mul(coefficient, coefficient);
    scaled_coefficient_square =
        coefficient_square && derivative_scale ? expr_mul(coefficient_square, derivative_scale) : NULL;
    denominator = reaction_square && scaled_coefficient_square
                      ? expr_sub_simplify_owned(reaction_square, scaled_coefficient_square)
                      : NULL;
    reaction_square = NULL;
    scaled_coefficient_square = NULL;
    if (!denominator || expr_is_exact_zero(denominator))
        goto cleanup;

    derivative_term = first_derivative ? expr_mul(coefficient, first_derivative) : NULL;
    forcing_term = expr_mul(reaction_coefficient, forcing);
    numerator = derivative_term && forcing_term ? expr_sub_simplify_owned(derivative_term, forcing_term) : NULL;
    derivative_term = NULL;
    forcing_term = NULL;
    particular = numerator ? expr_div_simplify_owned(numerator, expr_clone(denominator)) : NULL;
    numerator = NULL;

cleanup:
    expr_free(numerator);
    expr_free(forcing_term);
    expr_free(derivative_term);
    expr_free(denominator);
    expr_free(scaled_coefficient_square);
    expr_free(coefficient_square);
    expr_free(reaction_square);
    expr_free(remainder);
    expr_free(reconstructed);
    expr_free(derivative_scale);
    expr_free(second_derivative);
    expr_free(first_derivative);
    return particular;
}

static expr_t *de_transport_coordinate_reaction_particular(const expr_t *variable, const expr_t *coefficient,
                                                           const expr_t *reaction_coefficient, const expr_t *forcing)
{
    if (expr_is_exact_zero(forcing))
        return expr_new_const(NUM_ZERO);

    expr_t *closed = de_transport_closed_derivative_particular(variable, coefficient, reaction_coefficient, forcing);

    if (closed)
        return closed;

    expr_t *alpha = expr_div_simplify_owned(expr_clone(reaction_coefficient), expr_clone(coefficient));
    expr_t *exponent = alpha ? expr_mul_simplify_owned(alpha, expr_clone(variable)) : NULL;
    expr_t *factor = exponent ? expr_exp(exponent) : NULL;
    expr_t *source = expr_div_simplify_owned(expr_negate_owned(expr_clone(forcing)), expr_clone(coefficient));
    expr_t *weighted = source && factor ? expr_mul_simplify_owned(source, expr_clone(factor)) : NULL;
    expr_t *antiderivative = weighted ? de_integrate_or_formal(weighted, variable) : NULL;
    expr_t *particular =
        antiderivative && factor ? expr_div_simplify_owned(expr_clone(antiderivative), expr_clone(factor)) : NULL;

    alpha = NULL;
    exponent = NULL;
    source = NULL;
    expr_free(antiderivative);
    expr_free(weighted);
    expr_free(source);
    expr_free(factor);
    expr_free(exponent);
    expr_free(alpha);
    return particular;
}

static expr_t *de_transport_additive_reaction_solution(const expr_t *first, const expr_t *second,
                                                       const expr_t *first_coefficient,
                                                       const expr_t *second_coefficient,
                                                       const expr_t *reaction_coefficient, const expr_t *forcing,
                                                       expr_t *arbitrary)
{
    expr_t *zero = expr_new_const(NUM_ZERO);
    expr_t *at_second_zero = zero ? expr_substitute(forcing, second, zero) : NULL;
    expr_t *origin = at_second_zero && zero ? expr_substitute(at_second_zero, first, zero) : NULL;
    expr_t *first_part =
        at_second_zero && origin ? expr_sub_simplify_owned(expr_clone(at_second_zero), expr_clone(origin)) : NULL;
    expr_t *second_part = zero ? expr_substitute(forcing, first, zero) : NULL;
    expr_t *separated =
        first_part && second_part ? expr_add_simplify_owned(expr_clone(first_part), expr_clone(second_part)) : NULL;
    expr_t *mixed = separated ? expr_sub_simplify_owned(expr_clone(forcing), separated) : NULL;
    expr_t *first_solution = NULL;
    expr_t *second_solution = NULL;
    expr_t *particular = NULL;
    expr_t *alpha = NULL;
    expr_t *exponent = NULL;
    expr_t *negative_exponent = NULL;
    expr_t *decay = NULL;
    expr_t *homogeneous = NULL;
    expr_t *right = NULL;

    if (!mixed || !expr_is_exact_zero(mixed))
        goto cleanup;
    first_solution =
        de_transport_coordinate_reaction_particular(first, first_coefficient, reaction_coefficient, first_part);
    second_solution =
        de_transport_coordinate_reaction_particular(second, second_coefficient, reaction_coefficient, second_part);
    particular = first_solution && second_solution
                     ? expr_add_simplify_owned(expr_clone(first_solution), expr_clone(second_solution))
                     : NULL;

    alpha = expr_div_simplify_owned(expr_clone(reaction_coefficient), expr_clone(first_coefficient));
    exponent = alpha ? expr_mul_simplify_owned(alpha, expr_clone(first)) : NULL;
    alpha = NULL;
    negative_exponent = exponent ? expr_negate_owned(exponent) : NULL;
    exponent = NULL;
    decay = negative_exponent ? expr_exp(negative_exponent) : NULL;
    homogeneous = decay && arbitrary ? expr_mul(decay, arbitrary) : NULL;
    right = homogeneous && particular ? de_transport_prepend_sum(homogeneous, particular) : NULL;

cleanup:
    expr_free(homogeneous);
    expr_free(decay);
    expr_free(negative_exponent);
    expr_free(exponent);
    expr_free(alpha);
    expr_free(particular);
    expr_free(second_solution);
    expr_free(first_solution);
    expr_free(mixed);
    expr_free(second_part);
    expr_free(first_part);
    expr_free(origin);
    expr_free(at_second_zero);
    expr_free(zero);
    expr_free(arbitrary);
    return right;
}

static expr_t *de_transport_polynomial_reaction_solution(const expr_t *first, const expr_t *second,
                                                         const expr_t *first_coefficient,
                                                         const expr_t *second_coefficient,
                                                         const expr_t *reaction_coefficient, const expr_t *forcing,
                                                         expr_t *arbitrary)
{
    expr_t *negative_forcing = expr_negate_owned(expr_clone(forcing));
    expr_t *current =
        negative_forcing ? expr_div_simplify_owned(negative_forcing, expr_clone(reaction_coefficient)) : NULL;
    expr_t *sum = current ? expr_clone(current) : NULL;
    expr_t *alpha = expr_div_simplify_owned(expr_clone(reaction_coefficient), expr_clone(first_coefficient));
    expr_t *exponent = alpha ? expr_mul_simplify_owned(alpha, expr_clone(first)) : NULL;
    expr_t *negative_exponent = exponent ? expr_negate_owned(exponent) : NULL;
    expr_t *decay = negative_exponent ? expr_exp(negative_exponent) : NULL;
    expr_t *homogeneous = decay && arbitrary ? expr_mul(decay, arbitrary) : NULL;
    expr_t *right = NULL;

    negative_forcing = NULL;
    alpha = NULL;
    exponent = NULL;
    for (size_t degree = 0u; current && sum && degree < 64u; ++degree) {
        expr_t *dx = expr_create_deriv(current, first);
        expr_t *dy = expr_create_deriv(current, second);
        expr_t *x_part = dx ? expr_mul(first_coefficient, dx) : NULL;
        expr_t *y_part = dy ? expr_mul(second_coefficient, dy) : NULL;
        expr_t *directional = NULL;
        expr_t *negative_directional;
        expr_t *next;

        if (x_part && y_part) {
            directional = expr_add_simplify_owned(x_part, y_part);
            x_part = NULL;
            y_part = NULL;
        }
        expr_free(y_part);
        expr_free(x_part);
        expr_free(dy);
        expr_free(dx);
        if (!directional)
            break;
        if (expr_is_exact_zero(directional)) {
            right = homogeneous ? de_transport_prepend_sum(homogeneous, sum) : NULL;
            expr_free(directional);
            break;
        }

        negative_directional = expr_negate_owned(directional);
        directional = NULL;
        next = negative_directional ? expr_div_simplify_owned(negative_directional, expr_clone(reaction_coefficient))
                                    : NULL;
        negative_directional = NULL;
        if (!next)
            break;
        sum = expr_add_simplify_owned(sum, expr_clone(next));
        expr_free(current);
        current = next;
    }

    expr_free(homogeneous);
    expr_free(decay);
    expr_free(negative_exponent);
    expr_free(exponent);
    expr_free(alpha);
    expr_free(sum);
    expr_free(current);
    expr_free(negative_forcing);
    expr_free(arbitrary);
    return right;
}

static equation_t *de_transport_general_solution(const expr_t *dependent, const expr_t *first, const expr_t *second,
                                                 const expr_t *first_coefficient, const expr_t *second_coefficient,
                                                 const expr_t *reaction_coefficient, const expr_t *forcing)
{
    expr_t *invariant = de_transport_invariant(first, second, first_coefficient, second_coefficient);
    expr_t *arbitrary = invariant ? expr_new_arbitrary_function("F", invariant) : NULL;
    expr_t *particular = NULL;
    expr_t *right = NULL;
    equation_t *solution = NULL;

    expr_free(invariant);
    if (!arbitrary)
        return NULL;
    if (de_expr_uses(forcing, first) || de_expr_uses(forcing, second)) {
        invariant = de_transport_invariant(first, second, first_coefficient, second_coefficient);
        if (invariant && expr_is_exact_zero(reaction_coefficient))
            right = de_transport_additive_forcing_solution(first, second, first_coefficient, second_coefficient,
                                                           forcing, expr_clone(arbitrary));
        if (!right && invariant && !expr_is_exact_zero(reaction_coefficient))
            right = de_transport_polynomial_reaction_solution(first, second, first_coefficient, second_coefficient,
                                                              reaction_coefficient, forcing, expr_clone(arbitrary));
        if (!right && invariant && !expr_is_exact_zero(reaction_coefficient))
            right = de_transport_additive_reaction_solution(first, second, first_coefficient, second_coefficient,
                                                            reaction_coefficient, forcing, expr_clone(arbitrary));
        if (!right && invariant)
            particular = de_transport_closed_directional_particular(first, second, first_coefficient,
                                                                    second_coefficient, reaction_coefficient, forcing);
        if (!right && !particular && invariant && expr_is_exact_zero(reaction_coefficient))
            particular =
                de_transport_unary_phase_particular(first, second, first_coefficient, second_coefficient, forcing);
        if (!right && particular)
            right =
                de_transport_combine_particular(first, first_coefficient, reaction_coefficient, arbitrary, particular);
        if (!right && invariant)
            right = de_transport_variable_forcing_solution(first, second, first_coefficient, second_coefficient,
                                                           reaction_coefficient, forcing, invariant, arbitrary);
        else
            expr_free(arbitrary);
        arbitrary = NULL;
        expr_free(invariant);
    } else if (expr_is_exact_zero(reaction_coefficient)) {
        if (expr_is_exact_zero(forcing)) {
            right = arbitrary;
            arbitrary = NULL;
        } else {
            expr_t *negative = expr_negate_owned(expr_clone(forcing));
            expr_t *rate = negative ? expr_div_simplify_owned(negative, expr_clone(first_coefficient)) : NULL;

            negative = NULL;
            particular = rate ? expr_mul_simplify_owned(rate, expr_clone(first)) : NULL;
            rate = NULL;
            right = particular ? expr_add_simplify_owned(particular, arbitrary) : NULL;
            particular = NULL;
            arbitrary = NULL;
        }
    } else {
        expr_t *alpha = expr_div_simplify_owned(expr_clone(reaction_coefficient), expr_clone(first_coefficient));
        expr_t *exponent = alpha ? expr_mul_simplify_owned(alpha, expr_clone(first)) : NULL;
        expr_t *negative_exponent = exponent ? expr_negate_owned(exponent) : NULL;
        expr_t *decay = negative_exponent ? expr_exp(negative_exponent) : NULL;
        expr_t *homogeneous = decay ? expr_mul_simplify_owned(decay, arbitrary) : NULL;
        expr_t *offset = expr_div_simplify_owned(expr_clone(forcing), expr_clone(reaction_coefficient));

        alpha = NULL;
        exponent = NULL;
        arbitrary = NULL;
        if (homogeneous)
            decay = NULL;
        right = homogeneous && offset ? expr_sub_simplify_owned(homogeneous, offset) : NULL;
        if (right) {
            homogeneous = NULL;
            offset = NULL;
        }
        expr_free(offset);
        expr_free(homogeneous);
        expr_free(decay);
        expr_free(negative_exponent);
        expr_free(exponent);
        expr_free(alpha);
    }
    if (right)
        solution = de_pde_solution_equation(dependent, right);
    expr_free(right);
    expr_free(particular);
    expr_free(arbitrary);
    return solution;
}

de_attempt_t de_pde_attempt_constant_transport(const diffequ_t *de, const expr_t *residual, equation_t **solution_out,
                                               bool *recognized_out)
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

    *solution_out = NULL;
    *recognized_out = false;
    if (!de || !residual || de->independent_count != 2u)
        return DE_ATTEMPT_NOT_MATCHED;

    x = de->independent_vars[0];
    y = de->independent_vars[1];
    if (!de_pde_find_first_derivatives(residual, x, y, &dependent, &dx, &dy) || !dependent || !dx || !dy ||
        !de_linear_decompose(residual, dx, &x_coefficient, &without_dx) ||
        !de_linear_decompose(without_dx, dy, &y_coefficient, &remainder) ||
        !de_linear_decompose(remainder, dependent, &reaction_coefficient, &forcing) ||
        !de_transport_is_constant_coefficient(x_coefficient, x, y, dependent) ||
        !de_transport_is_constant_coefficient(y_coefficient, x, y, dependent) ||
        !de_transport_is_constant_term(reaction_coefficient, x, y, dependent) || de_expr_uses(forcing, dependent))
        goto cleanup;

    *recognized_out = true;
    if (de->condition_count > 0u) {
        if (expr_is_exact_zero(reaction_coefficient) || (!de_expr_uses(forcing, x) && !de_expr_uses(forcing, y)))
            *solution_out = de_transport_boundary_solution(de, dependent, x, y, x_coefficient, y_coefficient,
                                                           reaction_coefficient, forcing);
    } else {
        *solution_out =
            de_transport_general_solution(dependent, x, y, x_coefficient, y_coefficient, reaction_coefficient, forcing);
    }
    attempt = *solution_out ? DE_ATTEMPT_SOLVED : DE_ATTEMPT_NOT_MATCHED;

cleanup:
    expr_free(forcing);
    expr_free(reaction_coefficient);
    expr_free(remainder);
    expr_free(y_coefficient);
    expr_free(without_dx);
    expr_free(x_coefficient);
    return attempt;
}

static bool de_transport_is_constant_in_all(const expr_t *expr, const diffequ_t *de, const expr_t *dependent)
{
    if (!expr || !de || de_expr_uses(expr, dependent))
        return false;
    for (size_t i = 0u; i < de->independent_count; ++i)
        if (de_expr_uses(expr, de->independent_vars[i]))
            return false;
    return true;
}

static expr_t *de_transport_multi_invariant(const expr_t *anchor, const expr_t *variable,
                                            const expr_t *anchor_coefficient, const expr_t *coefficient)
{
    expr_t *ratio = expr_div_simplify_owned(expr_clone(coefficient), expr_clone(anchor_coefficient));
    expr_t *shift = ratio ? expr_mul_simplify_owned(ratio, expr_clone(anchor)) : NULL;

    ratio = NULL;
    if (!shift)
        return NULL;
    return expr_sub_simplify_owned(expr_clone(variable), shift);
}

static expr_t *de_transport_multi_right(const diffequ_t *de, expr_t *const *coefficients,
                                        const expr_t *reaction_coefficient, const expr_t *forcing)
{
    size_t invariant_count = de->independent_count - 1u;
    expr_t **invariants = calloc(invariant_count, sizeof(*invariants));
    expr_t *arbitrary = NULL;
    expr_t *right = NULL;

    if (!invariants)
        return NULL;
    for (size_t i = 0u; i < invariant_count; ++i) {
        invariants[i] = de_transport_multi_invariant(de->independent_vars[0], de->independent_vars[i + 1u],
                                                     coefficients[0], coefficients[i + 1u]);
        if (!invariants[i])
            goto cleanup;
    }
    arbitrary = expr_new_arbitrary_function_n("F", invariant_count, invariants);
    if (!arbitrary)
        goto cleanup;

    if (expr_is_exact_zero(reaction_coefficient)) {
        if (expr_is_exact_zero(forcing)) {
            right = arbitrary;
            arbitrary = NULL;
        } else {
            expr_t *rate = expr_div_simplify_owned(expr_negate_owned(expr_clone(forcing)), expr_clone(coefficients[0]));
            expr_t *particular = rate ? expr_mul_simplify_owned(rate, expr_clone(de->independent_vars[0])) : NULL;

            rate = NULL;
            right = particular ? expr_add_simplify_owned(arbitrary, particular) : NULL;
            if (right) {
                arbitrary = NULL;
                particular = NULL;
            }
            expr_free(particular);
            expr_free(rate);
        }
    } else {
        expr_t *alpha = expr_div_simplify_owned(expr_clone(reaction_coefficient), expr_clone(coefficients[0]));
        expr_t *exponent = alpha ? expr_mul_simplify_owned(alpha, expr_clone(de->independent_vars[0])) : NULL;
        expr_t *negative_exponent = exponent ? expr_negate_owned(exponent) : NULL;
        expr_t *decay = negative_exponent ? expr_exp(negative_exponent) : NULL;
        expr_t *homogeneous = decay ? expr_mul_simplify_owned(decay, arbitrary) : NULL;
        expr_t *offset = expr_div_simplify_owned(expr_clone(forcing), expr_clone(reaction_coefficient));

        alpha = NULL;
        exponent = NULL;
        arbitrary = NULL;
        if (homogeneous)
            decay = NULL;
        right = homogeneous && offset ? expr_sub_simplify_owned(homogeneous, offset) : NULL;
        if (right) {
            homogeneous = NULL;
            offset = NULL;
        }
        expr_free(offset);
        expr_free(homogeneous);
        expr_free(decay);
        expr_free(negative_exponent);
        expr_free(exponent);
        expr_free(alpha);
    }

cleanup:
    expr_free(arbitrary);
    for (size_t i = 0u; i < invariant_count; ++i)
        expr_free(invariants[i]);
    free(invariants);
    return right;
}

de_attempt_t de_pde_attempt_constant_transport_n(const diffequ_t *de, const expr_t *residual, equation_t **solution_out,
                                                 bool *recognized_out)
{
    const expr_t *dependent = NULL;
    const expr_t **derivatives = NULL;
    expr_t **coefficients = NULL;
    expr_t *remainder = NULL;
    expr_t *reaction_coefficient = NULL;
    expr_t *forcing = NULL;
    expr_t *right = NULL;
    de_attempt_t attempt = DE_ATTEMPT_NOT_MATCHED;

    *solution_out = NULL;
    *recognized_out = false;
    if (!de || !residual || de->independent_count < 3u || de->condition_count != 0u)
        return DE_ATTEMPT_NOT_MATCHED;

    derivatives = calloc(de->independent_count, sizeof(*derivatives));
    coefficients = calloc(de->independent_count, sizeof(*coefficients));
    if (!derivatives || !coefficients) {
        attempt = DE_ATTEMPT_FAILED;
        goto cleanup;
    }
    if (!de_pde_find_first_derivatives_n(residual, de->independent_count, de->independent_vars, &dependent,
                                         derivatives) ||
        !dependent)
        goto cleanup;

    remainder = expr_clone(residual);
    for (size_t i = 0u; i < de->independent_count; ++i) {
        expr_t *next = NULL;

        if (!derivatives[i] || !de_linear_decompose(remainder, derivatives[i], &coefficients[i], &next)) {
            expr_free(next);
            goto cleanup;
        }
        expr_free(remainder);
        remainder = next;
    }
    if (!de_linear_decompose(remainder, dependent, &reaction_coefficient, &forcing))
        goto cleanup;
    for (size_t i = 0u; i < de->independent_count; ++i) {
        if (expr_is_exact_zero(coefficients[i]) || !de_transport_is_constant_in_all(coefficients[i], de, dependent))
            goto cleanup;
    }
    if (!de_transport_is_constant_in_all(reaction_coefficient, de, dependent) ||
        !de_transport_is_constant_in_all(forcing, de, dependent))
        goto cleanup;

    *recognized_out = true;
    right = de_transport_multi_right(de, coefficients, reaction_coefficient, forcing);
    if (!right) {
        attempt = DE_ATTEMPT_FAILED;
        goto cleanup;
    }
    *solution_out = de_pde_solution_equation(dependent, right);
    attempt = *solution_out ? DE_ATTEMPT_SOLVED : DE_ATTEMPT_FAILED;

cleanup:
    expr_free(right);
    expr_free(forcing);
    expr_free(reaction_coefficient);
    expr_free(remainder);
    if (coefficients) {
        for (size_t i = 0u; i < de->independent_count; ++i)
            expr_free(coefficients[i]);
    }
    free(coefficients);
    free(derivatives);
    return attempt;
}
