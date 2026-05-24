#include <stddef.h>
#include <stdlib.h>

#include "dval_internal.h"

static number_t dv_reverse_zero(void)
{
    return NUM_ZERO;
}

static inline number_t num_owned_clone_local(const number_t value)
{
    return num_clone(value);
}

static inline number_t num_owned_neg_local(const number_t value)
{
    return num_neg(value);
}

static inline number_t num_owned_mul_local(const number_t a, const number_t b)
{
    return num_mul(a, b);
}

static inline number_t num_owned_div_local(const number_t a, const number_t b)
{
    return num_div(a, b);
}

static int reverse_find_node(const dval_t *const *nodes,
                             size_t count,
                             const dval_t *target)
{
    for (size_t i = 0; i < count; ++i)
        if (nodes[i] == target)
            return (int)i;
    return -1;
}

static int reverse_append_node(const dval_t ***nodes,
                               size_t *count,
                               size_t *capacity,
                               const dval_t *node)
{
    const dval_t **grown;
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

static int reverse_collect_postorder(const dval_t *node,
                                     const dval_t ***nodes,
                                     size_t *count,
                                     size_t *capacity)
{
    if (!node)
        return 0;
    if (reverse_find_node(*nodes, *count, node) >= 0)
        return 0;
    if (node->ops->arity != DV_OP_ATOM &&
        reverse_collect_postorder(node->a, nodes, count, capacity) != 0)
        return -1;
    if (node->ops->arity == DV_OP_BINARY &&
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

int dv_eval_derivatives(const dval_t *expr,
                        size_t nvars,
                        const dval_t *const *vars,
                        number_t *value_out,
                        number_t *derivs_out)
{
    const dval_t **nodes = NULL;
    size_t node_count = 0u;
    size_t node_capacity = 0u;
    number_t *bars = NULL;
    number_t value;

    if (!expr || (nvars > 0u && (!vars || !derivs_out)))
        return -1;

    value = dv_eval(expr);
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
        bars[i] = dv_reverse_zero();
    bars[node_count - 1u] = NUM_ONE;

    for (size_t i = node_count; i-- > 0u;) {
        const dval_t *node = nodes[i];
        int a_index;
        int b_index;

        if (node->ops->arity == DV_OP_ATOM || num_is_zero(bars[i]))
            continue;

        NUM_SCOPE(scope);
        number_t a_bar = (number_t){0};
        number_t b_bar = (number_t){0};
        node->ops->reverse(node, &bars[i], &a_bar, &b_bar);

        if (node->ops->arity != DV_OP_ATOM) {
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
        if (node->ops->arity == DV_OP_BINARY && node->b) {
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

        derivs_out[i] = index >= 0 ? num_clone(bars[index]) : dv_reverse_zero();
    }

    reverse_destroy_numbers(bars, node_count);
    free(bars);
    free(nodes);
    return 0;
}

void dv_reverse_atom(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    (void)dv;
    (void)out_bar;
    *a_bar = dv_reverse_zero();
    *b_bar = dv_reverse_zero();
}

void dv_reverse_add(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    (void)dv;
    *a_bar = num_owned_clone_local(*out_bar);
    *b_bar = num_owned_clone_local(*out_bar);
}

void dv_reverse_sub(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    (void)dv;
    *a_bar = num_owned_clone_local(*out_bar);
    *b_bar = num_owned_neg_local(*out_bar);
}

void dv_reverse_mul(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    *a_bar = num_owned_mul_local(*out_bar, dv_eval_num_internal(dv->b));
    *b_bar = num_owned_mul_local(*out_bar, dv_eval_num_internal(dv->a));
}

void dv_reverse_div(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t denom_sq = num_mul(dv_eval_num_internal(dv->b), dv_eval_num_internal(dv->b));
    number_t numer = num_mul(*out_bar, dv_eval_num_internal(dv->a));
    number_t frac;

    *a_bar = num_owned_div_local(*out_bar, dv_eval_num_internal(dv->b));
    frac = num_div(numer, denom_sq);
    *b_bar = num_owned_neg_local(frac);
}

void dv_reverse_pow(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t z = dv_eval_num_internal(dv);
    number_t z_times_b = num_mul(z, dv_eval_num_internal(dv->b));
    number_t numer = num_mul(*out_bar, z_times_b);
    number_t log_a = num_log(dv_eval_num_internal(dv->a));
    number_t z_log_a;

    *a_bar = num_owned_div_local(numer, dv_eval_num_internal(dv->a));

    z_log_a = num_mul(z, log_a);
    *b_bar = num_owned_mul_local(*out_bar, z_log_a);
}

void dv_reverse_pow_d(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t one_less = num_sub(dv->c, NUM_ONE);
    number_t power = num_pow(dv_eval_num_internal(dv->a), one_less);
    number_t scale = num_mul(dv->c, power);

    *a_bar = num_owned_mul_local(*out_bar, scale);
    *b_bar = dv_reverse_zero();
}

void dv_reverse_neg(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    (void)dv;
    *a_bar = num_owned_neg_local(*out_bar);
    *b_bar = dv_reverse_zero();
}
