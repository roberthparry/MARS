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

/** @brief Build the unit-volume camera, clipping transform and axis drawing plan. */
void lab_surface_frame(int view, int override_bounds);

/** @brief Project a normalised point using a previously returned frame. */
void lab_surface_project(int frame, int point);

/** @brief Clip and project a triangle, returning points and mean depth. */
void lab_surface_polygon(int frame, int points);

/**
 * @brief Clip and project retained primitives in one call, returning stable far-to-near order.
 * @param frame Native projection plan, including the geometry visibility flag.
 * @param primitives Browser-owned array of independent point arrays and colours; never mutated.
 * @param segments Non-zero accepts two-point segments; zero requires polygons of at least three points.
 * @details Reads the frame once and releases temporary handles per primitive and merge step.
 * Runtime is O(n log n), with O(n) browser-owned output and sorting storage, and bounded native scratch space.
 */
void lab_surface_polygons(int frame, int primitives, int segments);

/** @brief Clip a triangle already expressed in unit-volume coordinates. */
void lab_surface_clip(int points);

#endif
