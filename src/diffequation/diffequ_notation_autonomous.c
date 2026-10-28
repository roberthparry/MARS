/**
 * @file diffequ_notation_autonomous.c
 * @brief Describe autonomous characteristic solutions as semantic mathematics.
 *
 * The solver supplies its invariant, implicit relation and domain assumptions;
 * TeX renders the resulting nodes without access to native expression layouts.
 */
#include <stdlib.h>

#include "diffequation-t.h"
#include "TeX-t.h"

#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"
#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"
#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_notation_internal.h"
#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_internal.h"

static const TeX_model_node_t *pair(notation_build_t *build, TeX_node_kind_t kind,
                                   const TeX_model_node_t *left, const TeX_model_node_t *right)
{
    const TeX_model_node_t *children[] = {left, right};
    return notation_build_node(build, (TeX_node_view_t){.kind = kind, .child_count = 2u}, children);
}

static const TeX_model_node_t *nonzero(notation_build_t *build, const TeX_model_node_t *value)
{
    const TeX_model_node_t *children[] = {value, notation_build_literal(build, TeX_NODE_NUMBER, "0")};
    return notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_RELATION,
                               .relation = TeX_RELATION_NOT_EQUAL, .child_count = 2u}, children);
}

/* Project characteristic identities and the assumptions of each local solution. */
bool de_autonomous_steps(diffequ_solve_result_t *result, const expr_t *dependent, const expr_t *along,
                         const expr_t *across, const expr_t *coefficient, const expr_t *speed,
                         const expr_t *invariant, const expr_t *relation, bool assumption)
{
    const expr_t *values[] = {dependent, along, across, coefficient, speed, invariant, relation};
    const TeX_model_node_t *nodes[7] = {NULL};
    char *text[7] = {NULL};
    notation_build_t *build = notation_build_new();
    string_t *plain = NULL, *display = NULL;
    bool valid = result && build;
    for (size_t i = 0u; valid && i < 7u; ++i) {
        const char *name = i < 3u ? expr_symbol_name(values[i]) : NULL;
        expr_t *symbol = name ? expr_new_named_var(NUM_NAN, name) : NULL;
        const expr_t *value = symbol ? symbol : values[i];
        text[i] = expr_to_string(value, style_UNBOUND);
        nodes[i] = notation_build_expr(build, value);
        expr_free(symbol);
        valid = text[i] && nodes[i];
    }
    if (valid)
        plain = string_sprintf(
            "Autonomous transport: implicit characteristic solution.\n"
            "Along characteristics, d%s/d%s = %s and d%s/d%s = 0.\n"
            "Thus %s and %s are constant along each characteristic.\n"
            "Relating these constants gives %s = F(%s), with F an arbitrary differentiable function.\n"
            "Let H = %s. Partial derivatives of H hold %s independent of the coordinates.\n"
            "The native solver verifies the original transport operator applied to H is zero.\n"
            "Local smooth branches require the partial derivative of H with respect to %s to be nonzero.\n"
            "Coefficients must be defined; no initial or boundary data are imposed.\n"
            "This is a classical local solution, not a continuation through characteristic crossings.\n",
            text[2], text[1], text[4], text[0], text[1], text[0], text[5], text[0], text[5], text[6], text[0], text[0]);
    if (plain && assumption && string_append_format(plain, "Normalisation requires %s != 0.\n", text[3]) < 0) {
        string_free(plain);
        plain = NULL;
    }
    if (plain) {
        const TeX_model_node_t *zero = notation_build_literal(build, TeX_NODE_NUMBER, "0");
        const TeX_model_node_t *h = notation_build_literal(build, TeX_NODE_IDENTIFIER, "H");
        const TeX_model_node_t *family = pair(build, TeX_NODE_APPLY,
            notation_build_literal(build, TeX_NODE_FUNCTION_NAME, "F"), nodes[5]);
        const TeX_model_node_t *derivative_inputs[] = {nodes[2], nodes[1]};
        const TeX_model_node_t *constant_inputs[] = {nodes[0], nodes[1]};
        const TeX_model_node_t *branch_inputs[] = {h, nodes[0]};
        TeX_node_view_t derivative = {.kind = TeX_NODE_VARIABLE_DERIVATIVE, .child_count = 2u, .order = 1u};
        const TeX_model_node_t *d_across = notation_build_node(build, derivative, derivative_inputs);
        const TeX_model_node_t *d_dependent = notation_build_node(build, derivative, constant_inputs);
        derivative.partial = true;
        const TeX_model_node_t *d_h = notation_build_node(build, derivative, branch_inputs);
        const TeX_model_node_t *rows[12];
        size_t n = 0u;
        rows[n++] = notation_build_literal(build, TeX_NODE_TEXT, "Autonomous transport: implicit characteristic solution.");
        rows[n++] = pair(build, TeX_NODE_STATEMENTS, pair(build, TeX_NODE_EQUATION, d_across, nodes[4]),
                         pair(build, TeX_NODE_EQUATION, d_dependent, zero));
        const TeX_model_node_t *constants[] = {nodes[0], nodes[5]};
        rows[n++] = pair(build, TeX_NODE_STATEMENTS,
            notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_LIST, .child_count = 2u}, constants),
            notation_build_literal(build, TeX_NODE_TEXT, "are constant along characteristics."));
        rows[n++] = pair(build, TeX_NODE_EQUATION, nodes[0], family);
        rows[n++] = notation_build_literal(build, TeX_NODE_TEXT, "F is arbitrary and differentiable.");
        rows[n++] = pair(build, TeX_NODE_EQUATION, h, nodes[6]);
        rows[n++] = pair(build, TeX_NODE_STATEMENTS,
            notation_build_literal(build, TeX_NODE_TEXT, "The original transport operator annihilates H with"), nodes[0]);
        rows[n++] = notation_build_literal(build, TeX_NODE_TEXT, "held fixed.");
        rows[n++] = pair(build, TeX_NODE_STATEMENTS, nonzero(build, d_h),
                         notation_build_literal(build, TeX_NODE_TEXT, "defines a local smooth branch."));
        rows[n++] = notation_build_literal(build, TeX_NODE_TEXT, "Coefficients defined; no initial or boundary data imposed.");
        rows[n++] = notation_build_literal(build, TeX_NODE_TEXT, "No continuation through characteristic crossings is asserted.");
        if (assumption)
            rows[n++] = pair(build, TeX_NODE_STATEMENTS, nonzero(build, nodes[3]),
                             notation_build_literal(build, TeX_NODE_TEXT, "normalisation"));
        const TeX_model_node_t *root = notation_build_node(build,
            (TeX_node_view_t){.kind = TeX_NODE_LINES, .child_count = n}, rows);
        display = notation_build_render(build, root);
    }
    valid = plain && display && de_solve_result_set_steps(result, string_c_str(plain)) == 0 &&
            de_solve_result_set_steps_TeX(result, string_c_str(display)) == 0;
    string_free(display);
    string_free(plain);
    for (size_t i = 0u; i < 7u; ++i)
        free(text[i]);
    notation_build_free(build);
    return valid;
}
