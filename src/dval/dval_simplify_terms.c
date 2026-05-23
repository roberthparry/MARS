#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dval_bindings_internal.h"
#include "dval_internal.h"

extern dval_t *dv_simplify(const dval_t *dv);

static int dv_is_foldable_unnamed_real_const(const dval_t *dv)
{
    if (!dv_is_unnamed_const(dv) || !num_is_real(dv->c))
        return 0;
    if (dv->binding_expr && dv->binding_expr->kind != DV_BINDING_EXPR_NUMBER)
        return 0;
    return 1;
}

static int dv_try_get_unnamed_const_real_num(const dval_t *dv, number_t *out)
{
    if (!dv_is_foldable_unnamed_real_const(dv))
        return 0;
    *out = num_clone(dv->c);
    return 1;
}

static int dv_is_i_squared_term(const dval_t *dv)
{
    return dv_is_pow_d_expr(dv) &&
           dv_is_op(dv->a, &ops_const) &&
           !dv->a->binding_expr &&
           (num_eq(dv->a->c, NUM_I) || num_eq(dv->a->c, NUM_NEG_I)) &&
           num_eq(dv->c, NUM_TWO);
}

static void *dv_terms_xrealloc(void *ptr, size_t size)
{
    void *grown = realloc(ptr, size);

    if (grown)
        return grown;

    fprintf(stderr, "dval_simplify_terms: out of memory\n");
    abort();
}

static number_t dv_normalize_simple_rational_coeff(number_t coeff)
{
    if (num_eq(coeff, NUM_HALF))
        return num_clone(NUM_HALF);
    if (num_eq(coeff, NUM_QUARTER))
        return num_clone(NUM_QUARTER);
    if (num_eq(coeff, NUM_ONE_EIGHTH))
        return num_clone(NUM_ONE_EIGHTH);
    if (num_is_real(coeff)) {
        double d = num_to_double(coeff);

        if (d == 0.5)
            return num_clone(NUM_HALF);
        if (d == 0.25)
            return num_clone(NUM_QUARTER);
        if (d == 0.125)
            return num_clone(NUM_ONE_EIGHTH);
        if (d == -0.5)
            return num_neg(NUM_HALF);
        if (d == -0.25)
            return num_neg(NUM_QUARTER);
        if (d == -0.125)
            return num_neg(NUM_ONE_EIGHTH);
    }
    return num_clone(coeff);
}

static int term_coeff(const dval_t *term, const dval_t **base, number_t *coeff_out)
{
    if (dv_is_foldable_unnamed_real_const(term)) {
        *base = NULL;
        *coeff_out = num_clone(term->c);
        return 1;
    }
    if (dv_is_op(term, &ops_neg)) {
        if (dv_is_op(term->a, &ops_mul) &&
            dv_is_foldable_unnamed_real_const(term->a->a)) {
            *base = term->a->b;
            *coeff_out = num_neg(term->a->a->c);
            return 1;
        }
        *base = term->a;
        *coeff_out = num_clone(NUM_NEG_ONE);
        return 1;
    }
    if (dv_is_op(term, &ops_mul) &&
        dv_is_foldable_unnamed_real_const(term->a)) {
        *base = term->b;
        *coeff_out = num_clone(term->a->c);
        return 1;
    }
    *base = term;
    *coeff_out = num_clone(NUM_ONE);
    return 1;
}

static int split_leading_real_scalar(const dval_t *term,
                                     number_t *scalar_out,
                                     const dval_t **rest_out)
{
    if (dv_is_foldable_unnamed_real_const(term)) {
        *scalar_out = num_clone(term->c);
        *rest_out = NULL;
        return 1;
    }

    if (dv_is_op(term, &ops_mul) &&
        dv_is_foldable_unnamed_real_const(term->a)) {
        *scalar_out = num_clone(term->a->c);
        *rest_out = term->b;
        return 1;
    }

    *scalar_out = num_clone(NUM_ONE);
    *rest_out = term;
    return 0;
}

