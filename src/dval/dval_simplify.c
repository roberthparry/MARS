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

#include "dval_bindings_internal.h"
#include "dval_internal.h"
#include "internal/number_internal.h"

/* forward declaration — helpers below call dv_simplify recursively */
dval_t *dv_simplify(const dval_t *dv);

static void *dv_xrealloc(void *ptr, size_t size)
{
    void *grown = realloc(ptr, size);

    if (grown)
        return grown;

    fprintf(stderr, "dval_simplify: out of memory\n");
    abort();
}

static inline dval_t *dv_new_const_owned_local(number_t value)
{
    NUM_SCOPE_SUSPEND(saved_scope);
    dval_t *out = dv_new_const(value);

    return out;
}

static inline dval_t *dv_make_scaled_owned_local(number_t coeff, dval_t *base)
{
    NUM_SCOPE_SUSPEND(saved_scope);
    dval_t *out = dv_make_scaled(coeff, base);

    return out;
}

static inline dval_t *dv_make_pow_like_owned_local(dval_t *base, number_t exponent)
{
    NUM_SCOPE_SUSPEND(saved_scope);
    dval_t *out = dv_make_pow_like(base, exponent);

    return out;
}

static bool dv_contains_var_local(const dval_t *dv)
{
    if (!dv)
        return false;
    if (dv_is_var(dv))
        return true;
    return dv_contains_var_local(dv->a) || dv_contains_var_local(dv->b);
}

static bool dv_is_lambert_expr_local(const dval_t *dv)
{
    return dv_is_op(dv, &ops_lambert_w0) || dv_is_op(dv, &ops_lambert_wm1);
}

static dval_t *dv_try_simplify_lambert_product(dval_t *a, dval_t *b)
{
    dval_t *w;
    dval_t *exp_term;
    dval_t *inner;

    if (dv_is_lambert_expr_local(a) && dv_is_exp_expr(b)) {
        w = a;
        exp_term = b;
    } else if (dv_is_lambert_expr_local(b) && dv_is_exp_expr(a)) {
        w = b;
        exp_term = a;
    } else {
        return NULL;
    }

    if (!dv_struct_eq(w, exp_term->a))
        return NULL;

    inner = w->a;

    if (!dv_current_wrt_internal() && dv_is_var(inner) && inner->binding_expr)
        return dv_binding_expr_eval_dval(inner->binding_expr);

    dv_retain(inner);
    return inner;
}

static dval_t *dv_try_simplify_exp_quarter_turn(const dval_t *arg)
{
    NUM_SCOPE(scope);
    number_t arg_value;
    number_t exp_value;
    number_t neg_i;

    if (!arg || dv_contains_var_local(arg))
        return NULL;

    arg_value = dv_eval(arg);
    exp_value = num_exp(arg_value);
    neg_i = num_neg(NUM_I);

    if (num_eq(exp_value, NUM_I)) {
        num_destroy(&neg_i);
        num_destroy(&exp_value);
        num_destroy(&arg_value);
        return dv_new_named_const(NUM_I, "i");
    }
    if (num_eq(exp_value, NUM_NEG_ONE)) {
        num_destroy(&neg_i);
        num_destroy(&exp_value);
        num_destroy(&arg_value);
        return dv_new_const(NUM_NEG_ONE);
    }
    if (num_eq(exp_value, neg_i)) {
        dval_t *i = dv_new_named_const(NUM_I, "i");
        dval_t *out = dv_neg(i);

        dv_free(i);
        num_destroy(&neg_i);
        num_destroy(&exp_value);
        num_destroy(&arg_value);
        return out;
    }
    if (num_eq(exp_value, NUM_ONE)) {
        num_destroy(&neg_i);
        num_destroy(&exp_value);
        num_destroy(&arg_value);
        return dv_new_const(NUM_ONE);
    }

    num_destroy(&neg_i);
    num_destroy(&exp_value);
    num_destroy(&arg_value);
    return NULL;
}

int dv_fold_zero_to_zero(const number_t *in, number_t *out)
{
    if (!in || !out || !num_eq(*in, NUM_ZERO))
        return 0;
    *out = NUM_ZERO;
    return 1;
}

