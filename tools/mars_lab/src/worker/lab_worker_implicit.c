/**
 * @file lab_worker_implicit.c
 * @brief Precision-preserving frames for implicit contour and mesh algorithms.
 *
 * Ordinary views keep the existing world-coordinate double geometry. Narrow views
 * use a local unit box, so edge interpolation cannot collapse distinct large world
 * coordinates. Bounds and emitted vertices remain native numbers. A freshly parsed
 * guarded-precision equation supplies fallback values rather than promoting values
 * which were already rounded by the worksheet or by a double conversion.
 * Ordinary sine/cosine equalities expose independent periodic phase families to
 * the mesh worker, so crossing sheets are not joined through their product residual.
 */
#include <math.h>
#include <stdlib.h>

#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"
#define MARS_LAB_WORKER_INTERNAL_ACCESS
#include "lab_worker_internal.h"

struct _lab_worker_implicit_t {
    equation_t *equation;
    expr_t *residual, *axes[3];
    expr_t *phases[2], *active;
    double level;
    expr_real_sample_t *fast;
    number_t lower[3], span[3];
    size_t count, bits;
    bool local;
};

/* Convert either circular function to a cosine phase without changing its zeros. */
static expr_t *cosine_phase(const expr_t *expression)
{
    const expr_t *argument = NULL;
    bool negative = false;
    while (expr_match_neg_expr(expression, &argument)) {
        negative = !negative;
        expression = argument;
    }
    bool cosine = expr_is_unary_pattern_kind(expression, EXPR_PATTERN_UNARY_COS);
    if (!cosine && !expr_is_unary_pattern_kind(expression, EXPR_PATTERN_UNARY_SIN))
        return NULL;
    NUM_SCOPE(numbers);
    expr_t *half_pi = expr_new_const(num_div(NUM_PI, NUM_TWO));
    expr_t *phase = cosine ? expr_retain_expr(expr_first_child(expression)) :
                            expr_sub(half_pi, expr_first_child(expression));
    expr_free(half_pi);
    if (phase && negative) {
        expr_t *pi = expr_new_const(NUM_PI), *opposite = expr_sub(pi, phase);
        expr_free(pi);
        expr_free(phase);
        phase = opposite;
    }
    return phase;
}

/* Equal circular functions form two independent periodic sheets. Meshing their
 * oscillating difference joins crossing sheets incorrectly on a coarse grid. */
static void periodic_phases(lab_worker_implicit_t *sample)
{
    if (sample->local || sample->count != 3)
        return;
    const expr_t *left = equ_lhs(sample->equation), *right = equ_rhs(sample->equation);
    number_t constant = NUM_NAN;
    bool zero_right = expr_match_const_value(right, &constant) && num_is_zero(constant);
    num_destroy(&constant);
    constant = NUM_NAN;
    bool zero_left = expr_match_const_value(left, &constant) && num_is_zero(constant);
    num_destroy(&constant);
    bool subtract = true;
    if ((zero_right || zero_left) &&
        !expr_match_add_sub_expr(zero_right ? left : right, &left, &right, &subtract))
        return;
    expr_t *a = cosine_phase(left), *b = cosine_phase(right);
    if (a && b && !subtract) {
        expr_t *pi = expr_new_const(NUM_PI), *opposite = expr_sub(pi, b);
        expr_free(pi);
        expr_free(b);
        b = opposite;
    }
    if (a && b) {
        sample->phases[0] = expr_sub(a, b);
        sample->phases[1] = expr_add(a, b);
    }
    expr_free(a);
    expr_free(b);
}

