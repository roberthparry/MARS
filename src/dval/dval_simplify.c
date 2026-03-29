#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "qfloat.h"
#include "dval_internal.h"

/* ------------------------------------------------------------------------- */
/* Helpers                                                                   */
/* ------------------------------------------------------------------------- */

static int is_qf_zero(qfloat x)
{
    return qf_to_double(x) == 0.0;
}

static int is_qf_one(qfloat x)
{
    return qf_to_double(x) == 1.0;
}

/* ------------------------------------------------------------------------- */
/* Multiplication flattening                                                 */
/* ------------------------------------------------------------------------- */

static void collect_mul_flat(
    dval_t *f,
    qfloat *c_acc,
    int *is_zero,
    dval_t ***terms,
    size_t *nterms,
    size_t *cap)
{
    if (*is_zero) return;

    /* absorb constants */
    if (f->ops == &ops_const) {
        if (is_qf_zero(f->c)) {
            *is_zero = 1;
            return;
        }
        *c_acc = qf_mul(*c_acc, f->c);
        return;
    }

    /* absorb negation */
    if (f->ops == &ops_neg) {
        *c_acc = qf_neg(*c_acc);
        collect_mul_flat(f->a, c_acc, is_zero, terms, nterms, cap);
        return;
    }

    /* flatten multiplication */
    if (f->ops == &ops_mul) {
        collect_mul_flat(f->a, c_acc, is_zero, terms, nterms, cap);
        collect_mul_flat(f->b, c_acc, is_zero, terms, nterms, cap);
        return;
    }

    /* otherwise: append as a factor */
    if (*nterms == *cap) {
        *cap = (*cap == 0 ? 4 : *cap * 2);
        *terms = realloc(*terms, *cap * sizeof(dval_t *));
    }

    dv_retain(f);
    (*terms)[(*nterms)++] = f;
}

/* ------------------------------------------------------------------------- */
/* Unary function simplification helpers                                     */
/* ------------------------------------------------------------------------- */

static dval_t *simplify_unary_fun(dval_t *f, dval_t *a)
{
    /* constant folding */
    if (a->ops == &ops_const) {
        double x = qf_to_double(a->c);

        if (f->ops == &ops_sin  && x == 0.0) { dv_free(a); return dv_new_const_d(0.0); }
        if (f->ops == &ops_cos  && x == 0.0) { dv_free(a); return dv_new_const_d(1.0); }
        if (f->ops == &ops_tan  && x == 0.0) { dv_free(a); return dv_new_const_d(0.0); }
        if (f->ops == &ops_exp  && x == 0.0) { dv_free(a); return dv_new_const_d(1.0); }
        if (f->ops == &ops_log  && x == 1.0) { dv_free(a); return dv_new_const_d(0.0); }

        if (f->ops == &ops_sqrt) {
            if (x == 0.0) { dv_free(a); return dv_new_const_d(0.0); }
            if (x == 1.0) { dv_free(a); return dv_new_const_d(1.0); }
        }
    }

    /* NEW: generic unary dispatch */
    if (f->ops->apply_unary) {
        dval_t *out = f->ops->apply_unary(a);
        dv_free(a);
        return out;
    }

    /* fallback */
    dv_free(a);
    dv_retain(f);
    return f;
}

/* ------------------------------------------------------------------------- */
/* Structural equality for repeated-factor folding                           */
/* ------------------------------------------------------------------------- */
static int dv_struct_eq(const dval_t *u, const dval_t *v)
{
    if (u == v)
        return 1;

    if (u->ops != v->ops)
        return 0;

    if (u->ops == &ops_const)
        return qf_to_double(u->c) == qf_to_double(v->c);

    if (u->ops == &ops_var)
        return u == v; /* pointer identity */

    if (u->ops == &ops_neg)
        return dv_struct_eq(u->a, v->a);

    if (u->ops == &ops_pow_d)
        return dv_struct_eq(u->a, v->a) &&
               qf_to_double(u->c) == qf_to_double(v->c);

    /* binary ops */
    return dv_struct_eq(u->a, v->a) &&
           dv_struct_eq(u->b, v->b);
}

