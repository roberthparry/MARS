/**
 * @file lab_worker_plot_integer.c
 * @brief Native complex-plane plots of affine integer solution families.
 *
 * Structural coefficient matching certifies each lattice before drawing. Both viewport coordinates constrain
 * its integer interval directly; no scan from zero, numerical root search or browser algebra is involved.
 * Cached solved graphs survive viewport changes. Dense views retain bounded, labelled genuine integer members.
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "computation-t.h"
#include "equation-t.h"
#include "plot-t.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"
#define MARS_SHARED_EQUATION_INTERNAL_ACCESS
#include "internal/equation_internal.h"
#define MARS_LAB_WORKER_INTERNAL_ACCESS
#include "lab_worker_internal.h"

/* Certify affine dependence structurally; sampled values cannot establish an infinite lattice. */
bool lab_worker_integer_family(const expr_t *family, const string_t *parameter, number_t *origin, number_t *step)
{
    expr_bindings_t *symbols = family ? expr_bindings_from_expr_internal(family) : NULL;
    expr_t *index = parameter ? expr_bindings_get_text(symbols, parameter) : NULL;
    const expr_t *polynomial = family, *numerator = NULL, *denominator = NULL;
    number_t divisor = num_clone(NUM_ONE);
    bool regular = index != NULL;
    /* Remove independent outer denominators before asking the polynomial collector to match the numerator. */
    while (regular && expr_match_div_expr(polynomial, &numerator, &denominator)) {
        expr_t *variables[] = {index};
        bool dependent = false;
        regular = expr_collect_var_usage(denominator, 1, variables, &dependent) && !dependent;
        number_t value = regular ? expr_eval(denominator) : num_clone(NUM_NAN);
        regular = regular && num_is_finite(value) && !num_is_zero(value);
        number_t product = regular ? num_mul(divisor, value) : num_clone(NUM_NAN);
        num_destroy(&divisor);
        divisor = product;
        num_destroy(&value);
        polynomial = numerator;
    }
    expr_t **coefficients = NULL;
    size_t degree = 0;
    bool ok = regular && origin && step &&
              equ_match_symbolic_polynomial_alloc(polynomial, index, &coefficients, &degree) && degree == 1;
    number_t raw_constant = ok ? expr_eval(coefficients[0]) : num_clone(NUM_NAN);
    number_t raw_linear = ok ? expr_eval(coefficients[1]) : num_clone(NUM_NAN);
    number_t constant = num_div(raw_constant, divisor), linear = num_div(raw_linear, divisor);
    ok = ok && num_is_finite(constant) && num_is_finite(linear) && !num_is_zero(linear);
    if (ok) {
        num_destroy(origin);
        num_destroy(step);
        *origin = num_clone(constant);
        *step = num_clone(linear);
    }
    num_destroy(&linear);
    num_destroy(&constant);
    num_destroy(&raw_linear);
    num_destroy(&raw_constant);
    num_destroy(&divisor);
    for (size_t i = 0; coefficients && i <= degree; ++i)
        expr_free(coefficients[i]);
    free(coefficients);
    expr_bindings_free(symbols);
    return ok;
}

/* Bound one integer coordinate by its projection onto a viewport axis. */
static bool integer_interval(double origin, double step, double lower, double upper, double *first, double *last)
{
    if (!step)
        return origin >= lower && origin <= upper;
    double a = (lower - origin) / step, b = (upper - origin) / step;
    double low = ceil(nextafter(fmin(a, b), -INFINITY));
    double high = floor(nextafter(fmax(a, b), INFINITY));
    *first = fmax(*first, low);
    *last = fmin(*last, high);
    return *first <= *last;
}

static bool integer_point(plot_t *plot, number_t origin, number_t step, int64_t index,
                           const char *axis, const string_t *parameter)
{
    computation_t *scope = computation_current();
    if (scope && !computation_checkpoint(scope, 1))
        return false;
    number_t n = num_create_from_long((long)index), offset = num_mul(step, n), value = num_add(origin, offset);
    plot_point_t point = {num_real_part(value), num_imag_part(value), false};
    string_t *text = num_sprintf_text("%.15n", value);
    string_t *label = text ? string_sprintf("%S = %lld; %s ≈ %S…", parameter, (long long)index, axis, text) : NULL;
    bool ok = label && num_is_finite(value) &&
              plot_add_points(plot, &point, 1, string_c_str(label), PLOT_POINTS, 0x78d8cb);
    string_free(label);
    string_free(text);
    num_destroy(&point.y);
    num_destroy(&point.x);
    num_destroy(&value);
    num_destroy(&offset);
    num_destroy(&n);
    return ok;
}

