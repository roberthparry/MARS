#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qfloat.h"
#include "dval_internal.h"

/* forward declaration — helpers below call dv_simplify recursively */
dval_t *dv_simplify(dval_t *dv);

/* ========================================================================= */
/* Scalar predicates                                                          */
/* ========================================================================= */

static int is_qf_zero(qfloat x) { return qf_to_double(x) == 0.0; }
static int is_qf_one (qfloat x) { return qf_to_double(x) == 1.0; }

/* ========================================================================= */
/* Structural equality                                                        */
/* ========================================================================= */

static int dv_struct_eq(const dval_t *u, const dval_t *v)
{
    if (u == v) return 1;
    if (u->ops != v->ops) return 0;
    if (u->ops == &ops_const)
        return qf_to_double(u->c) == qf_to_double(v->c);
    if (u->ops == &ops_var)
        return u == v; /* identity */
    if (u->ops == &ops_neg)
        return dv_struct_eq(u->a, v->a);
    if (u->ops == &ops_pow_d)
        return dv_struct_eq(u->a, v->a) &&
               qf_to_double(u->c) == qf_to_double(v->c);
    return dv_struct_eq(u->a, v->a) && dv_struct_eq(u->b, v->b);
}

/* ========================================================================= */
/* Multiplication flattening                                                  */
/* ========================================================================= */

static void collect_mul_flat(
    dval_t *dv,
    qfloat *c_acc, int *is_zero,
    dval_t ***terms, size_t *nterms, size_t *cap)
{
    if (*is_zero) return;

    if (dv->ops == &ops_const && (!dv->name || !*dv->name)) {
        if (is_qf_zero(dv->c)) { *is_zero = 1; return; }
        *c_acc = qf_mul(*c_acc, dv->c);
        return;
    }
    if (dv->ops == &ops_neg) {
        *c_acc = qf_neg(*c_acc);
        collect_mul_flat(dv->a, c_acc, is_zero, terms, nterms, cap);
        return;
    }
    if (dv->ops == &ops_mul) {
        collect_mul_flat(dv->a, c_acc, is_zero, terms, nterms, cap);
        collect_mul_flat(dv->b, c_acc, is_zero, terms, nterms, cap);
        return;
    }
    if (*nterms == *cap) {
        *cap   = (*cap == 0 ? 4 : *cap * 2);
        *terms = realloc(*terms, *cap * sizeof(dval_t *));
    }
    dv_retain(dv);
    (*terms)[(*nterms)++] = dv;
}

/* ========================================================================= */
/* Like-term helpers for addition/subtraction                                */
/* ========================================================================= */

static qfloat term_coeff(const dval_t *term, const dval_t **base)
{
    /* unnamed pure constant: numeric only, no symbolic base */
    if (term->ops == &ops_const && (!term->name || !*term->name)) {
        *base = NULL;
        return term->c;
    }
    /* neg(c·rest): coefficient -c */
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
    *base = term;
    return qf_from_double(1.0);
}

