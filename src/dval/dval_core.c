/* dval_core.c - lazy, vtable-driven, reference-counted differentiable value DAG
 *
 * This file implements:
 *   - Node allocation and the dv_new_X / dv_create_X family of constructors
 *   - Reference counting (dv_retain / dv_free)
 *   - Name handling, including ASCII-to-Unicode normalisation for Greek letter
 *     names (e.g. "alpha" -> "alpha-as-unicode") so names are canonical in output
 *   - Lazy primal evaluation (dv_eval) via vtable dispatch (ops->eval)
 *   - Lazy derivative construction (dv_get_deriv / dv_create_deriv) via
 *     vtable dispatch (ops->deriv), with the result cached in dval_t::dx_cache
 *   - All arithmetic and math operator constructors (dv_add, dv_sin, etc.)
 *   - dv_cmp, dv_print, and other accessors
 *
 * The operator implementations (eval/deriv bodies) live in the same file,
 * grouped by operator family after the core infrastructure.
 *
 * Partial derivatives: tl_wrt is a thread-local pointer to the variable being
 * differentiated with respect to. NULL means "single-variable / differentiate
 * w.r.t. every variable" (the original behaviour of dv_get_deriv /
 * dv_create_deriv). This isolates the active differentiation target per
 * thread, but the DAG itself remains unsynchronised and is not safe for
 * concurrent mutation or evaluation.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>
#include <limits.h>
#include <math.h>
#include "number.h"
#include "dval_internal.h"
#include "dval.h"
#include "internal/number_internal.h"

/* Thread-local pointer to the variable being differentiated with respect to.
 * NULL = single-variable / "all variables" mode (original behaviour of
 * dv_get_deriv / dv_create_deriv). This is differentiation context only,
 * not a general thread-safety mechanism for the DAG. */
static __thread const dval_t *tl_wrt = NULL;

static struct _dval_t _DV_NAN_NODE = {
    .ops = &ops_const,
    .a = NULL,
    .b = NULL,
    .c = { { 0, 0, 0, 0, 0 } },
    .x = { { 0, 0, 0, 0, 0 } },
    .x_valid = 1,
    .epoch = 0,
    .dx_cache = NULL,
    .name = NULL,
    .refcount = INT_MAX,
    .var_id = 0
};

static dval_t *dv_nan_const_shared(void)
{
    static int initialized = 0;

    if (!initialized) {
        _DV_NAN_NODE.c = NUM_NAN;
        _DV_NAN_NODE.x = NUM_NAN;
        initialized = 1;
    }
    return &_DV_NAN_NODE;
}

/* ------------------------------------------------------------------------- */
/* Lazy eval / deriv                                                         */
/* ------------------------------------------------------------------------- */

static number_t dv_eval_cached_num(const dval_t *dv)
{
    if (!dv)
        return NUM_ZERO;

    dval_t *m = (dval_t *)dv;

    /* Atoms (constants and variables) are always up-to-date. */
    if (m->ops->arity == DV_OP_ATOM)
        return m->x;

    /* Recurse into children to bring their epochs current, then check whether
     * this node's cached value is still valid. ops->eval() will call dv_eval_qc
     * on children a second time, but those calls return immediately (x_valid=1). */
    dv_eval_cached_num(m->a);
    if (m->ops->arity == DV_OP_BINARY)
        dv_eval_cached_num(m->b);

    uint64_t child_epoch = m->a ? m->a->epoch : 0;
    if (m->b && m->b->epoch > child_epoch)
        child_epoch = m->b->epoch;

    if (!m->x_valid || child_epoch > m->epoch) {
        dv_store_value_num(m, m->ops->eval(m));
        m->x_valid = 1;
        m->epoch   = child_epoch;
    }
    return m->x;
}

number_t dv_eval_num_internal(const dval_t *dv)
{
    return dv_eval_cached_num(dv);
}

static uint64_t current_wrt_id(void)
{
    return tl_wrt ? tl_wrt->var_id : 0;
}

/* Look up (or compute and cache) the derivative of dv w.r.t. tl_wrt.
 * Returns a borrowed pointer owned by the cache entry. */
static dval_t *dv_build_dx(dval_t *dv)
{
    if (!dv)
        return NULL;

    uint64_t wrt_id = current_wrt_id();

    /* Search the cache for a matching wrt entry. */
    for (dv_deriv_cache_t *ce = dv->dx_cache; ce; ce = ce->next) {
        if (ce->wrt_id == wrt_id)
            return ce->dx; /* borrowed */
    }

    /* Not cached: compute and insert at head. */
    dval_t *dx = dv->ops->deriv(dv); /* refcount = 1, tl_wrt still set */
    dv_deriv_cache_t *ce = malloc(sizeof *ce);
    if (!ce) abort();
    ce->wrt_id = wrt_id;
    ce->dx   = dx;
    ce->next = dv->dx_cache;
    dv->dx_cache = ce;
    return dx; /* borrowed */
}

/* Return an owning reference to the derivative of dv w.r.t. tl_wrt.
 * Falls back to a zero constant when no derivative exists. */
