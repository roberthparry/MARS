#include <stdlib.h>
#include <string.h>

#include "expr_bindings.h"
#include "expr_internal.h"

static int const_is_protected_bound_symbol(const expr_t *dv)
{
    number_t value;
    int has_default;
    int is_default;

    if (!dv || !expr_is_const(dv) || !dv->binding_expr ||
        !dv->name || !*dv->name)
        return 0;

    has_default = expr_get_default_constant_num(dv->name, &value);
    is_default = has_default && num_eq(dv->c, value);
    if (has_default)
        num_destroy(&value);

    return !is_default;
}

static int const_struct_eq(const expr_t *u, const expr_t *v)
{
    if (const_is_protected_bound_symbol(u) ||
        const_is_protected_bound_symbol(v)) {
        return const_is_protected_bound_symbol(u) &&
               const_is_protected_bound_symbol(v) &&
               u->name && v->name &&
               strcmp(u->name, v->name) == 0 &&
               expr_binding_expr_struct_eq(u->binding_expr, v->binding_expr);
    }

    if (u->binding_expr || v->binding_expr) {
        if (expr_binding_expr_struct_eq(u->binding_expr, v->binding_expr))
            return 1;
        return num_eq(u->c, v->c) &&
               (!u->binding_expr ||
                u->binding_expr->kind == EXPR_BINDING_EXPR_NUMBER ||
                u->binding_expr->kind == EXPR_BINDING_EXPR_CONST) &&
               (!v->binding_expr ||
                v->binding_expr->kind == EXPR_BINDING_EXPR_NUMBER ||
                v->binding_expr->kind == EXPR_BINDING_EXPR_CONST);
    }

    return num_eq(u->c, v->c);
}

static void *expr_match_xrealloc(void *ptr, size_t size)
{
    void *grown = realloc(ptr, size);

    if (grown)
        return grown;

    abort();
}

static void collect_mul_factors_borrowed(const expr_t *dv,
                                         number_t *c_acc,
                                         const expr_t ***terms,
                                         size_t *nterms,
                                         size_t *cap)
{
    NUM_SCOPE(scope);
    if (expr_is_unnamed_const(dv) && num_is_real(dv->c) &&
        (!dv->binding_expr || dv->binding_expr->kind == EXPR_BINDING_EXPR_NUMBER)) {
        number_t product = num_mul(*c_acc, dv->c);

        num_destroy(c_acc);
        *c_acc = num_scope_detach(product);
        return;
    }
    if (expr_is_neg(dv)) {
        number_t negated = num_neg(*c_acc);

        num_destroy(c_acc);
        *c_acc = num_scope_detach(negated);
        collect_mul_factors_borrowed(dv->a, c_acc, terms, nterms, cap);
        return;
    }
    if (expr_is_mul(dv)) {
        collect_mul_factors_borrowed(dv->a, c_acc, terms, nterms, cap);
        collect_mul_factors_borrowed(dv->b, c_acc, terms, nterms, cap);
        return;
    }
    if (*nterms == *cap) {
        *cap = (*cap == 0) ? 4 : (*cap * 2);
        *terms = expr_match_xrealloc((void *)*terms, *cap * sizeof(**terms));
    }
    (*terms)[(*nterms)++] = dv;
}

static int mul_struct_eq(const expr_t *u, const expr_t *v)
{
    NUM_SCOPE(scope);
    const expr_t **u_terms = NULL;
    const expr_t **v_terms = NULL;
    unsigned char *matched = NULL;
    size_t u_n = 0, v_n = 0, u_cap = 0, v_cap = 0;
    number_t u_coeff = num_const(NUM_ONE);
    number_t v_coeff = num_const(NUM_ONE);
    int equal = 1;

    collect_mul_factors_borrowed(u, &u_coeff, &u_terms, &u_n, &u_cap);
    collect_mul_factors_borrowed(v, &v_coeff, &v_terms, &v_n, &v_cap);

    if (!num_eq(u_coeff, v_coeff) || u_n != v_n) {
        equal = 0;
        goto cleanup;
    }

    matched = calloc(v_n ? v_n : 1, sizeof(*matched));
    if (!matched) {
        equal = 0;
        goto cleanup;
    }

    for (size_t i = 0; i < u_n && equal; ++i) {
        int found = 0;

        for (size_t j = 0; j < v_n; ++j) {
            if (matched[j])
                continue;
            if (!expr_struct_eq(u_terms[i], v_terms[j]))
                continue;
            matched[j] = 1;
            found = 1;
            break;
        }

        if (!found)
            equal = 0;
    }

cleanup:
    num_destroy(&u_coeff);
    num_destroy(&v_coeff);
    free(matched);
    free((void *)u_terms);
    free((void *)v_terms);
    return equal;
}

