#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expr_bindings.h"
#include "expr_internal.h"

extern expr_t *expr_simplify(const expr_t *dv);

static int expr_is_i_squared_term(const expr_t *dv)
{
    return expr_is_pow_d_expr(dv) &&
           expr_is_op(dv->a, &ops_const) &&
           !dv->a->binding_expr &&
           (num_eq(dv->a->c, NUM_I) || num_eq(dv->a->c, NUM_NEG_I)) &&
           num_eq(dv->c, NUM_TWO);
}

static const expr_t *product_factor_base(const expr_t *dv)
{
    if (!dv)
        return NULL;
    if (expr_is_pow_d_expr(dv) && dv->a)
        return dv->a;
    if (expr_is_op(dv, &ops_pow) && dv->a)
        return dv->a;
    return dv;
}

static int is_unicode_subscript_byte(unsigned char c)
{
    return c == 0xE2u || c == 0x82u ||
           (c >= 0x80u && c <= 0x89u);
}

static int product_factor_is_primary_variable_name(const char *name)
{
    const unsigned char *p = (const unsigned char *)name;

    if (!p || (*p != 'x' && *p != 'y' && *p != 'z'))
        return 0;
    ++p;
    if (!*p)
        return 1;
    if (*p == '_')
        ++p;
    while (*p) {
        if ((*p >= '0' && *p <= '9') || is_unicode_subscript_byte(*p)) {
            ++p;
            continue;
        }
        return 0;
    }
    return 1;
}

static int product_factor_group(const expr_t *dv)
{
    const expr_t *base = product_factor_base(dv);

    if (!base)
        return 3;
    if ((expr_is_op(base, &ops_const) || expr_is_op(base, &ops_var)) &&
        base->name && *base->name) {
        if (expr_is_op(base, &ops_var) &&
            product_factor_is_primary_variable_name(base->name))
            return 1;
        return 0;
    }
    return 2;
}

static int compare_product_factors(const expr_t *lhs, const expr_t *rhs)
{
    int lg = product_factor_group(lhs);
    int rg = product_factor_group(rhs);

    return lg - rg;
}

static void expr_sort_product_factors(expr_t **terms, size_t nterms)
{
    for (size_t i = 1; i < nterms; ++i) {
        expr_t *key = terms[i];
        size_t j = i;

        while (j > 0 && compare_product_factors(terms[j - 1], key) > 0) {
            terms[j] = terms[j - 1];
            --j;
        }
        terms[j] = key;
    }
}

static void *expr_terms_xrealloc(void *ptr, size_t size)
{
    void *grown = realloc(ptr, size);

    if (grown)
        return grown;

    fprintf(stderr, "expr_simplify_terms: out of memory\n");
    abort();
}

static int term_coeff(const expr_t *term, const expr_t **base, number_t *coeff_out)
{
    if (expr_simplify_is_plain_real_const(term)) {
        *base = NULL;
        *coeff_out = num_clone(term->c);
        return 1;
    }
    if (expr_is_op(term, &ops_neg)) {
        if (expr_is_op(term->a, &ops_mul) &&
            expr_simplify_is_plain_real_const(term->a->a)) {
            *base = term->a->b;
            *coeff_out = num_neg(term->a->a->c);
            return 1;
        }
        *base = term->a;
        *coeff_out = num_clone(NUM_NEG_ONE);
        return 1;
    }
    if (expr_is_op(term, &ops_mul) &&
        expr_simplify_is_plain_real_const(term->a)) {
        *base = term->b;
        *coeff_out = num_clone(term->a->c);
        return 1;
    }
    *base = term;
    *coeff_out = num_clone(NUM_ONE);
    return 1;
}

static int split_leading_real_scalar(const expr_t *term,
                                     number_t *scalar_out,
                                     const expr_t **rest_out)
{
    if (expr_simplify_is_plain_real_const(term)) {
        *scalar_out = num_clone(term->c);
        *rest_out = NULL;
        return 1;
    }

    if (expr_is_op(term, &ops_mul) &&
        expr_simplify_is_plain_real_const(term->a)) {
        *scalar_out = num_clone(term->a->c);
        *rest_out = term->b;
        return 1;
    }

    *scalar_out = num_clone(NUM_ONE);
    *rest_out = term;
    return 0;
}

