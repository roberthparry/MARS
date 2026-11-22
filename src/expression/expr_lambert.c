/**
 * @file expr_lambert.c
 * @brief Construction, analytic calculus and finite quotient identities for MARS's Lambert sum.
 *
 * lambert_S is an explicitly defined MARS function, not a claimed standard special-function name.
 * Three continuous arguments and three discrete parameters use ordinary argument-list storage. Contiguous
 * relations provide chain rules and affine-coordinate primitives. Every native TeX context that uses this
 * operation includes its definition, through the owning notation provider.
 */
#include <stdint.h>

#include "expression-t.h"
#include "number-t.h"
#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_maths.h"

static expr_t *lambert_wrap(const expr_ops_t *ops, const expr_t *left, const expr_t *right)
{
    if (!left || !right) return NULL;
    expr_retain(left); expr_retain(right);
    return expr_new_binary_internal(ops, left, right);
}

expr_t *lambert_rebuild(const expr_t *left, const expr_t *right)
{
    return lambert_wrap(&ops_lambert_S, left, right);
}

bool expr_lambert_unpack(const expr_t *expression, const expr_t **args)
{
    if (!expression || expression->ops->kind != EXPR_KIND_LAMBERT_S || !args) return false;
    const expr_t *pack = expression->a;
    for (unsigned j = 4; j > 0; --j) {
        if (!pack || pack->ops->kind != EXPR_KIND_ARGUMENT_LIST) return false;
        args[j] = pack->b; pack = pack->a;
    }
    args[0] = pack; args[5] = expression->b;
    return pack && args[5];
}

/* Construct MARS's defined Lambert sum, retaining each borrowed argument. */
expr_t *expr_lambert_S(const expr_t *a, const expr_t *q, const expr_t *z, const expr_t *weight,
                       const expr_t *order, const expr_t *count)
{
    if (!a || !q || !z || !weight || !order || !count) return NULL;
    const expr_t *args[] = {a, q, z, weight, order};
    expr_t *pack = (expr_t *)a;
    expr_retain(a);
    for (unsigned j = 1; pack && j < 5; ++j) {
        expr_t *next = lambert_wrap(&ops_argument_list, pack, args[j]);
        expr_free(pack); pack = next;
    }
    expr_t *result = lambert_rebuild(pack, count);
    expr_free(pack);
    return result;
}

expr_t *expr_lambert_args(size_t count, expr_t *const *args)
{
    expr_t *infinity = count == 5 ? expr_new_const(NUM_INF) : NULL;
    expr_t *out = count == 5 || count == 6 ?
        expr_lambert_S(args[0], args[1], args[2], args[3], args[4], count == 5 ? infinity : args[5]) : NULL;
    expr_free(infinity);
    return out;
}

expr_t *expr_lambda_args(size_t count, expr_t *const *args)
{
    return count == 1 ? expr_tri(args[0]) : expr_lambert_args(count, args);
}

number_t lambert_eval(expr_t *expression)
{
    NUM_SCOPE(scope);
    const expr_t *args[6];
    if (!expr_lambert_unpack(expression, args)) return num_clone(NUM_NAN);
    number_t values[6];
    for (unsigned j = 0; j < 6; ++j) values[j] = expr_eval_num_internal(args[j]);
    number_t value = num_lambert_S(values[0], values[1], values[2], values[3], values[4], values[5]);
    /* Li_-1(exp(i*t)) is real; applying (1/i)*d/dt alternates real and imaginary negative orders.
     * Prove unit-circle arguments from the exponential's input, rather than a numerical tolerance. */
    long order, denominator;
    if (args[1]->ops->kind == EXPR_KIND_EXP &&
        num_get_small_rational(values[4], &order, &denominator) && denominator == 1 && order >= -32 && order < 0 &&
        num_is_real(values[2]) && num_is_integer(values[2]) && num_is_finite(values[5]) &&
        (num_eq(values[0], NUM_ONE) || num_eq(values[0], NUM_NEG_ONE) ||
         num_eq(values[0], NUM_I) || num_eq(values[0], num_neg(NUM_I)))) {
        number_t exponent = expr_eval_num_internal(args[1]->a);
        number_t real = num_real_part(exponent);
        if (num_is_finite(exponent) && num_is_zero(real) && num_is_finite(value))
            value = order % 2 ? num_real_part(value) : num_mul(NUM_I, num_imag_part(value));
    }
    return num_scope_detach(value);
}