/* Build  coeff * base,  consuming the retained base reference. */
static dval_t *make_scaled(qfloat coeff, dval_t *base)
{
    if (is_qf_zero(coeff)) { dv_free(base); return dv_new_const_d(0.0); }
    if (is_qf_one(coeff))  return base;
    if (qf_to_double(coeff) == -1.0) {
        /* neg((-c·rest)/den) → (c·rest)/den — eliminates double negative */
        if (base->ops == &ops_div && base->a->ops == &ops_mul &&
            base->a->a->ops == &ops_const &&
            (!base->a->a->name || !*base->a->a->name) &&
            qf_to_double(base->a->a->c) < 0.0) {
            qfloat    pos_c   = qf_neg(base->a->a->c);
            dval_t   *rest    = base->a->b;
            dval_t   *den     = base->b;
            dv_retain(rest); dv_retain(den); dv_free(base);
            dval_t *new_num   = make_scaled(pos_c, rest);
            dval_t *r         = dv_div(new_num, den);
            dv_free(new_num); dv_free(den); return r;
        }
        /* neg(neg(x)/den) → x/den */
        if (base->ops == &ops_div && base->a->ops == &ops_neg) {
            dval_t *inner = base->a->a;
            dval_t *den   = base->b;
            dv_retain(inner); dv_retain(den); dv_free(base);
            dval_t *r = dv_div(inner, den);
            dv_free(inner); dv_free(den); return r;
        }
        dval_t *r = dv_neg(base); dv_free(base); return r;
    }
    /* fold: coeff * (c * rest) → (coeff*c) * rest */
    if (base->ops == &ops_mul &&
        base->a->ops == &ops_const &&
        (!base->a->name || !*base->a->name)) {
        qfloat folded = qf_mul(coeff, base->a->c);
        dv_retain(base->b);
        dval_t *rest = base->b;
        dv_free(base);
        return make_scaled(folded, rest);
    }
    /* fold: coeff * (mul(c,a) * b) → (coeff*c) * mul(a,b)
     * handles one extra nesting level, e.g. (2x)*exp(-x) */
    if (base->ops == &ops_mul &&
        base->a->ops == &ops_mul &&
        base->a->a->ops == &ops_const &&
        (!base->a->a->name || !*base->a->a->name)) {
        qfloat folded = qf_mul(coeff, base->a->a->c);
        dv_retain(base->a->b);
        dv_retain(base->b);
        dval_t *inner = dv_mul(base->a->b, base->b);
        dv_free(base->a->b);
        dv_free(base->b);
        dv_free(base);
        return make_scaled(folded, inner);
    }
    dval_t *cn = dv_new_const(coeff);
    dval_t *r  = dv_mul(cn, base);
    dv_free(cn); dv_free(base);
    return r;
}

/* ========================================================================= */
/* Add/sub: collect coefficient·base pairs                                   */
/* ========================================================================= */

typedef struct { dval_t *base; qfloat coeff; } addend_t;

static void collect_addends(
    dval_t *dv, int sign,
    qfloat *c_const,
    addend_t **terms, size_t *n, size_t *cap)
{
    if (!dv) return;
    if (dv->ops == &ops_add) {
        collect_addends(dv->a,  sign, c_const, terms, n, cap);
        collect_addends(dv->b,  sign, c_const, terms, n, cap);
        return;
    }
    if (dv->ops == &ops_sub) {
        collect_addends(dv->a,  sign, c_const, terms, n, cap);
        collect_addends(dv->b, -sign, c_const, terms, n, cap);
        return;
    }
    if (dv->ops == &ops_neg &&
        (dv->a->ops == &ops_add || dv->a->ops == &ops_sub)) {
        collect_addends(dv->a, -sign, c_const, terms, n, cap);
        return;
    }
    const dval_t *base;
    qfloat coeff = term_coeff(dv, &base);
    if (sign < 0) coeff = qf_neg(coeff);
    if (!base) { *c_const = qf_add(*c_const, coeff); return; }

    for (size_t i = 0; i < *n; ++i) {
        if (dv_struct_eq((*terms)[i].base, base)) {
            (*terms)[i].coeff = qf_add((*terms)[i].coeff, coeff);
            return;
        }
    }
    if (*n == *cap) {
        *cap   = *cap ? *cap * 2 : 8;
        *terms = realloc(*terms, *cap * sizeof(addend_t));
    }
    dv_retain((dval_t *)base);
    (*terms)[*n].base  = (dval_t *)base;
    (*terms)[*n].coeff = coeff;
    (*n)++;
}

/* ========================================================================= */
/* Addend helpers for exp-combining                                          */
/* ========================================================================= */

