#include <stdlib.h>
#include <string.h>

#define MARS_DIFFEQUATION_PDE_INTERNAL_ACCESS
#include "diffequ_pde_internal.h"

static bool de_pde_collect_dependent_derivatives(const expr_t *expression, const expr_t *dependent,
                                                 const expr_t **derivatives, size_t *count, size_t capacity)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;

    if (!expression)
        return true;
    if (expr_is_formal_derivative(expression) &&
        de_pde_same_symbolic_form(expr_formal_derivative_dependent(expression), dependent)) {
        for (size_t i = 0u; i < *count; ++i)
            if (expr_struct_eq(derivatives[i], expression))
                return true;
        if (*count >= capacity)
            return false;
        derivatives[(*count)++] = expression;
        return true;
    }
    if (!expr_child_exprs(expression, &left, &right))
        return true;
    return de_pde_collect_dependent_derivatives(left, dependent, derivatives, count, capacity) &&
           de_pde_collect_dependent_derivatives(right, dependent, derivatives, count, capacity);
}

static expr_t *de_pde_apply_spatial_operator(const expr_t *operator_expression, const expr_t *dependent,
                                             const expr_t *initial)
{
    expr_t *applied = expr_substitute(operator_expression, dependent, initial);

    if (applied) {
        expr_t *simplified = expr_simplify(applied);

        expr_free(applied);
        applied = simplified;
    }
    return applied;
}

static bool de_pde_stationary_rate_matches_at_point(const expr_t *rate_expression, const diffequ_t *de,
                                                    size_t time_index, const expr_t *expected_rate, size_t sample_index)
{
    expr_t *sample = expr_clone(rate_expression);
    expr_t *difference_raw = NULL;
    expr_t *difference = NULL;
    bool matches = false;

    for (size_t i = 0u; sample && i < de->independent_count; ++i) {
        expr_t *point;
        expr_t *substituted;
        long coordinate;

        if (i == time_index)
            continue;
        coordinate = 1L + (long)((sample_index + 2u * i) % 3u);
        point = expr_const_long(coordinate);
        substituted = point ? expr_substitute(sample, de->independent_vars[i], point) : NULL;
        expr_free(point);
        expr_free(sample);
        sample = substituted;
    }
    if (sample) {
        expr_t *simplified = expr_simplify(sample);

        expr_free(sample);
        sample = simplified;
    }
    difference_raw = sample ? expr_sub(sample, expected_rate) : NULL;
    difference = difference_raw ? expr_simplify(difference_raw) : NULL;
    matches = difference && expr_is_exact_zero(difference);
    if (!matches && difference) {
        number_t numerical_difference = expr_eval(difference);
        number_t magnitude = num_abs(numerical_difference);

        matches = num_is_finite(magnitude) && num_to_double(magnitude) < 1.0e-20;
        num_destroy(&magnitude);
        num_destroy(&numerical_difference);
    }

    expr_free(difference);
    expr_free(difference_raw);
    expr_free(sample);
    return matches;
}

static const expr_t *de_pde_initial_time_point(const diffequ_t *de, size_t time_index)
{
    const expr_t *time_point = NULL;

    for (size_t point_index = 0u; point_index < de->condition_point_counts[0]; ++point_index) {
        const expr_t *point = de->condition_points[0][point_index];
        bool is_free_spatial_coordinate = false;

        for (size_t independent_index = 0u; independent_index < de->independent_count; ++independent_index) {
            if (independent_index != time_index &&
                de_pde_same_symbolic_form(point, de->independent_vars[independent_index])) {
                is_free_spatial_coordinate = true;
                break;
            }
        }
        if (is_free_spatial_coordinate)
            continue;
        if (time_point)
            return NULL;
        time_point = point;
    }
    return time_point;
}

static bool de_pde_verify_stationary_rate(const expr_t *rate_expression, const diffequ_t *de, size_t time_index,
                                          const expr_t *expected_rate)
{
    size_t sample_count = 2u * de->independent_count + 3u;

    if (!rate_expression || !de || !expected_rate)
        return false;
    for (size_t sample = 0u; sample < sample_count; ++sample)
        if (!de_pde_stationary_rate_matches_at_point(rate_expression, de, time_index, expected_rate, sample))
            return false;
    return true;
}

