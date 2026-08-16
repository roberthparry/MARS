#include <limits.h>
#include <stdlib.h>

#include "expr_maths.h"
#define MARS_NUMBER_INTERNAL_ACCESS
#include "number/number_internal.h"

static inline void expr_reverse_unary(number_t value, number_t *a_bar, number_t *b_bar)
{
    *a_bar = value;
    *b_bar = NUM_ZERO;
}

static inline void expr_reverse_binary(number_t a_value, number_t b_value, number_t *a_bar, number_t *b_bar)
{
    *a_bar = a_value;
    *b_bar = b_value;
}

void expr_reverse_atan2(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t ax2 = expr_reverse_num_sq(expr_eval_num_internal(dv->a));
    number_t bx2 = expr_reverse_num_sq(expr_eval_num_internal(dv->b));
    number_t denom = num_add(ax2, bx2);
    number_t y_over_denom = num_div(expr_eval_num_internal(dv->b), denom);
    number_t x_over_denom = num_div(expr_eval_num_internal(dv->a), denom);
    number_t scaled_x = num_mul(*out_bar, x_over_denom);

    *a_bar = expr_reverse_num_mul(*out_bar, y_over_denom);
    *b_bar = expr_reverse_num_neg(scaled_x);
}

void expr_reverse_sin(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t cos_x = num_cos(expr_eval_num_internal(dv->a));
    number_t factor = num_mul(*out_bar, cos_x);

    expr_reverse_unary(expr_reverse_num_clone(factor), a_bar, b_bar);
}

void expr_reverse_cos(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t sin_x = num_sin(expr_eval_num_internal(dv->a));
    number_t product = num_mul(*out_bar, sin_x);

    expr_reverse_unary(expr_reverse_num_neg(product), a_bar, b_bar);
}

void expr_reverse_tan(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t cos_x = num_cos(expr_eval_num_internal(dv->a));
    number_t cos_sq = expr_reverse_num_sq(cos_x);
    number_t inv = expr_reverse_num_inverse(cos_sq);

    expr_reverse_unary(expr_reverse_num_mul(*out_bar, inv), a_bar, b_bar);
}

void expr_reverse_sec(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t x = expr_eval_num_internal(dv->a);
    number_t factor = num_mul(num_sec(x), num_tan(x));

    expr_reverse_unary(expr_reverse_num_mul(*out_bar, factor), a_bar, b_bar);
}

void expr_reverse_cosec(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t x = expr_eval_num_internal(dv->a);
    number_t factor = num_mul(num_cosec(x), num_cot(x));
    number_t product = num_mul(*out_bar, factor);

    expr_reverse_unary(expr_reverse_num_neg(product), a_bar, b_bar);
}

void expr_reverse_cot(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t cosec_x = num_cosec(expr_eval_num_internal(dv->a));
    number_t factor = expr_reverse_num_sq(cosec_x);
    number_t product = num_mul(*out_bar, factor);

    expr_reverse_unary(expr_reverse_num_neg(product), a_bar, b_bar);
}

static void expr_reverse_unary_factor(const number_t *out_bar, number_t factor, number_t *a_bar, number_t *b_bar)
{
    number_t product = expr_reverse_num_mul(*out_bar, factor);

    num_destroy(&factor);
    expr_reverse_unary(product, a_bar, b_bar);
}

void expr_reverse_versin(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t factor = num_sin(expr_eval_num_internal(dv->a));

    expr_reverse_unary_factor(out_bar, factor, a_bar, b_bar);
}

void expr_reverse_vercos(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t sin_x = num_sin(expr_eval_num_internal(dv->a));
    number_t factor = num_neg(sin_x);

    num_destroy(&sin_x);
    expr_reverse_unary_factor(out_bar, factor, a_bar, b_bar);
}

void expr_reverse_coversin(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t cos_x = num_cos(expr_eval_num_internal(dv->a));
    number_t factor = num_neg(cos_x);

    num_destroy(&cos_x);
    expr_reverse_unary_factor(out_bar, factor, a_bar, b_bar);
}

void expr_reverse_covercos(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t factor = num_cos(expr_eval_num_internal(dv->a));

    expr_reverse_unary_factor(out_bar, factor, a_bar, b_bar);
}

void expr_reverse_haversin(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t sin_x = num_sin(expr_eval_num_internal(dv->a));
    number_t factor = num_div(sin_x, NUM_TWO);

    num_destroy(&sin_x);
    expr_reverse_unary_factor(out_bar, factor, a_bar, b_bar);
}

void expr_reverse_havercos(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t sin_x = num_sin(expr_eval_num_internal(dv->a));
    number_t neg_sin = num_neg(sin_x);
    number_t factor = num_div(neg_sin, NUM_TWO);

    num_destroy(&neg_sin);
    num_destroy(&sin_x);
    expr_reverse_unary_factor(out_bar, factor, a_bar, b_bar);
}

void expr_reverse_hacoversin(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t cos_x = num_cos(expr_eval_num_internal(dv->a));
    number_t neg_cos = num_neg(cos_x);
    number_t factor = num_div(neg_cos, NUM_TWO);

    num_destroy(&neg_cos);
    num_destroy(&cos_x);
    expr_reverse_unary_factor(out_bar, factor, a_bar, b_bar);
}

