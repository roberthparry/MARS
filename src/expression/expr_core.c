/* expr_core.c - lazy, vtable-driven, reference-counted differentiable value DAG
 *
 * This file implements:
 *   - Node allocation and the expr_new_X / expr_create_X family of constructors
 *   - Reference counting (expr_retain / expr_free)
 *   - Name handling, including ASCII-to-Unicode normalisation for Greek letter
 *     names (e.g. "alpha" -> "alpha-as-unicode") so names are canonical in output
 *   - Lazy primal evaluation (expr_eval) via vtable dispatch (ops->eval)
 *   - Lazy derivative construction (expr_get_deriv / expr_create_deriv) via
 *     vtable dispatch (ops->deriv), with the result cached in expr_t::dx_cache
 *   - All arithmetic and mathematical operator constructors (expr_add, expr_sin, etc.)
 *   - expr_cmp, expr_print, and other accessors
 *
 * The operator implementations (eval/deriv bodies) live in the same file,
 * grouped by operator family after the core infrastructure.
 *
 * Partial derivatives: tl_wrt is the active variable being
 * differentiated with respect to. NULL means "single-variable / differentiate
 * w.r.t. every variable" (the original behaviour of expr_get_deriv /
 * expr_create_deriv). This is ordinary process-local differentiation context;
 * the DAG itself remains unsynchronised and is not safe for concurrent
 * mutation or evaluation.
 */

#include <stdlib.h>
#include <limits.h>
#include "number.h"
#include "expr_bindings.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"
#include "expression.h"

/* Pointer to the variable being differentiated with respect to.
 * NULL = single-variable / "all variables" mode (original behaviour of
 * expr_get_deriv / expr_create_deriv). This is differentiation context only,
 * not a general thread-safety mechanism for the DAG. */
static const expr_t *tl_wrt = NULL;

static struct _expr_t _EXPR_NAN_NODE = {
    .ops = &ops_const,
    .a = NULL,
    .b = NULL,
    .c = { { 0, 0, 0, 0, 0 } },
    .x = { { 0, 0, 0, 0, 0 } },
    .x_valid = 1,
    .epoch = 0,
    .simplified = true,
    .simplify_epoch = 0,
    .dx_cache = NULL,
    .name = NULL,
    .refcount = INT_MAX,
    .var_id = 0
};

static expr_t *expr_nan_const_shared(void)
{
    static int initialised = 0;

    if (!initialised) {
        _EXPR_NAN_NODE.c = NUM_NAN;
        _EXPR_NAN_NODE.x = NUM_NAN;
        initialised = 1;
    }
    return &_EXPR_NAN_NODE;
}

/* ------------------------------------------------------------------------- */
/* Lazy eval / deriv                                                         */
/* ------------------------------------------------------------------------- */

static number_t expr_eval_cached_num(const expr_t *dv)
{
    if (!dv)
        return NUM_ZERO;

    expr_t *m = (expr_t *)dv;

    /* Binding-expression atoms are lazy constants/initialisers. They only
     * refresh when the global working precision increases. */
    if (m->ops->arity == EXPR_OP_ATOM) {
        if (m->binding_expr) {
            number_t refreshed;

            if (expr_binding_expr_eval_if_precision_increased(m->binding_expr,
                                                            &refreshed)) {
                expr_store_const_num(m, num_clone(refreshed));
                expr_store_value_num(m, refreshed);
                m->x_valid = 1;
                m->epoch++;
            }
        }
        return m->x;
    }

    /* Recurse into children to bring their epochs current, then check whether
     * this node's cached value is still valid. ops->eval() will call expr_eval_qc
     * on children a second time, but those calls return immediately (x_valid=1). */
    expr_eval_cached_num(m->a);
    if (m->ops->arity == EXPR_OP_BINARY)
        expr_eval_cached_num(m->b);

    uint64_t child_epoch = m->a ? m->a->epoch : 0;
    if (m->b && m->b->epoch > child_epoch)
        child_epoch = m->b->epoch;

    if (!m->x_valid || child_epoch > m->epoch) {
        expr_store_value_num(m, m->ops->eval(m));
        m->x_valid = 1;
        m->epoch   = child_epoch;
    }
    return m->x;
}

