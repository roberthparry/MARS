/**
 * @file expr_laplace_folded.c
 * @brief Unilateral Laplace spectra of affine periodic inverse-circular folds.
 *
 * The elementary dispatcher supplies finite real constant coefficients for
 * asin/acos composed with sin/cos. Reduce to acos(cos(q t+p)), q positive,
 * and sum its alternating slope jumps, separated by pi/q. Exact symbolic
 * phases and frequencies are retained; numerical evaluation only selects the
 * known real branch. Unsupported or unresolved phases remain formal transforms.
 * The caller retains the conservative convergence half-plane Re(s)>0.
 */
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"

/* Construct the periodic spectrum without substituting the target variable's binding. */
expr_t *expr_laplace_folded_formula(const expr_t *f, const expr_t *rate, const expr_t *offset, const expr_t *s)
{
    number_t a = expr_eval(rate);
    if (num_is_zero(a)) {
        expr_t *inner = f->a->ops->apply_unary(offset);
        expr_t *initial = f->ops->apply_unary(inner);
        expr_t *out = expr_div(initial, s);
        expr_free(initial);
        expr_free(inner);
        num_destroy(&a);
        return out;
    }
    bool negative = num_lt(a, NUM_ZERO);
    num_destroy(&a);
    expr_t *pi = expr_new_named_const(NUM_PI, "@pi"), *two = expr_const_long(2), *one = expr_const_one();
    expr_t *half_pi = expr_div(pi, two), *period = expr_mul(two, pi);
    expr_t *phase = f->a->ops == &ops_sin ? expr_sub(offset, half_pi) : expr_clone(offset);
    expr_t *directed = negative ? expr_neg(phase) : expr_clone(phase);
    expr_t *quotient = expr_div(directed, period), *turns = expr_floor(quotient);
    expr_t *winding = expr_mul(period, turns), *remainder = expr_sub(directed, winding);
    expr_t *p = expr_beautify(remainder), *q = negative ? expr_neg(rate) : expr_clone(rate);
    number_t pv = expr_eval(p), circumference = expr_eval(period), peak = expr_eval(pi);
    expr_t *out = NULL;
    if (num_is_real(pv) && num_is_finite(pv) && !num_lt(pv, NUM_ZERO) && num_lt(pv, circumference)) {
        bool rising = num_lt(pv, peak);
        expr_t *raw_height = rising ? expr_clone(p) : expr_sub(period, p);
        expr_t *height = expr_beautify(raw_height);
        expr_t *slope = rising ? expr_clone(q) : expr_neg(q);
        expr_t *distance = rising ? expr_sub(pi, p) : expr_clone(height);
        expr_t *delay = expr_div(distance, q), *half_period = expr_div(pi, q);
        expr_t *first_exponent = expr_mul(s, delay), *repeat_exponent = expr_mul(s, half_period);
        expr_t *negative_first = expr_neg(first_exponent), *negative_repeat = expr_neg(repeat_exponent);
        expr_t *first = expr_exp(negative_first), *repeat = expr_exp(negative_repeat);
        expr_t *denominator = expr_add(one, repeat), *jump = expr_mul(two, slope);
        expr_t *weighted = expr_mul(jump, first), *correction = expr_div(weighted, denominator);
        expr_t *scaled_height = expr_mul(height, s), *initial = expr_add(scaled_height, slope);
        expr_t *numerator = expr_sub(initial, correction), *square = expr_mul(s, s);
        out = expr_div(numerator, square);
        if (f->ops == &ops_asin) {
            expr_t *constant = expr_div(half_pi, s), *complement = expr_sub(constant, out);
            expr_free(out);
            expr_free(constant);
            out = complement;
        }
        expr_free(square);
        expr_free(numerator);
        expr_free(initial);
        expr_free(scaled_height);
        expr_free(correction);
        expr_free(weighted);
        expr_free(jump);
        expr_free(denominator);
        expr_free(repeat);
        expr_free(first);
        expr_free(negative_repeat);
        expr_free(negative_first);
        expr_free(repeat_exponent);
        expr_free(first_exponent);
        expr_free(half_period);
        expr_free(delay);
        expr_free(distance);
        expr_free(slope);
        expr_free(height);
        expr_free(raw_height);
    }
    num_destroy(&peak);
    num_destroy(&circumference);
    num_destroy(&pv);
    expr_free(q);
    expr_free(p);
    expr_free(remainder);
    expr_free(winding);
    expr_free(turns);
    expr_free(quotient);
    expr_free(directed);
    expr_free(phase);
    expr_free(period);
    expr_free(half_pi);
    expr_free(one);
    expr_free(two);
    expr_free(pi);
    return out;
}
