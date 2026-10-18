/**
 * @file lab_worker_surface.c
 * @brief Bounded native surface sampling and expression plot dimensionality selection.
 *
 * A private parsed graph receives both axis bindings, including occurrences in
 * scoped mathematical operations. Uniform sampler_t rows form an owned grid;
 * low-precision evaluations are checked against higher-precision coarse probes.
 * Bounded local facet refinement resolves folds without densifying the whole grid.
 * The worksheet graph and numerical default survive every exit unchanged.
 */
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plot-t.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"
#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"
#define MARS_LAB_WORKER_INTERNAL_ACCESS
#include "lab_worker_internal.h"

enum { SURFACE_SIDE = 70, SURFACE_PREVIEW_SIDE = 13 };

typedef struct {
    size_t side;
    number_t x[SURFACE_SIDE], y[SURFACE_SIDE];
    number_t z[SURFACE_SIDE * SURFACE_SIDE];
} surface_grid_t;

typedef struct {
    lab_worker_sample_t *evaluation;
    number_t second;
} surface_row_t;

static void grid_free(surface_grid_t *grid)
{
    if (!grid)
        return;
    for (size_t i = 0; i < grid->side; ++i) {
        num_destroy(&grid->x[i]);
        num_destroy(&grid->y[i]);
    }
    for (size_t i = 0; i < grid->side * grid->side; ++i)
        num_destroy(&grid->z[i]);
    free(grid);
}

static sampler_status_t sample_row(number_t parameter, number_t *x, number_t *z, void *argument)
{
    surface_row_t *row = argument;
    *x = num_clone(parameter);
    number_t coordinates[] = {parameter, row->second};
    *z = lab_worker_sample_eval(row->evaluation, coordinates);
    return SAMPLER_VALUE;
}

static surface_grid_t *grid_sample(const char *source, const char *first, const char *second,
                                   const number_t bounds[4], size_t side, size_t bits)
{
    num_set_default_prec_bits(bits);
    expr_bindings_t *bindings = NULL;
    expr_t *formula = expr_from_string(source, &bindings);
    expr_t *x = bindings ? expr_bindings_get(bindings, first) : NULL;
    expr_t *y = bindings ? expr_bindings_get(bindings, second) : NULL;
    bool own_x = !x, own_y = !y;
    if (own_x)
        x = expr_new_named_var(NUM_NAN, first);
    if (own_y)
        y = expr_new_named_var(NUM_NAN, second);
    surface_grid_t *grid = calloc(1, sizeof(*grid));
    if (grid)
        grid->side = side;
    number_t lo = lab_worker_sample_coordinate(bounds[0], bits), hi = lab_worker_sample_coordinate(bounds[1], bits);
    number_t bottom = lab_worker_sample_coordinate(bounds[2], bits), top = lab_worker_sample_coordinate(bounds[3], bits);
    number_t span = num_sub(top, bottom);
    bool ok = grid && formula && x && y;
    sampler_options_t options = sampler_options_default();
    options.method = SAMPLER_UNIFORM;
    options.intervals = side - 1;
    options.max_evaluations = side;
    expr_t *axes[] = {x, y};
    surface_row_t row = {.evaluation = lab_worker_sample_new(formula, axes, 2, bits)};
    ok = ok && row.evaluation;
    for (size_t r = 0; ok && r < side; ++r) {
        number_t fraction = num_create_from_frac((long)r, (long)side - 1);
        number_t offset = num_mul(span, fraction);
        grid->y[r] = r + 1 == side ? num_clone(top) : num_add(bottom, offset);
        num_destroy(&offset);
        num_destroy(&fraction);
        if (r && !num_gt(grid->y[r], grid->y[r - 1])) {
            errno = ERANGE;
            ok = false;
            break;
        }
        row.second = grid->y[r];
        sampler_t *samples = sampler_new(sample_row, &row, lo, hi, &options);
        ok = samples && sampler_count(samples) == side;
        for (size_t c = 0; ok && c < side; ++c) {
            const sampler_point_t *point = sampler_point(samples, c);
            if (!r)
                grid->x[c] = num_clone(point->x);
            grid->z[r * side + c] = num_clone(point->valid ? point->y : NUM_NAN);
        }
        sampler_free(samples);
    }
    lab_worker_sample_free(row.evaluation);
    num_destroy(&span);
    num_destroy(&top);
    num_destroy(&bottom);
    num_destroy(&hi);
    num_destroy(&lo);
    expr_free(formula);
    if (own_x)
        expr_free(x);
    if (own_y)
        expr_free(y);
    expr_bindings_free(bindings);
    if (!ok) {
        grid_free(grid);
        grid = NULL;
    }
    return grid;
}

