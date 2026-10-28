/**
 * @file expr_notation_functions.c
 * @brief Interpret packed special-function arguments for semantic presentation.
 *
 * Packed expression layouts and parameter groupings are private to expression.
 * These adapters unpack them into ordinary applications, indices and lists so
 * TeX needs no knowledge of expression encodings or operation tables.
 */
#include <stdlib.h>

#include "expression-t.h"
#include "TeX-t.h"

#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_notation_internal.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"

static const TeX_model_node_t *node(notation_build_t *build, TeX_node_kind_t kind,
                                   size_t count, const TeX_model_node_t *const *children)
{
    return notation_build_node(build, (TeX_node_view_t){.kind = kind, .child_count = count}, children);
}

static const TeX_model_node_t *parameter_list(notation_build_t *build, const expr_t *const *parameters, size_t count)
{
    if (!count)
        return notation_build_literal(build, TeX_NODE_TEXT, "-");
    if (count > 65536u)
        return NULL;
    const TeX_model_node_t **children = calloc(count, sizeof *children);
    if (!children)
        return NULL;
    for (size_t i = 0u; i < count; ++i)
        children[i] = notation_build_expr(build, parameters[i]);
    const TeX_model_node_t *result = node(build, TeX_NODE_LIST, count, children);
    free(children);
    return result;
}

static const TeX_model_node_t *size_node(notation_build_t *build, size_t count)
{
    string_t *text = string_sprintf("%zu", count);
    const TeX_model_node_t *result = text ? notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_NUMBER, .text = text}, NULL) : NULL;
    string_free(text);
    return result;
}

static const TeX_model_node_t *lerch(notation_build_t *build, const expr_t *expression)
{
    const expr_t *z, *s, *a;
    if (!expr_lerch_phi_unpack(expression, &z, &s, &a))
        return NULL;
    const TeX_model_node_t *children[] = {
        notation_build_literal(build, TeX_NODE_FUNCTION_NAME, "lerchphi"),
        notation_build_expr(build, z), notation_build_expr(build, s), notation_build_expr(build, a)
    };
    return node(build, TeX_NODE_APPLY, 4u, children);
}

static const TeX_model_node_t *lommel(notation_build_t *build, const expr_t *expression)
{
    const expr_t *mu, *nu, *argument;
    if (!expr_lommel_s_unpack(expression, &mu, &nu, &argument))
        return NULL;
    const expr_t *parameters[] = {mu, nu};
    const TeX_model_node_t *indexed[] = {
        notation_build_literal(build, TeX_NODE_FUNCTION_NAME, "lommels"), parameter_list(build, parameters, 2u)
    };
    const TeX_model_node_t *children[] = {
        node(build, TeX_NODE_SUBSCRIPT, 2u, indexed), notation_build_expr(build, argument)
    };
    return node(build, TeX_NODE_APPLY, 2u, children);
}

static const TeX_model_node_t *appell(notation_build_t *build, const expr_t *expression)
{
    const expr_t *a, *b1, *b2, *c, *x, *y;
    if (!expr_appell_f1_unpack(expression, &a, &b1, &b2, &c, &x, &y))
        return NULL;
    const expr_t *parameters[] = {b1, b2}, *variables[] = {x, y};
    const TeX_model_node_t *groups[] = {
        notation_build_expr(build, a), parameter_list(build, parameters, 2u),
        notation_build_expr(build, c), parameter_list(build, variables, 2u)
    };
    const TeX_model_node_t *children[] = {
        notation_build_literal(build, TeX_NODE_FUNCTION_NAME, "appellf1"),
        node(build, TeX_NODE_PARAMETER_GROUPS, 4u, groups)
    };
    return node(build, TeX_NODE_APPLY, 2u, children);
}

static const TeX_model_node_t *lauricella(notation_build_t *build, const expr_t *expression)
{
    const expr_t *a, **parameters = NULL, *c, **variables = NULL;
    size_t count = 0u;
    if (!expr_lauricella_f_unpack(expression, &a, &parameters, &c, &variables, &count))
        return NULL;
    const TeX_model_node_t *groups[] = {
        notation_build_expr(build, a), parameter_list(build, parameters, count),
        notation_build_expr(build, c), parameter_list(build, variables, count)
    };
    free(variables);
    free(parameters);
    const TeX_model_node_t *order = size_node(build, count);
    const TeX_model_node_t *fenced = notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_FENCED, .child_count = 1u, .fence = TeX_FENCE_PARENTHESES}, &order);
    const TeX_model_node_t *indexed[] = {
        notation_build_literal(build, TeX_NODE_FUNCTION_NAME, "lauricellaf"), fenced
    };
    const TeX_model_node_t *children[] = {
        node(build, TeX_NODE_POWER, 2u, indexed), node(build, TeX_NODE_PARAMETER_GROUPS, 4u, groups)
    };
    return node(build, TeX_NODE_APPLY, 2u, children);
}

static const TeX_model_node_t *hypergeometric(notation_build_t *build, const expr_t *expression)
{
    const expr_t **upper = NULL, **lower = NULL, *argument;
    size_t upper_count = 0u, lower_count = 0u;
    if (!expr_hypergeometric_pFq_unpack(expression, &upper, &upper_count, &lower, &lower_count, &argument))
        return NULL;
    const TeX_model_node_t *groups[] = {
        parameter_list(build, upper, upper_count), parameter_list(build, lower, lower_count),
        notation_build_expr(build, argument)
    };
    free(lower);
    free(upper);
    const TeX_model_node_t *indexed[] = {
        notation_build_literal(build, TeX_NODE_FUNCTION_NAME, "hypergeometricpfq"), size_node(build, lower_count)
    };
    const TeX_model_node_t *prescript[] = {
        node(build, TeX_NODE_SUBSCRIPT, 2u, indexed), size_node(build, upper_count)
    };
    const TeX_model_node_t *children[] = {
        node(build, TeX_NODE_LEFT_SUBSCRIPT, 2u, prescript), node(build, TeX_NODE_PARAMETER_GROUPS, 3u, groups)
    };
    return node(build, TeX_NODE_APPLY, 2u, children);
}

/* Operation identifiers index native unpackers directly; no function-name search is involved. */
const TeX_model_node_t *expr_notation_packed_function(notation_build_t *build, const expr_t *expression)
{
    typedef const TeX_model_node_t *(*unpack_fn)(notation_build_t *, const expr_t *);
    static const unpack_fn unpackers[EXPR_KIND_COUNT] = {
        [EXPR_KIND_LERCH_PHI]          = lerch,
        [EXPR_KIND_LOMMEL_S]           = lommel,
        [EXPR_KIND_APPELL_F1]          = appell,
        [EXPR_KIND_LAURICELLA_F]       = lauricella,
        [EXPR_KIND_HYPERGEOMETRIC_PFQ] = hypergeometric
    };
    unsigned kind = expression && expression->ops ? expression->ops->kind : EXPR_KIND_COUNT;
    return kind < EXPR_KIND_COUNT && unpackers[kind] ? unpackers[kind](build, expression) : NULL;
}
