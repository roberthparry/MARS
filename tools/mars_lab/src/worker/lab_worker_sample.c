/**
 * @file lab_worker_sample.c
 * @brief Shared guarded evaluation policy for private native plotting graphs.
 *
 * Coordinate precision belongs to the sampling caller. At ordinary precision a
 * compiled real plan avoids arbitrary-precision expression evaluation; unsupported
 * operations and uncertain results use the original graph. Higher-precision passes
 * always use the native evaluator, making reference probes independent of the fast
 * path. Graphs and bindings must remain private to the calling worker.
 */
#include <stdlib.h>

#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"
#define MARS_LAB_WORKER_INTERNAL_ACCESS
#include "lab_worker_internal.h"

typedef struct _lab_worker_sample_t {
    expr_t *expression, *axes[3];
    size_t count;
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
    if (bits <= 53)
        sample->fast = expr_real_sample_new(expression, inputs, count);
    return sample;
}

/* Never reinterpret a rejected approximation as a missing point. */
number_t lab_worker_sample_eval(lab_worker_sample_t *sample, const number_t *coordinates)
{
    if (!sample)
        return num_clone(NUM_NAN);
    double point[3], value;
    if (sample->fast) {
        for (size_t i = 0; i < sample->count; ++i)
            point[i] = num_to_double(coordinates[i]);
        if (expr_real_sample_eval(sample->fast, point, &value))
            return num_create_from_double(value);
    }
    for (size_t i = 0; i < sample->count; ++i)
        expr_set_val(sample->axes[i], coordinates[i]);
    return expr_eval(sample->expression);
}

/* The caller owns the graph and its precision lifetime. */
void lab_worker_sample_free(lab_worker_sample_t *sample)
{
    if (sample)
        expr_real_sample_free(sample->fast);
    free(sample);
}
