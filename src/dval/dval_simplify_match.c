#include <stdlib.h>

#include "qfloat.h"
#include "dval_internal.h"

static void *dv_match_xrealloc(void *ptr, size_t size)
{
    void *grown = realloc(ptr, size);

    if (grown)
        return grown;

    abort();
}

static void collect_mul_factors_borrowed(const dval_t *dv,
                                         qfloat_t *c_acc,
                                         const dval_t ***terms,
                                         size_t *nterms,
                                         size_t *cap)
{
    if (dv_is_unnamed_const(dv)) {
        *c_acc = qf_mul(*c_acc, dv->c.re);
        return;
    }
    if (dv_is_neg(dv)) {
        *c_acc = qf_neg(*c_acc);
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
    const dval_t **u_terms = NULL;
    const dval_t **v_terms = NULL;
    unsigned char *matched = NULL;
    size_t u_n = 0, v_n = 0, u_cap = 0, v_cap = 0;
    qfloat_t u_coeff = QF_ONE;
    qfloat_t v_coeff = QF_ONE;
    int equal = 1;

    collect_mul_factors_borrowed(u, &u_coeff, &u_terms, &u_n, &u_cap);
    collect_mul_factors_borrowed(v, &v_coeff, &v_terms, &v_n, &v_cap);

    if (!qf_eq(u_coeff, v_coeff) || u_n != v_n) {
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
        return qf_eq(u->c.re, v->c.re);
    if (dv_is_var(u))
        return u == v;
    if (dv_is_mul(u))
        return mul_struct_eq(u, v);
    if (dv_is_neg(u))
        return dv_struct_eq(u->a, v->a);
    if (dv_is_pow_d_expr(u))
        return dv_struct_eq(u->a, v->a) && qf_eq(u->c.re, v->c.re);
    return dv_struct_eq(u->a, v->a) && dv_struct_eq(u->b, v->b);
}
