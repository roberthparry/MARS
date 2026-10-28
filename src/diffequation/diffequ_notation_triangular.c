/**
 * @file diffequ_notation_triangular.c
 * @brief Describe triangular characteristic solutions through semantic nodes.
 *
 * The solver supplies the verified characteristic field, invariant and particular
 * solution. The TeX renderer chooses the notation for their mathematical structure.
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

static const TeX_model_node_t *relation(notation_build_t *build, TeX_relation_t kind,
                                       const TeX_model_node_t *left, const TeX_model_node_t *right)
{
    const TeX_model_node_t *children[] = {left, right};
    return notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_RELATION,
                               .relation = kind, .child_count = 2u}, children);
}

/* Project the verified characteristic identities and their real-chart restrictions. */
bool de_triangular_steps(diffequ_solve_result_t *result, const expr_t *dependent, const expr_t *x, const expr_t *y,
                         const expr_t *first, const expr_t *second, const expr_t *forcing,
                         const expr_t *invariant, const expr_t *particular, bool repeated)
{
    const expr_t *values[] = {x, y, first, second, forcing, invariant, particular, dependent};
    const TeX_model_node_t *nodes[8] = {NULL};
    char *text[8] = {NULL};
    notation_build_t *build = notation_build_new();
    string_t *plain = NULL, *display = NULL;
    bool valid = result && build;
    for (size_t i = 0u; valid && i < 8u; ++i) {
        const char *name = expr_symbol_name(values[i]);
        expr_t *symbol = name && (expr_is_variable(values[i]) || expr_is_named_const(values[i]))
            ? expr_new_named_var(NUM_NAN, name) : NULL;
        const expr_t *value = symbol ? symbol : values[i];
        text[i] = expr_to_string(value, style_UNBOUND);
        nodes[i] = notation_build_expr(build, value);
        expr_free(symbol);
        valid = text[i] && nodes[i];
    }
    if (valid)
        plain = string_sprintf(
            "Triangular linear characteristic field.\n"
            "d%s/ds = %s, d%s/ds = %s, d%s/ds = %s.\n"
            "%s\nInvariant: I = %s. Particular solution: P = %s.\n"
            "Thus %s = P + F(I), with F an arbitrary differentiable function.\n"
            "The invariant and complete solution are verified on both real charts %s>0 and %s<0.\n"
            "This representation requires %s != 0; no extension across that axis is asserted.\n"
            "No initial or boundary data are imposed.\n", text[0], text[2], text[1], text[3], text[7], text[4],
            repeated ? "Equal diagonal rates produce a logarithmic invariant." : "Distinct diagonal rates give a scaled invariant.",
            text[5], text[6], text[7], text[0], text[0], text[0]);
    if (plain) {
        const TeX_model_node_t *s = notation_build_literal(build, TeX_NODE_IDENTIFIER, "s");
        const TeX_model_node_t *zero = notation_build_literal(build, TeX_NODE_NUMBER, "0");
        const TeX_model_node_t *index = notation_build_literal(build, TeX_NODE_IDENTIFIER, "I");
        const TeX_model_node_t *part = notation_build_literal(build, TeX_NODE_IDENTIFIER, "P");
        const TeX_model_node_t *family = pair(build, TeX_NODE_APPLY,
            notation_build_literal(build, TeX_NODE_FUNCTION_NAME, "F"), index);
        const size_t variables[] = {0u, 1u, 7u}, derivatives[] = {2u, 3u, 4u};
        const TeX_model_node_t *equations[3];
        for (size_t i = 0u; i < 3u; ++i) {
            const TeX_model_node_t *children[] = {nodes[variables[i]], s};
            const TeX_model_node_t *derivative = notation_build_node(build,
                (TeX_node_view_t){.kind = TeX_NODE_VARIABLE_DERIVATIVE, .child_count = 2u, .order = 1u}, children);
            equations[i] = pair(build, TeX_NODE_EQUATION, derivative, nodes[derivatives[i]]);
        }
        const TeX_model_node_t *charts[] = {
            relation(build, TeX_RELATION_GREATER, nodes[0], zero),
            relation(build, TeX_RELATION_LESS, nodes[0], zero)
        };
        const TeX_model_node_t *rows[] = {
            notation_build_literal(build, TeX_NODE_TEXT, "Triangular linear characteristic field."),
            notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_STATEMENTS, .child_count = 3u}, equations),
            notation_build_literal(build, TeX_NODE_TEXT, repeated ? "Equal diagonal rates produce a logarithmic invariant."
                                                                    : "Distinct diagonal rates give a scaled invariant."),
            pair(build, TeX_NODE_EQUATION, index, nodes[5]),
            pair(build, TeX_NODE_EQUATION, part, nodes[6]),
            pair(build, TeX_NODE_EQUATION, nodes[7], pair(build, TeX_NODE_SUM, part, family)),
            notation_build_literal(build, TeX_NODE_TEXT, "F is arbitrary and differentiable."),
            pair(build, TeX_NODE_STATEMENTS, notation_build_literal(build, TeX_NODE_TEXT, "Verified separately on"),
                notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_LIST, .child_count = 2u}, charts)),
            pair(build, TeX_NODE_STATEMENTS, relation(build, TeX_RELATION_NOT_EQUAL, nodes[0], zero),
                notation_build_literal(build, TeX_NODE_TEXT, "no extension across that axis asserted.")),
            notation_build_literal(build, TeX_NODE_TEXT, "No initial or boundary data imposed.")
        };
        const TeX_model_node_t *root = notation_build_node(build,
            (TeX_node_view_t){.kind = TeX_NODE_LINES, .child_count = sizeof rows / sizeof *rows}, rows);
        display = notation_build_render(build, root);
    }
    valid = plain && display && de_solve_result_set_steps(result, string_c_str(plain)) == 0 &&
            de_solve_result_set_steps_TeX(result, string_c_str(display)) == 0;
    string_free(display);
    string_free(plain);
    for (size_t i = 0u; i < 8u; ++i)
        free(text[i]);
    notation_build_free(build);
    return valid;
}
