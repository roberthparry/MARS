/**
 * @file lab_worker_surface.c
 * @brief Bounded native surface sampling and expression plot dimensionality selection.
 *
 * A private parsed graph receives both axis bindings, including occurrences in
 * scoped mathematical operations. Uniform sampler_t rows form an owned grid;
 * low-precision evaluations are checked against higher-precision coarse probes.
 * The worksheet graph and numerical default survive every exit unchanged.
 */
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plot-t.h"
#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"
#define MARS_LAB_WORKER_INTERNAL_ACCESS
#include "lab_worker_internal.h"

enum { SURFACE_SIDE = 70, SURFACE_PREVIEW_SIDE = 13 };

typedef struct {
    size_t side;
    number_t x[SURFACE_SIDE], y[SURFACE_SIDE], z[SURFACE_SIDE * SURFACE_SIDE];
} surface_grid_t;

typedef struct {
    expr_t *formula, *axis;
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
    expr_set_val(row->axis, parameter);
    *x = num_clone(parameter);
    *z = expr_eval(row->formula);
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
    surface_grid_t *grid = calloc(1, sizeof(*grid));
    if (grid)
        grid->side = side;
    number_t lo = num_as_inexact_real_prec(bounds[0], bits), hi = num_as_inexact_real_prec(bounds[1], bits);
    number_t bottom = num_as_inexact_real_prec(bounds[2], bits), top = num_as_inexact_real_prec(bounds[3], bits);
    number_t span = num_sub(top, bottom);
    bool ok = grid && formula && x && y;
    sampler_options_t options = sampler_options_default();
    options.method = SAMPLER_UNIFORM;
    options.intervals = side - 1;
    options.max_evaluations = side;
    surface_row_t row = {.formula = formula, .axis = x};
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
        expr_set_val(y, grid->y[r]);
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
    num_destroy(&span);
    num_destroy(&top);
    num_destroy(&bottom);
    num_destroy(&hi);
    num_destroy(&lo);
    expr_free(formula);
    expr_bindings_free(bindings);
    if (!ok) {
        grid_free(grid);
        grid = NULL;
    }
    return grid;
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
                             double azimuth, double elevation, double roll, bool preview, const char *height)
{
    size_t saved_precision = num_get_default_prec_bits();
    number_t bounds[] = {NUM_NAN, NUM_NAN, NUM_NAN, NUM_NAN};
    bool ok = lab_worker_plot_ranges(source, first, second, limits, bounds);
    size_t bits = ok ? lab_worker_plot_range_precision(bounds[0], bounds[1]) : 0;
    size_t other = ok ? lab_worker_plot_range_precision(bounds[2], bounds[3]) : 0;
    if (other > bits)
        bits = other;
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
    plot_t *plot = grid ? plot_new(&options) : NULL;
    ok = plot && plot_set_view(plot, azimuth, elevation) && plot_set_roll(plot, roll) &&
         plot_set_surface(plot, grid->x, grid->side, grid->y, grid->side, grid->z);
    string_t *svg = ok ? plot_to_svg(plot) : NULL;
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
    if (strcmp(kind, "auto") && strcmp(kind, "curve") && strcmp(kind, "surface")) {
        printf("error       Select Automatic, 2-D curve or 3-D surface.\n");
        return 1;
    }
    const expr_t *second = NULL;
    const expr_t *requested_binding = *requested ? expr_bindings_get(bindings, requested) : NULL;
    size_t candidates = 0;
    if (strcmp(kind, "curve") && variable) {
        for (size_t i = 0; i < expr_bindings_count(bindings); ++i) {
            expr_t *binding = expr_bindings_expr_at(bindings, i);
            const char *name = expr_symbol_name(binding);
            if (!name || binding == variable || !expr_is_variable(binding) || expr_bindings_is_constant_at(bindings, i))
                continue;
            number_t value = expr_get_val(binding);
            bool select = *requested ? binding == requested_binding : !strcmp(kind, "surface") || num_is_nan(value);
            num_destroy(&value);
            if (select) {
                second = binding;
                ++candidates;
            }
        }
    }
    if (strcmp(kind, "curve") && (candidates > 1 || ((!strcmp(kind, "surface") || *requested) && candidates != 1))) {
        printf("error       Select a distinct second variable for the surface; bind any remaining parameters.\n");
        return 1;
    }
    if (!second)
        return lab_worker_plot(source, expression, variable, argc > 5 ? argv[5] : "", argc > 6 ? argv[6] : "",
                                argc > 7 ? argv[7] : "auto", argc > 8 ? argv[8] : "", argc > 9 ? argv[9] : "");
    const char *limits[] = {argc > 5 ? argv[5] : "", argc > 6 ? argv[6] : "",
                            argc > 12 ? argv[12] : "", argc > 13 ? argv[13] : ""};
    const char *angles[] = {argc > 14 ? argv[14] : "45", argc > 15 ? argv[15] : "30", argc > 16 ? argv[16] : "0"};
    double view[3];
    for (unsigned i = 0; i < 3; ++i) {
        char *end = NULL;
        errno = 0;
        view[i] = strtod(angles[i], &end);
        if (errno || end == angles[i] || *end || !isfinite(view[i]) || (i == 1 && (view[i] < 5 || view[i] > 85))) {
            printf("error       Use a finite azimuth and roll, and an elevation from 5 to 85 degrees.\n");
            return 1;
        }
    }
    /* Each candidate uses a binding-table lookup; at most count + 1 candidates can be occupied. */
    char name[64] = "u";
    const char *const preferred[] = {"v", "w"};
    for (size_t i = 0; expr_bindings_get(bindings, name); ++i) {
        if (i < sizeof(preferred) / sizeof(*preferred))
            snprintf(name, sizeof(name), "%s", preferred[i]);
        else
            snprintf(name, sizeof(name), "u%zu", i - 1);
    }
    expr_t *function = expr_new_named_var(NUM_NAN, name);
    char *height = function ? expr_to_string(function, style_UNBOUND) : NULL;
    string_t *svg = height ? surface_svg(source, expr_symbol_name(variable), expr_symbol_name(second), limits,
                                         view[0], view[1], view[2], !strcmp(quality, "preview"), height) : NULL;
    int result = lab_worker_plot_output(source, expression, svg, errno, function, variable, second);
    string_free(svg);
    free(height);
    expr_free(function);
    return result;
}
