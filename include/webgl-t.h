/**
 * @file webgl-t.h
 * @brief Native binary coverage plans for a thin WebGL browser backend.
 *
 * Builds bounded triangle fans and ordered stencil batches from simple polygon rings.
 * Coordinates are floating-point clip-space values, never formatted text. Concave
 * rings use non-zero stencil winding; all rings have normalised winding and batches
 * contain at most 127 rings, avoiding eight-bit stencil overflow even with overlaps.
 * The browser owns GPU objects and submits the supplied buffers and GLSL sources.
 * This module does not provide a graphics context or bypass browser GPU validation.
 * Plans have one serial owner and must not be read while being modified.
 */
#ifndef MARS_WEBGL_H
#define MARS_WEBGL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief Opaque bounded polygon coverage builder. */
typedef struct _webgl_mask_t webgl_mask_t;

/** @brief Binary draw command: vertex range and black (0) or white (1) coverage. */
typedef struct _webgl_mask_batch_t {
    uint32_t first; /**< First vertex, not byte offset. */
    uint32_t count; /**< Triangle vertex count. */
    uint32_t colour; /**< Coverage value. */
} webgl_mask_batch_t;

/** @brief Allocate a builder with a vertex budget in [3,1048576]; release with webgl_mask_free. */
webgl_mask_t *webgl_mask_new(size_t vertex_limit);

/** @brief Release a builder and its binary buffers; accepts NULL. */
void webgl_mask_free(webgl_mask_t *mask);

/** @brief Reset logical contents and failure state, retaining allocations for the next frame. */
void webgl_mask_reset(webgl_mask_t *mask);

/** @brief Begin a simple ring in nesting level 1..4, in non-decreasing level order. */
bool webgl_mask_begin(webgl_mask_t *mask, unsigned level);

/** @brief Append a finite clip-space ring vertex; failure invalidates the current frame until reset. */
bool webgl_mask_point(webgl_mask_t *mask, double x, double y);

/** @brief Close and normalise the ring; fewer than three points produce no coverage. */
bool webgl_mask_end(webgl_mask_t *mask);

/** @brief Borrow interleaved binary32 XY vertices until the next mutation; NULL for incomplete/failed frames. */
const float *webgl_mask_vertices(const webgl_mask_t *mask, size_t *count);

/** @brief Borrow ordered draw batches until the next mutation; NULL for incomplete/failed frames. */
const webgl_mask_batch_t *webgl_mask_batches(const webgl_mask_t *mask, size_t *count);

/** @brief Borrow the immutable WebGL 1 coverage vertex shader; attribute location is backend-owned. */
const char *webgl_mask_vertex_shader(void);

/** @brief Borrow the immutable WebGL 1 coverage fragment shader; uniform shade is black or white. */
const char *webgl_mask_fragment_shader(void);

#endif
