#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

    /* absorb unnamed constants into the numeric accumulator;
     * named constants (e.g. π) stay as symbolic terms */
    if (f->ops == &ops_const && (!f->name || !*f->name)) {
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
/* Like-term helpers for addition/subtraction                                */
/* ------------------------------------------------------------------------- */

/* Extract a scalar coefficient from a simplified additive term.
 * Sets *base to the non-coefficient factor (the "shape" of the term).
 * *base is NOT retained — caller must not free it independently of term.
 * Returns 1.0 for unrecognised forms (base = term itself). */
static qfloat term_coeff(const dval_t *term, const dval_t **base)
{
    /* unnamed pure constant: pure numeric, no symbolic base */
    if (term->ops == &ops_const && (!term->name || !*term->name)) {
        *base = NULL;
        return term->c;
    }
    /* neg(x): coefficient -1 */
    if (term->ops == &ops_neg) {
        *base = term->a;
        return qf_from_double(-1.0);
    }
    /* mul(unnamed_const, rest): coefficient is the leading const */
    if (term->ops == &ops_mul &&
        term->a->ops == &ops_const &&
        (!term->a->name || !*term->a->name)) {
        *base = term->b;
        return term->a->c;
    }
    /* everything else: coefficient 1 */
    *base = term;
    return qf_from_double(1.0);
}

/* Build coeff * base, owning the retained base reference. */
static dval_t *make_scaled(qfloat coeff, dval_t *base)
{
    if (is_qf_zero(coeff)) {
        dv_free(base);
        return dv_new_const_d(0.0);
    }
    if (is_qf_one(coeff))
        return base;
    dval_t *cn = dv_new_const(coeff);
    dval_t *r  = dv_mul(cn, base);
    dv_free(cn);
    dv_free(base);
    return r;
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

    /* generic unary dispatch via the vtable constructor */
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
/* Addend helpers for exp-combining                                          */
/* ------------------------------------------------------------------------- */

/* Flatten an addition tree into individual addends (DFS, iterative).
 * Each collected node is retained once; na is updated in place. */
static void flatten_add(dval_t *root, dval_t **addends, int *na, int max)
{
    dval_t *stk[64];
    int sp = 0;
    stk[sp++] = root;
    while (sp > 0 && *na < max) {
        dval_t *n = stk[--sp];
        if (n->ops == &ops_add) {
            if (sp < 63) { stk[sp++] = n->b; stk[sp++] = n->a; }
        } else {
            dv_retain(n);
            addends[(*na)++] = n;
        }
    }
}

/* Sort group for addends inside a combined exp argument:
 *   0 = unary function   (e.g. sin(x), cos(x))
 *   1 = variable         (e.g. x, y)
 *   2 = named constant   (e.g. π, e)
 *   3 = everything else
 */
static int addend_group(const dval_t *f)
{
    if (f->ops->arity == DV_OP_UNARY) return 0;
    if (f->ops == &ops_var)           return 1;
    if (f->ops == &ops_const && f->name && *f->name) return 2;
    return 3;
}

/* Primary sort name for an addend: the variable name it depends on,
 * or its own name for vars/consts. */
static const char *addend_sort_name(const dval_t *f)
{
    if (f->ops == &ops_var)
        return f->name ? f->name : "";
    if (f->ops == &ops_const)
        return f->name ? f->name : "";
    if (f->a && f->a->ops == &ops_var)
        return f->a->name ? f->a->name : "";
    return "";
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

        dval_t *r = dv_neg(a);
        dv_free(a);
        return r;
    }

    /* ============================================================
       ADDITION
       ============================================================ */
    if (f->ops == &ops_add) {

        if (a->ops == &ops_const && is_qf_zero(a->c)) { dv_free(a); return b; }
        if (b->ops == &ops_const && is_qf_zero(b->c)) { dv_free(b); return a; }

        if (a->ops == &ops_const && b->ops == &ops_const) {
            qfloat c = qf_add(a->c, b->c);
            dv_free(a); dv_free(b);
            return dv_new_const(c);
        }

        /* Combine like terms: ca*X + cb*X  →  (ca+cb)*X */
        const dval_t *base_a, *base_b;
        qfloat ca = term_coeff(a, &base_a);
        qfloat cb = term_coeff(b, &base_b);
        if (base_a && base_b && dv_struct_eq(base_a, base_b)) {
            dv_retain((dval_t *)base_a);
            dv_free(a);
            dv_free(b);
            return make_scaled(qf_add(ca, cb), (dval_t *)base_a);
        }

        dval_t *r = dv_add(a, b);
        dv_free(a);
        dv_free(b);
        return r;
    }

    /* ============================================================
       SUBTRACTION
       ============================================================ */
    if (f->ops == &ops_sub) {

        if (b->ops == &ops_const && is_qf_zero(b->c)) { dv_free(b); return a; }

        if (a->ops == &ops_const && b->ops == &ops_const) {
            qfloat c = qf_sub(a->c, b->c);
            dv_free(a); dv_free(b);
            return dv_new_const(c);
        }

        /* Combine like terms: ca*X - cb*X  →  (ca-cb)*X */
        const dval_t *base_a, *base_b;
        qfloat ca = term_coeff(a, &base_a);
        qfloat cb = term_coeff(b, &base_b);
        if (base_a && base_b && dv_struct_eq(base_a, base_b)) {
            dv_retain((dval_t *)base_a);
            dv_free(a);
            dv_free(b);
            return make_scaled(qf_sub(ca, cb), (dval_t *)base_a);
        }

        dval_t *r = dv_sub(a, b);
        dv_free(a);
        dv_free(b);
        return r;
    }

    /* ============================================================
       MULTIPLICATION (flatten + exponent combining)
       ============================================================ */
    if (f->ops == &ops_mul) {

        if ((a->ops == &ops_const && is_qf_zero(a->c)) ||
            (b->ops == &ops_const && is_qf_zero(b->c))) {
            dv_free(a);
            dv_free(b);
            return dv_new_const_d(0.0);
        }

        if (a->ops == &ops_const && is_qf_one(a->c)) {
            dv_free(a);
            return b;
        }
        if (b->ops == &ops_const && is_qf_one(b->c)) {
            dv_free(b);
            return a;
        }

        /* --- flatten --- */
        qfloat c_acc = qf_from_double(1.0);
        int is_zero = 0;
        dval_t **terms = NULL;
        size_t nterms = 0, cap = 0;

        collect_mul_flat(a, &c_acc, &is_zero, &terms, &nterms, &cap);
        collect_mul_flat(b, &c_acc, &is_zero, &terms, &nterms, &cap);

        dv_free(a);
        dv_free(b);

        if (is_zero) {
            for (size_t i = 0; i < nterms; ++i)
                dv_free(terms[i]);
            free(terms);
            return dv_new_const_d(0.0);
        }

        /* --- exponent combining: merge x, x^2, x^3, ... --- */
        for (size_t i = 0; i < nterms; ++i) {
            if (!terms[i]) continue;

            dval_t *ti = terms[i];

            /* detect base and exponent */
            dval_t *base = NULL;
            double exp = 1.0;

            if (ti->ops == &ops_pow_d) {
                base = ti->a;
                exp = qf_to_double(ti->c);
            } else {
                base = ti;
                exp = 1.0;
            }

            /* accumulate same-base factors */
            for (size_t j = i + 1; j < nterms; ++j) {
                if (!terms[j]) continue;

                dval_t *tj = terms[j];

                if (tj->ops == &ops_pow_d && dv_struct_eq(base, tj->a)) {
                    exp += qf_to_double(tj->c);
                    dv_free(tj);
                    terms[j] = NULL;
                }
                else if (dv_struct_eq(base, tj)) {
                    exp += 1.0;
                    dv_free(tj);
                    terms[j] = NULL;
                }
            }

            /* rebuild as power if exponent != 1 */
            if (exp != 1.0 || ti->ops == &ops_pow_d) {
                dv_retain(base);
                dv_free(ti);
                terms[i] = dv_pow_d(base, exp);
                dv_free(base);
            }
        }

        /* --- exp combining: exp(a) * exp(b) → exp(a+b) ---
         * Addends are sorted: unary fns (group 0) < vars (group 1) <
         * named consts (group 2) < everything else (group 3). */
        for (size_t i = 0; i < nterms; ++i) {
            if (!terms[i] || terms[i]->ops != &ops_exp) continue;

            for (size_t j = i + 1; j < nterms; ++j) {
                if (!terms[j] || terms[j]->ops != &ops_exp) continue;

                dval_t *addends[64];
                int na = 0;

                flatten_add(terms[i]->a, addends, &na, 64);
                flatten_add(terms[j]->a, addends, &na, 64);

                dv_free(terms[i]);
                dv_free(terms[j]);
                terms[j] = NULL;

                /* insertion sort by group then name */
                for (int s = 1; s < na; ++s) {
                    dval_t *key = addends[s];
                    int kg = addend_group(key);
                    int t  = s - 1;
                    while (t >= 0) {
                        int tg  = addend_group(addends[t]);
                        int cmp = (tg != kg)
                                  ? (tg - kg)
                                  : strcmp(addend_sort_name(addends[t]),
                                           addend_sort_name(key));
                        if (cmp <= 0) break;
                        addends[t + 1] = addends[t];
                        --t;
                    }
                    addends[t + 1] = key;
                }

                /* rebuild sum */
                dval_t *sum = addends[0];
                for (int k = 1; k < na; ++k) {
                    dval_t *s = dv_add(sum, addends[k]);
                    dv_free(sum);
                    dv_free(addends[k]);
                    sum = s;
                }
                dval_t *simp = dv_simplify(sum);
                dv_free(sum);

                terms[i] = dv_exp(simp);
                dv_free(simp);
            }
        }

        /* --- rebuild multiplication chain --- */
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

        if (b->ops == &ops_const && is_qf_one(b->c)) {
            dv_free(b);
            return a;
        }

        if (a->ops == &ops_const && is_qf_zero(a->c)) {
            dv_free(a);
            dv_free(b);
            return dv_new_const_d(0.0);
        }

        if (a->ops == &ops_const && b->ops == &ops_const) {
            qfloat q = qf_div(a->c, b->c);
            dv_free(a);
            dv_free(b);
            return dv_new_const(q);
        }

        dval_t *r = dv_div(a, b);
        dv_free(a);
        dv_free(b);
        return r;
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

        dval_t *r = dv_pow_d(a, ed);
        dv_free(a);
        return r;
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

        dval_t *r = dv_pow(a, b);
        dv_free(a);
        dv_free(b);
        return r;
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
