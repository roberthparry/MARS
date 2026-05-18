#include <stddef.h>
#include "dval_internal.h"
#include "internal/number_internal.h"

/* ------------------------------------------------------------------------- */
/* EVALUATION FUNCTIONS                                                      */
/* ------------------------------------------------------------------------- */

static number_t eval_const(dval_t *dv)
{
    return num_clone(dv->c);
}

static number_t eval_var(dval_t *dv)
{
    return num_clone(dv->c);
}

static number_t eval_add(dval_t *dv)
{
    return num_add(dv_eval_num_internal(dv->a), dv_eval_num_internal(dv->b));
}

static number_t eval_sub(dval_t *dv)
{
    return num_sub(dv_eval_num_internal(dv->a), dv_eval_num_internal(dv->b));
}

static number_t eval_mul(dval_t *dv)
{
    return num_mul(dv_eval_num_internal(dv->a), dv_eval_num_internal(dv->b));
}

static number_t eval_div(dval_t *dv)
{
    return num_div(dv_eval_num_internal(dv->a), dv_eval_num_internal(dv->b));
}

static number_t eval_neg(dval_t *dv)
{
    return num_neg(dv_eval_num_internal(dv->a));
}

static number_t eval_pow(dval_t *dv)
{
    return num_pow(dv_eval_num_internal(dv->a), dv_eval_num_internal(dv->b));
}

static number_t eval_pow_d(dval_t *dv)
{
    return num_pow(dv_eval_num_internal(dv->a), dv->c);
}

/* ------------------------------------------------------------------------- */
/* DERIVATIVE FUNCTIONS — lazy, stored in each node                          */
/* ------------------------------------------------------------------------- */

static dval_t *deriv_const(dval_t *dv)
{
    (void)dv;
    return dv_new_const(NUM_ZERO);
}

static dval_t *deriv_var(dval_t *dv)
{
    const dval_t *wrt = dv_current_wrt_internal();
    return dv_new_const((wrt == NULL || dv == wrt) ? NUM_ONE : NUM_ZERO);
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
    NUM_SCOPE(scope);
    dval_t *da   = dv_get_dx_internal(dv->a);
    dval_t *db   = dv_get_dx_internal(dv->b);
    dval_t *num1 = dv_mul(da, dv->b);
    dval_t *num2 = dv_mul(dv->a, db);
    dval_t *num  = dv_sub(num1, num2);
    number_t two = num_create_from_long(2);
    dval_t *den  = dv_pow(dv->b, &two);
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
    dval_t *powab   = dv_pow_dv(a, b);
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
    NUM_SCOPE(scope);
    number_t exponent = num_clone(dv->c);
    number_t exponent_minus_one = num_sub(exponent, NUM_ONE);
    dval_t *da   = dv_get_dx_internal(dv->a);
    dval_t *p    = dv_pow(dv->a, &exponent_minus_one);
    dval_t *coef = dv_new_const(exponent);
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
    .tex_name = NULL,
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
    .tex_name = NULL,
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
    .tex_name = "+",
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
    .tex_name = "-",
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
    .tex_name = "\\cdot",
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
    .tex_name = "/",
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
    .tex_name = "^",
    .apply_unary = NULL,
    .apply_binary = dv_pow_dv,
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
    .tex_name = "^",
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
    .tex_name = "-",
    .apply_unary = dv_neg,
    .apply_binary = NULL,
    .simplify = dv_simplify_neg_operator,
    .fold_const_unary = NULL
};

/* ------------------------------------------------------------------------- */
/* Arithmetic constructors (retain children)                                 */
/* ------------------------------------------------------------------------- */

dval_t *dv_neg(const dval_t *dv)
{
    if (!dv)
        return NULL;
    dv_retain(dv);
    return dv_new_unary_internal(&ops_neg, dv);
}

dval_t *dv_add(const dval_t *dv1, const dval_t *dv2)
{
    if (!dv1 || !dv2)
        return NULL;
    dv_retain(dv1);
    dv_retain(dv2);
    return dv_new_binary_internal(&ops_add, dv1, dv2);
}

dval_t *dv_sub(const dval_t *dv1, const dval_t *dv2)
{
    if (!dv1 || !dv2)
        return NULL;
    dv_retain(dv1);
    dv_retain(dv2);
    return dv_new_binary_internal(&ops_sub, dv1, dv2);
}

dval_t *dv_mul(const dval_t *dv1, const dval_t *dv2)
{
    if (!dv1 || !dv2)
        return NULL;
    dv_retain(dv1);
    dv_retain(dv2);
    return dv_new_binary_internal(&ops_mul, dv1, dv2);
}

dval_t *dv_div(const dval_t *dv1, const dval_t *dv2)
{
    if (!dv1 || !dv2)
        return NULL;
    dv_retain(dv1);
    dv_retain(dv2);
    return dv_new_binary_internal(&ops_div, dv1, dv2);
}

dval_t *dv_pow_dv(const dval_t *a, const dval_t *b)
{
    if (!a || !b)
        return NULL;
    dv_retain(a);
    dv_retain(b);
    return dv_new_binary_internal(&ops_pow, a, b);
}

dval_t *dv_pow(const dval_t *dv, const number_t *exponent)
{
    if (!dv || !exponent)
        return NULL;
    dv_retain(dv);
    return dv_new_pow_const_internal(dv, *exponent);
}

dval_t *dv_add_num(const dval_t *dv, const number_t *value)
{
    dval_t *c;
    dval_t *r;

    if (!value)
        return NULL;
    c = dv_new_const(*value);
    if (!c)
        return NULL;
    r = dv_add(dv, c);
    dv_free(c);
    return r;
}

dval_t *dv_sub_num(const dval_t *dv, const number_t *value)
{
    dval_t *c;
    dval_t *r;

    if (!value)
        return NULL;
    c = dv_new_const(*value);
    if (!c)
        return NULL;
    r = dv_sub(dv, c);
    dv_free(c);
    return r;
}

dval_t *dv_num_sub(const number_t *value, const dval_t *dv)
{
    dval_t *c;
    dval_t *r;

    if (!value)
        return NULL;
    c = dv_new_const(*value);
    if (!c)
        return NULL;
    r = dv_sub(c, dv);
    dv_free(c);
    return r;
}

dval_t *dv_mul_num(const dval_t *dv, const number_t *value)
{
    dval_t *c;
    dval_t *r;

    if (!value)
        return NULL;
    c = dv_new_const(*value);
    if (!c)
        return NULL;
    r = dv_mul(dv, c);
    dv_free(c);
    return r;
}

dval_t *dv_div_num(const dval_t *dv, const number_t *value)
{
    dval_t *c;
    dval_t *r;

    if (!value)
        return NULL;
    c = dv_new_const(*value);
    if (!c)
        return NULL;
    r = dv_div(dv, c);
    dv_free(c);
    return r;
}

dval_t *dv_num_div(const number_t *value, const dval_t *dv)
{
    dval_t *c;
    dval_t *r;

    if (!value)
        return NULL;
    c = dv_new_const(*value);
    if (!c)
        return NULL;
    r = dv_div(c, dv);
    dv_free(c);
    return r;
}
