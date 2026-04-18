/* dval_core.c - lazy, vtable-driven, reference-counted differentiable value DAG
 *
 * This file implements:
 *   - Node allocation and the dv_new_X / dv_create_X family of constructors
 *   - Reference counting (dv_retain / dv_free)
 *   - Name handling, including ASCII-to-Unicode normalisation for Greek letter
 *     names (e.g. "alpha" -> "alpha-as-unicode") so names are canonical in output
 *   - Lazy primal evaluation (dv_eval) via vtable dispatch (ops->eval)
 *   - Lazy derivative construction (dv_get_deriv / dv_create_deriv) via
 *     vtable dispatch (ops->deriv), with the result cached in dval_t::dx
 *   - All arithmetic and math operator constructors (dv_add, dv_sin, etc.)
 *   - dv_cmp, dv_print, and other accessors
 *
 * The operator implementations (eval/deriv bodies) live in the same file,
 * grouped by operator family after the core infrastructure.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>
#include <pthread.h>

#include "qfloat.h"
#include "dval_internal.h"
#include "dval.h"

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

static pthread_mutex_t refcount_lock = PTHREAD_MUTEX_INITIALIZER;

static inline void refcount_inc(int *rc)
{
    pthread_mutex_lock(&refcount_lock);
    (*rc)++;
    pthread_mutex_unlock(&refcount_lock);
}

static inline int refcount_dec(int *rc)
{
    pthread_mutex_lock(&refcount_lock);
    int prev = *rc;
    (*rc)--;
    pthread_mutex_unlock(&refcount_lock);
    return prev;
}

void dv_retain(dval_t *dv)
{
    if (dv) refcount_inc(&dv->refcount);
}

static void dv_release(dval_t *dv)
{
    if (!dv) return;

    if (refcount_dec(&dv->refcount) > 1)
        return;

    dval_t *a  = dv->a;
    dval_t *b  = dv->b;
    dval_t *dx = dv->dx;

    if (dv->name) free(dv->name);
    free(dv);

    dv_release(a);
    dv_release(b);
    dv_release(dx);
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
    dv->c        = qf_from_double(0.0);
    dv->x        = qf_from_double(0.0);
    dv->x_valid  = 0;
    dv->dx       = NULL;
    dv->dx_valid = 0;
    dv->name     = NULL;
    dv->refcount = 1;

    return dv;
}

/* ------------------------------------------------------------------------- */
/* Lazy eval / deriv                                                         */
/* ------------------------------------------------------------------------- */

static qfloat_t dv_eval_qf(const dval_t *dv)
{
    if (!dv)
        return qf_from_double(0.0);

    dval_t *mutable_dv = (dval_t *)dv;

    if (!mutable_dv->x_valid) {
        mutable_dv->x = mutable_dv->ops->eval(mutable_dv);
        mutable_dv->x_valid = 1;
    }
    return mutable_dv->x;
}

static dval_t *dv_build_dx(dval_t *dv)
{
    if (!dv)
        return NULL;

    if (!dv->dx_valid) {
        dv->dx = dv->ops->deriv(dv); /* refcount = 1 */
        dv->dx_valid = 1;
    }
    return dv->dx; /* borrowed */
}

/* Return an owning reference to the derivative of n.
 * Falls back to a zero constant when no derivative exists. */
static dval_t *get_dx(const dval_t *dv)
{
    const dval_t *d = dv_get_deriv(dv);
    if (d) {
        dv_retain((dval_t *)d);
        return (dval_t *)d;
    }
    return dv_new_const_d(0.0);
}

/* ------------------------------------------------------------------------- */
/* Public eval                                                               */
/* ------------------------------------------------------------------------- */

qfloat_t dv_eval(const dval_t *dv)
{
    return dv_eval_qf(dv);
}

double dv_eval_d(const dval_t *dv)
{
    return qf_to_double(dv_eval(dv));
}

/* ------------------------------------------------------------------------- */
/* Public derivative (borrowed)                                              */
/* ------------------------------------------------------------------------- */

const dval_t *dv_get_deriv(const dval_t *dv)
{
    if (!dv) return NULL;

    /* If already computed, return cached derivative */
    if (dv->dx_valid)
        return dv->dx;

    /* Cast away const to mutate internal cache */
    dval_t *mutable_dv = (dval_t *)dv;

    /* Compute derivative lazily */
    dval_t *d = mutable_dv->ops->deriv(mutable_dv);

    mutable_dv->dx = d;
    mutable_dv->dx_valid = 1;

    return d;
}

