/**
 * @file expr_progression_notation.c
 * @brief Prepare finite-progression identities for semantic notation.
 *
 * Expression recognises each progression and selects the mathematical closed
 * form. The notation provider receives ordinary operations and symbols only.
 */
#include <stdlib.h>
#include <string.h>

#include "expression-t.h"

#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_notation_internal.h"

static const TeX_model_node_t *pair(notation_build_t *build, TeX_node_kind_t kind,
                                    const TeX_model_node_t *left, const TeX_model_node_t *right)
{
    const TeX_model_node_t *children[] = {left, right};
    return notation_build_node(build, (TeX_node_view_t){.kind = kind, .child_count = 2u}, children);
}

static const TeX_model_node_t *call(notation_build_t *build, const char *name, const TeX_model_node_t *argument)
{
    string_t *text = string_new_with(name);
    const TeX_model_node_t *node = text ? notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_CALL, .text = text, .child_count = 1u}, &argument) : NULL;
    string_free(text);
    return node;
}

static const TeX_model_node_t *symbolic_closed_form(notation_build_t *build, const expr_t *upper,
                                                    const expr_t *step, const expr_ops_t *function)
{
    const bool circular = function == &ops_sin || function == &ops_cos;
    const char *first = circular ? "sin" : "sinh";
    const TeX_model_node_t *n = notation_build_expr(build, upper);
    const TeX_model_node_t *x = expr_notation_name(build, step->name);
    const TeX_model_node_t *one = notation_build_literal(build, TeX_NODE_NUMBER, "1");
    const TeX_model_node_t *two = notation_build_literal(build, TeX_NODE_NUMBER, "2");
    const TeX_model_node_t *half_x = pair(build, TeX_NODE_FRACTION, x, two);
    const TeX_model_node_t *nx_half = pair(build, TeX_NODE_FRACTION,
        pair(build, TeX_NODE_PRODUCT, n, x), two);
    const TeX_model_node_t *second_arg = pair(build, TeX_NODE_PRODUCT, half_x,
        pair(build, TeX_NODE_SUM, n, one));
    const TeX_model_node_t *top = pair(build, TeX_NODE_PRODUCT, call(build, first, nx_half),
        call(build, function->function_name, second_arg));
    return pair(build, TeX_NODE_FRACTION, top, call(build, first, half_x));
}

/* Return an independently owned identity using the shared semantic renderer. */
char *expr_finite_progression_identity_TeX(const expr_t *expression)
{
    if (!expression)
        return NULL;
    const expr_t *upper = NULL;
    const expr_ops_t *function = NULL;
    expr_t *step = NULL, *closed = NULL;
    bool symbolic = false;
    bool recognised = expr_finite_progression_parts(expression, &upper, &step, &function);
    if (!recognised) {
        closed = expr_finite_exponential_sine_progression_closed_form(expression);
        if (!closed)
            closed = expr_finite_progression_scaled_closed_form(expression);
    } else if (!function->finite_progression) {
        expr_free(step);
        return NULL;
    } else if (!step->name || !*step->name ||
               (function != &ops_sin && function != &ops_cos && function != &ops_sinh && function != &ops_cosh)) {
        closed = function->finite_progression(upper, step);
    } else
        symbolic = true;
    notation_build_t *build = notation_build_new();
    const TeX_model_node_t *left = build ? notation_build_expr(build, expression) : NULL;
    const TeX_model_node_t *right = !build ? NULL : closed ? notation_build_expr(build, closed)
        : symbolic ? symbolic_closed_form(build, upper, step, function) : NULL;
    const TeX_model_node_t *identity = left && right ? pair(build, TeX_NODE_EQUATION, left, right) : NULL;
    string_t *output = identity ? notation_build_render(build, identity) : NULL;
    char *result = output ? strdup(string_c_str(output)) : NULL;
    string_free(output);
    notation_build_free(build);
    expr_free(closed);
    expr_free(step);
    return result;
}