static expr_t *make_normalised_division_addend(const expr_t *num,
                                               const expr_t *den)
{
    expr_t *one;
    expr_t *out;

    if (!num && !den)
        return NULL;

    if (!den) {
        expr_retain((expr_t *)num);
        return (expr_t *)num;
    }

    if (!num) {
        one = expr_new_const(NUM_ONE);
        expr_retain((expr_t *)den);
        out = expr_div(one, (expr_t *)den);
        expr_free(one);
        expr_free((expr_t *)den);
        return out;
    }

    expr_retain((expr_t *)num);
    expr_retain((expr_t *)den);
    out = expr_div((expr_t *)num, (expr_t *)den);
    expr_free((expr_t *)num);
    expr_free((expr_t *)den);
    return out;
}

static expr_t *expr_try_fold_scaled_product(number_t coeff, expr_t *base)
{
    if (expr_is_op(base, &ops_mul)) {
        expr_t *left = base->a;
        expr_t *right = base->b;
        expr_t *scaled_left;
        expr_t *scaled_right;
        expr_t *r;

        if (expr_simplify_is_plain_real_const(left)) {
            expr_retain(left);
            expr_retain(right);
            expr_free(base);
            scaled_left = expr_make_scaled(coeff, left);
            r = expr_mul(scaled_left, right);
            expr_free(scaled_left);
            expr_free(right);
            return r;
        }

        if (expr_simplify_is_plain_real_const(right)) {
            expr_retain(left);
            expr_retain(right);
            expr_free(base);
            scaled_right = expr_make_scaled(coeff, right);
            r = expr_mul(left, scaled_right);
            expr_free(left);
            expr_free(scaled_right);
            return r;
        }

        expr_retain(left);
        scaled_left = expr_try_fold_scaled_product(coeff, left);
        if (scaled_left) {
            expr_retain(right);
            r = expr_mul(scaled_left, right);
            expr_free(scaled_left);
            expr_free(right);
            expr_free(base);
            return r;
        }
        expr_free(left);

        expr_retain(right);
        scaled_right = expr_try_fold_scaled_product(coeff, right);
        if (scaled_right) {
            expr_retain(left);
            r = expr_mul(left, scaled_right);
            expr_free(left);
            expr_free(scaled_right);
            expr_free(base);
            return r;
        }
        expr_free(right);
    }

    return NULL;
}