void expr_reverse_hacovercos(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t cos_x = num_cos(expr_eval_num_internal(dv->a));
    number_t factor = num_div(cos_x, NUM_TWO);

    num_destroy(&cos_x);
    expr_reverse_unary_factor(out_bar, factor, a_bar, b_bar);
}

void expr_reverse_sinh(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t cosh_x = num_cosh(expr_eval_num_internal(dv->a));
    number_t factor = num_mul(*out_bar, cosh_x);

    expr_reverse_unary(expr_reverse_num_clone(factor), a_bar, b_bar);
}

void expr_reverse_cosh(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t sinh_x = num_sinh(expr_eval_num_internal(dv->a));
    number_t factor = num_mul(*out_bar, sinh_x);

    expr_reverse_unary(expr_reverse_num_clone(factor), a_bar, b_bar);
}

void expr_reverse_tanh(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t expr_sq = expr_reverse_num_sq(expr_eval_num_internal(dv));
    number_t factor = num_sub(NUM_ONE, expr_sq);

    expr_reverse_unary(expr_reverse_num_mul(*out_bar, factor), a_bar, b_bar);
}

void expr_reverse_sech(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t x = expr_eval_num_internal(dv->a);
    number_t factor = num_mul(num_sech(x), num_tanh(x));
    number_t product = num_mul(*out_bar, factor);

    expr_reverse_unary(expr_reverse_num_neg(product), a_bar, b_bar);
}

void expr_reverse_cosech(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t x = expr_eval_num_internal(dv->a);
    number_t factor = num_mul(num_cosech(x), num_coth(x));
    number_t product = num_mul(*out_bar, factor);

    expr_reverse_unary(expr_reverse_num_neg(product), a_bar, b_bar);
}

void expr_reverse_coth(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t cosech_x = num_cosech(expr_eval_num_internal(dv->a));
    number_t factor = expr_reverse_num_sq(cosech_x);
    number_t product = num_mul(*out_bar, factor);

    expr_reverse_unary(expr_reverse_num_neg(product), a_bar, b_bar);
}

void expr_reverse_asin(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t x_sq = expr_reverse_num_sq(expr_eval_num_internal(dv->a));
    number_t inner = num_sub(NUM_ONE, x_sq);
    number_t denom = num_sqrt(inner);

    expr_reverse_unary(expr_reverse_num_div(*out_bar, denom), a_bar, b_bar);
}

void expr_reverse_acos(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t x_sq = expr_reverse_num_sq(expr_eval_num_internal(dv->a));
    number_t inner = num_sub(NUM_ONE, x_sq);
    number_t denom = num_sqrt(inner);
    number_t frac = num_div(*out_bar, denom);

    expr_reverse_unary(expr_reverse_num_neg(frac), a_bar, b_bar);
}

void expr_reverse_atan(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t x_sq = expr_reverse_num_sq(expr_eval_num_internal(dv->a));
    number_t denom = num_add(NUM_ONE, x_sq);

    expr_reverse_unary(expr_reverse_num_div(*out_bar, denom), a_bar, b_bar);
}

static number_t expr_reverse_inverse_reciprocal_factor(const expr_t *dv, number_t (*inner_factor)(const expr_t *))
{
    number_t x = expr_eval_num_internal(dv->a);
    number_t x_sq = expr_reverse_num_sq(x);
    number_t factor = inner_factor(dv);

    return num_div(num_neg(factor), x_sq);
}

static number_t expr_reverse_asec_inner(const expr_t *dv)
{
    number_t x = expr_eval_num_internal(dv->a);
    number_t x_sq = expr_reverse_num_sq(x);
    number_t inv_x_sq = expr_reverse_num_inverse(x_sq);
    number_t inner = num_sub(NUM_ONE, inv_x_sq);

    return num_neg(expr_reverse_num_inverse(num_sqrt(inner)));
}

static number_t expr_reverse_acosec_inner(const expr_t *dv)
{
    number_t x = expr_eval_num_internal(dv->a);
    number_t x_sq = expr_reverse_num_sq(x);
    number_t inv_x_sq = expr_reverse_num_inverse(x_sq);
    number_t inner = num_sub(NUM_ONE, inv_x_sq);

    return expr_reverse_num_inverse(num_sqrt(inner));
}

static number_t expr_reverse_acot_inner(const expr_t *dv)
{
    number_t x = expr_eval_num_internal(dv->a);
    number_t x_sq = expr_reverse_num_sq(x);
    number_t inv_x_sq = expr_reverse_num_inverse(x_sq);
    number_t denom = num_add(NUM_ONE, inv_x_sq);

    return expr_reverse_num_inverse(denom);
}

void expr_reverse_asec(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t factor = expr_reverse_inverse_reciprocal_factor(dv, expr_reverse_asec_inner);

    expr_reverse_unary(expr_reverse_num_mul(*out_bar, factor), a_bar, b_bar);
}

