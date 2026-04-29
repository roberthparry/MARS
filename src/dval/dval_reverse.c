#include <stddef.h>
#include <stdlib.h>
#include "dval_internal.h"

typedef struct {
    const dval_t *node;
    qfloat_t adjoint;
} dv_reverse_slot_t;

static int reverse_find_slot(const dv_reverse_slot_t *slots, size_t nslots,
                             const dval_t *node)
{
    for (size_t i = 0; i < nslots; ++i)
        if (slots[i].node == node)
            return (int)i;
    return -1;
}

static int reverse_collect_nodes(const dval_t *node,
                                 dv_reverse_slot_t **slots,
                                 size_t *nslots,
                                 size_t *capslots)
{
    if (!node)
        return 0;
    if (reverse_find_slot(*slots, *nslots, node) >= 0)
        return 0;
    if (reverse_collect_nodes(node->a, slots, nslots, capslots) != 0)
        return -1;
    if (reverse_collect_nodes(node->b, slots, nslots, capslots) != 0)
        return -1;
    if (*nslots == *capslots) {
        size_t new_cap = *capslots ? (*capslots * 2) : 32;
        dv_reverse_slot_t *grown = realloc(*slots, new_cap * sizeof(**slots));
        if (!grown)
            return -1;
        *slots = grown;
        *capslots = new_cap;
    }
    (*slots)[*nslots].node = node;
    (*slots)[*nslots].adjoint = QF_ZERO;
    (*nslots)++;
    return 0;
}

int dv_eval_derivatives(const dval_t *expr,
                        size_t nvars,
                        dval_t *const *vars,
                        qfloat_t *value_out,
                        qfloat_t *derivs_out)
{
    dv_reverse_slot_t *slots = NULL;
    size_t nslots = 0;
    size_t capslots = 0;
    qfloat_t value;

    if (!expr || (nvars > 0 && (!vars || !derivs_out)))
        return -1;

    value = dv_eval_qf(expr);
    if (value_out)
        *value_out = value;

    if (reverse_collect_nodes(expr, &slots, &nslots, &capslots) != 0) {
        free(slots);
        return -1;
    }

    if (nslots == 0) {
        free(slots);
        return -1;
    }

    slots[nslots - 1].adjoint = QF_ONE;

    for (size_t i = nslots; i-- > 0;) {
        const dval_t *node = slots[i].node;
        qfloat_t out_bar = slots[i].adjoint;
        qfloat_t a_bar = QF_ZERO;
        qfloat_t b_bar = QF_ZERO;
        int child_index;

        if (qf_eq(out_bar, QF_ZERO))
            continue;

        if (!node->ops || !node->ops->reverse) {
            free(slots);
            return -1;
        }

        node->ops->reverse(node, out_bar, &a_bar, &b_bar);

        if (node->a && !qf_eq(a_bar, QF_ZERO)) {
            child_index = reverse_find_slot(slots, nslots, node->a);
            if (child_index < 0) {
                free(slots);
                return -1;
            }
            slots[child_index].adjoint = qf_add(slots[child_index].adjoint, a_bar);
        }
        if (node->b && !qf_eq(b_bar, QF_ZERO)) {
            child_index = reverse_find_slot(slots, nslots, node->b);
            if (child_index < 0) {
                free(slots);
                return -1;
            }
            slots[child_index].adjoint = qf_add(slots[child_index].adjoint, b_bar);
        }
    }

    for (size_t i = 0; i < nvars; ++i) {
        int slot_index = reverse_find_slot(slots, nslots, vars[i]);
        derivs_out[i] = (slot_index >= 0) ? slots[slot_index].adjoint : QF_ZERO;
    }

    free(slots);
    return 0;
}

void dv_reverse_atom(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    (void)dv;
    (void)out_bar;
    *a_bar = QF_ZERO;
    *b_bar = QF_ZERO;
}

void dv_reverse_add(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    (void)dv;
    *a_bar = out_bar;
    *b_bar = out_bar;
}

void dv_reverse_sub(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    (void)dv;
    *a_bar = out_bar;
    *b_bar = qf_neg(out_bar);
}

void dv_reverse_mul(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    *a_bar = qf_mul(out_bar, dv->b->x.re);
    *b_bar = qf_mul(out_bar, dv->a->x.re);
}

void dv_reverse_div(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    qfloat_t denom_sq = qf_mul(dv->b->x.re, dv->b->x.re);
    *a_bar = qf_div(out_bar, dv->b->x.re);
    *b_bar = qf_neg(qf_div(qf_mul(out_bar, dv->a->x.re), denom_sq));
}

void dv_reverse_pow(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    qfloat_t z = dv->x.re;
    *a_bar = qf_mul(out_bar, qf_div(qf_mul(z, dv->b->x.re), dv->a->x.re));
    *b_bar = qf_mul(out_bar, qf_mul(z, qf_log(dv->a->x.re)));
}

void dv_reverse_pow_d(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    qfloat_t exponent = dv->c.re;
    qfloat_t one_less = qf_sub(exponent, QF_ONE);
    *a_bar = qf_mul(out_bar, qf_mul(exponent, qf_pow(dv->a->x.re, one_less)));
    *b_bar = QF_ZERO;
}

void dv_reverse_neg(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    (void)dv;
    *a_bar = qf_neg(out_bar);
    *b_bar = QF_ZERO;
}
