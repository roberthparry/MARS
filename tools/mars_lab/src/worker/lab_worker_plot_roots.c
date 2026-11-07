/**
 * @file lab_worker_plot_roots.c
 * @brief Native complex-plane diagrams of finite numerical equation solutions.
 *
 * Draws already verified roots without solving or sampling the equation again. Real and imaginary coordinates
 * remain independent axes; a minimum unit-wide horizontal frame avoids magnifying numerical round-off around
 * a shared real part. Viewport changes clip points rather than joining them into an invented curve.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "number-t.h"
#include "plot-t.h"
#include "string-t.h"
#define MARS_LAB_WORKER_INTERNAL_ACCESS
#include "lab_worker_internal.h"

/* Render original numerical values; display bounds never move roots onto a conjectured line. */
int lab_worker_plot_roots(const number_t *roots, size_t count, const char *axis, const char *title,
                           const char *TeX, char **argv)
{
    if (!roots || !count || count > 256 || !axis || !argv)
        return 1;
    size_t saved = num_get_default_prec_bits();
    num_set_default_prec_bits(53);
    double limits[4] = {INFINITY, -INFINITY, INFINITY, -INFINITY};
    plot_point_t points[256];
    size_t filled = 0;
    bool ok = true;
    for (size_t i = 0; ok && i < count; ++i) {
        points[i] = (plot_point_t){num_real_part(roots[i]), num_imag_part(roots[i]), false};
        ++filled;
        double x = num_to_double(points[i].x), y = num_to_double(points[i].y);
        ok = isfinite(x) && isfinite(y);
        limits[0] = fmin(limits[0], x);
        limits[1] = fmax(limits[1], x);
        limits[2] = fmin(limits[2], y);
        limits[3] = fmax(limits[3], y);
    }
    double cx = limits[0] * 0.5 + limits[1] * 0.5, cy = limits[2] * 0.5 + limits[3] * 0.5;
    double dx = fmax(0.5, (limits[1] - limits[0]) * 0.55);
    double dy = fmax(0.5, (limits[3] - limits[2]) * 0.55);
    number_t bounds[] = {num_create_from_double(cx - dx), num_create_from_double(cx + dx),
                         num_create_from_double(cy - dy), num_create_from_double(cy + dy)};
    bool xmanual = *argv[5] || *argv[6], ymanual = !strcmp(argv[7], "manual");
    if (ok && xmanual) {
        num_destroy(&bounds[0]);
        num_destroy(&bounds[1]);
        ok = lab_worker_plot_bounds(argv[5], argv[6], &bounds[0], &bounds[1]);
    }
    if (ok && ymanual) {
        num_destroy(&bounds[2]);
        num_destroy(&bounds[3]);
        ok = lab_worker_plot_bounds(argv[8], argv[9], &bounds[2], &bounds[3]);
    }
    string_t *xlabel = string_sprintf("Re(%s)", axis), *ylabel = string_sprintf("Im(%s)", axis);
    plot_options_t options = plot_options_default();
    options.height = 640;
    options.max_series = count;
    options.x_label = xlabel ? string_c_str(xlabel) : "Re";
    options.y_label = ylabel ? string_c_str(ylabel) : "Im";
    options.axis_colour = 0x94b8b1;
    options.grid_colour = 0x82aaa3;
    options.label_colour = 0xb8ccc5;
    options.legend = false;
    plot_t *plot = ok ? plot_new(&options) : NULL;
    ok = ok && plot && xlabel && ylabel;
    for (size_t i = 0; ok && i < count; ++i) {
        string_t *value = num_to_string(roots[i]);
        string_t *label = value ? string_sprintf("%s ≈ %s", axis, string_c_str(value)) : NULL;
        ok = label && plot_add_points(plot, points + i, 1, string_c_str(label), PLOT_POINTS, 0x78d8cb);
        string_free(label);
        string_free(value);
    }
    if (ok)
        ok = plot_set_bounds(plot, bounds[0], bounds[1], bounds[2], bounds[3]);
    string_t *svg = ok ? plot_to_svg(plot) : NULL;
    if (svg && string_replace(svg, "<svg xmlns=", "<svg data-root-plane=\"true\" xmlns=") >= 0) {
        printf("input %s\nexpression %s\nplot_title %s\nplot_title_TeX %s\nplot_svg %s\n"
               "plot_notice Complex solutions: %zu verified finite roots, including any real roots. "
               "Points are not joined; only roots found by this solve are shown.\ndifferentiable no\n",
               argv[1], argv[1], title ? title : "", TeX ? TeX : "", string_c_str(svg), count);
    } else {
        ok = false;
        printf("error Unable to draw the complex solutions; check the real and imaginary axis limits.\n");
    }
    string_free(svg);
    plot_free(plot);
    string_free(xlabel);
    string_free(ylabel);
    for (size_t i = 0; i < filled; ++i) {
        num_destroy(&points[i].x);
        num_destroy(&points[i].y);
    }
    for (size_t i = 0; i < 4; ++i)
        num_destroy(&bounds[i]);
    num_set_default_prec_bits(saved);
    return ok ? 0 : 1;
}