static int sqrt_half_pow_struct_eq(const expr_t *u, const expr_t *v)
{
    if (expr_is_sqrt_expr(u) &&
        expr_is_pow_d_expr(v) &&
        num_eq(v->c, NUM_HALF))
        return expr_struct_eq(u->a, v->a);
    if (expr_is_pow_d_expr(u) &&
        num_eq(u->c, NUM_HALF) &&
        expr_is_sqrt_expr(v))
        return expr_struct_eq(u->a, v->a);
    return 0;
}

int expr_struct_eq(const expr_t *u, const expr_t *v)
{
    if (u == v)
        return 1;
    if (!u || !v)
        return 0;
    if (sqrt_half_pow_struct_eq(u, v))
        return 1;
    if (u->ops != v->ops)
        return 0;
    if (expr_is_const(u))
        return const_struct_eq(u, v);
    if (expr_is_var(u))
        return u->var_id != 0 && u->var_id == v->var_id;
    if (expr_is_mul(u))
        return mul_struct_eq(u, v);
    if (expr_is_neg(u))
        return expr_struct_eq(u->a, v->a);
    if (expr_is_pow_d_expr(u))
        return expr_struct_eq(u->a, v->a) && num_eq(u->c, v->c);
    return expr_struct_eq(u->a, v->a) && expr_struct_eq(u->b, v->b);
}

static bool expr_simplify_same_named_leaf_local(const expr_t *left,
                                                const expr_t *right)
{
    const char *left_name = (left && left->name && *left->name) ? left->name : NULL;
    const char *right_name = (right && right->name && *right->name) ? right->name : NULL;

    if (left && right && left->ops && right->ops &&
        left->ops->kind == EXPR_KIND_VAR &&
        right->ops->kind == EXPR_KIND_VAR &&
        left->var_id != 0 && right->var_id != 0) {
        return left->var_id == right->var_id;
    }

    if (left_name || right_name)
        return left_name && right_name && strcmp(left_name, right_name) == 0 &&
               num_eq(left->c, right->c);
    return left && right && num_eq(left->c, right->c);
}

static bool expr_simplify_same_shape_local(const expr_t *left,
                                           const expr_t *right)
{
    if (left == right)
        return true;
    if (!left || !right || !left->ops || !right->ops || left->ops != right->ops)
        return false;

    if (left->ops->kind == EXPR_KIND_VAR || left->ops->kind == EXPR_KIND_CONST)
        return expr_simplify_same_named_leaf_local(left, right);

    if (left->ops->kind == EXPR_KIND_POW_D)
        return num_eq(left->c, right->c) &&
               expr_simplify_same_shape_local(left->a, right->a);

    if (left->ops->arity == EXPR_OP_UNARY)
        return expr_simplify_same_shape_local(left->a, right->a);

    if (left->ops->arity == EXPR_OP_BINARY) {
        if (expr_simplify_same_shape_local(left->a, right->a) &&
            expr_simplify_same_shape_local(left->b, right->b)) {
            return true;
        }
        if ((left->ops->kind == EXPR_KIND_MUL ||
             left->ops->kind == EXPR_KIND_ADD) &&
            expr_simplify_same_shape_local(left->a, right->b) &&
            expr_simplify_same_shape_local(left->b, right->a)) {
            return true;
        }
    }

    return false;
}

