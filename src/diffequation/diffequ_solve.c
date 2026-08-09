#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "diffequation.h"
#define MARS_DIFFEQUATION_SOLVE_INTERNAL_ACCESS
#include "diffequ_solve_internal.h"
#define MARS_DIFFEQUATION_PDE_INTERNAL_ACCESS
#include "diffequ_pde_internal.h"

expr_t *de_simplify_unary_owned(expr_t *owned, expr_t *(*operation)(const expr_t *))
{
    expr_t *raw = owned ? operation(owned) : NULL;
    expr_t *simplified = raw ? expr_simplify(raw) : NULL;

    expr_free(raw);
    expr_free(owned);
    return simplified;
}

expr_t *de_integrate_or_formal(const expr_t *integrand, const expr_t *wrt)
{
    expr_t *integral = integrand && wrt ? expr_integrate(integrand, wrt) : NULL;

    if (!integral && integrand && wrt)
        integral = expr_integral(integrand, wrt);
    return integral;
}

bool de_expr_contains(const expr_t *expr, const expr_t *needle)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;

    if (!expr || !needle)
        return false;
    if (expr_struct_eq(expr, needle))
        return true;
    if (expr_is_formal_derivative(expr))
        return false;
    if (!expr_child_exprs(expr, &left, &right))
        return false;
    return de_expr_contains(left, needle) || de_expr_contains(right, needle);
}

bool de_linear_decompose(const expr_t *expr, const expr_t *needle, expr_t **coefficient_out, expr_t **constant_out)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    bool is_sub = false;

    *coefficient_out = NULL;
    *constant_out = NULL;

    if (expr_struct_eq(expr, needle)) {
        *coefficient_out = expr_const_one();
        *constant_out = expr_const_zero();
        return *coefficient_out && *constant_out;
    }

    if (!de_expr_contains(expr, needle)) {
        *coefficient_out = expr_const_zero();
        *constant_out = expr_clone(expr);
        return *coefficient_out && *constant_out;
    }

    if (expr_match_neg_expr(expr, &left)) {
        expr_t *coefficient = NULL;
        expr_t *constant = NULL;

        if (!de_linear_decompose(left, needle, &coefficient, &constant))
            return false;
        *coefficient_out = de_simplify_unary_owned(coefficient, expr_neg);
        *constant_out = de_simplify_unary_owned(constant, expr_neg);
        if (*coefficient_out && *constant_out)
            return true;
        expr_free(*constant_out);
        expr_free(*coefficient_out);
        *constant_out = NULL;
        *coefficient_out = NULL;
        return false;
    }

    if (expr_match_add_sub_expr(expr, &left, &right, &is_sub)) {
        expr_t *left_coefficient = NULL;
        expr_t *left_constant = NULL;
        expr_t *right_coefficient = NULL;
        expr_t *right_constant = NULL;

        if (!de_linear_decompose(left, needle, &left_coefficient, &left_constant) ||
            !de_linear_decompose(right, needle, &right_coefficient, &right_constant))
            goto add_fail;

        *coefficient_out = is_sub ? expr_sub_simplify_owned(left_coefficient, right_coefficient)
                                  : expr_add_simplify_owned(left_coefficient, right_coefficient);
        *constant_out = is_sub ? expr_sub_simplify_owned(left_constant, right_constant)
                               : expr_add_simplify_owned(left_constant, right_constant);
        left_coefficient = NULL;
        right_coefficient = NULL;
        left_constant = NULL;
        right_constant = NULL;

    add_fail:
        expr_free(right_constant);
        expr_free(right_coefficient);
        expr_free(left_constant);
        expr_free(left_coefficient);
        if (*coefficient_out && *constant_out)
            return true;
        expr_free(*constant_out);
        expr_free(*coefficient_out);
        *constant_out = NULL;
        *coefficient_out = NULL;
        return false;
    }

    if (expr_match_mul_expr(expr, &left, &right)) {
        bool left_contains = de_expr_contains(left, needle);
        bool right_contains = de_expr_contains(right, needle);
        const expr_t *factor;
        const expr_t *linear;
        expr_t *coefficient = NULL;
        expr_t *constant = NULL;

        if (left_contains == right_contains)
            return false;
        linear = left_contains ? left : right;
        factor = left_contains ? right : left;
        if (!de_linear_decompose(linear, needle, &coefficient, &constant))
            return false;

        *coefficient_out = expr_mul_simplify_owned(coefficient, expr_clone(factor));
        coefficient = NULL;
        *constant_out = expr_mul_simplify_owned(constant, expr_clone(factor));
        constant = NULL;
        if (*coefficient_out && *constant_out)
            return true;
        expr_free(*constant_out);
        expr_free(*coefficient_out);
        *constant_out = NULL;
        *coefficient_out = NULL;
        return false;
    }

    if (expr_match_div_expr(expr, &left, &right)) {
        expr_t *coefficient = NULL;
        expr_t *constant = NULL;

        if (de_expr_contains(right, needle) || !de_linear_decompose(left, needle, &coefficient, &constant))
            return false;

        *coefficient_out = expr_div_simplify_owned(coefficient, expr_clone(right));
        coefficient = NULL;
        *constant_out = expr_div_simplify_owned(constant, expr_clone(right));
        constant = NULL;
        if (*coefficient_out && *constant_out)
            return true;
        expr_free(*constant_out);
        expr_free(*coefficient_out);
        *constant_out = NULL;
        *coefficient_out = NULL;
        return false;
    }

    return false;
}

