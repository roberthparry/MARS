#include <stddef.h>
#include <stdlib.h>

#include "qfloat.h"
#include "dval_internal.h"

/* ------------------------------------------------------------------------- */
/* Helpers                                                                   */
/* ------------------------------------------------------------------------- */

static int is_qf_zero(qfloat x)
{
    return qf_to_double(x) == 0.0;
}

static int is_qf_one(qfloat x)
{
    return qf_to_double(x) == 1.0;
}

/* ------------------------------------------------------------------------- */
/* Multiplication flattening                                                 */
/* ------------------------------------------------------------------------- */

static void collect_mul_flat(
    dval_t *n,
    qfloat *c_acc,
    int *is_zero,
    dval_t ***terms,
    size_t *nterms,
    size_t *cap)
{
    if (*is_zero || !n)
        return;

    if (n->ops == &ops_const) {
        if (is_qf_zero(n->c)) {
            *is_zero = 1;
        } else {
            *c_acc = qf_mul(*c_acc, n->c);
        }
        return;
    }

    if (n->ops == &ops_mul) {
        collect_mul_flat(n->a, c_acc, is_zero, terms, nterms, cap);
        collect_mul_flat(n->b, c_acc, is_zero, terms, nterms, cap);
        return;
    }

    if (*nterms == *cap) {
        size_t new_cap = (*cap == 0) ? 4 : (*cap * 2);
        dval_t **new_terms = realloc(*terms, new_cap * sizeof(dval_t *));
        if (!new_terms) {
            *is_zero = 1;
            return;
        }
        *terms = new_terms;
        *cap = new_cap;
    }

    dv_retain(n);
    (*terms)[(*nterms)++] = n;
}

/* ------------------------------------------------------------------------- */
/* Unary function simplification helpers                                     */
/* ------------------------------------------------------------------------- */

static dval_t *simplify_unary_fun(dval_t *f, dval_t *a)
{
    /* f is borrowed, a is owning */

    /* If argument is constant, we can sometimes fold or at least keep as fun(const). */
    if (a->ops == &ops_const) {
        double x = qf_to_double(a->c);

        if (f->ops == &ops_sin) {
            if (x == 0.0) {
                dv_free(a);
                return dv_new_const_d(0.0);
            }
        } else if (f->ops == &ops_cos) {
            if (x == 0.0) {
                dv_free(a);
                return dv_new_const_d(1.0);
            }
        } else if (f->ops == &ops_tan) {
            if (x == 0.0) {
                dv_free(a);
                return dv_new_const_d(0.0);
            }
        } else if (f->ops == &ops_exp) {
            if (x == 0.0) {
                dv_free(a);
                return dv_new_const_d(1.0);
            }
        } else if (f->ops == &ops_log) {
            if (x == 1.0) {
                dv_free(a);
                return dv_new_const_d(0.0);
            }
        } else if (f->ops == &ops_sqrt) {
            if (x == 0.0) {
                dv_free(a);
                return dv_new_const_d(0.0);
            }
            if (x == 1.0) {
                dv_free(a);
                return dv_new_const_d(1.0);
            }
        }
        /* fall through: keep as fun(const) */
    }

    /* rebuild generic unary op */
    dval_t *out = NULL;

    if      (f->ops == &ops_sin)   out = dv_sin(a);
    else if (f->ops == &ops_cos)   out = dv_cos(a);
    else if (f->ops == &ops_tan)   out = dv_tan(a);
    else if (f->ops == &ops_sinh)  out = dv_sinh(a);
    else if (f->ops == &ops_cosh)  out = dv_cosh(a);
    else if (f->ops == &ops_tanh)  out = dv_tanh(a);
    else if (f->ops == &ops_asin)  out = dv_asin(a);
    else if (f->ops == &ops_acos)  out = dv_acos(a);
    else if (f->ops == &ops_atan)  out = dv_atan(a);
    else if (f->ops == &ops_asinh) out = dv_asinh(a);
    else if (f->ops == &ops_acosh) out = dv_acosh(a);
    else if (f->ops == &ops_atanh) out = dv_atanh(a);
    else if (f->ops == &ops_exp)   out = dv_exp(a);
    else if (f->ops == &ops_log)   out = dv_log(a);
    else if (f->ops == &ops_sqrt)  out = dv_sqrt(a);
    else {
        /* unknown unary op: just retain original f */
        dv_free(a);
        dv_retain(f);
        return f;
    }

    dv_free(a);
    return out;
}

