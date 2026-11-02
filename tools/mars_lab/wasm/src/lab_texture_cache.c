/**
 * @file lab_texture_cache.c
 * @brief Dictionary-backed photographic cache policy, stable windows and bounded pixel preparation.
 *
 * Browser resources remain in host slots, while native dictionaries own keys and select reusable slots.
 * Each cache has a linked LRU order, a free-slot stack and a byte budget. Legacy windows inspect four GPU entries;
 * fixed photographic pages use direct dictionary lookups and pin visible and arrival ancestors among at most 64 slots.
 * Serial dispatch protects native state; worker pixel batches use a fixed 512 KiB scratch buffer and never block drawing.
 */
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "dictionary-t.h"
#include "lab_dom.h"
#include "lab_serial.h"
#include "lab_texture_cache.h"

typedef struct _texture_entry_t {
    char key[256];
    double bounds[4], zoom;
    unsigned bytes, quality;
    int previous, next;
    bool used, geographic;
} texture_entry_t;

typedef struct _texture_cache_t {
    dictionary_t *keys;
    texture_entry_t entries[64];
    unsigned free_slots[64], free_count;
    unsigned char page[16 * 16 * 4];
    unsigned token, limit, budget, bytes, journey_turn;
    int oldest, newest;
} texture_cache_t;

typedef struct _texture_service_t {
    lab_serial_t serial;
    dictionary_t *caches;
    unsigned next_token;
    unsigned char pixels[2048 * 64 * 4];
} texture_service_t;

static texture_service_t *texture_service(void)
{
    static texture_service_t state = {.serial = LAB_SERIAL_INITIALIZER};
    return &state;
}

static size_t text_hash(const void *key)
{
    const unsigned char *p = key;
    size_t hash = 2166136261u;
    while (*p) hash = (hash ^ *p++) * 16777619u;
    return hash;
}

static int text_compare(const void *a, const void *b) { return strcmp(a, b); }
static size_t token_hash(const void *key) { return *(const unsigned *)key; }
static int token_compare(const void *a, const void *b)
{
    unsigned x = *(const unsigned *)a, y = *(const unsigned *)b;
    return (x > y) - (x < y);
}

static double field(jsref request, const char *name)
{
    return lab_dom_to_number(lab_dom_get(request, name));
}

static bool bounds_read(jsref value, double *bounds)
{
    if (lab_dom_count(value) != 4) return false;
    for (unsigned i = 0; i < 4; ++i) {
        bounds[i] = lab_dom_to_number(lab_dom_item(value, i));
        if (!isfinite(bounds[i])) return false;
    }
    return bounds[2] > bounds[0] && bounds[3] > bounds[1];
}

static jsref bounds_result(const double *bounds)
{
    jsref result = lab_dom_object(LAB_VALUE_ARRAY);
    for (unsigned i = 0; i < 4; ++i) lab_dom_push(result, lab_dom_numeric(bounds[i]));
    return result;
}

static void unlink_entry(texture_cache_t *cache, int slot)
{
    texture_entry_t *entry = &cache->entries[slot];
    if (entry->previous >= 0) cache->entries[entry->previous].next = entry->next;
    else cache->oldest = entry->next;
    if (entry->next >= 0) cache->entries[entry->next].previous = entry->previous;
    else cache->newest = entry->previous;
}

static void touch(texture_cache_t *cache, int slot, bool linked)
{
    if (linked) unlink_entry(cache, slot);
    texture_entry_t *entry = &cache->entries[slot];
    entry->previous = cache->newest; entry->next = -1;
    if (cache->newest >= 0) cache->entries[cache->newest].next = slot;
    else cache->oldest = slot;
    cache->newest = slot;
}

static jsref result(texture_cache_t *cache, int slot, jsref evicted)
{
    jsref object = lab_dom_object(LAB_VALUE_OBJECT);
    lab_dom_set(object, "token", lab_dom_numeric(cache->token));
    lab_dom_set(object, "slot", lab_dom_numeric(slot));
    lab_dom_set(object, "count", lab_dom_numeric(dictionary_size(cache->keys)));
    lab_dom_set(object, "bytes", lab_dom_numeric(cache->bytes));
    if (evicted) lab_dom_set(object, "evicted", evicted);
    if (slot >= 0 && cache->entries[slot].geographic) {
        lab_dom_set(object, "bounds", bounds_result(cache->entries[slot].bounds));
        lab_dom_set(object, "zoom", lab_dom_numeric(cache->entries[slot].zoom));
    }
    return object;
}