void expr_reverse_acosec(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t factor = expr_reverse_inverse_reciprocal_factor(dv, expr_reverse_acosec_inner);

    expr_reverse_unary(expr_reverse_num_mul(*out_bar, factor), a_bar, b_bar);
}

void expr_reverse_acot(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t factor = expr_reverse_inverse_reciprocal_factor(dv, expr_reverse_acot_inner);

    expr_reverse_unary(expr_reverse_num_mul(*out_bar, factor), a_bar, b_bar);
}

static number_t expr_reverse_haversine_inverse_factor(const expr_t *dv, int scale, int offset_sign, int coeff)
{
    number_t x = expr_eval_num_internal(dv->a);
    number_t scaled = scale == 2 ? num_mul(NUM_TWO, x) : num_clone(x);
    number_t inner = offset_sign < 0 ? num_sub(scaled, NUM_ONE) : num_sub(NUM_ONE, scaled);
    number_t inner_sq = expr_reverse_num_sq(inner);
    number_t radicand = num_sub(NUM_ONE, inner_sq);
    number_t denom = num_sqrt(radicand);
    number_t coeff_num = num_create_from_long((long)coeff);
    number_t factor = num_div(coeff_num, denom);

    num_destroy(&coeff_num);
    num_destroy(&denom);
    num_destroy(&radicand);
    num_destroy(&inner_sq);
    num_destroy(&inner);
    num_destroy(&scaled);
    return factor;
}

void expr_reverse_arcversin(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    expr_reverse_unary_factor(out_bar, expr_reverse_haversine_inverse_factor(dv, 1, 1, 1), a_bar, b_bar);
}

void expr_reverse_arcvercos(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    expr_reverse_unary_factor(out_bar, expr_reverse_haversine_inverse_factor(dv, 1, -1, -1), a_bar, b_bar);
}

void expr_reverse_arccoversin(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    expr_reverse_unary_factor(out_bar, expr_reverse_haversine_inverse_factor(dv, 1, 1, -1), a_bar, b_bar);
}

void expr_reverse_arccovercos(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    expr_reverse_unary_factor(out_bar, expr_reverse_haversine_inverse_factor(dv, 1, -1, 1), a_bar, b_bar);
}

void expr_reverse_archaversin(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    expr_reverse_unary_factor(out_bar, expr_reverse_haversine_inverse_factor(dv, 2, 1, 2), a_bar, b_bar);
}

void expr_reverse_archavercos(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    expr_reverse_unary_factor(out_bar, expr_reverse_haversine_inverse_factor(dv, 2, -1, -2), a_bar, b_bar);
}

void expr_reverse_archacoversin(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    expr_reverse_unary_factor(out_bar, expr_reverse_haversine_inverse_factor(dv, 2, 1, -2), a_bar, b_bar);
}

void expr_reverse_archacovercos(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    expr_reverse_unary_factor(out_bar, expr_reverse_haversine_inverse_factor(dv, 2, -1, 2), a_bar, b_bar);
}

void expr_reverse_asinh(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t x_sq = expr_reverse_num_sq(expr_eval_num_internal(dv->a));
    number_t inner = num_add(x_sq, NUM_ONE);
    number_t denom = num_sqrt(inner);

    expr_reverse_unary(expr_reverse_num_div(*out_bar, denom), a_bar, b_bar);
}

void expr_reverse_acosh(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t xm1 = num_sub(expr_eval_num_internal(dv->a), NUM_ONE);
    number_t xp1 = num_add(expr_eval_num_internal(dv->a), NUM_ONE);
    number_t sqrt_xm1 = num_sqrt(xm1);
    number_t sqrt_xp1 = num_sqrt(xp1);
    number_t denom = num_mul(sqrt_xm1, sqrt_xp1);

    expr_reverse_unary(expr_reverse_num_div(*out_bar, denom), a_bar, b_bar);
}

void expr_reverse_atanh(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t x_sq = expr_reverse_num_sq(expr_eval_num_internal(dv->a));
    number_t denom = num_sub(NUM_ONE, x_sq);

    expr_reverse_unary(expr_reverse_num_div(*out_bar, denom), a_bar, b_bar);
}

static number_t expr_reverse_asech_inner(const expr_t *dv)
{
    number_t x = expr_eval_num_internal(dv->a);
    number_t inv_x = expr_reverse_num_inverse(x);
    number_t im1 = num_sub(inv_x, NUM_ONE);
    number_t ip1 = num_add(inv_x, NUM_ONE);
    number_t denom = num_mul(num_sqrt(im1), num_sqrt(ip1));

    return expr_reverse_num_inverse(denom);
}

static number_t expr_reverse_acosech_inner(const expr_t *dv)
{
    number_t x = expr_eval_num_internal(dv->a);
    number_t x_sq = expr_reverse_num_sq(x);
    number_t inv_x_sq = expr_reverse_num_inverse(x_sq);
    number_t inner = num_add(NUM_ONE, inv_x_sq);

    return expr_reverse_num_inverse(num_sqrt(inner));
}

