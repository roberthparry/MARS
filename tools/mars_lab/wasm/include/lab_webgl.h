/**
 * @file lab_webgl.h
 * @brief Scoped browser adapter for marslib coverage submission commands.
 *
 * Copies worker batch metadata into native memory, validates it through webgl-t.h,
 * and exposes a transient command buffer for synchronous GPU submission. No browser
 * resource handles are retained. Consumers must submit before the next planning call.
 */
#ifndef LAB_WEBGL_H
#define LAB_WEBGL_H

#include "lab_handle.h"

/** @brief Publish {pointer,count} for a validated byte-array of coverage batches; null on failure. */
void lab_webgl_mask_commands(jsref bytes, unsigned vertices);

/** @brief Create a native GPU renderer and return an opaque owner token, or zero. */
unsigned lab_webgl_renderer_create(jsref host);

/** @brief Release a renderer through its synchronous host adapter; stale tokens are ignored. */
void lab_webgl_renderer_free(jsref host, unsigned token);

/** @brief Submit borrowed batch bytes through native GPU ownership and return the published texture ID, or zero. */
unsigned lab_webgl_renderer_draw(jsref host, unsigned token, jsref bytes, unsigned vertices,
                                 unsigned pixels, unsigned quad, unsigned previous);

#endif