static bool de_find_derivatives(const expr_t *expr, const expr_t *independent, const expr_t **dependent_out,
                                const expr_t **first_out, const expr_t **second_out, size_t *highest_order_out)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    const expr_t *dependent;
    const expr_t **slot;
    size_t order;

    if (!expr)
        return true;
    if (expr_is_formal_derivative(expr)) {
        order = expr_formal_derivative_order(expr);
        if (order == 0u)
            return false;
        for (size_t i = 0u; i < order; ++i) {
            if (!expr_struct_eq(expr_formal_derivative_wrt_at(expr, i), independent))
                return false;
        }
        dependent = expr_formal_derivative_dependent(expr);
        if (!dependent || (*dependent_out && !expr_struct_eq(*dependent_out, dependent)))
            return false;
        *dependent_out = dependent;
        if (order <= 2u) {
            slot = order == 1u ? first_out : second_out;
            if (*slot && !expr_struct_eq(*slot, expr))
                return false;
            *slot = expr;
        }
        if (order > *highest_order_out)
            *highest_order_out = order;
        return true;
    }

    if (!expr_child_exprs(expr, &left, &right))
        return true;
    return de_find_derivatives(left, independent, dependent_out, first_out, second_out, highest_order_out) &&
           de_find_derivatives(right, independent, dependent_out, first_out, second_out, highest_order_out);
}

bool de_expr_uses(const expr_t *expr, const expr_t *variable)
{
    expr_t *variables[1] = {(expr_t *)variable};
    bool used[1] = {false};

    return expr_collect_var_usage(expr, 1u, variables, used) && used[0];
}

bool de_find_initial_condition(const diffequ_t *de, const expr_t *dependent, const expr_t **point_out,
                               const expr_t **value_out)
{
    for (size_t i = 0u; i < de->condition_count; ++i) {
        const equation_t *condition = de->conditions[i];

        if (condition && de->condition_point_counts[i] == 1u && expr_struct_eq(equ_lhs(condition), dependent)) {
            *point_out = de->condition_points[i][0];
            *value_out = equ_rhs(condition);
            return true;
        }
    }
    return false;
}

bool de_find_derivative_condition(const diffequ_t *de, const expr_t *dependent, const expr_t *independent, size_t order,
                                  const expr_t **point_out, const expr_t **value_out)
{
    for (size_t i = 0u; i < de->condition_count; ++i) {
        const equation_t *condition = de->conditions[i];
        const expr_t *left = condition ? equ_lhs(condition) : NULL;

        if (!left || de->condition_point_counts[i] != 1u || !expr_is_formal_derivative(left) ||
            expr_formal_derivative_order(left) != order ||
            !expr_struct_eq(expr_formal_derivative_dependent(left), dependent))
            continue;

        for (size_t j = 0u; j < order; ++j) {
            if (!expr_struct_eq(expr_formal_derivative_wrt_at(left, j), independent))
                goto next_condition;
        }
        *point_out = de->condition_points[i][0];
        *value_out = equ_rhs(condition);
        return true;

    next_condition:
        continue;
    }
    return false;
}

bool de_has_zero_initial_condition(const diffequ_t *de, const expr_t *dependent)
{
    const expr_t *point = NULL;
    const expr_t *value = NULL;

    return de_find_initial_condition(de, dependent, &point, &value) && expr_is_exact_zero(value);
}

expr_t *de_arbitrary_constant(void)
{
    return expr_new_named_const(NUM_NAN, "C");
}

