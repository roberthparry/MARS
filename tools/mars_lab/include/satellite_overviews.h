/**
 * @file satellite_overviews.h
 * @brief Offline repair of transparent overview gaps from packaged satellite detail.
 *
 * Used by the optional catalogue preparation executable and its native regression tests.
 * Reads indexed, intersecting detail images and fills only previously transparent overview
 * pixels. Existing colours and source transparency are retained. GDAL is a preparation
 * dependency; neither the Lab renderer nor installed database readers depend on it.
 */
#ifndef MARS_SATELLITE_OVERVIEWS_H
#define MARS_SATELLITE_OVERVIEWS_H

#include <stdbool.h>
#include <stddef.h>

#include "json-t.h"
#include "string-t.h"

/**
 * @brief Fill missing overview observations from detailed PNGs in a disposable texture package.
 * @param[in] directory Borrowed path to a prepared package, including its regional subdirectory.
 * @param[in,out] manifest Caller-owned manifest, updated with image digests, sizes and provenance on success.
 * @param[in] window Optional borrowed west, south, east and north degrees limiting the overviews
 * to repair; NULL checks the whole catalogue. Intersecting detail is still selected spatially.
 * @param[out] changed Required caller-owned count of replaced overview images; reset before processing.
 * @return True after all overviews have been checked; false for invalid metadata, allocation,
 * raster, index or file failure. On failure the disposable directory may contain completed
 * replacements and must not be published with its previous manifest.
 * @note Existing hard links are replaced atomically, never edited in place, so the original
 * source package is unchanged. The caller initialises GDAL and calls serially. No arguments
 * are retained, no returned allocation requires release, and no installed database is opened.
 */
bool satellite_overviews_repair(const string_t *directory, json_t *manifest, const double window[4], size_t *changed);

#endif
