/**
 * @file lab_surface_geometry.h
 * @brief Normalised C display geometry shared by WebGL, Canvas and SVG export.
 *
 * The browser executes the returned drawing plans without interpreting
 * coordinates. Absolute mathematical bounds remain in native MARS.
 */
#ifndef MARS_LAB_SURFACE_GEOMETRY_H
#define MARS_LAB_SURFACE_GEOMETRY_H

/** @brief Build the unit-volume camera, clipping transform and axis drawing plan. */
void lab_surface_frame(int view, int override_bounds);

/** @brief Project a normalised point using a previously returned frame. */
void lab_surface_project(int frame, int point);

/** @brief Clip and project a triangle, returning points and mean depth. */
void lab_surface_polygon(int frame, int points);

/** @brief Clip a triangle already expressed in unit-volume coordinates. */
void lab_surface_clip(int points);

#endif
