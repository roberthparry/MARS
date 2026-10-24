/**
 * @file lab_globe_shore.h
 * @brief Bounded native polygon ownership and live SVG shoreline projection.
 *
 * Geographic display polygons are retained in native arrays, not JavaScript.
 * Each operation enters the private singleton's serial dispatcher. Browser handles
 * are borrowed for one synchronous call and are never retained.
 */
#ifndef LAB_GLOBE_SHORE_H
#define LAB_GLOBE_SHORE_H

#include "lab_surface_math.h"

/** Replace binary MSH1 polygons (or legacy text fixtures) and coverage atomically; reject malformed input. */
bool lab_globe_shore_load(jsref text, jsref bounds);

/** Return whether the native polygon cache covers the supplied globe view. */
bool lab_globe_shore_covers(jsref view);

/** Return whether a published frame's geographic bounds cover a view, independently of pending geometry. */
bool lab_globe_shore_frame_covers(jsref view, jsref bounds);

/** Return live SVG surface markup projected with the globe camera, or null on failure. */
void lab_globe_shore_svg(quaternion_jsref orientation, double zoom, double size);

/** Return SVG and prepare a matching binary GPU mask, or null on failure; mask pointers expire on the next projection. */
void lab_globe_shore_frame(quaternion_jsref orientation, double zoom, double size);

/** Borrow the last frame's binary32 XY array from native memory. */
const float *lab_globe_mask_vertices(void);

/** Return the last frame's vertex count. */
unsigned lab_globe_mask_vertex_count(void);

/** Borrow the last frame's packed uint32 first/count/colour triples. */
const void *lab_globe_mask_batches(void);

/** Return the last frame's batch count. */
unsigned lab_globe_mask_batch_count(void);

/** Return the native coverage vertex (0) or fragment (1) shader source. */
void lab_globe_mask_shader(unsigned fragment);

/** Release the polygon cache and coverage. */
void lab_globe_shore_clear(void);

#endif
