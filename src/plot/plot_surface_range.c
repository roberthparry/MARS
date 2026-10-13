/**
 * @file plot_surface_range.c
 * @brief Finite height extents and robust or explicit surface viewports.
 *
 * Rendering retains the full data extent for its discontinuity heuristic, while
 * height ticks and geometric clipping use a separate viewport. Finite grid
 * samples contribute equally to the optional percentile estimate; missing
 * values never contribute zero. This private helper serves the SVG renderer.
 */
#include <errno.h>
#include <stdlib.h>

#define MARS_SHARED_COMPUTATION_INTERNAL_ACCESS
#include "internal/computation_internal.h"
#define MARS_PLOT_INTERNAL_ACCESS
#include "plot_internal.h"

bool plot_surface_range(const plot_t *plot, number_t bounds[2], number_t full[2])
{
    size_t size = plot->surface_columns * plot->surface_rows, count = 0;
    bool smart = plot->options.smart_y && !plot->height_fixed;
    number_t *values = smart ? calloc(size, sizeof(*values)) : NULL;
    if (smart && !values)
        return false;
    bool ok = true;
    for (size_t i = 0; ok && i < size; ++i) {
        ok = computation_step_current("plot.surface.range");
        number_t value = plot->surface_z[i];
        if (!num_is_real(value) || !num_is_finite(value))
            continue;
        if (smart)
            values[count] = value;
        ++count;
        for (unsigned end = 0; end < 2; ++end) {
            if (num_is_nan(full[end]) || (end ? num_gt(value, full[end]) : num_lt(value, full[end]))) {
                num_destroy(&full[end]);
                full[end] = num_clone(value);
            }
        }
    }
    if (ok && !count) {
        errno = ENODATA;
        ok = false;
    }
    if (ok) {
        for (unsigned end = 0; end < 2; ++end)
            bounds[end] = num_clone(plot->height_fixed ? plot->height_bounds[end] : full[end]);
        if (smart)
            plot_trim_range(values, count, bounds);
        if (num_eq(bounds[0], bounds[1])) {
            number_t magnitude = num_abs(bounds[0]), twenty = num_create_from_long(20);
            number_t padding = num_div(magnitude, twenty);
            if (num_lt(padding, NUM_ONE)) {
                num_destroy(&padding);
                padding = num_clone(NUM_ONE);
            }
            number_t lower = num_sub(bounds[0], padding), upper = num_add(bounds[1], padding);
            num_destroy(&bounds[0]);
            num_destroy(&bounds[1]);
            bounds[0] = lower;
            bounds[1] = upper;
            num_destroy(&padding);
            num_destroy(&magnitude);
            num_destroy(&twenty);
        }
        ok = num_is_finite(bounds[0]) && num_is_finite(bounds[1]) && num_lt(bounds[0], bounds[1]);
        if (!ok)
            errno = ERANGE;
    }
    free(values);
    return ok;
}
