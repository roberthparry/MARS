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
    /* neg(x): coefficient -1; neg(c·rest): coefficient -c */
    if (term->ops == &ops_neg) {
        if (term->a->ops == &ops_mul &&
            term->a->a->ops == &ops_const &&
            (!term->a->a->name || !*term->a->a->name)) {
            *base = term->a->b;
            return qf_neg(term->a->a->c);
        }
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
    if (qf_to_double(coeff) == -1.0) {
        dval_t *r = dv_neg(base);
        dv_free(base);
        return r;
    }
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

        /* Generic constant fold for all other unary functions applied to an
         * unnamed constant: build a temporary node, evaluate numerically,
         * and collapse to a constant. Named constants (e.g. π) are left
         * symbolic so their names are preserved in to_string output. */
        if (!a->name || !*a->name) {
            if (f->ops->apply_unary) {
                dval_t *tmp = f->ops->apply_unary(a); /* retains a */
                qfloat   v  = tmp->ops->eval(tmp);
                dv_free(tmp);
                dv_free(a);
                return dv_new_const(v);
            }
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
/* Addition/subtraction flattening with like-term combining                  */
/* ------------------------------------------------------------------------- */

typedef struct { dval_t *base; qfloat coeff; } addend_t;

/* Flatten an add/sub tree into coefficient·base pairs.
 * sign is +1 or -1 for the current branch.
 * Pure numeric constants accumulate into *c_const. */
static void collect_addends(
    dval_t *f, int sign,
    qfloat *c_const,
    addend_t **terms, size_t *n, size_t *cap)
{
    if (!f) return;

    if (f->ops == &ops_add) {
        collect_addends(f->a,  sign, c_const, terms, n, cap);
        collect_addends(f->b,  sign, c_const, terms, n, cap);
        return;
    }
    if (f->ops == &ops_sub) {
        collect_addends(f->a,  sign, c_const, terms, n, cap);
        collect_addends(f->b, -sign, c_const, terms, n, cap);
        return;
    }
    /* neg(add/sub): distribute negation into the inner sum */
    if (f->ops == &ops_neg &&
        (f->a->ops == &ops_add || f->a->ops == &ops_sub)) {
        collect_addends(f->a, -sign, c_const, terms, n, cap);
        return;
    }

    const dval_t *base;
    qfloat coeff = term_coeff(f, &base);
    if (sign < 0) coeff = qf_neg(coeff);

    if (!base) {
        *c_const = qf_add(*c_const, coeff);
        return;
    }

    /* combine with any existing entry that has the same base */
    for (size_t i = 0; i < *n; ++i) {
        if (dv_struct_eq((*terms)[i].base, base)) {
            (*terms)[i].coeff = qf_add((*terms)[i].coeff, coeff);
            return;
        }
    }

    /* new entry */
    if (*n == *cap) {
        *cap   = *cap ? *cap * 2 : 8;
        *terms = realloc(*terms, *cap * sizeof(addend_t));
    }
    dv_retain((dval_t *)base);
    (*terms)[*n].base  = (dval_t *)base;
    (*terms)[*n].coeff = coeff;
    (*n)++;
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
       ADDITION / SUBTRACTION  — flatten + combine like terms
       ============================================================ */
    if (f->ops == &ops_add || f->ops == &ops_sub) {

        qfloat   c_const = qf_from_double(0.0);
        addend_t *terms  = NULL;
        size_t    n = 0, cap = 0;

        collect_addends(a, +1, &c_const, &terms, &n, &cap);
        dv_free(a);
        collect_addends(b, (f->ops == &ops_sub) ? -1 : +1,
                        &c_const, &terms, &n, &cap);
        dv_free(b);

        /* rebuild: symbolic terms first, then constant */
        dval_t *cur = NULL;

        for (size_t i = 0; i < n; ++i) {
            if (is_qf_zero(terms[i].coeff)) {
                dv_free(terms[i].base);
                continue;
            }
            dval_t *term = make_scaled(terms[i].coeff, terms[i].base);
            if (!cur) {
                cur = term;
            } else {
                dval_t *tmp = dv_add(cur, term);
                dv_free(cur);
                dv_free(term);
                cur = tmp;
            }
        }

        free(terms);

        if (!is_qf_zero(c_const)) {
            dval_t *cterm = dv_new_const(c_const);
            if (!cur) {
                cur = cterm;
            } else {
                dval_t *tmp = dv_add(cur, cterm);
                dv_free(cur);
                dv_free(cterm);
                cur = tmp;
            }
        }

        return cur ? cur : dv_new_const_d(0.0);
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

        /* --- extract div factors: div(a,b) splits a into numer, b into denom --- */
        dval_t **dterms = NULL;
        size_t ndterms = 0, dcap = 0;

        for (size_t i = 0; i < nterms; ++i) {
            if (!terms[i] || terms[i]->ops != &ops_div) continue;
            dval_t *dnode = terms[i];
            dval_t *num   = dnode->a;
            dval_t *den   = dnode->b;
            dv_retain(num);
            dv_retain(den);
            dv_free(dnode);
            terms[i] = NULL;

            /* absorb numeric numerator into c_acc */
            if (num->ops == &ops_const && (!num->name || !*num->name)) {
                if (is_qf_zero(num->c))
                    is_zero = 1;
                else
                    c_acc = qf_mul(c_acc, num->c);
                dv_free(num);
            } else {
                terms[i] = num;
            }

            if (is_zero) { dv_free(den); continue; }

            /* absorb numeric denominator into c_acc (divide) */
            if (den->ops == &ops_const && (!den->name || !*den->name)) {
                c_acc = qf_div(c_acc, den->c);
                dv_free(den);
            } else {
                if (ndterms == dcap) {
                    dcap   = dcap ? dcap * 2 : 4;
                    dterms = realloc(dterms, dcap * sizeof(dval_t *));
                }
                dterms[ndterms++] = den;
            }
        }

        if (is_zero) {
            for (size_t i = 0; i < nterms; ++i) dv_free(terms[i]);
            for (size_t i = 0; i < ndterms; ++i) dv_free(dterms[i]);
            free(terms); free(dterms);
            return dv_new_const_d(0.0);
        }

        /* --- exponent combining on denominator terms --- */
        for (size_t i = 0; i < ndterms; ++i) {
            if (!dterms[i]) continue;
            dval_t *ti   = dterms[i];
            dval_t *base = (ti->ops == &ops_pow_d) ? ti->a : ti;
            double  exp  = (ti->ops == &ops_pow_d) ? qf_to_double(ti->c) : 1.0;
            for (size_t j = i + 1; j < ndterms; ++j) {
                if (!dterms[j]) continue;
                dval_t *tj = dterms[j];
                if (tj->ops == &ops_pow_d && dv_struct_eq(base, tj->a)) {
                    exp += qf_to_double(tj->c); dv_free(tj); dterms[j] = NULL;
                } else if (dv_struct_eq(base, tj)) {
                    exp += 1.0; dv_free(tj); dterms[j] = NULL;
                }
            }
            if (exp != 1.0 || ti->ops == &ops_pow_d) {
                dv_retain(base); dv_free(ti);
                dterms[i] = dv_pow_d(base, exp); dv_free(base);
            }
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

        /* --- rebuild numerator chain --- */
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
            cur = dv_new_const(c_acc);

        /* --- rebuild denominator and wrap with division --- */
        if (ndterms == 0) {
            free(dterms);
            return cur;
        }

        dval_t *denom = NULL;
        for (size_t i = 0; i < ndterms; ++i) {
            if (!dterms[i]) continue;
            if (!denom) {
                denom = dterms[i];
            } else {
                dval_t *tmp = dv_mul(denom, dterms[i]);
                dv_free(denom);
                dv_free(dterms[i]);
                denom = tmp;
            }
        }
        free(dterms);

        dval_t *div_node = dv_div(cur, denom);
        dv_free(cur);
        dv_free(denom);
        dval_t *result = dv_simplify(div_node);
        dv_free(div_node);
        return result;
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

        /* sinh(x)/cosh(x) → tanh(x) */
        if (a->ops == &ops_sinh && b->ops == &ops_cosh &&
            dv_struct_eq(a->a, b->a)) {
            dv_retain(a->a);
            dval_t *r = dv_tanh(a->a);
            dv_free(a->a);
            dv_free(a);
            dv_free(b);
            return r;
        }

        /* sin(x)/cos(x) → tan(x) */
        if (a->ops == &ops_sin && b->ops == &ops_cos &&
            dv_struct_eq(a->a, b->a)) {
            dv_retain(a->a);
            dval_t *r = dv_tan(a->a);
            dv_free(a->a);
            dv_free(a);
            dv_free(b);
            return r;
        }

        /* x/abs(x) → abs(x)/x  (canonical form for sign function) */
        if (b->ops == &ops_abs && dv_struct_eq(a, b->a)) {
            dval_t *r = dv_div(b, a);
            dv_free(a);
            dv_free(b);
            return r;
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

        /* constant folding: pow_d(unnamed_const, exp) → const */
        if (a->ops == &ops_const && (!a->name || !*a->name)) {
            qfloat v = qf_pow(a->c, f->c);
            dv_free(a);
            return dv_new_const(v);
        }

        /* sqrt(x)^n → x^(n/2) */
        if (a->ops == &ops_sqrt) {
            double half = ed / 2.0;
            dv_retain(a->a);
            dval_t *inner = a->a;
            dv_free(a);
            if (half == 1.0) return inner;
            dval_t *r = dv_pow_d(inner, half);
            dv_free(inner);
            return r;
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
       BINARY SPECIAL FUNCTIONS (hypot, beta, logbeta)
       ============================================================ */
    if (f->ops == &ops_hypot) {

        /* hypot(0, b) → |b| */
        if (a->ops == &ops_const && is_qf_zero(a->c)) {
            dv_free(a);
            dval_t *r = dv_abs(b);
            dv_free(b);
            return r;
        }

        /* hypot(a, 0) → |a| */
        if (b->ops == &ops_const && is_qf_zero(b->c)) {
            dv_free(b);
            dval_t *r = dv_abs(a);
            dv_free(a);
            return r;
        }

        /* hypot(const, const) → numeric constant */
        if (a->ops == &ops_const && (!a->name || !*a->name) &&
            b->ops == &ops_const && (!b->name || !*b->name)) {
            dval_t *tmp = dv_hypot(a, b);
            qfloat   v  = tmp->ops->eval(tmp);
            dv_free(tmp); dv_free(a); dv_free(b);
            return dv_new_const(v);
        }

        dval_t *r = dv_hypot(a, b);
        dv_free(a); dv_free(b);
        return r;
    }

    if (f->ops == &ops_beta || f->ops == &ops_logbeta) {

        /* both constant → numeric constant */
        if (a->ops == &ops_const && (!a->name || !*a->name) &&
            b->ops == &ops_const && (!b->name || !*b->name)) {
            dval_t *tmp = (f->ops == &ops_beta) ? dv_beta(a, b) : dv_logbeta(a, b);
            qfloat   v  = tmp->ops->eval(tmp);
            dv_free(tmp); dv_free(a); dv_free(b);
            return dv_new_const(v);
        }

        dval_t *r = (f->ops == &ops_beta) ? dv_beta(a, b) : dv_logbeta(a, b);
        dv_free(a); dv_free(b);
        return r;
    }

    /* ============================================================
       FALLBACK (unrecognised binary ops: rebuild with simplified children)
       ============================================================ */
    if (a) dv_free(a);
    if (b) dv_free(b);

    dv_retain(f);
    return f;
}
