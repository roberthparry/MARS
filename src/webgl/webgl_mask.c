/**
 * @file webgl_mask.c
 * @brief Bounded binary polygon fans and overflow-safe stencil batches.
 *
 * The renderer accumulates signed fan coverage with INCR_WRAP/DECR_WRAP, then
 * paints a full-screen quad wherever the stencil is non-zero and clears stencil
 * before the next batch. Ring winding is normalised so overlapping simple rings
 * form a union, including across batches. Higher nesting levels overwrite lower
 * ones. No triangulation, graphics driver or host formatting dependency is needed.
 */
#include <math.h>
#include <stdlib.h>

#include "webgl-t.h"

struct _webgl_mask_t {
    float *vertices;
    webgl_mask_batch_t *batches;
    size_t count, capacity, limit, batch_count, batch_capacity, start, points;
    unsigned level, batch_rings;
    double first[2], previous[2], area;
    bool active, failed;
};

static bool webgl_mask_fail(webgl_mask_t *mask)
{
    mask->failed = true;
    return false;
}

/* Allocate only the owner; geometry storage grows within the caller's budget. */
webgl_mask_t *webgl_mask_new(size_t vertex_limit)
{
    if (vertex_limit < 3 || vertex_limit > 1048576) return NULL;
    webgl_mask_t *mask = calloc(1, sizeof *mask);
    if (mask) mask->limit = vertex_limit;
    return mask;
}

/* Release both bounded buffers. */
void webgl_mask_free(webgl_mask_t *mask)
{
    if (!mask) return;
    free(mask->vertices); free(mask->batches); free(mask);
}

/* Reuse allocations without exposing an old frame as a new result. */
void webgl_mask_reset(webgl_mask_t *mask)
{
    if (!mask) return;
    mask->count = mask->batch_count = mask->points = mask->batch_rings = mask->level = 0;
    mask->active = mask->failed = false;
}

/* Bound stencil winding independently of how many triangles a concave ring needs. */
bool webgl_mask_begin(webgl_mask_t *mask, unsigned level)
{
    if (!mask) return false;
    if (mask->failed || mask->active || level < 1 || level > 4 || level < mask->level)
        return webgl_mask_fail(mask);
    if (level != mask->level || mask->batch_rings == 127) {
        if (mask->batch_count == mask->batch_capacity) {
            size_t capacity = mask->batch_capacity ? mask->batch_capacity * 2 : 16;
            if (capacity > 16384) return webgl_mask_fail(mask);
            void *next = realloc(mask->batches, capacity * sizeof *mask->batches);
            if (!next) return webgl_mask_fail(mask);
            mask->batches = next; mask->batch_capacity = capacity;
        }
        mask->batches[mask->batch_count++] = (webgl_mask_batch_t){(uint32_t)mask->count, 0, level % 2};
        mask->batch_rings = 0;
    }
    ++mask->batch_rings;
    mask->level = level; mask->start = mask->count; mask->points = 0; mask->area = 0; mask->active = true;
    return true;
}

/* Generate the fan in native memory; no coordinate crosses a host-value API. */
bool webgl_mask_point(webgl_mask_t *mask, double x, double y)
{
    if (!mask) return false;
    if (mask->failed || !mask->active || !isfinite(x) || !isfinite(y) || fabs(x) > 65536 || fabs(y) > 65536)
        return webgl_mask_fail(mask);
    if (mask->points >= 2) {
        if (mask->count + 3 > mask->limit) return webgl_mask_fail(mask);
        if (mask->count + 3 > mask->capacity) {
            size_t capacity = mask->capacity ? mask->capacity * 2 : 4096;
            if (capacity > mask->limit) capacity = mask->limit;
            void *next = realloc(mask->vertices, capacity * 2 * sizeof(float));
            if (!next) return webgl_mask_fail(mask);
            mask->vertices = next; mask->capacity = capacity;
        }
        float *p = mask->vertices + mask->count * 2;
        p[0] = (float)mask->first[0]; p[1] = (float)mask->first[1];
        p[2] = (float)mask->previous[0]; p[3] = (float)mask->previous[1]; p[4] = (float)x; p[5] = (float)y;
        mask->area += (mask->previous[0] - mask->first[0]) * (y - mask->first[1]) -
                      (mask->previous[1] - mask->first[1]) * (x - mask->first[0]);
        mask->count += 3;
    }
    if (!mask->points) { mask->first[0] = x; mask->first[1] = y; }
    mask->previous[0] = x; mask->previous[1] = y; ++mask->points;
    return true;
}