/* ------------------------------------------------------------------------- */
/* Simplifier                                                                */
/* ------------------------------------------------------------------------- */
dval_t *dv_simplify(dval_t *f)
{
    if (!f)
        return NULL;

    /* -------------------- ATOMS -------------------- */
    if (f->ops->arity == DV_OP_ATOM) {
        dv_retain(f);
        return f;
    }

    /* Recursively simplify children */
    dval_t *a = f->a ? dv_simplify(f->a) : NULL;
    dval_t *b = f->b ? dv_simplify(f->b) : NULL;

    /* ============================================================
       NEGATION
       ============================================================ */
    if (f->ops == &ops_neg) {

        /* --double negation-- */
        if (a->ops == &ops_neg) {
            dval_t *inner = a->a;
            dv_retain(inner);
            dv_free(a);
            return inner;
        }

        /* --neg(const)-- */
        if (a->ops == &ops_const) {
            qfloat c = qf_neg(a->c);
            dv_free(a);
            return dv_new_const(c);
        }

        /* default */
        return dv_neg(a);
    }

    /* ============================================================
       ADDITION
       ============================================================ */
    if (f->ops == &ops_add) {

        /* 0 + x → x */
        if (a->ops == &ops_const && is_qf_zero(a->c)) {
            dv_free(a);
            return b;
        }

        /* x + 0 → x */
        if (b->ops == &ops_const && is_qf_zero(b->c)) {
            dv_free(b);
            return a;
        }

        /* const + const */
        if (a->ops == &ops_const && b->ops == &ops_const) {
            qfloat c = qf_add(a->c, b->c);
            dv_free(a);
            dv_free(b);
            return dv_new_const(c);
        }

        return dv_add(a, b);
    }

    /* ============================================================
       SUBTRACTION
       ============================================================ */
    if (f->ops == &ops_sub) {

        /* x - 0 → x */
        if (b->ops == &ops_const && is_qf_zero(b->c)) {
            dv_free(b);
            return a;
        }

        /* const - const */
        if (a->ops == &ops_const && b->ops == &ops_const) {
            qfloat c = qf_sub(a->c, b->c);
            dv_free(a);
            dv_free(b);
            return dv_new_const(c);
        }

        return dv_sub(a, b);
    }

    /* ============================================================
       MULTIPLICATION (flatten + sign normalization + fold)
       ============================================================ */
    if (f->ops == &ops_mul) {

        /* zero rules */
        if ((a->ops == &ops_const && is_qf_zero(a->c)) ||
            (b->ops == &ops_const && is_qf_zero(b->c))) {
            dv_free(a);
            dv_free(b);
            return dv_new_const_d(0.0);
        }

        /* unit rules */
        if (a->ops == &ops_const && is_qf_one(a->c)) {
            dv_free(a);
            return b;
        }
        if (b->ops == &ops_const && is_qf_one(b->c)) {
            dv_free(b);
            return a;
        }

        /* --- flatten + sign normalization --- */
        qfloat c_acc = qf_from_double(1.0);
        int is_zero = 0;
        dval_t **terms = NULL;
        size_t nterms = 0, cap = 0;

        /* absorb sign from left */
        if (a->ops == &ops_neg) {
            c_acc = qf_neg(c_acc);
            collect_mul_flat(a->a, &c_acc, &is_zero, &terms, &nterms, &cap);
        } else {
            collect_mul_flat(a, &c_acc, &is_zero, &terms, &nterms, &cap);
        }

        /* absorb sign from right */
        if (b->ops == &ops_neg) {
            c_acc = qf_neg(c_acc);
            collect_mul_flat(b->a, &c_acc, &is_zero, &terms, &nterms, &cap);
        } else {
            collect_mul_flat(b, &c_acc, &is_zero, &terms, &nterms, &cap);
        }

        dv_free(a);
        dv_free(b);

        if (is_zero) {
            for (size_t i = 0; i < nterms; ++i)
                dv_free(terms[i]);
            free(terms);
            return dv_new_const_d(0.0);
        }

        /* --- fold repeated factors into powers --- */
        for (size_t i = 0; i < nterms; ++i) {
            if (!terms[i]) continue;

            int count = 1;

            for (size_t j = i + 1; j < nterms; ++j) {
                if (!terms[j]) continue;

                if (dv_struct_eq(terms[i], terms[j])) {
                    count++;
                    dv_free(terms[j]);
                    terms[j] = NULL;
                }
            }

            if (count > 1) {
                dval_t *base = terms[i];
                dval_t *pow  = dv_pow_d(base, (double)count);
                dv_free(base);
                terms[i] = pow;
            }
        }

        /* --- rebuild --- */
        dval_t *cur = NULL;

        if (!is_qf_one(c_acc))
            cur = dv_new_const(c_acc);

        for (size_t i = 0; i < nterms; ++i) {
            if (!terms[i]) continue;

            if (!cur) {
                cur = terms[i];
            } else {
                dval_t *tmp = dv_mul(cur, terms[i]);
                dv_free(cur);
                dv_free(terms[i]);
                cur = tmp;
            }
        }

        free(terms);

        if (!cur)
            return dv_new_const(c_acc);

        return cur;
    }

    /* ============================================================
       DIVISION
       ============================================================ */
    if (f->ops == &ops_div) {

        /* x / 1 → x */
        if (b->ops == &ops_const && is_qf_one(b->c)) {
            dv_free(b);
            return a;
        }

        /* 0 / x → 0 */
        if (a->ops == &ops_const && is_qf_zero(a->c)) {
            dv_free(a);
            dv_free(b);
            return dv_new_const_d(0.0);
        }

        /* const / const */
        if (a->ops == &ops_const && b->ops == &ops_const) {
            qfloat q = qf_div(a->c, b->c);
            dv_free(a);
            dv_free(b);
            return dv_new_const(q);
        }

        return dv_div(a, b);
    }

    /* ============================================================
       POW_D
       ============================================================ */
    if (f->ops == &ops_pow_d) {
        double ed = qf_to_double(f->c);

        if (ed == 1.0)
            return a;

        if (ed == 0.0) {
            dv_free(a);
            return dv_new_const_d(1.0);
        }

        return dv_pow_d(a, ed);
    }

    /* ============================================================
       BINARY POW
       ============================================================ */
    if (f->ops == &ops_pow) {

        if (b->ops == &ops_const && is_qf_one(b->c)) {
            dv_free(b);
            return a;
        }

        if (b->ops == &ops_const && is_qf_zero(b->c)) {
            dv_free(a);
            dv_free(b);
            return dv_new_const_d(1.0);
        }

        return dv_pow(a, b);
    }

    /* ============================================================
       UNARY FUNCTIONS
       ============================================================ */
    if (f->ops->arity == DV_OP_UNARY) {
        return simplify_unary_fun(f, a);
    }

    /* ============================================================
       FALLBACK
       ============================================================ */
    if (a) dv_free(a);
    if (b) dv_free(b);

    dv_retain(f);
    return f;
}