/* ------------------------------------------------------------------------- */
/* Constructors                                                              */
/* ------------------------------------------------------------------------- */

static dval_t *dv_make_const_qf(qfloat_t x)
{
    dval_t *dv = dv_alloc(&ops_const);
    dv->c = x;
    dv->x = x;
    dv->x_valid = 1;
    return dv;
}

/* A variable: value x, derivative dx = 1 (as a constant node) */
static dval_t *dv_make_var_qf(qfloat_t x)
{
    dval_t *dv = dv_alloc(&ops_var);
    dv->c = x;
    dv->x = x;
    dv->x_valid = 1;

    /* dx is a dval_t*; for a variable, derivative is 1 */
    dval_t *df = dv_make_const_qf(qf_from_double(1.0));
    dv->dx = df;
    dv->dx_valid = 1;

    return dv;
}

dval_t *dv_new_const_d(double x) { return dv_make_const_qf(qf_from_double(x)); }
dval_t *dv_new_const(qfloat_t x)   { return dv_make_const_qf(x); }

/* Public variable constructors: no derivative argument */
dval_t *dv_new_var_d(double x)
{
    return dv_make_var_qf(qf_from_double(x));
}

dval_t *dv_new_var(qfloat_t x)
{
    return dv_make_var_qf(x);
}