expr_t *expr_make_scaled(number_t coeff, expr_t *base)
{
    NUM_SCOPE(scope);
    if (num_is_zero(coeff)) { expr_free(base); return expr_new_const(NUM_ZERO); }
    if (num_eq(coeff, NUM_ONE))  return base;
    if (num_eq(coeff, NUM_NEG_ONE)) {
        expr_t *positive = expr_simplify_positive_part_if_negative(base);

        if (positive) {
            expr_free(base);
            return positive;
        }
        expr_t *r = expr_neg(base);
        expr_free(base);
        return r;
    }
    if (expr_is_unnamed_const(base) && base->binding_expr && num_is_real(base->c)) {
        number_t leading_coeff;
        expr_binding_expr_t *rest_expr = NULL;

        if (expr_binding_expr_split_leading_number(base->binding_expr,
                                                &leading_coeff,
                                                &rest_expr)) {
            if (rest_expr) {
                number_t folded = num_mul(coeff, leading_coeff);
                expr_t *rest = expr_binding_expr_eval_expr(rest_expr);
                expr_t *out;

                expr_binding_expr_free(rest_expr);
                num_destroy(&leading_coeff);
                expr_free(base);
                out = expr_make_scaled(folded, rest);
                num_destroy(&folded);
                return out;
            }
            num_destroy(&leading_coeff);
        }
        char *coeff_text = num_to_string(coeff);
        expr_binding_expr_t *coeff_expr =
            expr_binding_expr_new_number_text(coeff_text ? coeff_text : "NAN");
        expr_binding_expr_t *expr =
            expr_binding_expr_new_mul(coeff_expr, expr_binding_expr_clone(base->binding_expr));
        number_t scaled = num_mul(coeff, base->c);
        expr_t *out = expr_new_const(scaled);

        expr = expr_binding_expr_simplify(expr);
        free(coeff_text);
        num_destroy(&scaled);
        expr_free(base);
        out->binding_expr = expr;
        return out;
    }
    if (expr_is_op(base, &ops_div)) {
        expr_t *num = base->a;
        expr_t *den = base->b;
        expr_t *scaled_num;
        expr_t *r;

        expr_retain(num);
        expr_retain(den);
        expr_free(base);

        scaled_num = expr_make_scaled(coeff, num);
        r = expr_div(scaled_num, den);
        expr_free(scaled_num);
        expr_free(den);
        return r;
    }
    {
        expr_t *folded_product = expr_try_fold_scaled_product(coeff, base);

        if (folded_product)
            return folded_product;
    }
    if (expr_is_op(base, &ops_mul) &&
        expr_is_unnamed_const(base->a) &&
        (!base->a->binding_expr ||
         base->a->binding_expr->kind == EXPR_BINDING_EXPR_NUMBER) &&
        num_is_real(base->a->c)) {
        number_t folded = num_mul(coeff, base->a->c);
        expr_retain(base->b);
        expr_t *rest = base->b;
        expr_free(base);
        expr_t *out = expr_make_scaled(folded, rest);

        return out;
    }
    if (expr_is_op(base, &ops_mul) &&
        expr_is_op(base->a, &ops_mul) &&
        expr_is_unnamed_const(base->a->a) &&
        (!base->a->a->binding_expr ||
         base->a->a->binding_expr->kind == EXPR_BINDING_EXPR_NUMBER) &&
        num_is_real(base->a->a->c)) {
        number_t folded = num_mul(coeff, base->a->a->c);
        expr_retain(base->a->b);
        expr_retain(base->b);
        expr_t *inner = expr_mul(base->a->b, base->b);
        expr_free(base->a->b);
        expr_free(base->b);
        expr_free(base);
        expr_t *out = expr_make_scaled(folded, inner);

        return out;
    }
    number_t normalised = expr_simplify_normalise_simple_rational_coeff(coeff);
    expr_t *cn = expr_new_const(normalised);
    expr_t *r = expr_mul(cn, base);

    num_destroy(&normalised);
    expr_free(cn);
    expr_free(base);
    return r;
}

static int addend_group(const expr_t *dv)
{
    if (dv->ops->arity == EXPR_OP_UNARY)                  return 0;
    if (expr_is_op(dv, &ops_var))                            return 1;
    if (expr_is_op(dv, &ops_const) && dv->name && *dv->name) return 2;
    return 3;
}

typedef struct {
    int group;
    int shape;
    const char *name;
} addend_sort_key_t;

static int addend_is_default_const(const expr_t *dv)
{
    const char *canon;
    number_t value;
    int is_default;

    if (!dv || !expr_is_op(dv, &ops_const) || !dv->name || !*dv->name)
        return 0;

    canon = expr_default_constant_canonical_name(dv->name);
    if (canon && strcmp(canon, "@tau") == 0)
        return 0;

    is_default = expr_get_default_constant_num(dv->name, &value);
    if (is_default)
        num_destroy(&value);
    return is_default;
}