static jsref create_cache(texture_cache_t *unused, jsref request)
{
    (void)unused;
    texture_service_t *state = texture_service();
    double limit = field(request, "limit"), budget = field(request, "bytes");
    if (!isfinite(limit) || limit < 1 || limit > 64 || floor(limit) != limit ||
        !isfinite(budget) || budget < 1 || budget > 67108864 || floor(budget) != budget ||
        (state->caches && dictionary_size(state->caches) >= 16)) return 0;
    texture_cache_t *cache = calloc(1, sizeof *cache);
    if (!cache) return 0;
    cache->limit = (unsigned)limit; cache->budget = (unsigned)budget;
    cache->free_count = cache->limit;
    for (unsigned i = 0; i < cache->limit; ++i) cache->free_slots[i] = cache->limit - i - 1;
    cache->oldest = cache->newest = -1;
    cache->keys = dictionary_create(256, sizeof(int), text_hash, text_compare, NULL, NULL, NULL, NULL, NULL);
    if (!state->caches) state->caches = dictionary_create(sizeof(unsigned), sizeof(texture_cache_t *),
        token_hash, token_compare, NULL, NULL, NULL, NULL, NULL);
    cache->token = ++state->next_token;
    if (!cache->token || !cache->keys || !state->caches || !dictionary_set(state->caches, &cache->token, &cache)) {
        dictionary_destroy(cache->keys); free(cache); return 0;
    }
    return result(cache, -1, 0);
}

static bool read_key(jsref request, char *key)
{
    memset(key, 0, 256);
    int count = lab_dom_utf8_copy(lab_dom_get(request, "key"), (unsigned char *)key, 255);
    return count > 0 && strlen(key) == (unsigned)count;
}

static jsref get_entry(texture_cache_t *cache, jsref request)
{
    char key[256]; int slot;
    if (!read_key(request, key) || !dictionary_get(cache->keys, key, &slot)) return 0;
    touch(cache, slot, true);
    return result(cache, slot, 0);
}

static uint64_t protected_slots(jsref request)
{
    jsref protect = lab_dom_get(request, "protect");
    if (lab_dom_type(protect) != LAB_VALUE_ARRAY) {
        double slot = field(request, "protect");
        return isfinite(slot) && slot >= 0 && slot < 64 && floor(slot) == slot ? UINT64_C(1) << (unsigned)slot : 0;
    }
    int count = lab_dom_count(protect);
    if (count < 0 || count > 64) return UINT64_MAX;
    uint64_t slots = 0;
    for (int i = 0; i < count; ++i) {
        double slot = lab_dom_to_number(lab_dom_item(protect, i));
        if (isfinite(slot) && slot >= 0 && slot < 64 && floor(slot) == slot) slots |= UINT64_C(1) << (unsigned)slot;
    }
    return slots;
}

static jsref put_entry(texture_cache_t *cache, jsref request)
{
    char key[256]; int slot = -1;
    double bytes = field(request, "bytes"), zoom = field(request, "zoom"), quality = field(request, "quality");
    if (!read_key(request, key) || !isfinite(bytes) || bytes < 1 || bytes > cache->budget || floor(bytes) != bytes) return 0;
    if (dictionary_get(cache->keys, key, &slot)) {
        if (isfinite(zoom)) cache->entries[slot].zoom = fmax(cache->entries[slot].zoom, zoom);
        touch(cache, slot, true);
        return result(cache, slot, 0);
    }
    jsref evicted = lab_dom_object(LAB_VALUE_ARRAY);
    uint64_t pinned = protected_slots(request);
    while (dictionary_size(cache->keys) >= cache->limit || cache->bytes + (unsigned)bytes > cache->budget) {
        int victim = cache->oldest;
        while (victim >= 0 && (pinned & (UINT64_C(1) << victim))) victim = cache->entries[victim].next;
        if (victim < 0) return result(cache, -1, evicted);
        texture_entry_t *entry = &cache->entries[victim];
        dictionary_remove(cache->keys, entry->key);
        unlink_entry(cache, victim); cache->bytes -= entry->bytes; entry->used = false;
        cache->free_slots[cache->free_count++] = (unsigned)victim;
        lab_dom_push(evicted, lab_dom_numeric(victim));
    }
    if (!cache->free_count) return result(cache, -1, evicted);
    slot = (int)cache->free_slots[cache->free_count - 1];
    if (!dictionary_set(cache->keys, key, &slot)) return result(cache, -1, evicted);
    --cache->free_count;
    texture_entry_t *entry = &cache->entries[slot];
    memset(entry, 0, sizeof *entry); memcpy(entry->key, key, sizeof key);
    entry->bytes = (unsigned)bytes; entry->used = true; entry->zoom = isfinite(zoom) ? zoom : 0;
    entry->quality = isfinite(quality) && quality >= 0 && quality <= 2 ? (unsigned)quality : 0;
    entry->geographic = bounds_read(lab_dom_get(request, "bounds"), entry->bounds);
    cache->bytes += entry->bytes;
    touch(cache, slot, false);
    return result(cache, slot, evicted);
}

