/**
 * @file lab_globe_markers.h
 * @brief Native screen-cell admission and coordinate presentation for globe markers.
 *
 * The browser owns DOM nodes; this adapter owns bounded display policy. A serialised singleton
 * holds only the current frame's direct-indexed occupancy bitmap, never browser handles.
 */
#ifndef LAB_GLOBE_MARKERS_H
#define LAB_GLOBE_MARKERS_H

#include <stdbool.h>

#include "lab_handle.h"

/** @brief Reset bounded marker admission for one viewport; invalid sizes disable admission. */
void lab_globe_markers_begin(double size);

/** @brief Admit a marker and return its adjusted pixels, or null; browser bool arguments must be numeric 0 or 1. */
void lab_globe_markers_place(double x, double y, double width, bool town, bool selected);

/** @brief Convert a projected unit-sphere point to visible viewport pixels, or null outside the view. */
void lab_globe_marker_pixels(jsref point, double zoom, double size);

/** @brief Format geographic degrees and hundredths of minutes, with the supplied hemisphere labels. */
void lab_globe_coordinate(double value, jsref positive, jsref negative);

#endif
