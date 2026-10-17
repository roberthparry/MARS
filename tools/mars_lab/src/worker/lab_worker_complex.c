/**
 * @file lab_worker_complex.c
 * @brief Shared native sampling for the four complementary complex-function views.
 *
 * One adaptive parametric sample supplies a space curve, an Argand diagram,
 * components and amplitude. Parameter values and coordinates remain number_t
 * values until the plot renderer normalises them. Domain breaks are retained
 * in every projection. Distributions are handled separately, never sampled here.
 * Targeted curve refinement may retain all four views for wholly real subsets;
 * ordinary automatic plotting still delegates real-only output to the real plotter.
 */
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "plot-t.h"
#include "expr_context-t.h"
#define MARS_LAB_WORKER_INTERNAL_ACCESS
#include "lab_worker_internal.h"

typedef struct {
    const expr_t *expression, *variable;
    bool complex;
} complex_sample_t;

static sampler_status_t complex_value(number_t parameter, number_t *real, number_t *imaginary, void *opaque)
{
    complex_sample_t *context = opaque;
    expr_t *coordinate = expr_new_const(parameter);
    expr_t *bound = coordinate ? expr_substitute(context->expression, context->variable, coordinate) : NULL;
    number_t value = bound ? expr_eval(bound) : NUM_NAN;
    if (num_is_finite(value)) {
        *real = num_real_part(value);
        *imaginary = num_imag_part(value);
        context->complex |= !num_is_zero(*imaginary);
    }
    num_destroy(&value);
    expr_free(bound);
    expr_free(coordinate);
    return SAMPLER_VALUE;
}

static plot_t *scene(const char *x, const char *y, const char *z, bool equal)
{
    plot_options_t options = plot_options_default();
    options.width = 560;
    options.height = equal ? 560 : 420;
    options.x_label = x;
    options.y_label = y;
    options.z_label = z;
    options.title = "";
    options.axis_colour = 0x94b8b1;
    options.grid_colour = 0x82aaa3;
    options.label_colour = 0xb8ccc5;
    options.guard_gaps = true;
    options.equal_units = equal;
    options.legend_box = true;
    options.legend_background = 0x10241d;
    return plot_new(&options);
}

