/* dval_simplify.c - algebraic simplification of differentiable value nodes
 *
 * dv_simplify() rewrites a DAG node into a canonical form using a small set
 * of structural rules applied bottom-up.  It is called automatically by
 * dv_create_deriv() so that derivative expressions stay readable.
 *
 * Rules applied (in order):
 *   1. Constant folding — a sub-tree with no variable leaves is replaced by
 *      a single const node holding the evaluated value.
 *   2. Identity removal — x+0, x*1, x^1, neg(neg(x)), etc. are collapsed.
 *   3. Multiplication flattening — a chain of mul/neg nodes is collected into
 *      (scalar_coefficient * term₁ * term₂ * ...) with the coefficient folded
 *      into a single leading const node.
 *   4. Like-term collection in additions — terms with the same symbolic
 *      structure have their coefficients summed, e.g. 2x + 3x → 5x.
 *
 * dv_simplify() returns a new owning node (refcount = 1).  The input node is
 * borrowed; its refcount is not changed.
 */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qfloat.h"
#include "dval_internal.h"

/* forward declaration — helpers below call dv_simplify recursively */
dval_t *dv_simplify(dval_t *dv);

static void *dv_xrealloc(void *ptr, size_t size)
{
    void *grown = realloc(ptr, size);

    if (grown)
        return grown;

    fprintf(stderr, "dval_simplify: out of memory\n");
    abort();
}

/* ========================================================================= */
/* Scalar predicates                                                          */
/* ========================================================================= */

static int is_qf_zero(qfloat_t x) { return qf_eq(x, QF_ZERO); }
static int is_qf_one (qfloat_t x) { return qf_eq(x, QF_ONE); }
static int is_qf_minus_one(qfloat_t x) { return qf_eq(x, qf_neg(QF_ONE)); }
static int is_op(const dval_t *dv, const dval_ops_t *ops) { return dv && dv->ops == ops; }
static int is_addsub(const dval_t *dv) { return is_op(dv, &ops_add) || is_op(dv, &ops_sub); }
static int is_div(const dval_t *dv) { return is_op(dv, &ops_div); }
static int is_exp_expr(const dval_t *dv) { return is_op(dv, &ops_exp); }
static int is_sqrt_expr(const dval_t *dv) { return is_op(dv, &ops_sqrt); }
static int is_pow_d_expr(const dval_t *dv) { return is_op(dv, &ops_pow_d); }
static int is_unnamed_const(const dval_t *dv)
{
    return is_op(dv, &ops_const) && (!dv->name || !*dv->name);
}

int dv_fold_zero_to_zero(qfloat_t in, qfloat_t *out)
{
    if (!qf_eq(in, QF_ZERO))
        return 0;
    *out = QF_ZERO;
    return 1;
}

int dv_fold_cos_const(qfloat_t in, qfloat_t *out)
{
    if (!qf_eq(in, QF_ZERO))
        return 0;
    *out = QF_ONE;
    return 1;
}

int dv_fold_exp_const(qfloat_t in, qfloat_t *out)
{
    if (!qf_eq(in, QF_ZERO))
        return 0;
    *out = QF_ONE;
    return 1;
}

int dv_fold_log_const(qfloat_t in, qfloat_t *out)
{
    if (!qf_eq(in, QF_ONE))
        return 0;
    *out = QF_ZERO;
    return 1;
}

int dv_fold_sqrt_const(qfloat_t in, qfloat_t *out)
{
    if (qf_eq(in, QF_ZERO)) {
        *out = QF_ZERO;
        return 1;
    }
    if (qf_eq(in, QF_ONE)) {
        *out = QF_ONE;
        return 1;
    }
    return 0;
}

/* ========================================================================= */
/* Structural equality                                                        */
/* ========================================================================= */

static int dv_struct_eq(const dval_t *u, const dval_t *v)
{
    if (u == v) return 1;
    if (u->ops != v->ops) return 0;
    if (is_op(u, &ops_const))
        return qf_eq(u->c.re, v->c.re);
    if (is_op(u, &ops_var))
        return u == v; /* identity */
    if (is_op(u, &ops_neg))
        return dv_struct_eq(u->a, v->a);
    if (is_op(u, &ops_pow_d))
        return dv_struct_eq(u->a, v->a) &&
               qf_eq(u->c.re, v->c.re);
    return dv_struct_eq(u->a, v->a) && dv_struct_eq(u->b, v->b);
}

