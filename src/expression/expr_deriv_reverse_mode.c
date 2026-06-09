#include <stddef.h>
#include <stdlib.h>

#include "expr_internal.h"

static number_t expr_reverse_zero(void)
{
    return NUM_ZERO;
}

static int reverse_find_node(const expr_t *const *nodes,
                             size_t count,
                             const expr_t *target)
{
    for (size_t i = 0; i < count; ++i)
        if (nodes[i] == target)
            return (int)i;
    return -1;
}

static int reverse_append_node(const expr_t ***nodes,
                               size_t *count,
                               size_t *capacity,
                               const expr_t *node)
{
    const expr_t **grown;
    size_t new_capacity;

    if (*count < *capacity) {
        (*nodes)[(*count)++] = node;
        return 0;
    }

    new_capacity = *capacity ? (*capacity * 2u) : 32u;
    grown = realloc((void *)*nodes, new_capacity * sizeof(*grown));
    if (!grown)
        return -1;
    *nodes = grown;
    *capacity = new_capacity;
    (*nodes)[(*count)++] = node;
    return 0;
}

static int reverse_collect_postorder(const expr_t *node,
                                     const expr_t ***nodes,
                                     size_t *count,
                                     size_t *capacity)
{
    if (!node)
        return 0;
    if (reverse_find_node(*nodes, *count, node) >= 0)
        return 0;
    if (node->ops->arity != EXPR_OP_ATOM &&
        reverse_collect_postorder(node->a, nodes, count, capacity) != 0)
        return -1;
    if (node->ops->arity == EXPR_OP_BINARY &&
        reverse_collect_postorder(node->b, nodes, count, capacity) != 0)
        return -1;
    return reverse_append_node(nodes, count, capacity, node);
}

static void reverse_destroy_numbers(number_t *values, size_t count)
{
    if (!values)
        return;
    for (size_t i = 0; i < count; ++i)
        num_destroy(&values[i]);
}

int expr_eval_derivatives(const expr_t *expr,
                        size_t nvars,
                        const expr_t *const *vars,
                        number_t *value_out,
                        number_t *derivs_out)
{
    const expr_t **nodes = NULL;
    size_t node_count = 0u;
    size_t node_capacity = 0u;
    number_t *bars = NULL;
    number_t value;

    if (!expr || (nvars > 0u && (!vars || !derivs_out)))
        return -1;

    value = expr_eval(expr);
    if (reverse_collect_postorder(expr, &nodes, &node_count, &node_capacity) != 0) {
        free(nodes);
        num_destroy(&value);
        return -1;
    }

    bars = malloc(node_count * sizeof(*bars));
    if (!bars) {
        free(nodes);
        num_destroy(&value);
        return -1;
    }
    for (size_t i = 0; i + 1u < node_count; ++i)
        bars[i] = expr_reverse_zero();
    bars[node_count - 1u] = NUM_ONE;

    for (size_t i = node_count; i-- > 0u;) {
        const expr_t *node = nodes[i];
        int a_index;
        int b_index;

        if (node->ops->arity == EXPR_OP_ATOM || num_is_zero(bars[i]))
            continue;

        NUM_SCOPE(scope);
        number_t a_bar = (number_t){0};
        number_t b_bar = (number_t){0};
        node->ops->reverse(node, &bars[i], &a_bar, &b_bar);

        if (node->ops->arity != EXPR_OP_ATOM) {
            number_t accum;

            a_index = reverse_find_node(nodes, node_count, node->a);
            if (a_index < 0) {
                reverse_destroy_numbers(bars, node_count);
                free(bars);
                free(nodes);
                num_destroy(&value);
                return -1;
            }
            accum = num_add(bars[a_index], a_bar);
            num_destroy(&bars[a_index]);
            bars[a_index] = num_scope_detach(accum);
        }
        if (node->ops->arity == EXPR_OP_BINARY && node->b) {
            number_t accum;

            b_index = reverse_find_node(nodes, node_count, node->b);
            if (b_index < 0) {
                reverse_destroy_numbers(bars, node_count);
                free(bars);
                free(nodes);
                num_destroy(&value);
                return -1;
            }
            accum = num_add(bars[b_index], b_bar);
            num_destroy(&bars[b_index]);
            bars[b_index] = num_scope_detach(accum);
        }
    }

    if (value_out)
        *value_out = value;
    else
        num_destroy(&value);

    for (size_t i = 0; i < nvars; ++i) {
        int index = reverse_find_node(nodes, node_count, vars[i]);

        derivs_out[i] = index >= 0 ? num_clone(bars[index]) : expr_reverse_zero();
    }

    reverse_destroy_numbers(bars, node_count);
    free(bars);
    free(nodes);
    return 0;
}
