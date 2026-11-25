/**
 * @file lab_worker_sample.c
 * @brief Shared guarded evaluation policy for private native plotting graphs.
 *
 * Coordinate precision belongs to the sampling caller. At ordinary precision a
 * compiled real plan avoids arbitrary-precision expression evaluation; unsupported
 * operations and uncertain results use native evaluation. Recognised entire-function
 * progressions and proper rational sums use their native closed form, raising working precision according
 * to the denominator guard near cancellation and using the authored sum at actual
 * removable singularities. Real circular quotient identities are recovered once
 * into their finite sum, rather than re-recognised at every numerical sample. Higher-precision passes
 * always use the native evaluator, making reference probes independent of the fast
 * path. Graphs and bindings must remain private to the calling worker.
 */
#include <math.h>
#include <stdlib.h>

#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"
#define MARS_LAB_WORKER_INTERNAL_ACCESS
#include "lab_worker_internal.h"

typedef struct _lab_worker_sample_t {
    expr_t *expression, *reduced, *guard, *axes[3];
    size_t count;
    unsigned guard_power;
    expr_real_sample_t *fast;
} lab_worker_sample_t;

/* The public sampler keeps its usual precision-preserving arithmetic and backend dispatch. */
number_t lab_worker_sample_coordinate(number_t value, size_t bits)
{
    return bits == 53 ? num_create_from_double(num_to_double(value)) : num_as_inexact_real_prec(value, bits);
}

/* Compilation is optional; an unsupported graph remains a usable native evaluator. */
lab_worker_sample_t *lab_worker_sample_new(expr_t *expression, expr_t *const *axes, size_t count, size_t bits)
{
    if (!expression || !axes || !count || count > 3)
        return NULL;
    for (size_t i = 0; i < count; ++i)
        if (!axes[i])
            return NULL;
    lab_worker_sample_t *sample = calloc(1, sizeof(*sample));
    if (!sample)
        return NULL;
    sample->expression = expression;
    sample->count = count;
    const expr_t *inputs[3];
    for (size_t i = 0; i < count; ++i)
        inputs[i] = sample->axes[i] = axes[i];
    sample->reduced = expr_finite_progression_sample_form(expression, inputs, count);
    if (!sample->reduced && count == 1u) {
        number_t lower = NUM_NAN, upper = NUM_NAN;
        if (expr_finite_circular_progression_sample_bounds(expression, inputs[0], &lower, &upper))
            sample->reduced = expr_finite_progression_from_qdigamma_form(expression);
        num_destroy(&upper);
        num_destroy(&lower);
    }
    if (sample->reduced)
        sample->guard = expr_finite_progression_sample_guard(expression, &sample->guard_power);
    if (bits <= 53) {
        sample->fast = expr_real_sample_new(sample->reduced ? sample->reduced : expression, inputs, count);
        /* A special-function identity may be unavailable to the real compiler while its finite body is supported. */
        if (!sample->fast && sample->reduced)
            sample->fast = expr_real_sample_new(expression, inputs, count);
    }
    return sample;
}

/* Never reinterpret a rejected approximation as a missing point. */
number_t lab_worker_sample_eval(lab_worker_sample_t *sample, const number_t *coordinates)
{
    if (!sample)
        return num_clone(NUM_NAN);
    double point[3], value, error;
    if (sample->fast) {
        for (size_t i = 0; i < sample->count; ++i)
            point[i] = num_to_double(coordinates[i]);
        /* This is much tighter than adaptive drawing tolerance; independent native probes still validate it. */
        if (expr_real_sample_estimate(sample->fast, point, &value, &error) &&
            error <= 1e-7 * fmax(1, fabs(value)))
            return num_create_from_double(value);
    }
    for (size_t i = 0; i < sample->count; ++i)
        expr_set_val(sample->axes[i], coordinates[i]);
    bool stable = true;
    size_t precision = num_get_default_prec_bits(), working = precision;
    if (sample->guard) {
        number_t denominator = expr_eval(sample->guard);
        number_t magnitude = num_abs(denominator);
        stable = num_is_finite(denominator) && !num_is_zero(denominator);
        if (stable) {
            /* Native reduction supplies a conservative inverse-phase order for
             * the actual identity, including higher rational derivatives. */
            long exponent = num_get_exponent2(magnitude);
            size_t loss = exponent < 0 && exponent > -4096 ? (size_t)(-exponent) * sample->guard_power : 0u;
            stable = (exponent >= 0 || exponent > -4096) && precision <= 32736u && loss <= 32736u - precision;
            if (stable)
                working += loss + 32u;
        }
        num_destroy(&magnitude);
        num_destroy(&denominator);
    }
    if (sample->reduced && stable) {
        if (working != precision) {
            num_set_default_prec_bits(working);
            for (size_t i = 0; i < sample->count; ++i) {
                number_t coordinate = num_as_inexact_real_prec(coordinates[i], working);
                expr_set_val(sample->axes[i], coordinate);
                num_destroy(&coordinate);
            }
        }
        number_t value = expr_eval(sample->reduced);
        if (working != precision) {
            num_set_default_prec_bits(precision);
            for (size_t i = 0; i < sample->count; ++i)
                expr_set_val(sample->axes[i], coordinates[i]);
        }
        if (num_is_finite(value))
            return value;
        num_destroy(&value);
    }
    return expr_eval(sample->expression);
}

/* The caller owns the graph and its precision lifetime. */
void lab_worker_sample_free(lab_worker_sample_t *sample)
{
    if (sample) {
        expr_real_sample_free(sample->fast);
        expr_free(sample->guard);
        expr_free(sample->reduced);
    }
    free(sample);
}