static expr_t *lambert_shifted(const expr_t *const *args, long weight_shift, long order_shift)
{
    expr_t *raw_r = expr_add_long(args[3], weight_shift), *raw_s = expr_add_long(args[4], order_shift);
    expr_t *r = raw_r ? expr_simplify(raw_r) : NULL, *s = raw_s ? expr_simplify(raw_s) : NULL;
    expr_t *out = expr_lambert_S(args[0], args[1], args[2], r, s, args[5]);
    expr_free(raw_r); expr_free(raw_s);
    expr_free(r); expr_free(s);
    return out;
}

static expr_t *lambert_partial(const expr_t *const *args, unsigned axis)
{
    if (axis < 2) {
        expr_t *shifted = lambert_shifted(args, axis == 1 ? 1 : 0, -1);
        expr_t *out = shifted ? expr_div(shifted, args[axis]) : NULL;
        expr_free(shifted);
        return out;
    }
    expr_t *lower_s = lambert_shifted(args, 0, -1), *log_q = expr_log(args[1]);
    expr_t *right = lower_s && log_q ? expr_mul(log_q, lower_s) : NULL;
    expr_t *lower_r = expr_is_exact_zero(args[3]) ? NULL : lambert_shifted(args, -1, 0);
    expr_t *left = lower_r ? expr_mul(args[3], lower_r) : expr_const_zero();
    expr_t *out = left && right ? expr_add(left, right) : NULL;
    expr_free(left); expr_free(right); expr_free(lower_r); expr_free(log_q); expr_free(lower_s);
    return out;
}

expr_t *lambert_derivative(expr_t *expression)
{
    const expr_t *args[6];
    if (!expr_lambert_unpack(expression, args)) return expr_new_const(NUM_NAN);
    for (unsigned j = 3; j < 6; ++j) {
        expr_t *dx = expr_get_dx_internal(args[j]);
        bool fixed = dx && expr_is_exact_zero(dx);
        expr_free(dx);
        if (!fixed) return expr_new_const(NUM_NAN);
    }
    expr_t *out = expr_const_zero();
    for (unsigned axis = 0; out && axis < 3; ++axis) {
        expr_t *dx = expr_get_dx_internal(args[axis]);
        if (dx && !expr_is_exact_zero(dx)) {
            expr_t *partial = lambert_partial(args, axis);
            expr_t *term = partial ? expr_mul(partial, dx) : NULL;
            expr_t *next = term ? expr_add(out, term) : NULL;
            expr_free(out); out = next;
            expr_free(term); expr_free(partial);
        }
        expr_free(dx);
    }
    return out;
}

int lambert_reverse(const expr_t *expression, const number_t *out_bar,
                    expr_reverse_accumulate_fn accumulate, void *context)
{
    NUM_SCOPE(scope);
    const expr_t *args[6];
    if (!expr_lambert_unpack(expression, args)) return -1;
    for (unsigned j = 3; j < 6; ++j) {
        number_t unspecified = NUM_NAN;
        if (accumulate(context, args[j], &unspecified) != 0) return -1;
    }
    for (unsigned axis = 0; axis < 3; ++axis) {
        expr_t *partial = lambert_partial(args, axis);
        number_t value = partial ? expr_eval_num_internal(partial) : NUM_NAN;
        number_t contribution = num_mul(value, *out_bar);
        expr_free(partial);
        if (accumulate(context, args[axis], &contribution) != 0) return -1;
    }
    return 0;
}