static number_t expr_reverse_acoth_inner(const expr_t *dv)
{
    number_t x = expr_eval_num_internal(dv->a);
    number_t x_sq = expr_reverse_num_sq(x);
    number_t inv_x_sq = expr_reverse_num_inverse(x_sq);
    number_t denom = num_sub(NUM_ONE, inv_x_sq);

    return expr_reverse_num_inverse(denom);
}

void expr_reverse_asech(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t factor = expr_reverse_inverse_reciprocal_factor(dv, expr_reverse_asech_inner);

    expr_reverse_unary(expr_reverse_num_mul(*out_bar, factor), a_bar, b_bar);
}

void expr_reverse_acosech(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t factor = expr_reverse_inverse_reciprocal_factor(dv, expr_reverse_acosech_inner);

    expr_reverse_unary(expr_reverse_num_mul(*out_bar, factor), a_bar, b_bar);
}

void expr_reverse_acoth(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t factor = expr_reverse_inverse_reciprocal_factor(dv, expr_reverse_acoth_inner);

    expr_reverse_unary(expr_reverse_num_mul(*out_bar, factor), a_bar, b_bar);
}

void expr_reverse_exp(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t factor = num_mul(*out_bar, expr_eval_num_internal(dv));

    expr_reverse_unary(expr_reverse_num_clone(factor), a_bar, b_bar);
}

void expr_reverse_log(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t factor = num_div(*out_bar, expr_eval_num_internal(dv->a));

    expr_reverse_unary(expr_reverse_num_clone(factor), a_bar, b_bar);
}

void expr_reverse_log10(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t denom = num_mul(expr_eval_num_internal(dv->a), NUM_LN10);

    expr_reverse_unary(expr_reverse_num_div(*out_bar, denom), a_bar, b_bar);
}

void expr_reverse_sqrt(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t denom = num_mul(NUM_TWO, expr_eval_num_internal(dv));

    expr_reverse_unary(expr_reverse_num_div(*out_bar, denom), a_bar, b_bar);
}

void expr_reverse_cubrt(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t root_squared = num_sqr(expr_eval_num_internal(dv));
    number_t three = num_create_from_long(3);
    number_t denominator = num_mul(three, root_squared);
    number_t contribution = num_div(*out_bar, denominator);

    num_destroy(&root_squared);
    num_destroy(&three);
    num_destroy(&denominator);
    expr_reverse_unary(contribution, a_bar, b_bar);
}

void expr_reverse_root(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t value = expr_eval_num_internal(dv);
    number_t base = expr_eval_num_internal(dv->a);
    number_t order = expr_eval_num_internal(dv->b);
    number_t scaled = num_mul(*out_bar, value);
    number_t base_denominator = num_mul(base, order);
    number_t log_base = num_log(base);
    number_t order_squared = num_sqr(order);
    number_t order_contribution = num_mul(scaled, log_base);
    number_t neg_order_contribution;

    *a_bar = num_div(scaled, base_denominator);
    neg_order_contribution = num_neg(order_contribution);
    *b_bar = num_div(neg_order_contribution, order_squared);
    num_destroy(&scaled);
    num_destroy(&base_denominator);
    num_destroy(&log_base);
    num_destroy(&order_squared);
    num_destroy(&order_contribution);
    num_destroy(&neg_order_contribution);
}

void expr_reverse_floor(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    (void)dv;
    (void)out_bar;
    expr_reverse_unary(NUM_ZERO, a_bar, b_bar);
}

void expr_reverse_ceil(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    (void)dv;
    (void)out_bar;
    expr_reverse_unary(NUM_ZERO, a_bar, b_bar);
}

void expr_reverse_abs(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    if (!num_is_real(expr_eval_num_internal(dv->a))) {
        *a_bar = NUM_NAN;
        *b_bar = NUM_ZERO;
        return;
    }
    switch (num_cmp(expr_eval_num_internal(dv->a), NUM_ZERO)) {
        case 1:
            *a_bar = expr_reverse_num_clone(*out_bar);
            break;
        case -1:
            *a_bar = expr_reverse_num_neg(*out_bar);
            break;
        default:
            *a_bar = NUM_ZERO;
            break;
    }
    *b_bar = NUM_ZERO;
}

void expr_reverse_conj(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    (void)dv;
    *a_bar = num_conj(*out_bar);
    *b_bar = NUM_ZERO;
}

void expr_reverse_hypot(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t ax = num_div(expr_eval_num_internal(dv->a), expr_eval_num_internal(dv));
    number_t bx = num_div(expr_eval_num_internal(dv->b), expr_eval_num_internal(dv));

    *a_bar = expr_reverse_num_mul(*out_bar, ax);
    *b_bar = expr_reverse_num_mul(*out_bar, bx);
}

void expr_reverse_erf(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t x_sq = expr_reverse_num_sq(expr_eval_num_internal(dv->a));
    number_t neg_x_sq = num_neg(x_sq);
    number_t exp_term = num_exp(neg_x_sq);
    number_t factor = num_mul(NUM_2_SQRTPI, exp_term);

    expr_reverse_unary(expr_reverse_num_mul(*out_bar, factor), a_bar, b_bar);
}