/* ========================================================================= */
/* Multiplication flattening                                                  */
/* ========================================================================= */

static void collect_mul_flat(
    dval_t *dv,
    qfloat_t *c_acc, int *is_zero,
    dval_t ***terms, size_t *nterms, size_t *cap)
{
    if (*is_zero) return;

    if (is_unnamed_const(dv)) {
        if (is_qf_zero(dv->c.re)) { *is_zero = 1; return; }
        *c_acc = qf_mul(*c_acc, dv->c.re);
        return;
    }
    if (is_op(dv, &ops_neg)) {
        *c_acc = qf_neg(*c_acc);
        collect_mul_flat(dv->a, c_acc, is_zero, terms, nterms, cap);
        return;
    }
    if (is_op(dv, &ops_mul)) {
        collect_mul_flat(dv->a, c_acc, is_zero, terms, nterms, cap);
        collect_mul_flat(dv->b, c_acc, is_zero, terms, nterms, cap);
        return;
    }
    if (*nterms == *cap) {
        *cap   = (*cap == 0 ? 4 : *cap * 2);
        *terms = dv_xrealloc(*terms, *cap * sizeof(dval_t *));
    }
    dv_retain(dv);
    (*terms)[(*nterms)++] = dv;
}

/* ========================================================================= */
/* Like-term helpers for addition/subtraction                                */
/* ========================================================================= */

static qfloat_t term_coeff(const dval_t *term, const dval_t **base)
{
    /* unnamed pure constant: numeric only, no symbolic base */
    if (is_unnamed_const(term)) {
        *base = NULL;
        return term->c.re;
    }
    /* neg(c·rest): coefficient -c */
    if (is_op(term, &ops_neg)) {
        if (is_op(term->a, &ops_mul) && is_unnamed_const(term->a->a)) {
            *base = term->a->b;
            return qf_neg(term->a->a->c.re);
        }
        *base = term->a;
        return qf_from_double(-1.0);
    }
    /* mul(unnamed_const, rest): coefficient is the leading const */
    if (is_op(term, &ops_mul) && is_unnamed_const(term->a)) {
        *base = term->b;
        return term->a->c.re;
    }
    *base = term;
    return qf_from_double(1.0);
}

