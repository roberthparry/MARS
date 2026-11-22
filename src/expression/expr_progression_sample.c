/**
 * @file expr_progression_sample.c
 * @brief Pole-aware and growth-aware native sampling windows for finite progressions.
 *
 * Recognises fixed finite tangent, cotangent, secant and cosecant sums with an
 * affine real step, and rapidly growing sinh, cosh and exponential sums. Their
 * largest frequency or growth rate selects an informative automatic neighbourhood.
 * A separate circular interval check proves when value-only adaptive
 * sampling is sufficient, without changing the authored formula or its values.
 * Displayed q-digamma combinations are recovered through the existing native
 * recogniser; individual unit-circle q-digamma terms are never evaluated here.
 * Their explicit finite Lambert derivatives retain the same pole-aware real windows,
 * including fixed scalar multiples and paired secant/cosecant terms.
 */
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"

typedef struct {
    number_t count, slope, offset;
    bool pole_at_zero;
} affine_progression_t;

static bool contains_axis(const expr_t *expression, const expr_t *axis)
{
    return expression && (expression == axis ||
        (expr_is_var(expression) && expression->var_id && expression->var_id == axis->var_id) ||
        contains_axis(expression->a, axis) || contains_axis(expression->b, axis));
}

static const expr_t *lambert_progression(const expr_t *expression, const expr_t *axis)
{
    if (!expression)
        return NULL;
    expr_op_kind_t kind = expression->ops->kind;
    if (kind == EXPR_KIND_LAMBERT_S)
        return expression;
    if (kind == EXPR_KIND_NEG)
        return lambert_progression(expression->a, axis);
    if (kind == EXPR_KIND_MUL || kind == EXPR_KIND_ADD || kind == EXPR_KIND_SUB) {
        if (!contains_axis(expression->a, axis))
            return lambert_progression(expression->b, axis);
        if (!contains_axis(expression->b, axis))
            return lambert_progression(expression->a, axis);
        if (kind != EXPR_KIND_MUL) {
            const expr_t *left = lambert_progression(expression->a, axis);
            const expr_t *right = lambert_progression(expression->b, axis);
            const expr_t *a[6], *b[6];
            if (!expr_lambert_unpack(left, a) || !expr_lambert_unpack(right, b))
                return NULL;
            for (unsigned j = 1; j < 6; ++j)
                if (j != 3 && !expr_struct_eq(a[j], b[j]))
                    return NULL;
            number_t second_weight = expr_eval(b[3]);
            if (!num_is_real(second_weight) || !num_is_integer(second_weight) ||
                num_lt(second_weight, NUM_ZERO) || num_gt(second_weight, num_create_from_long(64)))
                return NULL;
            number_t first = expr_eval(a[0]), second = expr_eval(b[0]);
            if ((num_eq(first, NUM_ONE) && num_eq(second, NUM_NEG_ONE)) ||
                (num_eq(first, NUM_NEG_ONE) && num_eq(second, NUM_ONE)))
                return num_eq(first, NUM_ONE) ? left : right;
            bool matched = num_eq(first, second) ||
                ((num_eq(first, NUM_I) || num_eq(first, num_neg(NUM_I))) && num_eq(first, num_neg(second)));
            return matched ? left : NULL;
        }
    }
    return NULL;
}

static bool lambert_affine_progression(const expr_t *expression, const expr_t *axis, affine_progression_t *out)
{
    const expr_t *args[6];
    if (!expr_lambert_unpack(lambert_progression(expression, axis), args) || args[1]->ops->kind != EXPR_KIND_EXP)
        return false;
    for (unsigned j = 0; j < 6; ++j)
        if (j != 1 && contains_axis(args[j], axis))
            return false;
    number_t a = expr_eval(args[0]), z = expr_eval(args[2]), r = expr_eval(args[3]), s = expr_eval(args[4]);
    bool paired = num_eq(a, NUM_I) || num_eq(a, num_neg(NUM_I));
    if ((!paired && !num_eq(a, NUM_ONE) && !num_eq(a, NUM_NEG_ONE)) || !num_eq(z, NUM_ONE) ||
        !num_is_real(r) || !num_is_integer(r) || num_lt(r, NUM_ZERO) || num_gt(r, num_create_from_long(64)) ||
        !num_is_real(s) || !num_is_integer(s) || !num_lt(s, NUM_ZERO) || num_lt(s, num_create_from_long(-32)))
        return false;
    out->count = expr_eval(args[5]);
    if (!num_is_real(out->count) || !num_is_finite(out->count) || !num_is_integer(out->count) ||
        !num_gt(out->count, NUM_ZERO))
        return false;
    expr_t *divisor = expr_new_const(num_mul_long(NUM_I, paired ? 1 : 2));
    expr_t *raw = expr_div(args[1]->a, divisor), *step = raw ? expr_simplify(raw) : NULL;
    expr_t *slope = step ? expr_create_deriv(step, axis) : NULL;
    expr_t *offset = step ? expr_substitute(step, axis, EXPR_ZERO) : NULL;
    bool ok = slope && offset && !contains_axis(slope, axis) && !contains_axis(offset, axis);
    if (ok) {
        out->slope = expr_eval(slope);
        out->offset = expr_eval(offset);
        out->pole_at_zero = num_eq(a, NUM_ONE);
        ok = num_is_real(out->slope) && num_is_finite(out->slope) && !num_is_zero(out->slope) &&
             num_is_real(out->offset) && num_is_finite(out->offset);
    }
    expr_free(offset);
    expr_free(slope);
    expr_free(step);
    expr_free(raw);
    expr_free(divisor);
    return ok;
}