static bool de_modified_emden_steps(const expr_t *independent, const expr_t *dependent, const expr_t *scale,
                                    char **steps_out, char **steps_tex_out)
{
    const char *x_name = expr_symbol_name(independent);
    char *x = x_name ? strdup(x_name) : NULL;
    char *y = expr_to_string(dependent, style_UNBOUND);
    char *a = expr_to_string(scale, style_UNBOUND);
    char *x_tex = x_name ? strdup(x_name) : NULL;
    char *y_tex = expr_to_tex_body(dependent);
    char *a_tex = expr_to_tex_body(scale);
    char *ay = NULL;
    char *au = NULL;
    char *ay_den = NULL;
    char *au_den = NULL;
    char *ay_tex = NULL;
    char *au_tex = NULL;
    string_t *steps = string_new();
    string_t *steps_tex = string_new();
    if (a && strcmp(a, "1") == 0) {
        ay = y ? strdup(y) : NULL;
        au = strdup("u");
        ay_tex = y_tex ? strdup(y_tex) : NULL;
        au_tex = strdup("u");
        ay_den = y ? strdup(y) : NULL;
        au_den = strdup("u");
    } else {
        int ay_length = a && y ? asprintf(&ay, "%s%s", a, y) : -1;
        int au_length = a ? asprintf(&au, "%su", a) : -1;
        int ay_tex_length = a_tex && y_tex ? asprintf(&ay_tex, "%s%s", a_tex, y_tex) : -1;
        int au_tex_length = a_tex ? asprintf(&au_tex, "%su", a_tex) : -1;

        if (ay_length < 0 || au_length < 0 || ay_tex_length < 0 || au_tex_length < 0) {
            free(ay);
            free(au);
            free(ay_tex);
            free(au_tex);
            ay = NULL;
            au = NULL;
            ay_tex = NULL;
            au_tex = NULL;
        }
        if (ay && au) {
            if (asprintf(&ay_den, "(%s)", ay) < 0)
                ay_den = NULL;
            if (asprintf(&au_den, "(%s)", au) < 0)
                au_den = NULL;
        }
    }
    bool success =
        x && y && a && x_tex && y_tex && a_tex && ay && au && ay_den && au_den && ay_tex && au_tex && steps &&
        steps_tex &&
        string_append_format(steps,
                             "Recognise the modified-Emden rule\n"
                             "      %s″ + 3(%s)%s%s′ + (%s)²%s³ = 0\n"
                             "Set %s = u′/%s. Then\n"
                             "      %s″ + 3(%s)%s%s′ + (%s)²%s³ = "
                             "u‴/%s\n"
                             "so u‴ = 0 and u is quadratic.\n"
                             "Equivalently, the point transformation is\n"
                             "      X = %s − 1/%s\n"
                             "      Y = %s/%s − %s²/2\n"
                             "and d²Y/dX² = 0.",
                             y, a, y, y, a, y, y, au_den, y, a, y, y, a, y, au_den, x, ay_den, x, ay_den, x) >= 0 &&
        string_append_format(steps_tex,
                             "\\begin{aligned}[t]"
                             "\\text{Recognise the rule:}\\quad&%s''+3(%s)%s%s'"
                             "+(%s)^2%s^3=0\\\\"
                             "\\text{Set}\\quad&%s=\\frac{u'}{%s}\\\\"
                             "&%s''+3(%s)%s%s'+(%s)^2%s^3="
                             "\\frac{u'''}{%s}\\\\"
                             "&u'''=0\\\\"
                             "\\text{Point transformation:}\\quad&"
                             "X=%s-\\frac{1}{%s}\\\\"
                             "&Y=\\frac{%s}{%s}-\\frac{%s^2}{2}\\\\"
                             "&\\frac{d^2Y}{dX^2}=0"
                             "\\end{aligned}",
                             y_tex, a_tex, y_tex, y_tex, a_tex, y_tex, y_tex, au_tex, y_tex, a_tex, y_tex, y_tex, a_tex,
                             y_tex, au_tex, x_tex, ay_tex, x_tex, ay_tex, x_tex) >= 0;

    if (success) {
        *steps_out = strdup(string_c_str(steps));
        *steps_tex_out = strdup(string_c_str(steps_tex));
        success = *steps_out && *steps_tex_out;
    }
    if (!success) {
        free(*steps_out);
        free(*steps_tex_out);
        *steps_out = NULL;
        *steps_tex_out = NULL;
    }
    string_free(steps_tex);
    string_free(steps);
    free(au_tex);
    free(ay_tex);
    free(au_den);
    free(ay_den);
    free(au);
    free(ay);
    free(a_tex);
    free(y_tex);
    free(x_tex);
    free(a);
    free(y);
    free(x);
    return success;
}

static bool de_matches_modified_emden_power(const expr_t *dependent, const expr_t *first_derivative,
                                            const expr_t *second_derivative, const expr_t *residual, long power)
{
    expr_t *leading = NULL;
    expr_t *remainder = NULL;
    expr_t *three = NULL;
    expr_t *three_y = NULL;
    expr_t *three_y_y_prime = NULL;
    expr_t *y_power = NULL;
    expr_t *expected_remainder = NULL;
    expr_t *scaled_expected = NULL;
    bool matches = false;

    if (!dependent || !first_derivative || !second_derivative || !residual ||
        !de_linear_decompose(residual, second_derivative, &leading, &remainder))
        goto cleanup;

    three = expr_const_long(3L);
    three_y = three ? expr_mul(three, dependent) : NULL;
    three_y_y_prime = three_y ? expr_mul(three_y, first_derivative) : NULL;
    y_power = expr_pow_long(dependent, power);
    expected_remainder = three_y_y_prime && y_power ? expr_add(three_y_y_prime, y_power) : NULL;
    scaled_expected = leading && expected_remainder
                          ? expr_mul_simplify_owned(expr_clone(leading), expr_clone(expected_remainder))
                          : NULL;
    matches = scaled_expected && expr_struct_eq(remainder, scaled_expected);

cleanup:
    expr_free(scaled_expected);
    expr_free(expected_remainder);
    expr_free(y_power);
    expr_free(three_y_y_prime);
    expr_free(three_y);
    expr_free(three);
    expr_free(remainder);
    expr_free(leading);
    return matches;
}

