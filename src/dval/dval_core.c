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
#include "qcomplex.h"
#include "dval_internal.h"
#include "dval.h"

/* Thread-local pointer to the variable being differentiated with respect to.
 * NULL = single-variable / "all variables" mode (original behaviour of
 * dv_get_deriv / dv_create_deriv). This is differentiation context only,
 * not a general thread-safety mechanism for the DAG. */
static __thread dval_t *tl_wrt = NULL;

static inline int qc_is_zero(qcomplex_t z)
{
    return qc_eq(z, QC_ZERO);
}

static inline int qc_is_one(qcomplex_t z)
{
    return qc_eq(z, QC_ONE);
}

static struct _dval_t _DV_NAN_NODE = {
    .ops = &ops_const,
    .a = NULL,
    .b = NULL,
    .c = { { NAN, 0.0 }, { NAN, 0.0 } },
    .x = { { NAN, 0.0 }, { NAN, 0.0 } },
    .x_valid = 1,
    .epoch = 0,
    .dx_cache = NULL,
    .name = NULL,
    .refcount = INT_MAX,
    .var_id = 0
};

static dval_t *dv_nan_const_shared(void)
{
    return &_DV_NAN_NODE;
}

/* ------------------------------------------------------------------------- */
/* Lazy eval / deriv                                                         */
/* ------------------------------------------------------------------------- */

static qcomplex_t dv_eval_qc(const dval_t *dv)
{
    if (!dv)
        return QC_ZERO;

    dval_t *m = (dval_t *)dv;

    /* Atoms (constants and variables) are always up-to-date. */
    if (m->ops->arity == DV_OP_ATOM)
        return m->x;

    /* Recurse into children to bring their epochs current, then check whether
     * this node's cached value is still valid. ops->eval() will call dv_eval_qc
     * on children a second time, but those calls return immediately (x_valid=1). */
    dv_eval_qc(m->a);
    if (m->ops->arity == DV_OP_BINARY)
        dv_eval_qc(m->b);

    uint64_t child_epoch = m->a ? m->a->epoch : 0;
    if (m->b && m->b->epoch > child_epoch)
        child_epoch = m->b->epoch;

    if (!m->x_valid || child_epoch > m->epoch) {
        m->x       = m->ops->eval(m);
        m->x_valid = 1;
        m->epoch   = child_epoch;
    }
    return m->x;
}

qcomplex_t dv_eval_qc_internal(const dval_t *dv)
{
    return dv_eval_qc(dv);
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
    return dv_new_const_d(0.0);
}

dval_t *dv_get_dx_internal(const dval_t *dv)
{
    return get_dx(dv);
}

dval_t *dv_current_wrt_internal(void)
{
    return tl_wrt;
}

/* ------------------------------------------------------------------------- */
/* Public eval                                                               */
/* ------------------------------------------------------------------------- */

qcomplex_t dv_eval(const dval_t *dv)
{
    return dv_eval_qc(dv);
}

qfloat_t dv_eval_qf(const dval_t *dv)
{
    return dv_eval_qc(dv).re;
}

double dv_eval_d(const dval_t *dv)
{
    return qf_to_double(dv_eval_qf(dv));
}

/* ------------------------------------------------------------------------- */
/* Public derivative (borrowed)                                              */
/* ------------------------------------------------------------------------- */

const dval_t *dv_get_deriv(const dval_t *expr, dval_t *wrt)
{
    if (!expr || !wrt) return NULL;
    if (wrt->ops == &ops_const) return dv_nan_const_shared();
    dval_t *saved_wrt = tl_wrt;
    tl_wrt = wrt;
    const dval_t *result = dv_build_dx((dval_t *)expr);
    tl_wrt = saved_wrt;
    return result; /* borrowed */
}

/* ------------------------------------------------------------------------- */
/* Setters                                                                   */
/* ------------------------------------------------------------------------- */

void dv_set_val(dval_t *dv, qcomplex_t v)
{
    if (!dv)
        abort();
    if (dv->ops != &ops_var &&
        !(dv->ops == &ops_const && dv->name && *dv->name))
        abort();
    dv->c = v;
    dv->x = v;
    dv->x_valid = 1;
    dv->epoch++;
}

