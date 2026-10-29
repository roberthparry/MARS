/**
 * @file lab_worker_plot_equation_frame.c
 * @brief Feature-led automatic viewports for equation-side plots.
 *
 * Native adaptive samples of both sides and their residual locate turning points
 * and candidate intersections. Only connected finite samples bracket crossings;
 * poles and gaps are not treated as roots. The resulting padded viewing frame
 * preserves explicit endpoints and manual height limits in the caller. This is
 * a bounded search inside the initial domain, not proof that all roots were found.
 */
#include <math.h>
#include <stdlib.h>

#define MARS_LAB_WORKER_INTERNAL_ACCESS
#include "lab_worker_internal.h"

typedef struct {
    expr_t *sides[2];
    expr_t *axes[2];
    double xmin, xmax, ymin, ymax, root_min, root_max;
    bool found, roots, turning;
} equation_frame_t;

static void feature(equation_frame_t *frame, double x, bool root)
{
    double y[2];
    for (unsigned i = 0; i < 2; ++i) {
        number_t at = num_create_from_double(x);
        if (frame->axes[i])
            expr_set_val(frame->axes[i], at);
        num_destroy(&at);
        number_t value = expr_eval(frame->sides[i]);
        y[i] = num_to_double(value);
        bool finite = num_is_real(value) && num_is_finite(value) && isfinite(y[i]);
        num_destroy(&value);
        if (!finite)
            return;
    }
    frame->xmin = fmin(frame->xmin, x);
    frame->xmax = fmax(frame->xmax, x);
    frame->ymin = fmin(frame->ymin, fmin(y[0], y[1]));
    frame->ymax = fmax(frame->ymax, fmax(y[0], y[1]));
    frame->found = true;
    if (root) {
        double centre = y[0] / 2 + y[1] / 2;
        frame->root_min = fmin(frame->root_min, centre);
        frame->root_max = fmax(frame->root_max, centre);
        frame->roots = true;
    }
}

static void collect(equation_frame_t *frame, const sampler_t *samples, bool residual)
{
    /* Each bounded adaptive sample set is visited once; there is no feature-pair search. */
    const sampler_point_t *previous = NULL;
    for (size_t i = 0; i < sampler_count(samples); ++i) {
        const sampler_point_t *p = sampler_point(samples, i);
        if (!p->valid) {
            previous = NULL;
            continue;
        }
        double x = num_to_double(p->x), y = num_to_double(p->y);
        if (!isfinite(x) || !isfinite(y)) {
            previous = NULL;
            continue;
        }
        bool connected = previous && !p->break_before;
        bool turning = p->stationary || (connected && num_is_zero(p->dy) &&
                       num_is_real(previous->dy) && num_is_finite(previous->dy) && !num_is_zero(previous->dy));
        frame->turning |= turning;
        if (turning || (residual && !y))
            feature(frame, x, residual && !y);
        if (residual && connected) {
            double a = num_to_double(previous->y), before = num_to_double(previous->x);
            if (isfinite(a) && ((a < 0 && y > 0) || (a > 0 && y < 0))) {
                double scale = fmax(fabs(a), fabs(y));
                double fraction = (a / scale) / (a / scale - y / scale);
                feature(frame, before + (x - before) * fraction, true);
            }
        }
        previous = p;
    }
}

static bool viewport(equation_frame_t *frame, double lo, double hi, bool horizontal, number_t bounds[4])
{
    if (!frame->found)
        return false;
    double padding = fmax((frame->xmax - frame->xmin) * 0.15, (hi - lo) * 0.05);
    double xmin = horizontal ? fmax(lo, frame->xmin - padding) : lo;
    double xmax = horizontal ? fmin(hi, frame->xmax + padding) : hi;
    /* A monotonic crossing alone has almost no vertical spread. Include both sides at the visible edges,
     * while retaining the feature-led clipping used when actual turning points are present. */
    if (frame->roots && !frame->turning) {
        feature(frame, xmin, false);
        feature(frame, xmax, false);
    }
    double centre = frame->roots ? frame->root_min / 2 + frame->root_max / 2 : frame->ymin / 2 + frame->ymax / 2;
    double radius = fmax(centre - frame->ymin, frame->ymax - centre) * 1.1;
    if (!isfinite(centre) || !isfinite(radius))
        return false;
    if (radius <= 0)
        radius = fmax(1, fabs(centre) * 0.05);
    double values[] = {xmin, xmax, centre - radius, centre + radius};
    if (!isfinite(values[2]) || !isfinite(values[3]) || values[2] >= values[3])
        return false;
    for (unsigned i = 0; i < 4; ++i) {
        num_destroy(&bounds[i]);
        bounds[i] = num_create_from_double(values[i]);
    }
    return true;
}

/* Derive a padded feature viewport without altering either mathematical side. */
bool lab_worker_plot_equation_frame(const expr_t *const *sides, char *const *sources, const char *axis,
                                    bool horizontal, number_t bounds[4])
{
    double lo = num_to_double(bounds[0]), hi = num_to_double(bounds[1]);
    if (!isfinite(lo) || !isfinite(hi) || !isfinite(hi - lo) || hi <= lo)
        return false;
    equation_frame_t frame = {.xmin = INFINITY, .xmax = -INFINITY, .ymin = INFINITY, .ymax = -INFINITY,
                              .root_min = INFINITY, .root_max = -INFINITY};
    expr_bindings_t *bindings[2] = {NULL, NULL};
    expr_t *difference = expr_sub(sides[0], sides[1]);
    char *residual = difference ? expr_to_string(difference, style_EXPRESSION) : NULL;
    bool ok = residual != NULL;
    for (unsigned i = 0; ok && i < 2; ++i) {
        frame.sides[i] = expr_from_string(sources[i], &bindings[i]);
        frame.axes[i] = bindings[i] ? expr_bindings_get(bindings[i], axis) : NULL;
        ok = frame.sides[i] != NULL;
    }
    for (unsigned i = 0; ok && i < 3; ++i) {
        size_t precision = 53;
        sampler_t *samples = lab_worker_plot_samples(i == 2 ? residual : sources[i], axis,
                                                      bounds[0], bounds[1], NUM_NAN, NUM_NAN, &precision);
        ok = samples != NULL;
        if (ok)
            collect(&frame, samples, i == 2);
        sampler_free(samples);
    }
    bool framed = ok && viewport(&frame, lo, hi, horizontal, bounds);
    for (unsigned i = 0; i < 2; ++i) {
        expr_free(frame.sides[i]);
        expr_bindings_free(bindings[i]);
    }
    expr_free(difference);
    free(residual);
    return framed;
}
