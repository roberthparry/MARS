/**
 * @file plot-t.h
 * @brief Owned curves, space curves, height grids and meshes with in-memory SVG rendering.
 *
 * plot_t copies sampled or measured number_t coordinates, labels and styles.
 * Linear bottom/left axes with open top/right edges, automatic or explicit bounds, ticks, a grid and a legend
 * are rendered in C, with independent RGB colours for curves, axes, grid and labels.
 * Rectangular height grids have azimuth, elevation and roll controls, named positive-end
 * axis arrows, automatic or manual height limits, coloured facets and holes at missing values; they cannot be mixed
 * with curve series in the same scene. Endpoint names stay upright during rotation.
 * Independent triangle meshes also represent implicit surfaces with multiple
 * heights per horizontal point. Curve intervals can carry signed shading to zero.
 * Independent line segments represent space curves using the same equal-unit camera.
 * Two-dimensional projections may also opt into equal physical coordinate units.
 * Height grids can carry refined drawing facets independently of range statistics.
 * Axis ticks use decimal labels, never fractions, with superscript
 * scientific exponents when needed. Crowded labels are spaced or thinned without
 * removing tick strokes. Coordinate normalisation precedes conversion to drawing doubles,
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

/** @brief Line, point or dotted-tip impulse-stem rendering, selected by direct function dispatch. */
typedef enum { PLOT_LINES, PLOT_POINTS, PLOT_IMPULSES } plot_style_t;

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
    const char *z_label; /**< Surface height-axis endpoint name; default "f". Copied UTF-8 plain text. */
    bool grid, legend; /**< Draw grid and series legend. */
    bool smart_y; /**< Opt-in robust curve Y or surface height range; no visible clipping annotation. */
    bool guard_gaps; /**< Use robust Y bounds for discontinuous line series even in Fit all; manual bounds win. */
    bool equal_units; /**< Preserve equal physical coordinate units in two-dimensional projections. */
    bool legend_box; /**< Enclose the legend in a box with coloured line or dotted-tip impulse samples. */
    bool legend_above; /**< Reserve space above the axes for a boxed legend; requires legend and legend_box. */
    bool schematic_impulses; /**< Draw equal-height thin stems with dotted tips; weights remain in hover titles. */
    uint32_t legend_background; /**< 24-bit RGB background for a boxed legend. */
    uint32_t axis_colour, grid_colour, label_colour; /**< 24-bit RGB colours for axes, grid and tick/axis labels. */
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

/**
 * @brief Shade line series between zero and their sampled values within a real x interval.
 * Copies the limits; reversed limits reverse contribution colours. Positive and
 * negative contributions use teal and gold respectively. Gaps remain unfilled.
 * Infinite limits are clipped to the viewport; only finite limits receive markers.
 * This visual annotation neither evaluates an integral nor certifies convergence.
 */
bool plot_shade_interval(plot_t *plot, number_t lower, number_t upper);

/**
 * @brief Copy a rectangular height surface into an empty scene, excluding line/point series.
 * x and y contain strictly increasing finite real coordinates (at least two each).
 * z contains rows*columns heights in row-major order; non-real/non-finite heights
 * leave holes. Limits count all vertices. Inputs remain caller-owned. A scene owns
 * at most one surface; explicit 2-D bounds cannot be applied to it.
 * Rendering uses a configurable oblique view and a blue/teal/gold
 * height palette. Heights fit all finite data unless smart_y or manual height bounds are selected.
 */
bool plot_set_surface(plot_t *plot, const number_t *x, size_t columns, const number_t *y, size_t rows,
                      const number_t *z);

/**
 * @brief Copy independent triangular facets into an empty three-dimensional scene.
 * The x, y and z arrays contain count finite real vertices, with each consecutive
 * triple describing one triangle. Count must be divisible by three. Zero permits
 * NULL arrays and an empty scene, which requires explicit horizontal and height
 * bounds before rendering; it retains axes and camera controls without facets.
 * Unlike a height grid, a mesh permits multiple heights at the same x and y.
 * Scene limits, camera controls and height clipping also apply to meshes.
 */
bool plot_set_mesh(plot_t *plot, const number_t *x, const number_t *y, const number_t *z, size_t count);

/**
 * @brief Copy independent line segments into an empty three-dimensional scene.
 * Each consecutive pair of finite real vertices describes one segment. Count
 * must be positive and even. Separate segments preserve sampled domain breaks.
 * Coordinates retain native precision; camera, axes and equal-unit projection
 * are shared with meshes. Inputs remain caller-owned and failure is atomic.
 */
bool plot_set_segments(plot_t *plot, const number_t *x, const number_t *y, const number_t *z, size_t count);

/**
 * @brief Copy refined drawing facets for an existing rectangular height grid.
 * Consecutive triples contain finite real vertices. The original grid retains
 * ownership of horizontal bounds and robust height statistics; refined extrema
 * extend untrimmed automatic height bounds. Supply a complete conforming mesh
 * within the grid's horizontal domain. Attach at most once per scene. Count must
 * be positive, divisible by three and within the remaining scene point budget.
 * Invalid input leaves the scene unchanged.
 */
bool plot_set_surface_facets(plot_t *plot, const number_t *x, const number_t *y, const number_t *z, size_t count);

/**
 * @brief Atomically copy finite real minimum < maximum for an existing surface's height axis.
 * Invalid input or a scene without a surface gives EINVAL and leaves the previous bounds unchanged.
 * Facets are intersected with the height interval, never flattened onto its boundaries.
 * plot_auto_bounds() restores the configured automatic height policy without resampling.
 */
bool plot_set_height_bounds(plot_t *plot, number_t minimum, number_t maximum);

/**
 * @brief Include a located finite turning-point height in automatic surface limits.
 * Copies the value without modifying grid geometry. Repeated calls accumulate an
 * advisory range that smart cropping cannot remove; explicit height limits win.
 * Returns false with EINVAL for a non-finite/non-real value or no surface.
 */
bool plot_include_height(plot_t *plot, number_t height);

/**
 * @brief Set the surface view in degrees without changing or resampling its copied grid.
 * Defaults are azimuth 315 and elevation 30: horizontal ticks increase left to right.
 * Finite azimuths wrap into [0,360);
 * elevation must be finite and within [5,85] to keep the three axes readable.
 * Invalid input gives EINVAL and leaves the view unchanged. This has no effect on curves.
 */
bool plot_set_view(plot_t *plot, double azimuth, double elevation);

/**
 * @brief Set clockwise screen-plane rotation in degrees, without resampling a surface.
 * Defaults to zero. Finite angles wrap into [0,360); invalid input returns false
 * with EINVAL and leaves the camera unchanged. Curve rendering ignores roll.
 */
bool plot_set_roll(plot_t *plot, double roll);

/**
 * @brief Atomically copy finite real xmin < xmax and ymin < ymax; invalid bounds give EINVAL.
 * Curves are clipped to the viewport. For meshes these bounds set the horizontal
 * camera frame; callers must supply facets already clipped to that horizontal domain.
 * Rectangular height grids retain their own horizontal coordinate bounds.
 */
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
 * bounds is geometrically clipped in native precision before drawing conversion;
 * huge finite off-screen samples do not abort rendering. A gap is never joined. This is a static SVG,
 * not a raster image, DOM widget or mathematical discontinuity detector.
 */
string_t *plot_to_svg(const plot_t *plot);

#endif
