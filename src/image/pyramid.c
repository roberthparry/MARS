/**
 * @file pyramid.c
 * @brief Immutable rectangular image pyramids, pixel-demand selection and exact gutter mappings.
 *
 * Plans only intersecting block addresses using arithmetic row/column bounds. At most sixteen resolution tiers
 * are retained, with no image pixels, browser objects, geography, paths or cache implementation in the planner.
 */
#include <math.h>
#include <stdlib.h>

#include "image-t.h"

struct _image_pyramid_t {
    unsigned width, height, core, gutter, count;
    image_level_t levels[16];
};

static bool rectangle_valid(const image_rectangle_t *r)
{
    return r && isfinite(r->x) && isfinite(r->y) && isfinite(r->width) && isfinite(r->height) &&
           r->width > 0 && r->height > 0 && isfinite(r->x + r->width) && isfinite(r->y + r->height);
}

static bool crop_valid(const image_pyramid_t *p, const image_rectangle_t *s, const image_rectangle_t *t)
{
    return p && rectangle_valid(s) && rectangle_valid(t) && s->x >= 0 && s->y >= 0 &&
           s->x < p->width && s->y < p->height &&
           s->x + s->width <= p->width + .000001 && s->y + s->height <= p->height + .000001;
}

/* Construct metadata only, preserving original aspect ratio at every tier. */
image_pyramid_t *image_pyramid_new(unsigned width, unsigned height, unsigned core, unsigned gutter, unsigned minimum)
{
    unsigned longest = width > height ? width : height;
    if (!width || !height || width > 32768 || height > 32768 || !core || core > 32768 ||
        gutter > (32768 - core) / 2 || !minimum || minimum > longest) return NULL;
    image_pyramid_t *p = calloc(1, sizeof *p);
    if (!p) return NULL;
    p->width = width; p->height = height; p->core = core; p->gutter = gutter;
    for (unsigned edge = longest;;) {
        image_level_t *level = p->levels + p->count++;
        level->edge = edge;
        level->width = (unsigned)ceil((double)width * edge / longest);
        level->height = (unsigned)ceil((double)height * edge / longest);
        level->columns = (level->width + core - 1) / core;
        level->rows = (level->height + core - 1) / core;
        if (edge == minimum) break;
        unsigned next = 1;
        while (next * 2 < edge) next *= 2;
        edge = next < minimum ? minimum : next;
    }
    return p;
}

/* Release one immutable geometry object. */
void image_pyramid_free(image_pyramid_t *pyramid) { free(pyramid); }

/* Read the bounded tier count. */
unsigned image_pyramid_levels(const image_pyramid_t *pyramid) { return pyramid ? pyramid->count : 0; }

/* Read a tier by direct index rather than searching its blocks. */
bool image_pyramid_level(const image_pyramid_t *pyramid, unsigned index, image_level_t *level)
{
    if (!pyramid || !level || index >= pyramid->count) return false;
    *level = pyramid->levels[index];
    return true;
}

static const image_level_t *level_at(const image_pyramid_t *p, unsigned tier)
{
    unsigned left = 0, right = p->count;
    while (left < right) {
        unsigned at = left + (right - left) / 2, edge = p->levels[at].edge;
        if (edge == tier) return p->levels + at;
        if (edge > tier) left = at + 1;
        else right = at;
    }
    return NULL;
}

/* Scale an original-coordinate crop without exposing pyramid internals to adapters. */
bool image_pyramid_crop(const image_pyramid_t *p, unsigned tier, const image_rectangle_t *source,
                        image_rectangle_t *decoded)
{
    if (!decoded || !crop_valid(p, source, source)) return false;
    const image_level_t *level = level_at(p, tier);
    if (!level) return false;
    double sx = (double)level->width / p->width, sy = (double)level->height / p->height;
    *decoded = (image_rectangle_t){source->x * sx, source->y * sy, source->width * sx, source->height * sy};
    return true;
}