void expr_reverse_erfc(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t x_sq = expr_reverse_num_sq(expr_eval_num_internal(dv->a));
    number_t neg_x_sq = num_neg(x_sq);
    number_t exp_term = num_exp(neg_x_sq);
    number_t factor = num_mul(NUM_NEG_TWO_OVER_SQRT_PI, exp_term);

    expr_reverse_unary(expr_reverse_num_mul(*out_bar, factor), a_bar, b_bar);
}

void expr_reverse_erfinv(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t y_sq = expr_reverse_num_sq(expr_eval_num_internal(dv));
    number_t exp_term = num_exp(y_sq);
    number_t scale = num_div(NUM_SQRT_PI, NUM_TWO);
    number_t factor = num_mul(scale, exp_term);

    expr_reverse_unary(expr_reverse_num_mul(*out_bar, factor), a_bar, b_bar);
}

void expr_reverse_erfcinv(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t y_sq = expr_reverse_num_sq(expr_eval_num_internal(dv));
    number_t scale = num_div(NUM_SQRT_PI, NUM_TWO);
    number_t neg_scale = num_neg(scale);
    number_t exp_term = num_exp(y_sq);
    number_t factor = num_mul(neg_scale, exp_term);

    expr_reverse_unary(expr_reverse_num_mul(*out_bar, factor), a_bar, b_bar);
}

void expr_reverse_gamma(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t digamma_x = num_digamma(expr_eval_num_internal(dv->a));
    number_t factor = num_mul(expr_eval_num_internal(dv), digamma_x);

    expr_reverse_unary(expr_reverse_num_mul(*out_bar, factor), a_bar, b_bar);
}

void expr_reverse_lgamma(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    (void)dv;
    number_t digamma_x = num_digamma(expr_eval_num_internal(dv->a));
    number_t factor = num_mul(*out_bar, digamma_x);

    expr_reverse_unary(expr_reverse_num_clone(factor), a_bar, b_bar);
}

void expr_reverse_digamma(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    (void)dv;
    number_t trigamma_x = num_trigamma(expr_eval_num_internal(dv->a));
    number_t factor = num_mul(*out_bar, trigamma_x);

    expr_reverse_unary(expr_reverse_num_clone(factor), a_bar, b_bar);
}

void expr_reverse_trigamma(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    (void)dv;
    number_t tetragamma_x = num_tetragamma(expr_eval_num_internal(dv->a));
    number_t factor = num_mul(*out_bar, tetragamma_x);

    expr_reverse_unary(expr_reverse_num_clone(factor), a_bar, b_bar);
}

void expr_reverse_polygamma(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t order_value = expr_eval_num_internal(dv->a);
    number_t next;
    number_t factor;
    unsigned int order;

    if (!expr_number_to_polygamma_order(order_value, &order) || order == UINT_MAX) {
        expr_reverse_binary(NUM_ZERO, NUM_ZERO, a_bar, b_bar);
        return;
    }

    next = num_polygamma(order + 1u, expr_eval_num_internal(dv->b));
    factor = num_mul(*out_bar, next);
    expr_reverse_binary(NUM_ZERO, expr_reverse_num_clone(factor), a_bar, b_bar);
}

void expr_reverse_dilog(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t one_minus = num_sub(NUM_ONE, expr_eval_num_internal(dv->a));
    number_t log_term = num_log(one_minus);
    number_t neg_log = num_neg(log_term);
    number_t factor = num_div(neg_log, expr_eval_num_internal(dv->a));

    expr_reverse_unary(expr_reverse_num_mul(*out_bar, factor), a_bar, b_bar);
}

void expr_reverse_polylog(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t order_value = expr_eval_num_internal(dv->a);
    number_t z = expr_eval_num_internal(dv->b);
    number_t factor;
    number_t scaled;
    unsigned int order;

    if (!expr_number_to_polygamma_order(order_value, &order)) {
        expr_reverse_binary(NUM_ZERO, NUM_NAN, a_bar, b_bar);
        return;
    }

    if (order == 0u) {
        number_t one_minus = num_sub(NUM_ONE, z);
        number_t den = num_sqr(one_minus);

        factor = num_div(NUM_ONE, den);
    } else {
        number_t prev_order = num_create_from_long((long)order - 1L);
        number_t prev = num_polylog(prev_order, z);

        factor = num_div(prev, z);
    }

    scaled = num_mul(*out_bar, factor);
    expr_reverse_binary(NUM_ZERO, expr_reverse_num_clone(scaled), a_bar, b_bar);
}

void expr_reverse_legendre_chi(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t order_value = expr_eval_num_internal(dv->a);
    number_t z = expr_eval_num_internal(dv->b);
    number_t factor;
    number_t scaled;
    unsigned int order;

    if (!expr_number_to_polygamma_order(order_value, &order)) {
        expr_reverse_binary(NUM_ZERO, NUM_NAN, a_bar, b_bar);
        return;
    }

    if (order == 0u) {
        number_t z_sq = num_sqr(z);
        number_t numerator = num_add(NUM_ONE, z_sq);
        number_t one_minus_z_sq = num_sub(NUM_ONE, z_sq);
        number_t den = num_sqr(one_minus_z_sq);

        factor = num_div(numerator, den);
    } else {
        number_t prev_order = num_create_from_long((long)order - 1L);
        number_t prev = num_legendre_chi(prev_order, z);

        factor = num_div(prev, z);
    }

    scaled = num_mul(*out_bar, factor);
    expr_reverse_binary(NUM_ZERO, expr_reverse_num_clone(scaled), a_bar, b_bar);
}