static dval_t *make_normalized_division_addend(const dval_t *num,
                                               const dval_t *den)
{
    dval_t *one;
    dval_t *out;

    if (!num && !den)
        return NULL;

    if (!den) {
        dv_retain((dval_t *)num);
        return (dval_t *)num;
    }

    if (!num) {
        one = dv_new_const(NUM_ONE);
        dv_retain((dval_t *)den);
        out = dv_div(one, (dval_t *)den);
        dv_free(one);
        dv_free((dval_t *)den);
        return out;
    }

    dv_retain((dval_t *)num);
    dv_retain((dval_t *)den);
    out = dv_div((dval_t *)num, (dval_t *)den);
    dv_free((dval_t *)num);
    dv_free((dval_t *)den);
    return out;
}

static dval_t *dv_try_fold_scaled_product(number_t coeff, dval_t *base)
{
    if (dv_is_op(base, &ops_mul)) {
        dval_t *left = base->a;
        dval_t *right = base->b;
        dval_t *scaled_left;
        dval_t *scaled_right;
        dval_t *r;

        if (dv_is_foldable_unnamed_real_const(left)) {
            dv_retain(left);
            dv_retain(right);
            dv_free(base);
            scaled_left = dv_make_scaled(coeff, left);
            r = dv_mul(scaled_left, right);
            dv_free(scaled_left);
            dv_free(right);
            return r;
        }

        if (dv_is_foldable_unnamed_real_const(right)) {
            dv_retain(left);
            dv_retain(right);
            dv_free(base);
            scaled_right = dv_make_scaled(coeff, right);
            r = dv_mul(left, scaled_right);
            dv_free(left);
            dv_free(scaled_right);
            return r;
        }

        dv_retain(left);
        scaled_left = dv_try_fold_scaled_product(coeff, left);
        if (scaled_left) {
            dv_retain(right);
            r = dv_mul(scaled_left, right);
            dv_free(scaled_left);
            dv_free(right);
            dv_free(base);
            return r;
        }
        dv_free(left);

        dv_retain(right);
        scaled_right = dv_try_fold_scaled_product(coeff, right);
        if (scaled_right) {
            dv_retain(left);
            r = dv_mul(left, scaled_right);
            dv_free(left);
            dv_free(scaled_right);
            dv_free(base);
            return r;
        }
        dv_free(right);
    }

    return NULL;
}

