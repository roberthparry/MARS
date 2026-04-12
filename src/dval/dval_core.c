/* dval_core.c - lazy, vtable-driven, reference-counted differentiable value DAG */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>

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

void dv_retain(dval_t *f)
{
    if (f) f->refcount++;
}

static void dv_release(dval_t *f)
{
    if (!f) return;

    if (--f->refcount > 0)
        return;

    dval_t *a  = f->a;
    dval_t *b  = f->b;
    dval_t *dx = f->dx;

    if (f->name) free(f->name);
    free(f);

    dv_release(a);
    dv_release(b);
    dv_release(dx);
}

/* ------------------------------------------------------------------------- */
/* Node allocation                                                           */
/* ------------------------------------------------------------------------- */

static dval_t *dv_alloc(const dval_ops_t *ops)
{
    dval_t *f = malloc(sizeof *f);
    if (!f) abort();

    f->ops      = ops;
    f->a        = NULL;
    f->b        = NULL;
    f->c        = qf_from_double(0.0);
    f->x        = qf_from_double(0.0);
    f->x_valid  = 0;
    f->dx       = NULL;
    f->dx_valid = 0;
    f->name     = NULL;
    f->refcount = 1;

    return f;
}

/* ------------------------------------------------------------------------- */
/* Lazy eval / deriv                                                         */
/* ------------------------------------------------------------------------- */

static qfloat dv_eval_qf(const dval_t *f)
{
    if (!f)
        return qf_from_double(0.0);

    dval_t *mutable_f = (dval_t *)f;

    if (!mutable_f->x_valid) {
        mutable_f->x = mutable_f->ops->eval(mutable_f);
        mutable_f->x_valid = 1;
    }
    return mutable_f->x;
}

static dval_t *dv_build_dx(dval_t *f)
{
    if (!f)
        return NULL;

    if (!f->dx_valid) {
        f->dx = f->ops->deriv(f); /* refcount = 1 */
        f->dx_valid = 1;
    }
    return f->dx; /* borrowed */
}

/* Return an owning reference to the derivative of n.
 * Falls back to a zero constant when no derivative exists. */
static dval_t *get_dx(const dval_t *n)
{
    const dval_t *d = dv_get_deriv(n);
    if (d) {
        dv_retain((dval_t *)d);
        return (dval_t *)d;
    }
    return dv_new_const_d(0.0);
}

/* ------------------------------------------------------------------------- */
/* Public eval                                                               */
/* ------------------------------------------------------------------------- */

qfloat dv_eval(const dval_t *f)
{
    return dv_eval_qf(f);
}

double dv_eval_d(const dval_t *f)
{
    return qf_to_double(dv_eval(f));
}

/* ------------------------------------------------------------------------- */
/* Public derivative (borrowed)                                              */
/* ------------------------------------------------------------------------- */

const dval_t *dv_get_deriv(const dval_t *f)
{
    if (!f) return NULL;

    /* If already computed, return cached derivative */
    if (f->dx_valid)
        return f->dx;

    /* Cast away const to mutate internal cache */
    dval_t *mutable_f = (dval_t *)f;

    /* Compute derivative lazily */
    dval_t *d = mutable_f->ops->deriv(mutable_f);

    mutable_f->dx = d;
    mutable_f->dx_valid = 1;

    return d;
}

/* ------------------------------------------------------------------------- */
/* Constructors                                                              */
/* ------------------------------------------------------------------------- */

static dval_t *dv_make_const_qf(qfloat x)
{
    dval_t *f = dv_alloc(&ops_const);
    f->c = x;
    f->x = x;
    f->x_valid = 1;
    return f;
}

/* A variable: value x, derivative dx = 1 (as a constant node) */
static dval_t *dv_make_var_qf(qfloat x)
{
    dval_t *f = dv_alloc(&ops_var);
    f->c = x;
    f->x = x;
    f->x_valid = 1;

    /* dx is a dval_t*; for a variable, derivative is 1 */
    dval_t *df = dv_make_const_qf(qf_from_double(1.0));
    f->dx = df;
    f->dx_valid = 1;

    return f;
}

dval_t *dv_new_const_d(double x) { return dv_make_const_qf(qf_from_double(x)); }
dval_t *dv_new_const(qfloat x)   { return dv_make_const_qf(x); }

/* Public variable constructors: no derivative argument */
dval_t *dv_new_var_d(double x)
{
    return dv_make_var_qf(qf_from_double(x));
}

dval_t *dv_new_var(qfloat x)
{
    return dv_make_var_qf(x);
}