static bool de_pde_set_stationary_steps(diffequ_solve_result_t *result, const expr_t *dependent,
                                        const expr_t *time_coordinate, const expr_t *time_point,
                                        const expr_t *time_coefficient, const expr_t *spatial_operator,
                                        const expr_t *initial, const expr_t *rate, const equation_t *solution)
{
    char *u = expr_to_string(dependent, style_UNBOUND);
    char *t = expr_to_string(time_coordinate, style_UNBOUND);
    char *t0 = expr_to_string(time_point, style_UNBOUND);
    char *a = expr_to_string(time_coefficient, style_UNBOUND);
    char *h = expr_to_string(spatial_operator, style_UNBOUND);
    char *u0 = expr_to_string(initial, style_UNBOUND);
    char *lambda = expr_to_string(rate, style_UNBOUND);
    char *u_TeX = expr_to_string(dependent, style_LATEX);
    char *t_TeX = expr_to_string(time_coordinate, style_LATEX);
    char *t0_TeX = expr_to_string(time_point, style_LATEX);
    char *a_TeX = expr_to_string(time_coefficient, style_LATEX);
    char *h_TeX = expr_to_string(spatial_operator, style_LATEX);
    char *u0_TeX = expr_to_string(initial, style_LATEX);
    char *lambda_TeX = expr_to_string(rate, style_LATEX);
    string_t *solution_text = equ_to_text(solution, style_UNBOUND);
    string_t *solution_TeX = equ_to_text(solution, style_LATEX);
    string_t *steps = string_new();
    string_t *steps_TeX = string_new();
    bool success =
        u && t && t0 && a && h && u0 && lambda && u_TeX && t_TeX && t0_TeX && a_TeX && h_TeX && u0_TeX && lambda_TeX &&
        solution_text && solution_TeX && steps && steps_TeX &&
        string_append_format(steps,
                             "Separate the evolution derivative from the spatial operator:\n"
                             "      A·∂%s/∂%s + H[%s] = 0\n"
                             "      A = %s,    H[%s] = %s\n"
                             "Use the supplied initial state at %s = %s:\n"
                             "      %s₀ = %s\n"
                             "Apply H and divide by A%s₀. The derived constant rate is\n"
                             "      λ = −H[%s₀]/(A%s₀) = %s\n"
                             "Hence %s = %s₀·exp(λ(%s − %s)):\n"
                             "      %s",
                             u, t, u, a, u, h, t, t0, u, u0, u, u, u, lambda, u, u, t, t0,
                             string_c_str(solution_text)) >= 0 &&
        string_append_format(steps_TeX,
                             "\\begin{aligned}[t]"
                             "A\\frac{\\partial %s}{\\partial %s}+H[%s]&=0\\\\"
                             "A&=%s,\\qquad H[%s]=%s\\\\"
                             "%s_0&=%s,\\qquad %s=%s\\\\"
                             "\\lambda&=-\\frac{H[%s_0]}{A%s_0}=%s\\\\"
                             "%s&=%s_0e^{\\lambda(%s-%s)}\\\\"
                             "&%s"
                             "\\end{aligned}",
                             u_TeX, t_TeX, u_TeX, a_TeX, u_TeX, h_TeX, u_TeX, u0_TeX, t_TeX, t0_TeX, u_TeX, u_TeX,
                             lambda_TeX, u_TeX, u_TeX, t_TeX, t0_TeX, string_c_str(solution_TeX)) >= 0 &&
        de_solve_result_set_steps(result, string_c_str(steps)) == 0 &&
        de_solve_result_set_steps_TeX(result, string_c_str(steps_TeX)) == 0;

    string_free(steps_TeX);
    string_free(steps);
    string_free(solution_TeX);
    string_free(solution_text);
    free(lambda_TeX);
    free(u0_TeX);
    free(h_TeX);
    free(a_TeX);
    free(t0_TeX);
    free(t_TeX);
    free(u_TeX);
    free(lambda);
    free(u0);
    free(h);
    free(a);
    free(t0);
    free(t);
    free(u);
    return success;
}