static jsref cover_entry(texture_cache_t *cache, jsref request)
{
    double bounds[4], zoom = field(request, "zoom");
    if (cache->limit > 4 || !isfinite(zoom) || zoom <= 0 || !bounds_read(lab_dom_get(request, "bounds"), bounds)) return 0;
    bool partial = lab_dom_truth(lab_dom_get(request, "partial"));
    double prefer = field(request, "prefer");
    int selected = -1;
    double best_area = -1;
    /* Coverage is a spatial predicate over at most four resident GPU textures. */
    for (int slot = cache->newest; slot >= 0; slot = cache->entries[slot].previous) {
        texture_entry_t *entry = &cache->entries[slot];
        double *b = entry->bounds;
        if (!entry->geographic) continue;
        double centre = (bounds[0] + bounds[2] - b[0] - b[2]) * .5;
        double shift = 360 * nearbyint(centre / 360);
        double west = b[0] + shift, east = b[2] + shift;
        bool contains = bounds[0] >= west && bounds[1] >= b[1] && bounds[2] <= east && bounds[3] <= b[3];
        double area = fmax(0, fmin(bounds[2], east) - fmax(bounds[0], west)) *
                      fmax(0, fmin(bounds[3], b[3]) - fmax(bounds[1], b[1]));
        if (partial ? area <= 0 : !contains || zoom > entry->zoom * 1.5) continue;
        if (selected < 0 || entry->quality > cache->entries[selected].quality ||
            (entry->quality == cache->entries[selected].quality &&
             (partial ? area > best_area : slot == prefer))) {
            selected = slot;
            best_area = area;
        }
    }
    if (selected >= 0) touch(cache, selected, true);
    return selected >= 0 ? result(cache, selected, 0) : 0;
}

static jsref clear_cache(texture_cache_t *cache, jsref request)
{
    (void)request;
    dictionary_remove(texture_service()->caches, &cache->token);
    dictionary_destroy(cache->keys); free(cache);
    return 0;
}

static jsref cache_stats(texture_cache_t *cache, jsref request)
{
    (void)request;
    return result(cache, -1, 0);
}

static jsref remove_entry(texture_cache_t *cache, jsref request)
{
    char key[256];
    int slot;
    if (!read_key(request, key) || !dictionary_get(cache->keys, key, &slot)) return 0;
    texture_entry_t *entry = &cache->entries[slot];
    dictionary_remove(cache->keys, key); unlink_entry(cache, slot);
    cache->bytes -= entry->bytes; entry->used = false;
    cache->free_slots[cache->free_count++] = (unsigned)slot;
    jsref evicted = lab_dom_object(LAB_VALUE_ARRAY);
    lab_dom_push(evicted, lab_dom_numeric(slot));
    return result(cache, -1, evicted);
}

static char *photo_integer(char *out, unsigned value)
{
    char digits[10];
    unsigned count = 0;
    do { digits[count++] = (char)('0' + value % 10); value /= 10; } while (value);
    while (count) *out++ = digits[--count];
    *out++ = ':'; *out = 0;
    return out;
}

static void photo_key(char key[256], unsigned level, int x, unsigned y)
{
    int columns = 1 << level;
    unsigned column = (unsigned)((x % columns + columns) % columns);
    memcpy(key, "photo:", 7);
    char *out = photo_integer(key + 6, level);
    out = photo_integer(out, column);
    photo_integer(out, y);
}

static int photo_slot(texture_cache_t *cache, unsigned level, int x, unsigned y)
{
    char key[256] = {0};
    int slot = -1;
    photo_key(key, level, x, y);
    return dictionary_get(cache->keys, key, &slot) ? slot : -1;
}

