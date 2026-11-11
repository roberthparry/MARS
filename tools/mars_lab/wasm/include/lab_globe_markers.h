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

/**
 * @brief Format geographic degrees and hundredths of minutes for another synchronous native consumer.
 * @param value Finite signed degrees with magnitude at most 540; invalid values produce empty text.
 * @param positive Borrowed scoped hemisphere label used for positive values, for example N or E.
 * @param negative Borrowed scoped hemisphere label used for negative values, for example S or W.
 * @return Scoped browser string, valid until its handle scope is released; no ownership transfers.
 * @note Rounds minutes with degree carry. Zero has no hemisphere suffix. Call on the owning browser thread.
 */
jsref lab_globe_coordinate_text(double value, jsref positive, jsref negative);

/**
 * @brief Publish geographic degrees and hundredths of minutes through the browser bridge.
 * @param value Finite signed degrees with magnitude at most 540; invalid values publish empty text.
 * @param positive Borrowed scoped positive hemisphere label, used only during this synchronous call.
 * @param negative Borrowed scoped negative hemisphere label, used only during this synchronous call.
 * @return No C value; lab_dom_return receives the scoped string from lab_globe_coordinate_text().
 * @note No handles are retained; call on the owning browser thread.
 */
void lab_globe_coordinate(double value, jsref positive, jsref negative);

#endif
