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
