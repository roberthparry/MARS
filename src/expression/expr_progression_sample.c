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

/* The caller's numerical scope owns evaluated metadata; expression handles remain local. */
static bool affine_progression(const expr_t *expression, const expr_t *axis, bool growth, affine_progression_t *out)
{
    if (!expression || !expr_is_var(axis))
        return false;
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