int dv_fold_cos_const(const number_t *in, number_t *out)
{
    if (!in || !out || !num_eq(*in, NUM_ZERO))
        return 0;
    *out = NUM_ONE;
    return 1;
}

int dv_fold_exp_const(const number_t *in, number_t *out)
{
    if (!in || !out || !num_eq(*in, NUM_ZERO))
        return 0;
    *out = NUM_ONE;
    return 1;
}

int dv_fold_log_const(const number_t *in, number_t *out)
{
    if (!in || !out)
        return 0;
    if (num_eq(*in, NUM_ONE)) {
        *out = NUM_ZERO;
        return 1;
    }
    if (num_eq(*in, NUM_E)) {
        *out = NUM_ONE;
        return 1;
    }
    return 0;
}

int dv_fold_sqrt_const(const number_t *in, number_t *out)
{
    if (!in || !out)
        return 0;
    if (num_eq(*in, NUM_ZERO)) {
        *out = NUM_ZERO;
        return 1;
    }
    if (num_eq(*in, NUM_ONE)) {
        *out = NUM_ONE;
        return 1;
    }
    return 0;
}

/* ========================================================================= */
/* Multiplication flattening                                                  */
/* ========================================================================= */

static void collect_mul_flat(
    dval_t *dv,
    number_t *c_acc, int *is_zero,
    dval_t ***terms, size_t *nterms, size_t *cap)
{
    NUM_SCOPE(scope);
    if (*is_zero) {
        num_scope_leave(&(scope));
        return;
    }

    if (dv_is_unnamed_const(dv) && num_is_real(dv->c)) {
        if (num_is_zero(dv->c)) {
            *is_zero = 1;
            num_scope_leave(&(scope));
            return;
        }
        {
            NUM_SCOPE_SUSPEND(saved_scope);
            number_t product = num_mul(*c_acc, dv->c);

            num_destroy(c_acc);
            *c_acc = product;
        }
        num_scope_leave(&(scope));
        return;
    }
    if (dv_is_op(dv, &ops_neg)) {
        NUM_SCOPE_SUSPEND(saved_scope);
        number_t negated = num_neg(*c_acc);

        num_destroy(c_acc);
        *c_acc = negated;
        num_scope_leave(&(scope));
        collect_mul_flat(dv->a, c_acc, is_zero, terms, nterms, cap);
        return;
    }
    if (dv_is_op(dv, &ops_mul)) {
        num_scope_leave(&(scope));
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
    num_scope_leave(&(scope));
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
    NUM_SCOPE(scope);
    (void)b;
    if (dv_is_exp_expr(dv)) {
        dval_t *quarter_turn = dv_try_simplify_exp_quarter_turn(a);

        if (quarter_turn) {
            dv_free(a);
            return quarter_turn;
        }
    }

    /* exp(log(x)) -> x, log(exp(x)) -> x */
    if ((dv_is_exp_expr(dv) && dv_is_op(a, &ops_log)) ||
        (dv_is_op(dv, &ops_log) && dv_is_exp_expr(a))) {
        dval_t *inner = a->a;
        dv_retain(inner);
        dv_free(a);
        return inner;
    }

    /* log10(10^x) -> x */
    if (dv_is_op(dv, &ops_log10) &&
        dv_is_op(a, &ops_pow) &&
        dv_is_op(a->a, &ops_const) &&
        num_eq(a->a->c, NUM_TEN)) {
        dval_t *inner = a->b;
        dv_retain(inner);
        dv_free(a);
        return inner;
    }

    if (dv_is_op(a, &ops_const)) {
        number_t folded = num_new();

        if (dv->ops->fold_const_unary && dv->ops->fold_const_unary(&a->c, &folded)) {
            dval_t *out = dv_new_const_owned_local(folded);

            dv_free(a);
            return out;
        }

        if ((!a->name || !*a->name) && dv->ops->apply_unary) {
            dval_t *tmp = dv->ops->apply_unary(a);
            number_t value = tmp->ops->eval(tmp);
            dval_t *out = dv_new_const_owned_local(value);

            dv_free(tmp);
            dv_free(a);
            return out;
        }
    }

    if (dv_is_op(dv, &ops_sqrt) &&
        dv_is_op(a, &ops_mul) &&
        dv_is_unnamed_const(a->a)) {
        if (num_is_real(a->a->c) && num_gt(a->a->c, NUM_ZERO)) {
            number_t coeff_root = num_sqrt(a->a->c);
            number_t coeff_square = num_mul(coeff_root, coeff_root);

            if (num_eq(coeff_square, a->a->c)) {
                dval_t *raw;
                dval_t *simp;

                dv_retain(a->b);
                raw = dv_sqrt(a->b);
                dv_free(a->b);
                simp = dv_simplify(raw);
                dv_free(raw);
                dv_free(a);
                dval_t *out = dv_make_scaled_owned_local(coeff_root, simp);

                return out;
            }
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
    NUM_SCOPE(scope);
    if ((!a || !b) || !dv->ops->apply_binary)
    {
        return dv_simplify_passthrough(dv, a, b);
    }

    if (dv_is_unnamed_const(a) && dv_is_unnamed_const(b)) {
        dval_t *tmp = dv->ops->apply_binary(a, b);
        number_t value = tmp->ops->eval(tmp);
        dval_t *out = dv_new_const_owned_local(value);

        dv_free(tmp);
        dv_free(a);
        dv_free(b);
        return out;
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
    NUM_SCOPE(scope);
    (void)dv;
    (void)b;
    /* --x → x */
    if (dv_is_op(a, &ops_neg)) {
        dval_t *inner = a->a; dv_retain(inner); dv_free(a); return inner;
    }
    /* neg(c) → -c */
    if (dv_is_op(a, &ops_const)) {
        number_t c = num_neg(a->c);
        dval_t *out = dv_new_const_owned_local(c);

        dv_free(a);
        return out;
    }
    /* neg(c·x) where c < 0 → |c|·x  (eliminates spurious double-negative) */
    if (dv_is_op(a, &ops_mul) &&
        dv_is_op(a->a, &ops_const) &&
        (!a->a->name || !*a->a->name)) {
        if (num_is_real(a->a->c) && num_lt(a->a->c, NUM_ZERO)) {
            number_t pos_c = num_neg(a->a->c);

            dv_retain(a->b);
            dval_t *rest = a->b;
            dv_free(a);
            dval_t *out = dv_make_scaled_owned_local(pos_c, rest);

            return out;
        }
    }
    dval_t *r = dv_neg(a); dv_free(a); return r;
}

/* --- */

dval_t *dv_simplify_add_sub_operator(const dval_t *dv, dval_t *a, dval_t *b)
{
    NUM_SCOPE(scope);
    number_t c_const = num_const(NUM_ZERO);
    number_t common_coeff = num_const(NUM_ONE);
    addend_t *terms   = NULL;
    size_t    n = 0, cap = 0;

    dv_collect_addends(a, NUM_ONE, &c_const, &terms, &n, &cap);
    dv_free(a);
    dv_collect_addends(b, dv_is_op(dv, &ops_sub) ? NUM_NEG_ONE : NUM_ONE, &c_const, &terms, &n, &cap);
    dv_free(b);

    dv_combine_common_denominator_addends(terms, n);
    dv_sort_addends(terms, n);
    dv_extract_common_addend_coeff(terms, n, c_const, &common_coeff);

    dval_t *identity = dv_try_trig_pythagorean_identity(terms, n, c_const, common_coeff);

    if (identity) {
        for (size_t i = 0; i < n; ++i) {
            dv_free(terms[i].base);
            num_destroy(&terms[i].coeff);
        }
        free(terms);
        num_destroy(&c_const);
        num_destroy(&common_coeff);
        return identity;
    }

    /* find the leading non-cancelled symbolic term */
    int leading_neg = 0;
    for (size_t i = 0; i < n; ++i) {
        if (!num_is_zero(terms[i].coeff)) {
            leading_neg = num_lt(terms[i].coeff, NUM_ZERO);
            break;
        }
    }

    dval_t *cur = NULL;

    /* emit a positive constant before any leading negative symbolic term so
     * the expression reads "1 - tanh²(x)" rather than "-tanh²(x) + 1" */
    int const_emitted = 0;
    if (!num_is_zero(c_const) && num_gt(c_const, NUM_ZERO) && leading_neg) {
        cur = dv_new_const(c_const);
        const_emitted = 1;
    }

    for (size_t i = 0; i < n; ++i) {
        if (num_is_zero(terms[i].coeff)) {
            dv_free(terms[i].base);
            num_destroy(&terms[i].coeff);
            continue;
        }
        number_t scaled_coeff = num_div(terms[i].coeff, common_coeff);
        dval_t *term = dv_make_scaled(scaled_coeff, terms[i].base);

        num_destroy(&terms[i].coeff);
        if (!cur) cur = term;
        else {
            dval_t *tmp = dv_add(cur, term);
            dv_free(cur); dv_free(term); cur = tmp;
        }
    }
    free(terms);

    if (!const_emitted && !num_is_zero(c_const)) {
        dval_t *cterm = dv_new_const(c_const);
        if (!cur) cur = cterm;
        else {
            dval_t *tmp = dv_add(cur, cterm);
            dv_free(cur); dv_free(cterm); cur = tmp;
        }
    }

    if (!cur)
        cur = dv_new_const(NUM_ZERO);

    if (!num_is_one(common_coeff)) {
        dval_t *scaled = dv_make_scaled(common_coeff, cur);

        cur = scaled;
    }

    num_destroy(&c_const);
    num_destroy(&common_coeff);

    return cur;
}

/* --- */

dval_t *dv_simplify_mul_operator(const dval_t *dv, dval_t *a, dval_t *b)
{
    NUM_SCOPE(scope);
    dval_t **terms = NULL;
    dval_t **den_terms = NULL;
    size_t nterms = 0, term_cap = 0;
    size_t nden_terms = 0, den_cap = 0;
    number_t c_acc = num_const(NUM_ONE);
    int is_zero = 0;
    dval_t *expanded;
    dval_t *numerator;
    dval_t *denominator;
    dval_t *division;

    (void)dv;

    if ((dv_is_op(a, &ops_const) && dv_const_is_zero(a)) ||
        (dv_is_op(b, &ops_const) && dv_const_is_zero(b))) {
        dv_free(a);
        dv_free(b);
        return dv_new_const(NUM_ZERO);
    }
    if (dv_is_op(a, &ops_const) && dv_const_is_one(a)) {
        dv_free(a);
        return b;
    }
    if (dv_is_op(b, &ops_const) && dv_const_is_one(b)) {
        dv_free(b);
        return a;
    }

    {
        dval_t *lambert_identity = dv_try_simplify_lambert_product(a, b);

        if (lambert_identity) {
            dv_free(a);
            dv_free(b);
            return lambert_identity;
        }
    }

    collect_mul_flat(a, &c_acc, &is_zero, &terms, &nterms, &term_cap);
    collect_mul_flat(b, &c_acc, &is_zero, &terms, &nterms, &term_cap);
    dv_free(a);
    dv_free(b);

    if (is_zero) {
        dv_free_node_array(terms, nterms);
        num_destroy(&c_acc);
        return dv_new_const(NUM_ZERO);
    }

    dv_split_division_terms(&c_acc, &is_zero, terms, nterms,
                         &den_terms, &nden_terms, &den_cap);

    if (is_zero) {
        dv_free_node_array(terms, nterms);
        dv_free_node_array(den_terms, nden_terms);
        num_destroy(&c_acc);
        return dv_new_const(NUM_ZERO);
    }

    dv_combine_like_powers(den_terms, nden_terms);
    dv_combine_like_powers(terms, nterms);
    dv_cancel_common_powers(terms, nterms, den_terms, nden_terms);
    dv_combine_exp_terms(terms, nterms);
    dv_merge_sqrt_terms(terms, nterms);
    dv_merge_sqrt_terms(den_terms, nden_terms);

    expanded = dv_try_expand_shallow_product(c_acc, terms, nterms, den_terms, nden_terms);
    if (expanded) {
        num_destroy(&c_acc);
        return expanded;
    }

    numerator = dv_rebuild_product_chain(c_acc, terms, nterms);
    num_destroy(&c_acc);
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
    NUM_SCOPE(scope);
    (void)dv;

    if (dv_is_op(b, &ops_const) && dv_const_is_one(b)) { dv_free(b); return a; }
    if (dv_is_pow_d_expr(b) && dv_struct_eq(a, b->a)) {
        number_t exponent = num_sub(b->c, NUM_ONE);
        dval_t *base;
        dval_t *denom;
        dval_t *one;
        dval_t *out;

        dv_retain(b->a);
        base = b->a;
        dv_free(a);
        dv_free(b);

        if (num_eq(exponent, NUM_ZERO)) {
            dv_free(base);
            return dv_new_const(NUM_ONE);
        }

        denom = dv_make_pow_like_owned_local(base, exponent);
        one = dv_new_const(NUM_ONE);
        out = dv_div(one, denom);
        dv_free(one);
        dv_free(denom);
        return out;
    }
    if (dv_is_pow_d_expr(a) && dv_struct_eq(a->a, b)) {
        number_t exponent = num_sub(a->c, NUM_ONE);
        dval_t *base;

        dv_retain(a->a);
        base = a->a;
        dv_free(a);
        dv_free(b);
        return dv_make_pow_like_owned_local(base, exponent);
    }
    if (dv_is_unnamed_const(b) &&
        dv_is_op(a, &ops_mul) &&
        dv_is_unnamed_const(a->a) &&
        num_is_real(a->a->c) &&
        num_is_real(b->c)) {
        dval_t *rest;
        number_t folded = num_div(a->a->c, b->c);

        dv_retain(a->b);
        rest = a->b;
        dv_free(a);
        dv_free(b);
        dval_t *out = dv_make_scaled_owned_local(folded, rest);

        return out;
    }
    if (dv_is_unnamed_const(b) && dv_is_op(a, &ops_neg)) {
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
    if (dv_is_unnamed_const(b) && dv_is_addsub(a)) {
        number_t c_const = num_const(NUM_ZERO);
        number_t denom = NUM_ZERO;
        addend_t *terms = NULL;
        size_t n = 0, cap = 0;
        dval_t *cur = NULL;

        if (!num_is_real(b->c)) {
            num_destroy(&c_const);
            goto div_fallback_2;
        }
        denom = num_clone(b->c);

        dv_collect_addends(a, NUM_ONE, &c_const, &terms, &n, &cap);
        dv_free(a);
        dv_free(b);

        for (size_t i = 0; i < n; ++i) {
            dval_t *term;

            if (!terms[i].base || num_is_zero(terms[i].coeff)) {
                if (terms[i].base)
                    dv_free(terms[i].base);
                num_destroy(&terms[i].coeff);
                continue;
            }

            number_t scaled_coeff = num_div(terms[i].coeff, denom);

            term = dv_make_scaled_owned_local(scaled_coeff, terms[i].base);
            num_destroy(&terms[i].coeff);
            if (!cur) cur = term;
            else {
                dval_t *tmp = dv_add(cur, term);
                dv_free(cur);
                dv_free(term);
                cur = tmp;
            }
        }
        free(terms);

        if (!num_is_zero(c_const)) {
            number_t scaled_const = num_div(c_const, denom);
            dval_t *cterm = dv_new_const_owned_local(scaled_const);

            if (!cur)
                cur = cterm;
            else {
                dval_t *tmp = dv_add(cur, cterm);
                dv_free(cur);
                dv_free(cterm);
                cur = tmp;
            }
        }

        {
            dval_t *out = cur ? cur : dv_new_const(NUM_ZERO);

            num_destroy(&c_const);
            num_destroy(&denom);
            return out;
        }
    }
div_fallback_2:
    if (dv_is_op(a, &ops_mul) && dv_struct_eq(a->a, b)) {
        dval_t *rest;

        dv_retain(a->b);
        rest = a->b;
        dv_free(a);
        dv_free(b);
        return rest;
    }
    if (dv_is_op(a, &ops_mul) && dv_struct_eq(a->b, b)) {
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
        return dv_new_const(NUM_ONE);
    }
    if (dv_is_op(a, &ops_const) && dv_const_is_zero(a)) {
        dv_free(a); dv_free(b); return dv_new_const(NUM_ZERO);
    }
    if (dv_is_unnamed_const(a) && dv_is_unnamed_const(b)) {
        number_t q = num_div(a->c, b->c);

        dv_free(a);
        dv_free(b);
        dval_t *out = dv_new_const_owned_local(q);

        return out;
    }

    /* sinh(x)/cosh(x) → tanh(x) */
    if (dv_is_op(a, &ops_sinh) && dv_is_op(b, &ops_cosh) &&
        dv_struct_eq(a->a, b->a)) {
        dv_retain(a->a);
        dval_t *r = dv_tanh(a->a); dv_free(a->a); dv_free(a); dv_free(b);
        return r;
    }
    /* sin(x)/cos(x) → tan(x) */
    if (dv_is_op(a, &ops_sin) && dv_is_op(b, &ops_cos) &&
        dv_struct_eq(a->a, b->a)) {
        dv_retain(a->a);
        dval_t *r = dv_tan(a->a); dv_free(a->a); dv_free(a); dv_free(b);
        return r;
    }
    /* x/abs(x) → abs(x)/x  (canonical sign-function form) */
    if (dv_is_op(b, &ops_abs) && dv_struct_eq(a, b->a)) {
        dval_t *r = dv_div(b, a); dv_free(a); dv_free(b); return r;
    }

    dval_t *r = dv_div(a, b); dv_free(a); dv_free(b); return r;
}

/* --- */

dval_t *dv_simplify_pow_d_operator(const dval_t *dv, dval_t *a, dval_t *b)
{
    NUM_SCOPE(scope);
    (void)b;
    number_t exponent = dv->c;

    if (num_eq(exponent, NUM_ONE)) {
        return a;
    }
    if (num_eq(exponent, NUM_ZERO)) {
        dv_free(a);
        return dv_new_const(NUM_ONE);
    }

    if (dv_is_unnamed_const(a)) {
        number_t v = num_pow(a->c, exponent);

        dv_free(a);
        dval_t *out = dv_new_const_owned_local(v);

        return out;
    }

    /* sqrt(x)^n → x^(n/2) */
    if (dv_is_sqrt_expr(a)) {
        number_t half = num_div(exponent, NUM_TWO);
        dv_retain(a->a);
        dval_t *inner = a->a;
        dv_free(a);
        dval_t *out = dv_make_pow_like_owned_local(inner, half);

        return out;
    }

    return dv_make_pow_like_owned_local(a, exponent);
}

/* --- */

dval_t *dv_simplify_pow_operator(const dval_t *dv, dval_t *a, dval_t *b)
{
    (void)dv;
    if (dv_is_op(b, &ops_const) && dv_const_is_one(b)) { dv_free(b); return a; }
    if (dv_is_op(b, &ops_const) && dv_const_is_zero(b)) {
        dv_free(a); dv_free(b); return dv_new_const(NUM_ONE);
    }
    if (dv_is_op(a, &ops_const) && num_eq(a->c, NUM_E)) {
        dval_t *raw;
        dval_t *out;

        raw = dv_exp(b);
        out = dv_simplify(raw);
        dv_free(raw);
        dv_free(a);
        dv_free(b);
        return out;
    }
    if (dv_is_op(a, &ops_const) && num_eq(a->c, NUM_TEN) && dv_is_op(b, &ops_log10)) {
        dval_t *inner = b->a;
        dv_retain(inner);
        dv_free(a);
        dv_free(b);
        return inner;
    }
    dval_t *r = dv_pow_dv(a, b); dv_free(a); dv_free(b); return r;
}

/* --- */

dval_t *dv_simplify_hypot_operator(const dval_t *dv, dval_t *a, dval_t *b)
{
    (void)dv;

    if (dv_is_op(a, &ops_const) && dv_const_is_zero(a)) {
        dv_free(a);
        dval_t *r = dv_abs(b);
        dv_free(b);
        return r;
    }
    if (dv_is_op(b, &ops_const) && dv_const_is_zero(b)) {
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

dval_t *dv_simplify(const dval_t *dv)
{
    if (!dv) return NULL;

    if (dv->ops->arity == DV_OP_ATOM) { dv_retain(dv); return (dval_t *)dv; }

    dval_t *a = dv->a ? dv_simplify(dv->a) : NULL;
    dval_t *b = dv->b ? dv_simplify(dv->b) : NULL;

    if (dv->ops->simplify)
        return dv->ops->simplify(dv, a, b);

    return dv_simplify_passthrough(dv, a, b);
}
