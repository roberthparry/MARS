#include <stdbool.h>
#include <stdlib.h>

#include "diffequation.h"
#define MARS_DIFFEQUATION_SOLVE_INTERNAL_ACCESS
#include "diffequ_solve_internal.h"
#define MARS_DIFFEQUATION_PDE_INTERNAL_ACCESS
#include "diffequ_pde_internal.h"

expr_t *de_simplify_unary_owned(
    expr_t *owned,
    expr_t *(*operation)(const expr_t *))
{
    expr_t *raw = owned ? operation(owned) : NULL;
    expr_t *simplified = raw ? expr_simplify(raw) : NULL;

    expr_free(raw);
    expr_free(owned);
    return simplified;
}

expr_t *de_integrate_or_formal(const expr_t *integrand,
                               const expr_t *wrt)
{
    expr_t *integral =
        integrand && wrt ? expr_integrate(integrand, wrt) : NULL;

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
    return de_expr_contains(left, needle) ||
           de_expr_contains(right, needle);
}

bool de_linear_decompose(const expr_t *expr,
                         const expr_t *needle,
                         expr_t **coefficient_out,
                         expr_t **constant_out)
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

        if (!de_linear_decompose(
                left, needle, &coefficient, &constant))
            return false;
        *coefficient_out =
            de_simplify_unary_owned(coefficient, expr_neg);
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

        if (!de_linear_decompose(
                left, needle, &left_coefficient, &left_constant) ||
            !de_linear_decompose(
                right, needle, &right_coefficient, &right_constant))
            goto add_fail;

        *coefficient_out = is_sub
            ? expr_sub_simplify_owned(
                  left_coefficient, right_coefficient)
            : expr_add_simplify_owned(
                  left_coefficient, right_coefficient);
        *constant_out = is_sub
            ? expr_sub_simplify_owned(left_constant, right_constant)
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
        if (!de_linear_decompose(
                linear, needle, &coefficient, &constant))
            return false;

        *coefficient_out = expr_mul_simplify_owned(
            coefficient, expr_clone(factor));
        coefficient = NULL;
        *constant_out = expr_mul_simplify_owned(
            constant, expr_clone(factor));
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

        if (de_expr_contains(right, needle) ||
            !de_linear_decompose(
                left, needle, &coefficient, &constant))
            return false;

        *coefficient_out = expr_div_simplify_owned(
            coefficient, expr_clone(right));
        coefficient = NULL;
        *constant_out = expr_div_simplify_owned(
            constant, expr_clone(right));
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

static bool de_find_derivatives(const expr_t *expr,
                                const expr_t *independent,
                                const expr_t **dependent_out,
                                const expr_t **first_out,
                                const expr_t **second_out,
                                size_t *highest_order_out)
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
            if (!expr_struct_eq(
                    expr_formal_derivative_wrt_at(expr, i),
                    independent))
                return false;
        }
        dependent = expr_formal_derivative_dependent(expr);
        if (!dependent ||
            (*dependent_out &&
             !expr_struct_eq(*dependent_out, dependent)))
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
    return de_find_derivatives(
               left,
               independent,
               dependent_out,
               first_out,
               second_out,
               highest_order_out) &&
           de_find_derivatives(
               right,
               independent,
               dependent_out,
               first_out,
               second_out,
               highest_order_out);
}

bool de_expr_uses(const expr_t *expr, const expr_t *variable)
{
    expr_t *variables[1] = { (expr_t *)variable };
    bool used[1] = { false };

    return expr_collect_var_usage(expr, 1u, variables, used) &&
           used[0];
}

bool de_find_initial_condition(const diffequ_t *de,
                               const expr_t *dependent,
                               const expr_t **point_out,
                               const expr_t **value_out)
{
    for (size_t i = 0u; i < de->condition_count; ++i) {
        const equation_t *condition = de->conditions[i];

        if (condition &&
            de->condition_point_counts[i] == 1u &&
            expr_struct_eq(equ_lhs(condition), dependent)) {
            *point_out = de->condition_points[i][0];
            *value_out = equ_rhs(condition);
            return true;
        }
    }
    return false;
}

