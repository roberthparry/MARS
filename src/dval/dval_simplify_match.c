#include <stdlib.h>
#include <string.h>

#include "dval_internal.h"

static int binding_expr_struct_eq(const dv_binding_expr_t *u,
                                  const dv_binding_expr_t *v)
{
    if (u == v)
        return 1;
    if (!u || !v || u->kind != v->kind)
        return 0;

    switch (u->kind) {
        case DV_BINDING_EXPR_NUMBER:
            if (!u->u.text || !v->u.text)
                return u->u.text == v->u.text;
            return strcmp(u->u.text, v->u.text) == 0;
        case DV_BINDING_EXPR_CONST:
            return u->u.const_id == v->u.const_id;
        case DV_BINDING_EXPR_NEG:
            return binding_expr_struct_eq(u->u.unary.child,
                                          v->u.unary.child);
        case DV_BINDING_EXPR_ADD:
        case DV_BINDING_EXPR_SUB:
        case DV_BINDING_EXPR_MUL:
        case DV_BINDING_EXPR_DIV:
            return binding_expr_struct_eq(u->u.binary.left,
                                          v->u.binary.left) &&
                   binding_expr_struct_eq(u->u.binary.right,
                                          v->u.binary.right);
        case DV_BINDING_EXPR_POWI:
            return u->u.powi.exponent == v->u.powi.exponent &&
                   binding_expr_struct_eq(u->u.powi.base,
                                          v->u.powi.base);
        case DV_BINDING_EXPR_UNARY_OP:
            return u->u.unary_op.ops == v->u.unary_op.ops &&
                   binding_expr_struct_eq(u->u.unary_op.child,
                                          v->u.unary_op.child);
        case DV_BINDING_EXPR_BINARY_OP:
            return u->u.binary_op.ops == v->u.binary_op.ops &&
                   binding_expr_struct_eq(u->u.binary_op.left,
                                          v->u.binary_op.left) &&
                   binding_expr_struct_eq(u->u.binary_op.right,
                                          v->u.binary_op.right);
    }

    return 0;
}

static int const_struct_eq(const dval_t *u, const dval_t *v)
{
    if (u->binding_expr || v->binding_expr)
        return binding_expr_struct_eq(u->binding_expr, v->binding_expr);

    return num_eq(u->c, v->c);
}

static void *dv_match_xrealloc(void *ptr, size_t size)
{
    void *grown = realloc(ptr, size);

    if (grown)
        return grown;

    abort();
}

static void collect_mul_factors_borrowed(const dval_t *dv,
                                         number_t *c_acc,
                                         const dval_t ***terms,
                                         size_t *nterms,
                                         size_t *cap)
{
    NUM_SCOPE(scope);
    if (dv_is_unnamed_const(dv) && num_is_real(dv->c) &&
        (!dv->binding_expr || dv->binding_expr->kind == DV_BINDING_EXPR_NUMBER)) {
        number_t product = num_mul(*c_acc, dv->c);

        num_destroy(c_acc);
        *c_acc = num_scope_detach(product);
        return;
    }
    if (dv_is_neg(dv)) {
        number_t negated = num_neg(*c_acc);

        num_destroy(c_acc);
        *c_acc = num_scope_detach(negated);
        collect_mul_factors_borrowed(dv->a, c_acc, terms, nterms, cap);
        return;
    }
    if (dv_is_mul(dv)) {
        collect_mul_factors_borrowed(dv->a, c_acc, terms, nterms, cap);
        collect_mul_factors_borrowed(dv->b, c_acc, terms, nterms, cap);
        return;
    }
    if (*nterms == *cap) {
        *cap = (*cap == 0) ? 4 : (*cap * 2);
        *terms = dv_match_xrealloc((void *)*terms, *cap * sizeof(**terms));
    }
    (*terms)[(*nterms)++] = dv;
}

static int mul_struct_eq(const dval_t *u, const dval_t *v)
{
    NUM_SCOPE(scope);
    const dval_t **u_terms = NULL;
    const dval_t **v_terms = NULL;
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
            if (!dv_struct_eq(u_terms[i], v_terms[j]))
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

int dv_struct_eq(const dval_t *u, const dval_t *v)
{
    if (u == v)
        return 1;
    if (u->ops != v->ops)
        return 0;
    if (dv_is_const(u))
        return const_struct_eq(u, v);
    if (dv_is_var(u))
        return u == v;
    if (dv_is_mul(u))
        return mul_struct_eq(u, v);
    if (dv_is_neg(u))
        return dv_struct_eq(u->a, v->a);
    if (dv_is_pow_d_expr(u))
        return dv_struct_eq(u->a, v->a) && num_eq(u->c, v->c);
    return dv_struct_eq(u->a, v->a) && dv_struct_eq(u->b, v->b);
}