/* Normalise complete rings, preserving signed fan cancellation in concave regions. */
bool webgl_mask_end(webgl_mask_t *mask)
{
    if (!mask) return false;
    if (mask->failed || !mask->active) return webgl_mask_fail(mask);
    if (mask->area == 0) mask->count = mask->start;
    if (mask->area < 0) {
        for (size_t i = mask->start; i < mask->count; i += 3) {
            float *p = mask->vertices + i * 2;
            float x = p[2], y = p[3]; p[2] = p[4]; p[3] = p[5]; p[4] = x; p[5] = y;
        }
    }
    webgl_mask_batch_t *batch = &mask->batches[mask->batch_count - 1];
    batch->count = (uint32_t)(mask->count - batch->first);
    mask->active = false;
    return true;
}

/* Borrow packed vertices only after every ring has completed. */
const float *webgl_mask_vertices(const webgl_mask_t *mask, size_t *count)
{
    bool ok = mask && !mask->active && !mask->failed;
    if (count) *count = ok ? mask->count : 0;
    return ok ? mask->vertices : NULL;
}

/* Borrow the bounded draw list, not a textual rendering language. */
const webgl_mask_batch_t *webgl_mask_batches(const webgl_mask_t *mask, size_t *count)
{
    bool ok = mask && !mask->active && !mask->failed;
    if (count) *count = ok ? mask->batch_count : 0;
    return ok ? mask->batches : NULL;
}

/* Keep coverage shader ownership with the native render plan. */
const char *webgl_mask_vertex_shader(void)
{
    return "attribute vec2 position; void main(){gl_Position=vec4(position,0.,1.);}";
}

/* Uniform coverage colour is independent of source imagery and polar shading. */
const char *webgl_mask_fragment_shader(void)
{
    return "precision mediump float; uniform float shade; void main(){gl_FragColor=vec4(vec3(shade),1.);}";
}

/* Validate the entire frame before emitting any GPU state changes. */
size_t webgl_mask_commands(const webgl_mask_batch_t *batches, size_t count, size_t vertices,
                          webgl_mask_command_t *commands, size_t capacity)
{
    if (count > 16384 || vertices > 1048576 || vertices % 3 || (count && !batches)) return SIZE_MAX;
    size_t required = 0;
    for (size_t i = 0; i < count; ++i) {
        const webgl_mask_batch_t *batch = batches + i;
        if (batch->first % 3 || batch->count % 3 || batch->first > vertices ||
            batch->count > vertices - batch->first || batch->colour > 1) return SIZE_MAX;
        if (batch->count) required += 13;
    }
    if (!commands) return required;
    if (capacity < required) return SIZE_MAX;
    /* WebGL 1 enum values are part of its portable command ABI, not platform headers. */
    enum { triangles = 4, front = 0x0404, back = 0x0405, always = 0x0207, not_equal = 0x0205,
           keep = 0x1e00, increment_wrap = 0x8507, decrement_wrap = 0x8508, stencil_bit = 0x0400 };
    size_t next = 0;
    for (size_t i = 0; i < count; ++i) {
        const webgl_mask_batch_t *batch = batches + i;
        if (!batch->count) continue;
        const webgl_mask_command_t pass[] = {
            {WEBGL_MASK_BIND_XY, {0}},
            {WEBGL_MASK_COLOUR_WRITE, {0}},
            {WEBGL_MASK_STENCIL_FUNCTION, {always, 0, 255}},
            {WEBGL_MASK_STENCIL_SEPARATE, {front, keep, keep, increment_wrap}},
            {WEBGL_MASK_STENCIL_SEPARATE, {back, keep, keep, decrement_wrap}},
            {WEBGL_MASK_DRAW, {triangles, batch->first, batch->count}},
            {WEBGL_MASK_COLOUR_WRITE, {1}},
            {WEBGL_MASK_STENCIL_FUNCTION, {not_equal, 0, 255}},
            {WEBGL_MASK_STENCIL_OPERATION, {keep, keep, keep}},
            {WEBGL_MASK_BIND_XY, {1}},
            {WEBGL_MASK_SHADE, {batch->colour}},
            {WEBGL_MASK_DRAW, {triangles, 0, 6}},
            {WEBGL_MASK_CLEAR, {stencil_bit}}
        };
        for (size_t j = 0; j < sizeof pass / sizeof *pass; ++j) commands[next++] = pass[j];
    }
    return next;
}