typedef struct _photo_plan_t {
    unsigned char page[16 * 16 * 4];
    double bounds[4], span, coverage, detail;
    uint64_t pinned;
    unsigned next_level;
    int next_x, next_y;
    bool base;
} photo_plan_t;

/* A fixed 256-cell grid uses indexed ancestors; broad coverage precedes detail, with no resource-list scan. */
static bool photo_plan(texture_cache_t *cache, jsref bounds, double pixels, const char *avoid, photo_plan_t *plan)
{
    double view[4];
    memset(plan, 0, sizeof *plan);
    plan->next_y = -1;
    if (!bounds_read(bounds, view) || !isfinite(pixels) || pixels < 1 || pixels > 4096 ||
        view[0] < -360 || view[2] > 360 || view[1] < -90 || view[3] > 90) return false;
    double extent = fmax(view[2] - view[0], view[3] - view[1]);
    if (extent > 30 || extent < .000001) return false;
    unsigned level = (unsigned)fmax(4, fmin(18, floor(log2(360 * fmin(pixels, 1536) / (extent * 512)))));
    unsigned base = level > 6 ? level - 2 : 4;
    double step = 360.0 / (1u << level);
    unsigned fine = level + (level < 18) + (level < 17);
    double cell = 360.0 / (1u << fine);
    int left = (int)floor(((view[0] + view[2]) * .5 + 180) / cell) - 8;
    int top = (int)floor((90 - (view[1] + view[3]) * .5) / cell) - 8;
    int rows = 1 << (fine - 1);
    top = (int)fmax(0, fmin(rows - 16, top));
    plan->bounds[0] = left * cell - 180; plan->bounds[1] = 90 - (top + 16) * cell;
    plan->bounds[2] = (left + 16) * cell - 180; plan->bounds[3] = 90 - top * cell;
    plan->span = cell;
    double best = INFINITY, visible_area = 0, covered_area = 0, detail_area = 0;
    for (unsigned y = 0; y < 16; ++y) {
        for (unsigned x = 0; x < 16; ++x) {
            int column = left + (int)x, row = top + (int)y;
            double west = column * cell - 180, north = 90 - row * cell;
            bool visible = west < view[2] && west + cell > view[0] && north > view[1] && north - cell < view[3];
            bool found = false, covered = false;
            unsigned shown_level = 0;
            for (unsigned ancestor = fine; ancestor >= 1; --ancestor) {
                unsigned shift = fine - ancestor;
                int ax = (int)floor((double)column / (1u << shift));
                int slot = photo_slot(cache, ancestor, ax, (unsigned)row >> shift);
                if (slot < 0) continue;
                if (visible && (!found || ancestor == base)) {
                    plan->pinned |= UINT64_C(1) << slot;
                    touch(cache, slot, true);
                }
                if (!found) {
                    plan->page[4 * (y * 16 + x)] = (unsigned char)(slot + 1);
                    plan->page[4 * (y * 16 + x) + 1] = (unsigned char)shift;
                    found = true;
                    shown_level = ancestor;
                    covered = ancestor >= base;
                }
                if (ancestor <= base) break;
            }
            if (visible) {
                double area = (fmin(west + cell, view[2]) - fmax(west, view[0])) *
                              (fmin(north, view[3]) - fmax(north - cell, view[1]));
                visible_area += area;
                if (covered) covered_area += area;
                unsigned reduction = shown_level < level ? level - shown_level : 0;
                if (found) detail_area += area / (1u << reduction);
            }
            bool missing_base = visible && !covered;
            unsigned wanted = missing_base ? base : level, shift = fine - wanted;
            double wanted_step = missing_base ? 360.0 / (1u << base) : step;
            int tx = (int)floor((double)column / (1u << shift)), ty = row >> shift;
            double tile_west = tx * wanted_step - 180, tile_north = 90 - ty * wanted_step;
            if (tile_west > view[2] + wanted_step * .5 || tile_west + wanted_step < view[0] - wanted_step * .5 ||
                tile_north < view[1] - wanted_step * .5 || tile_north - wanted_step > view[3] + wanted_step * .5) continue;
            if (photo_slot(cache, wanted, tx, (unsigned)ty) >= 0) continue;
            if (wanted < 18 && photo_slot(cache, wanted + 1, tx * 2, (unsigned)ty * 2) >= 0 &&
                photo_slot(cache, wanted + 1, tx * 2 + 1, (unsigned)ty * 2) >= 0 &&
                photo_slot(cache, wanted + 1, tx * 2, (unsigned)ty * 2 + 1) >= 0 &&
                photo_slot(cache, wanted + 1, tx * 2 + 1, (unsigned)ty * 2 + 1) >= 0) continue;
            char key[256] = {0};
            photo_key(key, wanted, tx, (unsigned)ty);
            if (*avoid && !strcmp(key, avoid)) continue;
            double dx = ((tx + .5) * wanted_step - 180 - (view[0] + view[2]) * .5) / wanted_step;
            double dy = (90 - (ty + .5) * wanted_step - (view[1] + view[3]) * .5) / wanted_step;
            double score = dx * dx + dy * dy + (visible ? 0 : 100) + (missing_base ? 0 : 1000);
            if (score < best) {
                best = score; plan->next_x = tx; plan->next_y = ty;
                plan->next_level = wanted; plan->base = missing_base;
            }
        }
    }
    plan->coverage = visible_area > 0 ? fmax(0, fmin(1, covered_area / visible_area)) : 1;
    plan->detail = visible_area > 0 ? fmax(0, fmin(1, detail_area / visible_area)) : 1;
    return true;
}