number_t expr_eval_num_internal(const expr_t *dv)
{
    return expr_eval_cached_num(dv);
}

static uint64_t current_wrt_id(void)
{
    return tl_wrt ? tl_wrt->var_id : 0;
}

/* Look up (or compute and cache) the derivative of dv w.r.t. tl_wrt.
 * Returns a borrowed pointer owned by the cache entry. */
static expr_t *expr_build_dx(expr_t *dv)
{
    if (!dv)
        return NULL;

    uint64_t wrt_id = current_wrt_id();

    /* Search the cache for a matching wrt entry. */
    for (expr_deriv_cache_t *ce = dv->dx_cache; ce; ce = ce->next) {
        if (ce->wrt_id == wrt_id)
            return ce->dx; /* borrowed */
    }

    /* Not cached: compute and insert at head. */
    expr_t *dx = dv->ops->deriv(dv); /* refcount = 1, tl_wrt still set */
    expr_deriv_cache_t *ce = malloc(sizeof *ce);
    if (!ce) abort();
    ce->wrt_id = wrt_id;
    ce->dx   = dx;
    ce->next = dv->dx_cache;
    dv->dx_cache = ce;
    return dx; /* borrowed */
}

bool expr_is_differentiable(const expr_t *dv)
{
    if (!dv)
        return false;
    if (dv->ops && dv->ops->diff_kind == EXPR_DIFF_NONE)
        return false;
    if (dv->ops == &ops_pow_d)
        return expr_is_differentiable(dv->a);
    if (dv->ops == &ops_polygamma)
        return expr_is_differentiable(dv->b);
    if (dv->ops && dv->ops->arity != EXPR_OP_ATOM &&
        !expr_is_differentiable(dv->a))
        return false;
    if (dv->ops && dv->ops->arity == EXPR_OP_BINARY &&
        !expr_is_differentiable(dv->b))
        return false;
    return true;
}

/* Return an owning reference to the derivative of dv w.r.t. tl_wrt.
 * Falls back to a zero constant when no derivative exists. */
static expr_t *get_dx(const expr_t *dv)
{
    /* tl_wrt is already set by the caller; call expr_build_dx directly so we
     * don't go through the public expr_get_deriv signature which also sets it. */
    const expr_t *d = expr_build_dx((expr_t *)dv);
    if (d) {
        expr_retain((expr_t *)d);
        return (expr_t *)d;
    }
    return expr_new_const(NUM_ZERO);
}

expr_t *expr_get_dx_internal(const expr_t *dv)
{
    return get_dx(dv);
}

const expr_t *expr_current_wrt_internal(void)
{
    return tl_wrt;
}

number_t expr_eval(const expr_t *dv)
{
    return num_clone(expr_eval_num_internal(dv));
}

/* ------------------------------------------------------------------------- */
/* Public derivative (borrowed)                                              */
/* ------------------------------------------------------------------------- */

const expr_t *expr_get_deriv(const expr_t *expr, const expr_t *wrt)
{
    if (!expr || !wrt) return NULL;
    if (wrt->ops == &ops_const) return expr_nan_const_shared();
    const expr_t *saved_wrt = tl_wrt;
    tl_wrt = wrt;
    const expr_t *result = expr_build_dx((expr_t *)expr);
    tl_wrt = saved_wrt;
    return result; /* borrowed */
}

/* ------------------------------------------------------------------------- */
/* Setters                                                                   */
/* ------------------------------------------------------------------------- */

void expr_set_val(expr_t *dv, number_t value)
{
    bool preserve_binding_expr = false;

    if (!dv)
        abort();
    if (dv->ops != &ops_var &&
        !(dv->ops == &ops_const && dv->name && *dv->name))
        abort();
    if (dv->binding_expr &&
        expr_binding_expr_is_numeric_literal(dv->binding_expr)) {
        number_t binding_value = expr_binding_expr_eval(dv->binding_expr);

        preserve_binding_expr = num_eq(binding_value, value);
        num_destroy(&binding_value);
    }
    if (dv->binding_expr && !preserve_binding_expr) {
        expr_binding_expr_free(dv->binding_expr);
        dv->binding_expr = NULL;
    }
    expr_store_const_num(dv, num_clone(value));
    expr_store_value_num(dv, num_clone(dv->c));
    dv->x_valid = 1;
    dv->epoch++;
    dv->simplified = false;
    dv->simplify_epoch = 0;
}

