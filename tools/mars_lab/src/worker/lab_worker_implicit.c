/**
 * @file lab_worker_implicit.c
 * @brief Precision-preserving frames for implicit contour and mesh algorithms.
 *
 * Ordinary views keep the existing world-coordinate double geometry. Narrow views
 * use a local unit box, so edge interpolation cannot collapse distinct large world
 * coordinates. Bounds and emitted vertices remain native numbers. A freshly parsed
 * guarded-precision equation supplies fallback values rather than promoting values
 * which were already rounded by the worksheet or by a double conversion.
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
    expr_real_sample_t *fast;
    number_t lower[3], span[3];
    size_t count, bits;
    bool local;
};

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
        ok = sample->residual != NULL;
        if (!sample->local)
            sample->fast = expr_real_sample_new(sample->residual, inputs, count);
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
        return value;
    size_t saved = num_get_default_prec_bits();
    num_set_default_prec_bits(sample->bits);
    for (size_t axis = 0; axis < sample->count; ++axis) {
        number_t coordinate = lab_worker_implicit_coordinate(sample, axis, point[axis]);
        number_t precise = lab_worker_sample_coordinate(coordinate, sample->bits);
        expr_set_val(sample->axes[axis], precise);
        num_destroy(&precise);
        num_destroy(&coordinate);
    }
    number_t result = expr_eval(sample->residual);
    value = num_is_real(result) && num_is_finite(result) ? num_to_double(result) : NAN;
    num_destroy(&result);
    num_set_default_prec_bits(saved);
    return value;
}

/* Release all owning state; no worksheet bindings have been changed. */
void lab_worker_implicit_free(lab_worker_implicit_t *sample)
{
    if (!sample)
        return;
    expr_real_sample_free(sample->fast);
    expr_free(sample->residual);
    equ_free(sample->equation);
    for (size_t axis = 0; axis < sample->count; ++axis) {
        num_destroy(&sample->lower[axis]);
        num_destroy(&sample->span[axis]);
    }
    free(sample);
}