static void flatten_add(dval_t *root, dval_t **addends, int *na, int max)
{
    dval_t *stk[64]; int sp = 0;
    stk[sp++] = root;
    while (sp > 0 && *na < max) {
        dval_t *dv = stk[--sp];
        if (dv->ops == &ops_add) {
            if (sp < 63) { stk[sp++] = dv->b; stk[sp++] = dv->a; }
        } else { dv_retain(dv); addends[(*na)++] = dv; }
    }
}

static int addend_group(const dval_t *dv)
{
    if (dv->ops->arity == DV_OP_UNARY)                    return 0;
    if (dv->ops == &ops_var)                              return 1;
    if (dv->ops == &ops_const && dv->name && *dv->name)   return 2;
    return 3;
}

static const char *addend_sort_name(const dval_t *dv)
{
    if (dv->ops == &ops_var || dv->ops == &ops_const)
        return dv->name ? dv->name : "";
    if (dv->a && dv->a->ops == &ops_var)
        return dv->a->name ? dv->a->name : "";
    return "";
}

/* ========================================================================= */
/* Polynomial expansion: expand u*v into a sum (non-owning args)             */
/* ========================================================================= */

static dval_t *expand_product(const dval_t *u, const dval_t *v)
{
    if (u->ops == &ops_add) {
        dval_t *l = expand_product(u->a, v);
        dval_t *r = expand_product(u->b, v);
        dval_t *s = dv_add(l, r); dv_free(l); dv_free(r);
        return s;
    }
    if (u->ops == &ops_sub) {
        dval_t *l = expand_product(u->a, v);
        dval_t *r = expand_product(u->b, v);
        dval_t *s = dv_sub(l, r); dv_free(l); dv_free(r);
        return s;
    }
    if (v->ops == &ops_add || v->ops == &ops_sub)
        return expand_product(v, u);   /* swap so additive factor is on left */
    /* neither additive: u * v */
    dv_retain((dval_t *)u);
    dv_retain((dval_t *)v);
    dval_t *prod = dv_mul((dval_t *)u, (dval_t *)v);
    dv_free((dval_t *)u);
    dv_free((dval_t *)v);
    return prod;
}

/* ========================================================================= */
/* Unary function simplification                                             */
/* ========================================================================= */

static dval_t *simplify_unary_fun(dval_t *dv, dval_t *a)
{
    if (a->ops == &ops_const) {
        double x = qf_to_double(a->c);

        if (dv->ops == &ops_sin  && x == 0.0) { dv_free(a); return dv_new_const_d(0.0); }
        if (dv->ops == &ops_cos  && x == 0.0) { dv_free(a); return dv_new_const_d(1.0); }
        if (dv->ops == &ops_tan  && x == 0.0) { dv_free(a); return dv_new_const_d(0.0); }
        if (dv->ops == &ops_exp  && x == 0.0) { dv_free(a); return dv_new_const_d(1.0); }
        if (dv->ops == &ops_log  && x == 1.0) { dv_free(a); return dv_new_const_d(0.0); }
        if (dv->ops == &ops_sqrt) {
            if (x == 0.0) { dv_free(a); return dv_new_const_d(0.0); }
            if (x == 1.0) { dv_free(a); return dv_new_const_d(1.0); }
        }

        if (!a->name || !*a->name) {
            if (dv->ops->apply_unary) {
                dval_t *tmp = dv->ops->apply_unary(a);
                qfloat   v  = tmp->ops->eval(tmp);
                dv_free(tmp); dv_free(a);
                return dv_new_const(v);
            }
        }
    }

    if (dv->ops->apply_unary) {
        dval_t *out = dv->ops->apply_unary(a);
        dv_free(a);
        return out;
    }

    dv_free(a); dv_retain(dv);
    return dv;
}

/* ========================================================================= */
/* Per-operation simplifiers                                                 */
/* ========================================================================= */

