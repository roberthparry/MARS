/**
 * @file plot.c
 * @brief Plot scene ownership, bounded insertion, numeric viewport and surface camera selection.
 *
 * All inputs are copied before publication. A linear bounds pass is necessary
 * to visit each coordinate once; no key lookup or repeated series search is used.
 */
#include <errno.h>
#include <math.h>
#include <stdlib.h>

#define MARS_SHARED_COMPUTATION_INTERNAL_ACCESS
#include "internal/computation_internal.h"
#define MARS_PLOT_INTERNAL_ACCESS
#include "plot_internal.h"

/* Default linear scene with explicit resource limits. */
plot_options_t plot_options_default(void)
{
    return (plot_options_t){.width = 800, .height = 500, .max_points = 1000000, .max_series = 32,
                            .title = "", .x_label = "x", .y_label = "y", .z_label = "f", .grid = true, .legend = true,
                            .axis_colour = 0x64748b, .grid_colour = 0x94a3b8, .label_colour = 0x64748b};
}

/* Capture plain text and immutable settings without borrowing caller storage. */
plot_t *plot_new(const plot_options_t *options)
{
    plot_options_t settings = options ? *options : plot_options_default();
    if (settings.width < 320 || settings.width > 8192 || settings.height < 240 || settings.height > 8192 ||
        !settings.max_points || !settings.max_series || settings.max_points > SIZE_MAX / sizeof(plot_point_t) ||
        settings.max_series > SIZE_MAX / sizeof(plot_series_t) || settings.axis_colour > 0xffffffu ||
        settings.grid_colour > 0xffffffu || settings.label_colour > 0xffffffu) {
        errno = EINVAL;
        return NULL;
    }
    plot_t *plot = calloc(1, sizeof(*plot));
    if (!plot)
        return NULL;
    plot->options = settings;
    plot->azimuth = 45;
    plot->elevation = 30;
    plot->options.title = plot->options.x_label = plot->options.y_label = plot->options.z_label = NULL;
    plot->title = string_new_with(settings.title ? settings.title : "");
    plot->x_label = string_new_with(settings.x_label ? settings.x_label : "");
    plot->y_label = string_new_with(settings.y_label ? settings.y_label : "");
    plot->z_label = string_new_with(settings.z_label ? settings.z_label : "");
    for (size_t i = 0; i < 4; ++i)
        plot->bounds[i] = NUM_NAN;
    if (!plot->title || !plot->x_label || !plot->y_label || !plot->z_label) {
        plot_free(plot);
        return NULL;
    }
    return plot;
}

/* Change only the camera; retained surface samples are not evaluated again. */
bool plot_set_view(plot_t *plot, double azimuth, double elevation)
{
    if (!plot || !isfinite(azimuth) || !isfinite(elevation) || elevation < 5 || elevation > 85) {
        errno = EINVAL;
        return false;
    }
    plot->azimuth = fmod(azimuth, 360);
    if (plot->azimuth < 0)
        plot->azimuth += 360;
    plot->elevation = elevation;
    return true;
}

/* Rotate the projected view clockwise while retaining its samples and depth order. */
bool plot_set_roll(plot_t *plot, double roll)
{
    if (!plot || !isfinite(roll)) {
        errno = EINVAL;
        return false;
    }
    plot->roll = fmod(roll, 360) + 0.0;
    if (plot->roll < 0)
        plot->roll += 360;
    return true;
}

static void series_clear(plot_series_t *series)
{
    for (size_t i = 0; i < series->count; ++i) {
        num_destroy(&series->points[i].x);
        num_destroy(&series->points[i].y);
    }
    free(series->points);
    string_free(series->name);
}