expr_t *expr_integrate_lambert(const expr_t *expression, const expr_t *wrt)
{
    NUM_SCOPE(scope);
    const expr_t *args[6];
    if (!wrt || !expr_lambert_unpack(expression, args)) return NULL;
    for (unsigned j = 0; j < 6; ++j) {
        if (j == 2) continue;
        expr_t *dx = expr_create_deriv(args[j], wrt);
        bool fixed = dx && expr_is_exact_zero(dx);
        expr_free(dx);
        if (!fixed) return NULL;
    }
    number_t r_value = expr_eval_num_internal(args[3]);
    long r, denominator;
    if (!num_get_small_rational(r_value, &r, &denominator) || denominator != 1 || r < 0 || r > 64) return NULL;
    expr_t *dz = expr_create_deriv(args[2], wrt), *ddz = dz ? expr_create_deriv(dz, wrt) : NULL;
    if (!dz || expr_is_exact_zero(dz) || !ddz || !expr_is_exact_zero(ddz)) {
        expr_free(ddz); expr_free(dz); return NULL;
    }
    expr_free(ddz);
    if (expr_is_const(args[1]) && num_is_one(args[1]->c)) {
        expr_t *raised = lambert_shifted(args, 1, 0), *divided = raised ? expr_div_long(raised, r + 1) : NULL;
        expr_t *out = divided ? expr_div(divided, dz) : NULL;
        expr_free(divided); expr_free(raised); expr_free(dz);
        return out;
    }
    expr_t *one = expr_const_one(), *log_q = expr_log(args[1]);
    expr_t *coefficient = log_q && one ? expr_div(one, log_q) : NULL, *sum = expr_const_zero();
    expr_free(one);
    for (long j = 0; sum && coefficient && j <= r; ++j) {
        expr_t *function = lambert_shifted(args, -j, j + 1), *term = function ? expr_mul(coefficient, function) : NULL;
        expr_t *next = term ? expr_add(sum, term) : NULL;
        expr_free(sum); sum = next; expr_free(term); expr_free(function);
        if (j < r) {
            expr_t *scaled = expr_mul_long(coefficient, j - r);
            expr_t *next_coefficient = scaled ? expr_div(scaled, log_q) : NULL;
            expr_free(scaled); expr_free(coefficient); coefficient = next_coefficient;
        }
    }
    expr_t *out = sum ? expr_div(sum, dz) : NULL;
    expr_free(sum); expr_free(coefficient); expr_free(log_q); expr_free(dz);
    return out;
}

/* Li_-1 gives explicit derivatives of all eight finite trigonometric quotient families. */
expr_t *expr_lambert_progression_derivative(const expr_t *upper, const expr_t *step,
                                           expr_op_kind_t kind, const expr_t *wrt)
{
    unsigned slot = expr_trig_slot(kind);
    if (slot >= 12 || slot % 6 < 2) return NULL;
    unsigned family = slot % 6;
    bool hyperbolic = slot >= 6, paired = family == 3 || family == 4;
    expr_t *one = expr_const_one(), *minus_one = expr_new_const(NUM_NEG_ONE), *i = expr_new_const(NUM_I);
    number_t phase_value = hyperbolic ? num_create_from_long(paired ? -1 : -2) :
                                       num_mul_long(NUM_I, paired ? 1 : 2);
    expr_t *phase = expr_new_const(phase_value), *scaled = phase ? expr_mul(phase, step) : NULL;
    expr_t *q = scaled ? expr_exp(scaled) : NULL;
    num_destroy(&phase_value);
    expr_t *a = expr_clone(family == 2 ? minus_one : family == 3 ? i : one);
    expr_t *first = expr_lambert_S(a, q, one, one, minus_one, upper), *difference = first;
    if (paired) {
        expr_t *negative_a = expr_neg(a), *second = expr_lambert_S(negative_a, q, one, one, minus_one, upper);
        difference = first && second ? expr_sub(first, second) : NULL;
        expr_free(second); expr_free(negative_a); expr_free(first);
    }
    expr_t *factor = !paired ? expr_const_long(family == 2 || hyperbolic ? -4 : 4) :
                     hyperbolic && family == 3 ? expr_clone(i) : hyperbolic ? expr_clone(minus_one) : expr_clone(one);
    expr_t *dx = expr_create_deriv(step, wrt), *coefficient = factor && dx ? expr_mul(factor, dx) : NULL;
    expr_t *raw_result = coefficient && difference ? expr_mul(coefficient, difference) : NULL;
    expr_t *result = raw_result ? expr_simplify(raw_result) : NULL;
    expr_free(raw_result);
    expr_free(coefficient); expr_free(dx); expr_free(factor); expr_free(difference);
    expr_free(a); expr_free(q); expr_free(scaled); expr_free(phase);
    expr_free(i); expr_free(minus_one); expr_free(one);
    return result;
}