static dval_t *simplify_neg(dval_t *a)
{
    /* --x → x */
    if (a->ops == &ops_neg) {
        dval_t *inner = a->a; dv_retain(inner); dv_free(a); return inner;
    }
    /* neg(c) → -c */
    if (a->ops == &ops_const) {
        qfloat c = qf_neg(a->c); dv_free(a); return dv_new_const(c);
    }
    /* neg(c·x) where c < 0 → |c|·x  (eliminates spurious double-negative) */
    if (a->ops == &ops_mul &&
        a->a->ops == &ops_const &&
        (!a->a->name || !*a->a->name) &&
        qf_to_double(a->a->c) < 0.0) {
        qfloat pos_c = qf_neg(a->a->c);
        dv_retain(a->b);
        dval_t *rest = a->b;
        dv_free(a);
        return make_scaled(pos_c, rest);
    }
    dval_t *r = dv_neg(a); dv_free(a); return r;
}

/* --- */

static dval_t *simplify_add_sub(dval_t *dv, dval_t *a, dval_t *b)
{
    qfloat    c_const = qf_from_double(0.0);
    addend_t *terms   = NULL;
    size_t    n = 0, cap = 0;

    collect_addends(a, +1, &c_const, &terms, &n, &cap);
    dv_free(a);
    collect_addends(b, (dv->ops == &ops_sub) ? -1 : +1, &c_const, &terms, &n, &cap);
    dv_free(b);

    /* find the leading non-cancelled symbolic term */
    int leading_neg = 0;
    for (size_t i = 0; i < n; ++i) {
        if (!is_qf_zero(terms[i].coeff)) {
            leading_neg = qf_to_double(terms[i].coeff) < 0.0;
            break;
        }
    }

    dval_t *cur = NULL;

    /* emit a positive constant before any leading negative symbolic term so
     * the expression reads "1 - tanh²(x)" rather than "-tanh²(x) + 1" */
    int const_emitted = 0;
    if (!is_qf_zero(c_const) && qf_to_double(c_const) > 0.0 && leading_neg) {
        cur = dv_new_const(c_const);
        const_emitted = 1;
    }

    for (size_t i = 0; i < n; ++i) {
        if (is_qf_zero(terms[i].coeff)) { dv_free(terms[i].base); continue; }
        dval_t *term = make_scaled(terms[i].coeff, terms[i].base);
        if (!cur) cur = term;
        else {
            dval_t *tmp = dv_add(cur, term);
            dv_free(cur); dv_free(term); cur = tmp;
        }
    }
    free(terms);

    if (!const_emitted && !is_qf_zero(c_const)) {
        dval_t *cterm = dv_new_const(c_const);
        if (!cur) cur = cterm;
        else {
            dval_t *tmp = dv_add(cur, cterm);
            dv_free(cur); dv_free(cterm); cur = tmp;
        }
    }

    return cur ? cur : dv_new_const_d(0.0);
}

/* --- */