static bool expr_simplify_equal_exact_local(const expr_t *left,
                                            const expr_t *right)
{
    expr_t *diff = NULL;
    expr_t *simplified = NULL;
    bool equal = false;

    if (left == right)
        return true;
    if (!left || !right)
        return false;

    diff = expr_sub(left, right);
    simplified = expr_simplify_owned(diff);
    if (!simplified)
        return false;

    equal = expr_is_exact_zero(simplified);
    expr_free(simplified);
    return equal;
}

bool expr_simplify_same_factor(const expr_t *left, const expr_t *right)
{
    return expr_struct_eq(left, right) ||
           expr_simplify_same_shape_local(left, right) ||
           expr_simplify_equal_exact_local(left, right);
}

static void expr_simplify_additive_terms_clear_local(addend_t *terms,
                                                     size_t count)
{
    for (size_t i = 0u; i < count; ++i) {
        expr_free(terms[i].base);
        num_destroy(&terms[i].coeff);
    }
    free(terms);
}

bool expr_simplify_additive_terms_equal(const expr_t *left,
                                        const expr_t *right)
{
    addend_t *left_terms = NULL;
    addend_t *right_terms = NULL;
    size_t left_count = 0u;
    size_t right_count = 0u;
    size_t left_capacity = 0u;
    size_t right_capacity = 0u;
    number_t left_const = num_new();
    number_t right_const = num_new();
    bool equal = false;

    if (!left || !right)
        goto cleanup;

    expr_collect_addends((expr_t *)left, NUM_ONE, &left_const,
                         &left_terms, &left_count, &left_capacity);
    expr_collect_addends((expr_t *)right, NUM_ONE, &right_const,
                         &right_terms, &right_count, &right_capacity);
    expr_sort_addends(left_terms, left_count);
    expr_sort_addends(right_terms, right_count);

    if (!num_eq(left_const, right_const) || left_count != right_count)
        goto cleanup;

    for (size_t i = 0u; i < left_count; ++i) {
        if (!num_eq(left_terms[i].coeff, right_terms[i].coeff) ||
            !expr_simplify_same_factor(left_terms[i].base,
                                       right_terms[i].base)) {
            goto cleanup;
        }
    }

    equal = true;

cleanup:
    num_destroy(&right_const);
    num_destroy(&left_const);
    expr_simplify_additive_terms_clear_local(right_terms, right_count);
    expr_simplify_additive_terms_clear_local(left_terms, left_count);
    return equal;
}

typedef struct {
    expr_t *base;
    number_t exponent;
} expr_simplify_factor_t;

static void expr_simplify_free_factors_local(expr_simplify_factor_t *factors,
                                             size_t count)
{
    for (size_t i = 0u; i < count; ++i) {
        expr_free(factors[i].base);
        num_destroy(&factors[i].exponent);
    }
    free(factors);
}

static bool expr_simplify_append_factor_local(expr_simplify_factor_t **factors,
                                              size_t *count,
                                              size_t *capacity,
                                              const expr_t *base,
                                              number_t exponent)
{
    for (size_t i = 0u; i < *count; ++i) {
        if (expr_simplify_same_factor((*factors)[i].base, base)) {
            number_t sum = num_add((*factors)[i].exponent, exponent);

            num_destroy(&(*factors)[i].exponent);
            (*factors)[i].exponent = sum;
            return true;
        }
    }

    if (*count == *capacity) {
        size_t next_capacity = *capacity ? (*capacity * 2u) : 8u;
        expr_simplify_factor_t *grown =
            realloc(*factors, next_capacity * sizeof(**factors));

        if (!grown)
            return false;
        *factors = grown;
        *capacity = next_capacity;
    }

    (*factors)[*count].base = expr_clone(base);
    if (!(*factors)[*count].base)
        return false;
    (*factors)[*count].exponent = num_clone(exponent);
    ++(*count);
    return true;
}

