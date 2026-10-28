/**
 * @file diffequ_notation_gradient.c
 * @brief Project gradient-envelope solutions and restrictions into semantic notation.
 *
 * The native solver supplies the complete integral and regular local envelope.
 * Mathematical expressions and prose remain native data; TeX chooses their presentation.
 */
#include <stdlib.h>

#include "diffequation-t.h"
#include "TeX-t.h"

#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"
#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"
#define MARS_SHARED_EQUATION_INTERNAL_ACCESS
#include "internal/equation_internal.h"
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

static const TeX_model_node_t *nonzero(notation_build_t *build, const TeX_model_node_t *value,
                                       const TeX_model_node_t *zero)
{
    const TeX_model_node_t *children[] = {value, zero};
    return notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_RELATION,
                               .relation = TeX_RELATION_NOT_EQUAL, .child_count = 2u}, children);
}

static const TeX_model_node_t *prose(notation_build_t *build, const char *text)
{
    return notation_build_literal(build, TeX_NODE_TEXT, text);
}

typedef enum { GRADIENT_LEFT_ENVELOPE, GRADIENT_RIGHT_ENVELOPE, GRADIENT_LEFT_AFFINE,
               GRADIENT_RIGHT_AFFINE, GRADIENT_STEPS } gradient_part_t;

static string_t *gradient_render(const expr_t *const values[9], gradient_part_t part)
{
    notation_build_t *build = notation_build_new();
    if (!build)
        return NULL;
    const TeX_model_node_t *nodes[9];
    bool valid = true;
    for (size_t i = 0u; i < 9u; ++i) {
        const char *name = expr_symbol_name(values[i]);
        expr_t *symbol = name && (expr_is_variable(values[i]) || expr_is_named_const(values[i]))
            ? expr_new_named_var(NUM_NAN, name) : NULL;
        nodes[i] = notation_build_expr(build, symbol ? symbol : values[i]);
        expr_free(symbol);
        valid = valid && nodes[i];
    }
    const TeX_model_node_t *zero = notation_build_literal(build, TeX_NODE_NUMBER, "0");
    const TeX_model_node_t *h = notation_build_literal(build, TeX_NODE_IDENTIFIER, "H");
    const TeX_model_node_t *constraint = pair(build, TeX_NODE_EQUATION,
        pair(build, TeX_NODE_EQUATION, h, nodes[6]), zero);
    const TeX_model_node_t *derivative_children[] = {h, nodes[1]};
    const TeX_model_node_t *derivative = notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_VARIABLE_DERIVATIVE, .child_count = 2u, .order = 1u, .partial = true},
        derivative_children);
    const TeX_model_node_t *restrictions[] = {nonzero(build, nodes[2], zero), nonzero(build, derivative, zero)};
    const TeX_model_node_t *root = NULL;
    if (part == GRADIENT_LEFT_ENVELOPE || part == GRADIENT_LEFT_AFFINE) {
        root = pair(build, TeX_NODE_STATEMENTS,
            prose(build, part == GRADIENT_LEFT_ENVELOPE ? "Local envelope:" : "Affine complete integral:"), nodes[0]);
    } else if (part == GRADIENT_RIGHT_ENVELOPE) {
        const TeX_model_node_t *rows[] = {
            nodes[5], pair(build, TeX_NODE_STATEMENTS, prose(build, "subject to"), constraint),
            notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_STATEMENTS, .child_count = 2u}, restrictions),
            pair(build, TeX_NODE_STATEMENTS, nodes[8], prose(build, "arbitrary and twice continuously differentiable;")),
            pair(build, TeX_NODE_STATEMENTS, prose(build, "solve locally for"), nodes[1])
        };
        root = notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_LINES, .child_count = 5u}, rows);
    } else if (part == GRADIENT_RIGHT_AFFINE) {
        const TeX_model_node_t *constants[] = {nodes[1], nodes[4]};
        const TeX_model_node_t *rows[] = {
            nodes[7], restrictions[0],
            pair(build, TeX_NODE_STATEMENTS,
                notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_LIST, .child_count = 2u}, constants),
                prose(build, "arbitrary constants"))
        };
        root = notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_STATEMENTS, .child_count = 3u}, rows);
    } else {
        const TeX_model_node_t *p = notation_build_literal(build, TeX_NODE_IDENTIFIER, "p");
        const TeX_model_node_t *q = notation_build_literal(build, TeX_NODE_IDENTIFIER, "q");
        const TeX_model_node_t *identities[] = {
            pair(build, TeX_NODE_EQUATION, p, nodes[1]), pair(build, TeX_NODE_EQUATION, q, nodes[3]), restrictions[0]
        };
        const TeX_model_node_t *profile = pair(build, TeX_NODE_APPLY, nodes[8], nodes[1]);
        const TeX_model_node_t *rows[] = {
            prose(build, "Gradient-only PDE: characteristic slopes are constant."),
            notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_STATEMENTS, .child_count = 3u}, identities),
            pair(build, TeX_NODE_STATEMENTS, prose(build, "Replace the affine offset by"), profile),
            pair(build, TeX_NODE_STATEMENTS, prose(build, "and impose stationarity in"), nodes[1]),
            constraint,
            prose(build, "On H = 0, the chain rule recovers p and q satisfying the original PDE."),
            pair(build, TeX_NODE_STATEMENTS, restrictions[1], prose(build, "defines a regular local envelope.")),
            prose(build, "Affine families are separate; no initial or boundary data imposed."),
            prose(build, "No continuation through envelope singularities asserted.")
        };
        root = notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_LINES, .child_count = 9u}, rows);
    }
    string_t *result = valid && root ? notation_build_render(build, root) : NULL;
    notation_build_free(build);
    return result;
}

