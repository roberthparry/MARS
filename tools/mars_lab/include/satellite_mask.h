/**
 * @file satellite_mask.h
 * @brief Offline coastal transparency preparation for regional satellite photographs.
 *
 * Shared only by the optional GDAL preparation executable and its native regression
 * tests. Uses the public globe interface to trim offshore pixels; inland water and
 * missing observations retain their meaning. No installed database is modified.
 */
#ifndef MARS_SATELLITE_MASK_H
#define MARS_SATELLITE_MASK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "string-t.h"
#include "globe-t.h"

/** @brief Measurements from one completed, losslessly masked photograph. */
typedef struct _satellite_mask_stats_t {
    size_t visible_pixels; /**< Pixels retaining non-zero alpha after masking. */
    size_t removed_pixels; /**< Previously visible pixels wholly outside the coastline. */
    bool changed; /**< Whether pixel values changed; false permits immutable source reuse. */
} satellite_mask_stats_t;

/**
 * @brief Write a regional PNG with offshore RGB and alpha cleared and an antialiased coastline.
 * @param[in] globe Borrowed, serially owned shoreline controller, required throughout the call.
 * @param[in] source Borrowed input path to an existing 1800- or 3600-square RGB or RGBA PNG.
 * @param[in] destination Borrowed path for a new output PNG; existing files are rejected.
 * @param[in] bounds Borrowed west, south, east and north degrees matching the photograph.
 * @param[in] source_visible_pixels Verified count of source pixels with non-zero alpha,
 * or SIZE_MAX when unknown. A known count avoids decoding wholly inland or offshore images.
 * @param[out] stats Required caller-owned measurements, reset before processing.
 * @return True after a complete output is available; false for invalid input, unavailable
 * shoreline, decoding, allocation or write failure. Failed output is removed where possible.
 * @note Input pixels are never modified. Unchanged images are hard-linked, or copied across
 * filesystems. The caller owns all arguments; no returned object needs releasing. The optional
 * executable initialises GDAL before calling; calls must be serial. Inland lakes are retained.
 */
bool satellite_mask_png(globe_t *globe, const string_t *source, const string_t *destination,
                        const double bounds[4], size_t source_visible_pixels, satellite_mask_stats_t *stats);

#endif