/*
 * The modified Emden equation
 *
 *     y'' + 3 y y' + y^3 = 0
 *
 * is the logarithmic-derivative image of u''' = 0: setting y = u'/u
 * makes the left hand side u'''/u.  Thus u is quadratic and y is its
 * logarithmic derivative.  The two essential ratios among the three
 * coefficients of u are represented below by C₁ and C₂.
 */
static de_attempt_t de_attempt_modified_emden_linearization(const diffequ_t *de, const expr_t *independent,
                                                            const expr_t *dependent, const expr_t *first_derivative,
                                                            const expr_t *second_derivative, const expr_t *residual,
                                                            equation_t **solutions_out, size_t *solution_count_out,
                                                            expr_t **scale_out)
{
    expr_t *leading = NULL;
    expr_t *remainder = NULL;
    expr_t *mixed_monomial = NULL;
    expr_t *mixed_coefficient = NULL;
    expr_t *after_mixed = NULL;
    expr_t *cubic_coefficient = NULL;
    expr_t *constant_remainder = NULL;
    expr_t *three = NULL;
    expr_t *y_cubed = NULL;
    expr_t *three_leading = NULL;
    expr_t *scale = NULL;
    expr_t *scale_squared = NULL;
    expr_t *expected_cubic = NULL;
    expr_t *coefficient_difference = NULL;
    expr_t *constant_1 = NULL;
    expr_t *constant_2 = NULL;
    expr_t *two = NULL;
    expr_t *two_x = NULL;
    expr_t *numerator = NULL;
    expr_t *x_squared = NULL;
    expr_t *constant_1_x = NULL;
    expr_t *denominator_sum = NULL;
    expr_t *denominator = NULL;
    expr_t *scaled_denominator = NULL;
    expr_t *solution_denominator = NULL;
    expr_t *right = NULL;
    de_attempt_t attempt = DE_ATTEMPT_NOT_MATCHED;

    if (!de || !solutions_out || !solution_count_out || !scale_out || de->condition_count != 0u || !first_derivative ||
        !de_linear_decompose(residual, second_derivative, &leading, &remainder))
        goto cleanup;
    *solution_count_out = 0u;
    *scale_out = NULL;

    mixed_monomial = expr_mul(dependent, first_derivative);
    y_cubed = expr_pow_long(dependent, 3L);
    if (!mixed_monomial || !y_cubed ||
        !de_linear_decompose(remainder, mixed_monomial, &mixed_coefficient, &after_mixed) ||
        !de_linear_decompose(after_mixed, y_cubed, &cubic_coefficient, &constant_remainder) ||
        !expr_is_exact_zero(constant_remainder) || expr_is_exact_zero(leading))
        goto cleanup;

    three = expr_const_long(3L);
    three_leading = three ? expr_mul_simplify_owned(expr_clone(three), expr_clone(leading)) : NULL;
    scale = three_leading ? expr_div_simplify_owned(expr_clone(mixed_coefficient), three_leading) : NULL;
    three_leading = NULL;
    scale_squared = scale ? expr_pow_long(scale, 2L) : NULL;
    expected_cubic = scale_squared ? expr_mul_simplify_owned(scale_squared, expr_clone(leading)) : NULL;
    scale_squared = NULL;
    coefficient_difference =
        expected_cubic ? expr_sub_simplify_owned(expr_clone(cubic_coefficient), expected_cubic) : NULL;
    expected_cubic = NULL;
    if (!scale || expr_is_exact_zero(scale) || de_expr_uses(scale, independent) || de_expr_uses(scale, dependent) ||
        !coefficient_difference || !expr_is_exact_zero(coefficient_difference))
        goto cleanup;

    attempt = DE_ATTEMPT_FAILED;
    constant_1 = expr_new_named_const(NUM_NAN, "C1");
    constant_2 = expr_new_named_const(NUM_NAN, "C2");
    two = expr_const_long(2L);
    two_x = two ? expr_mul(two, independent) : NULL;
    numerator = two_x ? expr_add(two_x, constant_1) : NULL;
    x_squared = expr_pow_long(independent, 2L);
    constant_1_x = constant_1 ? expr_mul(constant_1, independent) : NULL;
    denominator_sum = x_squared && constant_1_x ? expr_add(x_squared, constant_1_x) : NULL;
    denominator = denominator_sum && constant_2 ? expr_add(denominator_sum, constant_2) : NULL;
    scaled_denominator = denominator ? expr_mul_simplify_owned(expr_clone(scale), denominator) : NULL;
    denominator = NULL;
    solution_denominator = scaled_denominator ? expr_clone(scaled_denominator) : NULL;
    right = numerator && solution_denominator ? expr_div(numerator, solution_denominator) : NULL;
    solutions_out[0] = right ? equ_new(dependent, right) : NULL;
    if (solutions_out[0]) {
        *solution_count_out = 1u;
        *scale_out = expr_clone(scale);
        attempt = DE_ATTEMPT_SOLVED;
    }

cleanup:
    if (attempt != DE_ATTEMPT_SOLVED) {
        equ_free(solutions_out ? solutions_out[0] : NULL);
        if (solutions_out)
            solutions_out[0] = NULL;
    }
    expr_free(right);
    expr_free(solution_denominator);
    expr_free(scaled_denominator);
    expr_free(denominator);
    expr_free(denominator_sum);
    expr_free(constant_1_x);
    expr_free(x_squared);
    expr_free(numerator);
    expr_free(two_x);
    expr_free(two);
    expr_free(constant_2);
    expr_free(constant_1);
    expr_free(coefficient_difference);
    expr_free(expected_cubic);
    expr_free(scale_squared);
    expr_free(scale);
    expr_free(three_leading);
    expr_free(y_cubed);
    expr_free(three);
    expr_free(constant_remainder);
    expr_free(cubic_coefficient);
    expr_free(after_mixed);
    expr_free(mixed_coefficient);
    expr_free(mixed_monomial);
    expr_free(remainder);
    expr_free(leading);
    return attempt;
}