dval_t *dv_new_named_const(qfloat x, const char *name)
{
    dval_t *f = dv_new_const(x);
    f->name = dv_normalize_name(name);
    return f;
}

dval_t *dv_new_named_const_d(double x, const char *name)
{
    return dv_new_named_const(qf_from_double(x), name);
}

/* Named variable: value x, derivative 1, with a name */
dval_t *dv_new_named_var(qfloat x, const char *name)
{
    dval_t *f = dv_new_var(x);
    f->name = dv_normalize_name(name);
    return f;
}

dval_t *dv_new_named_var_d(double x, const char *name)
{
    return dv_new_named_var(qf_from_double(x), name);
}

/* ------------------------------------------------------------------------- */
/* Setters                                                                   */
/* ------------------------------------------------------------------------- */

void dv_set_val(dval_t *x, qfloat v)
{
    if (x->ops != &ops_var) abort();
    x->c = v;
    x->x = v;
    x->x_valid = 1;
}

void dv_set_val_d(dval_t *x, double v)
{
    dv_set_val(x, qf_from_double(v));
}

void dv_set_name(dval_t *x, const char *name)
{
    if (!x) return;
    if (x->name) free(x->name);
    x->name = dv_normalize_name(name);
}

/* ------------------------------------------------------------------------- */
/* Accessors                                                                 */
/* ------------------------------------------------------------------------- */

double dv_get_val_d(const dval_t *f)
{
    return qf_to_double(dv_eval_qf((dval_t *)f));
}

qfloat dv_get_val(const dval_t *f)
{
    return dv_eval_qf((dval_t *)f);
}

/* ------------------------------------------------------------------------- */
/* EVALUATION FUNCTIONS                                                      */
/* ------------------------------------------------------------------------- */

static qfloat eval_const(dval_t *n) {
    return n->c;
}

static qfloat eval_var(dval_t *n) {
    return n->c;
}

static qfloat eval_add(dval_t *n) {
    return qf_add(dv_eval_qf(n->a), dv_eval_qf(n->b));
}

static qfloat eval_sub(dval_t *n) {
    return qf_sub(dv_eval_qf(n->a), dv_eval_qf(n->b));
}

static qfloat eval_mul(dval_t *n) {
    return qf_mul(dv_eval_qf(n->a), dv_eval_qf(n->b));
}

static qfloat eval_div(dval_t *n) {
    return qf_div(dv_eval_qf(n->a), dv_eval_qf(n->b));
}

static qfloat eval_neg(dval_t *n) {
    return qf_neg(dv_eval_qf(n->a));
}

static qfloat eval_pow(dval_t *n) {
    qfloat base = dv_eval_qf(n->a);
    qfloat exp  = dv_eval_qf(n->b);
    return qf_pow(base, exp);
}

static qfloat eval_pow_d(dval_t *n) {
    qfloat base = dv_eval_qf(n->a);
    return qf_pow(base, n->c);
}

/* --- Trig / Hyperbolic / Exp / Log / Sqrt -------------------------------- */

static qfloat eval_sin(dval_t *n)   { return qf_sin (dv_eval_qf(n->a)); }
static qfloat eval_cos(dval_t *n)   { return qf_cos (dv_eval_qf(n->a)); }
static qfloat eval_tan(dval_t *n)   { return qf_tan (dv_eval_qf(n->a)); }

static qfloat eval_sinh(dval_t *n)  { return qf_sinh(dv_eval_qf(n->a)); }
static qfloat eval_cosh(dval_t *n)  { return qf_cosh(dv_eval_qf(n->a)); }
static qfloat eval_tanh(dval_t *n)  { return qf_tanh(dv_eval_qf(n->a)); }

static qfloat eval_asin(dval_t *n)  { return qf_asin(dv_eval_qf(n->a)); }
static qfloat eval_acos(dval_t *n)  { return qf_acos(dv_eval_qf(n->a)); }
static qfloat eval_atan(dval_t *n)  { return qf_atan(dv_eval_qf(n->a)); }

static qfloat eval_asinh(dval_t *n) { return qf_asinh(dv_eval_qf(n->a)); }
static qfloat eval_acosh(dval_t *n) { return qf_acosh(dv_eval_qf(n->a)); }
static qfloat eval_atanh(dval_t *n) { return qf_atanh(dv_eval_qf(n->a)); }

