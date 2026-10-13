/**
 * @file sampler.c
 * @brief Precision-preserving sample ownership, uniform traversal and bounded callback execution.
 *
 * Constructors publish only complete immutable datasets. Callback outputs are
 * always released, including failures; each evaluation is a cooperative work
 * boundary. Linear traversal is required to emit the ordered sample sequence.
 */
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>

#define MARS_SHARED_COMPUTATION_INTERNAL_ACCESS
#include "internal/computation_internal.h"
#define MARS_SAMPLER_INTERNAL_ACCESS
#include "sampler_internal.h"

/* Supply conservative defaults without hidden process state. */
sampler_options_t sampler_options_default(void)
{
    return (sampler_options_t){SAMPLER_ADAPTIVE, 32, 65537, 12, 1e-6, 1e-3};
}

void sampler_point_clear(sampler_point_t *point)
{
    num_destroy(&point->parameter);
    num_destroy(&point->x);
    num_destroy(&point->y);
}

/* Release completed or partly constructed datasets without disturbing an error. */
void sampler_free(sampler_t *sampler)
{
    int saved = errno;
    if (sampler) {
        for (size_t i = 0; i < sampler->count; ++i)
            sampler_point_clear(&sampler->points[i]);
        free(sampler->points);
        num_destroy(&sampler->absolute);
        num_destroy(&sampler->relative);
        free(sampler);
    }
    errno = saved;
}

bool sampler_evaluate(sampler_t *sampler, number_t parameter, sampler_point_t *point)
{
    *point = (sampler_point_t){.parameter = num_clone(parameter), .x = NUM_NAN, .y = NUM_NAN};
    if (!num_is_finite(point->parameter)) {
        errno = ERANGE;
        return false;
    }
    if (sampler->evaluations >= sampler->options.max_evaluations) {
        errno = E2BIG;
        return false;
    }
    if (!computation_step_current("sampler.evaluate"))
        return false;
    ++sampler->evaluations;
    errno = 0;
    sampler_status_t status = sampler->callback(parameter, &point->x, &point->y, sampler->argument);
    int saved = errno;
    if (!computation_poll_current())
        return false;
    if (status != SAMPLER_VALUE && status != SAMPLER_GAP) {
        errno = status == SAMPLER_ERROR ? (saved ? saved : EIO) : EINVAL;
        return false;
    }
    point->valid = status == SAMPLER_VALUE && num_is_real(point->x) && num_is_finite(point->x) &&
                   num_is_real(point->y) && num_is_finite(point->y);
    return true;
}

bool sampler_append(sampler_t *sampler, const sampler_point_t *point, bool disconnect)
{
    if (sampler->count == sampler->capacity) {
        size_t capacity = sampler->capacity ? sampler->capacity * 2 : 64;
        if (capacity > sampler->options.max_evaluations)
            capacity = sampler->options.max_evaluations;
        if (capacity <= sampler->count || capacity > SIZE_MAX / sizeof(*sampler->points)) {
            errno = EOVERFLOW;
            return false;
        }
        sampler_point_t *points = realloc(sampler->points, capacity * sizeof(*points));
        if (!points)
            return false;
        sampler->points = points;
        sampler->capacity = capacity;
    }
    sampler_point_t copy = *point;
    copy.parameter = num_clone(point->parameter);
    copy.x = num_clone(point->x);
    copy.y = num_clone(point->y);
    if (!num_is_finite(copy.parameter) || (point->valid && (!num_is_finite(copy.x) || !num_is_finite(copy.y)))) {
        sampler_point_clear(&copy);
        errno = ENOMEM;
        return false;
    }
    copy.break_before = disconnect || !copy.valid || !sampler->count || !sampler->points[sampler->count - 1].valid;
    sampler->points[sampler->count++] = copy;
    return true;
}

/* Sample synchronously with exact interval fractions and no double coordinate conversion. */
sampler_t *sampler_new(sampler_callback_t callback, void *argument, number_t lower, number_t upper,
                       const sampler_options_t *options)
{
    sampler_options_t settings = options ? *options : sampler_options_default();
    if (!callback || !num_is_real(lower) || !num_is_finite(lower) || !num_is_real(upper) || !num_is_finite(upper) ||
        !num_lt(lower, upper) || settings.method < SAMPLER_UNIFORM || settings.method > SAMPLER_ADAPTIVE ||
        !settings.intervals || settings.intervals >= LONG_MAX || settings.max_evaluations <= settings.intervals ||
        settings.max_evaluations > SIZE_MAX / sizeof(sampler_point_t) || settings.max_depth > 24 ||
        !isfinite(settings.absolute_tolerance) || settings.absolute_tolerance < 0 ||
        !isfinite(settings.relative_tolerance) || settings.relative_tolerance < 0 ||
        (settings.method == SAMPLER_ADAPTIVE && settings.absolute_tolerance == 0 && settings.relative_tolerance == 0)) {
        errno = EINVAL;
        return NULL;
    }
    sampler_t *sampler = calloc(1, sizeof(*sampler));
    if (!sampler)
        return NULL;
    sampler->options = settings;
    sampler->callback = callback;
    sampler->argument = argument;
    sampler->absolute = num_create_from_double(settings.absolute_tolerance);
    sampler->relative = num_create_from_double(settings.relative_tolerance);
    number_t span = num_sub(upper, lower);
    sampler_point_t left;
    bool ok = sampler_evaluate(sampler, lower, &left) && sampler_append(sampler, &left, true);
    for (size_t i = 1; ok && i <= settings.intervals; ++i) {
        number_t fraction = num_create_from_frac((long)i, (long)settings.intervals);
        number_t offset = num_mul(span, fraction);
        number_t parameter = i == settings.intervals ? num_clone(upper) : num_add(lower, offset);
        sampler_point_t right;
        ok = sampler_evaluate(sampler, parameter, &right);
        if (ok && !num_gt(right.parameter, left.parameter)) {
            errno = ERANGE;
            ok = false;
        }
        if (ok)
            ok = settings.method == SAMPLER_UNIFORM ? sampler_append(sampler, &right, false) :
                                                     sampler_refine(sampler, &left, &right, 0);
        sampler_point_clear(&left);
        left = right;
        num_destroy(&parameter);
        num_destroy(&offset);
        num_destroy(&fraction);
    }
    sampler_point_clear(&left);
    num_destroy(&span);
    if (!ok || !computation_poll_current()) {
        sampler_free(sampler);
        return NULL;
    }
    sampler->callback = NULL;
    sampler->argument = NULL;
    return sampler;
}

/* Inspect completed data without traversing points. */
size_t sampler_count(const sampler_t *sampler)
{
    if (!sampler)
        errno = EINVAL;
    return sampler ? sampler->count : 0;
}

/* Count every probe, not only accepted endpoints. */
size_t sampler_evaluations(const sampler_t *sampler)
{
    if (!sampler)
        errno = EINVAL;
    return sampler ? sampler->evaluations : 0;
}

/* Direct indexing preserves sample order and explicit edge flags. */
const sampler_point_t *sampler_point(const sampler_t *sampler, size_t index)
{
    if (!sampler || index >= sampler->count) {
        errno = sampler ? ERANGE : EINVAL;
        return NULL;
    }
    return &sampler->points[index];
}