diffequ_solve_result_t *de_solve_with_options(const diffequ_t *de, unsigned int options)
{
    const bool include_steps = (options & DE_SOLVE_OPTION_STEPS) != 0u;
    const expr_t *independent;
    const expr_t *first_derivative = NULL;
    const expr_t *second_derivative = NULL;
    const expr_t *dependent = NULL;
    expr_t *operator_dependent = NULL;
    size_t highest_order = 0u;
    expr_t *residual = NULL;
    expr_t *derivative_coefficient = NULL;
    expr_t *remainder = NULL;
    expr_t *negative_remainder = NULL;
    expr_t *derivative_right = NULL;
    char *solution_steps = NULL;
    char *solution_steps_tex = NULL;
    equation_t *solution = NULL;
    equation_t *derivative_quadratic_solutions[3] = {NULL, NULL, NULL};
    equation_t *exact_derivative_solutions[7] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL};
    equation_t *modified_emden_solutions[1] = {NULL};
    equation_t *homogeneous_solutions[2] = {NULL, NULL};
    equation_t *exact_first_order_solutions[2] = {NULL, NULL};
    size_t derivative_quadratic_count = 0u;
    size_t exact_derivative_count = 0u;
    size_t modified_emden_count = 0u;
    size_t homogeneous_count = 0u;
    size_t exact_first_order_count = 0u;
    expr_t *modified_emden_scale = NULL;
    de_attempt_t derivative_quadratic;
    de_attempt_t exact_first_order;
    de_attempt_t exact_derivative;
    de_attempt_t separable;
    de_attempt_t linear;
    de_attempt_t bernoulli;
    de_attempt_t homogeneous;
    de_attempt_t linear_substitution;
    de_attempt_t linear_transformation;
    de_attempt_t sturm_liouville;
    de_attempt_t parameter_linear_pde;
    de_solver_t second_order_solver = DE_SOLVER_STURM_LIOUVILLE;
    diffequ_solve_result_t *result = NULL;

    if (!de)
        return de_solve_result_new(DE_SOLVE_STATUS_INVALID, DE_SOLVER_NONE, "differential equation is NULL");
    if (de->independent_count >= 2u) {
        residual = equ_residual(de->equation);
        result = de->independent_count == 2u ? de_pde_solve_two_variable(de, residual, include_steps)
                                             : de_pde_solve_multi_variable(de, residual, include_steps);
        goto cleanup;
    }
    if (de->independent_count != 1u)
        return de_solve_result_new(DE_SOLVE_STATUS_UNSUPPORTED, DE_SOLVER_NONE,
                                   "the solver requires at least one independent variable");

    independent = de->independent_vars[0];
    if (de->repeated_quadratic_power >= 2u && de->repeated_quadratic_square && de->repeated_quadratic_dependent) {
        operator_dependent = expr_new_named_var(NUM_NAN, de->repeated_quadratic_dependent);
        sturm_liouville = operator_dependent
                              ? de_attempt_repeated_quadratic_operator(de, independent, operator_dependent,
                                                                       de->repeated_quadratic_square,
                                                                       de->repeated_quadratic_power, &solution)
                              : DE_ATTEMPT_FAILED;
        if (sturm_liouville == DE_ATTEMPT_SOLVED) {
            result = de_solve_result_new(DE_SOLVE_STATUS_SOLVED, DE_SOLVER_CONSTANT_COEFFICIENT_LINEAR,
                                         "solved as a repeated quadratic constant-coefficient "
                                         "linear ODE");
            goto append;
        }
        if (sturm_liouville == DE_ATTEMPT_FAILED) {
            result = de_solve_result_new(DE_SOLVE_STATUS_FAILED, DE_SOLVER_NONE,
                                         "failed to complete the repeated quadratic "
                                         "constant-coefficient solution");
            goto cleanup;
        }
    }
    residual = equ_residual(de->equation);
    if (!residual ||
        !de_find_derivatives(residual, independent, &dependent, &first_derivative, &second_derivative,
                             &highest_order) ||
        !dependent) {
        result = de_solve_result_new(DE_SOLVE_STATUS_UNSUPPORTED, DE_SOLVER_NONE,
                                     "the equation is not a supported ordinary differential equation");
        goto cleanup;
    }

    if (highest_order > 2u) {
        exact_derivative = highest_order == 3u ? de_attempt_exact_derivative_linearization(
                                                     de, independent, dependent, first_derivative, second_derivative,
                                                     residual, exact_derivative_solutions, &exact_derivative_count)
                                               : DE_ATTEMPT_NOT_MATCHED;
        if (exact_derivative == DE_ATTEMPT_SOLVED) {
            result = de_solve_result_new(DE_SOLVE_STATUS_SOLVED, DE_SOLVER_EXACT_DERIVATIVE_LINEARIZATION,
                                         "linearized exactly, then solved by a convergent "
                                         "power-series recurrence");
            if (!result)
                goto cleanup;
            for (size_t i = 0u; i < exact_derivative_count; ++i) {
                if (de_solve_result_append(result, exact_derivative_solutions[i]) != 0) {
                    de_solve_result_free(result);
                    result = NULL;
                    goto cleanup;
                }
                exact_derivative_solutions[i] = NULL;
            }
            goto cleanup;
        }
        if (exact_derivative == DE_ATTEMPT_FAILED) {
            result = de_solve_result_new(DE_SOLVE_STATUS_FAILED, DE_SOLVER_NONE,
                                         "failed to complete the exact-derivative linearization");
            goto cleanup;
        }

        sturm_liouville =
            de_attempt_constant_coefficient_linear(de, independent, dependent, highest_order, residual, &solution);
        if (sturm_liouville == DE_ATTEMPT_SOLVED) {
            result = de_solve_result_new(DE_SOLVE_STATUS_SOLVED, DE_SOLVER_CONSTANT_COEFFICIENT_LINEAR,
                                         "solved as a constant-coefficient linear ODE");
            goto append;
        }
        result = de_solve_result_new(
            sturm_liouville == DE_ATTEMPT_FAILED ? DE_SOLVE_STATUS_FAILED : DE_SOLVE_STATUS_UNSUPPORTED, DE_SOLVER_NONE,
            sturm_liouville == DE_ATTEMPT_FAILED ? "failed to complete the constant-coefficient linear solution"
                                                 : "the higher-order equation is not constant-coefficient linear");
        goto cleanup;
    }

    if (second_derivative) {
        linear_transformation = de_attempt_modified_emden_linearization(
            de, independent, dependent, first_derivative, second_derivative, residual, modified_emden_solutions,
            &modified_emden_count, &modified_emden_scale);
        if (linear_transformation == DE_ATTEMPT_SOLVED) {
            char *modified_emden_steps = NULL;
            char *modified_emden_steps_tex = NULL;
            char *scale_text = expr_to_string(modified_emden_scale, style_UNBOUND);
            string_t *diagnostic = string_new();

            if (!scale_text || !diagnostic ||
                string_append_format(diagnostic,
                                     strcmp(scale_text, "1") == 0 ? "linearized by y = u'/u, then solved as u''' = 0"
                                                                  : "linearized by y = u'/(%su), then solved as "
                                                                    "u''' = 0",
                                     scale_text) < 0 ||
                (include_steps && !de_modified_emden_steps(independent, dependent, modified_emden_scale,
                                                           &modified_emden_steps, &modified_emden_steps_tex))) {
                free(scale_text);
                string_free(diagnostic);
                free(modified_emden_steps_tex);
                free(modified_emden_steps);
                goto cleanup;
            }
            result =
                de_solve_result_new(DE_SOLVE_STATUS_SOLVED, DE_SOLVER_LINEAR_TRANSFORMATION, string_c_str(diagnostic));
            if (!result || (include_steps && de_solve_result_set_steps(result, modified_emden_steps) != 0) ||
                (include_steps && de_solve_result_set_steps_tex(result, modified_emden_steps_tex) != 0) ||
                de_solve_result_set_symmetry(result, "SL(3, ℝ)") != 0) {
                de_solve_result_free(result);
                result = NULL;
                free(scale_text);
                string_free(diagnostic);
                free(modified_emden_steps_tex);
                free(modified_emden_steps);
                goto cleanup;
            }
            free(scale_text);
            string_free(diagnostic);
            free(modified_emden_steps_tex);
            free(modified_emden_steps);
            for (size_t i = 0u; i < modified_emden_count; ++i) {
                if (de_solve_result_append(result, modified_emden_solutions[i]) != 0) {
                    de_solve_result_free(result);
                    result = NULL;
                    goto cleanup;
                }
                modified_emden_solutions[i] = NULL;
            }
            goto cleanup;
        }
        if (linear_transformation == DE_ATTEMPT_FAILED) {
            result = de_solve_result_new(DE_SOLVE_STATUS_FAILED, DE_SOLVER_NONE,
                                         "failed to complete the modified-Emden linearization");
            goto cleanup;
        }

        if (de_matches_modified_emden_power(dependent, first_derivative, second_derivative, residual, 4L)) {
            result = de_solve_result_new(DE_SOLVE_STATUS_UNSUPPORTED, DE_SOLVER_NONE,
                                         "not point-linearizable: the Lie–Tressé invariant "
                                         "36y(1 − 2y) is not identically zero");
            goto cleanup;
        }

        sturm_liouville = de_attempt_sturm_liouville(de, independent, dependent, second_derivative, first_derivative,
                                                     residual, &solution, &second_order_solver);
        if (sturm_liouville == DE_ATTEMPT_SOLVED) {
            result = de_solve_result_new(DE_SOLVE_STATUS_SOLVED, second_order_solver,
                                         second_order_solver == DE_SOLVER_POWER_LAW_BESSEL
                                             ? "solved by a power-law reduction to Bessel's equation"
                                             : "solved as a second-order linear Sturm-Liouville equation");
            goto append;
        }
        if (sturm_liouville == DE_ATTEMPT_NOT_MATCHED) {
            sturm_liouville =
                de_attempt_constant_coefficient_linear(de, independent, dependent, highest_order, residual, &solution);
            if (sturm_liouville == DE_ATTEMPT_SOLVED) {
                result = de_solve_result_new(DE_SOLVE_STATUS_SOLVED, DE_SOLVER_CONSTANT_COEFFICIENT_LINEAR,
                                             "solved as a constant-coefficient linear ODE");
                goto append;
            }
        }
        result = de_solve_result_new(
            sturm_liouville == DE_ATTEMPT_FAILED ? DE_SOLVE_STATUS_FAILED : DE_SOLVE_STATUS_UNSUPPORTED, DE_SOLVER_NONE,
            sturm_liouville == DE_ATTEMPT_FAILED
                ? "failed to complete the Sturm-Liouville solution"
                : "the second-order linear equation has no supported closed-form basis");
        goto cleanup;
    }

    if (!first_derivative) {
        result =
            de_solve_result_new(DE_SOLVE_STATUS_UNSUPPORTED, DE_SOLVER_NONE, "the equation has no first derivative");
        goto cleanup;
    }

    derivative_quadratic = de_attempt_derivative_quadratic(de, independent, dependent, first_derivative, residual,
                                                           derivative_quadratic_solutions, &derivative_quadratic_count);
    if (derivative_quadratic == DE_ATTEMPT_SOLVED) {
        result = de_solve_result_new(DE_SOLVE_STATUS_SOLVED, DE_SOLVER_DERIVATIVE_QUADRATIC,
                                     "solved as an autonomous derivative-quadratic ODE");
        if (!result)
            goto cleanup;
        for (size_t i = 0u; i < derivative_quadratic_count; ++i) {
            if (de_solve_result_append(result, derivative_quadratic_solutions[i]) != 0) {
                de_solve_result_free(result);
                result = NULL;
                goto cleanup;
            }
            derivative_quadratic_solutions[i] = NULL;
        }
        goto cleanup;
    }
    if (derivative_quadratic == DE_ATTEMPT_FAILED) {
        result = de_solve_result_new(DE_SOLVE_STATUS_FAILED, DE_SOLVER_NONE,
                                     "failed to complete the derivative-quadratic solution");
        goto cleanup;
    }

    exact_first_order = de_attempt_exact_first_order(de, independent, dependent, first_derivative, residual,
                                                     exact_first_order_solutions, &exact_first_order_count);
    if (exact_first_order == DE_ATTEMPT_SOLVED) {
        result = de_solve_result_new(DE_SOLVE_STATUS_SOLVED, DE_SOLVER_EXACT_FIRST_ORDER,
                                     "solved as an exact first-order differential equation");
        if (!result)
            goto cleanup;
        for (size_t i = 0u; i < exact_first_order_count; ++i) {
            if (de_solve_result_append(result, exact_first_order_solutions[i]) != 0) {
                de_solve_result_free(result);
                result = NULL;
                goto cleanup;
            }
            exact_first_order_solutions[i] = NULL;
        }
        goto cleanup;
    }

    if (!de_linear_decompose(residual, first_derivative, &derivative_coefficient, &remainder)) {
        result = de_solve_result_new(DE_SOLVE_STATUS_UNSUPPORTED, DE_SOLVER_NONE,
                                     "the first derivative could not be isolated");
        goto cleanup;
    }

    negative_remainder = de_simplify_unary_owned(remainder, expr_neg);
    remainder = NULL;
    derivative_right = negative_remainder ? expr_div_simplify_owned(negative_remainder, derivative_coefficient) : NULL;
    if (negative_remainder) {
        negative_remainder = NULL;
        derivative_coefficient = NULL;
    }
    if (!derivative_right) {
        result =
            de_solve_result_new(DE_SOLVE_STATUS_FAILED, DE_SOLVER_NONE, "failed to construct the isolated derivative");
        goto cleanup;
    }

    parameter_linear_pde = de_pde_attempt_parameter_linear(de, independent, dependent, derivative_right, include_steps,
                                                           &solution, &solution_steps, &solution_steps_tex);
    if (parameter_linear_pde == DE_ATTEMPT_SOLVED) {
        result = de_solve_result_new(DE_SOLVE_STATUS_SOLVED, DE_SOLVER_PARAMETER_LINEAR_PDE,
                                     "solved as a parameter-dependent first-order linear PDE");
        if (!result || (include_steps && de_solve_result_set_steps(result, solution_steps) != 0) ||
            (include_steps && de_solve_result_set_steps_tex(result, solution_steps_tex) != 0)) {
            de_solve_result_free(result);
            result = NULL;
            goto cleanup;
        }
        goto append;
    }
    if (parameter_linear_pde == DE_ATTEMPT_FAILED) {
        result = de_solve_result_new(DE_SOLVE_STATUS_FAILED, DE_SOLVER_NONE,
                                     "failed to complete the parameter-dependent linear PDE");
        goto cleanup;
    }

    separable = de_attempt_separable(de, independent, dependent, derivative_right, &solution);
    if (separable == DE_ATTEMPT_SOLVED) {
        result =
            de_solve_result_new(DE_SOLVE_STATUS_SOLVED, DE_SOLVER_SEPARABLE, "solved as a first-order separable ODE");
        goto append;
    }

    linear = de_attempt_linear(de, independent, dependent, derivative_right, &solution);
    if (linear == DE_ATTEMPT_SOLVED) {
        result = de_solve_result_new(DE_SOLVE_STATUS_SOLVED, DE_SOLVER_LINEAR, "solved as a first-order linear ODE");
        goto append;
    }

    bernoulli = de_attempt_bernoulli_square(de, independent, dependent, derivative_right, &solution);
    if (bernoulli == DE_ATTEMPT_SOLVED) {
        result =
            de_solve_result_new(DE_SOLVE_STATUS_SOLVED, DE_SOLVER_BERNOULLI, "solved as a quadratic Bernoulli ODE");
        goto append;
    }

    homogeneous =
        de_attempt_homogeneous(de, independent, dependent, derivative_right, homogeneous_solutions, &homogeneous_count);
    if (homogeneous == DE_ATTEMPT_SOLVED) {
        result = de_solve_result_new(DE_SOLVE_STATUS_SOLVED, DE_SOLVER_HOMOGENEOUS,
                                     "solved as a first-order homogeneous ODE");
        if (!result)
            goto cleanup;
        for (size_t i = 0u; i < homogeneous_count; ++i) {
            if (de_solve_result_append(result, homogeneous_solutions[i]) != 0) {
                de_solve_result_free(result);
                result = NULL;
                goto cleanup;
            }
            homogeneous_solutions[i] = NULL;
        }
        goto cleanup;
    }

    linear_substitution = de_attempt_linear_substitution(de, independent, dependent, derivative_right, &solution);
    if (linear_substitution == DE_ATTEMPT_SOLVED) {
        result = de_solve_result_new(DE_SOLVE_STATUS_SOLVED, DE_SOLVER_LINEAR_SUBSTITUTION,
                                     "solved by a first-order linear substitution");
        goto append;
    }

    linear_transformation = de_attempt_linear_transformation(de, independent, dependent, derivative_right, &solution);
    if (linear_transformation == DE_ATTEMPT_SOLVED) {
        result = de_solve_result_new(DE_SOLVE_STATUS_SOLVED, DE_SOLVER_LINEAR_TRANSFORMATION,
                                     "solved by a linear change of variables");
        goto append;
    }

    result = de_solve_result_new(separable == DE_ATTEMPT_FAILED || exact_first_order == DE_ATTEMPT_FAILED ||
                                         linear == DE_ATTEMPT_FAILED || bernoulli == DE_ATTEMPT_FAILED ||
                                         homogeneous == DE_ATTEMPT_FAILED || linear_substitution == DE_ATTEMPT_FAILED ||
                                         linear_transformation == DE_ATTEMPT_FAILED
                                     ? DE_SOLVE_STATUS_FAILED
                                     : DE_SOLVE_STATUS_UNSUPPORTED,
                                 DE_SOLVER_NONE,
                                 de->differential_form_input && exact_first_order == DE_ATTEMPT_NOT_MATCHED
                                     ? "the differential form is not exact and no other symbolic "
                                       "solver completed the equation"
                                     : "no available symbolic solver completed the equation");
    goto cleanup;

