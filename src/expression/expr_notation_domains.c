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
    const expr_t *lower_value;
    const expr_t *upper_value;
    const expr_t *root_signs;
    bool lower_inclusive;
    bool upper_inclusive;
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

/* Index only unconditionally evaluated logarithms; conditional, integral and other scoped operands stay opaque. */
static bool index_log_arguments(const expr_t *body, dictionary_t *arguments, unsigned depth)
{
    if (!body || depth >= 128u)
        return true;
    unsigned char flags = 0u;
    /* Bit one marks a required non-zero argument; bit two marks a visited algebraic node. */
    if (dictionary_get(arguments, &body, &flags) && (flags & 2u))
        return true;
    flags |= 2u;
    if (!dictionary_set(arguments, &body, &flags))
        return false;
    bool logarithm = expr_is_op(body, &ops_log) || expr_is_op(body, &ops_log10);
    if (logarithm) {
        const expr_t *argument = expr_domain_nonzero_base(body->a);
        flags = 0u;
        dictionary_get(arguments, &argument, &flags);
        flags |= 1u;
        if (!dictionary_set(arguments, &argument, &flags))
            return false;
    }
    if (!(logarithm || expr_is_addsub(body) || expr_is_mul(body) || expr_is_div(body) ||
          expr_is_neg(body) || expr_is_op(body, &ops_pow) || expr_is_sqrt_expr(body) || expr_is_op(body, &ops_abs)))
        return true;
    return index_log_arguments(body->a, arguments, depth + 1u) &&
           index_log_arguments(body->b, arguments, depth + 1u);
}

static bool real_constant(const expr_t *expression)
{
    if (expr_is_unnamed_const(expression))
        return num_is_real(expression->c) && num_is_finite(expression->c);
    expr_bindings_t *bindings = expr_bindings_from_expr_internal(expression);
    bool fixed = expr_bindings_count(bindings) == 0u;
    expr_bindings_free(bindings);
    if (!fixed)
        return false;
    number_t value = expr_eval((expr_t *)expression);
    bool real = num_is_real(value) && num_is_finite(value);
    num_destroy(&value);
    return real;
}

static bool known_real(const dictionary_t *coordinates, const expr_t *expression)
{
    real_interval_t interval;
    return expr_is_op(expression, &ops_real_bound) || expr_is_op(expression, &ops_imag_coordinate) ||
           dictionary_get(coordinates, &expression, &interval) || real_constant(expression);
}

/* Recognise the paired indicators of the principal square root's range, including its zero boundary. */
static bool principal_root_condition(const expr_t *pair, const expr_t **real, const expr_t **operand)
{
    if (!pair || !pair->b || !expr_const_is_zero(pair->b->a) ||
        !expr_is_op(pair->a, &ops_real_at_least) || !expr_const_is_zero(pair->a->b) ||
        !expr_is_op(pair->a->a, &ops_add))
        return false;
    const expr_t *a = pair->a->a->a, *b = pair->a->a->b;
    if (!expr_is_op(a, &ops_sgn) || !expr_is_op(b, &ops_sgn))
        return false;
    a = a->a;
    b = b->a;
    if (expr_is_op(a, &ops_imag_coordinate)) {
        const expr_t *swap = a;
        a = b;
        b = swap;
    }
    if (!expr_is_op(a, &ops_real_bound) || !expr_is_op(b, &ops_imag_coordinate) || !expr_struct_eq(a->a, b->a))
        return false;
    *real = a;
    *operand = a->a;
    return true;
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
        bool inclusive = expr_is_op(left, &ops_real_at_least);
        if (inclusive || expr_is_op(left, &ops_real_greater)) {
            right = left->b;
            left = left->a;
        }
        real_interval_t interval = {0};
        if (real_constant(right) && known_real(coordinates, left)) {
            dictionary_get(coordinates, &left, &interval);
            if (interval.lower)
                continue;
            interval.lower = pair;
            interval.lower_value = right;
            interval.lower_inclusive = inclusive;
            if (!dictionary_set(coordinates, &left, &interval))
                goto fail;
        }
        if (real_constant(left) && known_real(coordinates, right)) {
            interval = (real_interval_t){0};
            dictionary_get(coordinates, &right, &interval);
            if (interval.upper)
                continue;
            interval.upper = pair;
            interval.upper_value = left;
            interval.upper_inclusive = inclusive;
            if (!dictionary_set(coordinates, &right, &interval))
                goto fail;
        }
    }
    for (const expr_t *pair = expression->b; pair; pair = pair->b->b) {
        const expr_t *real = NULL, *operand = NULL;
        real_interval_t interval;
        if (principal_root_condition(pair, &real, &operand) && dictionary_get(coordinates, &real, &interval) &&
            interval.lower && interval.lower_inclusive && expr_const_is_zero(interval.lower_value)) {
            interval.root_signs = pair;
            if (!dictionary_set(coordinates, &real, &interval))
                goto fail;
        }
    }
    return coordinates;