static void expr_reverse_bessel(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar,
                                number_t (*function)(number_t, number_t))
{
    number_t order = expr_eval_num_internal(dv->a);
    number_t argument = expr_eval_num_internal(dv->b);
    number_t lower_order = num_sub(order, NUM_ONE);
    number_t upper_order = num_add(order, NUM_ONE);
    number_t lower = function(lower_order, argument);
    number_t upper = function(upper_order, argument);
    number_t factor = num_mul(NUM_HALF, num_sub(lower, upper));

    expr_reverse_binary(NUM_NAN, num_mul(*out_bar, factor), a_bar, b_bar);
}

void expr_reverse_bessel_j(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    expr_reverse_bessel(dv, out_bar, a_bar, b_bar, num_bessel_j);
}

void expr_reverse_bessel_y(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    expr_reverse_bessel(dv, out_bar, a_bar, b_bar, num_bessel_y);
}

void expr_reverse_parameter_pack(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    (void)dv;
    expr_reverse_binary(num_clone(*out_bar), num_clone(*out_bar), a_bar, b_bar);
}

void expr_reverse_lommel_s(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    const expr_t *mu = NULL;
    const expr_t *nu = NULL;
    const expr_t *argument = NULL;
    number_t factor;

    if (!expr_lommel_s_unpack(dv, &mu, &nu, &argument)) {
        expr_reverse_binary(NUM_NAN, NUM_NAN, a_bar, b_bar);
        return;
    }
    factor = num_lommel_s_derivative_internal(expr_eval_num_internal(mu), expr_eval_num_internal(nu),
                                              expr_eval_num_internal(argument));
    expr_reverse_binary(NUM_NAN, num_mul(*out_bar, factor), a_bar, b_bar);
}

void expr_reverse_hypergeometric_pFq(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    const expr_t **upper = NULL;
    const expr_t **lower = NULL;
    const expr_t *argument_expr = NULL;
    number_t *upper_shifted = NULL;
    number_t *lower_shifted = NULL;
    number_t numerator = NUM_ONE;
    number_t denominator = NUM_ONE;
    number_t shifted;
    number_t factor;
    size_t p = 0u;
    size_t q = 0u;

    if (!expr_hypergeometric_pFq_unpack(dv, &upper, &p, &lower, &q, &argument_expr))
        goto failure;
    if (p > 0u) {
        upper_shifted = calloc(p, sizeof(*upper_shifted));
        if (!upper_shifted)
            goto failure;
    }
    if (q > 0u) {
        lower_shifted = calloc(q, sizeof(*lower_shifted));
        if (!lower_shifted)
            goto failure;
    }
    for (size_t i = 0u; i < p; ++i) {
        number_t value = expr_eval_num_internal(upper[i]);

        numerator = num_mul(numerator, value);
        upper_shifted[i] = num_add(value, NUM_ONE);
    }
    for (size_t i = 0u; i < q; ++i) {
        number_t value = expr_eval_num_internal(lower[i]);

        denominator = num_mul(denominator, value);
        lower_shifted[i] = num_add(value, NUM_ONE);
    }
    shifted = num_hypergeometric_pFq(upper_shifted, p, lower_shifted, q, expr_eval_num_internal(argument_expr));
    factor = num_mul(num_div(numerator, denominator), shifted);
    expr_reverse_binary(NUM_NAN, num_mul(*out_bar, factor), a_bar, b_bar);
    free(lower_shifted);
    free(upper_shifted);
    free(lower);
    free(upper);
    return;

failure:
    free(lower_shifted);
    free(upper_shifted);
    free(lower);
    free(upper);
    expr_reverse_binary(NUM_NAN, NUM_NAN, a_bar, b_bar);
}

static int expr_reverse_emit(const number_t *out_bar, number_t factor, expr_reverse_accumulate_fn accumulate,
                             void *context, const expr_t *child)
{
    number_t contribution = num_mul(*out_bar, factor);

    return accumulate(context, child, &contribution);
}

static int expr_reverse_emit_nan(expr_reverse_accumulate_fn accumulate, void *context, const expr_t *child)
{
    number_t contribution = NUM_NAN;

    return accumulate(context, child, &contribution);
}