static qfloat eval_exp(dval_t *n)   { return qf_exp (dv_eval_qf(n->a)); }
static qfloat eval_log(dval_t *n)   { return qf_log (dv_eval_qf(n->a)); }
static qfloat eval_sqrt(dval_t *n)  { return qf_sqrt(dv_eval_qf(n->a)); }

static qfloat eval_atan2(dval_t *n) {
    qfloat fy = dv_eval_qf(n->a);
    qfloat gx = dv_eval_qf(n->b);
    return qf_atan2(fy, gx);
}

/* ------------------------------------------------------------------------- */
/* DERIVATIVE FUNCTIONS — lazy, stored in each node                          */
/* ------------------------------------------------------------------------- */

static dval_t *deriv_const(dval_t *n)
{
    (void)n;
    return dv_new_const_d(0.0);
}

static dval_t *deriv_var(dval_t *n)
{
    /* Variables are constructed with dx pre-seeded to const(1).
     * get_dx returns an owning copy of that cached derivative. */
    return get_dx(n);
}

/* a + b */
static dval_t *deriv_add(dval_t *n)
{
    dval_t *da  = get_dx(n->a);
    dval_t *db  = get_dx(n->b);
    dval_t *out = dv_add(da, db);
    dv_free(da);
    dv_free(db);
    return out;
}

/* a - b */
static dval_t *deriv_sub(dval_t *n)
{
    dval_t *da  = get_dx(n->a);
    dval_t *db  = get_dx(n->b);
    dval_t *out = dv_sub(da, db);
    dv_free(da);
    dv_free(db);
    return out;
}

/* a * b */
static dval_t *deriv_mul(dval_t *n)
{
    dval_t *da  = get_dx(n->a);
    dval_t *db  = get_dx(n->b);
    dval_t *t1  = dv_mul(da, n->b);
    dval_t *t2  = dv_mul(n->a, db);
    dval_t *out = dv_add(t1, t2);
    dv_free(da);
    dv_free(db);
    dv_free(t1);
    dv_free(t2);
    return out;
}