/* Store the native descriptions and independently rendered semantic projections. */
bool de_gradient_present(diffequ_solve_result_t *result, const expr_t *dependent, const expr_t *parameter,
                         const expr_t *denominator, const expr_t *slope, const expr_t *offset,
                         const expr_t *profile_symbol, bool include_steps)
{
    if (!result || !result->parameter_constraint || !result->solutions[0] || !result->solutions[1])
        return false;
    const expr_t *values[] = {dependent, parameter, denominator, slope, offset,
        equ_rhs(result->solutions[0]), equ_lhs(result->parameter_constraint), equ_rhs(result->solutions[1]), profile_symbol};
    char *plain[9] = {NULL};
    bool valid = true;
    for (size_t i = 0u; valid && i < 9u; ++i) {
        const char *name = expr_symbol_name(values[i]);
        expr_t *symbol = name && (expr_is_variable(values[i]) || expr_is_named_const(values[i]))
            ? expr_new_named_var(NUM_NAN, name) : NULL;
        plain[i] = expr_to_string(symbol ? symbol : values[i], style_UNBOUND);
        expr_free(symbol);
        valid = plain[i] != NULL;
    }
    string_t *left = valid ? string_sprintf("Local envelope: %s", plain[0]) : NULL;
    string_t *right = valid ? string_sprintf("%s\n  subject to H = %s = 0; %s != 0; dH/d%s != 0.\n"
        "  %s is arbitrary and twice continuously differentiable; solve jointly for %s.",
        plain[5], plain[6], plain[2], plain[1], plain[8], plain[1]) : NULL;
    string_t *left_notation = valid ? gradient_render(values, GRADIENT_LEFT_ENVELOPE) : NULL;
    string_t *right_notation = valid ? gradient_render(values, GRADIENT_RIGHT_ENVELOPE) : NULL;
    valid = left && right && left_notation && right_notation &&
        equ_set_display_unbound(result->solutions[0], left, right) == 0 &&
        equ_set_display_TeX(result->solutions[0], left_notation, right_notation) == 0;
    string_free(left); string_free(right); string_free(left_notation); string_free(right_notation);
    left = valid ? string_sprintf("Affine complete integral: %s", plain[0]) : NULL;
    right = valid ? string_sprintf("%s; %s != 0; %s and %s are arbitrary constants.",
                                    plain[7], plain[2], plain[1], plain[4]) : NULL;
    left_notation = valid ? gradient_render(values, GRADIENT_LEFT_AFFINE) : NULL;
    right_notation = valid ? gradient_render(values, GRADIENT_RIGHT_AFFINE) : NULL;
    valid = valid && left && right && left_notation && right_notation &&
        equ_set_display_unbound(result->solutions[1], left, right) == 0 &&
        equ_set_display_TeX(result->solutions[1], left_notation, right_notation) == 0;
    string_free(left); string_free(right); string_free(left_notation); string_free(right_notation);
    if (valid && include_steps) {
        string_t *steps = string_sprintf(
            "Gradient-only equation: set p and q equal to the two first partial derivatives.\n"
            "The characteristic equations keep p and q constant along each characteristic.\n"
            "Parameterise the gradient curve by p = %s, q = %s, with %s != 0.\n"
            "Integrating constant gradients gives the affine complete integral.\n"
            "Replace its additive constant by %s(%s) and differentiate the resulting expression with respect to %s.\n"
            "Setting that derivative H to zero gives the envelope constraint.\n"
            "On H=0 the chain rule gives the original gradients p and q; the native solver verifies their PDE identity.\n"
            "dH/d%s != 0 determines a regular local branch. Affine solutions are listed separately.\n"
            "No initial data, boundary data or continuation through envelope singularities are asserted.",
            plain[1], plain[3], plain[2], plain[8], plain[1], plain[1], plain[1]);
        string_t *notation = gradient_render(values, GRADIENT_STEPS);
        valid = steps && notation && de_solve_result_set_steps(result, string_c_str(steps)) == 0 &&
                de_solve_result_set_steps_TeX(result, string_c_str(notation)) == 0;
        string_free(notation);
        string_free(steps);
    }
    for (size_t i = 0u; i < 9u; ++i)
        free(plain[i]);
    return valid;
}
