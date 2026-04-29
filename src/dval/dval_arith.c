#include <stddef.h>
#include "dval_internal.h"

/* ------------------------------------------------------------------------- */
/* EVALUATION FUNCTIONS                                                      */
/* ------------------------------------------------------------------------- */

static qcomplex_t eval_const(dval_t *dv)
{
    return dv->c;
}

static qcomplex_t eval_var(dval_t *dv)
{
    return dv->c;
}

static qcomplex_t eval_add(dval_t *dv)
{
    return qc_add(dv_eval_qc_internal(dv->a), dv_eval_qc_internal(dv->b));
}

static qcomplex_t eval_sub(dval_t *dv)
{
    return qc_sub(dv_eval_qc_internal(dv->a), dv_eval_qc_internal(dv->b));
}

static qcomplex_t eval_mul(dval_t *dv)
{
    return qc_mul(dv_eval_qc_internal(dv->a), dv_eval_qc_internal(dv->b));
}

static qcomplex_t eval_div(dval_t *dv)
{
    return qc_div(dv_eval_qc_internal(dv->a), dv_eval_qc_internal(dv->b));
}

static qcomplex_t eval_neg(dval_t *dv)
{
    return qc_neg(dv_eval_qc_internal(dv->a));
}

static qcomplex_t eval_pow(dval_t *dv)
{
    qcomplex_t base = dv_eval_qc_internal(dv->a);
    qcomplex_t exp  = dv_eval_qc_internal(dv->b);
    return qc_pow(base, exp);
}

static qcomplex_t eval_pow_d(dval_t *dv)
{
    qcomplex_t base = dv_eval_qc_internal(dv->a);
    return qc_pow(base, dv->c);
}

/* ------------------------------------------------------------------------- */
/* DERIVATIVE FUNCTIONS — lazy, stored in each node                          */
/* ------------------------------------------------------------------------- */

static dval_t *deriv_const(dval_t *dv)
{
    (void)dv;
    return dv_new_const_d(0.0);
}

static dval_t *deriv_var(dval_t *dv)
{
    dval_t *wrt = dv_current_wrt_internal();
    return dv_new_const_d(wrt == NULL || dv == wrt ? 1.0 : 0.0);
}

static dval_t *deriv_add(dval_t *dv)
{
    dval_t *da  = dv_get_dx_internal(dv->a);
    dval_t *db  = dv_get_dx_internal(dv->b);
    dval_t *out = dv_add(da, db);
    dv_free(da);
    dv_free(db);
    return out;
}

static dval_t *deriv_sub(dval_t *dv)
{
    dval_t *da  = dv_get_dx_internal(dv->a);
    dval_t *db  = dv_get_dx_internal(dv->b);
    dval_t *out = dv_sub(da, db);
    dv_free(da);
    dv_free(db);
    return out;
}

static dval_t *deriv_mul(dval_t *dv)
{
    dval_t *da  = dv_get_dx_internal(dv->a);
    dval_t *db  = dv_get_dx_internal(dv->b);
    dval_t *t1  = dv_mul(da, dv->b);
    dval_t *t2  = dv_mul(dv->a, db);
    dval_t *out = dv_add(t1, t2);
    dv_free(da);
    dv_free(db);
    dv_free(t1);
    dv_free(t2);
    return out;
}

static dval_t *deriv_div(dval_t *dv)
{
    dval_t *da   = dv_get_dx_internal(dv->a);
    dval_t *db   = dv_get_dx_internal(dv->b);
    dval_t *num1 = dv_mul(da, dv->b);
    dval_t *num2 = dv_mul(dv->a, db);
    dval_t *num  = dv_sub(num1, num2);
    dval_t *den  = dv_pow_d(dv->b, 2.0);
    dval_t *out  = dv_div(num, den);
    dv_free(da);
    dv_free(db);
    dv_free(num1);
    dv_free(num2);
    dv_free(num);
    dv_free(den);
    return out;
}

static dval_t *deriv_neg(dval_t *dv)
{
    dval_t *da  = dv_get_dx_internal(dv->a);
    dval_t *out = dv_neg(da);
    dv_free(da);
    return out;
}

