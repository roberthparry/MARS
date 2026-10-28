/**
 * @file diffequ_notation_stationary.c
 * @brief Describe a stationary-eigenfunction evolution using semantic mathematics.
 *
 * The native PDE solver supplies the operator, initial state and verified rate;
 * TeX renders their equations without accessing the solver's implementation.
 */
#include <stdlib.h>

#include "diffequation-t.h"
#include "TeX-t.h"

#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_notation_internal.h"
#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_internal.h"

static const TeX_model_node_t *pair(notation_build_t *b, TeX_node_kind_t kind,
                                   const TeX_model_node_t *a, const TeX_model_node_t *c)
{
    const TeX_model_node_t *children[] = {a, c};
    return notation_build_node(b, (TeX_node_view_t){.kind = kind, .child_count = 2u}, children);
}

/* Preserve the operator, initial state, derived rate and final verified solution. */
bool de_pde_set_stationary_steps(diffequ_solve_result_t *result, const expr_t *dependent,
                                 const expr_t *time_coordinate, const expr_t *time_point,
                                 const expr_t *time_coefficient, const expr_t *spatial_operator,
                                 const expr_t *initial, const expr_t *rate, const equation_t *solution)
{
    const expr_t *values[] = {dependent, time_coordinate, time_point, time_coefficient, spatial_operator, initial, rate};
    char *text[7] = {NULL};
    bool valid = result && solution;
    for (size_t i = 0u; valid && i < 7u; ++i) {
        text[i] = expr_to_string(values[i], style_UNBOUND);
        valid = text[i] != NULL;
    }
    string_t *solution_text = valid ? equ_to_text(solution, style_UNBOUND) : NULL;
    string_t *plain = solution_text ? string_sprintf(
        "Separate the evolution derivative from the spatial operator:\n"
        "      A·∂%s/∂%s + H[%s] = 0\n"
        "      A = %s,    H[%s] = %s\n"
        "Use the supplied initial state at %s = %s:\n"
        "      %s₀ = %s\n"
        "Apply H and divide by A%s₀. The derived constant rate is\n"
        "      λ = −H[%s₀]/(A%s₀) = %s\n"
        "Hence %s = %s₀·exp(λ(%s − %s)):\n"
        "      %s", text[0], text[1], text[0], text[3], text[0], text[4], text[1], text[2],
        text[0], text[5], text[0], text[0], text[0], text[6], text[0], text[0], text[1], text[2],
        string_c_str(solution_text)) : NULL;
    notation_build_t *b = plain ? notation_build_new() : NULL;
    string_t *display = NULL;
    if (b) {
        const TeX_model_node_t *nodes[7];
        for (size_t j = 0u; j < 7u; ++j)
            nodes[j] = notation_build_expr(b, values[j]);
        const TeX_model_node_t *u = nodes[0], *t = nodes[1], *t0 = nodes[2];
        const TeX_model_node_t *A = notation_build_literal(b, TeX_NODE_IDENTIFIER, "A");
        const TeX_model_node_t *H = notation_build_literal(b, TeX_NODE_FUNCTION_NAME, "H");
        const TeX_model_node_t *lambda = notation_build_literal(b, TeX_NODE_IDENTIFIER, "λ");
        const TeX_model_node_t *zero = notation_build_literal(b, TeX_NODE_NUMBER, "0");
        const TeX_model_node_t *u0 = pair(b, TeX_NODE_SUBSCRIPT, u, zero);
        const TeX_model_node_t *derivative_children[] = {u, t};
        const TeX_model_node_t *derivative = notation_build_node(b,
            (TeX_node_view_t){.kind = TeX_NODE_VARIABLE_DERIVATIVE, .child_count = 2u, .order = 1u, .partial = true},
            derivative_children);
        const TeX_model_node_t *H_u = pair(b, TeX_NODE_APPLY, H, u);
        const TeX_model_node_t *H_u0 = pair(b, TeX_NODE_APPLY, H, u0);
        const TeX_model_node_t *negative_rate = pair(b, TeX_NODE_FRACTION, H_u0, pair(b, TeX_NODE_PRODUCT, A, u0));
        negative_rate = notation_build_node(b, (TeX_node_view_t){.kind = TeX_NODE_NEGATIVE, .child_count = 1u},
                                           &negative_rate);
        const TeX_model_node_t *exponent = pair(b, TeX_NODE_PRODUCT, lambda, pair(b, TeX_NODE_DIFFERENCE, t, t0));
        const TeX_model_node_t *evolution = pair(b, TeX_NODE_PRODUCT, u0,
            pair(b, TeX_NODE_POWER, notation_build_literal(b, TeX_NODE_IDENTIFIER, "e"), exponent));
        const TeX_model_node_t *rows[] = {
            pair(b, TeX_NODE_EQUATION, pair(b, TeX_NODE_SUM, pair(b, TeX_NODE_PRODUCT, A, derivative), H_u), zero),
            pair(b, TeX_NODE_EQUATION, A, nodes[3]),
            pair(b, TeX_NODE_EQUATION, H_u, nodes[4]),
            pair(b, TeX_NODE_EQUATION, u0, nodes[5]),
            pair(b, TeX_NODE_EQUATION, t, t0),
            pair(b, TeX_NODE_EQUATION, pair(b, TeX_NODE_EQUATION, lambda, negative_rate), nodes[6]),
            pair(b, TeX_NODE_EQUATION, u, evolution),
            pair(b, TeX_NODE_EQUATION, notation_build_expr(b, equ_lhs(solution)), notation_build_expr(b, equ_rhs(solution)))
        };
        const TeX_model_node_t *root = notation_build_node(b,
            (TeX_node_view_t){.kind = TeX_NODE_LINES, .child_count = sizeof rows / sizeof *rows}, rows);
        display = notation_build_render(b, root);
    }
    valid = plain && display && de_solve_result_set_steps(result, string_c_str(plain)) == 0 &&
            de_solve_result_set_steps_TeX(result, string_c_str(display)) == 0;
    string_free(display); notation_build_free(b); string_free(plain); string_free(solution_text);
    for (size_t i = 0u; i < 7u; ++i)
        free(text[i]);
    return valid;
}