/* The caller's numerical scope owns evaluated metadata; expression handles remain local. */
static bool affine_progression(const expr_t *expression, const expr_t *axis, bool growth, affine_progression_t *out)
{
    if (!expression || !expr_is_var(axis))
        return false;
    if (!growth && lambert_affine_progression(expression, axis, out))
        return true;
    expr_t *quotient = !growth ? expr_finite_quotient_derivative_closed_form(expression, NULL) : NULL;
    bool quotient_ok = quotient && lambert_affine_progression(quotient, axis, out);
    expr_free(quotient);
    if (quotient_ok)
        return true;
    expr_t *sum = expr_is_op(expression, &ops_summation) ? expr_retain_expr(expression) :
                  expr_finite_progression_from_qdigamma_form(expression);
    const expr_t *upper = NULL;
    const expr_ops_t *function = NULL;
    expr_t *step = NULL, *slope = NULL, *offset = NULL;
    bool ok = expr_finite_progression_parts(sum, &upper, &step, &function);
    ok = ok && (growth ? function == &ops_sinh || function == &ops_cosh || function == &ops_exp :
                        function == &ops_tan || function == &ops_cot || function == &ops_sec || function == &ops_cosec);
    if (ok) {
        const expr_t *lower = sum->b->b->a;
        ok = !contains_axis(lower, axis) && !contains_axis(upper, axis);
    }
    if (ok) {
        out->count = expr_eval(upper);
        ok = num_is_real(out->count) && num_is_finite(out->count) && num_is_integer(out->count) &&
             num_gt(out->count, NUM_ZERO);
    }
    if (ok) {
        slope = expr_create_deriv(step, axis);
        offset = expr_substitute(step, axis, EXPR_ZERO);
        ok = slope && offset && !contains_axis(slope, axis) && !contains_axis(offset, axis);
    }
    if (ok) {
        out->slope = expr_eval(slope);
        out->offset = expr_eval(offset);
        out->pole_at_zero = function == &ops_cot || function == &ops_cosec;
        ok = num_is_real(out->slope) && num_is_finite(out->slope) && !num_is_zero(out->slope) &&
             num_is_real(out->offset) && num_is_finite(out->offset);
    }
    expr_free(offset);
    expr_free(slope);
    expr_free(step);
    expr_free(sum);
    return ok;
}

static bool progression_bounds(const expr_t *expression, const expr_t *axis, bool growth,
                                number_t *lower, number_t *upper)
{
    if (!lower || !upper || lower == upper)
        return false;
    *lower = *upper = NUM_NAN;
    NUM_SCOPE(numbers);
    affine_progression_t progression = {0};
    if (!affine_progression(expression, axis, growth, &progression))
        return false;
    number_t centre = num_neg(num_div(progression.offset, progression.slope));
    number_t phase = growth ? NUM_TWO : num_div(NUM_PI, num_create_from_long(4));
    number_t radius = num_div(phase, num_mul(progression.count, num_abs(progression.slope)));
    number_t lo = num_sub(centre, radius), hi = num_add(centre, radius);
    if (!num_is_finite(lo) || !num_is_finite(hi) || !num_lt(lo, hi))
        return false;
    *lower = num_scope_detach(num_clone(lo));
    *upper = num_scope_detach(num_clone(hi));
    return true;
}

/* Use half the distance to the nearest tangent/secant pole of the largest frequency. */
bool expr_finite_circular_progression_sample_bounds(const expr_t *expression, const expr_t *axis,
                                                    number_t *lower, number_t *upper)
{
    return progression_bounds(expression, axis, false, lower, upper);
}

/* Bound every summand's real exponent by two in either direction. */
bool expr_finite_growth_progression_sample_bounds(const expr_t *expression, const expr_t *axis,
                                                  number_t *lower, number_t *upper)
{
    return progression_bounds(expression, axis, true, lower, upper);
}

/* Affine endpoint phases bound every term throughout the whole interval. */
bool expr_finite_circular_progression_sample_regular(const expr_t *expression, const expr_t *axis,
                                                     number_t lower, number_t upper)
{
    NUM_SCOPE(numbers);
    affine_progression_t progression = {0};
    if (!num_is_real(lower) || !num_is_finite(lower) || !num_is_real(upper) || !num_is_finite(upper) ||
        !num_lt(lower, upper) || !affine_progression(expression, axis, false, &progression))
        return false;
    number_t first = num_add(num_mul(progression.slope, lower), progression.offset);
    number_t last = num_add(num_mul(progression.slope, upper), progression.offset);
    number_t edge = num_gt(num_abs(first), num_abs(last)) ? num_abs(first) : num_abs(last);
    /* Keep a wide phase margin from non-zero poles, including rounded symbolic endpoints. */
    number_t limit = num_div(num_mul_long(NUM_PI, progression.pole_at_zero ? 2 : 1), num_create_from_long(3));
    return num_lt(num_mul(progression.count, edge), limit) &&
           (!progression.pole_at_zero || num_gt(num_mul(first, last), NUM_ZERO));
}