void expr_set_name(expr_t *dv, const char *name)
{
    if (!dv) return;
    if (dv->name) free(dv->name);
    dv->name = expr_normalise_name(name);
    dv->epoch++;
    dv->simplified = false;
    dv->simplify_epoch = 0;
}

void expr_set_name_text(expr_t *dv, const string_t *name)
{
    if (!dv) return;
    if (dv->name) free(dv->name);
    dv->name = name
        ? expr_take_string_as_c_string(expr_normalise_name_text(name))
        : NULL;
    dv->epoch++;
    dv->simplified = false;
    dv->simplify_epoch = 0;
}

number_t expr_get_val(const expr_t *dv)
{
    return expr_eval(dv);
}

/* ------------------------------------------------------------------------- */
/* Core node constructors (no retaining here)                                */
/* ------------------------------------------------------------------------- */

expr_t *expr_new_unary_internal(const expr_ops_t *ops, const expr_t *a)
{
    expr_t *dv = expr_alloc(ops);
    dv->a = (expr_t *)a;
    return dv;
}

expr_t *expr_new_binary_internal(const expr_ops_t *ops, const expr_t *a, const expr_t *b)
{
    expr_t *dv = expr_alloc(ops);
    dv->a = (expr_t *)a;
    dv->b = (expr_t *)b;
    return dv;
}

expr_t *expr_new_pow_const_internal(const expr_t *a, number_t exponent)
{
    expr_t *dv = expr_alloc(&ops_pow_d);

    dv->a = (expr_t *)a;
    expr_store_const_num(dv, num_clone(exponent));
    return dv;
}

int expr_cmp(const expr_t *expr1, const expr_t *expr2) {
    NUM_SCOPE(scope);
    number_t a = expr_eval_num_internal(expr1);
    number_t b = expr_eval_num_internal(expr2);
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

expr_t *expr_create_deriv(const expr_t *expr, const expr_t *wrt)
{
    if (!expr || !wrt) return NULL;
    if (wrt->ops == &ops_const) return expr_nan_const_shared();
    {
        expr_t *special = expr_deriv_rational_over_quadratic_power(expr, wrt);

        if (special)
            return special;
    }
    const expr_t *saved_wrt = tl_wrt;
    tl_wrt = wrt;
    expr_t *raw = expr_build_dx((expr_t *)expr); /* borrowed */
    if (!raw) { tl_wrt = saved_wrt; return NULL; }
    expr_retain(raw);                  /* now owning */
    expr_t *simp = expr_simplify(raw);
    expr_free(raw);
    tl_wrt = saved_wrt;
    return simp;
}

expr_t *expr_create_2nd_deriv(const expr_t *expr, const expr_t *wrt1, const expr_t *wrt2)
{
    expr_t *g = expr_create_deriv(expr, wrt1);
    if (!g) return NULL;
    expr_t *h = expr_create_deriv(g, wrt2);
    expr_free(g);
    return h;
}

expr_t *expr_create_3rd_deriv(const expr_t *expr, const expr_t *wrt1, const expr_t *wrt2, const expr_t *wrt3)
{
    expr_t *g = expr_create_deriv(expr, wrt1);
    if (!g) return NULL;
    expr_t *h = expr_create_deriv(g, wrt2);
    expr_free(g);
    if (!h) return NULL;
    expr_t *k = expr_create_deriv(h, wrt3);
    expr_free(h);
    return k;
}

expr_t *expr_create_nth_deriv(unsigned int n, const expr_t *expr, const expr_t *wrt)
{
    const expr_t *cur = expr;
    while (n--) {
        expr_t *next = expr_create_deriv(cur, wrt);
        if (cur != expr) expr_free((expr_t *)cur);
        cur = next;
        if (!cur) break;
    }
    return (expr_t *)cur;
}