static dval_t *deriv_pow(dval_t *dv)
{
    dval_t *a  = dv->a;
    dval_t *b  = dv->b;
    dval_t *da = dv_get_dx_internal(a);
    dval_t *db = dv_get_dx_internal(b);

    dval_t *loga    = dv_log(a);
    dval_t *da_on_a = dv_div(da, a);
    dval_t *term1   = dv_mul(db, loga);
    dval_t *term2   = dv_mul(b, da_on_a);
    dval_t *sum     = dv_add(term1, term2);
    dval_t *powab   = dv_pow(a, b);
    dval_t *out     = dv_mul(powab, sum);

    dv_free(da);
    dv_free(db);
    dv_free(loga);
    dv_free(da_on_a);
    dv_free(term1);
    dv_free(term2);
    dv_free(sum);
    dv_free(powab);

    return out;
}

static dval_t *deriv_pow_d(dval_t *dv)
{
    double  c    = qf_to_double(dv->c.re);
    dval_t *da   = dv_get_dx_internal(dv->a);
    dval_t *p    = dv_pow_d(dv->a, c - 1.0);
    dval_t *coef = dv_new_const_d(c);
    dval_t *cp   = dv_mul(coef, p);
    dval_t *out  = dv_mul(cp, da);
    dv_free(da);
    dv_free(p);
    dv_free(coef);
    dv_free(cp);
    return out;
}

/* ------------------------------------------------------------------------- */
/* Operator vtable instances                                                 */
/* ------------------------------------------------------------------------- */

const dval_ops_t ops_const = {
    .eval = eval_const,
    .deriv = deriv_const,
    .reverse = dv_reverse_atom,
    .kind = DV_KIND_CONST,
    .arity = DV_OP_ATOM,
    .name = "const",
    .apply_unary = NULL,
    .apply_binary = NULL,
    .simplify = dv_simplify_passthrough,
    .fold_const_unary = NULL
};

const dval_ops_t ops_var = {
    .eval = eval_var,
    .deriv = deriv_var,
    .reverse = dv_reverse_atom,
    .kind = DV_KIND_VAR,
    .arity = DV_OP_ATOM,
    .name = "var",
    .apply_unary = NULL,
    .apply_binary = NULL,
    .simplify = dv_simplify_passthrough,
    .fold_const_unary = NULL
};

const dval_ops_t ops_add = {
    .eval = eval_add,
    .deriv = deriv_add,
    .reverse = dv_reverse_add,
    .kind = DV_KIND_ADD,
    .arity = DV_OP_BINARY,
    .name = "+",
    .apply_unary = NULL,
    .apply_binary = dv_add,
    .simplify = dv_simplify_add_sub_operator,
    .fold_const_unary = NULL
};

const dval_ops_t ops_sub = {
    .eval = eval_sub,
    .deriv = deriv_sub,
    .reverse = dv_reverse_sub,
    .kind = DV_KIND_SUB,
    .arity = DV_OP_BINARY,
    .name = "-",
    .apply_unary = NULL,
    .apply_binary = dv_sub,
    .simplify = dv_simplify_add_sub_operator,
    .fold_const_unary = NULL
};

const dval_ops_t ops_mul = {
    .eval = eval_mul,
    .deriv = deriv_mul,
    .reverse = dv_reverse_mul,
    .kind = DV_KIND_MUL,
    .arity = DV_OP_BINARY,
    .name = "*",
    .apply_unary = NULL,
    .apply_binary = dv_mul,
    .simplify = dv_simplify_mul_operator,
    .fold_const_unary = NULL
};

const dval_ops_t ops_div = {
    .eval = eval_div,
    .deriv = deriv_div,
    .reverse = dv_reverse_div,
    .kind = DV_KIND_DIV,
    .arity = DV_OP_BINARY,
    .name = "/",
    .apply_unary = NULL,
    .apply_binary = dv_div,
    .simplify = dv_simplify_div_operator,
    .fold_const_unary = NULL
};

const dval_ops_t ops_pow = {
    .eval = eval_pow,
    .deriv = deriv_pow,
    .reverse = dv_reverse_pow,
    .kind = DV_KIND_POW,
    .arity = DV_OP_BINARY,
    .name = "^",
    .apply_unary = NULL,
    .apply_binary = dv_pow,
    .simplify = dv_simplify_pow_operator,
    .fold_const_unary = NULL
};