static void addend_consider_leaf_key(const expr_t *dv, addend_sort_key_t *key)
{
    int group;

    if (!dv)
        return;
    if (!dv->name || !*dv->name)
        return;
    if (!expr_is_op(dv, &ops_var) && !expr_is_op(dv, &ops_const))
        return;

    if (num_is_nan(dv->c))
        group = 0;
    else if (addend_is_default_const(dv))
        group = 1;
    else if (expr_is_op(dv, &ops_const))
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

static void addend_collect_product_key(const expr_t *dv, addend_sort_key_t *key)
{
    if (!dv)
        return;

    if (expr_is_op(dv, &ops_mul)) {
        addend_collect_product_key(dv->a, key);
        addend_collect_product_key(dv->b, key);
        return;
    }

    addend_consider_leaf_key(dv, key);
}

static addend_sort_key_t addend_sort_key(const expr_t *dv)
{
    addend_sort_key_t key = {4, 2, ""};

    if (!dv)
        return key;

    if (expr_is_op(dv, &ops_mul)) {
        key.shape = 0;
        addend_collect_product_key(dv, &key);
        return key;
    }

    if (expr_is_op(dv, &ops_div)) {
        key.shape = 1;
        addend_collect_product_key(dv->a, &key);
        addend_collect_product_key(dv->b, &key);
        return key;
    }

    addend_consider_leaf_key(dv, &key);
    return key;
}

void expr_combine_common_denominator_addends(addend_t *terms, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        expr_t *ibase = terms[i].base;
        expr_t *sum_num = NULL;
        expr_t *simp_num = NULL;
        expr_t *combined = NULL;
        int merged_any = 0;

        if (!expr_is_div(ibase))
            continue;

        for (size_t j = i + 1; j < n; ++j) {
            expr_t *jbase = terms[j].base;
            expr_t *i_num_term;
            expr_t *j_num_term;
            expr_t *tmp;

            if (!expr_is_div(jbase))
                continue;
            if (!expr_struct_eq(ibase->b, jbase->b))
                continue;

            if (!merged_any) {
                expr_retain(ibase->a);
                sum_num = expr_make_scaled(terms[i].coeff, ibase->a);
                num_destroy(&terms[i].coeff);
                terms[i].coeff = num_clone(NUM_ONE);
                merged_any = 1;
            }

            expr_retain(jbase->a);
            j_num_term = expr_make_scaled(terms[j].coeff, jbase->a);
            i_num_term = sum_num;
            tmp = expr_add(i_num_term, j_num_term);
            expr_free(i_num_term);
            expr_free(j_num_term);
            sum_num = expr_simplify(tmp);
            expr_free(tmp);

            expr_free(jbase);
            terms[j].base = NULL;
            num_destroy(&terms[j].coeff);
            terms[j].coeff = num_clone(NUM_ZERO);
        }

        if (!merged_any)
            continue;

        expr_retain(ibase->b);
        simp_num = expr_simplify(sum_num);
        expr_free(sum_num);
        combined = expr_div(simp_num, ibase->b);
        expr_free(simp_num);
        expr_free(ibase->b);
        {
            expr_t *combined_raw = combined;
            combined = expr_simplify(combined_raw);
            expr_free(combined_raw);
        }
        expr_free(ibase);
        terms[i].base = combined;
        num_destroy(&terms[i].coeff);
        terms[i].coeff = num_clone(NUM_ONE);
    }
}

static int compare_addend_bases(const expr_t *lhs, const expr_t *rhs)
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

void expr_sort_addends(addend_t *terms, size_t n)
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

static int expr_contains_addsub_normalised(const expr_t *dv)
{
    if (!dv)
        return 0;
    if (expr_is_addsub(dv))
        return 1;
    return expr_contains_addsub_normalised(dv->a) ||
           expr_contains_addsub_normalised(dv->b);
}

