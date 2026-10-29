/**
 * @file diffequ_notation_problem.c
 * @brief Prepare authored differential equations for semantic presentation.
 *
 * Repeated quadratic operators and differential forms are interpreted here,
 * alongside the native equation representation. Only mathematical nodes cross
 * into TeX; the presentation module does not inspect solver data or coefficients.
 */
#include <limits.h>
#include <stdlib.h>

#include "diffequation-t.h"
#include "TeX-t.h"

#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"
#define MARS_SHARED_DIFFEQUATION_INTERNAL_ACCESS
#include "internal/diffequation_internal.h"
#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_notation_internal.h"
#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_internal.h"

static const TeX_model_node_t *binary(notation_build_t *build, TeX_node_kind_t kind,
                                     const TeX_model_node_t *left, const TeX_model_node_t *right)
{
    const TeX_model_node_t *parts[] = {left, right};
    return notation_build_node(build, (TeX_node_view_t){.kind = kind, .child_count = 2u}, parts);
}

static const TeX_model_node_t *negative(notation_build_t *build, const TeX_model_node_t *value)
{
    return notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_NEGATIVE, .child_count = 1u}, &value);
}

static const TeX_model_node_t *repeated_quadratic(notation_build_t *build, const diffequ_t *problem)
{
    if (problem->repeated_quadratic_power < 2u || problem->repeated_quadratic_power > UINT_MAX / 2u ||
        !problem->repeated_quadratic_square || !problem->repeated_quadratic_dependent ||
        problem->independent_count != 1u)
        return NULL;
    const TeX_model_node_t *dependent = notation_build_literal(build, TeX_NODE_IDENTIFIER,
                                                                problem->repeated_quadratic_dependent);
    const TeX_model_node_t *coordinate = notation_build_expr(build, problem->independent_vars[0]);
    const TeX_model_node_t *sum = NULL;
    for (size_t next = problem->repeated_quadratic_power + 1u; next > 0u; --next) {
        size_t degree = next - 1u;
        number_t n = num_create_from_long((long)problem->repeated_quadratic_power);
        number_t k = num_create_from_long((long)degree);
        number_t choose = num_binomial(n, k);
        expr_t *binomial = num_is_finite(choose) ? expr_new_const(choose) : NULL;
        expr_t *power = expr_pow_long(problem->repeated_quadratic_square,
                                      (long)(problem->repeated_quadratic_power - degree));
        expr_t *product = binomial && power ? expr_mul(binomial, power) : NULL;
        expr_t *coefficient = product ? expr_display_simplified(product) : NULL;
        expr_free(product);
        expr_free(power);
        expr_free(binomial);
        num_destroy(&choose);
        num_destroy(&k);
        num_destroy(&n);
        if (!coefficient)
            return NULL;
        const expr_t *magnitude = coefficient;
        bool subtract = expr_match_neg_expr(coefficient, &magnitude);
        number_t value = num_new();
        bool unit = expr_match_const_value(magnitude, &value) && num_eq(value, NUM_ONE);
        num_destroy(&value);
        const TeX_model_node_t *variables[] = {dependent, coordinate};
        const TeX_model_node_t *term = degree ? notation_build_node(build,
            (TeX_node_view_t){.kind = TeX_NODE_VARIABLE_DERIVATIVE, .child_count = 2u,
                             .order = (unsigned)(2u * degree)}, variables) : dependent;
        if (!unit)
            term = binary(build, TeX_NODE_PRODUCT, notation_build_expr(build, magnitude), term);
        expr_free(coefficient);
        if (!term)
            return NULL;
        sum = sum ? binary(build, subtract ? TeX_NODE_DIFFERENCE : TeX_NODE_SUM, sum, term)
                  : subtract ? negative(build, term) : term;
    }
    return binary(build, TeX_NODE_EQUATION, sum, notation_build_literal(build, TeX_NODE_NUMBER, "0"));
}

static const expr_t *first_derivative(const expr_t *expression, unsigned depth)
{
    if (!expression || depth >= 256u)
        return NULL;
    if (expr_is_formal_derivative(expression))
        return expression;
    const expr_t *left = NULL, *right = NULL;
    if (!expr_child_exprs(expression, &left, &right))
        return NULL;
    const expr_t *found = first_derivative(left, depth + 1u);
    return found ? found : first_derivative(right, depth + 1u);
}

