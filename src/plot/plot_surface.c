/**
 * @file plot_surface.c
 * @brief Owned rectangular height grids for native surface plots.
 *
 * Coordinates remain number_t values. The scene copies all inputs atomically;
 * missing heights are retained as holes, never replaced with zero. Surface and
 * curve scenes are deliberately separate, with the same bounded resource policy.
 */
#include <errno.h>
#include <stdlib.h>

#define MARS_SHARED_COMPUTATION_INTERNAL_ACCESS
#include "internal/computation_internal.h"
#define MARS_PLOT_INTERNAL_ACCESS
#include "plot_internal.h"

static bool coordinates(const number_t *values, size_t count)
{
    for (size_t i = 0; i < count; ++i)
        if (!num_is_real(values[i]) || !num_is_finite(values[i]) || (i && !num_gt(values[i], values[i - 1])))
            return false;
    return true;
}

/* Copy a complete grid before publishing it in the caller's empty scene. */
bool plot_set_surface(plot_t *plot, const number_t *x, size_t columns, const number_t *y, size_t rows,
                      const number_t *z)
{
    if (!plot || plot->count || plot->surface_z || plot->fixed || !x || !y || !z || columns < 2 || rows < 2) {
        errno = EINVAL;
        return false;
    }
    if (columns > plot->options.max_points / rows || columns * rows > SIZE_MAX / sizeof(number_t)) {
        errno = E2BIG;
        return false;
    }
    if (!coordinates(x, columns) || !coordinates(y, rows)) {
        errno = EINVAL;
        return false;
    }
    plot_t *copy = plot_new(&plot->options);
    if (!copy)
        return false;
    copy->surface_x = calloc(columns, sizeof(number_t));
    copy->surface_y = calloc(rows, sizeof(number_t));
    copy->surface_z = calloc(columns * rows, sizeof(number_t));
    bool ok = copy->surface_x && copy->surface_y && copy->surface_z;
    if (ok) {
        copy->surface_columns = columns;
        copy->surface_rows = rows;
        for (size_t i = 0; ok && i < columns; ++i) {
            copy->surface_x[i] = num_clone(x[i]);
            ok = num_is_finite(copy->surface_x[i]);
        }
        for (size_t i = 0; ok && i < rows; ++i) {
            copy->surface_y[i] = num_clone(y[i]);
            ok = num_is_finite(copy->surface_y[i]);
        }
        for (size_t i = 0; ok && i < rows * columns; ++i) {
            ok = computation_step_current("plot.surface.copy");
            if (ok) {
                copy->surface_z[i] = num_clone(z[i]);
                ok = !num_is_finite(z[i]) || num_is_finite(copy->surface_z[i]);
            }
        }
    }
    if (ok && computation_poll_current()) {
        plot->surface_x = copy->surface_x;
        plot->surface_y = copy->surface_y;
        plot->surface_z = copy->surface_z;
        plot->surface_columns = columns;
        plot->surface_rows = rows;
        plot->points = columns * rows;
        copy->surface_x = copy->surface_y = copy->surface_z = NULL;
        copy->surface_columns = copy->surface_rows = 0;
    } else {
        ok = false;
        if (!errno)
            errno = ENOMEM;
    }
    plot_free(copy);
    return ok;
}