static diffequ_solve_result_t *de_pde_solve_stationary_eigenfunction(const diffequ_t *de, const expr_t *residual,
                                                                     bool include_steps)
{
    const expr_t *dependent = NULL;
    const expr_t *time_derivative = NULL;
    const expr_t *initial;
    const expr_t *initial_time;
    size_t time_index = 0u;
    expr_t *time_coefficient = NULL;
    expr_t *spatial_operator = NULL;
    expr_t *applied_operator = NULL;
    expr_t *denominator = NULL;
    expr_t *rate_raw = NULL;
    expr_t *rate = NULL;
    expr_t *rate_sample = NULL;
    expr_t *time_offset = NULL;
    expr_t *phase_exponent = NULL;
    expr_t *phase = NULL;
    expr_t *right = NULL;
    equation_t *solution = NULL;
    diffequ_solve_result_t *result = NULL;

    if (!de || !residual || de->condition_count != 1u || de->condition_point_counts[0] != de->independent_count)
        goto cleanup;
    for (size_t i = 0u; i < de->independent_count; ++i) {
        const expr_t *candidate = NULL;
        size_t derivative_count = 0u;
        const expr_t *derivatives[64];

        if (!de_pde_collect_dependent_derivatives(residual, equ_lhs(de->conditions[0]), derivatives, &derivative_count,
                                                  64u))
            goto cleanup;
        for (size_t j = 0u; j < derivative_count; ++j)
            if (expr_formal_derivative_order(derivatives[j]) == 1u &&
                expr_struct_eq(expr_formal_derivative_wrt_at(derivatives[j], 0u), de->independent_vars[i])) {
                candidate = derivatives[j];
                break;
            }
        if (candidate) {
            dependent = expr_formal_derivative_dependent(candidate);
            time_derivative = candidate;
            time_index = i;
            break;
        }
    }
    if (!dependent || !time_derivative || !de_pde_same_symbolic_form(equ_lhs(de->conditions[0]), dependent) ||
        !de_linear_decompose(residual, time_derivative, &time_coefficient, &spatial_operator) ||
        expr_is_exact_zero(time_coefficient))
        goto cleanup;
    initial = equ_rhs(de->conditions[0]);
    initial_time = de_pde_initial_time_point(de, time_index);
    if (!initial_time)
        goto cleanup;
    applied_operator = de_pde_apply_spatial_operator(spatial_operator, dependent, initial);
    denominator = expr_mul_simplify_owned(expr_clone(time_coefficient), expr_clone(initial));
    rate_raw = applied_operator && denominator
                   ? expr_div_simplify_owned(expr_negate_owned(applied_operator), denominator)
                   : NULL;
    applied_operator = NULL;
    denominator = NULL;
    rate_sample = expr_clone(rate_raw);
    for (size_t i = 0u; rate_sample && i < de->independent_count; ++i) {
        expr_t *point;
        expr_t *substituted;

        if (i == time_index)
            continue;
        point = expr_const_long(i == 0u || (time_index == 0u && i == 1u) ? 1L : 0L);
        substituted = point ? expr_substitute(rate_sample, de->independent_vars[i], point) : NULL;
        expr_free(point);
        expr_free(rate_sample);
        rate_sample = substituted;
    }
    rate = rate_sample ? expr_simplify(rate_sample) : NULL;
    if (!rate || !de_pde_verify_stationary_rate(rate_raw, de, time_index, rate))
        goto cleanup;
    for (size_t i = 0u; i < de->independent_count; ++i)
        if (de_expr_uses(rate, de->independent_vars[i]))
            goto cleanup;
    time_offset = expr_sub_simplify_owned(expr_clone(de->independent_vars[time_index]), expr_clone(initial_time));
    phase_exponent = time_offset ? expr_mul_simplify_owned(expr_clone(rate), time_offset) : NULL;
    time_offset = NULL;
    phase = phase_exponent ? expr_exp(phase_exponent) : NULL;
    right = phase ? expr_mul_simplify_owned(expr_clone(initial), phase) : NULL;
    phase = NULL;
    solution = right ? equ_new(dependent, right) : NULL;
    result = solution ? de_solve_result_new(DE_SOLVE_STATUS_SOLVED, DE_SOLVER_STATIONARY_EIGENFUNCTION,
                                            "solved by the stationary-eigenfunction evolution rule")
                      : NULL;
    if (!result || de_solve_result_append(result, solution) != 0) {
        de_solve_result_free(result);
        result = NULL;
        goto cleanup;
    }
    solution = NULL;
    if (include_steps &&
        !de_pde_set_stationary_steps(result, dependent, de->independent_vars[time_index], initial_time,
                                     time_coefficient, spatial_operator, initial, rate, result->solutions[0])) {
        de_solve_result_free(result);
        result = NULL;
        goto cleanup;
    }

cleanup:
    equ_free(solution);
    expr_free(right);
    expr_free(phase);
    expr_free(phase_exponent);
    expr_free(time_offset);
    expr_free(rate);
    expr_free(rate_sample);
    expr_free(rate_raw);
    expr_free(denominator);
    expr_free(applied_operator);
    expr_free(spatial_operator);
    expr_free(time_coefficient);
    return result;
}

