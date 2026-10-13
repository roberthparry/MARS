/**
 * @file plot_range.c
 * @brief Robust vertical viewports without weighting adaptive clusters as extra evidence.
 *
 * One representative per horizontal bin and series supplies a bounded percentile
 * estimate. The same tail policy serves rectangular height grids. Only unusually
 * extended tails are cropped, without a visible annotation. This is a viewing
 * heuristic, never a claim about the function's range.
 */
#include <math.h>
#include <stdlib.h>

#define MARS_SHARED_COMPUTATION_INTERNAL_ACCESS
#include "internal/computation_internal.h"
#define MARS_PLOT_INTERNAL_ACCESS
#include "plot_internal.h"

static int compare_value(const void *left, const void *right)
{
    number_t a = *(const number_t *)left, b = *(const number_t *)right;
    return num_lt(a, b) ? -1 : num_gt(a, b) ? 1 : 0;
}

void plot_trim_range(number_t *values, size_t count, number_t bounds[2])
{
    if (count < 20)
        return;
    qsort(values, count, sizeof(*values), compare_value);
    number_t lo = values[(count - 1) / 20], hi = values[((count - 1) * 19) / 20];
    number_t core = num_sub(hi, lo), full = num_sub(bounds[1], bounds[0]);
    number_t four = num_create_from_long(4);
    if (num_is_zero(core)) {
        number_t magnitude = num_abs(lo), twenty = num_create_from_long(20);
        num_destroy(&core);
        core = num_div(magnitude, twenty);
        num_destroy(&magnitude);
        num_destroy(&twenty);
        if (num_lt(core, NUM_ONE)) {
            num_destroy(&core);
            core = num_clone(NUM_ONE);
        }
    }
    number_t threshold = num_mul(core, four);
    if (num_gt(core, NUM_ZERO) && num_gt(full, threshold)) {
        number_t padding = num_div(core, four), lower = num_sub(lo, padding), upper = num_add(hi, padding);
        if (num_gt(lower, bounds[0])) {
            num_destroy(&bounds[0]);
            bounds[0] = num_clone(lower);
        }
        if (num_lt(upper, bounds[1])) {
            num_destroy(&bounds[1]);
            bounds[1] = num_clone(upper);
        }
        num_destroy(&padding);
        num_destroy(&lower);
        num_destroy(&upper);
    }
    num_destroy(&threshold);
    num_destroy(&core);
    num_destroy(&full);
    num_destroy(&four);
}

bool plot_smart_range(const plot_t *plot, number_t bounds[4])
{
    number_t span = num_sub(bounds[1], bounds[0]);
    if (!num_gt(span, NUM_ZERO)) {
        num_destroy(&span);
        return true;
    }
    number_t *values = calloc(plot->count, 129 * sizeof(*values));
    size_t count = 0;
    bool ok = values != NULL;
    for (size_t s = 0; ok && s < plot->count; ++s) {
        const plot_point_t *bins[129] = {0};
        double distances[129] = {0};
        const plot_series_t *series = &plot->series[s];
        for (size_t i = 0; ok && i < series->count; ++i) {
            ok = computation_step_current("plot.range");
            const plot_point_t *p = &series->points[i];
            if (!num_is_real(p->x) || !num_is_finite(p->x) || !num_is_real(p->y) || !num_is_finite(p->y))
                continue;
            number_t delta = num_sub(p->x, bounds[0]), ratio = num_div(delta, span);
            double position = num_to_double(ratio) * 128;
            num_destroy(&ratio);
            num_destroy(&delta);
            if (!isfinite(position) || position < 0 || position > 128)
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
                values[count++] = bins[i]->y;
    }
    num_destroy(&span);
    if (ok)
        plot_trim_range(values, count, bounds + 2);
    free(values);
    return ok;
}