/* ------------------------------------------------------------------------- */
/* dv_simplify                                                               */
/* ------------------------------------------------------------------------- */

/*
 * dv_simplify:
 *   - f is borrowed
 *   - returns an owning node (refcount = 1)
 */
dval_t *dv_simplify(dval_t *f)
{
    if (!f)
        return NULL;

    /* constants and variables: just retain */
    if (f->ops == &ops_const || f->ops == &ops_var) {
        dv_retain(f);
        return f;
    }

    /* recursively simplify children */
    dval_t *a = f->a ? dv_simplify(f->a) : NULL;
    dval_t *b = f->b ? dv_simplify(f->b) : NULL;

    /* -------------------- ADD -------------------- */
    if (f->ops == &ops_add && a && b) {

        /* 0 + x → x */
        if (a->ops == &ops_const && is_qf_zero(a->c)) {
            dv_free(a);
            return b;
        }

        /* x + 0 → x */
        if (b->ops == &ops_const && is_qf_zero(b->c)) {
            dv_free(b);
            return a;
        }

        /* const + const → const */
        if (a->ops == &ops_const && b->ops == &ops_const) {
            qfloat sum = qf_add(a->c, b->c);
            dv_free(a);
            dv_free(b);
            return dv_new_const(sum);
        }

        /* x + (-y) → x - y */
        if (b->ops == &ops_neg && b->a) {
            dv_retain(a);
            dv_retain(b->a);
            dval_t *out = dv_sub(a, b->a);
            dv_free(a);
            dv_free(b);
            return out;
        }

        /* (-x) + y → y - x */
        if (a->ops == &ops_neg && a->a) {
            dv_retain(b);
            dv_retain(a->a);
            dval_t *out = dv_sub(b, a->a);
            dv_free(a);
            dv_free(b);
            return out;
        }

        dval_t *out = dv_add(a, b);
        dv_free(a);
        dv_free(b);
        return out;
    }

    /* -------------------- SUB -------------------- */
    if (f->ops == &ops_sub && a && b) {

        /* x - 0 → x */
        if (b->ops == &ops_const && is_qf_zero(b->c)) {
            dv_free(b);
            return a;
        }

        /* const - const → const */
        if (a->ops == &ops_const && b->ops == &ops_const) {
            qfloat diff = qf_sub(a->c, b->c);
            dv_free(a);
            dv_free(b);
            return dv_new_const(diff);
        }

        /* x - x → 0 */
        if (a->ops == &ops_var && b->ops == &ops_var && a == b) {
            dv_free(a);
            dv_free(b);
            return dv_new_const_d(0.0);
        }

        dval_t *out = dv_sub(a, b);
        dv_free(a);
        dv_free(b);
        return out;
    }

    /* -------------------- MUL (full flatten) -------------------- */
    if (f->ops == &ops_mul && a && b) {

        /* early 0 * x → 0 */
        if ((a->ops == &ops_const && is_qf_zero(a->c)) ||
            (b->ops == &ops_const && is_qf_zero(b->c))) {
            dv_free(a);
            dv_free(b);
            return dv_new_const_d(0.0);
        }

        qfloat c_acc = qf_from_double(1.0);
        int is_zero = 0;
        dval_t **terms = NULL;
        size_t nterms = 0, cap = 0;

        collect_mul_flat(a, &c_acc, &is_zero, &terms, &nterms, &cap);
        collect_mul_flat(b, &c_acc, &is_zero, &terms, &nterms, &cap);

        dv_free(a);
        dv_free(b);

        if (is_zero) {
            if (terms) {
                for (size_t i = 0; i < nterms; ++i)
                    dv_free(terms[i]);
                free(terms);
            }
            return dv_new_const_d(0.0);
        }

        /* no non-const factors → constant */
        if (nterms == 0) {
            free(terms);
            return dv_new_const(c_acc);
        }

        dval_t *cur = NULL;

        /* start with constant if not 1 */
        if (!is_qf_one(c_acc)) {
            dval_t *cnode = dv_new_const(c_acc);
            dval_t *tmp = dv_mul(cnode, terms[0]);
            dv_free(cnode);
            dv_free(terms[0]);
            cur = tmp;
        } else {
            cur = terms[0]; /* already retained */
        }

        for (size_t i = 1; i < nterms; ++i) {
            dval_t *tmp = dv_mul(cur, terms[i]);
            dv_free(cur);
            dv_free(terms[i]);
            cur = tmp;
        }

        free(terms);
        return cur;
    }

    /* -------------------- DIV -------------------- */
    if (f->ops == &ops_div && a && b) {

        /* x / 1 → x */
        if (b->ops == &ops_const && is_qf_one(b->c)) {
            dv_free(b);
            return a;
        }

        /* 0 / x → 0 (x ≠ 0) */
        if (a->ops == &ops_const && is_qf_zero(a->c)) {
            dv_free(a);
            dv_free(b);
            return dv_new_const_d(0.0);
        }

        /* const / const → const (no domain checks here) */
        if (a->ops == &ops_const && b->ops == &ops_const) {
            qfloat q = qf_div(a->c, b->c);
            dv_free(a);
            dv_free(b);
            return dv_new_const(q);
        }

        dval_t *out = dv_div(a, b);
        dv_free(a);
        dv_free(b);
        return out;
    }

    /* -------------------- NEG -------------------- */
    if (f->ops == &ops_neg && a) {

        /* -(-x) → x */
        if (a->ops == &ops_neg && a->a) {
            dval_t *inner = a->a;
            dv_retain(inner);
            dv_free(a);
            return inner;
        }

        /* -(const) → const */
        if (a->ops == &ops_const) {
            qfloat c = qf_neg(a->c);
            dv_free(a);
            return dv_new_const(c);
        }

        dval_t *out = dv_neg(a);
        dv_free(a);
        return out;
    }

    /* -------------------- POW_D -------------------- */
    if (f->ops == &ops_pow_d && a) {
        double ed = qf_to_double(f->c);

        /* x^1 → x */
        if (ed == 1.0)
            return a;

        /* x^0 → 1 (x ≠ 0) */
        if (ed == 0.0) {
            dv_free(a);
            return dv_new_const_d(1.0);
        }

        /* const^const → const */
        if (a->ops == &ops_const) {
            /* you can choose to call a qfloat pow here if you have one;
               for now, just rebuild structurally */
        }

        dval_t *out = dv_pow_d(a, ed);
        dv_free(a);
        return out;
    }

    /* -------------------- POW (symbolic exponent) -------------------- */
    if (f->ops == &ops_pow && a && b) {

        /* x^1 → x */
        if (b->ops == &ops_const && is_qf_one(b->c)) {
            dv_free(b);
            return a;
        }

        /* x^0 → 1 */
        if (b->ops == &ops_const && is_qf_zero(b->c)) {
            dv_free(a);
            dv_free(b);
            return dv_new_const_d(1.0);
        }

        /* const^const → const (if you later add qf_pow) */

        dval_t *out = dv_pow(a, b);
        dv_free(a);
        dv_free(b);
        return out;
    }

    /* -------------------- UNARY FUNCS -------------------- */
    if (f->ops == &ops_sin  || f->ops == &ops_cos  || f->ops == &ops_tan  ||
        f->ops == &ops_sinh || f->ops == &ops_cosh || f->ops == &ops_tanh ||
        f->ops == &ops_asin || f->ops == &ops_acos || f->ops == &ops_atan ||
        f->ops == &ops_asinh|| f->ops == &ops_acosh|| f->ops == &ops_atanh||
        f->ops == &ops_exp  || f->ops == &ops_log  || f->ops == &ops_sqrt)
    {
        if (!a) {
            dv_retain(f);
            return f;
        }
        return simplify_unary_fun(f, a);
    }

    /* -------------------- FALLBACK -------------------- */

    if (a) dv_free(a);
    if (b) dv_free(b);

    dv_retain(f);
    return f;
}
