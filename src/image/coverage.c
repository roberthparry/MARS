/**
 * @file coverage.c
 * @brief Exact sequential alpha coverage using compact row bitsets.
 *
 * Records only fully opaque pixels and tests intersecting words rather than scanning unrelated image pixels.
 * Dimensions and storage are caller-selected; no fixed photographic tile size or browser state is assumed.
 */
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MARS_IMAGE_INTERNAL_ACCESS
#include "image_internal.h"

struct _image_coverage_t {
    unsigned width, height, rows, words;
    uint64_t *bits;
};

/* Allocate only the compact alpha bitset; no colour pixels are retained. */
image_coverage_t *image_coverage_new(unsigned width, unsigned height)
{
    size_t size;
    if (!image_storage_size(width, height, &size)) return NULL;
    image_coverage_t *c = calloc(1, sizeof *c);
    if (!c) return NULL;
    c->width = width; c->height = height; c->words = (width + 63) / 64;
    c->bits = calloc((size_t)c->words * height, sizeof *c->bits);
    if (!c->bits) { free(c); return NULL; }
    return c;
}

/* Release compact coverage storage. */
void image_coverage_free(image_coverage_t *coverage)
{
    if (coverage) { free(coverage->bits); free(coverage); }
}

/* Replace contiguous rows, invalidating incomplete or malformed passes conservatively. */
bool image_coverage_write(image_coverage_t *c, unsigned row, const void *pixels, unsigned rows, size_t stride)
{
    if (!c) return false;
    if (!row) c->rows = 0;
    if (!pixels && !row && !rows) return true;
    if (!pixels || row != c->rows || !rows || rows > c->height - c->rows || stride < (size_t)c->width * 4 ||
        stride > SIZE_MAX / rows) { c->rows = 0; return false; }
    memset(c->bits + (size_t)row * c->words, 0, (size_t)rows * c->words * sizeof *c->bits);
    for (unsigned y = 0; y < rows; ++y) {
        const unsigned char *input = (const unsigned char *)pixels + y * stride;
        for (unsigned x = 0; x < c->width; ++x)
            if (input[x * 4 + 3] == 255)
                c->bits[(size_t)(row + y) * c->words + x / 64] |= UINT64_C(1) << (x % 64);
    }
    c->rows += rows;
    return true;
}

/* Query exact touched pixels only after the entire image has been recorded. */
bool image_coverage_contains(const image_coverage_t *c, const image_rectangle_t *r)
{
    if (!c || c->rows != c->height || !r || !isfinite(r->x) || !isfinite(r->y) || !isfinite(r->width) ||
        !isfinite(r->height) || r->x < 0 || r->y < 0 || r->width <= 0 || r->height <= 0 ||
        r->x >= c->width || r->y >= c->height || r->x + r->width > c->width + .000001 ||
        r->y + r->height > c->height + .000001) return false;
    unsigned left = (unsigned)floor(r->x), top = (unsigned)floor(r->y);
    unsigned right = (unsigned)fmin(c->width, ceil(r->x + r->width));
    unsigned bottom = (unsigned)fmin(c->height, ceil(r->y + r->height));
    for (unsigned word = left / 64; word <= (right - 1) / 64; ++word) {
        unsigned first = word == left / 64 ? left % 64 : 0;
        unsigned last = word == (right - 1) / 64 ? (right - 1) % 64 + 1 : 64;
        uint64_t mask = (UINT64_MAX << first) & (last == 64 ? UINT64_MAX : (UINT64_C(1) << last) - 1);
        for (unsigned row = top; row < bottom; ++row)
            if ((c->bits[(size_t)row * c->words + word] & mask) != mask) return false;
    }
    return true;
}