/* a / b */
static dval_t *deriv_div(dval_t *n)
{
    dval_t *da   = get_dx(n->a);
    dval_t *db   = get_dx(n->b);
    dval_t *num1 = dv_mul(da, n->b);
    dval_t *num2 = dv_mul(n->a, db);
    dval_t *num  = dv_sub(num1, num2);
    dval_t *den  = dv_pow_d(n->b, 2.0);
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
static dval_t *deriv_neg(dval_t *n)
{
    dval_t *da  = get_dx(n->a);
    dval_t *out = dv_neg(da);
    dv_free(da);
    return out;
}

/* a^b — d/dx = a^b * (b' * log(a) + b * a'/a) */
static dval_t *deriv_pow(dval_t *n)
{
    dval_t *a  = n->a;
    dval_t *b  = n->b;
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
static dval_t *deriv_pow_d(dval_t *n)
{
    double  c    = qf_to_double(n->c);
    dval_t *da   = get_dx(n->a);
    dval_t *p    = dv_pow_d(n->a, c - 1.0);
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
static dval_t *deriv_sin(dval_t *n)
{
    dval_t *da  = get_dx(n->a);
    dval_t *ca  = dv_cos(n->a);
    dval_t *out = dv_mul(ca, da);
    dv_free(ca);
    dv_free(da);
    return out;
}

/* cos(a) — d/dx = -sin(a) * a' */
static dval_t *deriv_cos(dval_t *n)
{
    dval_t *da      = get_dx(n->a);
    dval_t *sin_a   = dv_sin(n->a);
    dval_t *neg_sin = dv_neg(sin_a);
    dv_free(sin_a);
    dval_t *out     = dv_mul(neg_sin, da);
    dv_free(da);
    dv_free(neg_sin);
    return out;
}

/* tan(a) — d/dx = (1 + tan²(a)) * a' */
static dval_t *deriv_tan(dval_t *n)
{
    dval_t *da  = get_dx(n->a);
    dval_t *t   = dv_tan(n->a);
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
static dval_t *deriv_sinh(dval_t *n)
{
    dval_t *da   = get_dx(n->a);
    dval_t *ca   = dv_cosh(n->a);
    dval_t *out  = dv_mul(ca, da);
    dv_free(ca);
    dv_free(da);
    return out;
}

/* cosh(a) — d/dx = sinh(a) * a' */
static dval_t *deriv_cosh(dval_t *n)
{
    dval_t *da   = get_dx(n->a);
    dval_t *sa   = dv_sinh(n->a);
    dval_t *out  = dv_mul(sa, da);
    dv_free(sa);
    dv_free(da);
    return out;
}

/* tanh(a) — d/dx = (1 - tanh²(a)) * a' */
static dval_t *deriv_tanh(dval_t *n)
{
    dval_t *da  = get_dx(n->a);
    dval_t *t   = dv_tanh(n->a);
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
static dval_t *deriv_exp(dval_t *n)
{
    dval_t *da   = get_dx(n->a);
    dval_t *ea   = dv_exp(n->a);
    dval_t *out  = dv_mul(ea, da);
    dv_free(ea);
    dv_free(da);
    return out;
}

/* log(a) — d/dx = a' / a */
static dval_t *deriv_log(dval_t *n)
{
    dval_t *da  = get_dx(n->a);
    dval_t *out = dv_div(da, n->a);
    dv_free(da);
    return out;
}

/* sqrt(a) — d/dx = a' / (2 * sqrt(a)) */
static dval_t *deriv_sqrt(dval_t *n)
{
    dval_t *da   = get_dx(n->a);
    dval_t *two  = dv_new_const_d(2.0);
    dval_t *sqra = dv_sqrt(n->a);
    dval_t *den  = dv_mul(two, sqra);
    dv_free(sqra);
    dval_t *out  = dv_div(da, den);
    dv_free(da);
    dv_free(two);
    dv_free(den);
    return out;
}

/* asin(a) — d/dx = a' / sqrt(1 - a²) */
static dval_t *deriv_asin(dval_t *n)
{
    dval_t *da   = get_dx(n->a);
    dval_t *a2   = dv_pow_d(n->a, 2.0);
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
static dval_t *deriv_acos(dval_t *n)
{
    dval_t *da   = get_dx(n->a);
    dval_t *a2   = dv_pow_d(n->a, 2.0);
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
static dval_t *deriv_atan(dval_t *n)
{
    dval_t *da  = get_dx(n->a);
    dval_t *a2  = dv_pow_d(n->a, 2.0);
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
static dval_t *deriv_atan2(dval_t *self)
{
    dval_t *y  = self->a;  /* first argument */
    dval_t *x  = self->b;  /* second argument */
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
static dval_t *deriv_asinh(dval_t *n)
{
    dval_t *da   = get_dx(n->a);
    dval_t *a2   = dv_pow_d(n->a, 2.0);
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
static dval_t *deriv_acosh(dval_t *n)
{
    dval_t *da  = get_dx(n->a);
    dval_t *one = dv_new_const_d(1.0);
    dval_t *am1 = dv_sub(n->a, one);
    dval_t *ap1 = dv_add(n->a, one);
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
static dval_t *deriv_atanh(dval_t *n)
{
    dval_t *da  = get_dx(n->a);
    dval_t *a2  = dv_pow_d(n->a, 2.0);
    dval_t *one = dv_new_const_d(1.0);
    dval_t *den = dv_sub(one, a2);
    dval_t *out = dv_div(da, den);
    dv_free(da);
    dv_free(a2);
    dv_free(one);
    dv_free(den);
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

/* ------------------------------------------------------------------------- */
/* Core node constructors (no retaining here)                                */
/* ------------------------------------------------------------------------- */

static dval_t *dv_new_unary(const dval_ops_t *ops, dval_t *a)
{
    dval_t *n = dv_alloc(ops);
    n->a = a;
    return n;
}

static dval_t *dv_new_binary(const dval_ops_t *ops, dval_t *a, dval_t *b)
{
    dval_t *n = dv_alloc(ops);
    n->a = a;
    n->b = b;
    return n;
}

static dval_t *dv_new_pow_d(dval_t *a, double d)
{
    dval_t *n = dv_alloc(&ops_pow_d);
    n->a = a;
    n->c = qf_from_double(d);
    return n;
}

/* ------------------------------------------------------------------------- */
/* Arithmetic constructors (retain children)                                 */
/* ------------------------------------------------------------------------- */

dval_t *dv_neg(dval_t *f)
{
    if (!f) return NULL;
    dv_retain(f);
    return dv_new_unary(&ops_neg, f);
}

dval_t *dv_add(dval_t *f, dval_t *g)
{
    if (!f || !g) return NULL;
    dv_retain(f);
    dv_retain(g);
    return dv_new_binary(&ops_add, f, g);
}

dval_t *dv_sub(dval_t *f, dval_t *g)
{
    if (!f || !g) return NULL;
    dv_retain(f);
    dv_retain(g);
    return dv_new_binary(&ops_sub, f, g);
}

dval_t *dv_mul(dval_t *f, dval_t *g)
{
    if (!f || !g) return NULL;
    dv_retain(f);
    dv_retain(g);
    return dv_new_binary(&ops_mul, f, g);
}

dval_t *dv_div(dval_t *f, dval_t *g)
{
    if (!f || !g) return NULL;
    dv_retain(f);
    dv_retain(g);
    return dv_new_binary(&ops_div, f, g);
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

dval_t *dv_atan2(dval_t *y, dval_t *x)
{
    if (!y || !x) return NULL;
    dv_retain(y);
    dv_retain(x);
    return dv_new_binary(&ops_atan2, y, x);
}

/* ------------------------------------------------------------------------- */
/* Inverse hyperbolic (retain children)                                      */
/* ------------------------------------------------------------------------- */

dval_t *dv_asinh(dval_t *a) { dv_retain(a); return dv_new_unary(&ops_asinh, a); }
dval_t *dv_acosh(dval_t *a) { dv_retain(a); return dv_new_unary(&ops_acosh, a); }
dval_t *dv_atanh(dval_t *a) { dv_retain(a); return dv_new_unary(&ops_atanh, a); }

/* ------------------------------------------------------------------------- */
/* Mixed double + dval helpers (Option B: constructors retain children)      */
/* ------------------------------------------------------------------------- */

dval_t *dv_add_d(dval_t *f, double d)
{
    dval_t *c = dv_new_const_d(d);
    dval_t *r = dv_add(f, c);   /* retains f and c */
    dv_free(c);                 /* drop our temporary reference */
    return r;
}

dval_t *dv_sub_d(dval_t *f, double d)
{
    dval_t *c = dv_new_const_d(d);
    dval_t *r = dv_sub(f, c);
    dv_free(c);
    return r;
}

dval_t *dv_d_sub(double d, dval_t *f)
{
    dval_t *c = dv_new_const_d(d);
    dval_t *r = dv_sub(c, f);
    dv_free(c);
    return r;
}

dval_t *dv_mul_d(dval_t *f, double d)
{
    dval_t *c = dv_new_const_d(d);
    dval_t *r = dv_mul(f, c);
    dv_free(c);
    return r;
}

dval_t *dv_div_d(dval_t *f, double d)
{
    dval_t *c = dv_new_const_d(d);
    dval_t *r = dv_div(f, c);
    dv_free(c);
    return r;
}

dval_t *dv_d_div(double d, dval_t *f)
{
    dval_t *c = dv_new_const_d(d);
    dval_t *r = dv_div(c, f);
    dv_free(c);
    return r;
}

int dv_cmp(const dval_t *f, const dval_t *g) {
    double a = dv_eval_d(f);
    double b = dv_eval_d(g);
    if (a < b) return -1;
    if (a > b) return +1;
    return 0;
}

int dv_compare(const dval_t *f, const dval_t *g) {
    return dv_cmp(f, g);
}

/* ------------------------------------------------------------------------- */
/* Derivative creation (owning)                                              */
/* ------------------------------------------------------------------------- */

dval_t *dv_create_deriv(dval_t *f)
{
    if (!f) return NULL;

    dval_t *raw = dv_build_dx(f); /* borrowed */
    if (!raw) return NULL;

    dv_retain(raw);               /* now owning */
    dval_t *simp = dv_simplify(raw);
    dv_free(raw);

    return simp;
}

dval_t *dv_create_2nd_deriv(dval_t *f)
{
    dval_t *g = dv_create_deriv(f);
    if (!g) return NULL;
    dval_t *h = dv_create_deriv(g);
    dv_free(g);
    return h;
}

dval_t *dv_create_3rd_deriv(dval_t *f)
{
    dval_t *g = dv_create_deriv(f);
    if (!g) return NULL;
    dval_t *h = dv_create_deriv(g);
    dv_free(g);
    if (!h) return NULL;
    dval_t *k = dv_create_deriv(h);
    dv_free(h);
    return k;
}

dval_t *dv_create_nth_deriv(unsigned int n, dval_t *f)
{
    dval_t *cur = f;
    while (n--) {
        dval_t *next = dv_create_deriv(cur);
        if (cur != f) dv_free(cur);
        cur = next;
        if (!cur) break;
    }
    return cur;
}

/* ------------------------------------------------------------------------- */
/* Debug / lifetime                                                          */
/* ------------------------------------------------------------------------- */

void dv_free(dval_t *f)
{
    dv_release(f);
}
