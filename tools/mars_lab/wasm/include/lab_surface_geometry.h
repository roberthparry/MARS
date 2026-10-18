/**
 * @file lab_surface_geometry.h
 * @brief Normalised C display geometry shared by WebGL, Canvas and SVG export.
 *
 * The browser executes the returned drawing plans without interpreting
 * coordinates. Absolute mathematical bounds remain in native MARS.
 * Segment previews clip only the selected input interval; accepted refinements
 * use fresh native units and a centred sphere fit without camera magnification.
 */
#ifndef MARS_LAB_SURFACE_GEOMETRY_H
#define MARS_LAB_SURFACE_GEOMETRY_H

#include "lab_handle.h"

/** @brief Primitive topology shared by projection and SVG export; values retain the browser ABI. */
typedef enum {
    LAB_SURFACE_POLYGONS = 0, /**< Closed polygons with at least three projected vertices. */
    LAB_SURFACE_SEGMENTS = 1  /**< Open two-point segments, retaining curve discontinuities. */
} lab_surface_primitive_t;

/**
 * @brief Build the unit-volume camera, clipping transform and axis drawing plan.
 * @param view Scoped prepared view record containing chart, volume, orientation, dimensions, units, names and ticks.
 * @param override_bounds Scoped six-number [xmin, xmax, ymin, ymax, zmin, zmax] diagnostic bounds array, or zero
 * to use the volume's current zoom. These are display bounds, not exact mathematical sampling expressions.
 * @details Publishes the frame, GPU matrix and axis plan through lab_dom_return; invalid override spans publish null.
 */
void lab_surface_frame(view_jsref view, browser_jsref override_bounds);

/**
 * @brief Project a normalised point using a previously returned frame.
 * @param frame Scoped projection record returned by lab_surface_frame.
 * @param point Scoped three-number unit-volume coordinate array; no clipping or volume transform is applied.
 * @details Publishes a two-number [x, y] array in the frame's SVG coordinate system through lab_dom_return.
 */
void lab_surface_project(frame_jsref frame, browser_jsref point);

/**
 * @brief Transform, clip and project one segment or triangle.
 * @param frame Scoped projection record returned by lab_surface_frame.
 * @param points Scoped array of two or three unit-volume [x, y, z] arrays; more than three produces no vertices.
 * @details Publishes a record with projected two-dimensional points and mean camera depth through lab_dom_return.
 * An entirely clipped primitive has an empty points array and zero depth.
 */
void lab_surface_polygon(frame_jsref frame, browser_jsref points);

/**
 * @brief Clip and project retained primitives in stable far-to-near order.
 * @param frame Scoped native projection plan, including its drawMesh visibility flag.
 * @param primitives Scoped browser-owned array of records with independent points arrays and colour strings;
 * inputs are not mutated and each primitive must contain at most three unit-volume points.
 * @param kind Closed polygons or open segments. Other non-zero values retain segment handling for ABI compatibility.
 * @details Publishes the sorted array through lab_dom_return, or an empty array when drawMesh is false.
 * Reads the frame once and releases temporary handles per primitive and merge step. Runtime is O(n log n),
 * with O(n) browser-owned output and sorting storage, and bounded native scratch space.
 */
void lab_surface_polygons(frame_jsref frame, browser_jsref primitives, lab_surface_primitive_t kind);

/**
 * @brief Clip a segment or triangle against the closed unit cube.
 * @param points Scoped array of at most three [x, y, z] coordinate arrays; larger arrays produce an empty result.
 * @details Publishes an array of clipped three-dimensional vertices through lab_dom_return without changing input.
 */
void lab_surface_clip(browser_jsref points);

#endif