static bool de_pde_cartesian_laplace_steps(const diffequ_t *de, const equation_t *solution, char **steps_out,
                                           char **steps_TeX_out)
{
    const expr_t *dependent = solution ? equ_lhs(solution) : NULL;
    const expr_t *right = solution ? equ_rhs(solution) : NULL;
    const expr_t *first_coordinate;
    const expr_t *second_coordinate;
    expr_t *imaginary_unit = NULL;
    expr_t *first_coordinate_symbol = NULL;
    expr_t *second_coordinate_symbol = NULL;
    expr_t *imaginary_second = NULL;
    expr_t *positive_coordinate = NULL;
    expr_t *negative_coordinate = NULL;
    expr_t *first_derivative_raw = NULL;
    expr_t *first_derivative = NULL;
    expr_t *first_second_derivative_raw = NULL;
    expr_t *first_second_derivative = NULL;
    expr_t *second_derivative_raw = NULL;
    expr_t *second_derivative = NULL;
    expr_t *second_second_derivative_raw = NULL;
    expr_t *second_second_derivative = NULL;
    expr_t *laplacian_raw = NULL;
    expr_t *laplacian = NULL;
    char *dependent_text = NULL;
    char *first_coordinate_text = NULL;
    char *second_coordinate_text = NULL;
    char *positive_coordinate_text = NULL;
    char *negative_coordinate_text = NULL;
    char *right_text = NULL;
    char *first_second_derivative_text = NULL;
    char *second_second_derivative_text = NULL;
    char *dependent_TeX = NULL;
    char *first_coordinate_TeX = NULL;
    char *second_coordinate_TeX = NULL;
    char *positive_coordinate_TeX = NULL;
    char *negative_coordinate_TeX = NULL;
    char *right_TeX = NULL;
    char *first_second_derivative_TeX = NULL;
    char *second_second_derivative_TeX = NULL;
    string_t *steps = NULL;
    string_t *steps_TeX = NULL;
    bool success = false;

    if (steps_out)
        *steps_out = NULL;
    if (steps_TeX_out)
        *steps_TeX_out = NULL;
    if (!de || de->independent_count != 2u || !dependent || !right || !steps_out || !steps_TeX_out)
        return false;
    first_coordinate = de->independent_vars[0];
    second_coordinate = de->independent_vars[1];
    if (!first_coordinate || !second_coordinate || !expr_symbol_name(first_coordinate) ||
        !expr_symbol_name(second_coordinate))
        return false;

    imaginary_unit = expr_new_named_const(NUM_I, "i");
    first_coordinate_symbol = expr_new_named_const(NUM_NAN, expr_symbol_name(first_coordinate));
    second_coordinate_symbol = expr_new_named_const(NUM_NAN, expr_symbol_name(second_coordinate));
    imaginary_second =
        imaginary_unit ? expr_mul_simplify_owned(expr_clone(imaginary_unit), expr_clone(second_coordinate)) : NULL;
    positive_coordinate = imaginary_second ? expr_add(first_coordinate, imaginary_second) : NULL;
    negative_coordinate = imaginary_second ? expr_sub(first_coordinate, imaginary_second) : NULL;
    first_derivative_raw = expr_create_deriv(right, first_coordinate);
    first_derivative = first_derivative_raw ? expr_simplify(first_derivative_raw) : NULL;
    first_second_derivative_raw = first_derivative ? expr_create_deriv(first_derivative, first_coordinate) : NULL;
    first_second_derivative = first_second_derivative_raw ? expr_simplify(first_second_derivative_raw) : NULL;
    second_derivative_raw = expr_create_deriv(right, second_coordinate);
    second_derivative = second_derivative_raw ? expr_simplify(second_derivative_raw) : NULL;
    second_second_derivative_raw = second_derivative ? expr_create_deriv(second_derivative, second_coordinate) : NULL;
    second_second_derivative = second_second_derivative_raw ? expr_simplify(second_second_derivative_raw) : NULL;
    laplacian_raw = first_second_derivative && second_second_derivative
                        ? expr_add(first_second_derivative, second_second_derivative)
                        : NULL;
    laplacian = laplacian_raw ? expr_simplify(laplacian_raw) : NULL;
    if (!positive_coordinate || !negative_coordinate || !first_derivative || !first_second_derivative ||
        !second_derivative || !second_second_derivative || !laplacian || !expr_is_exact_zero(laplacian))
        goto cleanup;

#define DE_LAPLACE_TEXT(name, expression) name = expr_to_string((expression), style_UNBOUND)
#define DE_LAPLACE_LATEX(name, expression) name = expr_to_TeX_body((expression))
    DE_LAPLACE_TEXT(dependent_text, dependent);
    first_coordinate_text = strdup(expr_symbol_name(first_coordinate));
    second_coordinate_text = strdup(expr_symbol_name(second_coordinate));
    DE_LAPLACE_TEXT(positive_coordinate_text, positive_coordinate);
    DE_LAPLACE_TEXT(negative_coordinate_text, negative_coordinate);
    DE_LAPLACE_TEXT(right_text, right);
    DE_LAPLACE_TEXT(first_second_derivative_text, first_second_derivative);
    DE_LAPLACE_TEXT(second_second_derivative_text, second_second_derivative);
    DE_LAPLACE_LATEX(dependent_TeX, dependent);
    DE_LAPLACE_LATEX(first_coordinate_TeX, first_coordinate_symbol);
    DE_LAPLACE_LATEX(second_coordinate_TeX, second_coordinate_symbol);
    DE_LAPLACE_LATEX(positive_coordinate_TeX, positive_coordinate);
    DE_LAPLACE_LATEX(negative_coordinate_TeX, negative_coordinate);
    DE_LAPLACE_LATEX(right_TeX, right);
    DE_LAPLACE_LATEX(first_second_derivative_TeX, first_second_derivative);
    DE_LAPLACE_LATEX(second_second_derivative_TeX, second_second_derivative);
#undef DE_LAPLACE_LATEX
#undef DE_LAPLACE_TEXT
    if (!dependent_text || !first_coordinate_text || !second_coordinate_text || !positive_coordinate_text ||
        !negative_coordinate_text || !right_text || !first_second_derivative_text || !second_second_derivative_text ||
        !dependent_TeX || !first_coordinate_TeX || !second_coordinate_TeX || !positive_coordinate_TeX ||
        !negative_coordinate_TeX || !right_TeX || !first_second_derivative_TeX || !second_second_derivative_TeX)
        goto cleanup;

    steps = string_new();
    steps_TeX = string_new();
    if (!steps || !steps_TeX ||
        string_append_format(steps,
                             "Use the parsed coordinates to form:\n"
                             "      z = %s,    w = %s\n"
                             "Construct the harmonic family:\n"
                             "      %s = %s\n"
                             "Compute the required second partials with MARSlib:\n"
                             "      ∂²%s/∂%s² = %s\n"
                             "      ∂²%s/∂%s² = %s\n"
                             "Substitute the computed derivatives:\n"
                             "      Δ%s = ∂²%s/∂%s² + ∂²%s/∂%s²\n"
                             "           = (%s) + (%s)\n"
                             "           = 0",
                             positive_coordinate_text, negative_coordinate_text, dependent_text, right_text,
                             dependent_text, first_coordinate_text, first_second_derivative_text, dependent_text,
                             second_coordinate_text, second_second_derivative_text, dependent_text, dependent_text,
                             first_coordinate_text, dependent_text, second_coordinate_text,
                             first_second_derivative_text, second_second_derivative_text) < 0 ||
        string_append_format(steps_TeX,
                             "\\begin{aligned}[t]"
                             "\\text{From the parsed coordinates,}\\quad z&=%s\\\\"
                             "w&=%s\\\\[4pt]"
                             "%s&=%s\\\\[6pt]"
                             "\\frac{\\partial^2 %s}{\\partial %s^2}&=%s\\\\"
                             "\\frac{\\partial^2 %s}{\\partial %s^2}&=%s\\\\[6pt]"
                             "\\Delta %s"
                             "&=\\frac{\\partial^2 %s}{\\partial %s^2}"
                             "+\\frac{\\partial^2 %s}{\\partial %s^2}\\\\"
                             "&=\\left(%s\\right)\\\\"
                             "&\\quad+\\left(%s\\right)\\\\"
                             "&=0"
                             "\\end{aligned}",
                             positive_coordinate_TeX, negative_coordinate_TeX, dependent_TeX, right_TeX, dependent_TeX,
                             first_coordinate_TeX, first_second_derivative_TeX, dependent_TeX, second_coordinate_TeX,
                             second_second_derivative_TeX, dependent_TeX, dependent_TeX, first_coordinate_TeX,
                             dependent_TeX, second_coordinate_TeX, first_second_derivative_TeX,
                             second_second_derivative_TeX) < 0)
        goto cleanup;

    *steps_out = strdup(string_c_str(steps));
    *steps_TeX_out = strdup(string_c_str(steps_TeX));
    success = *steps_out && *steps_TeX_out;

cleanup:
    if (!success) {
        free(*steps_TeX_out);
        free(*steps_out);
        *steps_TeX_out = NULL;
        *steps_out = NULL;
    }
    string_free(steps_TeX);
    string_free(steps);
    free(second_second_derivative_TeX);
    free(first_second_derivative_TeX);
    free(right_TeX);
    free(negative_coordinate_TeX);
    free(positive_coordinate_TeX);
    free(second_coordinate_TeX);
    free(first_coordinate_TeX);
    free(dependent_TeX);
    free(second_second_derivative_text);
    free(first_second_derivative_text);
    free(right_text);
    free(negative_coordinate_text);
    free(positive_coordinate_text);
    free(second_coordinate_text);
    free(first_coordinate_text);
    free(dependent_text);
    expr_free(laplacian);
    expr_free(laplacian_raw);
    expr_free(second_second_derivative);
    expr_free(second_second_derivative_raw);
    expr_free(second_derivative);
    expr_free(second_derivative_raw);
    expr_free(first_second_derivative);
    expr_free(first_second_derivative_raw);
    expr_free(first_derivative);
    expr_free(first_derivative_raw);
    expr_free(negative_coordinate);
    expr_free(positive_coordinate);
    expr_free(imaginary_second);
    expr_free(second_coordinate_symbol);
    expr_free(first_coordinate_symbol);
    expr_free(imaginary_unit);
    return success;
}