void expr_collect_addends(expr_t *dv, number_t scale, number_t *c_const,
                        addend_t **terms, size_t *n, size_t *cap)
{
    NUM_SCOPE(scope);
    if (!dv)
        return;
    if (expr_is_op(dv, &ops_add)) {
        expr_collect_addends(dv->a, scale, c_const, terms, n, cap);
        expr_collect_addends(dv->b, scale, c_const, terms, n, cap);
        return;
    }
    if (expr_is_op(dv, &ops_sub)) {
        number_t neg_scale = num_neg(scale);

        expr_collect_addends(dv->a, scale, c_const, terms, n, cap);
        expr_collect_addends(dv->b, neg_scale, c_const, terms, n, cap);
        return;
    }
    if (expr_is_op(dv, &ops_neg)) {
        if (expr_is_addsub(dv->a)) {
            number_t neg_scale = num_neg(scale);

            expr_collect_addends(dv->a, neg_scale, c_const, terms, n, cap);
            return;
        }
        if (expr_is_op(dv->a, &ops_mul) &&
            expr_is_unnamed_const(dv->a->a) &&
            expr_is_addsub(dv->a->b)) {
            number_t ns;
            number_t coeff_num = num_new();
            number_t neg_scale = num_neg(scale);

            if (expr_simplify_try_get_plain_real_const(dv->a->a, &coeff_num)) {
                ns = num_mul(neg_scale, coeff_num);
                expr_collect_addends(dv->a->b, ns, c_const, terms, n, cap);
                return;
            }
        }
    }
    if (expr_is_op(dv, &ops_mul) &&
        expr_is_unnamed_const(dv->a) &&
        expr_is_addsub(dv->b)) {
        number_t ns;
        number_t coeff_num = num_new();

        if (expr_simplify_try_get_plain_real_const(dv->a, &coeff_num)) {
            ns = num_mul(scale, coeff_num);
            expr_collect_addends(dv->b, ns, c_const, terms, n, cap);
            return;
        }
    }
    if (expr_is_op(dv, &ops_mul) &&
        expr_is_op(dv->a, &ops_mul) &&
        expr_is_unnamed_const(dv->a->a)) {
        number_t ns;
        number_t coeff_num = num_new();
        expr_t *raw;
        expr_t *simp;

        if (expr_simplify_try_get_plain_real_const(dv->a->a, &coeff_num)) {
            ns = num_mul(scale, coeff_num);

            expr_retain(dv->a->b);
            expr_retain(dv->b);
            raw = expr_mul(dv->a->b, dv->b);
            expr_free(dv->a->b);
            expr_free(dv->b);
            simp = expr_simplify(raw);
            expr_collect_addends(simp ? simp : raw, ns, c_const, terms, n, cap);
            expr_free(simp);
            expr_free(raw);
            return;
        }
    }
    if (expr_is_div(dv)) {
        number_t num_scalar = num_new();
        number_t den_scalar = num_new();
        number_t ns;
        const expr_t *num_rest = NULL;
        const expr_t *den_rest = NULL;
        int changed_num;
        int changed_den;

        changed_num = split_leading_real_scalar(dv->a, &num_scalar, &num_rest);
        changed_den = split_leading_real_scalar(dv->b, &den_scalar, &den_rest);

        if (changed_num || changed_den) {
            expr_t *normalised;

            ns = num_mul(scale, num_scalar);
            ns = num_div(ns, den_scalar);
            normalised = make_normalised_division_addend(num_rest, den_rest);
            if (normalised) {
                if ((!num_rest && den_rest) ||
                    !expr_contains_addsub_normalised(normalised) ||
                    expr_struct_eq(normalised, dv)) {
                    size_t i;

                    for (i = 0; i < *n; ++i) {
                        if (expr_struct_eq((*terms)[i].base, normalised)) {
                            number_t sum = num_add((*terms)[i].coeff, ns);

                            num_destroy(&(*terms)[i].coeff);
                            (*terms)[i].coeff = num_scope_detach(sum);
                            expr_free(normalised);
                            return;
                        }
                    }
                    if (*n == *cap) {
                        *cap = *cap ? *cap * 2 : 8;
                        *terms = expr_terms_xrealloc(*terms, *cap * sizeof(addend_t));
                    }
                    (*terms)[*n].base = normalised;
                    (*terms)[*n].coeff = num_scope_detach(ns);
                    (*n)++;
                    return;
                }
                expr_collect_addends(normalised, ns, c_const, terms, n, cap);
                expr_free(normalised);
            } else {
                number_t sum = num_add(*c_const, ns);

                num_destroy(c_const);
                *c_const = num_scope_detach(sum);
            }
            return;
        }
    }

    const expr_t *base;
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
        if (expr_struct_eq((*terms)[i].base, base)) {
            number_t sum = num_add((*terms)[i].coeff, coeff);

            num_destroy(&(*terms)[i].coeff);
            (*terms)[i].coeff = num_scope_detach(sum);
            return;
        }
    }
    if (*n == *cap) {
        *cap = *cap ? *cap * 2 : 8;
        *terms = expr_terms_xrealloc(*terms, *cap * sizeof(addend_t));
    }
    expr_retain((expr_t *)base);
    (*terms)[*n].base = (expr_t *)base;
    (*terms)[*n].coeff = num_scope_detach(coeff);
    (*n)++;
}