static jsref photo_result(texture_cache_t *cache, const photo_plan_t *visible, const photo_plan_t *next,
                          uint64_t pinned, double pace)
{
    if (visible) memcpy(cache->page, visible->page, sizeof cache->page);
    else memset(cache->page, 0, sizeof cache->page);
    jsref object = lab_dom_object(LAB_VALUE_OBJECT), protect = lab_dom_object(LAB_VALUE_ARRAY);
    const double fallback[] = {-180, -90, 180, 90};
    lab_dom_set(object, "visible", lab_dom_scalar(LAB_VALUE_BOOLEAN, visible != NULL));
    lab_dom_set(object, "bounds", bounds_result(visible ? visible->bounds : fallback));
    lab_dom_set(object, "span", lab_dom_numeric(visible ? visible->span : 360));
    lab_dom_set(object, "pace", lab_dom_numeric(pace));
    lab_dom_set(object, "coverage", lab_dom_numeric(visible ? visible->coverage : 1));
    lab_dom_set(object, "pointer", lab_dom_numeric((unsigned)(size_t)cache->page));
    for (unsigned slot = 0; slot < cache->limit; ++slot)
        if (pinned & (UINT64_C(1) << slot)) lab_dom_push(protect, lab_dom_numeric(slot));
    lab_dom_set(object, "protect", protect);
    if (next && next->next_y >= 0) {
        char key[256] = {0};
        unsigned level = next->next_level;
        photo_key(key, level, next->next_x, (unsigned)next->next_y);
        int columns = 1 << level, wrapped = (next->next_x % columns + columns) % columns;
        double step = 360.0 / columns;
        double b[] = {wrapped * step - 180, 90 - (next->next_y + 1) * step,
                      (wrapped + 1) * step - 180, 90 - next->next_y * step};
        jsref cell = lab_dom_object(LAB_VALUE_OBJECT);
        lab_dom_set(cell, "key", lab_dom_string(key));
        lab_dom_set(cell, "bounds", bounds_result(b));
        lab_dom_set(cell, "level", lab_dom_numeric(level));
        lab_dom_set(cell, "coverage", lab_dom_scalar(LAB_VALUE_BOOLEAN, next->base));
        lab_dom_set(object, "next", cell);
    }
    return object;
}

static void photo_avoid(jsref request, char avoid[64])
{
    memset(avoid, 0, 64);
    lab_dom_utf8_copy(lab_dom_get(request, "avoid"), (unsigned char *)avoid, 63);
}

static jsref photo_page(texture_cache_t *cache, jsref request)
{
    photo_plan_t plan;
    char avoid[64];
    photo_avoid(request, avoid);
    if (!photo_plan(cache, lab_dom_get(request, "bounds"), field(request, "pixels"), avoid, &plan)) return 0;
    return photo_result(cache, &plan, &plan, plan.pinned | protected_slots(request), 1);
}