/* Enumerate the rectangular crop directly, including fractional destination mappings. */
bool image_pyramid_visit(const image_pyramid_t *p, unsigned tier, const image_rectangle_t *source,
                         const image_rectangle_t *target, image_block_visit_fn visitor, void *context)
{
    if (!visitor || !crop_valid(p, source, target)) return false;
    const image_level_t *level = level_at(p, tier);
    if (!level) return false;
    image_rectangle_t s;
    if (!image_pyramid_crop(p, tier, source, &s)) return false;
    unsigned first_x = (unsigned)floor(s.x / p->core), first_y = (unsigned)floor(s.y / p->core);
    unsigned last_x = (unsigned)fmin(level->columns, ceil((s.x + s.width) / p->core));
    unsigned last_y = (unsigned)fmin(level->rows, ceil((s.y + s.height) / p->core));
    for (unsigned row = first_y; row < last_y; ++row) {
        for (unsigned column = first_x; column < last_x; ++column) {
            double x = column * p->core, y = row * p->core;
            double left = fmax(x, s.x), top = fmax(y, s.y);
            double right = fmin(fmin(level->width, x + p->core), s.x + s.width);
            double bottom = fmin(fmin(level->height, y + p->core), s.y + s.height);
            if (right <= left || bottom <= top) continue;
            double rx = fmax(0, x - p->gutter), ry = fmax(0, y - p->gutter);
            double rw = fmin(level->width, x + p->core + p->gutter) - rx;
            double rh = fmin(level->height, y + p->core + p->gutter) - ry;
            image_block_t block = {
                .tier = tier, .row = row, .column = column,
                .read = {rx, ry, rw, rh},
                .write = {rx - x + p->gutter, ry - y + p->gutter, rw, rh},
                .source = {left - x + p->gutter, top - y + p->gutter, right - left, bottom - top},
                .target = {target->x + (left - s.x) * target->width / s.width,
                           target->y + (top - s.y) * target->height / s.height,
                           (right - left) * target->width / s.width, (bottom - top) * target->height / s.height}
            };
            if (!visitor(&block, context)) return false;
        }
    }
    return true;
}

struct cache_probe {
    image_pyramid_cached_fn cached;
    void *context;
};

static bool probe_block(const image_block_t *block, void *context)
{
    struct cache_probe *probe = context;
    return probe->cached(block->tier, block, probe->context);
}

/* Select by pixel demand, then test only adequate cached tiers and their intersecting blocks. */
bool image_pyramid_select(const image_pyramid_t *p, const image_rectangle_t *source,
                          const image_rectangle_t *target, image_pyramid_cached_fn cached, void *context,
                          image_level_t *level, bool *reused)
{
    if (!level || !crop_valid(p, source, target)) return false;
    double demand = fmax(target->width / source->width, target->height / source->height);
    unsigned selected = 0;
    for (unsigned i = 1; i < p->count; ++i) {
        const image_level_t *next = p->levels + i;
        if ((double)next->width / p->width < demand || (double)next->height / p->height < demand) break;
        selected = i;
    }
    bool hit = false;
    struct cache_probe probe = {cached, context};
    for (unsigned i = selected + 1; cached && i > 0; --i) {
        unsigned edge = p->levels[i - 1].edge;
        if (cached(edge, NULL, context) || image_pyramid_visit(p, edge, source, target, probe_block, &probe)) {
            selected = i - 1; hit = true; break;
        }
    }
    *level = p->levels[selected];
    if (reused) *reused = hit;
    return true;
}

/* Intersect positive finite rectangles without geographic or viewport assumptions. */
bool image_rectangle_intersection(const image_rectangle_t *a, const image_rectangle_t *b, image_rectangle_t *out)
{
    if (!out || !rectangle_valid(a) || !rectangle_valid(b)) return false;
    double left = fmax(a->x, b->x), top = fmax(a->y, b->y);
    double right = fmin(a->x + a->width, b->x + b->width), bottom = fmin(a->y + a->height, b->y + b->height);
    if (right <= left || bottom <= top) return false;
    *out = (image_rectangle_t){left, top, right - left, bottom - top};
    return true;
}