int expr_extract_common_addend_coeff(const addend_t *terms, size_t n,
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

static void flatten_add(expr_t *root, expr_t **addends, int *na, int max)
{
    expr_t *stk[64];
    int sp = 0;

    stk[sp++] = root;
    while (sp > 0 && *na < max) {
        expr_t *dv = stk[--sp];
        if (expr_is_op(dv, &ops_add)) {
            if (sp < 63) {
                stk[sp++] = dv->b;
                stk[sp++] = dv->a;
            }
        } else {
            expr_retain(dv);
            addends[(*na)++] = dv;
        }
    }
}

static expr_t *expand_product(const expr_t *u, const expr_t *v)
{
    if (expr_is_op(u, &ops_add)) {
        expr_t *l = expand_product(u->a, v);
        expr_t *r = expand_product(u->b, v);
        expr_t *s = expr_add(l, r);
        expr_free(l);
        expr_free(r);
        return s;
    }
    if (expr_is_op(u, &ops_sub)) {
        expr_t *l = expand_product(u->a, v);
        expr_t *r = expand_product(u->b, v);
        expr_t *s = expr_sub(l, r);
        expr_free(l);
        expr_free(r);
        return s;
    }
    if (expr_is_addsub(v))
        return expand_product(v, u);

    expr_retain((expr_t *)u);
    expr_retain((expr_t *)v);
    expr_t *prod = expr_mul((expr_t *)u, (expr_t *)v);
    expr_free((expr_t *)u);
    expr_free((expr_t *)v);
    return prod;
}

void expr_free_node_array(expr_t **nodes, size_t count)
{
    if (!nodes)
        return;
    for (size_t i = 0; i < count; ++i)
        expr_free(nodes[i]);
    free(nodes);
}

void expr_append_node(expr_t ***nodes, size_t *count, size_t *cap, expr_t *node)
{
    if (*count == *cap) {
        *cap = (*cap == 0) ? 4 : (*cap * 2);
        *nodes = expr_terms_xrealloc(*nodes, *cap * sizeof(**nodes));
    }
    (*nodes)[(*count)++] = node;
}

static number_t pow_exponent(const expr_t *dv)
{
    if (expr_is_sqrt_expr(dv))
        return num_div(NUM_ONE, NUM_TWO);
    if (!expr_is_op(dv, &ops_pow_d))
        return num_clone(NUM_ONE);
    return num_clone(dv->c);
}

static expr_t *pow_base(const expr_t *dv)
{
    if (expr_is_sqrt_expr(dv))
        return dv->a;
    return expr_is_op(dv, &ops_pow_d) ? dv->a : (expr_t *)dv;
}

expr_t *expr_make_pow_like(expr_t *base, number_t exponent)
{
    if (num_eq(exponent, NUM_ZERO)) {
        expr_free(base);
        return expr_new_const(NUM_ONE);
    }
    if (num_eq(exponent, NUM_ONE))
        return base;
    if (num_eq(exponent, NUM_TWO) &&
        expr_is_op(base, &ops_const) &&
        !base->binding_expr &&
        (num_eq(base->c, NUM_I) || num_eq(base->c, NUM_NEG_I))) {
        expr_free(base);
        return expr_new_const(NUM_NEG_ONE);
    }

    expr_t *pow = expr_pow(base, &exponent);
    expr_free(base);
    return pow;
}

static void expr_append_denominator_factor(number_t *c_acc, int *is_zero,
                                         expr_t ***den_terms,
                                         size_t *nden_terms,
                                         size_t *den_cap,
                                         expr_t *den)
{
    if (*is_zero) {
        expr_free(den);
        return;
    }

    if (expr_is_op(den, &ops_mul)) {
        expr_t *left = den->a;
        expr_t *right = den->b;

        expr_retain(left);
        expr_retain(right);
        expr_free(den);
        expr_append_denominator_factor(c_acc, is_zero, den_terms,
                                     nden_terms, den_cap, left);
        expr_append_denominator_factor(c_acc, is_zero, den_terms,
                                     nden_terms, den_cap, right);
        return;
    }

    if (expr_is_unnamed_const(den) &&
        (!den->binding_expr || den->binding_expr->kind == EXPR_BINDING_EXPR_NUMBER) &&
        num_is_real(den->c)) {
        number_t quotient = num_div(*c_acc, den->c);

        num_destroy(c_acc);
        *c_acc = num_scope_detach(quotient);
        expr_free(den);
        return;
    }

    expr_append_node(den_terms, nden_terms, den_cap, den);
}

void expr_split_division_terms(number_t *c_acc, int *is_zero,
                             expr_t **terms, size_t nterms,
                             expr_t ***den_terms, size_t *nden_terms,
                             size_t *den_cap)
{
    NUM_SCOPE(scope);
    for (size_t i = 0; i < nterms; ++i) {
        expr_t *term = terms[i];
        expr_t *num;
        expr_t *den;

        if (!expr_is_div(term))
            continue;

        num = term->a;
        den = term->b;
        expr_retain(num);
        expr_retain(den);
        expr_free(term);
        terms[i] = NULL;

        if (expr_is_unnamed_const(num) &&
            (!num->binding_expr || num->binding_expr->kind == EXPR_BINDING_EXPR_NUMBER) &&
            num_is_real(num->c)) {
            if (num_is_zero(num->c))
                *is_zero = 1;
            else {
                number_t product = num_mul(*c_acc, num->c);

                num_destroy(c_acc);
                *c_acc = num_scope_detach(product);
            }
            expr_free(num);
        } else {
            terms[i] = num;
        }

        if (*is_zero) {
            expr_free(den);
            continue;
        }

        expr_append_denominator_factor(c_acc, is_zero, den_terms, nden_terms,
                                     den_cap, den);
    }
}

void expr_combine_like_powers(expr_t **terms, size_t nterms)
{
    NUM_SCOPE(scope);
    for (size_t i = 0; i < nterms; ++i) {
        expr_t *term = terms[i];
        expr_t *base;
        number_t exponent;
        int combined_any = 0;

        if (!term)
            continue;

        base = pow_base(term);
        exponent = pow_exponent(term);

        for (size_t j = i + 1; j < nterms; ++j) {
            expr_t *other = terms[j];

            if (!other)
                continue;
            if (!expr_struct_eq(base, pow_base(other)))
                continue;

            {
                number_t other_exponent = pow_exponent(other);
                number_t sum = num_add(exponent, other_exponent);

                exponent = sum;
            }
            expr_free(other);
            terms[j] = NULL;
            combined_any = 1;
        }

        if (!combined_any && !expr_is_pow_d_expr(term)) {
            num_destroy(&exponent);
            continue;
        }

        if (num_is_one(exponent) && !expr_is_pow_d_expr(term)) {
            num_destroy(&exponent);
            continue;
        }

        expr_retain(base);
        expr_free(term);
        terms[i] = expr_make_pow_like(base, exponent);
        num_destroy(&exponent);
    }
}

void expr_cancel_common_powers(expr_t **terms, size_t nterms,
                             expr_t **den_terms, size_t nden_terms)
{
    NUM_SCOPE(scope);

    for (size_t i = 0; i < nterms; ++i) {
        expr_t *term = terms[i];
        expr_t *base;
        number_t exponent;

        if (!term)
            continue;

        base = pow_base(term);
        exponent = pow_exponent(term);

        for (size_t j = 0; j < nden_terms; ++j) {
            expr_t *den = den_terms[j];
            expr_t *common_base;
            number_t den_exponent;
            number_t diff;

            if (!den)
                continue;
            if (!expr_struct_eq(base, pow_base(den)))
                continue;

            den_exponent = pow_exponent(den);
            diff = num_sub(exponent, den_exponent);

            common_base = pow_base(term);
            expr_retain(common_base);
            expr_free(term);
            expr_free(den);

            if (num_eq(diff, NUM_ZERO)) {
                expr_free(common_base);
                terms[i] = NULL;
                den_terms[j] = NULL;
            } else if (num_gt(diff, NUM_ZERO)) {
                terms[i] = expr_make_pow_like(common_base, diff);
                den_terms[j] = NULL;
            } else {
                number_t den_diff = num_neg(diff);

                terms[i] = NULL;
                den_terms[j] = expr_make_pow_like(common_base, den_diff);
            }
            break;
        }
    }
}

void expr_combine_exp_terms(expr_t **terms, size_t nterms)
{
    for (size_t i = 0; i < nterms; ++i) {
        if (!expr_is_exp_expr(terms[i]))
            continue;

        for (size_t j = i + 1; j < nterms; ++j) {
            expr_t *addends[64];
            int na = 0;
            expr_t *sum;
            expr_t *simp;
            expr_t *combined;

            if (!expr_is_exp_expr(terms[j]))
                continue;

            flatten_add(terms[i]->a, addends, &na, 64);
            flatten_add(terms[j]->a, addends, &na, 64);
            expr_free(terms[i]);
            expr_free(terms[j]);
            terms[j] = NULL;

            for (int s = 1; s < na; ++s) {
                expr_t *key = addends[s];
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
                expr_t *tmp = expr_add(sum, addends[k]);
                expr_free(sum);
                expr_free(addends[k]);
                sum = tmp;
            }

            simp = expr_simplify(sum);
            expr_free(sum);
            combined = expr_exp(simp);
            expr_free(simp);
            terms[i] = expr_simplify(combined);
            expr_free(combined);
        }
    }
}

void expr_merge_sqrt_terms(expr_t **terms, size_t nterms)
{
    for (size_t i = 0; i < nterms; ++i) {
        if (!expr_is_sqrt_expr(terms[i]))
            continue;

        for (size_t j = i + 1; j < nterms; ++j) {
            expr_t *prod;
            expr_t *simp_arg;
            expr_t *raw;

            if (!expr_is_sqrt_expr(terms[j]))
                continue;

            expr_retain(terms[i]->a);
            expr_retain(terms[j]->a);
            prod = expr_mul(terms[i]->a, terms[j]->a);
            expr_free(terms[i]->a);
            expr_free(terms[j]->a);
            simp_arg = expr_simplify(prod);
            expr_free(prod);
            raw = expr_sqrt(simp_arg);
            expr_free(simp_arg);
            expr_free(terms[i]);
            expr_free(terms[j]);
            terms[j] = NULL;
            terms[i] = expr_simplify(raw);
            expr_free(raw);
            break;
        }
    }
}

expr_t *expr_try_expand_shallow_product(number_t c_acc,
                                      expr_t **terms, size_t nterms,
                                      expr_t **den_terms, size_t nden_terms)
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

    expr_t *t0 = terms[first];
    expr_t *t1 = terms[second];
    if (!(expr_is_addsub(t0) && expr_is_addsub(t1)))
        return NULL;

    int share = 0;
    const expr_t *t0c[2] = { t0->a, t0->b };
    const expr_t *t1c[2] = { t1->a, t1->b };

    for (int p = 0; p < 2 && !share; ++p) {
        for (int q = 0; q < 2 && !share; ++q) {
            const expr_t *u = t0c[p];
            const expr_t *v = t1c[q];

            if (expr_is_unnamed_const(u) || expr_is_unnamed_const(v))
                continue;
            if (expr_struct_eq(u, v))
                share = 1;
        }
    }

    if (!share ||
        expr_is_addsub(t0->a) || expr_is_addsub(t0->b) ||
        expr_is_addsub(t1->a) || expr_is_addsub(t1->b))
        return NULL;

    expr_t *expanded = expand_product(t0, t1);
    expr_t *simp;

    free(den_terms);
    free(terms);
    expr_free(t0);
    expr_free(t1);

    simp = expr_simplify(expanded);
    expr_free(expanded);
    return expr_make_scaled(c_acc, simp);
}