/* Build  coeff * base,  consuming the retained base reference. */
static dval_t *make_scaled(qfloat_t coeff, dval_t *base)
{
    if (is_qf_zero(coeff)) { dv_free(base); return dv_new_const_d(0.0); }
    if (is_qf_one(coeff))  return base;
    if (is_qf_minus_one(coeff)) {
        /* neg((-c·rest)/den) → (c·rest)/den — eliminates double negative */
        if (is_op(base, &ops_div) && is_op(base->a, &ops_mul) &&
            is_unnamed_const(base->a->a) &&
            qf_cmp(base->a->a->c.re, QF_ZERO) < 0) {
            qfloat_t    pos_c   = qf_neg(base->a->a->c.re);
            dval_t   *rest    = base->a->b;
            dval_t   *den     = base->b;
            dv_retain(rest); dv_retain(den); dv_free(base);
            dval_t *new_num   = make_scaled(pos_c, rest);
            dval_t *r         = dv_div(new_num, den);
            dv_free(new_num); dv_free(den); return r;
        }
        /* neg(neg(x)/den) → x/den */
        if (is_op(base, &ops_div) && is_op(base->a, &ops_neg)) {
            dval_t *inner = base->a->a;
            dval_t *den   = base->b;
            dv_retain(inner); dv_retain(den); dv_free(base);
            dval_t *r = dv_div(inner, den);
            dv_free(inner); dv_free(den); return r;
        }
        dval_t *r = dv_neg(base); dv_free(base); return r;
    }
    /* fold: coeff * (c * rest) → (coeff*c) * rest */
    if (is_op(base, &ops_mul) && is_unnamed_const(base->a)) {
        qfloat_t folded = qf_mul(coeff, base->a->c.re);
        dv_retain(base->b);
        dval_t *rest = base->b;
        dv_free(base);
        return make_scaled(folded, rest);
    }
    /* fold: coeff * (mul(c,a) * b) → (coeff*c) * mul(a,b)
     * handles one extra nesting level, e.g. (2x)*exp(-x) */
    if (is_op(base, &ops_mul) &&
        is_op(base->a, &ops_mul) &&
        is_unnamed_const(base->a->a)) {
        qfloat_t folded = qf_mul(coeff, base->a->a->c.re);
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

typedef struct { dval_t *base; qfloat_t coeff; } addend_t;

static int addend_group(const dval_t *dv);
static const char *addend_sort_name(const dval_t *dv);

static void combine_common_denominator_addends(addend_t *terms, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        dval_t *ibase = terms[i].base;
        dval_t *sum_num = NULL;
        dval_t *simp_num = NULL;
        dval_t *combined = NULL;
        int merged_any = 0;

        if (!is_div(ibase))
            continue;

        for (size_t j = i + 1; j < n; ++j) {
            dval_t *jbase = terms[j].base;
            dval_t *i_num_term;
            dval_t *j_num_term;
            dval_t *tmp;

            if (!is_div(jbase))
                continue;
            if (!dv_struct_eq(ibase->b, jbase->b))
                continue;

            if (!merged_any) {
                dv_retain(ibase->a);
                sum_num = make_scaled(terms[i].coeff, ibase->a);
                terms[i].coeff = QF_ONE;
                merged_any = 1;
            }

            dv_retain(jbase->a);
            j_num_term = make_scaled(terms[j].coeff, jbase->a);
            i_num_term = sum_num;
            tmp = dv_add(i_num_term, j_num_term);
            dv_free(i_num_term);
            dv_free(j_num_term);
            sum_num = dv_simplify(tmp);
            dv_free(tmp);

            dv_free(jbase);
            terms[j].base = NULL;
            terms[j].coeff = QF_ZERO;
        }

        if (!merged_any)
            continue;

        dv_retain(ibase->b);
        simp_num = dv_simplify(sum_num);
        dv_free(sum_num);
        combined = dv_div(simp_num, ibase->b);
        dv_free(simp_num);
        dv_free(ibase->b);
        {
            dval_t *combined_raw = combined;
            combined = dv_simplify(combined_raw);
            dv_free(combined_raw);
        }
        dv_free(ibase);
        terms[i].base = combined;
        terms[i].coeff = QF_ONE;
    }
}

static int compare_addends(const addend_t *lhs, const addend_t *rhs)
{
    int lg, rg;

    if (!lhs->base && !rhs->base)
        return 0;
    if (!lhs->base)
        return 1;
    if (!rhs->base)
        return -1;

    lg = addend_group(lhs->base);
    rg = addend_group(rhs->base);
    if (lg != rg)
        return lg - rg;

    return strcmp(addend_sort_name(lhs->base), addend_sort_name(rhs->base));
}

static void sort_addends(addend_t *terms, size_t n)
{
    for (size_t i = 1; i < n; ++i) {
        addend_t key = terms[i];
        size_t j = i;

        while (j > 0 && compare_addends(&terms[j - 1], &key) > 0) {
            terms[j] = terms[j - 1];
            --j;
        }
        terms[j] = key;
    }
}

static void collect_addends(
    dval_t *dv, qfloat_t scale,
    qfloat_t *c_const,
    addend_t **terms, size_t *n, size_t *cap)
{
    if (!dv) return;
    if (is_op(dv, &ops_add)) {
        collect_addends(dv->a, scale,           c_const, terms, n, cap);
        collect_addends(dv->b, scale,           c_const, terms, n, cap);
        return;
    }
    if (is_op(dv, &ops_sub)) {
        collect_addends(dv->a, scale,           c_const, terms, n, cap);
        collect_addends(dv->b, qf_neg(scale),   c_const, terms, n, cap);
        return;
    }
    if (is_op(dv, &ops_neg)) {
        if (is_addsub(dv->a)) {
            collect_addends(dv->a, qf_neg(scale), c_const, terms, n, cap);
            return;
        }
        /* neg(c·(a ± b)) — strip neg and distribute */
        if (is_op(dv->a, &ops_mul) &&
            is_unnamed_const(dv->a->a) &&
            is_addsub(dv->a->b)) {
            qfloat_t ns = qf_mul(qf_neg(scale), dv->a->a->c.re);
            collect_addends(dv->a->b, ns, c_const, terms, n, cap);
            return;
        }
    }
    /* distribute: c · (a ± b) → c·a ± c·b */
    if (is_op(dv, &ops_mul) &&
        is_unnamed_const(dv->a) &&
        is_addsub(dv->b)) {
        qfloat_t ns = qf_mul(scale, dv->a->c.re);
        collect_addends(dv->b, ns, c_const, terms, n, cap);
        return;
    }
    const dval_t *base;
    qfloat_t coeff = term_coeff(dv, &base);
    coeff = qf_mul(coeff, scale);
    if (!base) { *c_const = qf_add(*c_const, coeff); return; }

    for (size_t i = 0; i < *n; ++i) {
        if (dv_struct_eq((*terms)[i].base, base)) {
            (*terms)[i].coeff = qf_add((*terms)[i].coeff, coeff);
            return;
        }
    }
    if (*n == *cap) {
        *cap   = *cap ? *cap * 2 : 8;
        *terms = dv_xrealloc(*terms, *cap * sizeof(addend_t));
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
        if (is_op(dv, &ops_add)) {
            if (sp < 63) { stk[sp++] = dv->b; stk[sp++] = dv->a; }
        } else { dv_retain(dv); addends[(*na)++] = dv; }
    }
}

static int addend_group(const dval_t *dv)
{
    if (dv->ops->arity == DV_OP_UNARY)                    return 0;
    if (is_op(dv, &ops_var))                              return 1;
    if (is_op(dv, &ops_const) && dv->name && *dv->name)   return 2;
    return 3;
}

static const char *addend_sort_name(const dval_t *dv)
{
    if (is_op(dv, &ops_var) || is_op(dv, &ops_const))
        return dv->name ? dv->name : "";
    if (dv->a && is_op(dv->a, &ops_var))
        return dv->a->name ? dv->a->name : "";
    return "";
}

/* ========================================================================= */
/* Polynomial expansion: expand u*v into a sum (non-owning args)             */
/* ========================================================================= */

static dval_t *expand_product(const dval_t *u, const dval_t *v)
{
    if (is_op(u, &ops_add)) {
        dval_t *l = expand_product(u->a, v);
        dval_t *r = expand_product(u->b, v);
        dval_t *s = dv_add(l, r); dv_free(l); dv_free(r);
        return s;
    }
    if (is_op(u, &ops_sub)) {
        dval_t *l = expand_product(u->a, v);
        dval_t *r = expand_product(u->b, v);
        dval_t *s = dv_sub(l, r); dv_free(l); dv_free(r);
        return s;
    }
    if (is_addsub(v))
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
/* Product rebuild helpers                                                   */
/* ========================================================================= */

static void free_node_array(dval_t **nodes, size_t count)
{
    if (!nodes)
        return;
    for (size_t i = 0; i < count; ++i)
        dv_free(nodes[i]);
    free(nodes);
}

static void append_node(dval_t ***nodes, size_t *count, size_t *cap, dval_t *node)
{
    if (*count == *cap) {
        *cap = (*cap == 0) ? 4 : (*cap * 2);
        *nodes = dv_xrealloc(*nodes, *cap * sizeof(**nodes));
    }
    (*nodes)[(*count)++] = node;
}

static qfloat_t pow_exponent(const dval_t *dv)
{
    return is_op(dv, &ops_pow_d) ? dv->c.re : QF_ONE;
}

static dval_t *pow_base(const dval_t *dv)
{
    return is_op(dv, &ops_pow_d) ? dv->a : (dval_t *)dv;
}

static dval_t *make_pow_like(dval_t *base, qfloat_t exponent)
{
    if (qf_eq(exponent, QF_ZERO)) {
        dv_free(base);
        return dv_new_const_d(1.0);
    }
    if (qf_eq(exponent, QF_ONE))
        return base;

    {
        dval_t *pow = dv_pow_qf(base, exponent);
        dv_free(base);
        return pow;
    }
}

static void split_division_terms(qfloat_t *c_acc,
                                 int *is_zero,
                                 dval_t **terms,
                                 size_t nterms,
                                 dval_t ***den_terms,
                                 size_t *nden_terms,
                                 size_t *den_cap)
{
    for (size_t i = 0; i < nterms; ++i) {
        dval_t *term = terms[i];
        dval_t *num;
        dval_t *den;

        if (!is_div(term))
            continue;

        num = term->a;
        den = term->b;
        dv_retain(num);
        dv_retain(den);
        dv_free(term);
        terms[i] = NULL;

        if (is_unnamed_const(num)) {
            if (is_qf_zero(num->c.re))
                *is_zero = 1;
            else
                *c_acc = qf_mul(*c_acc, num->c.re);
            dv_free(num);
        } else {
            terms[i] = num;
        }

        if (*is_zero) {
            dv_free(den);
            continue;
        }

        if (is_unnamed_const(den)) {
            *c_acc = qf_div(*c_acc, den->c.re);
            dv_free(den);
            continue;
        }

        append_node(den_terms, nden_terms, den_cap, den);
    }
}

static void combine_like_powers(dval_t **terms, size_t nterms)
{
    for (size_t i = 0; i < nterms; ++i) {
        dval_t *term = terms[i];
        dval_t *base;
        qfloat_t exponent;

        if (!term)
            continue;

        base = pow_base(term);
        exponent = pow_exponent(term);

        for (size_t j = i + 1; j < nterms; ++j) {
            dval_t *other = terms[j];

            if (!other)
                continue;

            if (!dv_struct_eq(base, pow_base(other)))
                continue;

            exponent = qf_add(exponent, pow_exponent(other));
            dv_free(other);
            terms[j] = NULL;
        }

        if (qf_eq(exponent, QF_ONE) && !is_pow_d_expr(term))
            continue;

        dv_retain(base);
        dv_free(term);
        terms[i] = make_pow_like(base, exponent);
    }
}

static void combine_exp_terms(dval_t **terms, size_t nterms)
{
    for (size_t i = 0; i < nterms; ++i) {
        if (!is_exp_expr(terms[i]))
            continue;

        for (size_t j = i + 1; j < nterms; ++j) {
            dval_t *addends[64];
            int na = 0;
            dval_t *sum;
            dval_t *simp;
            dval_t *combined;

            if (!is_exp_expr(terms[j]))
                continue;

            flatten_add(terms[i]->a, addends, &na, 64);
            flatten_add(terms[j]->a, addends, &na, 64);
            dv_free(terms[i]);
            dv_free(terms[j]);
            terms[j] = NULL;

            for (int s = 1; s < na; ++s) {
                dval_t *key = addends[s];
                int kg = addend_group(key);
                int t = s - 1;

                while (t >= 0) {
                    int tg = addend_group(addends[t]);
                    int cmp = (tg != kg) ? (tg - kg)
                                         : strcmp(addend_sort_name(addends[t]),
                                                  addend_sort_name(key));
                    if (cmp <= 0)
                        break;
                    addends[t + 1] = addends[t];
                    --t;
                }
                addends[t + 1] = key;
            }

            sum = addends[0];
            for (int k = 1; k < na; ++k) {
                dval_t *tmp = dv_add(sum, addends[k]);
                dv_free(sum);
                dv_free(addends[k]);
                sum = tmp;
            }

            simp = dv_simplify(sum);
            dv_free(sum);
            combined = dv_exp(simp);
            dv_free(simp);
            terms[i] = dv_simplify(combined);
            dv_free(combined);
        }
    }
}

static void merge_sqrt_terms(dval_t **terms, size_t nterms)
{
    for (size_t i = 0; i < nterms; ++i) {
        if (!is_sqrt_expr(terms[i]))
            continue;

        for (size_t j = i + 1; j < nterms; ++j) {
            dval_t *prod;
            dval_t *simp_arg;
            dval_t *raw;

            if (!is_sqrt_expr(terms[j]))
                continue;

            dv_retain(terms[i]->a);
            dv_retain(terms[j]->a);
            prod = dv_mul(terms[i]->a, terms[j]->a);
            dv_free(terms[i]->a);
            dv_free(terms[j]->a);
            simp_arg = dv_simplify(prod);
            dv_free(prod);
            raw = dv_sqrt(simp_arg);
            dv_free(simp_arg);
            dv_free(terms[i]);
            dv_free(terms[j]);
            terms[j] = NULL;
            terms[i] = dv_simplify(raw);
            dv_free(raw);
            break;
        }
    }
}

static dval_t *try_expand_shallow_product(qfloat_t c_acc,
                                          dval_t **terms,
                                          size_t nterms,
                                          dval_t **den_terms,
                                          size_t nden_terms)
{
    size_t first = nterms;
    size_t second = nterms;
    int too_many = 0;

    if (nden_terms != 0)
        return NULL;

    for (size_t i = 0; i < nterms; ++i) {
        if (!terms[i])
            continue;
        if (first == nterms)
            first = i;
        else if (second == nterms)
            second = i;
        else {
            too_many = 1;
            break;
        }
    }

    if (too_many || first == nterms || second == nterms)
        return NULL;

    {
        dval_t *t0 = terms[first];
        dval_t *t1 = terms[second];
        int t0_add = is_addsub(t0);
        int t1_add = is_addsub(t1);
        int share = 0;

        if (!(t0_add && t1_add))
            return NULL;

        {
            const dval_t *t0c[2] = { t0->a, t0->b };
            const dval_t *t1c[2] = { t1->a, t1->b };

            for (int p = 0; p < 2 && !share; ++p) {
                for (int q = 0; q < 2 && !share; ++q) {
                    const dval_t *u = t0c[p];
                    const dval_t *v = t1c[q];

                    if (is_unnamed_const(u) || is_unnamed_const(v))
                        continue;
                    if (dv_struct_eq(u, v))
                        share = 1;
                }
            }
        }

        if (!share ||
            is_addsub(t0->a) || is_addsub(t0->b) ||
            is_addsub(t1->a) || is_addsub(t1->b))
            return NULL;

        {
            dval_t *expanded = expand_product(t0, t1);
            dval_t *simp;

            free(den_terms);
            free(terms);
            dv_free(t0);
            dv_free(t1);

            simp = dv_simplify(expanded);
            dv_free(expanded);
            return make_scaled(c_acc, simp);
        }
    }
}

static dval_t *rebuild_product_chain(qfloat_t c_acc, dval_t **terms, size_t nterms)
{
    dval_t *cur = NULL;

    if (!is_qf_one(c_acc))
        cur = dv_new_const(c_acc);

    for (size_t i = 0; i < nterms; ++i) {
        if (!terms[i])
            continue;
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
    return cur ? cur : dv_new_const(c_acc);
}

static dval_t *rebuild_division_chain(dval_t **den_terms, size_t nden_terms)
{
    dval_t *denom = NULL;

    for (size_t i = 0; i < nden_terms; ++i) {
        if (!den_terms[i])
            continue;
        if (!denom) {
            denom = den_terms[i];
        } else {
            dval_t *tmp = dv_mul(denom, den_terms[i]);
            dv_free(denom);
            dv_free(den_terms[i]);
            denom = tmp;
        }
    }

    free(den_terms);
    return denom;
}

/* ========================================================================= */
/* Unary function simplification                                             */
/* ========================================================================= */

dval_t *dv_simplify_passthrough(const dval_t *dv, dval_t *a, dval_t *b)
{
    if (a)
        dv_free(a);
    if (b)
        dv_free(b);
    dv_retain((dval_t *)dv);
    return (dval_t *)dv;
}

dval_t *dv_simplify_unary_operator(const dval_t *dv, dval_t *a, dval_t *b)
{
    (void)b;

    /* exp(log(x)) -> x, log(exp(x)) -> x */
    if ((is_exp_expr(dv) && is_op(a, &ops_log)) ||
        (is_op(dv, &ops_log) && is_exp_expr(a))) {
        dval_t *inner = a->a;
        dv_retain(inner);
        dv_free(a);
        return inner;
    }

    if (is_op(a, &ops_const)) {
        qfloat_t folded;

        if (dv->ops->fold_const_unary && dv->ops->fold_const_unary(a->c.re, &folded)) {
            dv_free(a);
            return dv_new_const(folded);
        }

        if ((!a->name || !*a->name) && dv->ops->apply_unary) {
            dval_t *tmp = dv->ops->apply_unary(a);
            qfloat_t v = tmp->ops->eval(tmp).re;
            dv_free(tmp);
            dv_free(a);
            return dv_new_const(v);
        }
    }

    if (dv->ops->apply_unary) {
        dval_t *out = dv->ops->apply_unary(a);
        dv_free(a);
        return out;
    }

    dv_free(a);
    dv_retain((dval_t *)dv);
    return (dval_t *)dv;
}

dval_t *dv_simplify_binary_operator(const dval_t *dv, dval_t *a, dval_t *b)
{
    if ((!a || !b) || !dv->ops->apply_binary)
        return dv_simplify_passthrough(dv, a, b);

    if (is_unnamed_const(a) && is_unnamed_const(b)) {
        dval_t *tmp = dv->ops->apply_binary(a, b);
        qfloat_t v = tmp->ops->eval(tmp).re;
        dv_free(tmp);
        dv_free(a);
        dv_free(b);
        return dv_new_const(v);
    }

    dval_t *out = dv->ops->apply_binary(a, b);
    dv_free(a);
    dv_free(b);
    return out;
}

/* ========================================================================= */
/* Per-operation simplifiers                                                 */
/* ========================================================================= */

dval_t *dv_simplify_neg_operator(const dval_t *dv, dval_t *a, dval_t *b)
{
    (void)dv;
    (void)b;
    /* --x → x */
    if (is_op(a, &ops_neg)) {
        dval_t *inner = a->a; dv_retain(inner); dv_free(a); return inner;
    }
    /* neg(c) → -c */
    if (is_op(a, &ops_const)) {
        qfloat_t c = qf_neg(a->c.re); dv_free(a); return dv_new_const(c);
    }
    /* neg(c·x) where c < 0 → |c|·x  (eliminates spurious double-negative) */
    if (is_op(a, &ops_mul) &&
        is_op(a->a, &ops_const) &&
        (!a->a->name || !*a->a->name) &&
        qf_cmp(a->a->c.re, QF_ZERO) < 0) {
        qfloat_t pos_c = qf_neg(a->a->c.re);
        dv_retain(a->b);
        dval_t *rest = a->b;
        dv_free(a);
        return make_scaled(pos_c, rest);
    }
    dval_t *r = dv_neg(a); dv_free(a); return r;
}

/* --- */

dval_t *dv_simplify_add_sub_operator(const dval_t *dv, dval_t *a, dval_t *b)
{
    qfloat_t    c_const = qf_from_double(0.0);
    addend_t *terms   = NULL;
    size_t    n = 0, cap = 0;

    collect_addends(a, qf_from_double(1.0), &c_const, &terms, &n, &cap);
    dv_free(a);
    collect_addends(b, is_op(dv, &ops_sub) ? qf_from_double(-1.0) : qf_from_double(1.0), &c_const, &terms, &n, &cap);
    dv_free(b);

    combine_common_denominator_addends(terms, n);
    sort_addends(terms, n);

    /* find the leading non-cancelled symbolic term */
    int leading_neg = 0;
    for (size_t i = 0; i < n; ++i) {
        if (!is_qf_zero(terms[i].coeff)) {
            leading_neg = qf_cmp(terms[i].coeff, QF_ZERO) < 0;
            break;
        }
    }

    dval_t *cur = NULL;

    /* emit a positive constant before any leading negative symbolic term so
     * the expression reads "1 - tanh²(x)" rather than "-tanh²(x) + 1" */
    int const_emitted = 0;
    if (!is_qf_zero(c_const) && qf_cmp(c_const, QF_ZERO) > 0 && leading_neg) {
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

dval_t *dv_simplify_mul_operator(const dval_t *dv, dval_t *a, dval_t *b)
{
    dval_t **terms = NULL;
    dval_t **den_terms = NULL;
    size_t nterms = 0, term_cap = 0;
    size_t nden_terms = 0, den_cap = 0;
    qfloat_t c_acc = QF_ONE;
    int is_zero = 0;
    dval_t *expanded;
    dval_t *numerator;
    dval_t *denominator;
    dval_t *division;

    (void)dv;

    if ((is_op(a, &ops_const) && is_qf_zero(a->c.re)) ||
        (is_op(b, &ops_const) && is_qf_zero(b->c.re))) {
        dv_free(a);
        dv_free(b);
        return dv_new_const_d(0.0);
    }
    if (is_op(a, &ops_const) && is_qf_one(a->c.re)) {
        dv_free(a);
        return b;
    }
    if (is_op(b, &ops_const) && is_qf_one(b->c.re)) {
        dv_free(b);
        return a;
    }

    collect_mul_flat(a, &c_acc, &is_zero, &terms, &nterms, &term_cap);
    collect_mul_flat(b, &c_acc, &is_zero, &terms, &nterms, &term_cap);
    dv_free(a);
    dv_free(b);

    if (is_zero) {
        free_node_array(terms, nterms);
        return dv_new_const_d(0.0);
    }

    split_division_terms(&c_acc, &is_zero, terms, nterms,
                         &den_terms, &nden_terms, &den_cap);

    if (is_zero) {
        free_node_array(terms, nterms);
        free_node_array(den_terms, nden_terms);
        return dv_new_const_d(0.0);
    }

    combine_like_powers(den_terms, nden_terms);
    combine_like_powers(terms, nterms);
    combine_exp_terms(terms, nterms);
    merge_sqrt_terms(terms, nterms);
    merge_sqrt_terms(den_terms, nden_terms);

    expanded = try_expand_shallow_product(c_acc, terms, nterms, den_terms, nden_terms);
    if (expanded)
        return expanded;

    numerator = rebuild_product_chain(c_acc, terms, nterms);
    if (nden_terms == 0) {
        free(den_terms);
        return numerator;
    }

    denominator = rebuild_division_chain(den_terms, nden_terms);
    division = dv_div(numerator, denominator);
    dv_free(numerator);
    dv_free(denominator);
    numerator = dv_simplify(division);
    dv_free(division);
    return numerator;
}

/* --- */

dval_t *dv_simplify_div_operator(const dval_t *dv, dval_t *a, dval_t *b)
{
    (void)dv;

    if (is_op(b, &ops_const) && is_qf_one(b->c.re)) { dv_free(b); return a; }
    if (dv_struct_eq(a, b)) {
        dv_free(a);
        dv_free(b);
        return dv_new_const_d(1.0);
    }
    if (is_op(a, &ops_const) && is_qf_zero(a->c.re)) {
        dv_free(a); dv_free(b); return dv_new_const_d(0.0);
    }
    if (is_op(a, &ops_const) && is_op(b, &ops_const)) {
        qfloat_t q = qf_div(a->c.re, b->c.re); dv_free(a); dv_free(b);
        return dv_new_const(q);
    }

    /* sinh(x)/cosh(x) → tanh(x) */
    if (is_op(a, &ops_sinh) && is_op(b, &ops_cosh) &&
        dv_struct_eq(a->a, b->a)) {
        dv_retain(a->a);
        dval_t *r = dv_tanh(a->a); dv_free(a->a); dv_free(a); dv_free(b);
        return r;
    }
    /* sin(x)/cos(x) → tan(x) */
    if (is_op(a, &ops_sin) && is_op(b, &ops_cos) &&
        dv_struct_eq(a->a, b->a)) {
        dv_retain(a->a);
        dval_t *r = dv_tan(a->a); dv_free(a->a); dv_free(a); dv_free(b);
        return r;
    }
    /* x/abs(x) → abs(x)/x  (canonical sign-function form) */
    if (is_op(b, &ops_abs) && dv_struct_eq(a, b->a)) {
        dval_t *r = dv_div(b, a); dv_free(a); dv_free(b); return r;
    }

    dval_t *r = dv_div(a, b); dv_free(a); dv_free(b); return r;
}

/* --- */

dval_t *dv_simplify_pow_d_operator(const dval_t *dv, dval_t *a, dval_t *b)
{
    (void)b;
    qfloat_t exponent = dv->c.re;

    if (qf_eq(exponent, QF_ONE))
        return a;
    if (qf_eq(exponent, QF_ZERO)) {
        dv_free(a);
        return dv_new_const_d(1.0);
    }

    if (is_unnamed_const(a)) {
        qfloat_t v = qf_pow(a->c.re, exponent);
        dv_free(a);
        return dv_new_const(v);
    }

    /* sqrt(x)^n → x^(n/2) */
    if (is_sqrt_expr(a)) {
        qfloat_t half = qf_div(exponent, qf_from_double(2.0));
        dv_retain(a->a);
        dval_t *inner = a->a;
        dv_free(a);
        return make_pow_like(inner, half);
    }

    return make_pow_like(a, exponent);
}

/* --- */

dval_t *dv_simplify_pow_operator(const dval_t *dv, dval_t *a, dval_t *b)
{
    (void)dv;
    if (is_op(b, &ops_const) && is_qf_one(b->c.re)) { dv_free(b); return a; }
    if (is_op(b, &ops_const) && is_qf_zero(b->c.re)) {
        dv_free(a); dv_free(b); return dv_new_const_d(1.0);
    }
    dval_t *r = dv_pow(a, b); dv_free(a); dv_free(b); return r;
}

/* --- */

dval_t *dv_simplify_hypot_operator(const dval_t *dv, dval_t *a, dval_t *b)
{
    (void)dv;

    if (is_op(a, &ops_const) && is_qf_zero(a->c.re)) {
        dv_free(a);
        dval_t *r = dv_abs(b);
        dv_free(b);
        return r;
    }
    if (is_op(b, &ops_const) && is_qf_zero(b->c.re)) {
        dv_free(b);
        dval_t *r = dv_abs(a);
        dv_free(a);
        return r;
    }

    return dv_simplify_binary_operator(dv, a, b);
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

    if (dv->ops->simplify)
        return dv->ops->simplify(dv, a, b);

    return dv_simplify_passthrough(dv, a, b);
}