/* Release scene ownership, never caller coordinates. */
void plot_free(plot_t *plot)
{
    int saved = errno;
    if (plot) {
        for (size_t i = 0; i < plot->count; ++i)
            series_clear(&plot->series[i]);
        free(plot->series);
        for (size_t i = 0; i < plot->surface_columns; ++i)
            num_destroy(&plot->surface_x[i]);
        for (size_t i = 0; i < plot->surface_rows; ++i)
            num_destroy(&plot->surface_y[i]);
        for (size_t i = 0; i < plot->surface_columns * plot->surface_rows; ++i)
            num_destroy(&plot->surface_z[i]);
        free(plot->surface_x);
        free(plot->surface_y);
        free(plot->surface_z);
        for (size_t i = 0; i < 4; ++i)
            num_destroy(&plot->bounds[i]);
        string_free(plot->title);
        string_free(plot->x_label);
        string_free(plot->y_label);
        string_free(plot->z_label);
        free(plot);
    }
    errno = saved;
}

static bool valid_coordinate(number_t value)
{
    return num_is_real(value) && num_is_finite(value);
}

/* Copy a complete series before updating counts or making it visible. */
bool plot_add_points(plot_t *plot, const plot_point_t *points, size_t count, const char *name,
                     plot_style_t style, uint32_t colour)
{
    if (!plot || plot->surface_z || !points || !count || style < PLOT_LINES || style > PLOT_POINTS || colour > 0xffffffu) {
        errno = EINVAL;
        return false;
    }
    if (count > plot->options.max_points - plot->points || plot->count == plot->options.max_series) {
        errno = E2BIG;
        return false;
    }
    plot_series_t series = {.name = string_new_with(name ? name : ""), .style = style, .colour = colour};
    series.points = calloc(count, sizeof(*series.points));
    bool ok = series.name && series.points;
    for (size_t i = 0; ok && i < count; ++i) {
        ok = computation_step_current("plot.copy");
        if (!ok)
            break;
        plot_point_t point = {.x = num_clone(points[i].x), .y = num_clone(points[i].y),
                              .break_before = points[i].break_before};
        series.points[series.count++] = point;
        if ((valid_coordinate(points[i].x) && !valid_coordinate(point.x)) ||
            (valid_coordinate(points[i].y) && !valid_coordinate(point.y))) {
            errno = ENOMEM;
            ok = false;
        }
    }
    if (ok && plot->count == plot->capacity) {
        size_t capacity = plot->capacity ? plot->capacity * 2 : 4;
        if (capacity > plot->options.max_series)
            capacity = plot->options.max_series;
        plot_series_t *replacement = realloc(plot->series, capacity * sizeof(*replacement));
        ok = replacement != NULL;
        if (ok) {
            plot->series = replacement;
            plot->capacity = capacity;
        }
    }
    if (!ok || !computation_poll_current()) {
        int saved = errno;
        series_clear(&series);
        errno = saved;
        return false;
    }
    plot->series[plot->count++] = series;
    plot->points += count;
    return true;
}

/* Preserve sampler gaps while taking independent copies of its coordinates. */
bool plot_add_sampler(plot_t *plot, const sampler_t *sampler, const char *name, plot_style_t style, uint32_t colour)
{
    if (!plot || !sampler) {
        errno = EINVAL;
        return false;
    }
    size_t count = sampler_count(sampler);
    if (count > plot->options.max_points - plot->points) {
        errno = E2BIG;
        return false;
    }
    plot_point_t *points = calloc(count, sizeof(*points));
    if (!points)
        return false;
    for (size_t i = 0; i < count; ++i) {
        const sampler_point_t *sample = sampler_point(sampler, i);
        points[i] = (plot_point_t){sample->valid ? sample->x : NUM_NAN, sample->valid ? sample->y : NUM_NAN,
                                   sample->break_before};
    }
    bool ok = plot_add_points(plot, points, count, name, style, colour);
    int saved = errno;
    free(points);
    errno = saved;
    return ok;
}

