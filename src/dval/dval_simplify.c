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
static int is_op(const dval_t *dv, const dval_ops_t *ops) { return dv && dv->ops == ops; }
static int is_addsub(const dval_t *dv) { return is_op(dv, &ops_add) || is_op(dv, &ops_sub); }
static int is_exp_expr(const dval_t *dv) { return is_op(dv, &ops_exp); }
static int is_sqrt_expr(const dval_t *dv) { return is_op(dv, &ops_sqrt); }
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

    if (is_op(dv, &ops_sqrt) &&
        is_op(a, &ops_mul) &&
        is_unnamed_const(a->a) &&
        qf_cmp(a->a->c.re, QF_ZERO) > 0) {
        qfloat_t coeff_root = qf_sqrt(a->a->c.re);

        if (qf_eq(qf_mul(coeff_root, coeff_root), a->a->c.re)) {
            dval_t *raw;
            dval_t *simp;

            dv_retain(a->b);
            raw = dv_sqrt(a->b);
            dv_free(a->b);
            simp = dv_simplify(raw);
            dv_free(raw);
            dv_free(a);
            return dv_make_scaled(coeff_root, simp);
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
        return dv_make_scaled(pos_c, rest);
    }
    dval_t *r = dv_neg(a); dv_free(a); return r;
}

/* --- */

dval_t *dv_simplify_add_sub_operator(const dval_t *dv, dval_t *a, dval_t *b)
{
    qfloat_t    c_const = qf_from_double(0.0);
    qfloat_t    common_coeff = QF_ONE;
    addend_t *terms   = NULL;
    size_t    n = 0, cap = 0;

    dv_collect_addends(a, qf_from_double(1.0), &c_const, &terms, &n, &cap);
    dv_free(a);
    dv_collect_addends(b, is_op(dv, &ops_sub) ? qf_from_double(-1.0) : qf_from_double(1.0), &c_const, &terms, &n, &cap);
    dv_free(b);

    dv_combine_common_denominator_addends(terms, n);
    dv_sort_addends(terms, n);
    dv_extract_common_addend_coeff(terms, n, c_const, &common_coeff);

    {
        dval_t *identity = dv_try_trig_pythagorean_identity(terms, n, c_const, common_coeff);

        if (identity) {
            for (size_t i = 0; i < n; ++i)
                dv_free(terms[i].base);
            free(terms);
            return identity;
        }
    }

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
        dval_t *term = dv_make_scaled(qf_div(terms[i].coeff, common_coeff), terms[i].base);
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

    if (!cur)
        return dv_new_const_d(0.0);

    if (!is_qf_one(common_coeff))
        return dv_make_scaled(common_coeff, cur);

    return cur;
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
        dv_free_node_array(terms, nterms);
        return dv_new_const_d(0.0);
    }

    dv_split_division_terms(&c_acc, &is_zero, terms, nterms,
                         &den_terms, &nden_terms, &den_cap);

    if (is_zero) {
        dv_free_node_array(terms, nterms);
        dv_free_node_array(den_terms, nden_terms);
        return dv_new_const_d(0.0);
    }

    dv_combine_like_powers(den_terms, nden_terms);
    dv_combine_like_powers(terms, nterms);
    dv_combine_exp_terms(terms, nterms);
    dv_merge_sqrt_terms(terms, nterms);
    dv_merge_sqrt_terms(den_terms, nden_terms);

    expanded = dv_try_expand_shallow_product(c_acc, terms, nterms, den_terms, nden_terms);
    if (expanded)
        return expanded;

    numerator = dv_rebuild_product_chain(c_acc, terms, nterms);
    if (nden_terms == 0) {
        free(den_terms);
        return numerator;
    }

    denominator = dv_rebuild_division_chain(den_terms, nden_terms);
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
    if (is_unnamed_const(b) &&
        is_op(a, &ops_mul) &&
        is_unnamed_const(a->a)) {
        qfloat_t folded = qf_div(a->a->c.re, b->c.re);
        dval_t *rest;

        dv_retain(a->b);
        rest = a->b;
        dv_free(a);
        dv_free(b);
        return dv_make_scaled(folded, rest);
    }
    if (is_unnamed_const(b) && is_op(a, &ops_neg)) {
        dval_t *inner;
        dval_t *quot;
        dval_t *simp;

        dv_retain(a->a);
        dv_retain(b);
        inner = a->a;
        quot = dv_div(inner, b);
        dv_free(inner);
        dv_free(b);
        simp = dv_simplify(quot);
        dv_free(quot);
        dv_free(a);
        return dv_simplify_neg_operator(dv, simp, NULL);
    }
    if (is_unnamed_const(b) && is_addsub(a)) {
        qfloat_t c_const = QF_ZERO;
        addend_t *terms = NULL;
        size_t n = 0, cap = 0;
        dval_t *cur = NULL;
        qfloat_t denom = b->c.re;

        dv_collect_addends(a, QF_ONE, &c_const, &terms, &n, &cap);
        dv_free(a);
        dv_free(b);

        for (size_t i = 0; i < n; ++i) {
            dval_t *term;

            if (!terms[i].base || is_qf_zero(terms[i].coeff)) {
                if (terms[i].base)
                    dv_free(terms[i].base);
                continue;
            }

            term = dv_make_scaled(qf_div(terms[i].coeff, denom), terms[i].base);
            if (!cur) cur = term;
            else {
                dval_t *tmp = dv_add(cur, term);
                dv_free(cur);
                dv_free(term);
                cur = tmp;
            }
        }
        free(terms);

        if (!is_qf_zero(c_const)) {
            dval_t *cterm = dv_new_const(qf_div(c_const, denom));

            if (!cur)
                cur = cterm;
            else {
                dval_t *tmp = dv_add(cur, cterm);
                dv_free(cur);
                dv_free(cterm);
                cur = tmp;
            }
        }

        return cur ? cur : dv_new_const_d(0.0);
    }
    if (is_op(a, &ops_mul) && dv_struct_eq(a->a, b)) {
        dval_t *rest;

        dv_retain(a->b);
        rest = a->b;
        dv_free(a);
        dv_free(b);
        return rest;
    }
    if (is_op(a, &ops_mul) && dv_struct_eq(a->b, b)) {
        dval_t *rest;

        dv_retain(a->a);
        rest = a->a;
        dv_free(a);
        dv_free(b);
        return rest;
    }
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
        return dv_make_pow_like(inner, half);
    }

    return dv_make_pow_like(a, exponent);
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