/* Opposite finite heights alone are not evidence of a pole. Require actual growth
 * under repeated bisection, or a non-real/non-finite interior sample. */
static bool edge_gap(expr_t *formula, expr_t *axis, number_t lo, number_t hi, number_t left, number_t right)
{
    NUM_SCOPE(numbers);
    if (!num_is_real(left) || !num_is_real(right) || !num_is_finite(left) || !num_is_finite(right) ||
        num_is_zero(left) || num_is_zero(right) || num_lt(left, NUM_ZERO) == num_lt(right, NUM_ZERO))
        return false;
    number_t a = num_abs(left), b = num_abs(right);
    number_t threshold = num_mul(num_gt(a, b) ? a : b, num_create_from_long(8));
    number_t previous = num_gt(a, b) ? a : b;
    unsigned growing = 0;
    for (unsigned i = 0; i < 8; ++i) {
        number_t mid = num_div(num_add(lo, hi), NUM_TWO);
        expr_set_val(axis, mid);
        number_t z = expr_eval(formula);
        if (!num_is_real(z) || !num_is_finite(z))
            return true;
        if (num_is_zero(z))
            return false;
        number_t magnitude = num_abs(z);
        growing = num_gt(magnitude, previous) ? growing + 1 : 0;
        previous = magnitude;
        if (num_lt(left, NUM_ZERO) == num_lt(z, NUM_ZERO)) {
            lo = mid;
            left = z;
        } else {
            hi = mid;
        }
    }
    return growing >= 3 && num_gt(previous, threshold);
}

static bool grid_gaps(surface_grid_t *grid, const char *source, const char *first, const char *second)
{
    expr_bindings_t *bindings = NULL;
    expr_t *formula = expr_from_string(source, &bindings);
    expr_t *x = bindings ? expr_bindings_get(bindings, first) : NULL;
    expr_t *y = bindings ? expr_bindings_get(bindings, second) : NULL;
    bool *missing = calloc(grid->side * grid->side, sizeof(*missing));
    bool ok = formula && missing;
    for (size_t r = 0; ok && r < grid->side; ++r) {
        for (size_t c = 0; c < grid->side; ++c) {
            size_t i = r * grid->side + c;
            if (y)
                expr_set_val(y, grid->y[r]);
            if (x && c + 1 < grid->side &&
                edge_gap(formula, x, grid->x[c], grid->x[c + 1], grid->z[i], grid->z[i + 1]))
                missing[i] = missing[i + 1] = true;
            if (x)
                expr_set_val(x, grid->x[c]);
            if (y && r + 1 < grid->side &&
                edge_gap(formula, y, grid->y[r], grid->y[r + 1], grid->z[i], grid->z[i + grid->side]))
                missing[i] = missing[i + grid->side] = true;
        }
    }
    for (size_t i = 0; ok && i < grid->side * grid->side; ++i)
        if (missing[i]) {
            num_destroy(&grid->z[i]);
            grid->z[i] = num_clone(NUM_NAN);
        }
    free(missing);
    expr_free(formula);
    expr_bindings_free(bindings);
    return ok;
}

