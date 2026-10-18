/**
 * @file lab_worker_distribution.c
 * @brief Native complex distribution plots with signed impulse-weight arrows.
 *
 * Separates the regular spectrum from affine Dirac impulses in the expression
 * module. Real and imaginary curves use the normal adaptive native sampler.
 * Infinite impulse sums are labelled as bounded previews. No browser algebra
 * or finite-height approximation of a delta is involved.
 */
#include <errno.h>
#include <string.h>

#include "plot-t.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"
#define MARS_SHARED_EXPR_DISTRIBUTION_INTERNAL_ACCESS
#include "internal/expr_distribution_internal.h"
#define MARS_LAB_WORKER_INTERNAL_ACCESS
#include "lab_worker_internal.h"

typedef struct {
    plot_point_t real[256], imaginary[256];
    size_t count;
} impulse_points_t;

typedef struct {
    const expr_t *regular, *variable;
    bool imaginary;
} regular_sample_t;

static sampler_status_t regular_value(number_t parameter, number_t *x, number_t *y, void *opaque)
{
    regular_sample_t *sample = opaque;
    expr_t *coordinate = expr_new_const(parameter);
    expr_t *bound = expr_substitute(sample->regular, sample->variable, coordinate);
    number_t value = bound ? expr_eval(bound) : NUM_NAN;
    *x = num_clone(parameter);
    *y = num_is_finite(value) ? (sample->imaginary ? num_imag_part(value) : num_real_part(value)) : num_clone(NUM_NAN);
    num_destroy(&value);
    expr_free(bound);
    expr_free(coordinate);
    return SAMPLER_VALUE;
}

static bool impulse(number_t coordinate, number_t weight, void *opaque)
{
    impulse_points_t *points = opaque;
    if (points->count == 256)
        return false;
    size_t i = points->count++;
    points->real[i] = (plot_point_t){num_clone(coordinate), num_real_part(weight), true};
    points->imaginary[i] = (plot_point_t){num_clone(coordinate), num_imag_part(weight), true};
    return true;
}

/* Use the same sampler and plot renderer as ordinary curves, with symbolic impulse metadata. */
int lab_worker_distribution_plot(const char *source, const expr_t *expression, const expr_t *variable,
                                 const char *lower, const char *upper, const char *scale,
                                 const char *y_lower, const char *y_upper, bool preview)
{
    impulse_points_t points = {0};
    bool truncated = false;
    expr_t *simplified = expr_simplify(expression);
    bool distribution = expr_is_distribution(simplified);
    expr_t *regular = expr_distribution_decompose(simplified, variable, impulse, &points, &truncated);
    if (regular) {
        expr_t *compact = expr_simplify(regular);
        expr_free(regular);
        regular = compact;
    }
    bool ok = regular != NULL;
    number_t lo = NUM_NAN, hi = NUM_NAN, ymin = NUM_NAN, ymax = NUM_NAN;
    bool manual = !strcmp(scale, "manual");
    if (ok)
        ok = lab_worker_plot_bounds(*lower ? lower : "-10", *upper ? upper : "10", &lo, &hi) &&
             (!manual || lab_worker_plot_bounds(y_lower, y_upper, &ymin, &ymax));
    plot_options_t options = plot_options_default();
    options.axis_colour = 0x94b8b1;
    options.grid_colour = 0x82aaa3;
    options.label_colour = 0xb8ccc5;
    options.smart_y = !strcmp(scale, "auto");
    options.guard_gaps = true;
    options.x_label = expr_symbol_name(variable);
    options.y_label = "Value / weight";
    options.title = "";
    plot_t *plot = ok ? plot_new(&options) : NULL;
    ok = ok && plot;
    const char *names[] = {"Real curve", "Imaginary curve"};
    const uint32_t colours[] = {0xd6aa55, 0x78d8cb};
    for (size_t i = 0; ok && i < 2; ++i) {
        regular_sample_t context = {regular, variable, i != 0};
        sampler_options_t sampling = sampler_options_default();
        sampling.intervals = preview ? 16 : 64;
        sampling.max_evaluations = preview ? 513 : 8193;
        sampling.max_depth = preview ? 4 : 8;
        size_t saved_precision = num_get_default_prec_bits();
        size_t required = lab_worker_plot_range_precision(lo, hi);
        if (required > saved_precision)
            num_set_default_prec_bits(required);
        sampler_t *samples = sampler_new(regular_value, &context, lo, hi, &sampling);
        num_set_default_prec_bits(saved_precision);
        bool nonzero = false;
        for (size_t j = 0; samples && j < sampler_count(samples); ++j) {
            const sampler_point_t *point = sampler_point(samples, j);
            nonzero |= point->valid && !num_is_zero(point->y);
        }
        ok = samples && (!nonzero || plot_add_sampler(plot, samples, names[i], PLOT_LINES, colours[i]));
        sampler_free(samples);
    }
    for (size_t i = 0; i < points.count; ++i) {
        if (num_lt(points.real[i].x, lo) || num_gt(points.real[i].x, hi)) {
            num_destroy(&points.real[i].y);
            num_destroy(&points.imaginary[i].y);
            points.real[i].y = NUM_NAN;
            points.imaginary[i].y = NUM_NAN;
        }
    }
    plot_point_t *impulses[] = {points.real, points.imaginary};
    const char *impulse_names[] = {"Real impulse weight", "Imaginary impulse weight"};
    for (size_t i = 0; ok && i < 2; ++i) {
        bool nonzero = false;
        for (size_t j = 0; j < points.count; ++j)
            nonzero |= num_is_finite(impulses[i][j].y) && !num_is_zero(impulses[i][j].y);
        if (nonzero)
            ok = plot_add_points(plot, impulses[i], points.count, impulse_names[i], PLOT_IMPULSES, colours[i]);
    }
    plot_point_t baseline[] = {{lo, NUM_ZERO, true}, {hi, NUM_ZERO, false}};
    if (ok)
        ok = plot_add_points(plot, baseline, 2, "", PLOT_LINES, 0x94b8b1);
    if (ok && manual)
        ok = plot_set_bounds(plot, lo, hi, ymin, ymax);
    string_t *svg = ok ? plot_to_svg(plot) : NULL;
    int result = -1;
    if (svg) {
        const char *title = truncated ? "Fourier spectrum (32-term preview)" : "Fourier spectrum";
        printf("plot_title %s\nplot_title_TeX \\text{%s}\nplot_svg %s\ndifferentiable yes\n",
               title, title, string_c_str(svg));
        printf("plot_notice Curves show component values away from singularities; arrows show Dirac impulse weights, "
               "not point values. The full distribution has no pointwise 3-D, complex-plane or amplitude view.\n");
        result = 0;
    } else if (regular)
        result = lab_worker_plot_output(source, expression, NULL, EDOM, NULL, NULL, NULL);
    else if (distribution) {
        printf("error       This distribution cannot be plotted as ordinary pointwise values or amplitude.\n");
        result = 1;
    }
    string_free(svg);
    plot_free(plot);
    num_destroy(&ymax); num_destroy(&ymin); num_destroy(&hi); num_destroy(&lo);
    expr_free(regular); expr_free(simplified);
    for (size_t i = 0; i < points.count; ++i) {
        num_destroy(&points.real[i].x); num_destroy(&points.real[i].y);
        num_destroy(&points.imaginary[i].x); num_destroy(&points.imaginary[i].y);
    }
    return result;
}