append:
    if (!result || de_solve_result_append(result, solution) != 0) {
        equ_free(solution);
        de_solve_result_free(result);
        result = NULL;
    }
    solution = NULL;

cleanup:
    free(solution_steps_tex);
    free(solution_steps);
    for (size_t i = 0u; i < 2u; ++i)
        equ_free(exact_first_order_solutions[i]);
    for (size_t i = 0u; i < 2u; ++i)
        equ_free(homogeneous_solutions[i]);
    for (size_t i = 0u; i < 1u; ++i)
        equ_free(modified_emden_solutions[i]);
    for (size_t i = 0u; i < 7u; ++i)
        equ_free(exact_derivative_solutions[i]);
    for (size_t i = 0u; i < 3u; ++i)
        equ_free(derivative_quadratic_solutions[i]);
    equ_free(solution);
    expr_free(operator_dependent);
    expr_free(modified_emden_scale);
    expr_free(derivative_right);
    expr_free(negative_remainder);
    expr_free(remainder);
    expr_free(derivative_coefficient);
    expr_free(residual);
    if (include_steps && result && de_solve_result_ensure_rule_steps(de, result) != 0) {
        de_solve_result_free(result);
        result = NULL;
    }
    return result;
}

diffequ_solve_result_t *de_solve(const diffequ_t *de)
{
    return de_solve_with_options(de, DE_SOLVE_OPTION_NONE);
}