static bool grid_agrees(const surface_grid_t *low, const surface_grid_t *high)
{
    NUM_SCOPE(numbers);
    number_t lo = NUM_NAN, hi = NUM_NAN;
    for (size_t i = 0; i < high->side * high->side; ++i) {
        number_t z = high->z[i];
        if (!num_is_finite(z))
            continue;
        if (num_is_nan(lo) || num_lt(z, lo))
            lo = num_clone(z);
        if (num_is_nan(hi) || num_gt(z, hi))
            hi = num_clone(z);
    }
    number_t scale = num_sub(hi, lo);
    if (num_is_zero(scale))
        scale = num_abs(hi);
    number_t tolerance = num_mul(scale, num_create_from_frac(1, 10000000));
    for (size_t i = 0; i < high->side * high->side; ++i) {
        bool a = num_is_finite(low->z[i]), b = num_is_finite(high->z[i]);
        if (a != b || (a && !num_le(num_abs(num_sub(low->z[i], high->z[i])), tolerance)))
            return false;
    }
    return true;
}

static string_t *surface_svg(const char *source, const char *first, const char *second, const char *const limits[4],
                             double azimuth, double elevation, double roll, bool preview, const char *height,
                             const char *scale, const number_t height_bounds[2])
{
    size_t saved_precision = num_get_default_prec_bits();
    number_t bounds[] = {NUM_NAN, NUM_NAN, NUM_NAN, NUM_NAN};
    bool ok = lab_worker_plot_ranges(source, first, second, limits, bounds);
    size_t bits = ok ? lab_worker_plot_range_precision(bounds[0], bounds[1]) : 0;
    size_t other = ok ? lab_worker_plot_range_precision(bounds[2], bounds[3]) : 0;
    if (other > bits)
        bits = other;
    bool manual = !strcmp(scale, "manual");
    if (manual) {
        other = lab_worker_plot_range_precision(height_bounds[0], height_bounds[1]);
        if (other > bits)
            bits = other;
    }
    size_t guard = ok ? lab_worker_plot_check_precision(bounds[0], bounds[1]) : 128;
    other = ok ? lab_worker_plot_check_precision(bounds[2], bounds[3]) : 128;
    if (other > guard)
        guard = other;
    size_t worksheet_check = saved_precision < 4096 ? saved_precision : 4096;
    if (worksheet_check > guard)
        guard = worksheet_check;
    surface_grid_t *grid = NULL;
    while (ok) {
        if (!bits || bits > 32768) {
            errno = ERANGE;
            break;
        }
        size_t reference = bits * 2 > guard ? bits * 2 : guard;
        surface_grid_t *low = grid_sample(source, first, second, bounds, 7, bits);
        surface_grid_t *high = low ? grid_sample(source, first, second, bounds, 7, reference) : NULL;
        bool agree = low && high && grid_agrees(low, high);
        ok = low && high;
        grid_free(high);
        grid_free(low);
        if (ok && agree) {
            grid = grid_sample(source, first, second, bounds, preview ? SURFACE_PREVIEW_SIDE : SURFACE_SIDE, bits);
            if (grid && !grid_gaps(grid, source, first, second)) {
                grid_free(grid);
                grid = NULL;
            }
            break;
        }
        bits *= 2;
    }
    plot_options_t options = plot_options_default();
    options.title = "";
    options.x_label = first;
    options.y_label = second;
    options.z_label = height;
    options.axis_colour = 0x94b8b1;
    options.label_colour = 0xb8ccc5;
    options.legend = false;
    options.smart_y = !strcmp(scale, "auto");
    plot_t *plot = grid ? plot_new(&options) : NULL;
    ok = plot && plot_set_view(plot, azimuth, elevation) && plot_set_roll(plot, roll) &&
         plot_set_surface(plot, grid->x, grid->side, grid->y, grid->side, grid->z);
    if (ok && manual)
        ok = plot_set_height_bounds(plot, height_bounds[0], height_bounds[1]);
    if (ok && !manual && options.smart_y && !preview)
        ok = lab_worker_surface_extrema(plot, source, first, second, grid->x, grid->y, grid->z, grid->side);
    if (ok && !preview)
        ok = lab_worker_surface_refine(plot, source, first, second, grid->x, grid->y, grid->z, grid->side, bits);
    string_t *svg = ok ? plot_to_svg(plot) : NULL;
    bool reduced = false;
    bool retry_uniform = !preview;
    while (svg && !lab_worker_plot_fits(svg) && grid->side > SURFACE_PREVIEW_SIDE) {
        /* Optional refinement must not skip the full-size uniform fallback. */
        size_t side = retry_uniform ? grid->side : (grid->side + 1) / 2;
        retry_uniform = false;
        if (side < SURFACE_PREVIEW_SIDE)
            side = SURFACE_PREVIEW_SIDE;
        string_free(svg);
        plot_free(plot);
        plot = plot_new(&options);
        grid_free(grid);
        grid = grid_sample(source, first, second, bounds, side, bits);
        ok = plot && grid && grid_gaps(grid, source, first, second) &&
             plot_set_surface(plot, grid->x, grid->side, grid->y, grid->side, grid->z) &&
             plot_set_view(plot, azimuth, elevation) && plot_set_roll(plot, roll);
        if (ok && manual)
            ok = plot_set_height_bounds(plot, height_bounds[0], height_bounds[1]);
        if (ok && !manual && options.smart_y && !preview)
            ok = lab_worker_surface_extrema(plot, source, first, second, grid->x, grid->y, grid->z, grid->side);
        svg = ok ? plot_to_svg(plot) : NULL;
        reduced = true;
    }
    if (svg && (!lab_worker_plot_fits(svg) || (reduced && !lab_worker_plot_reduced(svg)))) {
        string_free(svg);
        svg = NULL;
    }
    if (svg && !lab_worker_plot_quality(svg, preview, grid->side)) {
        string_free(svg);
        svg = NULL;
    }
    int saved = errno;
    plot_free(plot);
    grid_free(grid);
    for (size_t i = 0; i < 4; ++i)
        num_destroy(&bounds[i]);
    num_set_default_prec_bits(saved_precision);
    errno = saved;
    return svg;
}