dval_t *dv_new_named_const(qfloat_t x, const char *name)
{
    dval_t *dv = dv_new_const(x);
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

dval_t *dv_new_named_var_d(double x, const char *name)
{
    return dv_new_named_var(qf_from_double(x), name);
}

/* ------------------------------------------------------------------------- */
/* Setters                                                                   */
/* ------------------------------------------------------------------------- */

void dv_set_val(dval_t *dv, qfloat_t v)
{
    if (dv->ops != &ops_var) abort();
    dv->c = v;
    dv->x = v;
    dv->x_valid = 1;
}

void dv_set_val_d(dval_t *dv, double v)
{
    dv_set_val(dv, qf_from_double(v));
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
    return qf_to_double(dv_eval_qf((dval_t *)dv));
}

qfloat_t dv_get_val(const dval_t *dv)
{
    return dv_eval_qf((dval_t *)dv);
}

/* ------------------------------------------------------------------------- */
/* EVALUATION FUNCTIONS                                                      */
/* ------------------------------------------------------------------------- */

static qfloat_t eval_const(dval_t *dv) {
    return dv->c;
}

static qfloat_t eval_var(dval_t *dv) {
    return dv->c;
}

static qfloat_t eval_add(dval_t *dv) {
    return qf_add(dv_eval_qf(dv->a), dv_eval_qf(dv->b));
}

static qfloat_t eval_sub(dval_t *dv) {
    return qf_sub(dv_eval_qf(dv->a), dv_eval_qf(dv->b));
}

static qfloat_t eval_mul(dval_t *dv) {
    return qf_mul(dv_eval_qf(dv->a), dv_eval_qf(dv->b));
}

static qfloat_t eval_div(dval_t *dv) {
    return qf_div(dv_eval_qf(dv->a), dv_eval_qf(dv->b));
}

static qfloat_t eval_neg(dval_t *dv) {
    return qf_neg(dv_eval_qf(dv->a));
}

static qfloat_t eval_pow(dval_t *dv) {
    qfloat_t base = dv_eval_qf(dv->a);
    qfloat_t exp  = dv_eval_qf(dv->b);
    return qf_pow(base, exp);
}

static qfloat_t eval_pow_d(dval_t *dv) {
    qfloat_t base = dv_eval_qf(dv->a);
    return qf_pow(base, dv->c);
}

/* --- Trig / Hyperbolic / Exp / Log / Sqrt -------------------------------- */

static qfloat_t eval_sin(dval_t *dv)   { return qf_sin (dv_eval_qf(dv->a)); }
static qfloat_t eval_cos(dval_t *dv)   { return qf_cos (dv_eval_qf(dv->a)); }
static qfloat_t eval_tan(dval_t *dv)   { return qf_tan (dv_eval_qf(dv->a)); }

static qfloat_t eval_sinh(dval_t *dv)  { return qf_sinh(dv_eval_qf(dv->a)); }
static qfloat_t eval_cosh(dval_t *dv)  { return qf_cosh(dv_eval_qf(dv->a)); }
static qfloat_t eval_tanh(dval_t *dv)  { return qf_tanh(dv_eval_qf(dv->a)); }

static qfloat_t eval_asin(dval_t *dv)  { return qf_asin(dv_eval_qf(dv->a)); }
static qfloat_t eval_acos(dval_t *dv)  { return qf_acos(dv_eval_qf(dv->a)); }
static qfloat_t eval_atan(dval_t *dv)  { return qf_atan(dv_eval_qf(dv->a)); }

static qfloat_t eval_asinh(dval_t *dv) { return qf_asinh(dv_eval_qf(dv->a)); }
static qfloat_t eval_acosh(dval_t *dv) { return qf_acosh(dv_eval_qf(dv->a)); }
static qfloat_t eval_atanh(dval_t *dv) { return qf_atanh(dv_eval_qf(dv->a)); }

static qfloat_t eval_exp(dval_t *dv)    { return qf_exp   (dv_eval_qf(dv->a)); }
static qfloat_t eval_log(dval_t *dv)    { return qf_log   (dv_eval_qf(dv->a)); }
static qfloat_t eval_sqrt(dval_t *dv)   { return qf_sqrt  (dv_eval_qf(dv->a)); }

static qfloat_t eval_abs(dval_t *dv)    { return qf_abs   (dv_eval_qf(dv->a)); }
static qfloat_t eval_erf(dval_t *dv)    { return qf_erf   (dv_eval_qf(dv->a)); }
static qfloat_t eval_erfc(dval_t *dv)   { return qf_erfc  (dv_eval_qf(dv->a)); }
static qfloat_t eval_lgamma(dval_t *dv) { return qf_lgamma(dv_eval_qf(dv->a)); }

static qfloat_t eval_hypot(dval_t *dv) {
    return qf_hypot(dv_eval_qf(dv->a), dv_eval_qf(dv->b));
}

static qfloat_t eval_erfinv(dval_t *dv)        { return qf_erfinv    (dv_eval_qf(dv->a)); }
static qfloat_t eval_erfcinv(dval_t *dv)       { return qf_erfcinv   (dv_eval_qf(dv->a)); }
static qfloat_t eval_gamma(dval_t *dv)         { return qf_gamma     (dv_eval_qf(dv->a)); }
static qfloat_t eval_digamma(dval_t *dv)        { return qf_digamma  (dv_eval_qf(dv->a)); }
static qfloat_t eval_trigamma(dval_t *dv)       { return qf_trigamma (dv_eval_qf(dv->a)); }
static qfloat_t eval_lambert_w0(dval_t *dv)    { return qf_lambert_w0 (dv_eval_qf(dv->a)); }
static qfloat_t eval_lambert_wm1(dval_t *dv)   { return qf_lambert_wm1(dv_eval_qf(dv->a)); }
static qfloat_t eval_normal_pdf(dval_t *dv)    { return qf_normal_pdf (dv_eval_qf(dv->a)); }
static qfloat_t eval_normal_cdf(dval_t *dv)    { return qf_normal_cdf (dv_eval_qf(dv->a)); }
static qfloat_t eval_normal_logpdf(dval_t *dv) { return qf_normal_logpdf(dv_eval_qf(dv->a)); }
static qfloat_t eval_ei(dval_t *dv)            { return qf_ei        (dv_eval_qf(dv->a)); }
static qfloat_t eval_e1(dval_t *dv)            { return qf_e1        (dv_eval_qf(dv->a)); }

static qfloat_t eval_beta(dval_t *dv) {
    return qf_beta(dv_eval_qf(dv->a), dv_eval_qf(dv->b));
}
static qfloat_t eval_logbeta(dval_t *dv) {
    return qf_logbeta(dv_eval_qf(dv->a), dv_eval_qf(dv->b));
}

static qfloat_t eval_atan2(dval_t *dv) {
    qfloat_t fy = dv_eval_qf(dv->a);
    qfloat_t gx = dv_eval_qf(dv->b);
    return qf_atan2(fy, gx);
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
    /* Variables are constructed with dx pre-seeded to const(1).
     * get_dx returns an owning copy of that cached derivative. */
    return get_dx(dv);
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
    double  c    = qf_to_double(dv->c);
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
/* Operator vtable instances                                                 */
/* ------------------------------------------------------------------------- */

/* -------------------- ATOMS -------------------- */

const dval_ops_t ops_const = {
    eval_const,
    deriv_const,
    DV_OP_ATOM,
    "const",
    NULL
};

const dval_ops_t ops_var = {
    eval_var,
    deriv_var,
    DV_OP_ATOM,
    "var",
    NULL
};

/* -------------------- BINARY OPS -------------------- */

const dval_ops_t ops_add = {
    eval_add,
    deriv_add,
    DV_OP_BINARY,
    "+",
    NULL
};

const dval_ops_t ops_sub = {
    eval_sub,
    deriv_sub,
    DV_OP_BINARY,
    "-",
    NULL
};

const dval_ops_t ops_mul = {
    eval_mul,
    deriv_mul,
    DV_OP_BINARY,
    "*",
    NULL
};

const dval_ops_t ops_div = {
    eval_div,
    deriv_div,
    DV_OP_BINARY,
    "/",
    NULL
};

const dval_ops_t ops_pow = {
    eval_pow,
    deriv_pow,
    DV_OP_BINARY,
    "^",
    NULL
};

const dval_ops_t ops_pow_d = {
    eval_pow_d,
    deriv_pow_d,
    DV_OP_BINARY,
    "^",
    NULL
};

const dval_ops_t ops_atan2 = {
    eval_atan2,
    deriv_atan2,
    DV_OP_BINARY,
    "atan2",
    NULL
};

/* -------------------- UNARY OPS -------------------- */

const dval_ops_t ops_neg = {
    eval_neg,
    deriv_neg,
    DV_OP_UNARY,
    "-",
    dv_neg
};

const dval_ops_t ops_sin = {
    eval_sin,
    deriv_sin,
    DV_OP_UNARY,
    "sin",
    dv_sin
};

const dval_ops_t ops_cos = {
    eval_cos,
    deriv_cos,
    DV_OP_UNARY,
    "cos",
    dv_cos
};

const dval_ops_t ops_tan = {
    eval_tan,
    deriv_tan,
    DV_OP_UNARY,
    "tan",
    dv_tan
};

const dval_ops_t ops_sinh = {
    eval_sinh,
    deriv_sinh,
    DV_OP_UNARY,
    "sinh",
    dv_sinh
};

const dval_ops_t ops_cosh = {
    eval_cosh,
    deriv_cosh,
    DV_OP_UNARY,
    "cosh",
    dv_cosh
};

const dval_ops_t ops_tanh = {
    eval_tanh,
    deriv_tanh,
    DV_OP_UNARY,
    "tanh",
    dv_tanh
};

const dval_ops_t ops_asin = {
    eval_asin,
    deriv_asin,
    DV_OP_UNARY,
    "asin",
    dv_asin
};

const dval_ops_t ops_acos = {
    eval_acos,
    deriv_acos,
    DV_OP_UNARY,
    "acos",
    dv_acos
};

const dval_ops_t ops_atan = {
    eval_atan,
    deriv_atan,
    DV_OP_UNARY,
    "atan",
    dv_atan
};

const dval_ops_t ops_asinh = {
    eval_asinh,
    deriv_asinh,
    DV_OP_UNARY,
    "asinh",
    dv_asinh
};

const dval_ops_t ops_acosh = {
    eval_acosh,
    deriv_acosh,
    DV_OP_UNARY,
    "acosh",
    dv_acosh
};

const dval_ops_t ops_atanh = {
    eval_atanh,
    deriv_atanh,
    DV_OP_UNARY,
    "atanh",
    dv_atanh
};

const dval_ops_t ops_exp = {
    eval_exp,
    deriv_exp,
    DV_OP_UNARY,
    "exp",
    dv_exp
};

const dval_ops_t ops_log = {
    eval_log,
    deriv_log,
    DV_OP_UNARY,
    "log",
    dv_log
};

const dval_ops_t ops_sqrt = {
    eval_sqrt,
    deriv_sqrt,
    DV_OP_UNARY,
    "sqrt",
    dv_sqrt
};

const dval_ops_t ops_abs = {
    eval_abs,
    deriv_abs,
    DV_OP_UNARY,
    "abs",
    dv_abs
};

const dval_ops_t ops_erf = {
    eval_erf,
    deriv_erf,
    DV_OP_UNARY,
    "erf",
    dv_erf
};

const dval_ops_t ops_erfc = {
    eval_erfc,
    deriv_erfc,
    DV_OP_UNARY,
    "erfc",
    dv_erfc
};

const dval_ops_t ops_lgamma = {
    eval_lgamma,
    deriv_lgamma,
    DV_OP_UNARY,
    "lgamma",
    dv_lgamma
};

const dval_ops_t ops_hypot = {
    eval_hypot,
    deriv_hypot,
    DV_OP_BINARY,
    "hypot",
    NULL
};

const dval_ops_t ops_erfinv = {
    eval_erfinv,    deriv_erfinv,    DV_OP_UNARY, "erfinv",         dv_erfinv
};
const dval_ops_t ops_erfcinv = {
    eval_erfcinv,   deriv_erfcinv,   DV_OP_UNARY, "erfcinv",        dv_erfcinv
};
const dval_ops_t ops_gamma = {
    eval_gamma,     deriv_gamma,     DV_OP_UNARY, "gamma",          dv_gamma
};
const dval_ops_t ops_digamma = {
    eval_digamma,   deriv_digamma,   DV_OP_UNARY, "digamma",        dv_digamma
};
const dval_ops_t ops_trigamma = {
    eval_trigamma,  deriv_trigamma,  DV_OP_UNARY, "trigamma",       dv_trigamma
};
const dval_ops_t ops_lambert_w0 = {
    eval_lambert_w0,  deriv_lambert_w0,  DV_OP_UNARY, "lambert_w0",  dv_lambert_w0
};
const dval_ops_t ops_lambert_wm1 = {
    eval_lambert_wm1, deriv_lambert_wm1, DV_OP_UNARY, "lambert_wm1", dv_lambert_wm1
};
const dval_ops_t ops_normal_pdf = {
    eval_normal_pdf,    deriv_normal_pdf,    DV_OP_UNARY, "normal_pdf",    dv_normal_pdf
};
const dval_ops_t ops_normal_cdf = {
    eval_normal_cdf,    deriv_normal_cdf,    DV_OP_UNARY, "normal_cdf",    dv_normal_cdf
};
const dval_ops_t ops_normal_logpdf = {
    eval_normal_logpdf, deriv_normal_logpdf, DV_OP_UNARY, "normal_logpdf", dv_normal_logpdf
};
const dval_ops_t ops_ei = {
    eval_ei, deriv_ei, DV_OP_UNARY, "Ei", dv_ei
};
const dval_ops_t ops_e1 = {
    eval_e1, deriv_e1, DV_OP_UNARY, "E1", dv_e1
};
const dval_ops_t ops_beta = {
    eval_beta,    deriv_beta,    DV_OP_BINARY, "beta",    NULL
};
const dval_ops_t ops_logbeta = {
    eval_logbeta, deriv_logbeta, DV_OP_BINARY, "logbeta", NULL
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
    dv->c = qf_from_double(d);
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
    double a = dv_eval_d(dv1);
    double b = dv_eval_d(dv2);
    if (a < b) return -1;
    if (a > b) return +1;
    return 0;
}


/* ------------------------------------------------------------------------- */
/* Derivative creation (owning)                                              */
/* ------------------------------------------------------------------------- */

dval_t *dv_create_deriv(dval_t *dv)
{
    if (!dv) return NULL;

    dval_t *raw = dv_build_dx(dv); /* borrowed */
    if (!raw) return NULL;

    dv_retain(raw);                /* now owning */
    dval_t *simp = dv_simplify(raw);
    dv_free(raw);

    return simp;
}

dval_t *dv_create_2nd_deriv(dval_t *dv)
{
    dval_t *g = dv_create_deriv(dv);
    if (!g) return NULL;
    dval_t *h = dv_create_deriv(g);
    dv_free(g);
    return h;
}

dval_t *dv_create_3rd_deriv(dval_t *dv)
{
    dval_t *g = dv_create_deriv(dv);
    if (!g) return NULL;
    dval_t *h = dv_create_deriv(g);
    dv_free(g);
    if (!h) return NULL;
    dval_t *k = dv_create_deriv(h);
    dv_free(h);
    return k;
}

dval_t *dv_create_nth_deriv(unsigned int n, dval_t *dv)
{
    dval_t *cur = dv;
    while (n--) {
        dval_t *next = dv_create_deriv(cur);
        if (cur != dv) dv_free(cur);
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