int expr_reverse_appell_f1_many(const expr_t *dv, const number_t *out_bar, expr_reverse_accumulate_fn accumulate,
                                void *context)
{
    const expr_t *a = NULL;
    const expr_t *b1 = NULL;
    const expr_t *b2 = NULL;
    const expr_t *c = NULL;
    const expr_t *x = NULL;
    const expr_t *y = NULL;
    number_t av;
    number_t b1v;
    number_t b2v;
    number_t cv;
    number_t xv;
    number_t yv;
    number_t a1;
    number_t c1;
    number_t x_factor;
    number_t y_factor;

    if (!expr_appell_f1_unpack(dv, &a, &b1, &b2, &c, &x, &y))
        return -1;
    if (expr_reverse_emit_nan(accumulate, context, a) != 0 || expr_reverse_emit_nan(accumulate, context, b1) != 0 ||
        expr_reverse_emit_nan(accumulate, context, b2) != 0 || expr_reverse_emit_nan(accumulate, context, c) != 0)
        return -1;

    av = expr_eval_num_internal(a);
    b1v = expr_eval_num_internal(b1);
    b2v = expr_eval_num_internal(b2);
    cv = expr_eval_num_internal(c);
    xv = expr_eval_num_internal(x);
    yv = expr_eval_num_internal(y);
    a1 = num_add(av, NUM_ONE);
    c1 = num_add(cv, NUM_ONE);
    x_factor = num_mul(num_div(num_mul(av, b1v), cv), num_appell_f1(a1, num_add(b1v, NUM_ONE), b2v, c1, xv, yv));
    y_factor = num_mul(num_div(num_mul(av, b2v), cv), num_appell_f1(a1, b1v, num_add(b2v, NUM_ONE), c1, xv, yv));
    return expr_reverse_emit(out_bar, x_factor, accumulate, context, x) == 0 &&
                   expr_reverse_emit(out_bar, y_factor, accumulate, context, y) == 0
               ? 0
               : -1;
}

int expr_reverse_lauricella_f_many(const expr_t *dv, const number_t *out_bar, expr_reverse_accumulate_fn accumulate,
                                   void *context)
{
    const expr_t *a = NULL;
    const expr_t **b = NULL;
    const expr_t *c = NULL;
    const expr_t **x = NULL;
    number_t *b_values = NULL;
    number_t *x_values = NULL;
    number_t *shifted_b = NULL;
    number_t av;
    number_t cv;
    number_t a1;
    number_t c1;
    size_t count = 0u;
    int status = -1;

    if (!expr_lauricella_f_unpack(dv, &a, &b, &c, &x, &count) || expr_reverse_emit_nan(accumulate, context, a) != 0 ||
        expr_reverse_emit_nan(accumulate, context, c) != 0)
        goto cleanup;
    if (count > 0u) {
        b_values = calloc(count, sizeof(*b_values));
        x_values = calloc(count, sizeof(*x_values));
        shifted_b = calloc(count, sizeof(*shifted_b));
        if (!b_values || !x_values || !shifted_b)
            goto cleanup;
    }
    for (size_t i = 0u; i < count; ++i) {
        if (expr_reverse_emit_nan(accumulate, context, b[i]) != 0)
            goto cleanup;
        b_values[i] = expr_eval_num_internal(b[i]);
        x_values[i] = expr_eval_num_internal(x[i]);
        shifted_b[i] = b_values[i];
    }
    av = expr_eval_num_internal(a);
    cv = expr_eval_num_internal(c);
    a1 = num_add(av, NUM_ONE);
    c1 = num_add(cv, NUM_ONE);
    for (size_t i = 0u; i < count; ++i) {
        number_t factor;

        shifted_b[i] = num_add(b_values[i], NUM_ONE);
        factor = num_mul(num_div(num_mul(av, b_values[i]), cv), num_lauricella_f(a1, shifted_b, c1, x_values, count));
        if (expr_reverse_emit(out_bar, factor, accumulate, context, x[i]) != 0)
            goto cleanup;
        shifted_b[i] = b_values[i];
    }
    status = 0;

cleanup:
    free(shifted_b);
    free(x_values);
    free(b_values);
    free(x);
    free(b);
    return status;
}

void expr_reverse_gammainv(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t y = expr_eval_num_internal(dv);
    number_t psi_y = num_digamma(y);
    number_t x_psi = num_mul(expr_eval_num_internal(dv->a), psi_y);
    number_t factor = num_inv(x_psi);

    expr_reverse_unary(expr_reverse_num_mul(*out_bar, factor), a_bar, b_bar);
}

static number_t num_lambert_reverse_factor(const number_t z, const number_t w)
{
    if (num_eq(z, NUM_ZERO))
        return NUM_ONE;
    number_t one_plus_w = num_add(NUM_ONE, w);
    {
        number_t denom = num_mul(z, one_plus_w);
        return num_div(w, denom);
    }
}

void expr_reverse_lambert_w0(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t factor = num_lambert_reverse_factor(expr_eval_num_internal(dv->a), expr_eval_num_internal(dv));

    expr_reverse_unary(expr_reverse_num_mul(*out_bar, factor), a_bar, b_bar);
}

void expr_reverse_lambert_w(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t factor = num_lambert_reverse_factor(expr_eval_num_internal(dv->a), expr_eval_num_internal(dv));

    expr_reverse_unary(expr_reverse_num_mul(*out_bar, factor), a_bar, b_bar);
}

