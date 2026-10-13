/**
 * @file plot-t.h
 * @brief Owned two-dimensional plot scenes and in-memory SVG rendering.
 *
 * plot_t copies sampled or measured number_t coordinates, labels and styles.
 * Linear axes, automatic or explicit bounds, ticks, a grid and a legend are
 * rendered in C. Axis ticks use decimal labels, never fractions, with superscript
 * scientific exponents when needed. Coordinate normalisation precedes conversion to drawing doubles,
 * preserving small differences at large offsets. SVG contains no scripts,
 * external resources or caller-supplied markup. The file module may save the
 * returned string; no files or subprocesses are used here. Scene mutation and
 * mathematical operations require external synchronisation. Rendering inherits
 * computation_run() and publishes no partial output on failure.
 */
#ifndef MARS_PLOT_H
#define MARS_PLOT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "string-t.h"
#include "sampler-t.h"

/** @brief Opaque mutable scene; release with plot_free(). */
typedef struct _plot_t plot_t;

/** @brief Rendering styles selected by direct function dispatch. */
typedef enum { PLOT_LINES, PLOT_POINTS } plot_style_t;

/** @brief Borrowed input coordinate; non-real/non-finite values mark a gap. */
typedef struct {
    number_t x; /**< Borrowed coordinate, copied on insertion. */
    number_t y; /**< Borrowed coordinate, copied on insertion. */
    bool break_before; /**< Start a new path at this point. */
} plot_point_t;

/** @brief Copied configuration; start with plot_options_default() before overriding fields. */
typedef struct {
    unsigned width, height; /**< SVG dimensions: 320..8192 by 240..8192. */
    size_t max_points, max_series; /**< Positive hard scene limits, including gap records. */
    const char *title, *x_label, *y_label; /**< Optional copied UTF-8 plain text, never markup. */
    bool grid, legend; /**< Draw grid and series legend. */
    bool smart_y; /**< Opt-in robust vertical range; clipped samples are explicitly labelled in the SVG. */
} plot_options_t;

/** @brief Default 800x500 scene, one million points, 32 series, grid and legend enabled. */
plot_options_t plot_options_default(void);

/** @brief Copy settings and labels; NULL selects defaults. Invalid input gives EINVAL, allocation failure ENOMEM. */
plot_t *plot_new(const plot_options_t *options);

/** @brief Destroy a scene and its copies, accepting NULL and preserving errno. */
void plot_free(plot_t *plot);

/**
 * @brief Atomically copy a non-empty series with optional name, style and 24-bit RGB colour.
 * No ownership transfers. Invalid input gives EINVAL, limits E2BIG, size overflow
 * EOVERFLOW, allocation failure ENOMEM. A failed addition leaves the scene unchanged.
 */
bool plot_add_points(plot_t *plot, const plot_point_t *points, size_t count, const char *name,
                     plot_style_t style, uint32_t colour);

/** @brief Copy a sampler's points and gap flags with the same contract as plot_add_points(). */
bool plot_add_sampler(plot_t *plot, const sampler_t *sampler, const char *name, plot_style_t style, uint32_t colour);

/** @brief Atomically copy finite real xmin < xmax and ymin < ymax; invalid bounds give EINVAL. */
bool plot_set_bounds(plot_t *plot, number_t xmin, number_t xmax, number_t ymin, number_t ymax);

/** @brief Restore automatic data bounds; NULL is harmless. */
void plot_auto_bounds(plot_t *plot);

/** @brief Read the number of copied series; NULL returns zero with EINVAL. */
size_t plot_series_count(const plot_t *plot);

/**
 * @brief Generate an independent SVG string, released with string_free().
 * No finite data gives ENODATA. Unrepresentable drawing coordinates give ERANGE;
 * allocation/scoped stopping errors preserve errno. Labels are XML-escaped and
 * colours are numeric, so user data cannot inject SVG. Data outside explicit
 * bounds is geometrically clipped; a gap is never joined. This is a static SVG,
 * not a raster image, DOM widget or mathematical discontinuity detector.
 */
string_t *plot_to_svg(const plot_t *plot);

#endif
