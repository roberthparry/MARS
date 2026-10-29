/**
 * @file expr_series_identities.c
 * @brief Prepare recognised product identities and trigonometric sums as mathematical snapshots.
 *
 * Expression owns the identities, index choices and closed forms. TeX receives ordinary semantic nodes and
 * decides their notation without knowing which series recogniser selected them. Parser callers use the private
 * expr_series_notation.h interface; applications use expression-t.h.
 */
#include <stdlib.h>

#include "expression-t.h"

#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_notation_internal.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_series_notation.h"

static const TeX_model_node_t *pair(notation_build_t *build, TeX_node_kind_t kind,
                                    const TeX_model_node_t *left, const TeX_model_node_t *right)
{
    const TeX_model_node_t *parts[] = {left, right};
    return notation_build_node(build, (TeX_node_view_t){.kind = kind, .child_count = 2u}, parts);
}

static const TeX_model_node_t *indexed(notation_build_t *build, TeX_node_kind_t kind,
                                       const TeX_model_node_t *body, const TeX_model_node_t *index,
                                       const TeX_model_node_t *lower, const TeX_model_node_t *upper)
{
    const TeX_model_node_t *parts[] = {body, index, lower, upper};
    return notation_build_node(build, (TeX_node_view_t){.kind = kind, .child_count = 4u}, parts);
}

/* Select the telescoping identity without evaluating its symbolic upper endpoint. */
string_t *expr_series_telescoping_identity_TeX(const char *endpoint)
{
    expr_t *upper = endpoint ? expr_from_string(endpoint, NULL) : NULL;
    notation_build_t *build = upper ? notation_build_new() : NULL;
    const TeX_model_node_t *one = notation_build_literal(build, TeX_NODE_NUMBER, "1");
    const TeX_model_node_t *two = notation_build_literal(build, TeX_NODE_NUMBER, "2");
    const TeX_model_node_t *k = notation_build_literal(build, TeX_NODE_IDENTIFIER, "k");
    const TeX_model_node_t *n = upper ? notation_build_expr(build, upper) : NULL;
    const TeX_model_node_t *product = indexed(build, TeX_NODE_INDEXED_PRODUCT,
        pair(build, TeX_NODE_SUM, one, pair(build, TeX_NODE_FRACTION, one, k)), k, two, n);
    const TeX_model_node_t *quotient = pair(build, TeX_NODE_FRACTION, pair(build, TeX_NODE_SUM, n, one), two);
    string_t *result = notation_build_render(build, pair(build, TeX_NODE_EQUATION, product, quotient));
    notation_build_free(build);
    expr_free(upper);
    return result;
}

/* Choose the appropriate reciprocal-prime product and its known limiting value. */
string_t *expr_series_prime_identity_TeX(bool positive)
{
    notation_build_t *build = notation_build_new();
    const TeX_model_node_t *p = notation_build_literal(build, TeX_NODE_IDENTIFIER, "p");
    const TeX_model_node_t *one = notation_build_literal(build, TeX_NODE_NUMBER, "1");
    const TeX_model_node_t *body = pair(build, positive ? TeX_NODE_SUM : TeX_NODE_DIFFERENCE,
        one, pair(build, TeX_NODE_FRACTION, one, p));
    const TeX_model_node_t *members[] = {p, notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_NUMBER_SET, .number_set = TeX_SET_PRIMES}, NULL)};
    const TeX_model_node_t *condition = notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_RELATION, .relation = TeX_RELATION_MEMBER, .child_count = 2u}, members);
    const TeX_model_node_t *parts[] = {body, condition};
    const TeX_model_node_t *product = notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_CONDITIONAL_PRODUCT, .child_count = 2u}, parts);
    const TeX_model_node_t *limit = notation_build_literal(build,
        positive ? TeX_NODE_IDENTIFIER : TeX_NODE_NUMBER, positive ? "∞" : "0");
    string_t *result = notation_build_render(build, pair(build, TeX_NODE_EQUATION, product, limit));
    notation_build_free(build);
    return result;
}

/* Preserve the recognised angle progression and optional reciprocal index weight. */
string_t *expr_series_trigonometric_sum_TeX(const expr_t *endpoint, const char *function,
                                           const expr_t *angle, bool weighted)
{
    notation_build_t *build = endpoint && function ? notation_build_new() : NULL;
    const TeX_model_node_t *k = notation_build_literal(build, TeX_NODE_IDENTIFIER, "k");
    const TeX_model_node_t *argument = angle ? pair(build, TeX_NODE_PRODUCT, k, notation_build_expr(build, angle)) : k;
    string_t *name = function ? string_new_with(function) : NULL;
    const TeX_model_node_t *body = notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_CALL, .text = name, .child_count = 1u}, &argument);
    if (weighted)
        body = pair(build, TeX_NODE_FRACTION, body, k);
    const TeX_model_node_t *sum = indexed(build, TeX_NODE_SUMMATION, body, k,
        notation_build_literal(build, TeX_NODE_NUMBER, "1"), notation_build_expr(build, endpoint));
    string_t *result = notation_build_render(build, sum);
    string_free(name);
    notation_build_free(build);
    return result;
}