/* Draw only genuine integer members; exact worksheet formulae and their bindings remain untouched. */
int lab_worker_plot_integer_families(const expr_t *const *families, string_t *const *parameters, size_t count,
                                     const char *axis, const char *title, const char *TeX, char **argv)
{
    if (!families || !parameters || !count || count > 16 || !axis || !argv)
        return 1;
    size_t saved = num_get_default_prec_bits();
    num_set_default_prec_bits(106);
    number_t origins[16] = {0}, steps[16] = {0};
    double coordinates[16][4];
    double limits[4] = {INFINITY, -INFINITY, INFINITY, -INFINITY};
    bool ok = true, dense = false, index_limited = false;
    size_t filled = 0, drawn = 0;
    for (size_t i = 0; ok && i < count; ++i) {
        origins[i] = num_clone(NUM_NAN);
        steps[i] = num_clone(NUM_NAN);
        ++filled;
        ok = lab_worker_integer_family(families[i], parameters[i], origins + i, steps + i);
        number_t components[] = {num_real_part(origins[i]), num_imag_part(origins[i]),
                                  num_real_part(steps[i]), num_imag_part(steps[i])};
        for (size_t j = 0; j < 4; ++j) {
            coordinates[i][j] = num_to_double(components[j]);
            ok = ok && isfinite(coordinates[i][j]);
            num_destroy(components + j);
        }
        for (size_t j = 0; ok && j < 2; ++j) {
            double centre = coordinates[i][j], span = 5 * fabs(coordinates[i][j + 2]);
            limits[j * 2] = fmin(limits[j * 2], centre - span);
            limits[j * 2 + 1] = fmax(limits[j * 2 + 1], centre + span);
        }
    }
    number_t bounds[4] = {NUM_NAN, NUM_NAN, NUM_NAN, NUM_NAN};
    for (size_t j = 0; ok && j < 2; ++j) {
        double centre = limits[j * 2] * 0.5 + limits[j * 2 + 1] * 0.5;
        double span = fmax(0.5, (limits[j * 2 + 1] - limits[j * 2]) * 0.55);
        bounds[j * 2] = num_create_from_double(centre - span);
        bounds[j * 2 + 1] = num_create_from_double(centre + span);
        ok = num_is_finite(bounds[j * 2]) && num_is_finite(bounds[j * 2 + 1]);
    }
    if (ok && (*argv[5] || *argv[6])) {
        num_destroy(bounds);
        num_destroy(bounds + 1);
        ok = lab_worker_plot_bounds(argv[5], argv[6], bounds, bounds + 1);
    }
    if (ok && !strcmp(argv[7], "manual")) {
        num_destroy(bounds + 2);
        num_destroy(bounds + 3);
        ok = lab_worker_plot_bounds(argv[8], argv[9], bounds + 2, bounds + 3);
    }
    string_t *xlabel = string_sprintf("Re(%s)", axis), *ylabel = string_sprintf("Im(%s)", axis);
    plot_options_t options = plot_options_default();
    options.height = 640;
    options.max_series = 256;
    options.x_label = xlabel ? string_c_str(xlabel) : "Re";
    options.y_label = ylabel ? string_c_str(ylabel) : "Im";
    options.axis_colour = 0x94b8b1;
    options.grid_colour = 0x82aaa3;
    options.label_colour = 0xb8ccc5;
    options.legend = false;
    plot_t *plot = ok && xlabel && ylabel ? plot_new(&options) : NULL;
    ok = ok && plot;
    for (size_t i = 0; ok && i < count; ++i) {
        double first = -INFINITY, last = INFINITY;
        bool visible = integer_interval(coordinates[i][0], coordinates[i][2], num_to_double(bounds[0]),
                                          num_to_double(bounds[1]), &first, &last) &&
                       integer_interval(coordinates[i][1], coordinates[i][3], num_to_double(bounds[2]),
                                          num_to_double(bounds[3]), &first, &last);
        if (!visible)
            continue;
        /* Guard conversion to signed indices, and disclose views beyond exactly representable integer labels. */
        index_limited |= first < -4503599627370495.0 || last > 4503599627370495.0;
        first = fmax(first, -4503599627370495.0);
        last = fmin(last, 4503599627370495.0);
        if (first > last)
            continue;
        int64_t begin = (int64_t)first, end = (int64_t)last;
        uint64_t members = (uint64_t)(end - begin) + 1, capacity = 256 / count;
        uint64_t stride = members > capacity ? (members - 1) / (capacity - 1) + 1 : 1;
        dense |= stride > 1;
        for (int64_t n = begin; ok && n <= end; n += (int64_t)stride) {
            ok = integer_point(plot, origins[i], steps[i], n, axis, parameters[i]);
            drawn += ok;
        }
    }
    /* Retain a genuine offscreen member so an empty viewport still has a valid, clipped point scene. */
    if (ok && !drawn)
        ok = integer_point(plot, origins[0], steps[0], 0, axis, parameters[0]);
    if (ok)
        ok = plot_set_bounds(plot, bounds[0], bounds[1], bounds[2], bounds[3]);
    string_t *svg = ok ? plot_to_svg(plot) : NULL;
    if (svg && string_replace(svg, "<svg xmlns=", "<svg data-root-plane=\"true\" data-integer-family=\"true\" xmlns=") >= 0)
        printf("input %s\nexpression %s\nplot_title %s\nplot_title_TeX %s\nplot_svg %s\n"
               "plot_notice Integer solution families: %zu visible points. Points are not joined. "
               "Pan or zoom to reveal further integer branches.%s%s%s\ndifferentiable no\n",
               argv[1], argv[1], title ? title : "", TeX ? TeX : "", string_c_str(svg), drawn,
               drawn || index_limited ? "" : " No solution points lie in this viewport.",
               dense ? " Dense view: representative integer members are shown; zoom in to resolve every point." : "",
               index_limited ? " Displayed integer indices are limited to ±4503599627370495." : "");
    else {
        printf("error Unable to draw the integer solution families; check the real and imaginary limits.\n");
        ok = false;
    }
    string_free(svg);
    plot_free(plot);
    string_free(ylabel);
    string_free(xlabel);
    for (size_t i = 0; i < 4; ++i)
        num_destroy(bounds + i);
    for (size_t i = 0; i < filled; ++i) {
        num_destroy(steps + i);
        num_destroy(origins + i);
    }
    num_set_default_prec_bits(saved);
    return ok ? 0 : 1;
}