fail:
    dictionary_destroy(coordinates);
    return NULL;
}

/* Express the same closed half-plane with its chosen imaginary boundary using a standard argument interval. */
static const TeX_model_node_t *build_root_range(notation_build_t *build, const expr_t *operand)
{
    const TeX_model_node_t *value = notation_build_expr(build, operand);
    const TeX_model_node_t *zero = notation_build_literal(build, TeX_NODE_NUMBER, "0");
    const TeX_model_node_t *equal[] = {value, zero};
    string_t *name = string_new_with("arg");
    const TeX_model_node_t *argument = name ? notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_CALL, .text = name, .child_count = 1u}, &value) : NULL;
    string_free(name);
    expr_t *pi = expr_new_named_const(NUM_PI, "@pi");
    expr_t *half = pi ? expr_div_long(pi, 2) : NULL;
    expr_t *negative = half ? expr_neg(half) : NULL;
    const TeX_model_node_t *bounds[] = {
        notation_build_expr(build, negative), argument, notation_build_expr(build, half)
    };
    const TeX_model_node_t *parts[] = {
        notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_RELATION,
            .relation = TeX_RELATION_EQUAL, .child_count = 2u}, equal),
        notation_build_literal(build, TeX_NODE_TEXT, "or"),
        notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_INTERVAL_CHAIN,
            .upper_inclusive = true, .child_count = 3u}, bounds)
    };
    const TeX_model_node_t *result = notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_STATEMENTS, .child_count = 3u}, parts);
    expr_free(negative);
    expr_free(half);
    expr_free(pi);
    return result;
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
    dictionary_t *log_arguments = dictionary_create(sizeof(const expr_t *), sizeof(unsigned char), coordinate_hash,
        coordinate_compare, NULL, NULL, NULL, NULL, NULL);
    if (log_arguments && !index_log_arguments(expression->a, log_arguments, 0u)) {
        dictionary_destroy(log_arguments);
        log_arguments = NULL;
    }
    size_t next = 1u;
    conditions[0] = body;
    for (const expr_t *pair = expression->b; pair; pair = pair->b->b) {
        const expr_t *nonzero = expr_domain_nonzero_operand(pair);
        unsigned char flags = 0u;
        if (nonzero && log_arguments && dictionary_get(log_arguments, &nonzero, &flags) && (flags & 1u))
            continue;
        real_interval_t interval;
        const expr_t *left = pair->a, *right = pair->b->a;
        const expr_t *real = NULL, *operand = NULL;
        if (principal_root_condition(pair, &real, &operand) && dictionary_get(coordinates, &real, &interval) &&
            interval.root_signs == pair) {
            conditions[next++] = build_root_range(build, operand);
            continue;
        }
        if (expr_is_op(left, &ops_real_parameter) &&
            dictionary_get(coordinates, &left->a, &interval) && (interval.lower || interval.upper))
            continue;
        if (expr_is_op(left, &ops_real_at_least) || expr_is_op(left, &ops_real_greater)) {
            right = left->b;
            left = left->a;
        }
        if (dictionary_get(coordinates, &left, &interval) && interval.lower == pair && interval.root_signs)
            continue;
        if (dictionary_get(coordinates, &right, &interval) && interval.upper == pair && interval.lower &&
            !interval.root_signs)
            continue;
        if (dictionary_get(coordinates, &left, &interval) && interval.lower == pair && interval.upper) {
            const TeX_model_node_t *chain[] = {
                notation_build_expr(build, interval.lower_value), notation_build_expr(build, left),
                notation_build_expr(build, interval.upper_value)
            };
            conditions[next++] = notation_build_node(build,
                (TeX_node_view_t){.kind = TeX_NODE_INTERVAL_CHAIN, .child_count = 3u,
                    .lower_inclusive = interval.lower_inclusive, .upper_inclusive = interval.upper_inclusive}, chain);
        } else {
            conditions[next++] = build_condition(build, pair, coordinates);
        }
    }
    dictionary_destroy(coordinates);
    dictionary_destroy(log_arguments);
    if (distribution) {
        const TeX_model_node_t *operand = notation_build_expr(build, distribution->a);
        string_t *caption = string_new_with(expr_is_op(distribution, &ops_principal_value)
            ? "principal value" : "finite part");
        conditions[next++] = caption ? notation_build_node(build, (TeX_node_view_t){
            .kind = TeX_NODE_DISTRIBUTION_CONDITION, .text = caption, .child_count = 1u}, &operand) : NULL;
        string_free(caption);
    }
    if (next == 1u) {
        free(conditions);
        return body;
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
