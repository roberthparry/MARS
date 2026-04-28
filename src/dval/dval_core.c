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
#include "qcomplex.h"
#include "dval_internal.h"
#include "dval.h"

/* Thread-local pointer to the variable being differentiated with respect to.
 * NULL = single-variable / "all variables" mode (original behaviour of
 * dv_get_deriv / dv_create_deriv). This is differentiation context only,
 * not a general thread-safety mechanism for the DAG. */
static __thread dval_t *tl_wrt = NULL;

static inline qcomplex_t qc_real_qf(qfloat_t x)
{
    return qc_make(x, QF_ZERO);
}

static inline qcomplex_t qc_real_d(double x)
{
    return qc_make(qf_from_double(x), QF_ZERO);
}

static inline int qc_is_zero(qcomplex_t z)
{
    return qc_eq(z, QC_ZERO);
}

static inline int qc_is_one(qcomplex_t z)
{
    return qc_eq(z, QC_ONE);
}

static struct _dval_t _DV_ZERO_NODE = {
    .ops = &ops_const,
    .a = NULL,
    .b = NULL,
    .c = { { 0.0, 0.0 }, { 0.0, 0.0 } },
    .x = { { 0.0, 0.0 }, { 0.0, 0.0 } },
    .x_valid = 1,
    .epoch = 0,
    .dx_cache = NULL,
    .name = NULL,
    .refcount = INT_MAX,
    .var_id = 0
};

static struct _dval_t _DV_ONE_NODE = {
    .ops = &ops_const,
    .a = NULL,
    .b = NULL,
    .c = { { 1.0, 0.0 }, { 0.0, 0.0 } },
    .x = { { 1.0, 0.0 }, { 0.0, 0.0 } },
    .x_valid = 1,
    .epoch = 0,
    .dx_cache = NULL,
    .name = NULL,
    .refcount = INT_MAX,
    .var_id = 0
};

dval_t *DV_ZERO = &_DV_ZERO_NODE;
dval_t *DV_ONE = &_DV_ONE_NODE;

/* ------------------------------------------------------------------------- */
/* Name handling                                                             */
/* ------------------------------------------------------------------------- */

static char *dv_normalize_name(const char *name)
{
    static const struct {
        const char *ascii;
        const char *lower;
        const char *upper;
    } greek[] = {
        { "alpha",   "α", "Α" },
        { "beta",    "β", "Β" },
        { "gamma",   "γ", "Γ" },
        { "delta",   "δ", "Δ" },
        { "epsilon", "ε", "Ε" },
        { "zeta",    "ζ", "Ζ" },
        { "eta",     "η", "Η" },
        { "theta",   "θ", "Θ" },
        { "iota",    "ι", "Ι" },
        { "kappa",   "κ", "Κ" },
        { "lambda",  "λ", "Λ" },
        { "mu",      "μ", "Μ" },
        { "nu",      "ν", "Ν" },
        { "xi",      "ξ", "Ξ" },
        { "omicron", "ο", "Ο" },
        { "pi",      "π", "Π" },
        { "rho",     "ρ", "Ρ" },
        { "sigma",   "σ", "Σ" },
        { "tau",     "τ", "Τ" },
        { "upsilon", "υ", "Υ" },
        { "phi",     "φ", "Φ" },
        { "chi",     "χ", "Χ" },
        { "psi",     "ψ", "Ψ" },
        { "omega",   "ω", "Ω" }
    };

    if (!name)
        return NULL;

    /* trim */
    const char *s = name;
    while (*s && isspace((unsigned char)*s)) s++;
    const char *e = name + strlen(name);
    while (e > s && isspace((unsigned char)e[-1])) e--;

    size_t len = (size_t)(e - s);
    if (len == 0)
        return NULL;

    char *t = malloc(len + 1);
    memcpy(t, s, len);
    t[len] = '\0';

    if (t[0] != '@')
        return t;

    const char *p = t + 1;

    for (size_t i = 0; i < sizeof(greek)/sizeof(greek[0]); ++i) {
        size_t alen = strlen(greek[i].ascii);
        if (strncasecmp(p, greek[i].ascii, alen) == 0) {
            int upper = 1;
            for (size_t k = 0; k < alen; ++k)
                if (!isupper((unsigned char)p[k])) upper = 0;

            const char *g = upper ? greek[i].upper : greek[i].lower;
            const char *rest = p + alen;

            size_t gl = strlen(g);
            size_t rl = strlen(rest);

            char *out = malloc(gl + rl + 1);
            memcpy(out, g, gl);
            memcpy(out + gl, rest, rl);
            out[gl + rl] = '\0';

            free(t);
            t = out;
            break;
        }
    }

    /* remove remaining '@' */
    size_t n = strlen(t);
    char *clean = malloc(n + 1);
    size_t w = 0;
    for (size_t r = 0; r < n; ++r)
        if (t[r] != '@') clean[w++] = t[r];
    clean[w] = '\0';

    free(t);
    return clean;
}

/* ------------------------------------------------------------------------- */
/* Refcount                                                                  */
/* ------------------------------------------------------------------------- */

static uint64_t next_var_id = 1;

static inline void refcount_inc(int *rc)
{
    if (*rc < INT_MAX)
        (*rc)++;
}

static inline int refcount_dec(int *rc)
{
    int prev = *rc;
    if (*rc < INT_MAX)
        (*rc)--;
    return prev;
}

static uint64_t alloc_var_id(void)
{
    return next_var_id++;
}

void dv_retain(dval_t *dv)
{
    if (dv)
        refcount_inc(&dv->refcount);
}


static void dv_release(dval_t *dv)
{
    if (!dv) return;

    if (refcount_dec(&dv->refcount) > 1)
        return;

    dval_t *a = dv->a;
    dval_t *b = dv->b;

    /* Free the derivative cache linked list */
    dv_deriv_cache_t *ce = dv->dx_cache;
    while (ce) {
        dv_deriv_cache_t *next = ce->next;
        dv_release(ce->dx);
        free(ce);
        ce = next;
    }

    if (dv->name) free(dv->name);
    free(dv);

    dv_release(a);
    dv_release(b);
}

/* ------------------------------------------------------------------------- */
/* Node allocation                                                           */
/* ------------------------------------------------------------------------- */

