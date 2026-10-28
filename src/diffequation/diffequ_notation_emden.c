/**
 * @file diffequ_notation_emden.c
 * @brief Describe the modified Emden linearisation as mathematical nodes.
 *
 * The differential-equation solver owns the substitution and point transformation;
 * TeX presents their semantic structure without inspecting native solver objects.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diffequation-t.h"
#include "TeX-t.h"

#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"
#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_notation_internal.h"

static const TeX_model_node_t *pair(notation_build_t *build, TeX_node_kind_t kind,
                                   const TeX_model_node_t *left, const TeX_model_node_t *right)
{
    const TeX_model_node_t *children[] = {left, right};
    return notation_build_node(build, (TeX_node_view_t){.kind = kind, .child_count = 2u}, children);
}

static const TeX_model_node_t *prime(notation_build_t *build, const TeX_model_node_t *value, unsigned order)
{
    return notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_PRIME, .child_count = 1u, .order = order}, &value);
}

static const TeX_model_node_t *not_zero(notation_build_t *build, const TeX_model_node_t *value,
                                       const TeX_model_node_t *zero)
{
    const TeX_model_node_t *children[] = {value, zero};
    return notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_RELATION,
                               .relation = TeX_RELATION_NOT_EQUAL, .child_count = 2u}, children);
}

/* Explain both the auxiliary linear equation and its point transformation. */
bool de_modified_emden_steps(const expr_t *independent, const expr_t *dependent, const expr_t *scale,
                             char **steps_out, char **steps_TeX_out)
{
    if (!steps_out || !steps_TeX_out)
        return false;
    *steps_out = NULL;
    *steps_TeX_out = NULL;
    const char *x = independent ? expr_symbol_name(independent) : NULL;
    char *y = dependent ? de_emden_symbolic_text(dependent) : NULL;
    char *a = scale ? de_emden_symbolic_text(scale) : NULL;
    bool unit = a && strcmp(a, "1") == 0;
    char *ay = NULL, *au = NULL, *ay_den = NULL, *au_den = NULL;
    if (a && y) {
        if (asprintf(&ay, "%s%s", unit ? "" : a, y) < 0 ||
            asprintf(&au, "%su", unit ? "" : a) < 0 ||
            asprintf(&ay_den, unit ? "%s" : "(%s)", ay) < 0 ||
            asprintf(&au_den, unit ? "%s" : "(%s)", au) < 0) {
            free(ay_den); ay_den = NULL;
        }
    }
    string_t *plain = x && y && ay_den && au_den ? string_sprintf(
        "Modified Emden linearisation\n"
        "Set %s = (1/%s)u′, with u non-zero. Then\n"
        "      (1/%s)u‴ = 0, hence u‴ = 0.\n"
        "Thus u = A%s² + B%s + C; a common non-zero factor cancels from %s.\n"
        "Equivalently, the point transformation is\n"
        "      X = %s − 1/%s\n"
        "      Y = %s/%s − %s²/2\n"
        "and d²Y/dX² = 0.", y, au_den, au_den, x, x, y, x, ay_den, x, ay_den, x) : NULL;
    notation_build_t *build = plain ? notation_build_new() : NULL;
    string_t *rendered = NULL;
    if (build) {
        const TeX_model_node_t *x_node = notation_build_expr(build, independent);
        const TeX_model_node_t *y_node = notation_build_expr(build, dependent);
        const TeX_model_node_t *factor = notation_build_expr(build, scale);
        const TeX_model_node_t *u = notation_build_literal(build, TeX_NODE_IDENTIFIER, "u");
        const TeX_model_node_t *one = notation_build_literal(build, TeX_NODE_NUMBER, "1");
        const TeX_model_node_t *two = notation_build_literal(build, TeX_NODE_NUMBER, "2");
        const TeX_model_node_t *zero = notation_build_literal(build, TeX_NODE_NUMBER, "0");
        const TeX_model_node_t *A = notation_build_literal(build, TeX_NODE_IDENTIFIER, "A");
        const TeX_model_node_t *B = notation_build_literal(build, TeX_NODE_IDENTIFIER, "B");
        const TeX_model_node_t *C = notation_build_literal(build, TeX_NODE_IDENTIFIER, "C");
        const TeX_model_node_t *X = notation_build_literal(build, TeX_NODE_IDENTIFIER, "X");
        const TeX_model_node_t *Y = notation_build_literal(build, TeX_NODE_IDENTIFIER, "Y");
        const TeX_model_node_t *au_node = unit ? u : pair(build, TeX_NODE_PRODUCT, factor, u);
        const TeX_model_node_t *ay_node = unit ? y_node : pair(build, TeX_NODE_PRODUCT, factor, y_node);
        const TeX_model_node_t *inverse_au = pair(build, TeX_NODE_FRACTION, one, au_node);
        const TeX_model_node_t *square = pair(build, TeX_NODE_POWER, x_node, two);
        const TeX_model_node_t *polynomial = pair(build, TeX_NODE_SUM,
            pair(build, TeX_NODE_SUM, pair(build, TeX_NODE_PRODUCT, A, square),
                 pair(build, TeX_NODE_PRODUCT, B, x_node)), C);
        const TeX_model_node_t *coordinates[] = {Y, X};
        const TeX_model_node_t *second = notation_build_node(build,
            (TeX_node_view_t){.kind = TeX_NODE_VARIABLE_DERIVATIVE, .child_count = 2u, .order = 2u}, coordinates);
        const TeX_model_node_t *rows[] = {
            notation_build_literal(build, TeX_NODE_TEXT, "Modified Emden linearisation"),
            pair(build, TeX_NODE_STATEMENTS,
                pair(build, TeX_NODE_EQUATION, y_node, pair(build, TeX_NODE_PRODUCT, inverse_au, prime(build, u, 1u))),
                not_zero(build, u, zero)),
            pair(build, TeX_NODE_EQUATION, pair(build, TeX_NODE_PRODUCT, inverse_au, prime(build, u, 3u)), zero),
            pair(build, TeX_NODE_EQUATION, prime(build, u, 3u), zero),
            pair(build, TeX_NODE_EQUATION, u, polynomial),
            notation_build_literal(build, TeX_NODE_TEXT, "Point transformation:"),
            pair(build, TeX_NODE_EQUATION, X,
                pair(build, TeX_NODE_DIFFERENCE, x_node, pair(build, TeX_NODE_FRACTION, one, ay_node))),
            pair(build, TeX_NODE_EQUATION, Y,
                pair(build, TeX_NODE_DIFFERENCE, pair(build, TeX_NODE_FRACTION, x_node, ay_node),
                     pair(build, TeX_NODE_FRACTION, square, two))),
            pair(build, TeX_NODE_EQUATION, second, zero)
        };
        const TeX_model_node_t *root = notation_build_node(build,
            (TeX_node_view_t){.kind = TeX_NODE_LINES, .child_count = sizeof rows / sizeof *rows}, rows);
        rendered = notation_build_render(build, root);
    }
    bool success = plain && rendered && (*steps_out = strdup(string_c_str(plain))) &&
                   (*steps_TeX_out = strdup(string_c_str(rendered)));
    if (!success) {
        free(*steps_out); free(*steps_TeX_out);
        *steps_out = NULL; *steps_TeX_out = NULL;
    }
    notation_build_free(build);
    string_free(rendered); string_free(plain);
    free(au_den); free(ay_den); free(au); free(ay); free(a); free(y);
    return success;
}