/* Three bounded spatial plans share one native scheduling decision; the host submits only that selected job. */
static jsref photo_journey(texture_cache_t *cache, jsref request)
{
    photo_plan_t visible, ahead, arrival;
    char avoid[64];
    photo_avoid(request, avoid);
    bool display = photo_plan(cache, lab_dom_get(request, "bounds"), field(request, "pixels"), avoid, &visible);
    bool predicted = photo_plan(cache, lab_dom_get(request, "ahead"), field(request, "pixels"), avoid, &ahead);
    bool destination = photo_plan(cache, lab_dom_get(request, "arrival"), field(request, "arrival_pixels"), avoid, &arrival);
    const photo_plan_t *next = NULL;
    if (display && visible.base) next = &visible;
    else if (predicted && ahead.base) next = &ahead;
    else if (destination && arrival.base) next = &arrival;
    else {
        bool prefer_arrival = cache->journey_turn % 2;
        if (prefer_arrival && destination && arrival.next_y >= 0) next = &arrival;
        else if (display && visible.next_y >= 0) next = &visible;
        else if (predicted && ahead.next_y >= 0) next = &ahead;
        else if (destination && arrival.next_y >= 0) next = &arrival;
    }
    if (next && lab_dom_truth(lab_dom_get(request, "idle"))) ++cache->journey_turn;
    uint64_t pinned = (display ? visible.pinned : 0) | (destination ? arrival.pinned : 0);
    return photo_result(cache, display ? &visible : NULL, next, pinned, display ? .3 + .7 * visible.detail : 1);
}

static void cache_dispatch(void *context)
{
    jsref request = *(jsref *)context;
    static jsref (*const operations[])(texture_cache_t *, jsref) = {
        create_cache, get_entry, put_entry, cover_entry, clear_cache, cache_stats, photo_page, remove_entry, photo_journey
    };
    double action = field(request, "action"), token = field(request, "token");
    if (!isfinite(action) || action < 0 || action >= sizeof operations / sizeof *operations || floor(action) != action) {
        lab_dom_return(0); return;
    }
    texture_cache_t *cache = NULL;
    if (action != 0) {
        if (!isfinite(token) || token < 1 || token > 4294967295.0 || floor(token) != token) { lab_dom_return(0); return; }
        unsigned id = (unsigned)token;
        if (!texture_service()->caches || !dictionary_get(texture_service()->caches, &id, &cache)) { lab_dom_return(0); return; }
    }
    lab_dom_return(operations[(unsigned)action](cache, request));
}

/* Keep complete dictionary mutations within the owning service's dispatch boundary. */
void lab_texture_cache(jsref request)
{
    if (!lab_serial_dispatch(&texture_service()->serial, cache_dispatch, &request)) lab_dom_return(0);
}

/* Quantise padded presentation windows, never the source geography. */
void lab_globe_surface_bounds(jsref value)
{
    double b[4], original[4];
    if (!bounds_read(value, b)) { lab_dom_return(0); return; }
    memcpy(original, b, sizeof b);
    for (unsigned axis = 0; axis < 2; ++axis) {
        double span = b[axis + 2] - b[axis], width = fmax(1e-9, pow(2, ceil(2 * log2(span / .875)) / 2));
        if (width > 30) { lab_dom_return(bounds_result(original)); return; }
        double step = width / 8, centre = round((b[axis] + b[axis + 2]) / (2 * step)) * step;
        double minimum = axis ? -90 : -360, maximum = axis ? 90 : 360;
        b[axis] = fmax(minimum, fmin(maximum - width, centre - width / 2));
        b[axis + 2] = b[axis] + width;
    }
    lab_dom_return(bounds_result(b));
}

typedef struct _texture_pixels_t { jsref bytes; unsigned pointer; } texture_pixels_t;

static void pixels_dispatch(void *context)
{
    texture_pixels_t *request = context;
    unsigned char *pixels = texture_service()->pixels;
    int count = lab_dom_bytes_copy(request->bytes, pixels, sizeof texture_service()->pixels);
    if (count < 0 || count % 4) return;
    for (int i = 0; i < count; i += 4)
        for (int channel = 0; channel < 3; ++channel)
            pixels[i + channel] = (unsigned char)((pixels[i + channel] * pixels[i + 3] + 127) / 255);
    request->pointer = (unsigned)(size_t)pixels;
}

/* Process one small batch synchronously; the worker yields between batches and copies the result immediately. */
unsigned lab_texture_premultiply(jsref bytes)
{
    texture_pixels_t request = {.bytes = bytes};
    lab_serial_dispatch(&texture_service()->serial, pixels_dispatch, &request);
    return request.pointer;
}
