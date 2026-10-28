/**
 * @file expr_notation_domains.c
 * @brief Translate expression domain conditions into mathematical relations.
 *
 * The expression module owns the linked condition encoding and recognises
 * non-zero constraints, set membership and convergence half-planes. TeX sees
 * only ordinary relations, number sets and function applications.
 */
#include <stdlib.h>

#include "expression-t.h"
#include "TeX-t.h"

#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_notation_internal.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"

static const TeX_model_node_t *build_condition(notation_build_t *build, const expr_t *pair)
{
    if (!pair || !pair->a || !pair->b)
        return NULL;
    const expr_t *nonzero = expr_domain_nonzero_operand(pair);
    TeX_relation_t relation;
    const TeX_model_node_t *parts[2];
    if (nonzero) {
        relation = TeX_RELATION_NOT_EQUAL;
        parts[0] = notation_build_expr(build, nonzero);
        parts[1] = notation_build_literal(build, TeX_NODE_NUMBER, "0");
    } else if (expr_is_op(pair->a, &ops_nonnegative_integer) || expr_is_op(pair->a, &ops_real_parameter)) {
        relation = TeX_RELATION_MEMBER;
        parts[0] = notation_build_expr(build, pair->a->a);
        parts[1] = notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_NUMBER_SET,
            .number_set = expr_is_op(pair->a, &ops_real_parameter)
                ? TeX_SET_REALS : TeX_SET_NONNEGATIVE_INTEGERS}, NULL);
    } else {
        relation = TeX_RELATION_GREATER;
        const TeX_model_node_t *argument = notation_build_expr(build, pair->a);
        parts[0] = notation_build_node(build,
            (TeX_node_view_t){.kind = TeX_NODE_REAL_PART, .child_count = 1u}, &argument);
        parts[1] = notation_build_expr(build, pair->b->a);
    }
    return notation_build_node(build,
        (TeX_node_view_t){.kind = expr_notation_results_enabled(build) ? TeX_NODE_DOMAIN_RELATION : TeX_NODE_RELATION,
                          .relation = relation, .child_count = 2u}, parts);
}

/* Preserve every supplied restriction, including its native order and symbolic operands. */
const TeX_model_node_t *expr_notation_domain(notation_build_t *build, const expr_t *expression)
{
    size_t count = 0u;
    const expr_t *distribution = expr_notation_domain_distribution(build, expression);
    /* Both passes visit authored conditions, never search for a dispatch key. */
    for (const expr_t *pair = expression->b; pair; pair = pair->b->b) {
        if (!pair->b || ++count >= 65536u)
            return NULL;
    }
    const TeX_model_node_t *body = notation_build_expr(build, expression->a);
    if (!count && !distribution)
        return body;
    const TeX_model_node_t **conditions = calloc(count + 1u + (distribution ? 1u : 0u), sizeof *conditions);
    if (!conditions)
        return NULL;
    size_t next = 1u;
    conditions[0] = body;
    for (const expr_t *pair = expression->b; pair; pair = pair->b->b)
        conditions[next++] = build_condition(build, pair);
    if (distribution) {
        const TeX_model_node_t *operand = notation_build_expr(build, distribution->a);
        string_t *caption = string_new_with(expr_is_op(distribution, &ops_principal_value)
            ? "principal value" : "finite part");
        conditions[next++] = caption ? notation_build_node(build, (TeX_node_view_t){
            .kind = TeX_NODE_DISTRIBUTION_CONDITION, .text = caption, .child_count = 1u}, &operand) : NULL;
        string_free(caption);
    }
    if (!expr_notation_results_enabled(build)) {
        const TeX_model_node_t *list = notation_build_node(build,
            (TeX_node_view_t){.kind = TeX_NODE_LIST, .child_count = count}, conditions + 1u);
        const TeX_model_node_t *parts[] = {body, notation_build_node(build,
            (TeX_node_view_t){.kind = TeX_NODE_FENCED, .fence = TeX_FENCE_PARENTHESES, .child_count = 1u}, &list)};
        free(conditions);
        return notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_STATEMENTS, .child_count = 2u}, parts);
    }
    const TeX_model_node_t *result = notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_DOMAIN, .domain_qualification = distribution != NULL,
                          .child_count = next}, conditions);
    free(conditions);
    return result;
}
