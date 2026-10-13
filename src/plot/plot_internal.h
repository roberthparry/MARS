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

typedef struct {
    double left, top, right, bottom;
} plot_label_box_t;

typedef struct {
    double width, height, font_size;
    plot_label_box_t boxes[39];
    size_t count;
} plot_tick_layout_t;

/**
 * @brief Place a tick label with conservative glyph bounds and clearance from earlier labels.
 * Anchor is -1 for end, 0 for middle, 1 for start. Outward dx/dy permits small
 * displacement; zero permits thinning only. Omitted crowded labels are successful.
 */
bool plot_tick_text(string_t *out, plot_tick_layout_t *layout, const string_t *label, unsigned axis,
                    double x, double y, int anchor, double dx, double dy);

/** @brief Measure conservative numeric tick width; a negative result reports formatting failure. */
double plot_tick_width(number_t lower, number_t upper, double font_size);

struct _plot_t {
    double azimuth, elevation, roll;
    plot_options_t options;
    string_t *title, *x_label, *y_label, *z_label;
    plot_series_t *series;
    size_t count, capacity, points;
    number_t bounds[4];
    bool fixed;
    number_t *surface_x, *surface_y, *surface_z;
    size_t surface_columns, surface_rows;
};

/** @brief Render a copied rectangular surface; caller supplies the C numeric locale. */
string_t *plot_surface_svg(const plot_t *plot);

/** @brief Format one decimal tick with superscript scientific notation; caller owns the string. */
string_t *plot_tick_label(number_t value);

/** @brief Compute owned ranges from explicit bounds or one pass over finite data. */
bool plot_ranges(const plot_t *plot, number_t bounds[4]);

/** @brief Select a robust vertical viewport using horizontally distributed finite representatives. */
bool plot_smart_range(const plot_t *plot, number_t bounds[4]);

/** @brief Generate up to twelve owned, evenly spaced decimal ticks inside an axis range. */
size_t plot_ticks(number_t low, number_t high, number_t values[12]);

/** @brief Clip a finite segment before double conversion; return -1 on failure, 0 outside, or 1 with normalised xyxy. */
int plot_clip_segment(const plot_point_t *first, const plot_point_t *last, const number_t bounds[4], double output[4]);

/** @brief Append XML-safe plain text; reject control characters not allowed in XML. */
bool plot_escape(string_t *out, const string_t *text);

#endif