static dval_t *dv_alloc(const dval_ops_t *ops)
{
    dval_t *dv = malloc(sizeof *dv);
    if (!dv) abort();

    dv->ops      = ops;
    dv->a        = NULL;
    dv->b        = NULL;
    dv->c        = QC_ZERO;
    dv->x        = QC_ZERO;
    dv->x_valid  = 0;
    dv->epoch    = 0;
    dv->dx_cache = NULL;
    dv->name     = NULL;
    dv->refcount = 1;
    dv->var_id   = 0;

    return dv;
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
/* Reverse-mode evaluation                                                   */
/* ------------------------------------------------------------------------- */

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

/* ------------------------------------------------------------------------- */
/* Public derivative (borrowed)                                              */
/* ------------------------------------------------------------------------- */

const dval_t *dv_get_deriv(const dval_t *expr, dval_t *wrt)
{
    if (!expr || !wrt) return NULL;
    dval_t *saved_wrt = tl_wrt;
    tl_wrt = wrt;
    const dval_t *result = dv_build_dx((dval_t *)expr);
    tl_wrt = saved_wrt;
    return result; /* borrowed */
}

/* ------------------------------------------------------------------------- */
/* Constructors                                                              */
/* ------------------------------------------------------------------------- */

static dval_t *dv_make_const_qc(qcomplex_t x)
{
    dval_t *dv = dv_alloc(&ops_const);
    dv->c = x;
    dv->x = x;
    dv->x_valid = 1;
    return dv;
}

/* A variable: value x. The derivative is computed lazily by deriv_var,
 * keyed by tl_wrt at the time of the first request. */
static dval_t *dv_make_var_qc(qcomplex_t x)
{
    dval_t *dv = dv_alloc(&ops_var);
    dv->c = x;
    dv->x = x;
    dv->x_valid = 1;
    dv->var_id = alloc_var_id();
    return dv;
}

dval_t *dv_new_const_d(double x) { return dv_make_const_qc(qc_real_d(x)); }
dval_t *dv_new_const(qfloat_t x) { return dv_make_const_qc(qc_real_qf(x)); }
dval_t *dv_new_const_qc(qcomplex_t x) { return dv_make_const_qc(x); }

/* Public variable constructors: no derivative argument */
dval_t *dv_new_var_d(double x)
{
    return dv_make_var_qc(qc_real_d(x));
}

dval_t *dv_new_var(qfloat_t x)
{
    return dv_make_var_qc(qc_real_qf(x));
}

dval_t *dv_new_var_qc(qcomplex_t x)
{
    return dv_make_var_qc(x);
}

dval_t *dv_new_named_const(qfloat_t x, const char *name)
{
    dval_t *dv = dv_new_const(x);
    dv->name = dv_normalize_name(name);
    return dv;
}

dval_t *dv_new_named_const_qc(qcomplex_t x, const char *name)
{
    dval_t *dv = dv_new_const_qc(x);
    dv->name = dv_normalize_name(name);
    return dv;
}

dval_t *dv_new_named_const_d(double x, const char *name)
{
    return dv_new_named_const(qf_from_double(x), name);
}

/* Named variable: value x, derivative 1, with a name */
dval_t *dv_new_named_var(qfloat_t x, const char *name)
{
    dval_t *dv = dv_new_var(x);
    dv->name = dv_normalize_name(name);
    return dv;
}

dval_t *dv_new_named_var_qc(qcomplex_t x, const char *name)
{
    dval_t *dv = dv_new_var_qc(x);
    dv->name = dv_normalize_name(name);
    return dv;
}

dval_t *dv_new_named_var_d(double x, const char *name)
{
    return dv_new_named_var(qf_from_double(x), name);
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
    dv_set_val(dv, qc_real_qf(v));
}

void dv_set_val_d(dval_t *dv, double v)
{
    dv_set_val(dv, qc_real_d(v));
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
/* EVALUATION FUNCTIONS                                                      */
/* ------------------------------------------------------------------------- */

static qcomplex_t eval_const(dval_t *dv) {
    return dv->c;
}

static qcomplex_t eval_var(dval_t *dv) {
    return dv->c;
}

static qcomplex_t eval_add(dval_t *dv) {
    return qc_add(dv_eval_qc(dv->a), dv_eval_qc(dv->b));
}

static qcomplex_t eval_sub(dval_t *dv) {
    return qc_sub(dv_eval_qc(dv->a), dv_eval_qc(dv->b));
}

static qcomplex_t eval_mul(dval_t *dv) {
    return qc_mul(dv_eval_qc(dv->a), dv_eval_qc(dv->b));
}

static qcomplex_t eval_div(dval_t *dv) {
    return qc_div(dv_eval_qc(dv->a), dv_eval_qc(dv->b));
}

static qcomplex_t eval_neg(dval_t *dv) {
    return qc_neg(dv_eval_qc(dv->a));
}

static qcomplex_t eval_pow(dval_t *dv) {
    qcomplex_t base = dv_eval_qc(dv->a);
    qcomplex_t exp  = dv_eval_qc(dv->b);
    return qc_pow(base, exp);
}

static qcomplex_t eval_pow_d(dval_t *dv) {
    qcomplex_t base = dv_eval_qc(dv->a);
    return qc_pow(base, dv->c);
}

/* --- Trig / Hyperbolic / Exp / Log / Sqrt -------------------------------- */

static qcomplex_t eval_sin(dval_t *dv)   { return qc_sin (dv_eval_qc(dv->a)); }
static qcomplex_t eval_cos(dval_t *dv)   { return qc_cos (dv_eval_qc(dv->a)); }
static qcomplex_t eval_tan(dval_t *dv)   { return qc_tan (dv_eval_qc(dv->a)); }

static qcomplex_t eval_sinh(dval_t *dv)  { return qc_sinh(dv_eval_qc(dv->a)); }
static qcomplex_t eval_cosh(dval_t *dv)  { return qc_cosh(dv_eval_qc(dv->a)); }
static qcomplex_t eval_tanh(dval_t *dv)  { return qc_tanh(dv_eval_qc(dv->a)); }

static qcomplex_t eval_asin(dval_t *dv)  { return qc_asin(dv_eval_qc(dv->a)); }
static qcomplex_t eval_acos(dval_t *dv)  { return qc_acos(dv_eval_qc(dv->a)); }
static qcomplex_t eval_atan(dval_t *dv)  { return qc_atan(dv_eval_qc(dv->a)); }

static qcomplex_t eval_asinh(dval_t *dv) { return qc_asinh(dv_eval_qc(dv->a)); }
static qcomplex_t eval_acosh(dval_t *dv) { return qc_acosh(dv_eval_qc(dv->a)); }
static qcomplex_t eval_atanh(dval_t *dv) { return qc_atanh(dv_eval_qc(dv->a)); }

static qcomplex_t eval_exp(dval_t *dv)    { return qc_exp   (dv_eval_qc(dv->a)); }
static qcomplex_t eval_log(dval_t *dv)    { return qc_log   (dv_eval_qc(dv->a)); }
static qcomplex_t eval_sqrt(dval_t *dv)   { return qc_sqrt  (dv_eval_qc(dv->a)); }

static qcomplex_t eval_abs(dval_t *dv)    { return qc_real_qf(qc_abs(dv_eval_qc(dv->a))); }
static qcomplex_t eval_erf(dval_t *dv)    { return qc_erf   (dv_eval_qc(dv->a)); }
static qcomplex_t eval_erfc(dval_t *dv)   { return qc_erfc  (dv_eval_qc(dv->a)); }
static qcomplex_t eval_lgamma(dval_t *dv) { return qc_lgamma(dv_eval_qc(dv->a)); }

static qcomplex_t eval_hypot(dval_t *dv) {
    return qc_hypot(dv_eval_qc(dv->a), dv_eval_qc(dv->b));
}

static qcomplex_t eval_erfinv(dval_t *dv)        { return qc_erfinv       (dv_eval_qc(dv->a)); }
static qcomplex_t eval_erfcinv(dval_t *dv)       { return qc_erfcinv      (dv_eval_qc(dv->a)); }
static qcomplex_t eval_gamma(dval_t *dv)         { return qc_gamma        (dv_eval_qc(dv->a)); }
static qcomplex_t eval_digamma(dval_t *dv)       { return qc_digamma      (dv_eval_qc(dv->a)); }
static qcomplex_t eval_trigamma(dval_t *dv)      { return qc_trigamma     (dv_eval_qc(dv->a)); }
static qcomplex_t eval_lambert_w0(dval_t *dv)    { return qc_productlog   (dv_eval_qc(dv->a)); }
static qcomplex_t eval_lambert_wm1(dval_t *dv)   { return qc_lambert_wm1  (dv_eval_qc(dv->a)); }
static qcomplex_t eval_normal_pdf(dval_t *dv)    { return qc_normal_pdf   (dv_eval_qc(dv->a)); }
static qcomplex_t eval_normal_cdf(dval_t *dv)    { return qc_normal_cdf   (dv_eval_qc(dv->a)); }
static qcomplex_t eval_normal_logpdf(dval_t *dv) { return qc_normal_logpdf(dv_eval_qc(dv->a)); }
static qcomplex_t eval_ei(dval_t *dv)            { return qc_ei           (dv_eval_qc(dv->a)); }
static qcomplex_t eval_e1(dval_t *dv)            { return qc_e1           (dv_eval_qc(dv->a)); }

static qcomplex_t eval_beta(dval_t *dv) {
    return qc_beta(dv_eval_qc(dv->a), dv_eval_qc(dv->b));
}
static qcomplex_t eval_logbeta(dval_t *dv) {
    return qc_logbeta(dv_eval_qc(dv->a), dv_eval_qc(dv->b));
}

static qcomplex_t eval_atan2(dval_t *dv) {
    qcomplex_t fy = dv_eval_qc(dv->a);
    qcomplex_t gx = dv_eval_qc(dv->b);
    return qc_atan2(fy, gx);
}

/* ------------------------------------------------------------------------- */
/* DERIVATIVE FUNCTIONS — lazy, stored in each node                          */
/* ------------------------------------------------------------------------- */

static dval_t *deriv_const(dval_t *dv)
{
    (void)dv;
    return dv_new_const_d(0.0);
}

static dval_t *deriv_var(dval_t *dv)
{
    /* tl_wrt == NULL: single-variable mode — this variable IS the variable,
     * so return 1.  Otherwise return 1 iff this node is the target variable. */
    return dv_new_const_d(tl_wrt == NULL || dv == tl_wrt ? 1.0 : 0.0);
}

/* a + b */
static dval_t *deriv_add(dval_t *dv)
{
    dval_t *da  = get_dx(dv->a);
    dval_t *db  = get_dx(dv->b);
    dval_t *out = dv_add(da, db);
    dv_free(da);
    dv_free(db);
    return out;
}

/* a - b */
static dval_t *deriv_sub(dval_t *dv)
{
    dval_t *da  = get_dx(dv->a);
    dval_t *db  = get_dx(dv->b);
    dval_t *out = dv_sub(da, db);
    dv_free(da);
    dv_free(db);
    return out;
}

/* a * b */
static dval_t *deriv_mul(dval_t *dv)
{
    dval_t *da  = get_dx(dv->a);
    dval_t *db  = get_dx(dv->b);
    dval_t *t1  = dv_mul(da, dv->b);
    dval_t *t2  = dv_mul(dv->a, db);
    dval_t *out = dv_add(t1, t2);
    dv_free(da);
    dv_free(db);
    dv_free(t1);
    dv_free(t2);
    return out;
}

/* a / b */
static dval_t *deriv_div(dval_t *dv)
{
    dval_t *da   = get_dx(dv->a);
    dval_t *db   = get_dx(dv->b);
    dval_t *num1 = dv_mul(da, dv->b);
    dval_t *num2 = dv_mul(dv->a, db);
    dval_t *num  = dv_sub(num1, num2);
    dval_t *den  = dv_pow_d(dv->b, 2.0);
    dval_t *out  = dv_div(num, den);
    dv_free(da);
    dv_free(db);
    dv_free(num1);
    dv_free(num2);
    dv_free(num);
    dv_free(den);
    return out;
}

/* -a */
static dval_t *deriv_neg(dval_t *dv)
{
    dval_t *da  = get_dx(dv->a);
    dval_t *out = dv_neg(da);
    dv_free(da);
    return out;
}

/* a^b — d/dx = a^b * (b' * log(a) + b * a'/a) */
static dval_t *deriv_pow(dval_t *dv)
{
    dval_t *a  = dv->a;
    dval_t *b  = dv->b;
    dval_t *da = get_dx(a);
    dval_t *db = get_dx(b);

    dval_t *loga  = dv_log(a);
    dval_t *da_on_a = dv_div(da, a);
    dval_t *term1 = dv_mul(db, loga);
    dval_t *term2 = dv_mul(b, da_on_a);
    dval_t *sum   = dv_add(term1, term2);
    dval_t *powab = dv_pow(a, b);
    dval_t *out   = dv_mul(powab, sum);

    dv_free(da);
    dv_free(db);
    dv_free(loga);
    dv_free(da_on_a);
    dv_free(term1);
    dv_free(term2);
    dv_free(sum);
    dv_free(powab);

    return out;
}

/* a^c — d/dx = c * a^(c-1) * a' */
static dval_t *deriv_pow_d(dval_t *dv)
{
    double  c    = qf_to_double(dv->c.re);
    dval_t *da   = get_dx(dv->a);
    dval_t *p    = dv_pow_d(dv->a, c - 1.0);
    dval_t *coef = dv_new_const_d(c);
    dval_t *cp   = dv_mul(coef, p);
    dval_t *out  = dv_mul(cp, da);
    dv_free(da);
    dv_free(p);
    dv_free(coef);
    dv_free(cp);
    return out;
}

/* sin(a) — d/dx = cos(a) * a' */
static dval_t *deriv_sin(dval_t *dv)
{
    dval_t *da  = get_dx(dv->a);
    dval_t *ca  = dv_cos(dv->a);
    dval_t *out = dv_mul(ca, da);
    dv_free(ca);
    dv_free(da);
    return out;
}

/* cos(a) — d/dx = -sin(a) * a' */
static dval_t *deriv_cos(dval_t *dv)
{
    dval_t *da      = get_dx(dv->a);
    dval_t *sin_a   = dv_sin(dv->a);
    dval_t *neg_sin = dv_neg(sin_a);
    dv_free(sin_a);
    dval_t *out     = dv_mul(neg_sin, da);
    dv_free(da);
    dv_free(neg_sin);
    return out;
}

/* tan(a) — d/dx = (1 + tan²(a)) * a' */
static dval_t *deriv_tan(dval_t *dv)
{
    dval_t *da  = get_dx(dv->a);
    dval_t *t   = dv_tan(dv->a);
    dval_t *t2  = dv_pow_d(t, 2.0);
    dval_t *one = dv_new_const_d(1.0);
    dval_t *fac = dv_add(one, t2);
    dval_t *out = dv_mul(fac, da);
    dv_free(da);
    dv_free(t);
    dv_free(t2);
    dv_free(one);
    dv_free(fac);
    return out;
}

/* sinh(a) — d/dx = cosh(a) * a' */
static dval_t *deriv_sinh(dval_t *dv)
{
    dval_t *da   = get_dx(dv->a);
    dval_t *ca   = dv_cosh(dv->a);
    dval_t *out  = dv_mul(ca, da);
    dv_free(ca);
    dv_free(da);
    return out;
}

/* cosh(a) — d/dx = sinh(a) * a' */
static dval_t *deriv_cosh(dval_t *dv)
{
    dval_t *da   = get_dx(dv->a);
    dval_t *sa   = dv_sinh(dv->a);
    dval_t *out  = dv_mul(sa, da);
    dv_free(sa);
    dv_free(da);
    return out;
}

/* tanh(a) — d/dx = (1 - tanh²(a)) * a' */
static dval_t *deriv_tanh(dval_t *dv)
{
    dval_t *da  = get_dx(dv->a);
    dval_t *t   = dv_tanh(dv->a);
    dval_t *t2  = dv_pow_d(t, 2.0);
    dval_t *one = dv_new_const_d(1.0);
    dval_t *fac = dv_sub(one, t2);
    dval_t *out = dv_mul(fac, da);
    dv_free(da);
    dv_free(t);
    dv_free(t2);
    dv_free(one);
    dv_free(fac);
    return out;
}

/* exp(a) — d/dx = exp(a) * a' */
static dval_t *deriv_exp(dval_t *dv)
{
    dval_t *da   = get_dx(dv->a);
    dval_t *ea   = dv_exp(dv->a);
    dval_t *out  = dv_mul(ea, da);
    dv_free(ea);
    dv_free(da);
    return out;
}

/* log(a) — d/dx = a' / a */
static dval_t *deriv_log(dval_t *dv)
{
    dval_t *da  = get_dx(dv->a);
    dval_t *out = dv_div(da, dv->a);
    dv_free(da);
    return out;
}

/* sqrt(a) — d/dx = a' / (2 * sqrt(a)) */
static dval_t *deriv_sqrt(dval_t *dv)
{
    dval_t *da   = get_dx(dv->a);
    dval_t *two  = dv_new_const_d(2.0);
    dval_t *sqra = dv_sqrt(dv->a);
    dval_t *den  = dv_mul(two, sqra);
    dv_free(sqra);
    dval_t *out  = dv_div(da, den);
    dv_free(da);
    dv_free(two);
    dv_free(den);
    return out;
}

/* asin(a) — d/dx = a' / sqrt(1 - a²) */
static dval_t *deriv_asin(dval_t *dv)
{
    dval_t *da   = get_dx(dv->a);
    dval_t *a2   = dv_pow_d(dv->a, 2.0);
    dval_t *one  = dv_new_const_d(1.0);
    dval_t *sub  = dv_sub(one, a2);
    dval_t *den  = dv_sqrt(sub);
    dv_free(sub);
    dval_t *out  = dv_div(da, den);
    dv_free(da);
    dv_free(a2);
    dv_free(one);
    dv_free(den);
    return out;
}

/* acos(a) — d/dx = -a' / sqrt(1 - a²) */
static dval_t *deriv_acos(dval_t *dv)
{
    dval_t *da   = get_dx(dv->a);
    dval_t *a2   = dv_pow_d(dv->a, 2.0);
    dval_t *one  = dv_new_const_d(1.0);
    dval_t *sub  = dv_sub(one, a2);
    dval_t *den  = dv_sqrt(sub);
    dv_free(sub);
    dval_t *num  = dv_neg(da);
    dval_t *out  = dv_div(num, den);
    dv_free(da);
    dv_free(a2);
    dv_free(one);
    dv_free(den);
    dv_free(num);
    return out;
}

/* atan(a) — d/dx = a' / (1 + a²) */
static dval_t *deriv_atan(dval_t *dv)
{
    dval_t *da  = get_dx(dv->a);
    dval_t *a2  = dv_pow_d(dv->a, 2.0);
    dval_t *one = dv_new_const_d(1.0);
    dval_t *den = dv_add(one, a2);
    dval_t *out = dv_div(da, den);
    dv_free(da);
    dv_free(a2);
    dv_free(one);
    dv_free(den);
    return out;
}

/* atan2(y, x) — d/dx = (x*y' - y*x') / (x² + y²) */
static dval_t *deriv_atan2(dval_t *dv)
{
    dval_t *y  = dv->a;  /* first argument */
    dval_t *x  = dv->b;  /* second argument */
    dval_t *dy = get_dx(y);
    dval_t *dx = get_dx(x);

    /* numerator = x*dy - y*dx */
    dval_t *x_dy = dv_mul(x, dy);
    dval_t *y_dx = dv_mul(y, dx);
    dval_t *num  = dv_sub(x_dy, y_dx);

    /* denominator = x² + y² */
    dval_t *x2  = dv_mul(x, x);
    dval_t *y2  = dv_mul(y, y);
    dval_t *den = dv_add(x2, y2);

    dval_t *out = dv_div(num, den);

    dv_free(dy);
    dv_free(dx);
    dv_free(x_dy);
    dv_free(y_dx);
    dv_free(num);
    dv_free(x2);
    dv_free(y2);
    dv_free(den);

    return out;
}

/* asinh(a) — d/dx = a' / sqrt(1 + a²) */
static dval_t *deriv_asinh(dval_t *dv)
{
    dval_t *da   = get_dx(dv->a);
    dval_t *a2   = dv_pow_d(dv->a, 2.0);
    dval_t *one  = dv_new_const_d(1.0);
    dval_t *sum  = dv_add(one, a2);
    dval_t *den  = dv_sqrt(sum);
    dv_free(sum);
    dval_t *out  = dv_div(da, den);
    dv_free(da);
    dv_free(a2);
    dv_free(one);
    dv_free(den);
    return out;
}

/* acosh(a) — d/dx = a' / (sqrt(a-1) * sqrt(a+1)) */
static dval_t *deriv_acosh(dval_t *dv)
{
    dval_t *da  = get_dx(dv->a);
    dval_t *one = dv_new_const_d(1.0);
    dval_t *am1 = dv_sub(dv->a, one);
    dval_t *ap1 = dv_add(dv->a, one);
    dval_t *s1  = dv_sqrt(am1);
    dval_t *s2  = dv_sqrt(ap1);
    dval_t *den = dv_mul(s1, s2);
    dval_t *out = dv_div(da, den);
    dv_free(da);
    dv_free(one);
    dv_free(am1);
    dv_free(ap1);
    dv_free(s1);
    dv_free(s2);
    dv_free(den);
    return out;
}

/* atanh(a) — d/dx = a' / (1 - a²) */
static dval_t *deriv_atanh(dval_t *dv)
{
    dval_t *da  = get_dx(dv->a);
    dval_t *a2  = dv_pow_d(dv->a, 2.0);
    dval_t *one = dv_new_const_d(1.0);
    dval_t *den = dv_sub(one, a2);
    dval_t *out = dv_div(da, den);
    dv_free(da);
    dv_free(a2);
    dv_free(one);
    dv_free(den);
    return out;
}

/* abs(a) — d/dx = sign(a) * a' = (a / |a|) * a'
 * Undefined at a = 0; subgradient value is 0 there (via 0/0 → NaN path,
 * which callers should avoid). */
static dval_t *deriv_abs(dval_t *dv)
{
    dval_t *da   = get_dx(dv->a);
    dval_t *absa = dv_abs(dv->a);
    dval_t *sign = dv_div(dv->a, absa);
    dval_t *out  = dv_mul(sign, da);
    dv_free(da);
    dv_free(absa);
    dv_free(sign);
    return out;
}

/* Helper: 2/√π — used by erf and erfc derivatives */
static qfloat_t two_over_sqrtpi(void)
{
    qfloat_t pi = qf_mul(qf_from_double(4.0), qf_atan(qf_from_double(1.0)));
    return qf_div(qf_from_double(2.0), qf_sqrt(pi));
}

/* erf(a) — d/dx = (2/√π) * exp(-a²) * a' */
static dval_t *deriv_erf(dval_t *dv)
{
    dval_t *da     = get_dx(dv->a);
    dval_t *c      = dv_new_const(two_over_sqrtpi());
    dval_t *a2     = dv_pow_d(dv->a, 2.0);
    dval_t *neg_a2 = dv_neg(a2);
    dval_t *ea2    = dv_exp(neg_a2);
    dval_t *fac    = dv_mul(c, ea2);
    dval_t *out    = dv_mul(fac, da);
    dv_free(da);
    dv_free(c);
    dv_free(a2);
    dv_free(neg_a2);
    dv_free(ea2);
    dv_free(fac);
    return out;
}

/* erfc(a) — d/dx = -(2/√π) * exp(-a²) * a' */
static dval_t *deriv_erfc(dval_t *dv)
{
    dval_t *da     = get_dx(dv->a);
    dval_t *c      = dv_new_const(qf_neg(two_over_sqrtpi()));
    dval_t *a2     = dv_pow_d(dv->a, 2.0);
    dval_t *neg_a2 = dv_neg(a2);
    dval_t *ea2    = dv_exp(neg_a2);
    dval_t *fac    = dv_mul(c, ea2);
    dval_t *out    = dv_mul(fac, da);
    dv_free(da);
    dv_free(c);
    dv_free(a2);
    dv_free(neg_a2);
    dv_free(ea2);
    dv_free(fac);
    return out;
}

/* lgamma(a) — d/dx = digamma(a) * a'
 * digamma coefficient is evaluated numerically at differentiation time
 * (no dv_digamma node yet; higher-order derivatives of lgamma are not supported). */
/* lgamma(a) — d/dx = digamma(a) * a'
 * Return a symbolic digamma node so that higher-order derivatives remain
 * meaningful (digamma' = frozen trigamma, giving correct d²/dx² lgamma). */
static dval_t *deriv_lgamma(dval_t *dv)
{
    dval_t *da  = get_dx(dv->a);
    dval_t *dg  = dv_digamma(dv->a);
    dval_t *out = dv_mul(dg, da);
    dv_free(da);
    dv_free(dg);
    return out;
}

/* hypot(a, b) — d/dx = (a·a' + b·b') / hypot(a, b) */
static dval_t *deriv_hypot(dval_t *dv)
{
    dval_t *a    = dv->a;
    dval_t *b    = dv->b;
    dval_t *da   = get_dx(a);
    dval_t *db   = get_dx(b);
    dval_t *a_da = dv_mul(a, da);
    dval_t *b_db = dv_mul(b, db);
    dval_t *num  = dv_add(a_da, b_db);
    dval_t *h    = dv_hypot(a, b);
    dval_t *out  = dv_div(num, h);
    dv_free(da);
    dv_free(db);
    dv_free(a_da);
    dv_free(b_db);
    dv_free(num);
    dv_free(h);
    return out;
}

/* Helper: √π/2 — used by erfinv and erfcinv derivatives */
static qfloat_t sqrtpi_over_2(void)
{
    qfloat_t pi = qf_mul(qf_from_double(4.0), qf_atan(qf_from_double(1.0)));
    return qf_div(qf_sqrt(pi), qf_from_double(2.0));
}

/* erfinv(a) — d/dx = (√π/2) * exp(erfinv(a)²) * a' */
static dval_t *deriv_erfinv(dval_t *dv)
{
    dval_t *da  = get_dx(dv->a);
    dval_t *w   = dv_erfinv(dv->a);
    dval_t *w2  = dv_pow_d(w, 2.0);
    dval_t *ew2 = dv_exp(w2);
    dval_t *c   = dv_new_const(sqrtpi_over_2());
    dval_t *fac = dv_mul(c, ew2);
    dval_t *out = dv_mul(fac, da);
    dv_free(da); dv_free(w); dv_free(w2); dv_free(ew2); dv_free(c); dv_free(fac);
    return out;
}

/* erfcinv(a) — d/dx = -(√π/2) * exp(erfcinv(a)²) * a' */
static dval_t *deriv_erfcinv(dval_t *dv)
{
    dval_t *da  = get_dx(dv->a);
    dval_t *w   = dv_erfcinv(dv->a);
    dval_t *w2  = dv_pow_d(w, 2.0);
    dval_t *ew2 = dv_exp(w2);
    dval_t *c   = dv_new_const(qf_neg(sqrtpi_over_2()));
    dval_t *fac = dv_mul(c, ew2);
    dval_t *out = dv_mul(fac, da);
    dv_free(da); dv_free(w); dv_free(w2); dv_free(ew2); dv_free(c); dv_free(fac);
    return out;
}

/* gamma(a) — d/dx = gamma(a) * digamma(a) * a' */
static dval_t *deriv_gamma(dval_t *dv)
{
    dval_t *da  = get_dx(dv->a);
    dval_t *g   = dv_gamma(dv->a);
    dval_t *dg  = dv_digamma(dv->a);
    dval_t *gdg = dv_mul(g, dg);
    dval_t *out = dv_mul(gdg, da);
    dv_free(da); dv_free(g); dv_free(dg); dv_free(gdg);
    return out;
}

/* digamma(a) — d/dx = trigamma(a) * a' (symbolic) */
static dval_t *deriv_digamma(dval_t *dv)
{
    dval_t *da  = get_dx(dv->a);
    dval_t *tg  = dv_trigamma(dv->a);
    dval_t *out = dv_mul(tg, da);
    dv_free(da); dv_free(tg);
    return out;
}

/* trigamma(a) — d/dx = tetragamma(a) * a' */
static dval_t *deriv_trigamma(dval_t *dv)
{
    qfloat_t t2     = qf_tetragamma(dv_eval_qf(dv->a));
    dval_t *da    = get_dx(dv->a);
    dval_t *coeff = dv_new_const(t2);
    dval_t *out   = dv_mul(coeff, da);
    dv_free(da); dv_free(coeff);
    return out;
}

/* lambert_w0(a) — d/dx = W₀(a) / (a * (1 + W₀(a))) * a' */
static dval_t *deriv_lambert_w0(dval_t *dv)
{
    dval_t *da  = get_dx(dv->a);
    dval_t *w   = dv_lambert_w0(dv->a);
    dval_t *wp1 = dv_add_d(w, 1.0);
    dval_t *den = dv_mul(dv->a, wp1);
    dval_t *fac = dv_div(w, den);
    dval_t *out = dv_mul(fac, da);
    dv_free(da); dv_free(w); dv_free(wp1); dv_free(den); dv_free(fac);
    return out;
}

/* lambert_wm1(a) — d/dx = W₋₁(a) / (a * (1 + W₋₁(a))) * a' */
static dval_t *deriv_lambert_wm1(dval_t *dv)
{
    dval_t *da  = get_dx(dv->a);
    dval_t *w   = dv_lambert_wm1(dv->a);
    dval_t *wp1 = dv_add_d(w, 1.0);
    dval_t *den = dv_mul(dv->a, wp1);
    dval_t *fac = dv_div(w, den);
    dval_t *out = dv_mul(fac, da);
    dv_free(da); dv_free(w); dv_free(wp1); dv_free(den); dv_free(fac);
    return out;
}

/* normal_pdf(a) — d/dx = -a * normal_pdf(a) * a' */
static dval_t *deriv_normal_pdf(dval_t *dv)
{
    dval_t *da   = get_dx(dv->a);
    dval_t *neg_a = dv_neg(dv->a);
    dval_t *phi  = dv_normal_pdf(dv->a);
    dval_t *fac  = dv_mul(neg_a, phi);
    dval_t *out  = dv_mul(fac, da);
    dv_free(da); dv_free(neg_a); dv_free(phi); dv_free(fac);
    return out;
}

/* normal_cdf(a) — d/dx = normal_pdf(a) * a' */
static dval_t *deriv_normal_cdf(dval_t *dv)
{
    dval_t *da  = get_dx(dv->a);
    dval_t *phi = dv_normal_pdf(dv->a);
    dval_t *out = dv_mul(phi, da);
    dv_free(da); dv_free(phi);
    return out;
}

/* normal_logpdf(a) — d/dx = -a * a'
 * log φ(x) = -x²/2 - log(√(2π)), so d/dx = -x */
static dval_t *deriv_normal_logpdf(dval_t *dv)
{
    dval_t *da    = get_dx(dv->a);
    dval_t *neg_a = dv_neg(dv->a);
    dval_t *out   = dv_mul(neg_a, da);
    dv_free(da); dv_free(neg_a);
    return out;
}

/* ei(a) — d/dx = exp(a) / a * a' */
static dval_t *deriv_ei(dval_t *dv)
{
    dval_t *da  = get_dx(dv->a);
    dval_t *ea  = dv_exp(dv->a);
    dval_t *fac = dv_div(ea, dv->a);
    dval_t *out = dv_mul(fac, da);
    dv_free(da); dv_free(ea); dv_free(fac);
    return out;
}

/* e1(a) — d/dx = -exp(-a) / a * a' */
static dval_t *deriv_e1(dval_t *dv)
{
    dval_t *da    = get_dx(dv->a);
    dval_t *neg_a = dv_neg(dv->a);
    dval_t *en_a  = dv_exp(neg_a);
    dval_t *neg_en = dv_neg(en_a);
    dval_t *fac   = dv_div(neg_en, dv->a);
    dval_t *out   = dv_mul(fac, da);
    dv_free(da); dv_free(neg_a); dv_free(en_a); dv_free(neg_en); dv_free(fac);
    return out;
}

/* beta(a, b) — partial derivatives frozen numerically at differentiation time:
 *   d/da = B(a,b) * (ψ(a) - ψ(a+b)) * a'
 *   d/db = B(a,b) * (ψ(b) - ψ(a+b)) * b'
 */
/* beta(a, b) — d/da = beta(a,b) * (ψ(a) - ψ(a+b)) * a'
 *               d/db = beta(a,b) * (ψ(b) - ψ(a+b)) * b'
 * Built symbolically so that second derivatives propagate correctly. */
static dval_t *deriv_beta(dval_t *dv)
{
    dval_t *da    = get_dx(dv->a);
    dval_t *db    = get_dx(dv->b);
    dval_t *apb   = dv_add(dv->a, dv->b);
    dval_t *dg_a  = dv_digamma(dv->a);
    dval_t *dg_b  = dv_digamma(dv->b);
    dval_t *dg_ab = dv_digamma(apb);
    dval_t *diff_a = dv_sub(dg_a, dg_ab);
    dval_t *diff_b = dv_sub(dg_b, dg_ab);
    dval_t *beta_n = dv_beta(dv->a, dv->b);
    dval_t *ca    = dv_mul(beta_n, diff_a);
    dval_t *cb    = dv_mul(beta_n, diff_b);
    dval_t *ta    = dv_mul(ca, da);
    dval_t *tb    = dv_mul(cb, db);
    dval_t *out   = dv_add(ta, tb);
    dv_free(da); dv_free(db); dv_free(apb);
    dv_free(dg_a); dv_free(dg_b); dv_free(dg_ab);
    dv_free(diff_a); dv_free(diff_b); dv_free(beta_n);
    dv_free(ca); dv_free(cb); dv_free(ta); dv_free(tb);
    return out;
}

/* logbeta(a, b) — d/da = (ψ(a) - ψ(a+b)) * a'
 *                 d/db = (ψ(b) - ψ(a+b)) * b'
 * Built symbolically so that second derivatives propagate correctly. */
static dval_t *deriv_logbeta(dval_t *dv)
{
    dval_t *da    = get_dx(dv->a);
    dval_t *db    = get_dx(dv->b);
    dval_t *apb   = dv_add(dv->a, dv->b);
    dval_t *dg_a  = dv_digamma(dv->a);
    dval_t *dg_b  = dv_digamma(dv->b);
    dval_t *dg_ab = dv_digamma(apb);
    dval_t *diff_a = dv_sub(dg_a, dg_ab);
    dval_t *diff_b = dv_sub(dg_b, dg_ab);
    dval_t *ta    = dv_mul(diff_a, da);
    dval_t *tb    = dv_mul(diff_b, db);
    dval_t *out   = dv_add(ta, tb);
    dv_free(da); dv_free(db); dv_free(apb);
    dv_free(dg_a); dv_free(dg_b); dv_free(dg_ab);
    dv_free(diff_a); dv_free(diff_b); dv_free(ta); dv_free(tb);
    return out;
}

/* ------------------------------------------------------------------------- */
/* Reverse-mode local adjoint propagation                                    */
/* ------------------------------------------------------------------------- */

static qfloat_t qf_from_int_local(int x)
{
    return qf_from_double((double)x);
}

static qfloat_t qf_sq_local(qfloat_t x)
{
    return qf_mul(x, x);
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

void dv_reverse_atan2(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    qfloat_t denom = qf_add(qf_sq_local(dv->a->x.re), qf_sq_local(dv->b->x.re));
    *a_bar = qf_mul(out_bar, qf_div(dv->b->x.re, denom));
    *b_bar = qf_neg(qf_mul(out_bar, qf_div(dv->a->x.re, denom)));
}

void dv_reverse_neg(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    (void)dv;
    *a_bar = qf_neg(out_bar);
    *b_bar = QF_ZERO;
}

void dv_reverse_sin(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    *a_bar = qf_mul(out_bar, qf_cos(dv->a->x.re));
    *b_bar = QF_ZERO;
}

void dv_reverse_cos(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    *a_bar = qf_neg(qf_mul(out_bar, qf_sin(dv->a->x.re)));
    *b_bar = QF_ZERO;
}

void dv_reverse_tan(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    qfloat_t cos_x = qf_cos(dv->a->x.re);
    *a_bar = qf_mul(out_bar, qf_div(QF_ONE, qf_sq_local(cos_x)));
    *b_bar = QF_ZERO;
}

void dv_reverse_sinh(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    *a_bar = qf_mul(out_bar, qf_cosh(dv->a->x.re));
    *b_bar = QF_ZERO;
}

void dv_reverse_cosh(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    *a_bar = qf_mul(out_bar, qf_sinh(dv->a->x.re));
    *b_bar = QF_ZERO;
}

void dv_reverse_tanh(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    *a_bar = qf_mul(out_bar, qf_sub(QF_ONE, qf_sq_local(dv->x.re)));
    *b_bar = QF_ZERO;
}

void dv_reverse_asin(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    qfloat_t denom = qf_sqrt(qf_sub(QF_ONE, qf_sq_local(dv->a->x.re)));
    *a_bar = qf_div(out_bar, denom);
    *b_bar = QF_ZERO;
}

void dv_reverse_acos(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    qfloat_t denom = qf_sqrt(qf_sub(QF_ONE, qf_sq_local(dv->a->x.re)));
    *a_bar = qf_neg(qf_div(out_bar, denom));
    *b_bar = QF_ZERO;
}

void dv_reverse_atan(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    *a_bar = qf_div(out_bar, qf_add(QF_ONE, qf_sq_local(dv->a->x.re)));
    *b_bar = QF_ZERO;
}

void dv_reverse_asinh(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    qfloat_t denom = qf_sqrt(qf_add(qf_sq_local(dv->a->x.re), QF_ONE));
    *a_bar = qf_div(out_bar, denom);
    *b_bar = QF_ZERO;
}

void dv_reverse_acosh(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    qfloat_t denom = qf_mul(qf_sqrt(qf_sub(dv->a->x.re, QF_ONE)),
                            qf_sqrt(qf_add(dv->a->x.re, QF_ONE)));
    *a_bar = qf_div(out_bar, denom);
    *b_bar = QF_ZERO;
}

void dv_reverse_atanh(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    *a_bar = qf_div(out_bar, qf_sub(QF_ONE, qf_sq_local(dv->a->x.re)));
    *b_bar = QF_ZERO;
}

void dv_reverse_exp(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    *a_bar = qf_mul(out_bar, dv->x.re);
    *b_bar = QF_ZERO;
}

void dv_reverse_log(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    *a_bar = qf_div(out_bar, dv->a->x.re);
    *b_bar = QF_ZERO;
}

void dv_reverse_sqrt(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    *a_bar = qf_div(out_bar, qf_mul(qf_from_int_local(2), dv->x.re));
    *b_bar = QF_ZERO;
}

void dv_reverse_abs(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    int cmp = qf_cmp(dv->a->x.re, QF_ZERO);
    if (cmp > 0)
        *a_bar = out_bar;
    else if (cmp < 0)
        *a_bar = qf_neg(out_bar);
    else
        *a_bar = QF_ZERO;
    *b_bar = QF_ZERO;
}

void dv_reverse_hypot(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    *a_bar = qf_mul(out_bar, qf_div(dv->a->x.re, dv->x.re));
    *b_bar = qf_mul(out_bar, qf_div(dv->b->x.re, dv->x.re));
}

void dv_reverse_erf(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    qfloat_t scale = qf_div(qf_from_int_local(2), qf_sqrt(QF_PI));
    *a_bar = qf_mul(out_bar, qf_mul(scale, qf_exp(qf_neg(qf_sq_local(dv->a->x.re)))));
    *b_bar = QF_ZERO;
}

void dv_reverse_erfc(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    qfloat_t scale = qf_div(qf_from_int_local(2), qf_sqrt(QF_PI));
    *a_bar = qf_neg(qf_mul(out_bar, qf_mul(scale, qf_exp(qf_neg(qf_sq_local(dv->a->x.re))))));
    *b_bar = QF_ZERO;
}

void dv_reverse_erfinv(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    qfloat_t scale = qf_div(qf_sqrt(QF_PI), qf_from_int_local(2));
    *a_bar = qf_mul(out_bar, qf_mul(scale, qf_exp(qf_sq_local(dv->x.re))));
    *b_bar = QF_ZERO;
}

void dv_reverse_erfcinv(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    qfloat_t scale = qf_div(qf_sqrt(QF_PI), qf_from_int_local(2));
    *a_bar = qf_neg(qf_mul(out_bar, qf_mul(scale, qf_exp(qf_sq_local(dv->x.re)))));
    *b_bar = QF_ZERO;
}

void dv_reverse_gamma(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    *a_bar = qf_mul(out_bar, qf_mul(dv->x.re, qf_digamma(dv->a->x.re)));
    *b_bar = QF_ZERO;
}

void dv_reverse_lgamma(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    (void)dv;
    *a_bar = qf_mul(out_bar, qf_digamma(dv->a->x.re));
    *b_bar = QF_ZERO;
}

void dv_reverse_digamma(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    (void)dv;
    *a_bar = qf_mul(out_bar, qf_trigamma(dv->a->x.re));
    *b_bar = QF_ZERO;
}

void dv_reverse_trigamma(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    (void)dv;
    *a_bar = qf_mul(out_bar, qf_tetragamma(dv->a->x.re));
    *b_bar = QF_ZERO;
}

static qfloat_t qf_lambert_reverse_factor(qfloat_t z, qfloat_t w)
{
    if (qf_eq(z, QF_ZERO))
        return QF_ONE;
    return qf_div(w, qf_mul(z, qf_add(QF_ONE, w)));
}

void dv_reverse_lambert_w0(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    *a_bar = qf_mul(out_bar, qf_lambert_reverse_factor(dv->a->x.re, dv->x.re));
    *b_bar = QF_ZERO;
}

void dv_reverse_lambert_wm1(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    *a_bar = qf_mul(out_bar, qf_lambert_reverse_factor(dv->a->x.re, dv->x.re));
    *b_bar = QF_ZERO;
}

void dv_reverse_normal_pdf(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    *a_bar = qf_neg(qf_mul(out_bar, qf_mul(dv->a->x.re, dv->x.re)));
    *b_bar = QF_ZERO;
}

void dv_reverse_normal_cdf(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    *a_bar = qf_mul(out_bar, qf_normal_pdf(dv->a->x.re));
    *b_bar = QF_ZERO;
}

void dv_reverse_normal_logpdf(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    *a_bar = qf_neg(qf_mul(out_bar, dv->a->x.re));
    *b_bar = QF_ZERO;
}

void dv_reverse_ei(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    *a_bar = qf_mul(out_bar, qf_div(qf_exp(dv->a->x.re), dv->a->x.re));
    *b_bar = QF_ZERO;
}

void dv_reverse_e1(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    *a_bar = qf_neg(qf_mul(out_bar, qf_div(qf_exp(qf_neg(dv->a->x.re)), dv->a->x.re)));
    *b_bar = QF_ZERO;
}

void dv_reverse_beta(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    qfloat_t psi_ab = qf_digamma(qf_add(dv->a->x.re, dv->b->x.re));
    *a_bar = qf_mul(out_bar, qf_mul(dv->x.re, qf_sub(qf_digamma(dv->a->x.re), psi_ab)));
    *b_bar = qf_mul(out_bar, qf_mul(dv->x.re, qf_sub(qf_digamma(dv->b->x.re), psi_ab)));
}

void dv_reverse_logbeta(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    qfloat_t psi_ab = qf_digamma(qf_add(dv->a->x.re, dv->b->x.re));
    (void)dv;
    *a_bar = qf_mul(out_bar, qf_sub(qf_digamma(dv->a->x.re), psi_ab));
    *b_bar = qf_mul(out_bar, qf_sub(qf_digamma(dv->b->x.re), psi_ab));
}

/* ------------------------------------------------------------------------- */
/* Operator vtable instances                                                 */
/* ------------------------------------------------------------------------- */

/* -------------------- ATOMS -------------------- */

const dval_ops_t ops_const = {
    .eval = eval_const,
    .deriv = deriv_const,
    .reverse = dv_reverse_atom,
    .kind = DV_KIND_CONST,
    .arity = DV_OP_ATOM,
    .name = "const",
    .apply_unary = NULL,
    .apply_binary = NULL,
    .simplify = dv_simplify_passthrough,
    .fold_const_unary = NULL
};

const dval_ops_t ops_var = {
    .eval = eval_var,
    .deriv = deriv_var,
    .reverse = dv_reverse_atom,
    .kind = DV_KIND_VAR,
    .arity = DV_OP_ATOM,
    .name = "var",
    .apply_unary = NULL,
    .apply_binary = NULL,
    .simplify = dv_simplify_passthrough,
    .fold_const_unary = NULL
};

/* -------------------- BINARY OPS -------------------- */

const dval_ops_t ops_add = {
    .eval = eval_add,
    .deriv = deriv_add,
    .reverse = dv_reverse_add,
    .kind = DV_KIND_ADD,
    .arity = DV_OP_BINARY,
    .name = "+",
    .apply_unary = NULL,
    .apply_binary = dv_add,
    .simplify = dv_simplify_add_sub_operator,
    .fold_const_unary = NULL
};

const dval_ops_t ops_sub = {
    .eval = eval_sub,
    .deriv = deriv_sub,
    .reverse = dv_reverse_sub,
    .kind = DV_KIND_SUB,
    .arity = DV_OP_BINARY,
    .name = "-",
    .apply_unary = NULL,
    .apply_binary = dv_sub,
    .simplify = dv_simplify_add_sub_operator,
    .fold_const_unary = NULL
};

const dval_ops_t ops_mul = {
    .eval = eval_mul,
    .deriv = deriv_mul,
    .reverse = dv_reverse_mul,
    .kind = DV_KIND_MUL,
    .arity = DV_OP_BINARY,
    .name = "*",
    .apply_unary = NULL,
    .apply_binary = dv_mul,
    .simplify = dv_simplify_mul_operator,
    .fold_const_unary = NULL
};

const dval_ops_t ops_div = {
    .eval = eval_div,
    .deriv = deriv_div,
    .reverse = dv_reverse_div,
    .kind = DV_KIND_DIV,
    .arity = DV_OP_BINARY,
    .name = "/",
    .apply_unary = NULL,
    .apply_binary = dv_div,
    .simplify = dv_simplify_div_operator,
    .fold_const_unary = NULL
};

const dval_ops_t ops_pow = {
    .eval = eval_pow,
    .deriv = deriv_pow,
    .reverse = dv_reverse_pow,
    .kind = DV_KIND_POW,
    .arity = DV_OP_BINARY,
    .name = "^",
    .apply_unary = NULL,
    .apply_binary = dv_pow,
    .simplify = dv_simplify_pow_operator,
    .fold_const_unary = NULL
};

const dval_ops_t ops_pow_d = {
    .eval = eval_pow_d,
    .deriv = deriv_pow_d,
    .reverse = dv_reverse_pow_d,
    .kind = DV_KIND_POW_D,
    .arity = DV_OP_BINARY,
    .name = "^",
    .apply_unary = NULL,
    .apply_binary = NULL,
    .simplify = dv_simplify_pow_d_operator,
    .fold_const_unary = NULL
};

const dval_ops_t ops_atan2 = {
    .eval = eval_atan2,
    .deriv = deriv_atan2,
    .reverse = dv_reverse_atan2,
    .kind = DV_KIND_ATAN2,
    .arity = DV_OP_BINARY,
    .name = "atan2",
    .apply_unary = NULL,
    .apply_binary = dv_atan2,
    .simplify = dv_simplify_binary_operator,
    .fold_const_unary = NULL
};

/* -------------------- UNARY OPS -------------------- */

const dval_ops_t ops_neg = {
    .eval = eval_neg,
    .deriv = deriv_neg,
    .reverse = dv_reverse_neg,
    .kind = DV_KIND_NEG,
    .arity = DV_OP_UNARY,
    .name = "-",
    .apply_unary = dv_neg,
    .apply_binary = NULL,
    .simplify = dv_simplify_neg_operator,
    .fold_const_unary = NULL
};

const dval_ops_t ops_sin = {
    .eval = eval_sin,
    .deriv = deriv_sin,
    .reverse = dv_reverse_sin,
    .kind = DV_KIND_SIN,
    .arity = DV_OP_UNARY,
    .name = "sin",
    .apply_unary = dv_sin,
    .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator,
    .fold_const_unary = dv_fold_zero_to_zero
};

const dval_ops_t ops_cos = {
    .eval = eval_cos,
    .deriv = deriv_cos,
    .reverse = dv_reverse_cos,
    .kind = DV_KIND_COS,
    .arity = DV_OP_UNARY,
    .name = "cos",
    .apply_unary = dv_cos,
    .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator,
    .fold_const_unary = dv_fold_cos_const
};

const dval_ops_t ops_tan = {
    .eval = eval_tan,
    .deriv = deriv_tan,
    .reverse = dv_reverse_tan,
    .kind = DV_KIND_TAN,
    .arity = DV_OP_UNARY,
    .name = "tan",
    .apply_unary = dv_tan,
    .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator,
    .fold_const_unary = dv_fold_zero_to_zero
};

const dval_ops_t ops_sinh = {
    .eval = eval_sinh,
    .deriv = deriv_sinh,
    .reverse = dv_reverse_sinh,
    .kind = DV_KIND_SINH,
    .arity = DV_OP_UNARY,
    .name = "sinh",
    .apply_unary = dv_sinh,
    .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator,
    .fold_const_unary = NULL
};

const dval_ops_t ops_cosh = {
    .eval = eval_cosh,
    .deriv = deriv_cosh,
    .reverse = dv_reverse_cosh,
    .kind = DV_KIND_COSH,
    .arity = DV_OP_UNARY,
    .name = "cosh",
    .apply_unary = dv_cosh,
    .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator,
    .fold_const_unary = NULL
};

const dval_ops_t ops_tanh = {
    .eval = eval_tanh,
    .deriv = deriv_tanh,
    .reverse = dv_reverse_tanh,
    .kind = DV_KIND_TANH,
    .arity = DV_OP_UNARY,
    .name = "tanh",
    .apply_unary = dv_tanh,
    .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator,
    .fold_const_unary = NULL
};

const dval_ops_t ops_asin = {
    .eval = eval_asin,
    .deriv = deriv_asin,
    .reverse = dv_reverse_asin,
    .kind = DV_KIND_ASIN,
    .arity = DV_OP_UNARY,
    .name = "asin",
    .apply_unary = dv_asin,
    .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator,
    .fold_const_unary = NULL
};

const dval_ops_t ops_acos = {
    .eval = eval_acos,
    .deriv = deriv_acos,
    .reverse = dv_reverse_acos,
    .kind = DV_KIND_ACOS,
    .arity = DV_OP_UNARY,
    .name = "acos",
    .apply_unary = dv_acos,
    .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator,
    .fold_const_unary = NULL
};

const dval_ops_t ops_atan = {
    .eval = eval_atan,
    .deriv = deriv_atan,
    .reverse = dv_reverse_atan,
    .kind = DV_KIND_ATAN,
    .arity = DV_OP_UNARY,
    .name = "atan",
    .apply_unary = dv_atan,
    .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator,
    .fold_const_unary = NULL
};

const dval_ops_t ops_asinh = {
    .eval = eval_asinh,
    .deriv = deriv_asinh,
    .reverse = dv_reverse_asinh,
    .kind = DV_KIND_ASINH,
    .arity = DV_OP_UNARY,
    .name = "asinh",
    .apply_unary = dv_asinh,
    .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator,
    .fold_const_unary = NULL
};

const dval_ops_t ops_acosh = {
    .eval = eval_acosh,
    .deriv = deriv_acosh,
    .reverse = dv_reverse_acosh,
    .kind = DV_KIND_ACOSH,
    .arity = DV_OP_UNARY,
    .name = "acosh",
    .apply_unary = dv_acosh,
    .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator,
    .fold_const_unary = NULL
};

const dval_ops_t ops_atanh = {
    .eval = eval_atanh,
    .deriv = deriv_atanh,
    .reverse = dv_reverse_atanh,
    .kind = DV_KIND_ATANH,
    .arity = DV_OP_UNARY,
    .name = "atanh",
    .apply_unary = dv_atanh,
    .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator,
    .fold_const_unary = NULL
};

const dval_ops_t ops_exp = {
    .eval = eval_exp,
    .deriv = deriv_exp,
    .reverse = dv_reverse_exp,
    .kind = DV_KIND_EXP,
    .arity = DV_OP_UNARY,
    .name = "exp",
    .apply_unary = dv_exp,
    .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator,
    .fold_const_unary = dv_fold_exp_const
};

const dval_ops_t ops_log = {
    .eval = eval_log,
    .deriv = deriv_log,
    .reverse = dv_reverse_log,
    .kind = DV_KIND_LOG,
    .arity = DV_OP_UNARY,
    .name = "log",
    .apply_unary = dv_log,
    .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator,
    .fold_const_unary = dv_fold_log_const
};

const dval_ops_t ops_sqrt = {
    .eval = eval_sqrt,
    .deriv = deriv_sqrt,
    .reverse = dv_reverse_sqrt,
    .kind = DV_KIND_SQRT,
    .arity = DV_OP_UNARY,
    .name = "sqrt",
    .apply_unary = dv_sqrt,
    .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator,
    .fold_const_unary = dv_fold_sqrt_const
};

const dval_ops_t ops_abs = {
    .eval = eval_abs,
    .deriv = deriv_abs,
    .reverse = dv_reverse_abs,
    .kind = DV_KIND_ABS,
    .arity = DV_OP_UNARY,
    .name = "abs",
    .apply_unary = dv_abs,
    .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator,
    .fold_const_unary = NULL
};

const dval_ops_t ops_erf = {
    .eval = eval_erf,
    .deriv = deriv_erf,
    .reverse = dv_reverse_erf,
    .kind = DV_KIND_ERF,
    .arity = DV_OP_UNARY,
    .name = "erf",
    .apply_unary = dv_erf,
    .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator,
    .fold_const_unary = NULL
};

const dval_ops_t ops_erfc = {
    .eval = eval_erfc,
    .deriv = deriv_erfc,
    .reverse = dv_reverse_erfc,
    .kind = DV_KIND_ERFC,
    .arity = DV_OP_UNARY,
    .name = "erfc",
    .apply_unary = dv_erfc,
    .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator,
    .fold_const_unary = NULL
};

const dval_ops_t ops_lgamma = {
    .eval = eval_lgamma,
    .deriv = deriv_lgamma,
    .reverse = dv_reverse_lgamma,
    .kind = DV_KIND_LGAMMA,
    .arity = DV_OP_UNARY,
    .name = "lgamma",
    .apply_unary = dv_lgamma,
    .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator,
    .fold_const_unary = NULL
};

const dval_ops_t ops_hypot = {
    .eval = eval_hypot,
    .deriv = deriv_hypot,
    .reverse = dv_reverse_hypot,
    .kind = DV_KIND_HYPOT,
    .arity = DV_OP_BINARY,
    .name = "hypot",
    .apply_unary = NULL,
    .apply_binary = dv_hypot,
    .simplify = dv_simplify_hypot_operator,
    .fold_const_unary = NULL
};

const dval_ops_t ops_erfinv = {
    .eval = eval_erfinv, .deriv = deriv_erfinv, .reverse = dv_reverse_erfinv, .kind = DV_KIND_ERFINV, .arity = DV_OP_UNARY, .name = "erfinv",
    .apply_unary = dv_erfinv, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_erfcinv = {
    .eval = eval_erfcinv, .deriv = deriv_erfcinv, .reverse = dv_reverse_erfcinv, .kind = DV_KIND_ERFCINV, .arity = DV_OP_UNARY, .name = "erfcinv",
    .apply_unary = dv_erfcinv, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_gamma = {
    .eval = eval_gamma, .deriv = deriv_gamma, .reverse = dv_reverse_gamma, .kind = DV_KIND_GAMMA, .arity = DV_OP_UNARY, .name = "gamma",
    .apply_unary = dv_gamma, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_digamma = {
    .eval = eval_digamma, .deriv = deriv_digamma, .reverse = dv_reverse_digamma, .kind = DV_KIND_DIGAMMA, .arity = DV_OP_UNARY, .name = "digamma",
    .apply_unary = dv_digamma, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_trigamma = {
    .eval = eval_trigamma, .deriv = deriv_trigamma, .reverse = dv_reverse_trigamma, .kind = DV_KIND_TRIGAMMA, .arity = DV_OP_UNARY, .name = "trigamma",
    .apply_unary = dv_trigamma, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_lambert_w0 = {
    .eval = eval_lambert_w0, .deriv = deriv_lambert_w0, .reverse = dv_reverse_lambert_w0, .kind = DV_KIND_LAMBERT_W0, .arity = DV_OP_UNARY, .name = "lambert_w0",
    .apply_unary = dv_lambert_w0, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_lambert_wm1 = {
    .eval = eval_lambert_wm1, .deriv = deriv_lambert_wm1, .reverse = dv_reverse_lambert_wm1, .kind = DV_KIND_LAMBERT_WM1, .arity = DV_OP_UNARY, .name = "lambert_wm1",
    .apply_unary = dv_lambert_wm1, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_normal_pdf = {
    .eval = eval_normal_pdf, .deriv = deriv_normal_pdf, .reverse = dv_reverse_normal_pdf, .kind = DV_KIND_NORMAL_PDF, .arity = DV_OP_UNARY, .name = "normal_pdf",
    .apply_unary = dv_normal_pdf, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_normal_cdf = {
    .eval = eval_normal_cdf, .deriv = deriv_normal_cdf, .reverse = dv_reverse_normal_cdf, .kind = DV_KIND_NORMAL_CDF, .arity = DV_OP_UNARY, .name = "normal_cdf",
    .apply_unary = dv_normal_cdf, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_normal_logpdf = {
    .eval = eval_normal_logpdf, .deriv = deriv_normal_logpdf, .reverse = dv_reverse_normal_logpdf, .kind = DV_KIND_NORMAL_LOGPDF, .arity = DV_OP_UNARY, .name = "normal_logpdf",
    .apply_unary = dv_normal_logpdf, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_ei = {
    .eval = eval_ei, .deriv = deriv_ei, .reverse = dv_reverse_ei, .kind = DV_KIND_EI, .arity = DV_OP_UNARY, .name = "Ei",
    .apply_unary = dv_ei, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_e1 = {
    .eval = eval_e1, .deriv = deriv_e1, .reverse = dv_reverse_e1, .kind = DV_KIND_E1, .arity = DV_OP_UNARY, .name = "E1",
    .apply_unary = dv_e1, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_beta = {
    .eval = eval_beta, .deriv = deriv_beta, .reverse = dv_reverse_beta, .kind = DV_KIND_BETA, .arity = DV_OP_BINARY, .name = "beta",
    .apply_unary = NULL, .apply_binary = dv_beta,
    .simplify = dv_simplify_binary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_logbeta = {
    .eval = eval_logbeta, .deriv = deriv_logbeta, .reverse = dv_reverse_logbeta, .kind = DV_KIND_LOGBETA, .arity = DV_OP_BINARY, .name = "logbeta",
    .apply_unary = NULL, .apply_binary = dv_logbeta,
    .simplify = dv_simplify_binary_operator, .fold_const_unary = NULL
};

/* ------------------------------------------------------------------------- */
/* Core node constructors (no retaining here)                                */
/* ------------------------------------------------------------------------- */

static dval_t *dv_new_unary(const dval_ops_t *ops, dval_t *a)
{
    dval_t *dv = dv_alloc(ops);
    dv->a = a;
    return dv;
}

static dval_t *dv_new_binary(const dval_ops_t *ops, dval_t *a, dval_t *b)
{
    dval_t *dv = dv_alloc(ops);
    dv->a = a;
    dv->b = b;
    return dv;
}

static dval_t *dv_new_pow_d(dval_t *a, double d)
{
    dval_t *dv = dv_alloc(&ops_pow_d);
    dv->a = a;
    dv->c = qc_real_d(d);
    return dv;
}

static dval_t *dv_new_pow_qf(dval_t *a, qfloat_t exponent)
{
    dval_t *dv = dv_alloc(&ops_pow_d);
    dv->a = a;
    dv->c = qc_real_qf(exponent);
    return dv;
}

static dval_t *dv_new_pow_qc_internal(dval_t *a, qcomplex_t exponent)
{
    dval_t *dv = dv_alloc(&ops_pow_d);
    dv->a = a;
    dv->c = exponent;
    return dv;
}

/* ------------------------------------------------------------------------- */
/* Arithmetic constructors (retain children)                                 */
/* ------------------------------------------------------------------------- */

dval_t *dv_neg(dval_t *dv)
{
    if (!dv) return NULL;
    dv_retain(dv);
    return dv_new_unary(&ops_neg, dv);
}

dval_t *dv_add(dval_t *dv1, dval_t *dv2)
{
    if (!dv1 || !dv2) return NULL;
    dv_retain(dv1);
    dv_retain(dv2);
    return dv_new_binary(&ops_add, dv1, dv2);
}

dval_t *dv_sub(dval_t *dv1, dval_t *dv2)
{
    if (!dv1 || !dv2) return NULL;
    dv_retain(dv1);
    dv_retain(dv2);
    return dv_new_binary(&ops_sub, dv1, dv2);
}

dval_t *dv_mul(dval_t *dv1, dval_t *dv2)
{
    if (!dv1 || !dv2) return NULL;
    dv_retain(dv1);
    dv_retain(dv2);
    return dv_new_binary(&ops_mul, dv1, dv2);
}

dval_t *dv_div(dval_t *dv1, dval_t *dv2)
{
    if (!dv1 || !dv2) return NULL;
    dv_retain(dv1);
    dv_retain(dv2);
    return dv_new_binary(&ops_div, dv1, dv2);
}

/* ------------------------------------------------------------------------- */
/* Power / exp / log / sqrt (retain children)                                */
/* ------------------------------------------------------------------------- */

dval_t *dv_pow(dval_t *a, dval_t *b)
{
    if (!a || !b) return NULL;
    dv_retain(a);
    dv_retain(b);
    return dv_new_binary(&ops_pow, a, b);
}

dval_t *dv_pow_d(dval_t *a, double d)
{
    if (!a) return NULL;
    dv_retain(a);
    return dv_new_pow_d(a, d);
}

dval_t *dv_pow_qf(dval_t *a, qfloat_t exponent)
{
    if (!a)
        return NULL;
    dv_retain(a);
    return dv_new_pow_qf(a, exponent);
}

dval_t *dv_pow_qc(dval_t *a, qcomplex_t exponent)
{
    if (!a)
        return NULL;
    dv_retain(a);
    return dv_new_pow_qc_internal(a, exponent);
}

dval_t *dv_sqrt(dval_t *a)
{
    if (!a) return NULL;
    dv_retain(a);
    return dv_new_unary(&ops_sqrt, a);
}

dval_t *dv_exp(dval_t *a)
{
    if (!a) return NULL;
    dv_retain(a);
    return dv_new_unary(&ops_exp, a);
}

dval_t *dv_log(dval_t *a)
{
    if (!a) return NULL;
    dv_retain(a);
    return dv_new_unary(&ops_log, a);
}

/* ------------------------------------------------------------------------- */
/* Trigonometric (retain children)                                           */
/* ------------------------------------------------------------------------- */

dval_t *dv_sin(dval_t *a)   { dv_retain(a); return dv_new_unary(&ops_sin, a); }
dval_t *dv_cos(dval_t *a)   { dv_retain(a); return dv_new_unary(&ops_cos, a); }
dval_t *dv_tan(dval_t *a)   { dv_retain(a); return dv_new_unary(&ops_tan, a); }

/* ------------------------------------------------------------------------- */
/* Hyperbolic (retain children)                                              */
/* ------------------------------------------------------------------------- */

dval_t *dv_sinh(dval_t *a)  { dv_retain(a); return dv_new_unary(&ops_sinh, a); }
dval_t *dv_cosh(dval_t *a)  { dv_retain(a); return dv_new_unary(&ops_cosh, a); }
dval_t *dv_tanh(dval_t *a)  { dv_retain(a); return dv_new_unary(&ops_tanh, a); }

/* ------------------------------------------------------------------------- */
/* Inverse trigonometric (retain children)                                   */
/* ------------------------------------------------------------------------- */

dval_t *dv_asin(dval_t *a)  { dv_retain(a); return dv_new_unary(&ops_asin, a); }
dval_t *dv_acos(dval_t *a)  { dv_retain(a); return dv_new_unary(&ops_acos, a); }
dval_t *dv_atan(dval_t *a)  { dv_retain(a); return dv_new_unary(&ops_atan, a); }

dval_t *dv_atan2(dval_t *dv1, dval_t *dv2)
{
    if (!dv1 || !dv2) return NULL;
    dv_retain(dv1);
    dv_retain(dv2);
    return dv_new_binary(&ops_atan2, dv1, dv2);
}

/* ------------------------------------------------------------------------- */
/* Inverse hyperbolic (retain children)                                      */
/* ------------------------------------------------------------------------- */

dval_t *dv_asinh(dval_t *a) { dv_retain(a); return dv_new_unary(&ops_asinh, a); }
dval_t *dv_acosh(dval_t *a) { dv_retain(a); return dv_new_unary(&ops_acosh, a); }
dval_t *dv_atanh(dval_t *a) { dv_retain(a); return dv_new_unary(&ops_atanh, a); }

/* ------------------------------------------------------------------------- */
/* Special functions (retain children)                                       */
/* ------------------------------------------------------------------------- */

dval_t *dv_abs(dval_t *a)    { if (!a) return NULL; dv_retain(a); return dv_new_unary(&ops_abs,    a); }
dval_t *dv_erf(dval_t *a)    { if (!a) return NULL; dv_retain(a); return dv_new_unary(&ops_erf,    a); }
dval_t *dv_erfc(dval_t *a)   { if (!a) return NULL; dv_retain(a); return dv_new_unary(&ops_erfc,   a); }
dval_t *dv_lgamma(dval_t *a) { if (!a) return NULL; dv_retain(a); return dv_new_unary(&ops_lgamma, a); }

dval_t *dv_hypot(dval_t *a, dval_t *b)
{
    if (!a || !b) return NULL;
    dv_retain(a);
    dv_retain(b);
    return dv_new_binary(&ops_hypot, a, b);
}

dval_t *dv_erfinv(dval_t *a)     { if (!a) return NULL; dv_retain(a); return dv_new_unary(&ops_erfinv,        a); }
dval_t *dv_erfcinv(dval_t *a)    { if (!a) return NULL; dv_retain(a); return dv_new_unary(&ops_erfcinv,       a); }
dval_t *dv_gamma(dval_t *a)      { if (!a) return NULL; dv_retain(a); return dv_new_unary(&ops_gamma,         a); }
dval_t *dv_digamma(dval_t *a)    { if (!a) return NULL; dv_retain(a); return dv_new_unary(&ops_digamma,       a); }
dval_t *dv_trigamma(dval_t *a)   { if (!a) return NULL; dv_retain(a); return dv_new_unary(&ops_trigamma,      a); }
dval_t *dv_lambert_w0(dval_t *a) { if (!a) return NULL; dv_retain(a); return dv_new_unary(&ops_lambert_w0,   a); }
dval_t *dv_lambert_wm1(dval_t *a){ if (!a) return NULL; dv_retain(a); return dv_new_unary(&ops_lambert_wm1,  a); }
dval_t *dv_normal_pdf(dval_t *a) { if (!a) return NULL; dv_retain(a); return dv_new_unary(&ops_normal_pdf,   a); }
dval_t *dv_normal_cdf(dval_t *a) { if (!a) return NULL; dv_retain(a); return dv_new_unary(&ops_normal_cdf,   a); }
dval_t *dv_normal_logpdf(dval_t *a) { if (!a) return NULL; dv_retain(a); return dv_new_unary(&ops_normal_logpdf, a); }
dval_t *dv_ei(dval_t *a)         { if (!a) return NULL; dv_retain(a); return dv_new_unary(&ops_ei,           a); }
dval_t *dv_e1(dval_t *a)         { if (!a) return NULL; dv_retain(a); return dv_new_unary(&ops_e1,           a); }

dval_t *dv_beta(dval_t *a, dval_t *b)
{
    if (!a || !b) return NULL;
    dv_retain(a);
    dv_retain(b);
    return dv_new_binary(&ops_beta, a, b);
}

dval_t *dv_logbeta(dval_t *a, dval_t *b)
{
    if (!a || !b) return NULL;
    dv_retain(a);
    dv_retain(b);
    return dv_new_binary(&ops_logbeta, a, b);
}

/* ------------------------------------------------------------------------- */
/* Mixed double + dval helpers (Option B: constructors retain children)      */
/* ------------------------------------------------------------------------- */

dval_t *dv_add_d(dval_t *dv, double d)
{
    dval_t *c = dv_new_const_d(d);
    dval_t *r = dv_add(dv, c);   /* retains dv and c */
    dv_free(c);                  /* drop our temporary reference */
    return r;
}

dval_t *dv_sub_d(dval_t *dv, double d)
{
    dval_t *c = dv_new_const_d(d);
    dval_t *r = dv_sub(dv, c);
    dv_free(c);
    return r;
}

dval_t *dv_d_sub(double d, dval_t *dv)
{
    dval_t *c = dv_new_const_d(d);
    dval_t *r = dv_sub(c, dv);
    dv_free(c);
    return r;
}

dval_t *dv_mul_d(dval_t *dv, double d)
{
    dval_t *c = dv_new_const_d(d);
    dval_t *r = dv_mul(dv, c);
    dv_free(c);
    return r;
}

dval_t *dv_div_d(dval_t *dv, double d)
{
    dval_t *c = dv_new_const_d(d);
    dval_t *r = dv_div(dv, c);
    dv_free(c);
    return r;
}

dval_t *dv_d_div(double d, dval_t *dv)
{
    dval_t *c = dv_new_const_d(d);
    dval_t *r = dv_div(c, dv);
    dv_free(c);
    return r;
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

/* ------------------------------------------------------------------------- */
/* Debug / lifetime                                                          */
/* ------------------------------------------------------------------------- */

void dv_free(dval_t *dv)
{
    dv_release(dv);
}
