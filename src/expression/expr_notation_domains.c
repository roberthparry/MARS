/**
 * @file expr_notation_domains.c
 * @brief Translate expression domain conditions into mathematical relations.
 *
 * The expression module owns the linked condition encoding and recognises
 * non-zero constraints, set membership and convergence half-planes. TeX sees
 * only ordinary relations, number sets and function applications.
 */
#include <stdint.h>
#include <stdlib.h>

#include "dictionary-t.h"
#include "expression-t.h"
#include "TeX-t.h"

#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_notation_internal.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"

typedef struct {
    const expr_t *lower;
    const expr_t *upper;
} real_interval_t;

static size_t coordinate_hash(const void *key)
{
    return expr_struct_hash(*(const expr_t *const *)key);
}

static int coordinate_compare(const void *a, const void *b)
{
    const expr_t *left = *(const expr_t *const *)a, *right = *(const expr_t *const *)b;
    return expr_struct_eq(left, right) ? 0 : (uintptr_t)left < (uintptr_t)right ? -1 : 1;
}

static bool real_constant(const expr_t *expression)
{
    return expr_is_const(expression) && num_is_real(expression->c) && num_is_finite(expression->c);
}

static bool known_real(const dictionary_t *coordinates, const expr_t *expression)
{
    real_interval_t interval;
    return dictionary_get(coordinates, &expression, &interval) || real_constant(expression);
}

/* Index declared real operands once; matching bounds never search the condition list. */
static dictionary_t *index_real_intervals(const expr_t *expression)
{
    dictionary_t *coordinates = dictionary_create(sizeof(const expr_t *), sizeof(real_interval_t),
        coordinate_hash, coordinate_compare, NULL, NULL, NULL, NULL, NULL);
    if (!coordinates)
        return NULL;
    for (const expr_t *pair = expression->b; pair; pair = pair->b->b) {
        if (expr_is_op(pair->a, &ops_real_parameter)) {
            const expr_t *operand = pair->a->a;
            real_interval_t interval = {0};
            if (!dictionary_set(coordinates, &operand, &interval))
                goto fail;
        }
    }
    for (const expr_t *pair = expression->b; pair; pair = pair->b->b) {
        const expr_t *left = pair->a, *right = pair->b->a;
        real_interval_t interval;
        if (real_constant(right) && dictionary_get(coordinates, &left, &interval) && !interval.lower) {
            interval.lower = pair;
            if (!dictionary_set(coordinates, &left, &interval))
                goto fail;
        }
        if (real_constant(left) && dictionary_get(coordinates, &right, &interval) && !interval.upper) {
            interval.upper = pair;
            if (!dictionary_set(coordinates, &right, &interval))
                goto fail;
        }
    }
    return coordinates;
fail:
    dictionary_destroy(coordinates);
    return NULL;
}

static const TeX_model_node_t *build_condition(notation_build_t *build, const expr_t *pair,
                                               const dictionary_t *coordinates)
{
    if (!pair || !pair->a || !pair->b)
        return NULL;
    const expr_t *nonzero = expr_domain_nonzero_operand(pair);
    TeX_relation_t relation;
    const TeX_model_node_t *parts[2];
    if (expr_is_op(pair->a, &ops_real_greater) || expr_is_op(pair->a, &ops_real_at_least)) {
        relation = expr_is_op(pair->a, &ops_real_at_least) ? TeX_RELATION_GREATER_EQUAL : TeX_RELATION_GREATER;
        parts[0] = notation_build_expr(build, pair->a->a);
        parts[1] = notation_build_expr(build, pair->a->b);
    } else if (nonzero) {
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
        parts[0] = known_real(coordinates, pair->a) ? argument : notation_build_node(build,
            (TeX_node_view_t){.kind = TeX_NODE_REAL_PART, .child_count = 1u}, &argument);
        parts[1] = notation_build_expr(build, pair->b->a);
    }
    return notation_build_node(build,
        (TeX_node_view_t){.kind = expr_notation_results_enabled(build) ? TeX_NODE_DOMAIN_RELATION : TeX_NODE_RELATION,
                          .relation = relation, .child_count = 2u}, parts);
}

/* Preserve every restriction, combining numeric endpoint pairs without moving checks into the algebra. */
const TeX_model_node_t *expr_notation_domain(notation_build_t *build, const expr_t *expression)
{
    size_t count = 0u;
    const expr_t *distribution = expr_notation_domain_distribution(build, expression);
    /* Traverse authored conditions; the coordinate dictionary supplies all matching and lookup. */
    for (const expr_t *pair = expression->b; pair; pair = pair->b->b) {
        if (!pair->b || ++count >= 65536u)
            return NULL;
    }
    const TeX_model_node_t *body = notation_build_expr(build, expression->a);
    if (!count && !distribution)
        return body;
    const TeX_model_node_t **conditions = calloc(count + 1u + (distribution ? 1u : 0u), sizeof *conditions);
    dictionary_t *coordinates = index_real_intervals(expression);
    if (!conditions || !coordinates) {
        free(conditions);
        dictionary_destroy(coordinates);
        return NULL;
    }
    size_t next = 1u;
    conditions[0] = body;
    for (const expr_t *pair = expression->b; pair; pair = pair->b->b) {
        real_interval_t interval;
        const expr_t *left = pair->a, *right = pair->b->a;
        if (expr_is_op(left, &ops_real_parameter) &&
            dictionary_get(coordinates, &left->a, &interval) && (interval.lower || interval.upper))
            continue;
        if (dictionary_get(coordinates, &right, &interval) && interval.upper == pair && interval.lower)
            continue;
        if (dictionary_get(coordinates, &left, &interval) && interval.lower == pair && interval.upper) {
            const TeX_model_node_t *chain[] = {
                notation_build_expr(build, right), notation_build_expr(build, left),
                notation_build_expr(build, interval.upper->a)
            };
            conditions[next++] = notation_build_node(build,
                (TeX_node_view_t){.kind = TeX_NODE_RELATION, .relation = TeX_RELATION_LESS, .child_count = 3u}, chain);
        } else {
            conditions[next++] = build_condition(build, pair, coordinates);
        }
    }
    dictionary_destroy(coordinates);
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
            (TeX_node_view_t){.kind = TeX_NODE_LIST, .child_count = next - 1u}, conditions + 1u);
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