const dval_ops_t ops_pow_d = {
    .eval = eval_pow_d,
    .deriv = deriv_pow_d,
    .reverse = dv_reverse_pow_d,
    .kind = DV_KIND_POW_D,
    .arity = DV_OP_BINARY,
    .name = "^",
    .apply_unary = NULL,
    .apply_binary = NULL,
    .simplify = dv_simplify_pow_d_operator,
    .fold_const_unary = NULL
};

const dval_ops_t ops_neg = {
    .eval = eval_neg,
    .deriv = deriv_neg,
    .reverse = dv_reverse_neg,
    .kind = DV_KIND_NEG,
    .arity = DV_OP_UNARY,
    .name = "-",
    .apply_unary = dv_neg,
    .apply_binary = NULL,
    .simplify = dv_simplify_neg_operator,
    .fold_const_unary = NULL
};

/* ------------------------------------------------------------------------- */
/* Arithmetic constructors (retain children)                                 */
/* ------------------------------------------------------------------------- */

dval_t *dv_neg(dval_t *dv)
{
    if (!dv)
        return NULL;
    dv_retain(dv);
    return dv_new_unary_internal(&ops_neg, dv);
}

dval_t *dv_add(dval_t *dv1, dval_t *dv2)
{
    if (!dv1 || !dv2)
        return NULL;
    dv_retain(dv1);
    dv_retain(dv2);
    return dv_new_binary_internal(&ops_add, dv1, dv2);
}

dval_t *dv_sub(dval_t *dv1, dval_t *dv2)
{
    if (!dv1 || !dv2)
        return NULL;
    dv_retain(dv1);
    dv_retain(dv2);
    return dv_new_binary_internal(&ops_sub, dv1, dv2);
}

dval_t *dv_mul(dval_t *dv1, dval_t *dv2)
{
    if (!dv1 || !dv2)
        return NULL;
    dv_retain(dv1);
    dv_retain(dv2);
    return dv_new_binary_internal(&ops_mul, dv1, dv2);
}

dval_t *dv_div(dval_t *dv1, dval_t *dv2)
{
    if (!dv1 || !dv2)
        return NULL;
    dv_retain(dv1);
    dv_retain(dv2);
    return dv_new_binary_internal(&ops_div, dv1, dv2);
}

dval_t *dv_pow(dval_t *a, dval_t *b)
{
    if (!a || !b)
        return NULL;
    dv_retain(a);
    dv_retain(b);
    return dv_new_binary_internal(&ops_pow, a, b);
}

dval_t *dv_pow_d(dval_t *a, double d)
{
    if (!a)
        return NULL;
    dv_retain(a);
    return dv_new_pow_d_internal(a, d);
}

dval_t *dv_pow_qf(dval_t *a, qfloat_t exponent)
{
    if (!a)
        return NULL;
    dv_retain(a);
    return dv_new_pow_qf_internal(a, exponent);
}

dval_t *dv_pow_qc(dval_t *a, qcomplex_t exponent)
{
    if (!a)
        return NULL;
    dv_retain(a);
    return dv_new_pow_qc_internal(a, exponent);
}

dval_t *dv_add_d(dval_t *dv, double d)
{
    dval_t *c = dv_new_const_d(d);
    dval_t *r = dv_add(dv, c);
    dv_free(c);
    return r;
}

dval_t *dv_sub_d(dval_t *dv, double d)
{
    dval_t *c = dv_new_const_d(d);
    dval_t *r = dv_sub(dv, c);
    dv_free(c);
    return r;
}

dval_t *dv_d_sub(double d, dval_t *dv)
{
    dval_t *c = dv_new_const_d(d);
    dval_t *r = dv_sub(c, dv);
    dv_free(c);
    return r;
}

dval_t *dv_mul_d(dval_t *dv, double d)
{
    dval_t *c = dv_new_const_d(d);
    dval_t *r = dv_mul(dv, c);
    dv_free(c);
    return r;
}

dval_t *dv_div_d(dval_t *dv, double d)
{
    dval_t *c = dv_new_const_d(d);
    dval_t *r = dv_div(dv, c);
    dv_free(c);
    return r;
}

dval_t *dv_d_div(double d, dval_t *dv)
{
    dval_t *c = dv_new_const_d(d);
    dval_t *r = dv_div(c, dv);
    dv_free(c);
    return r;
}
