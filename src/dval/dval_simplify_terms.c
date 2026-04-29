#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qfloat.h"
#include "dval_internal.h"

extern dval_t *dv_simplify(dval_t *dv);

static void *dv_terms_xrealloc(void *ptr, size_t size)
{
    void *grown = realloc(ptr, size);

    if (grown)
        return grown;

    fprintf(stderr, "dval_simplify_terms: out of memory\n");
    abort();
}

#define is_qf_zero dv_qf_is_zero
#define is_qf_one dv_qf_is_one
#define is_qf_minus_one dv_qf_is_minus_one
#define is_op dv_is_op
#define is_addsub dv_is_addsub
#define is_div dv_is_div
#define is_exp_expr dv_is_exp_expr
#define is_sqrt_expr dv_is_sqrt_expr
#define is_pow_d_expr dv_is_pow_d_expr
#define is_unnamed_const dv_is_unnamed_const

static qfloat_t term_coeff(const dval_t *term, const dval_t **base)
{
    if (is_unnamed_const(term)) {
        *base = NULL;
        return term->c.re;
    }
    if (is_op(term, &ops_neg)) {
        if (is_op(term->a, &ops_mul) && is_unnamed_const(term->a->a)) {
            *base = term->a->b;
            return qf_neg(term->a->a->c.re);
        }
        *base = term->a;
        return qf_from_double(-1.0);
    }
    if (is_op(term, &ops_mul) && is_unnamed_const(term->a)) {
        *base = term->b;
        return term->a->c.re;
    }
    *base = term;
    return qf_from_double(1.0);
}

dval_t *dv_make_scaled(qfloat_t coeff, dval_t *base)
{
    if (is_qf_zero(coeff)) { dv_free(base); return dv_new_const_d(0.0); }
    if (is_qf_one(coeff))  return base;
    if (is_qf_minus_one(coeff)) {
        if (is_op(base, &ops_div) && is_op(base->a, &ops_mul) &&
            is_unnamed_const(base->a->a) &&
            qf_cmp(base->a->a->c.re, QF_ZERO) < 0) {
            qfloat_t pos_c = qf_neg(base->a->a->c.re);
            dval_t *rest = base->a->b;
            dval_t *den = base->b;
            dv_retain(rest);
            dv_retain(den);
            dv_free(base);
            dval_t *new_num = dv_make_scaled(pos_c, rest);
            dval_t *r = dv_div(new_num, den);
            dv_free(new_num);
            dv_free(den);
            return r;
        }
        if (is_op(base, &ops_div) && is_op(base->a, &ops_neg)) {
            dval_t *inner = base->a->a;
            dval_t *den = base->b;
            dv_retain(inner);
            dv_retain(den);
            dv_free(base);
            dval_t *r = dv_div(inner, den);
            dv_free(inner);
            dv_free(den);
            return r;
        }
        dval_t *r = dv_neg(base);
        dv_free(base);
        return r;
    }
    if (is_op(base, &ops_mul) && is_unnamed_const(base->a)) {
        qfloat_t folded = qf_mul(coeff, base->a->c.re);
        dv_retain(base->b);
        dval_t *rest = base->b;
        dv_free(base);
        return dv_make_scaled(folded, rest);
    }
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
        return dv_make_scaled(folded, inner);
    }
    dval_t *cn = dv_new_const(coeff);
    dval_t *r = dv_mul(cn, base);
    dv_free(cn);
    dv_free(base);
    return r;
}

static int addend_group(const dval_t *dv)
{
    if (dv->ops->arity == DV_OP_UNARY)                  return 0;
    if (is_op(dv, &ops_var))                            return 1;
    if (is_op(dv, &ops_const) && dv->name && *dv->name) return 2;
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

void dv_combine_common_denominator_addends(addend_t *terms, size_t n)
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
                sum_num = dv_make_scaled(terms[i].coeff, ibase->a);
                terms[i].coeff = QF_ONE;
                merged_any = 1;
            }

            dv_retain(jbase->a);
            j_num_term = dv_make_scaled(terms[j].coeff, jbase->a);
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

void dv_sort_addends(addend_t *terms, size_t n)
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

