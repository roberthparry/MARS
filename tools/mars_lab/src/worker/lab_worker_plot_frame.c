/**
 * @file lab_worker_plot_frame.c
 * @brief Sampler-guided horizontal framing of representative solution families.
 *
 * Uses one sample per horizontal bin to avoid overweighting adaptive clusters.
 * Winsorised ordinate changes locate the central activity of each curve without
 * letting poles dominate the frame. Located stationary points are retained. This
 * bounded viewing heuristic refines only entirely automatic horizontal bounds;
 * it is not a complete domain or feature search and never changes the solutions.
 */
#include <math.h>
#include <stdlib.h>

#define MARS_LAB_WORKER_INTERNAL_ACCESS
#include "lab_worker_internal.h"

static int compare_double(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

/* Refine a shared automatic frame using bounded, equally weighted per-curve evidence. */
void lab_worker_plot_frame(char *const *sources, size_t count, const char *axis, number_t bounds[4])
{
    double lo = num_to_double(bounds[0]), hi = num_to_double(bounds[1]), span = hi - lo;
    if (!isfinite(lo) || !isfinite(hi) || !isfinite(span) || span <= 0)
        return;
    double activity[128] = {0}, stationary_lo = hi, stationary_hi = lo;
    for (size_t s = 0; s < count; ++s) {
        size_t precision = 53;
        sampler_t *samples = lab_worker_plot_samples(sources[s], axis, bounds[0], bounds[1],
                                                      NUM_NAN, NUM_NAN, &precision);
        if (!samples)
            return; /* Optional framing must not replace the normal sampling diagnostic. */
        const sampler_point_t *bins[129] = {0};
        double distances[129] = {0}, values[129], weights[128] = {0};
        size_t used = 0;
        for (size_t i = 0; i < sampler_count(samples); ++i) {
            const sampler_point_t *p = sampler_point(samples, i);
            double x = num_to_double(p->x), y = num_to_double(p->y);
            if (!p->valid || !isfinite(x) || !isfinite(y))
                continue;
            if (p->stationary) {
                stationary_lo = fmin(stationary_lo, x);
                stationary_hi = fmax(stationary_hi, x);
            }
            double position = (x - lo) / span * 128;
            if (position < 0 || position > 128)
                continue;
            unsigned bin = (unsigned)floor(position + 0.5);
            double distance = fabs(position - bin);
            if (!bins[bin] || distance < distances[bin]) {
                bins[bin] = p;
                distances[bin] = distance;
            }
        }
        for (unsigned i = 0; i < 129; ++i)
            if (bins[i])
                values[used++] = num_to_double(bins[i]->y);
        if (used < 20) {
            sampler_free(samples);
            return;
        }
        qsort(values, used, sizeof *values, compare_double);
        double bottom = values[(used - 1) / 10], top = values[(used - 1) * 9 / 10], total = 0;
        for (unsigned i = 0; i < 128; ++i) {
            if (!bins[i] || !bins[i + 1])
                continue;
            double a = fmax(bottom, fmin(top, num_to_double(bins[i]->y)));
            double b = fmax(bottom, fmin(top, num_to_double(bins[i + 1]->y)));
            weights[i] = fabs(b - a);
            total += weights[i];
        }
        if (isfinite(total) && total > 0)
            for (unsigned i = 0; i < 128; ++i)
                activity[i] += weights[i] / total;
        sampler_free(samples);
    }
    double total = 0, cumulative = 0;
    for (unsigned i = 0; i < 128; ++i)
        total += activity[i];
    if (total <= 0)
        return;
    unsigned first = 0, last = 127;
    for (unsigned i = 0; i < 128; ++i) {
        cumulative += activity[i];
        if (cumulative < total * 0.1)
            first = i + 1;
        if (cumulative < total * 0.9)
            last = i + 1;
    }
    double left = fmin(lo + span * first / 128, stationary_lo);
    double right = fmax(lo + span * (last + 1) / 128, stationary_hi);
    /* Do not crop growth at an edge, or magnify a tiny unresolved feature. */
    if (left < lo + span * 0.1 || right > hi - span * 0.1)
        return;
    double centre = (left + right) / 2, radius = fmax((right - left) * 0.7, span / 8);
    if (!isfinite(centre) || !isfinite(radius) || radius * 2 >= span)
        return;
    number_t lower = num_create_from_double(fmax(lo, centre - radius));
    number_t upper = num_create_from_double(fmin(hi, centre + radius));
    if (num_lt(lower, upper)) {
        num_destroy(&bounds[0]);
        num_destroy(&bounds[1]);
        bounds[0] = num_clone(lower);
        bounds[1] = num_clone(upper);
    }
    num_destroy(&lower);
    num_destroy(&upper);
}