dval_t *dv_make_scaled(number_t coeff, dval_t *base)
{
    NUM_SCOPE(scope);
    if (num_is_zero(coeff)) { dv_free(base); return dv_new_const(NUM_ZERO); }
    if (num_eq(coeff, NUM_ONE))  return base;
    if (num_eq(coeff, NUM_NEG_ONE)) {
        if (dv_is_op(base, &ops_div) && dv_is_op(base->a, &ops_mul) &&
            dv_is_foldable_unnamed_real_const(base->a->a) &&
            num_lt(base->a->a->c, NUM_ZERO)) {
            number_t pos_c = num_neg(base->a->a->c);
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
        if (dv_is_op(base, &ops_div) && dv_is_op(base->a, &ops_neg)) {
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
    if (dv_is_unnamed_const(base) && base->binding_expr && num_is_real(base->c)) {
        number_t leading_coeff;
        dv_binding_expr_t *rest_expr = NULL;

        if (dv_binding_expr_split_leading_number(base->binding_expr,
                                                &leading_coeff,
                                                &rest_expr)) {
            if (rest_expr) {
                number_t folded = num_mul(coeff, leading_coeff);
                dval_t *rest = dv_binding_expr_eval_dval(rest_expr);
                dval_t *out;

                dv_binding_expr_free(rest_expr);
                num_destroy(&leading_coeff);
                dv_free(base);
                out = dv_make_scaled(folded, rest);
                num_destroy(&folded);
                return out;
            }
            num_destroy(&leading_coeff);
        }
        char *coeff_text = num_to_string(coeff);
        dv_binding_expr_t *coeff_expr =
            dv_binding_expr_new_number_text(coeff_text ? coeff_text : "NAN");
        dv_binding_expr_t *expr =
            dv_binding_expr_new_mul(coeff_expr, dv_binding_expr_clone(base->binding_expr));
        number_t scaled = num_mul(coeff, base->c);
        dval_t *out = dv_new_const(scaled);

        expr = dv_binding_expr_simplify(expr);
        free(coeff_text);
        num_destroy(&scaled);
        dv_free(base);
        out->binding_expr = expr;
        return out;
    }
    if (dv_is_op(base, &ops_div)) {
        dval_t *num = base->a;
        dval_t *den = base->b;
        dval_t *scaled_num;
        dval_t *r;

        dv_retain(num);
        dv_retain(den);
        dv_free(base);

        scaled_num = dv_make_scaled(coeff, num);
        r = dv_div(scaled_num, den);
        dv_free(scaled_num);
        dv_free(den);
        return r;
    }
    {
        dval_t *folded_product = dv_try_fold_scaled_product(coeff, base);

        if (folded_product)
            return folded_product;
    }
    if (dv_is_op(base, &ops_mul) &&
        dv_is_unnamed_const(base->a) &&
        (!base->a->binding_expr ||
         base->a->binding_expr->kind == DV_BINDING_EXPR_NUMBER) &&
        num_is_real(base->a->c)) {
        number_t folded = num_mul(coeff, base->a->c);
        dv_retain(base->b);
        dval_t *rest = base->b;
        dv_free(base);
        dval_t *out = dv_make_scaled(folded, rest);

        return out;
    }
    if (dv_is_op(base, &ops_mul) &&
        dv_is_op(base->a, &ops_mul) &&
        dv_is_unnamed_const(base->a->a) &&
        (!base->a->a->binding_expr ||
         base->a->a->binding_expr->kind == DV_BINDING_EXPR_NUMBER) &&
        num_is_real(base->a->a->c)) {
        number_t folded = num_mul(coeff, base->a->a->c);
        dv_retain(base->a->b);
        dv_retain(base->b);
        dval_t *inner = dv_mul(base->a->b, base->b);
        dv_free(base->a->b);
        dv_free(base->b);
        dv_free(base);
        dval_t *out = dv_make_scaled(folded, inner);

        return out;
    }
    number_t normalized = dv_normalize_simple_rational_coeff(coeff);
    dval_t *cn = dv_new_const(normalized);
    dval_t *r = dv_mul(cn, base);

    num_destroy(&normalized);
    dv_free(cn);
    dv_free(base);
    return r;
}

static int addend_group(const dval_t *dv)
{
    if (dv->ops->arity == DV_OP_UNARY)                  return 0;
    if (dv_is_op(dv, &ops_var))                            return 1;
    if (dv_is_op(dv, &ops_const) && dv->name && *dv->name) return 2;
    return 3;
}

typedef struct {
    int group;
    int shape;
    const char *name;
} addend_sort_key_t;

static int addend_is_default_const(const dval_t *dv)
{
    const char *canon;
    number_t value;
    int is_default;

    if (!dv || !dv_is_op(dv, &ops_const) || !dv->name || !*dv->name)
        return 0;

    canon = dv_default_constant_canonical_name(dv->name);
    if (canon && strcmp(canon, "@tau") == 0)
        return 0;

    is_default = dv_get_default_constant_num(dv->name, &value);
    if (is_default)
        num_destroy(&value);
    return is_default;
}

static void addend_consider_leaf_key(const dval_t *dv, addend_sort_key_t *key)
{
    int group;

    if (!dv)
        return;
    if (!dv->name || !*dv->name)
        return;
    if (!dv_is_op(dv, &ops_var) && !dv_is_op(dv, &ops_const))
        return;

    if (num_is_nan(dv->c))
        group = 0;
    else if (addend_is_default_const(dv))
        group = 1;
    else if (dv_is_op(dv, &ops_const))
        group = 2;
    else
        group = 3;

    if (group < key->group ||
        (group == key->group && key->name && *key->name &&
         strcmp(dv->name, key->name) < 0)) {
        key->group = group;
        key->name = dv->name;
    }
}

static void addend_collect_product_key(const dval_t *dv, addend_sort_key_t *key)
{
    if (!dv)
        return;

    if (dv_is_op(dv, &ops_mul)) {
        addend_collect_product_key(dv->a, key);
        addend_collect_product_key(dv->b, key);
        return;
    }

    addend_consider_leaf_key(dv, key);
}

static addend_sort_key_t addend_sort_key(const dval_t *dv)
{
    addend_sort_key_t key = {4, 2, ""};

    if (!dv)
        return key;

    if (dv_is_op(dv, &ops_mul)) {
        key.shape = 0;
        addend_collect_product_key(dv, &key);
        return key;
    }

    if (dv_is_op(dv, &ops_div)) {
        key.shape = 1;
        addend_collect_product_key(dv->a, &key);
        addend_collect_product_key(dv->b, &key);
        return key;
    }

    addend_consider_leaf_key(dv, &key);
    return key;
}

void dv_combine_common_denominator_addends(addend_t *terms, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        dval_t *ibase = terms[i].base;
        dval_t *sum_num = NULL;
        dval_t *simp_num = NULL;
        dval_t *combined = NULL;
        int merged_any = 0;

        if (!dv_is_div(ibase))
            continue;

        for (size_t j = i + 1; j < n; ++j) {
            dval_t *jbase = terms[j].base;
            dval_t *i_num_term;
            dval_t *j_num_term;
            dval_t *tmp;

            if (!dv_is_div(jbase))
                continue;
            if (!dv_struct_eq(ibase->b, jbase->b))
                continue;

            if (!merged_any) {
                dv_retain(ibase->a);
                sum_num = dv_make_scaled(terms[i].coeff, ibase->a);
                num_destroy(&terms[i].coeff);
                terms[i].coeff = num_clone(NUM_ONE);
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
            num_destroy(&terms[j].coeff);
            terms[j].coeff = num_clone(NUM_ZERO);
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
        num_destroy(&terms[i].coeff);
        terms[i].coeff = num_clone(NUM_ONE);
    }
}

static int compare_addend_bases(const dval_t *lhs, const dval_t *rhs)
{
    int lg, rg;

    if (!lhs && !rhs)
        return 0;
    if (!lhs)
        return 1;
    if (!rhs)
        return -1;

    lg = addend_group(lhs);
    rg = addend_group(rhs);
    if (lg != rg)
        return lg - rg;

    {
        addend_sort_key_t lk = addend_sort_key(lhs);
        addend_sort_key_t rk = addend_sort_key(rhs);
        int cmp;

        if (lk.group != rk.group)
            return lk.group - rk.group;

        cmp = strcmp(lk.name, rk.name);
        if (cmp != 0)
            return cmp;

        return lk.shape - rk.shape;
    }
}

static int compare_addends(const addend_t *lhs, const addend_t *rhs)
{
    return compare_addend_bases(lhs->base, rhs->base);
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

static int dv_contains_addsub_normalized(const dval_t *dv)
{
    if (!dv)
        return 0;
    if (dv_is_addsub(dv))
        return 1;
    return dv_contains_addsub_normalized(dv->a) ||
           dv_contains_addsub_normalized(dv->b);
}

void dv_collect_addends(dval_t *dv, number_t scale, number_t *c_const,
                        addend_t **terms, size_t *n, size_t *cap)
{
    NUM_SCOPE(scope);
    if (!dv)
        return;
    if (dv_is_op(dv, &ops_add)) {
        dv_collect_addends(dv->a, scale, c_const, terms, n, cap);
        dv_collect_addends(dv->b, scale, c_const, terms, n, cap);
        return;
    }
    if (dv_is_op(dv, &ops_sub)) {
        number_t neg_scale = num_neg(scale);

        dv_collect_addends(dv->a, scale, c_const, terms, n, cap);
        dv_collect_addends(dv->b, neg_scale, c_const, terms, n, cap);
        return;
    }
    if (dv_is_op(dv, &ops_neg)) {
        if (dv_is_addsub(dv->a)) {
            number_t neg_scale = num_neg(scale);

            dv_collect_addends(dv->a, neg_scale, c_const, terms, n, cap);
            return;
        }
        if (dv_is_op(dv->a, &ops_mul) &&
            dv_is_unnamed_const(dv->a->a) &&
            dv_is_addsub(dv->a->b)) {
            number_t ns;
            number_t coeff_num = num_new();
            number_t neg_scale = num_neg(scale);

            if (dv_try_get_unnamed_const_real_num(dv->a->a, &coeff_num)) {
                ns = num_mul(neg_scale, coeff_num);
                dv_collect_addends(dv->a->b, ns, c_const, terms, n, cap);
                return;
            }
        }
    }
    if (dv_is_op(dv, &ops_mul) &&
        dv_is_unnamed_const(dv->a) &&
        dv_is_addsub(dv->b)) {
        number_t ns;
        number_t coeff_num = num_new();

        if (dv_try_get_unnamed_const_real_num(dv->a, &coeff_num)) {
            ns = num_mul(scale, coeff_num);
            dv_collect_addends(dv->b, ns, c_const, terms, n, cap);
            return;
        }
    }
    if (dv_is_op(dv, &ops_mul) &&
        dv_is_op(dv->a, &ops_mul) &&
        dv_is_unnamed_const(dv->a->a)) {
        number_t ns;
        number_t coeff_num = num_new();
        dval_t *raw;
        dval_t *simp;

        if (dv_try_get_unnamed_const_real_num(dv->a->a, &coeff_num)) {
            ns = num_mul(scale, coeff_num);

            dv_retain(dv->a->b);
            dv_retain(dv->b);
            raw = dv_mul(dv->a->b, dv->b);
            dv_free(dv->a->b);
            dv_free(dv->b);
            simp = dv_simplify(raw);
            dv_collect_addends(simp ? simp : raw, ns, c_const, terms, n, cap);
            dv_free(simp);
            dv_free(raw);
            return;
        }
    }
    if (dv_is_div(dv)) {
        number_t num_scalar = num_new();
        number_t den_scalar = num_new();
        number_t ns;
        const dval_t *num_rest = NULL;
        const dval_t *den_rest = NULL;
        int changed_num;
        int changed_den;

        changed_num = split_leading_real_scalar(dv->a, &num_scalar, &num_rest);
        changed_den = split_leading_real_scalar(dv->b, &den_scalar, &den_rest);

        if (changed_num || changed_den) {
            dval_t *normalized;

            ns = num_mul(scale, num_scalar);
            ns = num_div(ns, den_scalar);
            normalized = make_normalized_division_addend(num_rest, den_rest);
            if (normalized) {
                if ((!num_rest && den_rest) ||
                    !dv_contains_addsub_normalized(normalized) ||
                    dv_struct_eq(normalized, dv)) {
                    size_t i;

                    for (i = 0; i < *n; ++i) {
                        if (dv_struct_eq((*terms)[i].base, normalized)) {
                            number_t sum = num_add((*terms)[i].coeff, ns);

                            num_destroy(&(*terms)[i].coeff);
                            (*terms)[i].coeff = num_scope_detach(sum);
                            dv_free(normalized);
                            return;
                        }
                    }
                    if (*n == *cap) {
                        *cap = *cap ? *cap * 2 : 8;
                        *terms = dv_terms_xrealloc(*terms, *cap * sizeof(addend_t));
                    }
                    (*terms)[*n].base = normalized;
                    (*terms)[*n].coeff = num_scope_detach(ns);
                    (*n)++;
                    return;
                }
                dv_collect_addends(normalized, ns, c_const, terms, n, cap);
                dv_free(normalized);
            } else {
                number_t sum = num_add(*c_const, ns);

                num_destroy(c_const);
                *c_const = num_scope_detach(sum);
            }
            return;
        }
    }

    const dval_t *base;
    number_t coeff = num_new();

    if (!term_coeff(dv, &base, &coeff))
        return;
    {
        number_t scaled = num_mul(coeff, scale);

        coeff = scaled;
    }
    if (!base) {
        number_t sum = num_add(*c_const, coeff);

        num_destroy(c_const);
        *c_const = num_scope_detach(sum);
        return;
    }

    for (size_t i = 0; i < *n; ++i) {
        if (dv_struct_eq((*terms)[i].base, base)) {
            number_t sum = num_add((*terms)[i].coeff, coeff);

            num_destroy(&(*terms)[i].coeff);
            (*terms)[i].coeff = num_scope_detach(sum);
            return;
        }
    }
    if (*n == *cap) {
        *cap = *cap ? *cap * 2 : 8;
        *terms = dv_terms_xrealloc(*terms, *cap * sizeof(addend_t));
    }
    dv_retain((dval_t *)base);
    (*terms)[*n].base = (dval_t *)base;
    (*terms)[*n].coeff = num_scope_detach(coeff);
    (*n)++;
}

int dv_extract_common_addend_coeff(const addend_t *terms, size_t n,
                                   number_t c_const, number_t *common_out)
{
    NUM_SCOPE(scope);
    number_t common = num_new();
    int have_common = 0;
    size_t nonzero_terms = 0;

    if (!num_is_zero(c_const))
        return 0;

    for (size_t i = 0; i < n; ++i) {
        if (!terms[i].base || num_is_zero(terms[i].coeff))
            continue;
        if (!have_common) {
            common = num_clone(terms[i].coeff);
            have_common = 1;
        } else if (!num_eq(common, terms[i].coeff)) {
            return 0;
        }
        nonzero_terms++;
    }

    if (!have_common || nonzero_terms < 2 ||
        num_is_one(common) || num_eq(common, NUM_NEG_ONE)) {
        return 0;
    }

    num_destroy(common_out);
    *common_out = num_scope_detach(common);
    return 1;
}

static int is_trig_square_of(const dval_t *dv, const dval_ops_t *op, const dval_t **arg_out)
{
    if (!dv_is_pow_d_expr(dv) || !num_eq(dv->c, NUM_TWO))
        return 0;
    if (!dv_is_op(dv->a, op))
        return 0;

    *arg_out = dv->a->a;
    return 1;
}

dval_t *dv_try_trig_pythagorean_identity(const addend_t *terms, size_t n,
                                         number_t c_const, number_t common_coeff)
{
    const dval_t *sin_arg = NULL;
    const dval_t *cos_arg = NULL;
    const dval_t *sinh_arg = NULL;
    const dval_t *cosh_arg = NULL;
    size_t nonzero_terms = 0;
    int have_sin = 0;
    int have_cos = 0;
    int have_sinh = 0;
    int have_cosh = 0;

    if (!num_is_zero(c_const))
        return NULL;

    for (size_t i = 0; i < n; ++i) {
        if (!terms[i].base || num_is_zero(terms[i].coeff))
            continue;
        nonzero_terms++;
        if (nonzero_terms > 2)
            return NULL;

        if (num_is_one(terms[i].coeff) &&
            is_trig_square_of(terms[i].base, &ops_sin, &sin_arg)) {
            have_sin = 1;
            continue;
        }
        if (num_is_one(terms[i].coeff) &&
            is_trig_square_of(terms[i].base, &ops_cos, &cos_arg)) {
            have_cos = 1;
            continue;
        }
        if (num_is_one(terms[i].coeff) &&
            is_trig_square_of(terms[i].base, &ops_cosh, &cosh_arg)) {
            have_cosh = 1;
            continue;
        }
        if (num_eq(terms[i].coeff, NUM_NEG_ONE) &&
            is_trig_square_of(terms[i].base, &ops_sinh, &sinh_arg)) {
            have_sinh = 1;
            continue;
        }
        return NULL;
    }

    if (nonzero_terms == 2) {
        if (have_sin && have_cos && sin_arg && cos_arg &&
            dv_struct_eq(sin_arg, cos_arg))
            return dv_new_const(common_coeff);
        if (have_sinh && have_cosh && sinh_arg && cosh_arg &&
            dv_struct_eq(sinh_arg, cosh_arg))
            return dv_new_const(common_coeff);
    }

    return NULL;
}

static void flatten_add(dval_t *root, dval_t **addends, int *na, int max)
{
    dval_t *stk[64];
    int sp = 0;

    stk[sp++] = root;
    while (sp > 0 && *na < max) {
        dval_t *dv = stk[--sp];
        if (dv_is_op(dv, &ops_add)) {
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
    if (dv_is_op(u, &ops_add)) {
        dval_t *l = expand_product(u->a, v);
        dval_t *r = expand_product(u->b, v);
        dval_t *s = dv_add(l, r);
        dv_free(l);
        dv_free(r);
        return s;
    }
    if (dv_is_op(u, &ops_sub)) {
        dval_t *l = expand_product(u->a, v);
        dval_t *r = expand_product(u->b, v);
        dval_t *s = dv_sub(l, r);
        dv_free(l);
        dv_free(r);
        return s;
    }
    if (dv_is_addsub(v))
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

static number_t pow_exponent(const dval_t *dv)
{
    if (dv_is_sqrt_expr(dv))
        return num_div(NUM_ONE, NUM_TWO);
    if (!dv_is_op(dv, &ops_pow_d))
        return num_clone(NUM_ONE);
    return num_clone(dv->c);
}

static dval_t *pow_base(const dval_t *dv)
{
    if (dv_is_sqrt_expr(dv))
        return dv->a;
    return dv_is_op(dv, &ops_pow_d) ? dv->a : (dval_t *)dv;
}

dval_t *dv_make_pow_like(dval_t *base, number_t exponent)
{
    if (num_eq(exponent, NUM_ZERO)) {
        dv_free(base);
        return dv_new_const(NUM_ONE);
    }
    if (num_eq(exponent, NUM_ONE))
        return base;
    if (num_eq(exponent, NUM_TWO) &&
        dv_is_op(base, &ops_const) &&
        !base->binding_expr &&
        (num_eq(base->c, NUM_I) || num_eq(base->c, NUM_NEG_I))) {
        dv_free(base);
        return dv_new_const(NUM_NEG_ONE);
    }

    dval_t *pow = dv_pow(base, &exponent);
    dv_free(base);
    return pow;
}

static void dv_append_denominator_factor(number_t *c_acc, int *is_zero,
                                         dval_t ***den_terms,
                                         size_t *nden_terms,
                                         size_t *den_cap,
                                         dval_t *den)
{
    if (*is_zero) {
        dv_free(den);
        return;
    }

    if (dv_is_op(den, &ops_mul)) {
        dval_t *left = den->a;
        dval_t *right = den->b;

        dv_retain(left);
        dv_retain(right);
        dv_free(den);
        dv_append_denominator_factor(c_acc, is_zero, den_terms,
                                     nden_terms, den_cap, left);
        dv_append_denominator_factor(c_acc, is_zero, den_terms,
                                     nden_terms, den_cap, right);
        return;
    }

    if (dv_is_unnamed_const(den) &&
        (!den->binding_expr || den->binding_expr->kind == DV_BINDING_EXPR_NUMBER) &&
        num_is_real(den->c)) {
        number_t quotient = num_div(*c_acc, den->c);

        num_destroy(c_acc);
        *c_acc = num_scope_detach(quotient);
        dv_free(den);
        return;
    }

    dv_append_node(den_terms, nden_terms, den_cap, den);
}

void dv_split_division_terms(number_t *c_acc, int *is_zero,
                             dval_t **terms, size_t nterms,
                             dval_t ***den_terms, size_t *nden_terms,
                             size_t *den_cap)
{
    NUM_SCOPE(scope);
    for (size_t i = 0; i < nterms; ++i) {
        dval_t *term = terms[i];
        dval_t *num;
        dval_t *den;

        if (!dv_is_div(term))
            continue;

        num = term->a;
        den = term->b;
        dv_retain(num);
        dv_retain(den);
        dv_free(term);
        terms[i] = NULL;

        if (dv_is_unnamed_const(num) &&
            (!num->binding_expr || num->binding_expr->kind == DV_BINDING_EXPR_NUMBER) &&
            num_is_real(num->c)) {
            if (num_is_zero(num->c))
                *is_zero = 1;
            else {
                number_t product = num_mul(*c_acc, num->c);

                num_destroy(c_acc);
                *c_acc = num_scope_detach(product);
            }
            dv_free(num);
        } else {
            terms[i] = num;
        }

        if (*is_zero) {
            dv_free(den);
            continue;
        }

        dv_append_denominator_factor(c_acc, is_zero, den_terms, nden_terms,
                                     den_cap, den);
    }
}

void dv_combine_like_powers(dval_t **terms, size_t nterms)
{
    NUM_SCOPE(scope);
    for (size_t i = 0; i < nterms; ++i) {
        dval_t *term = terms[i];
        dval_t *base;
        number_t exponent;
        int combined_any = 0;

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

            {
                number_t other_exponent = pow_exponent(other);
                number_t sum = num_add(exponent, other_exponent);

                exponent = sum;
            }
            dv_free(other);
            terms[j] = NULL;
            combined_any = 1;
        }

        if (!combined_any && !dv_is_pow_d_expr(term)) {
            num_destroy(&exponent);
            continue;
        }

        if (num_is_one(exponent) && !dv_is_pow_d_expr(term)) {
            num_destroy(&exponent);
            continue;
        }

        dv_retain(base);
        dv_free(term);
        terms[i] = dv_make_pow_like(base, exponent);
        num_destroy(&exponent);
    }
}

void dv_cancel_common_powers(dval_t **terms, size_t nterms,
                             dval_t **den_terms, size_t nden_terms)
{
    NUM_SCOPE(scope);

    for (size_t i = 0; i < nterms; ++i) {
        dval_t *term = terms[i];
        dval_t *base;
        number_t exponent;

        if (!term)
            continue;

        base = pow_base(term);
        exponent = pow_exponent(term);

        for (size_t j = 0; j < nden_terms; ++j) {
            dval_t *den = den_terms[j];
            dval_t *common_base;
            number_t den_exponent;
            number_t diff;

            if (!den)
                continue;
            if (!dv_struct_eq(base, pow_base(den)))
                continue;

            den_exponent = pow_exponent(den);
            diff = num_sub(exponent, den_exponent);

            common_base = pow_base(term);
            dv_retain(common_base);
            dv_free(term);
            dv_free(den);

            if (num_eq(diff, NUM_ZERO)) {
                dv_free(common_base);
                terms[i] = NULL;
                den_terms[j] = NULL;
            } else if (num_gt(diff, NUM_ZERO)) {
                terms[i] = dv_make_pow_like(common_base, diff);
                den_terms[j] = NULL;
            } else {
                number_t den_diff = num_neg(diff);

                terms[i] = NULL;
                den_terms[j] = dv_make_pow_like(common_base, den_diff);
            }
            break;
        }
    }
}

void dv_combine_exp_terms(dval_t **terms, size_t nterms)
{
    for (size_t i = 0; i < nterms; ++i) {
        if (!dv_is_exp_expr(terms[i]))
            continue;

        for (size_t j = i + 1; j < nterms; ++j) {
            dval_t *addends[64];
            int na = 0;
            dval_t *sum;
            dval_t *simp;
            dval_t *combined;

            if (!dv_is_exp_expr(terms[j]))
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
                                         : compare_addend_bases(addends[t], key);
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
        if (!dv_is_sqrt_expr(terms[i]))
            continue;

        for (size_t j = i + 1; j < nterms; ++j) {
            dval_t *prod;
            dval_t *simp_arg;
            dval_t *raw;

            if (!dv_is_sqrt_expr(terms[j]))
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

dval_t *dv_try_expand_shallow_product(number_t c_acc,
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
    if (!(dv_is_addsub(t0) && dv_is_addsub(t1)))
        return NULL;

    int share = 0;
    const dval_t *t0c[2] = { t0->a, t0->b };
    const dval_t *t1c[2] = { t1->a, t1->b };

    for (int p = 0; p < 2 && !share; ++p) {
        for (int q = 0; q < 2 && !share; ++q) {
            const dval_t *u = t0c[p];
            const dval_t *v = t1c[q];

            if (dv_is_unnamed_const(u) || dv_is_unnamed_const(v))
                continue;
            if (dv_struct_eq(u, v))
                share = 1;
        }
    }

    if (!share ||
        dv_is_addsub(t0->a) || dv_is_addsub(t0->b) ||
        dv_is_addsub(t1->a) || dv_is_addsub(t1->b))
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

dval_t *dv_rebuild_product_chain(number_t c_acc, dval_t **terms, size_t nterms)
{
    dval_t *cur = NULL;

    if (!num_eq(c_acc, NUM_ONE)) {
        number_t normalized = dv_normalize_simple_rational_coeff(c_acc);

        cur = dv_new_const(normalized);
        num_destroy(&normalized);
    }

    for (size_t i = 0; i < nterms; ++i) {
        if (!terms[i])
            continue;
        if (dv_is_i_squared_term(terms[i])) {
            dval_t *neg_one = dv_new_const(NUM_NEG_ONE);

            dv_free(terms[i]);
            terms[i] = neg_one;
        }
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
    if (!cur) {
        number_t normalized = dv_normalize_simple_rational_coeff(c_acc);

        cur = dv_new_const(normalized);
        num_destroy(&normalized);
    }
    return cur;
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