/* Decide dimensionality from native binding metadata, never by parsing source in the browser. */
int lab_worker_plot_request(const char *source, const expr_t *expression, expr_bindings_t *bindings,
                            const expr_t *variable, int argc, char **argv)
{
    if (!expr_has_numeric_form(expression)) {
        printf("plot_available  no\n");
        printf("error       This symbolic result has no supported numerical form to plot.\n");
        return 1;
    }
    const char *kind = argc > 10 ? argv[10] : "auto", *requested = argc > 11 ? argv[11] : "";
    const char *quality = argc > 17 ? argv[17] : "full";
    if (strcmp(quality, "full") && strcmp(quality, "preview")) {
        printf("error       Select full or preview plotting quality.\n");
        return 1;
    }
    if (!variable || !expr_is_variable(variable)) {
        printf("error       Select a variable to plot.\n");
        return 1;
    }
    bool force_complex = !strcmp(kind, "complex_curve");
    /* Distributions have impulse supports, not a second sampled surface coordinate. */
    int distribution = lab_worker_distribution_plot(source, expression, variable, argc > 5 ? argv[5] : "",
                                                    argc > 6 ? argv[6] : "", argc > 7 ? argv[7] : "auto",
                                                    argc > 8 ? argv[8] : "", argc > 9 ? argv[9] : "",
                                                    !strcmp(quality, "preview"));
    if (distribution >= 0)
        return distribution;
    bool curve = force_complex || !strcmp(kind, "curve");
    if (strcmp(kind, "auto") && !curve && strcmp(kind, "surface")) {
        printf("error       Select Automatic, 2-D curve or 3-D surface.\n");
        return 1;
    }
    const expr_t *second = NULL;
    const expr_t *requested_binding = *requested ? expr_bindings_get(bindings, requested) : NULL;
    size_t candidates = 0;
    if (!curve && variable) {
        for (size_t i = 0; i < expr_bindings_count(bindings); ++i) {
            expr_t *binding = expr_bindings_expr_at(bindings, i);
            const char *name = expr_symbol_name(binding);
            if (!name || binding == variable || !expr_is_variable(binding) || expr_bindings_is_constant_at(bindings, i))
                continue;
            number_t value = expr_get_val(binding);
            bool select = *requested ? binding == requested_binding : !strcmp(kind, "surface") || num_is_nan(value);
            num_destroy(&value);
            if (select) {
                if (!second)
                    second = binding;
                ++candidates;
            }
        }
    }
    if (!curve && ((!strcmp(kind, "surface") || *requested) && !candidates)) {
        printf("error       Select a distinct second variable for the surface; bind any remaining parameters.\n");
        return 1;
    }
    if (!second) {
        const char *plane_bounds[4] = {"", "", "", ""};
        for (unsigned i = 0; i < 4 && argc >= 24; ++i)
            plane_bounds[i] = argv[20 + i];
        return lab_worker_plot(source, expression, variable, argc > 5 ? argv[5] : "", argc > 6 ? argv[6] : "",
                                argc > 7 ? argv[7] : "auto", argc > 8 ? argv[8] : "", argc > 9 ? argv[9] : "",
                                plane_bounds, !strcmp(quality, "preview"), force_complex);
    }
    /* At most count + 1 candidates can be occupied; each lookup uses the native binding index. */
    char name[64] = "u";
    const char *const preferred[] = {"v", "w"};
    for (size_t i = 0; expr_bindings_get(bindings, name); ++i) {
        if (i < sizeof(preferred) / sizeof(*preferred))
            snprintf(name, sizeof(name), "%s", preferred[i]);
        else
            snprintf(name, sizeof(name), "u%zu", i - 1);
    }
    expr_t *function = expr_new_named_var(NUM_NAN, name);
    char *sliced = lab_worker_surface_slice(source, expr_symbol_name(variable), expr_symbol_name(second),
                                            argc > 19 ? argv[19] : "");
    int result = sliced ? lab_worker_plot_surface(sliced, expression, variable, second, function, argc, argv) : 1;
    free(sliced);
    expr_free(function);
    return result;
}