diffequ_solve_result_t *de_pde_solve_two_variable(const diffequ_t *de, const expr_t *residual, bool include_steps)
{
    static const char polar_laplace_steps[] = "Set the complex polar coordinates:\n"
                                              "      z = r·exp(iθ),    w = r·exp(−iθ)\n"
                                              "Their derivatives are:\n"
                                              "      z_r = exp(iθ),    z_rr = 0,    z_θ = iz,    "
                                              "z_θθ = −z\n"
                                              "      w_r = exp(−iθ),   w_rr = 0,    w_θ = −iw,   "
                                              "w_θθ = −w\n"
                                              "For u = F(z) + G(w), the chain rule gives:\n"
                                              "      u_r = exp(iθ)F'(z) + exp(−iθ)G'(w)\n"
                                              "      u_rr = exp(2iθ)F''(z) + exp(−2iθ)G''(w)\n"
                                              "      u_θθ = −zF'(z) − z²F''(z) − wG'(w) − "
                                              "w²G''(w)\n"
                                              "Substitute these into the polar Laplacian:\n"
                                              "      Δu = u_rr + u_r/r + u_θθ/r²\n"
                                              "The F' and G' terms cancel because z/r = exp(iθ) and "
                                              "w/r = exp(−iθ).\n"
                                              "The F'' and G'' terms cancel because z²/r² = exp(2iθ) "
                                              "and w²/r² = exp(−2iθ).\n"
                                              "Therefore Δu = 0, and the general harmonic family is:\n"
                                              "      u(r, θ) = F(r·exp(iθ)) + G(r·exp(−iθ))";
    static const char polar_laplace_steps_TeX[] = "\\begin{aligned}[t]"
                                                  "\\text{Set}\\quad z&=r e^{i\\theta},"
                                                  "\\qquad w=r e^{-i\\theta}\\\\"
                                                  "u&=F(z)+G(w)\\\\[6pt]"
                                                  "z_r&=e^{i\\theta},\\qquad z_{rr}=0\\\\"
                                                  "z_\\theta&=iz,\\qquad z_{\\theta\\theta}=-z\\\\"
                                                  "w_r&=e^{-i\\theta},\\qquad w_{rr}=0\\\\"
                                                  "w_\\theta&=-iw,\\qquad w_{\\theta\\theta}=-w"
                                                  "\\\\[6pt]"
                                                  "u_r&=e^{i\\theta}F'(z)+e^{-i\\theta}G'(w)\\\\"
                                                  "u_{rr}&=e^{2i\\theta}F''(z)+e^{-2i\\theta}G''(w)"
                                                  "\\\\"
                                                  "u_{\\theta\\theta}"
                                                  "&=-zF'(z)-z^2F''(z)-wG'(w)-w^2G''(w)\\\\[6pt]"
                                                  "\\Delta u"
                                                  "&=u_{rr}+\\frac{1}{r}u_r+\\frac{1}{r^2}"
                                                  "u_{\\theta\\theta}\\\\"
                                                  "&=\\left(e^{2i\\theta}-\\frac{z^2}{r^2}\\right)F''(z)"
                                                  "\\\\"
                                                  "&\\quad+\\left(\\frac{e^{i\\theta}}{r}"
                                                  "-\\frac{z}{r^2}\\right)F'(z)\\\\"
                                                  "&\\quad+\\left(e^{-2i\\theta}-\\frac{w^2}{r^2}"
                                                  "\\right)G''(w)"
                                                  "\\\\"
                                                  "&\\quad+\\left(\\frac{e^{-i\\theta}}{r}"
                                                  "-\\frac{w}{r^2}\\right)G'(w)\\\\"
                                                  "&=0,\\qquad\\text{since }\\frac{z}{r}=e^{i\\theta}"
                                                  "\\text{ and }\\frac{w}{r}=e^{-i\\theta}.\\\\[8pt]"
                                                  "\\text{Therefore,}\\quad u(r,\\theta)"
                                                  "&=F\\left(r e^{i\\theta}\\right)"
                                                  "+G\\left(r e^{-i\\theta}\\right)"
                                                  "\\end{aligned}";
    equation_t *laplace_solution = NULL;
    equation_t *polar_laplace_solution = NULL;
    equation_t *transport_solution = NULL;
    equation_t *characteristic_solutions[2] = {NULL, NULL};
    size_t characteristic_solution_count = 0u;
    bool transport_recognized = false;
    de_attempt_t transport;
    de_attempt_t characteristics;
    de_attempt_t laplace;
    de_attempt_t polar_laplace;
    diffequ_solve_result_t *result = NULL;
    char *laplace_steps = NULL;
    char *laplace_steps_TeX = NULL;
    diffequ_solve_result_t *stationary = NULL;

    polar_laplace = residual ? de_pde_attempt_polar_laplace(de, residual, &polar_laplace_solution) : DE_ATTEMPT_FAILED;
    if (polar_laplace == DE_ATTEMPT_SOLVED) {
        result = de_solve_result_new(DE_SOLVE_STATUS_SOLVED, DE_SOLVER_LAPLACE,
                                     "solved as the polar form of the two-dimensional "
                                     "Laplace equation");
        if (!result || (include_steps && de_solve_result_set_steps(result, polar_laplace_steps) != 0) ||
            (include_steps && de_solve_result_set_steps_TeX(result, polar_laplace_steps_TeX) != 0) ||
            de_solve_result_append(result, polar_laplace_solution) != 0) {
            de_solve_result_free(result);
            result = NULL;
            goto cleanup;
        }
        polar_laplace_solution = NULL;
        goto cleanup;
    }

    laplace = residual ? de_pde_attempt_laplace(de, residual, &laplace_solution) : DE_ATTEMPT_FAILED;
    if (laplace == DE_ATTEMPT_SOLVED) {
        if (include_steps &&
            !de_pde_cartesian_laplace_steps(de, laplace_solution, &laplace_steps, &laplace_steps_TeX)) {
            result = NULL;
            goto cleanup;
        }
        result = de_solve_result_new(DE_SOLVE_STATUS_SOLVED, DE_SOLVER_LAPLACE,
                                     "solved as the two-dimensional Laplace equation");
        if (!result || (include_steps && de_solve_result_set_steps(result, laplace_steps) != 0) ||
            (include_steps && de_solve_result_set_steps_TeX(result, laplace_steps_TeX) != 0) ||
            de_solve_result_append(result, laplace_solution) != 0) {
            de_solve_result_free(result);
            result = NULL;
            goto cleanup;
        }
        laplace_solution = NULL;
        goto cleanup;
    }

    transport = residual ? de_pde_attempt_constant_transport(de, residual, &transport_solution, &transport_recognized)
                         : DE_ATTEMPT_FAILED;
    if (transport == DE_ATTEMPT_SOLVED) {
        result = de_solve_result_new(DE_SOLVE_STATUS_SOLVED, DE_SOLVER_CONSTANT_COEFFICIENT_TRANSPORT,
                                     "solved by the method of characteristics");
        if (!result || de_solve_result_append(result, transport_solution) != 0) {
            de_solve_result_free(result);
            result = NULL;
            goto cleanup;
        }
        transport_solution = NULL;
        goto cleanup;
    }

    characteristics = residual ? de_pde_attempt_characteristics(de, residual, characteristic_solutions,
                                                                &characteristic_solution_count)
                               : DE_ATTEMPT_FAILED;
    if (characteristics == DE_ATTEMPT_SOLVED) {
        result = de_solve_result_new(DE_SOLVE_STATUS_SOLVED, DE_SOLVER_CHARACTERISTICS,
                                     "solved by the method of characteristics");
        if (!result)
            goto cleanup;
        for (size_t i = 0u; i < characteristic_solution_count; ++i) {
            if (de_solve_result_append(result, characteristic_solutions[i]) != 0) {
                de_solve_result_free(result);
                result = NULL;
                goto cleanup;
            }
            characteristic_solutions[i] = NULL;
        }
        goto cleanup;
    }

    stationary = de_pde_solve_stationary_eigenfunction(de, residual, include_steps);
    if (stationary) {
        result = stationary;
        goto cleanup;
    }

    result = de_solve_result_new(polar_laplace == DE_ATTEMPT_FAILED || laplace == DE_ATTEMPT_FAILED ||
                                         transport == DE_ATTEMPT_FAILED || characteristics == DE_ATTEMPT_FAILED
                                     ? DE_SOLVE_STATUS_FAILED
                                     : DE_SOLVE_STATUS_UNSUPPORTED,
                                 DE_SOLVER_NONE,
                                 polar_laplace == DE_ATTEMPT_FAILED
                                     ? "failed to complete the polar Laplace-equation solution"
                                 : laplace == DE_ATTEMPT_FAILED ? "failed to complete the Laplace-equation solution"
                                 : transport == DE_ATTEMPT_FAILED || characteristics == DE_ATTEMPT_FAILED
                                     ? "failed to complete the PDE characteristic solution"
                                 : transport_recognized ? "the transport equation boundary data is unsupported"
                                                        : "no available symbolic PDE solver matched the equation");

cleanup:
    free(laplace_steps_TeX);
    free(laplace_steps);
    for (size_t i = 0u; i < 2u; ++i)
        equ_free(characteristic_solutions[i]);
    equ_free(transport_solution);
    equ_free(laplace_solution);
    equ_free(polar_laplace_solution);
    return result;
}

