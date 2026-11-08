/**
 * @file lab_worker_plot_roots.c
 * @brief Native complex-plane diagrams of finite numerical equation solutions.
 *
 * Draws retained roots and extends supported zero searches outside their height range at display precision.
 * Real and imaginary coordinates remain independent axes; a minimum unit-wide horizontal frame avoids round-off around
 * a shared real part. Viewport changes clip points rather than joining them into an invented curve.
 */
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "number-t.h"
#include "equation-t.h"
#include "plot-t.h"
#include "string-t.h"
#define MARS_LAB_WORKER_INTERNAL_ACCESS
#include "lab_worker_internal.h"

/* Keep the original roots untouched; a viewport request searches only newly exposed upper and lower bands. */
static bool extend_roots(const equation_t *equation, const double original[4], const number_t bounds[4],
                         bool preview, size_t capacity, number_t *extra, size_t *count, bool *limited)
{
    *count = 0;
    *limited = false;
    if (num_to_double(bounds[1]) <= 0 || num_to_double(bounds[0]) >= 1)
        return true;
    double low = num_to_double(bounds[2]), high = num_to_double(bounds[3]);
    double ranges[][2] = {{fmax(low, nextafter(original[3], INFINITY)), high},
                          {low, fmin(high, nextafter(original[2], -INFINITY))}};
    bool ok = true;
    size_t saved = num_get_default_prec_bits();
    num_set_default_prec_digits(preview ? 16 : 31);
    for (size_t side = 0; ok && side < 2; ++side) {
        if (ranges[side][0] >= ranges[side][1])
            continue;
        *limited |= ranges[side][0] < -10000 || ranges[side][1] > 10000;
        double lower = fmax(-10000, ranges[side][0]), upper = fmin(10000, ranges[side][1]);
        if (lower >= upper)
            continue;
        size_t available = capacity - *count;
        if (!available) {
            *limited = true;
            break;
        }
        if (available > 96)
            available = 96;
        number_t lo = num_create_from_double(lower), hi = num_create_from_double(upper);
        bool capped = false;
        errno = 0;
        equation_solutions_t *solutions = equ_search_zeta_zeros(equation, lo, hi, available, &capped);
        ok = solutions || errno == ENOTSUP;
        *limited |= capped;
        for (size_t i = 0; solutions && i < equ_solutions_count(solutions); ++i)
            extra[(*count)++] = expr_eval(equ_rhs(equ_solutions_at(solutions, i)));
        equ_solutions_free(solutions);
        num_destroy(&lo);
        num_destroy(&hi);
    }
    num_set_default_prec_bits(saved);
    return ok;
}

/* Tooltips describe plot coordinates, with an ellipsis wherever the numerical spelling is abbreviated. */
static string_t *root_label(const char *axis, number_t root)
{
    size_t bits = num_get_prec_bits(root);
    unsigned digits = bits && bits < 100 ? 15 : 30;
    string_t *value = num_sprintf_text("%.*n", (int)digits, root);
    if (!value)
        return NULL;
    const char *text = string_c_str(value);
    if (strpbrk(text, ".eE")) {
        size_t length = string_length(value);
        size_t at = length && text[string_byte_length(value) - 1] == 'i' ? length - 1 : length;
        if (string_insert(value, at, "…") != 0) {
            string_free(value);
            return NULL;
        }
    }
    string_t *label = string_sprintf("%s ≈ %s", axis, string_c_str(value));
    string_free(value);
    return label;
}

/* Render original numerical values; display bounds never move roots onto a conjectured line. */
int lab_worker_plot_roots(const number_t *roots, size_t count, const equation_t *equation,
                         const char *axis, const char *title, const char *TeX, char **argv)
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
    number_t extra[256];
    size_t extra_count = 0;
    bool limited = false;
    if (ok && ymanual)
        ok = extend_roots(equation, limits, bounds, !strcmp(argv[17], "preview"), 256 - count,
                            extra, &extra_count, &limited);
    for (size_t i = 0; ok && i < extra_count; ++i) {
        points[count + i] = (plot_point_t){num_real_part(extra[i]), num_imag_part(extra[i]), false};
        ++filled;
        ok = num_is_finite(extra[i]);
    }
    string_t *xlabel = string_sprintf("Re(%s)", axis), *ylabel = string_sprintf("Im(%s)", axis);
    plot_options_t options = plot_options_default();
    options.height = 640;
    options.max_series = count + extra_count;
    options.x_label = xlabel ? string_c_str(xlabel) : "Re";
    options.y_label = ylabel ? string_c_str(ylabel) : "Im";
    options.axis_colour = 0x94b8b1;
    options.grid_colour = 0x82aaa3;
    options.label_colour = 0xb8ccc5;
    options.legend = false;
    plot_t *plot = ok ? plot_new(&options) : NULL;
    ok = ok && plot && xlabel && ylabel;
    for (size_t i = 0; ok && i < count + extra_count; ++i) {
        number_t root = i < count ? roots[i] : extra[i - count];
        string_t *label = root_label(axis, root);
        ok = label && plot_add_points(plot, points + i, 1, string_c_str(label), PLOT_POINTS, 0x78d8cb);
        string_free(label);
    }
    if (ok)
        ok = plot_set_bounds(plot, bounds[0], bounds[1], bounds[2], bounds[3]);
    string_t *svg = ok ? plot_to_svg(plot) : NULL;
    if (svg && string_replace(svg, "<svg xmlns=", "<svg data-root-plane=\"true\" xmlns=") >= 0) {
        printf("input %s\nexpression %s\nplot_title %s\nplot_title_TeX %s\nplot_svg %s\n"
               "plot_notice Complex solutions: %zu verified finite roots, including any real roots. "
               "Points are not joined; completeness is not certified. "
               "Worksheet solutions retain their selected precision.%s%s\ndifferentiable no\n",
               argv[1], argv[1], title ? title : "", TeX ? TeX : "", string_c_str(svg), count + extra_count,
               extra_count ? " Newly visible roots are calculated at display precision." : "",
               limited ? " Additional search is bounded to 96 roots per band and imaginary heights ±10000; zoom in." : "");
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
    for (size_t i = 0; i < extra_count; ++i)
        num_destroy(&extra[i]);
    num_set_default_prec_bits(saved);
    return ok ? 0 : 1;
}