/* Keep parsing, numerical evaluation and coordinate precision in one worker-owned policy. */
lab_worker_implicit_t *lab_worker_implicit_new(const char *source, const char *const *names,
                                              const char *const *limits, size_t count, double *bounds)
{
    if (!count || count > 3)
        return NULL;
    lab_worker_implicit_t *sample = calloc(1, sizeof(*sample));
    if (!sample)
        return NULL;
    sample->count = count;
    size_t saved = num_get_default_prec_bits();
    sample->bits = 128;
    bool ok = true;
    for (size_t axis = 0; ok && axis < count; ++axis) {
        number_t upper = NUM_NAN;
        const char *lo = limits[2 * axis], *hi = limits[2 * axis + 1];
        string_t *omitted_lo = !*lo && *hi ? string_sprintf("(%s)-4", hi) : NULL;
        string_t *omitted_hi = !*hi && *lo ? string_sprintf("(%s)+4", lo) : NULL;
        ok = lab_worker_plot_bounds(*lo ? lo : omitted_lo ? string_c_str(omitted_lo) : "-2",
                                    *hi ? hi : omitted_hi ? string_c_str(omitted_hi) : "2",
                                    &sample->lower[axis], &upper);
        string_free(omitted_hi);
        string_free(omitted_lo);
        number_t minus_two = num_create_from_long(-2);
        if (ok && !*lo && num_gt(sample->lower[axis], minus_two)) {
            num_destroy(&sample->lower[axis]);
            sample->lower[axis] = num_clone(minus_two);
        }
        if (ok && !*hi && num_lt(upper, NUM_TWO)) {
            num_destroy(&upper);
            upper = num_clone(NUM_TWO);
        }
        num_destroy(&minus_two);
        size_t needed = ok ? lab_worker_plot_range_precision(sample->lower[axis], upper) : 0;
        ok = ok && needed && needed <= 32768;
        if (ok) {
            sample->local |= needed > 53;
            if (sample->bits < needed + 64)
                sample->bits = needed + 64;
            size_t guard = lab_worker_plot_check_precision(sample->lower[axis], upper);
            if (sample->bits < guard)
                sample->bits = guard;
            num_set_default_prec_bits(sample->bits);
            sample->span[axis] = num_sub(upper, sample->lower[axis]);
            bounds[2 * axis] = num_to_double(sample->lower[axis]);
            bounds[2 * axis + 1] = num_to_double(upper);
            sample->local |= !isfinite(bounds[2 * axis]) || !isfinite(bounds[2 * axis + 1]) ||
                             !(bounds[2 * axis] < bounds[2 * axis + 1]);
        }
        num_destroy(&upper);
    }
    num_set_default_prec_bits(sample->bits);
    if (ok)
        sample->equation = equ_from_string(source);
    ok = ok && sample->equation;
    const expr_t *inputs[3];
    for (size_t axis = 0; ok && axis < count; ++axis) {
        number_t lower = lab_worker_sample_coordinate(sample->lower[axis], sample->bits);
        number_t span = lab_worker_sample_coordinate(sample->span[axis], sample->bits);
        num_destroy(&sample->lower[axis]);
        num_destroy(&sample->span[axis]);
        sample->lower[axis] = lower;
        sample->span[axis] = span;
        inputs[axis] = sample->axes[axis] = equ_binding(sample->equation, names[axis]);
        ok = sample->axes[axis] != NULL;
        if (sample->local) {
            bounds[2 * axis] = 0;
            bounds[2 * axis + 1] = 1;
        }
    }
    if (ok) {
        sample->residual = expr_sub(equ_lhs(sample->equation), equ_rhs(sample->equation));
        sample->active = sample->residual;
        ok = sample->residual != NULL;
        if (!sample->local)
            sample->fast = expr_real_sample_new(sample->residual, inputs, count);
        periodic_phases(sample);
    }
    num_set_default_prec_bits(saved);
    if (!ok) {
        lab_worker_implicit_free(sample);
        return NULL;
    }
    return sample;
}

/* Rendering uses the same coordinate precision as the sampling frame. */
size_t lab_worker_implicit_precision(const lab_worker_implicit_t *sample)
{
    return sample && sample->local ? sample->bits : 53;
}

/* Local geometry retains precision by subtracting the large origin only in native arithmetic. */
number_t lab_worker_implicit_coordinate(const lab_worker_implicit_t *sample, size_t axis, double value)
{
    if (!sample || !sample->local)
        return num_create_from_double(value);
    size_t saved = num_get_default_prec_bits();
    num_set_default_prec_bits(sample->bits);
    number_t fraction = num_create_from_double(value), offset = num_mul(sample->span[axis], fraction);
    number_t result = num_add(sample->lower[axis], offset);
    num_destroy(&offset);
    num_destroy(&fraction);
    num_set_default_prec_bits(saved);
    return result;
}

/* Guarded double sampling is optional; rejected points are evaluated at guard precision. */
double lab_worker_implicit_eval(lab_worker_implicit_t *sample, const double *point)
{
    if (!sample)
        return NAN;
    double value;
    if (expr_real_sample_eval(sample->fast, point, &value))
        return value - sample->level;
    size_t saved = num_get_default_prec_bits();
    num_set_default_prec_bits(sample->bits);
    for (size_t axis = 0; axis < sample->count; ++axis) {
        number_t coordinate = lab_worker_implicit_coordinate(sample, axis, point[axis]);
        number_t precise = lab_worker_sample_coordinate(coordinate, sample->bits);
        expr_set_val(sample->axes[axis], precise);
        num_destroy(&precise);
        num_destroy(&coordinate);
    }
    number_t result = expr_eval(sample->active);
    value = num_is_real(result) && num_is_finite(result) ? num_to_double(result) : NAN;
    num_destroy(&result);
    num_set_default_prec_bits(saved);
    return value - sample->level;
}

/* Ordinary and high-precision frames retain the original residual sampler. */
unsigned lab_worker_implicit_layers(const lab_worker_implicit_t *sample)
{
    return sample && sample->phases[0] && sample->phases[1] ? 2 : 1;
}

/* The phase family is selected once per sampled grid, not once per vertex. */
double lab_worker_implicit_layer(lab_worker_implicit_t *sample, unsigned layer)
{
    bool periodic = lab_worker_implicit_layers(sample) == 2;
    sample->active = periodic ? sample->phases[layer] : sample->residual;
    sample->level = 0;
    expr_real_sample_free(sample->fast);
    const expr_t *inputs[] = {sample->axes[0], sample->axes[1], sample->axes[2]};
    sample->fast = sample->local ? NULL : expr_real_sample_new(sample->active, inputs, sample->count);
    return periodic ? 2 * acos(-1.0) : 0;
}

/* Shift only the sampled level; authored expressions and bindings remain unchanged. */
void lab_worker_implicit_level(lab_worker_implicit_t *sample, double level)
{
    sample->level = level;
}

/* Release all owning state; no worksheet bindings have been changed. */
void lab_worker_implicit_free(lab_worker_implicit_t *sample)
{
    if (!sample)
        return;
    expr_real_sample_free(sample->fast);
    expr_free(sample->residual);
    expr_free(sample->phases[0]);
    expr_free(sample->phases[1]);
    equ_free(sample->equation);
    for (size_t axis = 0; axis < sample->count; ++axis) {
        num_destroy(&sample->lower[axis]);
        num_destroy(&sample->span[axis]);
    }
    free(sample);
}