static dval_t *simplify_mul(dval_t *dv, dval_t *a, dval_t *b)
{
    (void)dv;

    if ((a->ops == &ops_const && is_qf_zero(a->c)) ||
        (b->ops == &ops_const && is_qf_zero(b->c))) {
        dv_free(a); dv_free(b); return dv_new_const_d(0.0);
    }
    if (a->ops == &ops_const && is_qf_one(a->c)) { dv_free(a); return b; }
    if (b->ops == &ops_const && is_qf_one(b->c)) { dv_free(b); return a; }

    /* --- flatten --- */
    qfloat   c_acc  = qf_from_double(1.0);
    int      is_zero = 0;
    dval_t **terms   = NULL;
    size_t   nterms  = 0, cap = 0;

    collect_mul_flat(a, &c_acc, &is_zero, &terms, &nterms, &cap);
    collect_mul_flat(b, &c_acc, &is_zero, &terms, &nterms, &cap);
    dv_free(a); dv_free(b);

    if (is_zero) {
        for (size_t i = 0; i < nterms; ++i) dv_free(terms[i]);
        free(terms); return dv_new_const_d(0.0);
    }

    /* --- extract div factors --- */
    dval_t **dterms  = NULL;
    size_t   ndterms = 0, dcap = 0;

    for (size_t i = 0; i < nterms; ++i) {
        if (!terms[i] || terms[i]->ops != &ops_div) continue;
        dval_t *dnode = terms[i];
        dval_t *num   = dnode->a;
        dval_t *den   = dnode->b;
        dv_retain(num); dv_retain(den);
        dv_free(dnode); terms[i] = NULL;

        if (num->ops == &ops_const && (!num->name || !*num->name)) {
            if (is_qf_zero(num->c)) is_zero = 1;
            else c_acc = qf_mul(c_acc, num->c);
            dv_free(num);
        } else { terms[i] = num; }

        if (is_zero) { dv_free(den); continue; }

        if (den->ops == &ops_const && (!den->name || !*den->name)) {
            c_acc = qf_div(c_acc, den->c); dv_free(den);
        } else {
            if (ndterms == dcap) {
                dcap   = dcap ? dcap * 2 : 4;
                dterms = realloc(dterms, dcap * sizeof(dval_t *));
            }
            dterms[ndterms++] = den;
        }
    }

    if (is_zero) {
        for (size_t i = 0; i < nterms;  ++i) dv_free(terms[i]);
        for (size_t i = 0; i < ndterms; ++i) dv_free(dterms[i]);
        free(terms); free(dterms); return dv_new_const_d(0.0);
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

    /* --- exponent combining on numerator terms --- */
    for (size_t i = 0; i < nterms; ++i) {
        if (!terms[i]) continue;
        dval_t *ti   = terms[i];
        dval_t *base = (ti->ops == &ops_pow_d) ? ti->a : ti;
        double  exp  = (ti->ops == &ops_pow_d) ? qf_to_double(ti->c) : 1.0;
        for (size_t j = i + 1; j < nterms; ++j) {
            if (!terms[j]) continue;
            dval_t *tj = terms[j];
            if (tj->ops == &ops_pow_d && dv_struct_eq(base, tj->a)) {
                exp += qf_to_double(tj->c); dv_free(tj); terms[j] = NULL;
            } else if (dv_struct_eq(base, tj)) {
                exp += 1.0; dv_free(tj); terms[j] = NULL;
            }
        }
        if (exp != 1.0 || ti->ops == &ops_pow_d) {
            dv_retain(base); dv_free(ti);
            terms[i] = dv_pow_d(base, exp); dv_free(base);
        }
    }

    /* --- exp combining: exp(a) * exp(b) → exp(a+b) --- */
    for (size_t i = 0; i < nterms; ++i) {
        if (!terms[i] || terms[i]->ops != &ops_exp) continue;
        for (size_t j = i + 1; j < nterms; ++j) {
            if (!terms[j] || terms[j]->ops != &ops_exp) continue;

            dval_t *addends[64]; int na = 0;
            flatten_add(terms[i]->a, addends, &na, 64);
            flatten_add(terms[j]->a, addends, &na, 64);
            dv_free(terms[i]); dv_free(terms[j]); terms[j] = NULL;

            /* insertion sort by group then name */
            for (int s = 1; s < na; ++s) {
                dval_t *key = addends[s]; int kg = addend_group(key); int t = s - 1;
                while (t >= 0) {
                    int tg  = addend_group(addends[t]);
                    int cmp = (tg != kg) ? (tg - kg)
                              : strcmp(addend_sort_name(addends[t]),
                                       addend_sort_name(key));
                    if (cmp <= 0) break;
                    addends[t + 1] = addends[t]; --t;
                }
                addends[t + 1] = key;
            }

            dval_t *sum = addends[0];
            for (int k = 1; k < na; ++k) {
                dval_t *s = dv_add(sum, addends[k]);
                dv_free(sum); dv_free(addends[k]); sum = s;
            }
            dval_t *simp     = dv_simplify(sum); dv_free(sum);
            dval_t *combined = dv_exp(simp); dv_free(simp);
            terms[i] = dv_simplify(combined); dv_free(combined);
        }
    }

    /* --- sqrt merging: sqrt(a) * sqrt(b) → sqrt(simplify(a*b)) --- */
    for (size_t i = 0; i < nterms; ++i) {
        if (!terms[i] || terms[i]->ops != &ops_sqrt) continue;
        for (size_t j = i + 1; j < nterms; ++j) {
            if (!terms[j] || terms[j]->ops != &ops_sqrt) continue;
            dv_retain(terms[i]->a); dv_retain(terms[j]->a);
            dval_t *prod     = dv_mul(terms[i]->a, terms[j]->a);
            dv_free(terms[i]->a); dv_free(terms[j]->a);
            dval_t *simp_arg = dv_simplify(prod); dv_free(prod);
            dval_t *raw      = dv_sqrt(simp_arg); dv_free(simp_arg);
            dv_free(terms[i]); dv_free(terms[j]);
            terms[j] = NULL; terms[i] = dv_simplify(raw); dv_free(raw);
            break; /* restart outer loop implicitly; inner handled */
        }
    }

    /* --- sqrt merging in denominator --- */
    for (size_t i = 0; i < ndterms; ++i) {
        if (!dterms[i] || dterms[i]->ops != &ops_sqrt) continue;
        for (size_t j = i + 1; j < ndterms; ++j) {
            if (!dterms[j] || dterms[j]->ops != &ops_sqrt) continue;
            dv_retain(dterms[i]->a); dv_retain(dterms[j]->a);
            dval_t *prod     = dv_mul(dterms[i]->a, dterms[j]->a);
            dv_free(dterms[i]->a); dv_free(dterms[j]->a);
            dval_t *simp_arg = dv_simplify(prod); dv_free(prod);
            dval_t *raw      = dv_sqrt(simp_arg); dv_free(simp_arg);
            dv_free(dterms[i]); dv_free(dterms[j]);
            dterms[j] = NULL; dterms[i] = dv_simplify(raw); dv_free(raw);
            break;
        }
    }

    /* --- polynomial expansion: (a ± b) * (c ± d) → expand + re-simplify
     * Conservative guard: exactly two non-null factors, no denominator,
     * both are direct binary add/sub (no nested add/sub children). --- */
    {
        size_t ai = nterms, bi_idx = nterms;
        int    too_many = 0;
        for (size_t i = 0; i < nterms; ++i) {
            if (!terms[i]) continue;
            if      (ai     == nterms) ai     = i;
            else if (bi_idx == nterms) bi_idx = i;
            else                     { too_many = 1; break; }
        }
        if (!too_many && ai < nterms && bi_idx < nterms && ndterms == 0) {
            dval_t *t0 = terms[ai], *t1 = terms[bi_idx];
            int t0_add = (t0->ops == &ops_add || t0->ops == &ops_sub);
            int t1_add = (t1->ops == &ops_add || t1->ops == &ops_sub);
            /* require both to be shallow (no nested add/sub in their children)
             * so expansion produces at most 4 terms;
             * also require a shared non-numeric-constant child — otherwise
             * (x+π)*(y+τ) would explode into 4 unrelated terms */
            int share = 0;
            if (t0_add && t1_add) {
                const dval_t *t0c[2] = { t0->a, t0->b };
                const dval_t *t1c[2] = { t1->a, t1->b };
                for (int p = 0; p < 2 && !share; ++p)
                    for (int q = 0; q < 2 && !share; ++q) {
                        const dval_t *u = t0c[p], *v = t1c[q];
                        if (u->ops == &ops_const && (!u->name || !*u->name)) continue;
                        if (v->ops == &ops_const && (!v->name || !*v->name)) continue;
                        if (dv_struct_eq(u, v)) share = 1;
                    }
            }
            if (t0_add && t1_add && share &&
                t0->a->ops != &ops_add && t0->a->ops != &ops_sub &&
                t0->b->ops != &ops_add && t0->b->ops != &ops_sub &&
                t1->a->ops != &ops_add && t1->a->ops != &ops_sub &&
                t1->b->ops != &ops_add && t1->b->ops != &ops_sub) {
                dval_t *expanded = expand_product(t0, t1);
                dv_free(t0); dv_free(t1);
                free(terms); free(dterms);
                dval_t *simp = dv_simplify(expanded); dv_free(expanded);
                return make_scaled(c_acc, simp);
            }
        }
    }

    /* --- rebuild numerator chain --- */
    dval_t *cur = NULL;
    if (!is_qf_one(c_acc)) cur = dv_new_const(c_acc);
    for (size_t i = 0; i < nterms; ++i) {
        if (!terms[i]) continue;
        if (!cur) cur = terms[i];
        else {
            dval_t *tmp = dv_mul(cur, terms[i]);
            dv_free(cur); dv_free(terms[i]); cur = tmp;
        }
    }
    free(terms);
    if (!cur) cur = dv_new_const(c_acc);

    if (ndterms == 0) { free(dterms); return cur; }

    /* --- rebuild denominator chain --- */
    dval_t *denom = NULL;
    for (size_t i = 0; i < ndterms; ++i) {
        if (!dterms[i]) continue;
        if (!denom) denom = dterms[i];
        else {
            dval_t *tmp = dv_mul(denom, dterms[i]);
            dv_free(denom); dv_free(dterms[i]); denom = tmp;
        }
    }
    free(dterms);

    dval_t *div_node = dv_div(cur, denom);
    dv_free(cur); dv_free(denom);
    dval_t *result = dv_simplify(div_node); dv_free(div_node);
    return result;
}

/* --- */

static dval_t *simplify_div(dval_t *dv, dval_t *a, dval_t *b)
{
    (void)dv;

    if (b->ops == &ops_const && is_qf_one(b->c)) { dv_free(b); return a; }
    if (a->ops == &ops_const && is_qf_zero(a->c)) {
        dv_free(a); dv_free(b); return dv_new_const_d(0.0);
    }
    if (a->ops == &ops_const && b->ops == &ops_const) {
        qfloat q = qf_div(a->c, b->c); dv_free(a); dv_free(b);
        return dv_new_const(q);
    }

    /* sinh(x)/cosh(x) → tanh(x) */
    if (a->ops == &ops_sinh && b->ops == &ops_cosh &&
        dv_struct_eq(a->a, b->a)) {
        dv_retain(a->a);
        dval_t *r = dv_tanh(a->a); dv_free(a->a); dv_free(a); dv_free(b);
        return r;
    }
    /* sin(x)/cos(x) → tan(x) */
    if (a->ops == &ops_sin && b->ops == &ops_cos &&
        dv_struct_eq(a->a, b->a)) {
        dv_retain(a->a);
        dval_t *r = dv_tan(a->a); dv_free(a->a); dv_free(a); dv_free(b);
        return r;
    }
    /* x/abs(x) → abs(x)/x  (canonical sign-function form) */
    if (b->ops == &ops_abs && dv_struct_eq(a, b->a)) {
        dval_t *r = dv_div(b, a); dv_free(a); dv_free(b); return r;
    }

    dval_t *r = dv_div(a, b); dv_free(a); dv_free(b); return r;
}

/* --- */

static dval_t *simplify_pow_d(dval_t *dv, dval_t *a)
{
    double ed = qf_to_double(dv->c);

    if (ed == 1.0) return a;
    if (ed == 0.0) { dv_free(a); return dv_new_const_d(1.0); }

    if (a->ops == &ops_const && (!a->name || !*a->name)) {
        qfloat v = qf_pow(a->c, dv->c); dv_free(a); return dv_new_const(v);
    }

    /* sqrt(x)^n → x^(n/2) */
    if (a->ops == &ops_sqrt) {
        double half = ed / 2.0;
        dv_retain(a->a); dval_t *inner = a->a; dv_free(a);
        if (half == 1.0) return inner;
        dval_t *r = dv_pow_d(inner, half); dv_free(inner); return r;
    }

    dval_t *r = dv_pow_d(a, ed); dv_free(a); return r;
}

/* --- */

static dval_t *simplify_pow(dval_t *dv, dval_t *a, dval_t *b)
{
    (void)dv;
    if (b->ops == &ops_const && is_qf_one(b->c)) { dv_free(b); return a; }
    if (b->ops == &ops_const && is_qf_zero(b->c)) {
        dv_free(a); dv_free(b); return dv_new_const_d(1.0);
    }
    dval_t *r = dv_pow(a, b); dv_free(a); dv_free(b); return r;
}

/* --- */

static dval_t *simplify_binary_special(dval_t *dv, dval_t *a, dval_t *b)
{
    if (dv->ops == &ops_hypot) {
        if (a->ops == &ops_const && is_qf_zero(a->c)) {
            dv_free(a); dval_t *r = dv_abs(b); dv_free(b); return r;
        }
        if (b->ops == &ops_const && is_qf_zero(b->c)) {
            dv_free(b); dval_t *r = dv_abs(a); dv_free(a); return r;
        }
        if (a->ops == &ops_const && (!a->name || !*a->name) &&
            b->ops == &ops_const && (!b->name || !*b->name)) {
            dval_t *tmp = dv_hypot(a, b);
            qfloat v = tmp->ops->eval(tmp);
            dv_free(tmp); dv_free(a); dv_free(b);
            return dv_new_const(v);
        }
        dval_t *r = dv_hypot(a, b); dv_free(a); dv_free(b); return r;
    }

    if (dv->ops == &ops_beta || dv->ops == &ops_logbeta) {
        if (a->ops == &ops_const && (!a->name || !*a->name) &&
            b->ops == &ops_const && (!b->name || !*b->name)) {
            dval_t *tmp = (dv->ops == &ops_beta) ? dv_beta(a,b) : dv_logbeta(a,b);
            qfloat v = tmp->ops->eval(tmp);
            dv_free(tmp); dv_free(a); dv_free(b);
            return dv_new_const(v);
        }
        dval_t *r = (dv->ops == &ops_beta) ? dv_beta(a,b) : dv_logbeta(a,b);
        dv_free(a); dv_free(b); return r;
    }

    if (a) dv_free(a);
    if (b) dv_free(b);
    dv_retain(dv);
    return dv;
}

/* ========================================================================= */
/* Main dispatcher                                                            */
/* ========================================================================= */

dval_t *dv_simplify(dval_t *dv)
{
    if (!dv) return NULL;

    if (dv->ops->arity == DV_OP_ATOM) { dv_retain(dv); return dv; }

    dval_t *a = dv->a ? dv_simplify(dv->a) : NULL;
    dval_t *b = dv->b ? dv_simplify(dv->b) : NULL;

    if (dv->ops == &ops_neg)
        return simplify_neg(a);

    if (dv->ops == &ops_add || dv->ops == &ops_sub)
        return simplify_add_sub(dv, a, b);

    if (dv->ops == &ops_mul)
        return simplify_mul(dv, a, b);

    if (dv->ops == &ops_div)
        return simplify_div(dv, a, b);

    if (dv->ops == &ops_pow_d)
        return simplify_pow_d(dv, a);

    if (dv->ops == &ops_pow)
        return simplify_pow(dv, a, b);

    if (dv->ops->arity == DV_OP_UNARY)
        return simplify_unary_fun(dv, a);

    if (dv->ops == &ops_hypot ||
        dv->ops == &ops_beta  ||
        dv->ops == &ops_logbeta)
        return simplify_binary_special(dv, a, b);

    if (a) dv_free(a);
    if (b) dv_free(b);
    dv_retain(dv);
    return dv;
}
