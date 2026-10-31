/**
 * @file diffequ_notation_provider.c
 * @brief Expose a differential equation and its conditions as semantic nodes.
 *
 * The native module selects the governing equation and recorded conditions.
 * Derivative meaning is supplied by the expression provider; TeX receives a
 * sequence of mathematical equalities without inspecting solver structures.
 */
#include <stdint.h>
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

static const TeX_model_node_t *build_condition(notation_build_t *build, const diffequ_t *problem,
                                               size_t index, bool primes)
{
    const equation_t *equation = de_condition_at(problem, index);
    size_t count = de_condition_argument_count(problem, index);
    if (!equation || count != de_independent_count(problem) || count >= 65536u)
        return NULL;
    const expr_t *left = equ_lhs(equation);
    const expr_t *dependent = expr_is_formal_derivative(left) ? expr_formal_derivative_dependent(left) : left;
    if (primes && count == 1u && expr_is_variable(dependent)) {
        const TeX_model_node_t *application[] = {
            notation_build_expr(build, left), notation_build_expr(build, de_condition_argument_at(problem, index, 0u))
        };
        const TeX_model_node_t *sides[] = {
            notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_APPLY, .child_count = 2u}, application),
            notation_build_expr(build, equ_rhs(equation))
        };
        return notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_EQUATION, .child_count = 2u}, sides);
    }
    const TeX_model_node_t **evaluation = calloc(count + 1u, sizeof *evaluation);
    if (!evaluation)
        return NULL;
    evaluation[0] = notation_build_expr(build, equ_lhs(equation));
    for (size_t i = 0u; i < count; ++i) {
        const TeX_model_node_t *assignment[] = {
            notation_build_expr(build, de_independent_at(problem, i)),
            notation_build_expr(build, de_condition_argument_at(problem, index, i))
        };
        evaluation[i + 1u] = notation_build_node(build,
            (TeX_node_view_t){.kind = TeX_NODE_ASSIGNMENT, .child_count = 2u}, assignment);
    }
    const TeX_model_node_t *sides[] = {
        count ? notation_build_node(build,
            (TeX_node_view_t){.kind = TeX_NODE_EVALUATION, .child_count = count + 1u}, evaluation) : evaluation[0],
        notation_build_expr(build, equ_rhs(equation))
    };
    free(evaluation);
    return notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_EQUATION, .child_count = 2u}, sides);
}

static const TeX_model_node_t *build_problem(notation_build_t *build, const diffequ_t *problem, bool primes)
{
    if (!build || !problem)
        return NULL;
    size_t conditions = de_condition_count(problem);
    if (conditions >= 65536u)
        return NULL;
    notation_build_derivatives(build, de_independent_count(problem) > 1u || problem->partial_derivative_input);
    primes = primes && de_independent_count(problem) == 1u && !problem->partial_derivative_input;
    expr_notation_prime_derivatives(build, primes);
    notation_build_dependent_variables(build, true);
    notation_build_source_order(build, true);
    const TeX_model_node_t **parts = calloc(conditions + 1u, sizeof *parts);
    if (!parts)
        return NULL;
    parts[0] = de_notation_equation(build, problem);
    for (size_t i = 0; i < conditions; ++i)
        parts[i + 1u] = build_condition(build, problem, i, primes);
    const TeX_model_node_t *root = conditions ? notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_CONDITIONS, .child_count = conditions + 1u}, parts) : parts[0];
    free(parts);
    return root;
}

/* Project the canonical equation and conditions without changing ordinary native rendering preferences. */
const TeX_model_node_t *de_notation_problem(notation_build_t *build, const diffequ_t *problem)
{
    return build_problem(build, problem, false);
}

/* Snapshot the governing equation and each recorded condition in native order. */
TeX_context_t *de_TeX_context(const diffequ_t *problem)
{
    notation_build_t *build = problem ? notation_build_new() : NULL;
    const TeX_model_node_t *root = build ? de_notation_problem(build, problem) : NULL;
    TeX_context_t *context = root ? notation_build_context(build, root) : NULL;
    notation_build_free(build);
    return context;
}

/* Keep the authored derivative choice in mathematical input without exposing rendering contexts to callers. */
TeX_t *de_notation_input(const string_t *text)
{
    diffequ_t *problem = text ? de_from_text(text) : NULL;
    notation_build_t *build = problem ? notation_build_new() : NULL;
    const TeX_model_node_t *root = build ? build_problem(build, problem, problem->prime_derivative_input) : NULL;
    TeX_context_t *context = root ? notation_build_context(build, root) : NULL;
    notation_build_free(build);
    de_free(problem);
    TeX_t *result = context ? TeX_from_context(context) : NULL;
    TeX_context_free(context);
    return result;
}