static bool expr_simplify_split_factor_product_local(
    const expr_t *expr,
    number_t sign,
    number_t *coeff_io,
    expr_simplify_factor_t **factors,
    size_t *count,
    size_t *capacity)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    number_t const_value = num_new();
    number_t exponent = num_new();
    bool ok = false;

    if (!expr)
        return true;

    if (expr_match_mul_expr(expr, &left, &right)) {
        ok = expr_simplify_split_factor_product_local(
                 left, sign, coeff_io, factors, count, capacity) &&
             expr_simplify_split_factor_product_local(
                 right, sign, coeff_io, factors, count, capacity);
        goto cleanup;
    }

    if (expr_is_div(expr) && expr->a && expr->b) {
        number_t neg_sign = num_neg(sign);

        ok = expr_simplify_split_factor_product_local(
                 expr->a, sign, coeff_io, factors, count, capacity) &&
             expr_simplify_split_factor_product_local(
                 expr->b, neg_sign, coeff_io, factors, count, capacity);
        num_destroy(&neg_sign);
        goto cleanup;
    }

    if (expr_match_const_value(expr, &const_value)) {
        number_t updated = num_lt(sign, NUM_ZERO) ? num_div(*coeff_io, const_value)
                                                  : num_mul(*coeff_io, const_value);

        num_destroy(coeff_io);
        *coeff_io = updated;
        ok = true;
        goto cleanup;
    }

    if (expr_is_pow_d_expr(expr) && expr->a) {
        num_destroy(&exponent);
        exponent = num_mul(expr->c, sign);
        ok = expr_simplify_append_factor_local(factors, count, capacity,
                                               expr->a, exponent);
        goto cleanup;
    }

    if (expr && expr->ops && expr->ops->kind == EXPR_KIND_POW &&
        expr->a && expr->b && expr_match_const_value(expr->b, &exponent)) {
        number_t signed_exponent = num_mul(exponent, sign);

        ok = expr_simplify_append_factor_local(factors, count, capacity,
                                               expr->a, signed_exponent);
        num_destroy(&signed_exponent);
        goto cleanup;
    }

    ok = expr_simplify_append_factor_local(factors, count, capacity,
                                           expr, sign);

cleanup:
    num_destroy(&exponent);
    num_destroy(&const_value);
    return ok;
}

static expr_t *expr_simplify_rebuild_factor_product_local(
    number_t coeff,
    expr_simplify_factor_t *factors,
    size_t count)
{
    expr_t *out = expr_new_const(coeff);

    if (!out)
        return NULL;

    for (size_t i = 0u; i < count; ++i) {
        expr_t *factor = NULL;
        expr_t *next = NULL;

        if (num_eq(factors[i].exponent, NUM_ZERO))
            continue;
        if (num_eq(factors[i].exponent, NUM_ONE))
            factor = expr_clone(factors[i].base);
        else
            factor = expr_pow(factors[i].base, &factors[i].exponent);

        if (!factor) {
            expr_free(out);
            return NULL;
        }
        next = expr_mul(out, factor);
        expr_free(out);
        expr_free(factor);
        if (!next)
            return NULL;
        out = next;
    }

    return expr_simplify_owned(out);
}

expr_t *expr_simplify_extract_exact_factor_quotient(const expr_t *expr,
                                                    const expr_t *factor)
{
    expr_simplify_factor_t *expr_factors = NULL;
    expr_simplify_factor_t *factor_factors = NULL;
    size_t expr_count = 0u;
    size_t factor_count = 0u;
    size_t expr_capacity = 0u;
    size_t factor_capacity = 0u;
    number_t expr_coeff = num_new();
    number_t factor_coeff = num_new();
    expr_t *out = NULL;

    if (!expr || !factor)
        goto cleanup;
    if (!expr_simplify_split_factor_product_local(
            expr, NUM_ONE, &expr_coeff, &expr_factors,
            &expr_count, &expr_capacity) ||
        !expr_simplify_split_factor_product_local(
            factor, NUM_ONE, &factor_coeff, &factor_factors,
            &factor_count, &factor_capacity) ||
        num_eq(factor_coeff, NUM_ZERO)) {
        goto cleanup;
    }

    for (size_t i = 0u; i < factor_count; ++i) {
        bool matched = false;

        for (size_t j = 0u; j < expr_count; ++j) {
            if (expr_simplify_same_factor(expr_factors[j].base,
                                          factor_factors[i].base)) {
                number_t updated = num_sub(expr_factors[j].exponent,
                                           factor_factors[i].exponent);

                num_destroy(&expr_factors[j].exponent);
                expr_factors[j].exponent = updated;
                matched = true;
                break;
            }
        }

        if (!matched)
            goto cleanup;
    }

    {
        number_t quotient_coeff = num_div(expr_coeff, factor_coeff);

        out = expr_simplify_rebuild_factor_product_local(quotient_coeff,
                                                         expr_factors,
                                                         expr_count);
        num_destroy(&quotient_coeff);
    }

cleanup:
    num_destroy(&factor_coeff);
    num_destroy(&expr_coeff);
    expr_simplify_free_factors_local(factor_factors, factor_count);
    expr_simplify_free_factors_local(expr_factors, expr_count);
    return out;
}