void expr_reverse_lambert_wn(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t factor = num_lambert_reverse_factor(expr_eval_num_internal(dv->b), expr_eval_num_internal(dv));

    expr_reverse_binary(NUM_ZERO, expr_reverse_num_mul(*out_bar, factor), a_bar, b_bar);
}

void expr_reverse_lambert_wm1(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t factor = num_lambert_reverse_factor(expr_eval_num_internal(dv->a), expr_eval_num_internal(dv));

    expr_reverse_unary(expr_reverse_num_mul(*out_bar, factor), a_bar, b_bar);
}

void expr_reverse_normal_pdf(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t x_pdf = num_mul(expr_eval_num_internal(dv->a), expr_eval_num_internal(dv));
    number_t factor = num_neg(x_pdf);

    expr_reverse_unary(expr_reverse_num_mul(*out_bar, factor), a_bar, b_bar);
}

void expr_reverse_normal_cdf(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t pdf_x = num_normal_pdf(expr_eval_num_internal(dv->a));
    number_t factor = num_mul(*out_bar, pdf_x);

    expr_reverse_unary(expr_reverse_num_clone(factor), a_bar, b_bar);
}

void expr_reverse_normal_logpdf(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t neg_x = num_neg(expr_eval_num_internal(dv->a));
    number_t factor = num_mul(*out_bar, neg_x);

    expr_reverse_unary(expr_reverse_num_clone(factor), a_bar, b_bar);
}

void expr_reverse_ei(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t exp_x = num_exp(expr_eval_num_internal(dv->a));
    number_t factor = num_div(exp_x, expr_eval_num_internal(dv->a));

    expr_reverse_unary(expr_reverse_num_mul(*out_bar, factor), a_bar, b_bar);
}

void expr_reverse_e1(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t neg_x = num_neg(expr_eval_num_internal(dv->a));
    number_t exp_neg_x = num_exp(neg_x);
    number_t factor = num_div(exp_neg_x, expr_eval_num_internal(dv->a));
    number_t neg_factor = num_neg(factor);

    expr_reverse_unary(expr_reverse_num_mul(*out_bar, neg_factor), a_bar, b_bar);
}

void expr_reverse_beta(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t a_plus_b = num_add(expr_eval_num_internal(dv->a), expr_eval_num_internal(dv->b));
    number_t digamma_a = num_digamma(expr_eval_num_internal(dv->a));
    number_t digamma_b = num_digamma(expr_eval_num_internal(dv->b));
    number_t psi_ab = num_digamma(a_plus_b);
    number_t psi_a_minus = num_sub(digamma_a, psi_ab);
    number_t psi_b_minus = num_sub(digamma_b, psi_ab);
    number_t scale_a = num_mul(expr_eval_num_internal(dv), psi_a_minus);
    number_t scale_b = num_mul(expr_eval_num_internal(dv), psi_b_minus);

    *a_bar = expr_reverse_num_mul(*out_bar, scale_a);
    *b_bar = expr_reverse_num_mul(*out_bar, scale_b);
}

void expr_reverse_logbeta(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t a_plus_b = num_add(expr_eval_num_internal(dv->a), expr_eval_num_internal(dv->b));
    number_t digamma_a = num_digamma(expr_eval_num_internal(dv->a));
    number_t digamma_b = num_digamma(expr_eval_num_internal(dv->b));
    number_t psi_ab = num_digamma(a_plus_b);
    number_t scale_a = num_sub(digamma_a, psi_ab);
    number_t scale_b = num_sub(digamma_b, psi_ab);

    *a_bar = expr_reverse_num_mul(*out_bar, scale_a);
    *b_bar = expr_reverse_num_mul(*out_bar, scale_b);
}

static number_t gammainc_x_density_num(const expr_t *dv)
{
    number_t s_minus_one = num_sub(expr_eval_num_internal(dv->a), NUM_ONE);
    number_t x_pow = num_pow(expr_eval_num_internal(dv->b), s_minus_one);
    number_t neg_x = num_neg(expr_eval_num_internal(dv->b));
    number_t exp_neg_x = num_exp(neg_x);

    return num_mul(x_pow, exp_neg_x);
}

static void expr_reverse_gammainc_x_only(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar,
                                         int sign, int regularised)
{
    number_t factor = gammainc_x_density_num(dv);

    if (regularised) {
        number_t gamma_s = num_gamma(expr_eval_num_internal(dv->a));
        factor = num_div(factor, gamma_s);
    }
    if (sign < 0)
        factor = num_neg(factor);

    *a_bar = NUM_NAN;
    *b_bar = expr_reverse_num_mul(*out_bar, factor);
}

void expr_reverse_gammainc_lower(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    expr_reverse_gammainc_x_only(dv, out_bar, a_bar, b_bar, 1, 0);
}

void expr_reverse_gammainc_upper(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    expr_reverse_gammainc_x_only(dv, out_bar, a_bar, b_bar, -1, 0);
}

void expr_reverse_gammainc_P(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    expr_reverse_gammainc_x_only(dv, out_bar, a_bar, b_bar, 1, 1);
}

void expr_reverse_gammainc_Q(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    expr_reverse_gammainc_x_only(dv, out_bar, a_bar, b_bar, -1, 1);
}
