#include "expr_internal.h"

static number_t expr_reverse_zero(void)
{
    return NUM_ZERO;
}

static inline number_t num_owned_clone_local(const number_t value)
{
    return num_clone(value);
}

static inline number_t num_owned_neg_local(const number_t value)
{
    return num_neg(value);
}

static inline number_t num_owned_mul_local(const number_t a, const number_t b)
{
    return num_mul(a, b);
}

static inline number_t num_owned_div_local(const number_t a, const number_t b)
{
    return num_div(a, b);
}

void expr_reverse_atom(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    (void)dv;
    (void)out_bar;
    *a_bar = expr_reverse_zero();
    *b_bar = expr_reverse_zero();
}

void expr_reverse_not_differentiable(const expr_t *dv,
                                   const number_t *out_bar,
                                   number_t *a_bar,
                                   number_t *b_bar)
{
    (void)out_bar;
    *a_bar = dv && dv->ops->arity != EXPR_OP_ATOM ? NUM_NAN : expr_reverse_zero();
    *b_bar = dv && dv->ops->arity == EXPR_OP_BINARY ? NUM_NAN : expr_reverse_zero();
}

void expr_reverse_add(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    (void)dv;
    *a_bar = num_owned_clone_local(*out_bar);
    *b_bar = num_owned_clone_local(*out_bar);
}

void expr_reverse_sub(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    (void)dv;
    *a_bar = num_owned_clone_local(*out_bar);
    *b_bar = num_owned_neg_local(*out_bar);
}

void expr_reverse_mul(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    *a_bar = num_owned_mul_local(*out_bar, expr_eval_num_internal(dv->b));
    *b_bar = num_owned_mul_local(*out_bar, expr_eval_num_internal(dv->a));
}

void expr_reverse_div(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t denom_sq = num_mul(expr_eval_num_internal(dv->b), expr_eval_num_internal(dv->b));
    number_t numer = num_mul(*out_bar, expr_eval_num_internal(dv->a));
    number_t frac;

    *a_bar = num_owned_div_local(*out_bar, expr_eval_num_internal(dv->b));
    frac = num_div(numer, denom_sq);
    *b_bar = num_owned_neg_local(frac);
}

void expr_reverse_pow(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t z = expr_eval_num_internal(dv);
    number_t z_times_b = num_mul(z, expr_eval_num_internal(dv->b));
    number_t numer = num_mul(*out_bar, z_times_b);
    number_t log_a = num_log(expr_eval_num_internal(dv->a));
    number_t z_log_a;

    *a_bar = num_owned_div_local(numer, expr_eval_num_internal(dv->a));

    z_log_a = num_mul(z, log_a);
    *b_bar = num_owned_mul_local(*out_bar, z_log_a);
}

void expr_reverse_pow_d(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t one_less = num_sub(dv->c, NUM_ONE);
    number_t power = num_pow(expr_eval_num_internal(dv->a), one_less);
    number_t scale = num_mul(dv->c, power);

    *a_bar = num_owned_mul_local(*out_bar, scale);
    *b_bar = expr_reverse_zero();
}

void expr_reverse_neg(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    (void)dv;
    *a_bar = num_owned_neg_local(*out_bar);
    *b_bar = expr_reverse_zero();
}