void dv_collect_addends(dval_t *dv, qfloat_t scale, qfloat_t *c_const,
                        addend_t **terms, size_t *n, size_t *cap)
{
    if (!dv)
        return;
    if (is_op(dv, &ops_add)) {
        dv_collect_addends(dv->a, scale, c_const, terms, n, cap);
        dv_collect_addends(dv->b, scale, c_const, terms, n, cap);
        return;
    }
    if (is_op(dv, &ops_sub)) {
        dv_collect_addends(dv->a, scale, c_const, terms, n, cap);
        dv_collect_addends(dv->b, qf_neg(scale), c_const, terms, n, cap);
        return;
    }
    if (is_op(dv, &ops_neg)) {
        if (is_addsub(dv->a)) {
            dv_collect_addends(dv->a, qf_neg(scale), c_const, terms, n, cap);
            return;
        }
        if (is_op(dv->a, &ops_mul) &&
            is_unnamed_const(dv->a->a) &&
            is_addsub(dv->a->b)) {
            qfloat_t ns = qf_mul(qf_neg(scale), dv->a->a->c.re);
            dv_collect_addends(dv->a->b, ns, c_const, terms, n, cap);
            return;
        }
    }
    if (is_op(dv, &ops_mul) &&
        is_unnamed_const(dv->a) &&
        is_addsub(dv->b)) {
        qfloat_t ns = qf_mul(scale, dv->a->c.re);
        dv_collect_addends(dv->b, ns, c_const, terms, n, cap);
        return;
    }
    if (is_op(dv, &ops_mul) &&
        is_op(dv->a, &ops_mul) &&
        is_unnamed_const(dv->a->a)) {
        qfloat_t ns = qf_mul(scale, dv->a->a->c.re);
        dval_t *raw;
        dval_t *simp;

        dv_retain(dv->a->b);
        dv_retain(dv->b);
        raw = dv_mul(dv->a->b, dv->b);
        dv_free(dv->a->b);
        dv_free(dv->b);
        simp = dv_simplify(raw);
        dv_free(raw);
        dv_collect_addends(simp, ns, c_const, terms, n, cap);
        dv_free(simp);
        return;
    }

    const dval_t *base;
    qfloat_t coeff = term_coeff(dv, &base);
    coeff = qf_mul(coeff, scale);
    if (!base) {
        *c_const = qf_add(*c_const, coeff);
        return;
    }

    for (size_t i = 0; i < *n; ++i) {
        if (dv_struct_eq((*terms)[i].base, base)) {
            (*terms)[i].coeff = qf_add((*terms)[i].coeff, coeff);
            return;
        }
    }
    if (*n == *cap) {
        *cap = *cap ? *cap * 2 : 8;
        *terms = dv_terms_xrealloc(*terms, *cap * sizeof(addend_t));
    }
    dv_retain((dval_t *)base);
    (*terms)[*n].base = (dval_t *)base;
    (*terms)[*n].coeff = coeff;
    (*n)++;
}

int dv_extract_common_addend_coeff(const addend_t *terms, size_t n,
                                   qfloat_t c_const, qfloat_t *common_out)
{
    qfloat_t common = QF_ZERO;
    int have_common = 0;
    size_t nonzero_terms = 0;

    if (!is_qf_zero(c_const))
        return 0;

    for (size_t i = 0; i < n; ++i) {
        if (!terms[i].base || is_qf_zero(terms[i].coeff))
            continue;
        if (!have_common) {
            common = terms[i].coeff;
            have_common = 1;
        } else if (!qf_eq(common, terms[i].coeff)) {
            return 0;
        }
        nonzero_terms++;
    }

    if (!have_common || nonzero_terms < 2 ||
        is_qf_one(common) || is_qf_minus_one(common))
        return 0;

    *common_out = common;
    return 1;
}

static int is_trig_square_of(const dval_t *dv, const dval_ops_t *op, const dval_t **arg_out)
{
    if (!is_pow_d_expr(dv) || !qf_eq(dv->c.re, qf_from_double(2.0)))
        return 0;
    if (!is_op(dv->a, op))
        return 0;

    *arg_out = dv->a->a;
    return 1;
}

dval_t *dv_try_trig_pythagorean_identity(const addend_t *terms, size_t n,
                                         qfloat_t c_const, qfloat_t common_coeff)
{
    const dval_t *sin_arg = NULL;
    const dval_t *cos_arg = NULL;
    size_t nonzero_terms = 0;

    if (!is_qf_zero(c_const))
        return NULL;

    for (size_t i = 0; i < n; ++i) {
        if (!terms[i].base || is_qf_zero(terms[i].coeff))
            continue;
        if (!qf_eq(terms[i].coeff, QF_ONE))
            return NULL;
        nonzero_terms++;
        if (nonzero_terms > 2)
            return NULL;
        if (is_trig_square_of(terms[i].base, &ops_sin, &sin_arg))
            continue;
        if (is_trig_square_of(terms[i].base, &ops_cos, &cos_arg))
            continue;
        return NULL;
    }

    if (nonzero_terms != 2 || !sin_arg || !cos_arg || !dv_struct_eq(sin_arg, cos_arg))
        return NULL;

    return dv_new_const(common_coeff);
}