/* Replace an explicit viewport only after every bound is copied successfully. */
bool plot_set_bounds(plot_t *plot, number_t xmin, number_t xmax, number_t ymin, number_t ymax)
{
    if (!plot || plot->surface_z || !valid_coordinate(xmin) || !valid_coordinate(xmax) || !valid_coordinate(ymin) ||
        !valid_coordinate(ymax) || !num_lt(xmin, xmax) || !num_lt(ymin, ymax)) {
        errno = EINVAL;
        return false;
    }
    number_t bounds[] = {num_clone(xmin), num_clone(xmax), num_clone(ymin), num_clone(ymax)};
    bool ok = true;
    for (size_t i = 0; i < 4; ++i)
        ok = ok && valid_coordinate(bounds[i]);
    if (!ok) {
        for (size_t i = 0; i < 4; ++i)
            num_destroy(&bounds[i]);
        errno = ENOMEM;
        return false;
    }
    for (size_t i = 0; i < 4; ++i) {
        num_destroy(&plot->bounds[i]);
        plot->bounds[i] = bounds[i];
    }
    plot->fixed = true;
    return true;
}

/* Select automatic bounds without throwing away existing series. */
void plot_auto_bounds(plot_t *plot)
{
    if (plot)
        plot->fixed = false;
}

/* Inspect the scene in constant time. */
size_t plot_series_count(const plot_t *plot)
{
    if (!plot)
        errno = EINVAL;
    return plot ? (plot->surface_z ? 1 : plot->count) : 0;
}

static bool update_range(number_t bounds[2], number_t value)
{
    if (num_is_nan(bounds[0]) || num_lt(value, bounds[0])) {
        num_destroy(&bounds[0]);
        bounds[0] = num_clone(value);
    }
    if (num_is_nan(bounds[1]) || num_gt(value, bounds[1])) {
        num_destroy(&bounds[1]);
        bounds[1] = num_clone(value);
    }
    return valid_coordinate(bounds[0]) && valid_coordinate(bounds[1]);
}

bool plot_ranges(const plot_t *plot, number_t bounds[4])
{
    bool found = false;
    for (size_t i = 0; i < plot->count; ++i) {
        const plot_series_t *series = &plot->series[i];
        for (size_t j = 0; j < series->count; ++j) {
            if (!computation_step_current("plot.bounds"))
                return false;
            const plot_point_t *point = &series->points[j];
            if (!valid_coordinate(point->x) || !valid_coordinate(point->y))
                continue;
            found = true;
            if (!update_range(bounds, point->x) || !update_range(bounds + 2, point->y)) {
                errno = ENOMEM;
                return false;
            }
        }
    }
    if (!found) {
        errno = ENODATA;
        return false;
    }
    if (!plot->fixed && plot->options.smart_y && !plot_smart_range(plot, bounds))
        return false;
    for (size_t axis = 0; axis < 4; axis += 2) {
        if (plot->fixed) {
            num_destroy(&bounds[axis]);
            num_destroy(&bounds[axis + 1]);
            bounds[axis] = num_clone(plot->bounds[axis]);
            bounds[axis + 1] = num_clone(plot->bounds[axis + 1]);
        } else if (!num_lt(bounds[axis], bounds[axis + 1])) {
            number_t magnitude = num_abs(bounds[axis]), fraction = num_create_from_frac(1, 20);
            number_t radius = num_mul(magnitude, fraction);
            if (num_lt(radius, NUM_ONE)) {
                num_destroy(&radius);
                radius = num_clone(NUM_ONE);
            }
            number_t low = num_sub(bounds[axis], radius), high = num_add(bounds[axis + 1], radius);
            num_destroy(&bounds[axis]);
            num_destroy(&bounds[axis + 1]);
            bounds[axis] = low;
            bounds[axis + 1] = high;
            num_destroy(&radius);
            num_destroy(&fraction);
            num_destroy(&magnitude);
        }
        if (!valid_coordinate(bounds[axis]) || !valid_coordinate(bounds[axis + 1]) ||
            !num_lt(bounds[axis], bounds[axis + 1])) {
            errno = ERANGE;
            return false;
        }
    }
    return true;
}