bool de_find_derivative_condition(const diffequ_t *de,
                                  const expr_t *dependent,
                                  const expr_t *independent,
                                  size_t order,
                                  const expr_t **point_out,
                                  const expr_t **value_out)
{
    for (size_t i = 0u; i < de->condition_count; ++i) {
        const equation_t *condition = de->conditions[i];
        const expr_t *left = condition ? equ_lhs(condition) : NULL;

        if (!left ||
            de->condition_point_counts[i] != 1u ||
            !expr_is_formal_derivative(left) ||
            expr_formal_derivative_order(left) != order ||
            !expr_struct_eq(
                expr_formal_derivative_dependent(left), dependent))
            continue;

        for (size_t j = 0u; j < order; ++j) {
            if (!expr_struct_eq(
                    expr_formal_derivative_wrt_at(left, j),
                    independent))
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

bool de_has_zero_initial_condition(const diffequ_t *de,
                                   const expr_t *dependent)
{
    const expr_t *point = NULL;
    const expr_t *value = NULL;

    return de_find_initial_condition(
               de, dependent, &point, &value) &&
           expr_is_exact_zero(value);
}

expr_t *de_arbitrary_constant(void)
{
    return expr_new_named_const(NUM_NAN, "C");
}

diffequ_solve_result_t *de_solve(const diffequ_t *de)
{
    const expr_t *independent;
    const expr_t *first_derivative = NULL;
    const expr_t *second_derivative = NULL;
    const expr_t *dependent = NULL;
    size_t highest_order = 0u;
    expr_t *residual = NULL;
    expr_t *derivative_coefficient = NULL;
    expr_t *remainder = NULL;
    expr_t *negative_remainder = NULL;
    expr_t *derivative_right = NULL;
    equation_t *solution = NULL;
    equation_t *derivative_quadratic_solutions[3] = {
        NULL, NULL, NULL
    };
    equation_t *exact_derivative_solutions[7] = {
        NULL, NULL, NULL, NULL, NULL, NULL, NULL
    };
    size_t derivative_quadratic_count = 0u;
    size_t exact_derivative_count = 0u;
    de_attempt_t derivative_quadratic;
    de_attempt_t exact_derivative;
    de_attempt_t separable;
    de_attempt_t linear;
    de_attempt_t bernoulli;
    de_attempt_t homogeneous;
    de_attempt_t linear_substitution;
    de_attempt_t linear_transformation;
    de_attempt_t sturm_liouville;
    de_attempt_t parameter_linear_pde;
    diffequ_solve_result_t *result = NULL;

    if (!de)
        return de_solve_result_new(
            DE_SOLVE_STATUS_INVALID,
            DE_SOLVER_NONE,
            "differential equation is NULL");
    if (de->independent_count == 2u) {
        residual = equ_residual(de->equation);
        result = de_pde_solve_two_variable(de, residual);
        goto cleanup;
    }
    if (de->independent_count != 1u)
        return de_solve_result_new(
            DE_SOLVE_STATUS_UNSUPPORTED,
            DE_SOLVER_NONE,
            "the solver requires one ODE variable or two PDE variables");

    independent = de->independent_vars[0];
    residual = equ_residual(de->equation);
    if (!residual ||
        !de_find_derivatives(
            residual,
            independent,
            &dependent,
            &first_derivative,
            &second_derivative,
            &highest_order) ||
        !dependent) {
        result = de_solve_result_new(
            DE_SOLVE_STATUS_UNSUPPORTED,
            DE_SOLVER_NONE,
            "the equation is not a supported ordinary differential equation");
        goto cleanup;
    }

    if (highest_order > 2u) {
        exact_derivative = highest_order == 3u
            ? de_attempt_exact_derivative_linearization(
                  de,
                  independent,
                  dependent,
                  first_derivative,
                  second_derivative,
                  residual,
                  exact_derivative_solutions,
                  &exact_derivative_count)
            : DE_ATTEMPT_NOT_MATCHED;
        if (exact_derivative == DE_ATTEMPT_SOLVED) {
            result = de_solve_result_new(
                DE_SOLVE_STATUS_SOLVED,
                DE_SOLVER_EXACT_DERIVATIVE_LINEARIZATION,
                "linearized exactly, then solved by a convergent "
                "power-series recurrence");
            if (!result)
                goto cleanup;
            for (size_t i = 0u; i < exact_derivative_count; ++i) {
                if (de_solve_result_append(
                        result,
                        exact_derivative_solutions[i]) != 0) {
                    de_solve_result_free(result);
                    result = NULL;
                    goto cleanup;
                }
                exact_derivative_solutions[i] = NULL;
            }
            goto cleanup;
        }
        if (exact_derivative == DE_ATTEMPT_FAILED) {
            result = de_solve_result_new(
                DE_SOLVE_STATUS_FAILED,
                DE_SOLVER_NONE,
                "failed to complete the exact-derivative linearization");
            goto cleanup;
        }

        sturm_liouville = de_attempt_constant_coefficient_linear(
            de,
            independent,
            dependent,
            highest_order,
            residual,
            &solution);
        if (sturm_liouville == DE_ATTEMPT_SOLVED) {
            result = de_solve_result_new(
                DE_SOLVE_STATUS_SOLVED,
                DE_SOLVER_CONSTANT_COEFFICIENT_LINEAR,
                "solved as a constant-coefficient linear ODE");
            goto append;
        }
        result = de_solve_result_new(
            sturm_liouville == DE_ATTEMPT_FAILED
                ? DE_SOLVE_STATUS_FAILED
                : DE_SOLVE_STATUS_UNSUPPORTED,
            DE_SOLVER_NONE,
            sturm_liouville == DE_ATTEMPT_FAILED
                ? "failed to complete the constant-coefficient linear solution"
                : "the higher-order equation is not constant-coefficient linear");
        goto cleanup;
    }

    if (second_derivative) {
        sturm_liouville = de_attempt_sturm_liouville(
            de,
            independent,
            dependent,
            second_derivative,
            first_derivative,
            residual,
            &solution);
        if (sturm_liouville == DE_ATTEMPT_SOLVED) {
            result = de_solve_result_new(
                DE_SOLVE_STATUS_SOLVED,
                DE_SOLVER_STURM_LIOUVILLE,
                "solved as a second-order linear Sturm-Liouville equation");
            goto append;
        }
        if (sturm_liouville == DE_ATTEMPT_NOT_MATCHED) {
            sturm_liouville =
                de_attempt_constant_coefficient_linear(
                    de,
                    independent,
                    dependent,
                    highest_order,
                    residual,
                    &solution);
            if (sturm_liouville == DE_ATTEMPT_SOLVED) {
                result = de_solve_result_new(
                    DE_SOLVE_STATUS_SOLVED,
                    DE_SOLVER_CONSTANT_COEFFICIENT_LINEAR,
                    "solved as a constant-coefficient linear ODE");
                goto append;
            }
        }
        result = de_solve_result_new(
            sturm_liouville == DE_ATTEMPT_FAILED
                ? DE_SOLVE_STATUS_FAILED
                : DE_SOLVE_STATUS_UNSUPPORTED,
            DE_SOLVER_NONE,
            sturm_liouville == DE_ATTEMPT_FAILED
                ? "failed to complete the Sturm-Liouville solution"
                : "the second-order linear equation has no supported closed-form basis");
        goto cleanup;
    }

    if (!first_derivative) {
        result = de_solve_result_new(
            DE_SOLVE_STATUS_UNSUPPORTED,
            DE_SOLVER_NONE,
            "the equation has no first derivative");
        goto cleanup;
    }

    derivative_quadratic = de_attempt_derivative_quadratic(
        de,
        independent,
        dependent,
        first_derivative,
        residual,
        derivative_quadratic_solutions,
        &derivative_quadratic_count);
    if (derivative_quadratic == DE_ATTEMPT_SOLVED) {
        result = de_solve_result_new(
            DE_SOLVE_STATUS_SOLVED,
            DE_SOLVER_DERIVATIVE_QUADRATIC,
            "solved as an autonomous derivative-quadratic ODE");
        if (!result)
            goto cleanup;
        for (size_t i = 0u;
             i < derivative_quadratic_count;
             ++i) {
            if (de_solve_result_append(
                    result,
                    derivative_quadratic_solutions[i]) != 0) {
                de_solve_result_free(result);
                result = NULL;
                goto cleanup;
            }
            derivative_quadratic_solutions[i] = NULL;
        }
        goto cleanup;
    }
    if (derivative_quadratic == DE_ATTEMPT_FAILED) {
        result = de_solve_result_new(
            DE_SOLVE_STATUS_FAILED,
            DE_SOLVER_NONE,
            "failed to complete the derivative-quadratic solution");
        goto cleanup;
    }

    if (!de_linear_decompose(
            residual,
            first_derivative,
            &derivative_coefficient,
            &remainder)) {
        result = de_solve_result_new(
            DE_SOLVE_STATUS_UNSUPPORTED,
            DE_SOLVER_NONE,
            "the first derivative could not be isolated");
        goto cleanup;
    }

    negative_remainder =
        de_simplify_unary_owned(remainder, expr_neg);
    remainder = NULL;
    derivative_right = negative_remainder
        ? expr_div_simplify_owned(
              negative_remainder, derivative_coefficient)
        : NULL;
    if (negative_remainder) {
        negative_remainder = NULL;
        derivative_coefficient = NULL;
    }
    if (!derivative_right) {
        result = de_solve_result_new(
            DE_SOLVE_STATUS_FAILED,
            DE_SOLVER_NONE,
            "failed to construct the isolated derivative");
        goto cleanup;
    }

    parameter_linear_pde = de_pde_attempt_parameter_linear(
        independent,
        dependent,
        derivative_right,
        &solution);
    if (parameter_linear_pde == DE_ATTEMPT_SOLVED) {
        result = de_solve_result_new(
            DE_SOLVE_STATUS_SOLVED,
            DE_SOLVER_PARAMETER_LINEAR_PDE,
            "solved as a parameter-dependent first-order linear PDE");
        goto append;
    }
    if (parameter_linear_pde == DE_ATTEMPT_FAILED) {
        result = de_solve_result_new(
            DE_SOLVE_STATUS_FAILED,
            DE_SOLVER_NONE,
            "failed to complete the parameter-dependent linear PDE");
        goto cleanup;
    }

    separable = de_attempt_separable(
        de,
        independent,
        dependent,
        derivative_right,
        &solution);
    if (separable == DE_ATTEMPT_SOLVED) {
        result = de_solve_result_new(
            DE_SOLVE_STATUS_SOLVED,
            DE_SOLVER_SEPARABLE,
            "solved as a first-order separable ODE");
        goto append;
    }

    linear = de_attempt_linear(
        de,
        independent,
        dependent,
        derivative_right,
        &solution);
    if (linear == DE_ATTEMPT_SOLVED) {
        result = de_solve_result_new(
            DE_SOLVE_STATUS_SOLVED,
            DE_SOLVER_LINEAR,
            "solved as a first-order linear ODE");
        goto append;
    }

    bernoulli = de_attempt_bernoulli_square(
        de,
        independent,
        dependent,
        derivative_right,
        &solution);
    if (bernoulli == DE_ATTEMPT_SOLVED) {
        result = de_solve_result_new(
            DE_SOLVE_STATUS_SOLVED,
            DE_SOLVER_BERNOULLI,
            "solved as a quadratic Bernoulli ODE");
        goto append;
    }

    homogeneous = de_attempt_homogeneous(
        de,
        independent,
        dependent,
        derivative_right,
        &solution);
    if (homogeneous == DE_ATTEMPT_SOLVED) {
        result = de_solve_result_new(
            DE_SOLVE_STATUS_SOLVED,
            DE_SOLVER_HOMOGENEOUS,
            "solved as a first-order homogeneous ODE");
        goto append;
    }

    linear_substitution = de_attempt_linear_substitution(
        de,
        independent,
        dependent,
        derivative_right,
        &solution);
    if (linear_substitution == DE_ATTEMPT_SOLVED) {
        result = de_solve_result_new(
            DE_SOLVE_STATUS_SOLVED,
            DE_SOLVER_LINEAR_SUBSTITUTION,
            "solved by a first-order linear substitution");
        goto append;
    }

    linear_transformation = de_attempt_linear_transformation(
        de,
        independent,
        dependent,
        derivative_right,
        &solution);
    if (linear_transformation == DE_ATTEMPT_SOLVED) {
        result = de_solve_result_new(
            DE_SOLVE_STATUS_SOLVED,
            DE_SOLVER_LINEAR_TRANSFORMATION,
            "solved by a linear change of variables");
        goto append;
    }

    result = de_solve_result_new(
        separable == DE_ATTEMPT_FAILED ||
            linear == DE_ATTEMPT_FAILED ||
            bernoulli == DE_ATTEMPT_FAILED ||
            homogeneous == DE_ATTEMPT_FAILED ||
            linear_substitution == DE_ATTEMPT_FAILED ||
            linear_transformation == DE_ATTEMPT_FAILED
            ? DE_SOLVE_STATUS_FAILED
            : DE_SOLVE_STATUS_UNSUPPORTED,
        DE_SOLVER_NONE,
        "no available symbolic solver completed the equation");
    goto cleanup;

append:
    if (!result ||
        de_solve_result_append(result, solution) != 0) {
        equ_free(solution);
        de_solve_result_free(result);
        result = NULL;
    }
    solution = NULL;

cleanup:
    for (size_t i = 0u; i < 7u; ++i)
        equ_free(exact_derivative_solutions[i]);
    for (size_t i = 0u; i < 3u; ++i)
        equ_free(derivative_quadratic_solutions[i]);
    equ_free(solution);
    expr_free(derivative_right);
    expr_free(negative_remainder);
    expr_free(remainder);
    expr_free(derivative_coefficient);
    expr_free(residual);
    return result;
}
