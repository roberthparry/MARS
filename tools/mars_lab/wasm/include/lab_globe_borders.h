/**
 * @file lab_globe_borders.h
 * @brief Native ownership, screen projection and GPU meshes for optional globe border vectors.
 *
 * The browser supplies bounded geographic segments and receives an SVG path for stationary views.
 * A worker can build bounded packed xyz vertices for a retained GPU travel layer; the browser copies
 * these before yielding, and native shaders keep projection policy out of JavaScript.
 * This display-only adapter shares the globe camera and does not classify geographic positions.
 * Each entry point dispatches synchronously through the service's private singleton. Browser handles
 * require the owning non-shared WASM instance; native dispatch serialises complete operations with a mutex.
 */
#ifndef LAB_GLOBE_BORDERS_H
#define LAB_GLOBE_BORDERS_H

#include "lab_surface_math.h"

/** @brief Replace the native segment cache atomically; return false for malformed or oversized input. */
bool lab_globe_borders_load(jsref text);

/** @brief Atomically load segments and their geographic coverage [west, south, east, north]. */
bool lab_globe_borders_load_region(jsref text, jsref bounds);

/** @brief Test whether native cached coverage contains a globe view, including wrapped longitudes. */
bool lab_globe_borders_covers(jsref view);

/** @brief Release the optional border geometry. */
void lab_globe_borders_clear(void);

/** @brief Return a horizon- and viewport-clipped CSS-pixel SVG path for zoom 1–4096, or null on invalid input or failure. */
void lab_globe_borders_path(quaternion_jsref orientation, double zoom, double size);

/** @brief Build at most one million short spherical segments for GPU travel rendering; return vertex count or zero. */
unsigned lab_globe_borders_mesh(jsref text);

/** @brief Borrow the packed xyz float vertices until the next mesh build or clear; copy before yielding. */
const float *lab_globe_borders_vertices(void);

/** @brief Return native-owned GPU border vertex (0) or fragment (1) shader source. */
void lab_globe_borders_shader(unsigned fragment);

#endif