void dv_set_val_qf(dval_t *dv, qfloat_t v)
{
    dv_set_val(dv, dv_qc_real_qf(v));
}

void dv_set_val_d(dval_t *dv, double v)
{
    dv_set_val(dv, dv_qc_real_d(v));
}

void dv_set_name(dval_t *dv, const char *name)
{
    if (!dv) return;
    if (dv->name) free(dv->name);
    dv->name = dv_normalize_name(name);
}

/* ------------------------------------------------------------------------- */
/* Accessors                                                                 */
/* ------------------------------------------------------------------------- */

double dv_get_val_d(const dval_t *dv)
{
    return qf_to_double(dv_eval_qc((dval_t *)dv).re);
}

qfloat_t dv_get_val_qf(const dval_t *dv)
{
    return dv_eval_qc((dval_t *)dv).re;
}

qcomplex_t dv_get_val(const dval_t *dv)
{
    return dv_eval_qc((dval_t *)dv);
}

/* ------------------------------------------------------------------------- */
/* Core node constructors (no retaining here)                                */
/* ------------------------------------------------------------------------- */

dval_t *dv_new_unary_internal(const dval_ops_t *ops, dval_t *a)
{
    dval_t *dv = dv_alloc(ops);
    dv->a = a;
    return dv;
}

dval_t *dv_new_binary_internal(const dval_ops_t *ops, dval_t *a, dval_t *b)
{
    dval_t *dv = dv_alloc(ops);
    dv->a = a;
    dv->b = b;
    return dv;
}

dval_t *dv_new_pow_d_internal(dval_t *a, double d)
{
    dval_t *dv = dv_alloc(&ops_pow_d);
    dv->a = a;
    dv->c = dv_qc_real_d(d);
    return dv;
}

dval_t *dv_new_pow_qf_internal(dval_t *a, qfloat_t exponent)
{
    dval_t *dv = dv_alloc(&ops_pow_d);
    dv->a = a;
    dv->c = dv_qc_real_qf(exponent);
    return dv;
}

dval_t *dv_new_pow_qc_internal(dval_t *a, qcomplex_t exponent)
{
    dval_t *dv = dv_alloc(&ops_pow_d);
    dv->a = a;
    dv->c = exponent;
    return dv;
}

int dv_cmp(const dval_t *dv1, const dval_t *dv2) {
    qcomplex_t a = dv_eval(dv1);
    qcomplex_t b = dv_eval(dv2);

    if (!qf_eq(a.re, b.re))
        return qf_cmp(a.re, b.re);
    return qf_cmp(a.im, b.im);
}


/* ------------------------------------------------------------------------- */
/* Derivative creation (owning)                                              */
/* ------------------------------------------------------------------------- */

dval_t *dv_create_deriv(dval_t *expr, dval_t *wrt)
{
    if (!expr || !wrt) return NULL;
    if (wrt->ops == &ops_const) return dv_nan_const_shared();
    dval_t *saved_wrt = tl_wrt;
    tl_wrt = wrt;
    dval_t *raw = dv_build_dx(expr); /* borrowed */
    if (!raw) { tl_wrt = saved_wrt; return NULL; }
    dv_retain(raw);                  /* now owning */
    dval_t *simp = dv_simplify(raw);
    dv_free(raw);
    tl_wrt = saved_wrt;
    return simp;
}

dval_t *dv_create_2nd_deriv(dval_t *expr, dval_t *wrt1, dval_t *wrt2)
{
    dval_t *g = dv_create_deriv(expr, wrt1);
    if (!g) return NULL;
    dval_t *h = dv_create_deriv(g, wrt2);
    dv_free(g);
    return h;
}

dval_t *dv_create_3rd_deriv(dval_t *expr, dval_t *wrt1, dval_t *wrt2, dval_t *wrt3)
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

dval_t *dv_create_nth_deriv(unsigned int n, dval_t *expr, dval_t *wrt)
{
    dval_t *cur = expr;
    while (n--) {
        dval_t *next = dv_create_deriv(cur, wrt);
        if (cur != expr) dv_free(cur);
        cur = next;
        if (!cur) break;
    }
    return cur;
}