static dval_t *get_dx(const dval_t *dv)
{
    /* tl_wrt is already set by the caller; call dv_build_dx directly so we
     * don't go through the public dv_get_deriv signature which also sets it. */
    const dval_t *d = dv_build_dx((dval_t *)dv);
    if (d) {
        dv_retain((dval_t *)d);
        return (dval_t *)d;
    }
    return dv_new_const(NUM_ZERO);
}

dval_t *dv_get_dx_internal(const dval_t *dv)
{
    return get_dx(dv);
}

const dval_t *dv_current_wrt_internal(void)
{
    return tl_wrt;
}

number_t dv_eval(const dval_t *dv)
{
    return num_clone(dv_eval_num_internal(dv));
}

/* ------------------------------------------------------------------------- */
/* Public derivative (borrowed)                                              */
/* ------------------------------------------------------------------------- */

const dval_t *dv_get_deriv(const dval_t *expr, const dval_t *wrt)
{
    if (!expr || !wrt) return NULL;
    if (wrt->ops == &ops_const) return dv_nan_const_shared();
    const dval_t *saved_wrt = tl_wrt;
    tl_wrt = wrt;
    const dval_t *result = dv_build_dx((dval_t *)expr);
    tl_wrt = saved_wrt;
    return result; /* borrowed */
}

/* ------------------------------------------------------------------------- */
/* Setters                                                                   */
/* ------------------------------------------------------------------------- */

void dv_set_val(dval_t *dv, number_t value)
{
    if (!dv)
        abort();
    if (dv->ops != &ops_var &&
        !(dv->ops == &ops_const && dv->name && *dv->name))
        abort();
    dv_store_const_num(dv, num_clone(value));
    dv_store_value_num(dv, num_clone(dv->c));
    dv->x_valid = 1;
    dv->epoch++;
}

void dv_set_name(dval_t *dv, const char *name)
{
    if (!dv) return;
    if (dv->name) free(dv->name);
    dv->name = dv_normalize_name(name);
}

number_t dv_get_val(const dval_t *dv)
{
    return dv_eval(dv);
}

/* ------------------------------------------------------------------------- */
/* Core node constructors (no retaining here)                                */
/* ------------------------------------------------------------------------- */

dval_t *dv_new_unary_internal(const dval_ops_t *ops, const dval_t *a)
{
    dval_t *dv = dv_alloc(ops);
    dv->a = (dval_t *)a;
    return dv;
}

dval_t *dv_new_binary_internal(const dval_ops_t *ops, const dval_t *a, const dval_t *b)
{
    dval_t *dv = dv_alloc(ops);
    dv->a = (dval_t *)a;
    dv->b = (dval_t *)b;
    return dv;
}

dval_t *dv_new_pow_const_internal(const dval_t *a, number_t exponent)
{
    dval_t *dv = dv_alloc(&ops_pow_d);

    dv->a = (dval_t *)a;
    dv_store_const_num(dv, num_clone(exponent));
    return dv;
}

int dv_cmp(const dval_t *dv1, const dval_t *dv2) {
    NUM_SCOPE(scope);
    number_t a = dv_eval_num_internal(dv1);
    number_t b = dv_eval_num_internal(dv2);
    number_t a_real = num_real_part(a);
    number_t b_real = num_real_part(b);
    int cmp = num_cmp(a_real, b_real);

    if (cmp == 0) {
        number_t a_imag = num_imag_part(a);
        number_t b_imag = num_imag_part(b);

        cmp = num_cmp(a_imag, b_imag);
    }
    return cmp;
}


/* ------------------------------------------------------------------------- */
/* Derivative creation (owning)                                              */
/* ------------------------------------------------------------------------- */

dval_t *dv_create_deriv(const dval_t *expr, const dval_t *wrt)
{
    if (!expr || !wrt) return NULL;
    if (wrt->ops == &ops_const) return dv_nan_const_shared();
    const dval_t *saved_wrt = tl_wrt;
    tl_wrt = wrt;
    dval_t *raw = dv_build_dx((dval_t *)expr); /* borrowed */
    if (!raw) { tl_wrt = saved_wrt; return NULL; }
    dv_retain(raw);                  /* now owning */
    dval_t *simp = dv_simplify(raw);
    dv_free(raw);
    tl_wrt = saved_wrt;
    return simp;
}

dval_t *dv_create_2nd_deriv(const dval_t *expr, const dval_t *wrt1, const dval_t *wrt2)
{
    dval_t *g = dv_create_deriv(expr, wrt1);
    if (!g) return NULL;
    dval_t *h = dv_create_deriv(g, wrt2);
    dv_free(g);
    return h;
}

dval_t *dv_create_3rd_deriv(const dval_t *expr, const dval_t *wrt1, const dval_t *wrt2, const dval_t *wrt3)
{
    dval_t *g = dv_create_deriv(expr, wrt1);
    if (!g) return NULL;
    dval_t *h = dv_create_deriv(g, wrt2);
    dv_free(g);
    if (!h) return NULL;
    dval_t *k = dv_create_deriv(h, wrt3);
    dv_free(h);
    return k;
}

dval_t *dv_create_nth_deriv(unsigned int n, const dval_t *expr, const dval_t *wrt)
{
    const dval_t *cur = expr;
    while (n--) {
        dval_t *next = dv_create_deriv(cur, wrt);
        if (cur != expr) dv_free((dval_t *)cur);
        cur = next;
        if (!cur) break;
    }
    return (dval_t *)cur;
}