/* Share sampling, viewport and camera policy between expression and solved PDE surfaces. */
int lab_worker_plot_surface(const char *source, const expr_t *expression, const expr_t *variable,
                            const expr_t *second, const expr_t *function, int argc, char **argv)
{
    const char *quality = argc > 17 ? argv[17] : "full";
    const char *scale = argc > 7 ? argv[7] : "auto";
    number_t height_bounds[] = {NUM_NAN, NUM_NAN};
    bool manual = !strcmp(scale, "manual");
    if ((strcmp(scale, "auto") && strcmp(scale, "all") && !manual) ||
        (manual && !lab_worker_plot_bounds(argc > 8 ? argv[8] : "", argc > 9 ? argv[9] : "",
                                           &height_bounds[0], &height_bounds[1]))) {
        num_destroy(&height_bounds[0]);
        num_destroy(&height_bounds[1]);
        printf("error       Select a valid height scale and finite manual bounds with minimum < maximum.\n");
        return 1;
    }
    const char *limits[] = {argc > 5 ? argv[5] : "", argc > 6 ? argv[6] : "",
                            argc > 12 ? argv[12] : "", argc > 13 ? argv[13] : ""};
    const char *angles[] = {argc > 14 ? argv[14] : "315", argc > 15 ? argv[15] : "30", argc > 16 ? argv[16] : "0"};
    double view[3];
    for (unsigned i = 0; i < 3; ++i) {
        char *end = NULL;
        errno = 0;
        view[i] = strtod(angles[i], &end);
        if (errno || end == angles[i] || *end || !isfinite(view[i]) || (i == 1 && (view[i] < 5 || view[i] > 85))) {
            num_destroy(&height_bounds[0]);
            num_destroy(&height_bounds[1]);
            printf("error       Use a finite azimuth and roll, and an elevation from 5 to 85 degrees.\n");
            return 1;
        }
    }
    char *height = function ? expr_to_string(function, style_UNBOUND) : NULL;
    string_t *svg = height ? surface_svg(source, expr_symbol_name(variable), expr_symbol_name(second), limits,
                                         view[0], view[1], view[2], !strcmp(quality, "preview"), height,
                                         scale, height_bounds) : NULL;
    int result = lab_worker_plot_output(source, expression, svg, errno, function, variable, second);
    string_free(svg);
    free(height);
    num_destroy(&height_bounds[0]);
    num_destroy(&height_bounds[1]);
    return result;
}