expr_t *expr_simplify_extract_common_factor_quotient(const expr_t *expr,
                                                     const expr_t *factor)
{
    const expr_t *left_node = NULL;
    const expr_t *right_node = NULL;
    expr_t *left = NULL;
    expr_t *right = NULL;
    expr_t *out = NULL;

    if (!expr || !factor)
        return NULL;

    if (expr_simplify_same_factor(expr, factor))
        return expr_new_const(NUM_ONE);

    if (expr_match_mul_expr(expr, &left_node, &right_node)) {
        if (expr_simplify_same_factor(left_node, factor))
            return expr_retain_expr(right_node);
        if (expr_simplify_same_factor(right_node, factor))
            return expr_retain_expr(left_node);

        left = expr_simplify_extract_common_factor_quotient(left_node, factor);
        if (left) {
            out = expr_mul(left, right_node);
            expr_free(left);
            return expr_simplify_owned(out);
        }

        right = expr_simplify_extract_common_factor_quotient(right_node,
                                                            factor);
        if (right) {
            out = expr_mul(left_node, right);
            expr_free(right);
            return expr_simplify_owned(out);
        }

        return NULL;
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_ADD && expr->a && expr->b) {
        left = expr_simplify_extract_common_factor_quotient(expr->a, factor);
        right = expr_simplify_extract_common_factor_quotient(expr->b, factor);
        out = (left && right) ? expr_add(left, right) : NULL;
    } else if (expr->ops && expr->ops->kind == EXPR_KIND_SUB &&
               expr->a && expr->b) {
        left = expr_simplify_extract_common_factor_quotient(expr->a, factor);
        right = expr_simplify_extract_common_factor_quotient(expr->b, factor);
        out = (left && right) ? expr_sub(left, right) : NULL;
    } else if (expr->ops && expr->ops->kind == EXPR_KIND_NEG && expr->a) {
        left = expr_simplify_extract_common_factor_quotient(expr->a, factor);
        out = expr_negate_owned(left);
        left = NULL;
    } else {
        return expr_simplify_extract_exact_factor_quotient(expr, factor);
    }

    expr_free(right);
    expr_free(left);
    return expr_simplify_owned(out);
}

expr_t *expr_simplify_normalize_negated_mul_factor(const expr_t *expr)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    expr_t *out = NULL;

    if (!expr)
        return NULL;

    if (expr->ops && expr->ops->kind == EXPR_KIND_NEG && expr->a &&
        expr_match_mul_expr(expr->a, &left, &right)) {
        expr_t *neg_left = expr_neg(left);

        out = neg_left ? expr_mul(neg_left, right) : NULL;
        expr_free(neg_left);
        return out;
    }

    if (!expr_match_mul_expr(expr, &left, &right))
        return NULL;

    if (left && left->ops && left->ops->kind == EXPR_KIND_NEG && left->a) {
        expr_t *neg_right = expr_neg(right);

        out = neg_right ? expr_mul(left->a, neg_right) : NULL;
        expr_free(neg_right);
        return out;
    }

    if (right && right->ops && right->ops->kind == EXPR_KIND_NEG && right->a) {
        expr_t *neg_left = expr_neg(left);

        out = neg_left ? expr_mul(neg_left, right->a) : NULL;
        expr_free(neg_left);
        return out;
    }

    return out;
}