static const TeX_model_node_t *differential_term(notation_build_t *build, const expr_t *coefficient,
                                                const expr_t *variable, bool *subtract)
{
    const expr_t *magnitude = coefficient;
    *subtract = expr_match_neg_expr(coefficient, &magnitude);
    number_t value = num_new();
    bool unit = expr_match_const_value(magnitude, &value) && num_eq(value, NUM_ONE);
    num_destroy(&value);
    const TeX_model_node_t *coordinate = notation_build_expr(build, variable);
    const TeX_model_node_t *differential = notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_DIFFERENTIAL, .child_count = 1u}, &coordinate);
    return unit ? differential : binary(build, TeX_NODE_PRODUCT, notation_build_expr(build, magnitude), differential);
}

static const TeX_model_node_t *differential_form(notation_build_t *build, const diffequ_t *problem)
{
    if (!problem->differential_form_input || problem->independent_count != 1u)
        return NULL;
    expr_t *residual = equ_residual(problem->equation);
    const expr_t *derivative = first_derivative(residual, 0u);
    const expr_t *dependent = derivative ? expr_formal_derivative_dependent(derivative) : NULL;
    expr_t *dependent_coefficient = NULL, *independent_coefficient = NULL;
    const TeX_model_node_t *result = NULL;
    if (dependent && de_linear_decompose(residual, derivative, &dependent_coefficient, &independent_coefficient)) {
        bool minus_left = false, minus_right = false;
        const TeX_model_node_t *left = differential_term(build, dependent_coefficient, dependent, &minus_left);
        const TeX_model_node_t *right = differential_term(build, independent_coefficient,
                                                          problem->independent_vars[0], &minus_right);
        const TeX_model_node_t *sum = binary(build, minus_right ? TeX_NODE_DIFFERENCE : TeX_NODE_SUM,
                                             minus_left ? negative(build, left) : left, right);
        result = binary(build, TeX_NODE_EQUATION, sum, notation_build_literal(build, TeX_NODE_NUMBER, "0"));
    }
    expr_free(independent_coefficient);
    expr_free(dependent_coefficient);
    expr_free(residual);
    return result;
}

static bool append_ordered_terms(notation_build_t *build, const expr_t *expression,
                                 const TeX_model_node_t **sum, bool subtract, unsigned depth)
{
    if (!expression || depth >= 256u)
        return false;
    const expr_t *left = NULL, *right = NULL;
    bool difference = false;
    if (expr_match_neg_expr(expression, &left))
        return append_ordered_terms(build, left, sum, !subtract, depth + 1u);
    if (expr_match_add_sub_expr(expression, &left, &right, &difference))
        return append_ordered_terms(build, left, sum, subtract, depth + 1u) &&
               append_ordered_terms(build, right, sum, subtract != difference, depth + 1u);
    const TeX_model_node_t *term = notation_build_expr(build, expression);
    *sum = *sum ? binary(build, subtract ? TeX_NODE_DIFFERENCE : TeX_NODE_SUM, *sum, term)
                : subtract ? negative(build, term) : term;
    return *sum != NULL;
}

static const TeX_model_node_t *ordered_side(notation_build_t *build, const expr_t *expression)
{
    const TeX_model_node_t *sum = NULL;
    return append_ordered_terms(build, expression, &sum, false, 0u) ? sum : NULL;
}

/* Prefer the authored equation form, preserving term order and the declared derivative meaning. */
const TeX_model_node_t *de_notation_equation(notation_build_t *build, const diffequ_t *problem)
{
    if (!problem || !problem->equation)
        return NULL;
    const TeX_model_node_t *special = repeated_quadratic(build, problem);
    if (!special)
        special = differential_form(build, problem);
    if (special)
        return special;
    return binary(build, TeX_NODE_EQUATION,
        ordered_side(build, problem->display_lhs ? problem->display_lhs : equ_lhs(problem->equation)),
        ordered_side(build, problem->display_rhs ? problem->display_rhs : equ_rhs(problem->equation)));
}

/* Render an equation-only snapshot through TeX's semantic callbacks. */
string_t *de_to_TeX(const diffequ_t *problem)
{
    if (!problem || !problem->equation)
        return string_new_with("NULL");
    notation_build_t *build = notation_build_new();
    notation_build_derivatives(build, problem->independent_count > 1u || problem->partial_derivative_input);
    notation_build_dependent_variables(build, true);
    notation_build_source_order(build, true);
    string_t *result = notation_build_render(build, de_notation_equation(build, problem));
    notation_build_free(build);
    return result;
}

/* Conditions and equation are obtained from the same immutable native snapshot. */
string_t *de_problem_to_TeX(const diffequ_t *problem)
{
    TeX_context_t *context = de_TeX_context(problem);
    TeX_t *notation = context ? TeX_from_context(context) : NULL;
    string_t *result = notation ? TeX_render(notation) : NULL;
    TeX_free(notation);
    TeX_context_free(context);
    return result;
}