static void flatten_add(dval_t *root, dval_t **addends, int *na, int max)
{
    dval_t *stk[64];
    int sp = 0;

    stk[sp++] = root;
    while (sp > 0 && *na < max) {
        dval_t *dv = stk[--sp];
        if (is_op(dv, &ops_add)) {
            if (sp < 63) {
                stk[sp++] = dv->b;
                stk[sp++] = dv->a;
            }
        } else {
            dv_retain(dv);
            addends[(*na)++] = dv;
        }
    }
}

static dval_t *expand_product(const dval_t *u, const dval_t *v)
{
    if (is_op(u, &ops_add)) {
        dval_t *l = expand_product(u->a, v);
        dval_t *r = expand_product(u->b, v);
        dval_t *s = dv_add(l, r);
        dv_free(l);
        dv_free(r);
        return s;
    }
    if (is_op(u, &ops_sub)) {
        dval_t *l = expand_product(u->a, v);
        dval_t *r = expand_product(u->b, v);
        dval_t *s = dv_sub(l, r);
        dv_free(l);
        dv_free(r);
        return s;
    }
    if (is_addsub(v))
        return expand_product(v, u);

    dv_retain((dval_t *)u);
    dv_retain((dval_t *)v);
    dval_t *prod = dv_mul((dval_t *)u, (dval_t *)v);
    dv_free((dval_t *)u);
    dv_free((dval_t *)v);
    return prod;
}

void dv_free_node_array(dval_t **nodes, size_t count)
{
    if (!nodes)
        return;
    for (size_t i = 0; i < count; ++i)
        dv_free(nodes[i]);
    free(nodes);
}

void dv_append_node(dval_t ***nodes, size_t *count, size_t *cap, dval_t *node)
{
    if (*count == *cap) {
        *cap = (*cap == 0) ? 4 : (*cap * 2);
        *nodes = dv_terms_xrealloc(*nodes, *cap * sizeof(**nodes));
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

dval_t *dv_make_pow_like(dval_t *base, qfloat_t exponent)
{
    if (qf_eq(exponent, QF_ZERO)) {
        dv_free(base);
        return dv_new_const_d(1.0);
    }
    if (qf_eq(exponent, QF_ONE))
        return base;

    dval_t *pow = dv_pow_qf(base, exponent);
    dv_free(base);
    return pow;
}

void dv_split_division_terms(qfloat_t *c_acc, int *is_zero,
                             dval_t **terms, size_t nterms,
                             dval_t ***den_terms, size_t *nden_terms,
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

        dv_append_node(den_terms, nden_terms, den_cap, den);
    }
}

void dv_combine_like_powers(dval_t **terms, size_t nterms)
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
        terms[i] = dv_make_pow_like(base, exponent);
    }
}

void dv_combine_exp_terms(dval_t **terms, size_t nterms)
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

void dv_merge_sqrt_terms(dval_t **terms, size_t nterms)
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

dval_t *dv_try_expand_shallow_product(qfloat_t c_acc,
                                      dval_t **terms, size_t nterms,
                                      dval_t **den_terms, size_t nden_terms)
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

    dval_t *t0 = terms[first];
    dval_t *t1 = terms[second];
    if (!(is_addsub(t0) && is_addsub(t1)))
        return NULL;

    int share = 0;
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

    if (!share ||
        is_addsub(t0->a) || is_addsub(t0->b) ||
        is_addsub(t1->a) || is_addsub(t1->b))
        return NULL;

    dval_t *expanded = expand_product(t0, t1);
    dval_t *simp;

    free(den_terms);
    free(terms);
    dv_free(t0);
    dv_free(t1);

    simp = dv_simplify(expanded);
    dv_free(expanded);
    return dv_make_scaled(c_acc, simp);
}

dval_t *dv_rebuild_product_chain(qfloat_t c_acc, dval_t **terms, size_t nterms)
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

dval_t *dv_rebuild_division_chain(dval_t **den_terms, size_t nden_terms)
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