diffequ_solve_result_t *de_pde_solve_multi_variable(const diffequ_t *de, const expr_t *residual, bool include_steps)
{
    diffequ_solve_result_t *stationary = NULL;
    equation_t *solution = NULL;
    bool recognized = false;
    de_attempt_t attempt =
        residual ? de_pde_attempt_constant_transport_n(de, residual, &solution, &recognized) : DE_ATTEMPT_FAILED;
    diffequ_solve_result_t *result;

    if (attempt == DE_ATTEMPT_SOLVED) {
        result = de_solve_result_new(DE_SOLVE_STATUS_SOLVED, DE_SOLVER_CONSTANT_COEFFICIENT_TRANSPORT,
                                     "solved by the method of characteristics");
        if (!result || de_solve_result_append(result, solution) != 0) {
            de_solve_result_free(result);
            equ_free(solution);
            return NULL;
        }
        return result;
    }

    equ_free(solution);
    stationary = de_pde_solve_stationary_eigenfunction(de, residual, include_steps);
    if (stationary)
        return stationary;
    return de_solve_result_new(
        attempt == DE_ATTEMPT_FAILED ? DE_SOLVE_STATUS_FAILED : DE_SOLVE_STATUS_UNSUPPORTED, DE_SOLVER_NONE,
        attempt == DE_ATTEMPT_FAILED ? "failed to complete the multidimensional transport solution"
        : recognized                 ? "the multidimensional transport data is unsupported"
                                     : "no available symbolic multidimensional PDE solver "
                                       "matched the equation");
}