/* Auto delegates real-only samples; targeted refinement retains the existing complex representation. */
int lab_worker_complex_plot(const char *source, const expr_t *expression, const expr_t *variable,
                             const char *lower, const char *upper, const char *scale,
                             const char *y_lower, const char *y_upper, const char *const plane_bounds[4], bool preview,
                             bool force_complex)
{
    NUM_SCOPE(numbers);
    /* The input axis is real; prove ordinary real output before doing any extra sampling. */
    expr_context_t *assumptions = expr_context_from_expression(expression, variable);
    bool real = assumptions && expr_context_set(assumptions, variable, EXPR_CONTEXT_FREE, EXPR_CONTEXT_REAL, NUM_NAN) &&
                expr_context_proves(assumptions, expression, EXPR_CONTEXT_REAL);
    expr_context_free(assumptions);
    if (real && !force_complex)
        return -1;
    number_t lo = NUM_NAN, hi = NUM_NAN;
    if (!lab_worker_plot_bounds(*lower ? lower : "-10", *upper ? upper : "10", &lo, &hi))
        return force_complex ? lab_worker_plot_output(source, expression, NULL, errno, NULL, NULL, NULL) : -1;
    complex_sample_t context = {expression, variable, false};
    sampler_options_t options = sampler_options_default();
    options.intervals = preview ? 32 : 64;
    options.max_evaluations = preview ? 2049 : 16385;
    options.max_depth = preview ? 4 : 8;
    size_t saved = num_get_default_prec_bits(), required = lab_worker_plot_range_precision(lo, hi);
    if (required > saved)
        num_set_default_prec_bits(required);
    sampler_t *samples = sampler_new(complex_value, &context, lo, hi, &options);
    if (!samples || (!context.complex && !force_complex)) {
        sampler_free(samples);
        num_set_default_prec_bits(saved);
        return context.complex || force_complex ?
            lab_worker_plot_output(source, expression, NULL, errno, NULL, NULL, NULL) : -1;
    }
    size_t count = sampler_count(samples), segments = 0;
    plot_point_t *points = calloc(count * 4, sizeof(*points));
    number_t *vertices = calloc(count * 6, sizeof(*vertices));
    const char *name = expr_symbol_name(variable);
    plot_t *plots[] = {scene(name, "ℜ", "ℑ", false), scene("ℜ", "ℑ", "", true),
                        scene(name, "", "", false), scene(name, "", "", false)};
    bool ok = points && vertices && plots[0] && plots[1] && plots[2] && plots[3];
    number_t amplitude_max = NUM_ZERO;
    for (size_t i = 0; ok && i < count; ++i) {
        const sampler_point_t *p = sampler_point(samples, i);
        bool gap = p->break_before || !p->valid;
        points[i] = (plot_point_t){p->x, p->y, gap};
        points[count + i] = (plot_point_t){p->parameter, p->valid ? p->x : NUM_NAN, gap};
        points[2 * count + i] = (plot_point_t){p->parameter, p->valid ? p->y : NUM_NAN, gap};
        points[3 * count + i] = (plot_point_t){p->parameter,
            p->valid ? num_hypot(p->x, p->y) : NUM_NAN, gap};
        if (p->valid && num_gt(points[3 * count + i].y, amplitude_max))
            amplitude_max = points[3 * count + i].y;
        if (!i || gap || !sampler_point(samples, i - 1)->valid)
            continue;
        for (unsigned end = 0; end < 2; ++end) {
            const sampler_point_t *at = sampler_point(samples, i - 1 + end);
            vertices[segments] = at->parameter;
            vertices[2 * count + segments] = at->x;
            vertices[4 * count + segments++] = at->y;
        }
    }
    if (ok)
        ok = segments && plot_set_segments(plots[0], vertices, vertices + 2 * count, vertices + 4 * count, segments) &&
             plot_add_points(plots[1], points, count, "", PLOT_LINES, 0x78d8cb) &&
             plot_add_points(plots[2], points + count, count, "Real", PLOT_LINES, 0xd6aa55) &&
             plot_add_points(plots[2], points + 2 * count, count, "Imaginary", PLOT_LINES, 0x78d8cb) &&
             plot_add_points(plots[3], points + 3 * count, count, "", PLOT_LINES, 0x78d8cb);
    if (ok)
        ok = plot_set_bounds(plots[3], lo, hi, NUM_ZERO,
                              num_is_zero(amplitude_max) ? NUM_ONE : num_mul(amplitude_max, num_create_from_double(1.05)));
    if (ok && !strcmp(scale, "manual")) {
        number_t bottom = NUM_NAN, top = NUM_NAN;
        ok = lab_worker_plot_bounds(y_lower, y_upper, &bottom, &top) &&
             plot_set_bounds(plots[2], lo, hi, bottom, top) &&
             plot_set_bounds(plots[3], lo, hi, bottom, top);
        num_destroy(&bottom);
        num_destroy(&top);
    }
    if (ok && plane_bounds && (*plane_bounds[0] || *plane_bounds[1] || *plane_bounds[2] || *plane_bounds[3])) {
        number_t bounds[4] = {NUM_NAN, NUM_NAN, NUM_NAN, NUM_NAN};
        ok = *plane_bounds[0] && *plane_bounds[1] && *plane_bounds[2] && *plane_bounds[3] &&
             lab_worker_plot_bounds(plane_bounds[0], plane_bounds[1], &bounds[0], &bounds[1]) &&
             lab_worker_plot_bounds(plane_bounds[2], plane_bounds[3], &bounds[2], &bounds[3]) &&
             plot_set_bounds(plots[1], bounds[0], bounds[1], bounds[2], bounds[3]);
        for (unsigned i = 0; i < 4; ++i)
            num_destroy(&bounds[i]);
    }
    string_t *images[4] = {0};
    size_t bytes = 0;
    for (unsigned i = 0; ok && i < 4; ++i) {
        images[i] = plot_to_svg(plots[i]);
        ok = lab_worker_plot_fits(images[i]);
        if (ok) {
            bytes += string_byte_length(images[i]);
            if (bytes > 6u * 1024u * 1024u) {
                errno = E2BIG;
                ok = false;
            }
        }
    }
    if (ok)
        ok = lab_worker_plot_quality(images[0], preview, count);
    int result = lab_worker_plot_output(source, expression, ok ? images[0] : NULL, errno, NULL, NULL, NULL);
    if (ok)
        printf("plot_complex_plane %s\nplot_components %s\nplot_amplitude %s\n",
               string_c_str(images[1]), string_c_str(images[2]), string_c_str(images[3]));
    for (unsigned i = 0; i < 4; ++i) {
        string_free(images[i]);
        plot_free(plots[i]);
    }
    free(vertices);
    free(points);
    sampler_free(samples);
    num_set_default_prec_bits(saved);
    return result;
}
