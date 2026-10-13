/**
 * @file plot_internal.h
 * @brief Private owned plot series, numeric ranges and renderer helpers.
 *
 * Only plot implementation files may use this layout. Other modules insert
 * copied records through plot-t.h and never share mutable scene internals.
 */
#ifndef MARS_PLOT_INTERNAL_H
#define MARS_PLOT_INTERNAL_H
#if !defined(MARS_PLOT_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_PLOT_INTERNAL_ACCESS."
#endif

#include "plot-t.h"

typedef struct {
    plot_point_t *points;
    size_t count;
    string_t *name;
    plot_style_t style;
    uint32_t colour;
} plot_series_t;

struct _plot_t {
    plot_options_t options;
    string_t *title, *x_label, *y_label;
    plot_series_t *series;
    size_t count, capacity, points;
    number_t bounds[4];
    bool fixed;
};

/** @brief Compute owned ranges from explicit bounds or one pass over finite data. */
bool plot_ranges(const plot_t *plot, number_t bounds[4]);

/** @brief Select a robust vertical viewport using horizontally distributed finite representatives. */
bool plot_smart_range(const plot_t *plot, number_t bounds[4]);

/** @brief Generate up to twelve owned, evenly spaced decimal ticks inside an axis range. */
size_t plot_ticks(number_t low, number_t high, number_t values[12]);

/** @brief Append XML-safe plain text; reject control characters not allowed in XML. */
bool plot_escape(string_t *out, const string_t *text);

#endif