expr_t *expr_rebuild_product_chain(number_t c_acc, expr_t **terms, size_t nterms)
{
    expr_t *cur = NULL;

    expr_sort_product_factors(terms, nterms);

    if (!num_eq(c_acc, NUM_ONE)) {
        number_t normalised = expr_simplify_normalise_simple_rational_coeff(c_acc);

        cur = expr_new_const(normalised);
        num_destroy(&normalised);
    }

    for (size_t i = 0; i < nterms; ++i) {
        if (!terms[i])
            continue;
        if (expr_is_i_squared_term(terms[i])) {
            expr_t *neg_one = expr_new_const(NUM_NEG_ONE);

            expr_free(terms[i]);
            terms[i] = neg_one;
        }
        if (!cur) {
            cur = terms[i];
        } else {
            expr_t *tmp = expr_mul(cur, terms[i]);
            expr_free(cur);
            expr_free(terms[i]);
            cur = tmp;
        }
    }

    free(terms);
    if (!cur) {
        number_t normalised = expr_simplify_normalise_simple_rational_coeff(c_acc);

        cur = expr_new_const(normalised);
        num_destroy(&normalised);
    }
    return cur;
}

expr_t *expr_rebuild_division_chain(expr_t **den_terms, size_t nden_terms)
{
    expr_t *denom = NULL;

    for (size_t i = 0; i < nden_terms; ++i) {
        if (!den_terms[i])
            continue;
        if (!denom) {
            denom = den_terms[i];
        } else {
            expr_t *tmp = expr_mul(denom, den_terms[i]);
            expr_free(denom);
            expr_free(den_terms[i]);
            denom = tmp;
        }
    }

    free(den_terms);
    return denom;
}
