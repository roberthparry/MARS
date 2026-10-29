/**
 * @file lab_texture_cache.c
 * @brief Dictionary-backed photographic cache policy, stable windows and bounded pixel preparation.
 *
 * Browser resources remain in host slots, while native dictionaries own keys and select reusable slots.
 * Each cache has a linked LRU order and a byte budget. Geographic coverage examines at most four GPU entries.
 * Serial dispatch protects native state; worker pixel batches use a fixed 512 KiB scratch buffer and never block drawing.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "dictionary-t.h"
#include "lab_dom.h"
#include "lab_serial.h"
#include "lab_texture_cache.h"

typedef struct _texture_entry_t {
    char key[256];
    double bounds[4], zoom;
    unsigned bytes;
    int previous, next;
    bool used, geographic;
} texture_entry_t;

typedef struct _texture_cache_t {
    dictionary_t *keys;
    texture_entry_t entries[32];
    unsigned token, limit, budget, bytes;
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
    if (!isfinite(limit) || limit < 1 || limit > 32 || floor(limit) != limit ||
        !isfinite(budget) || budget < 1 || budget > 67108864 || floor(budget) != budget ||
        (state->caches && dictionary_size(state->caches) >= 16)) return 0;
    texture_cache_t *cache = calloc(1, sizeof *cache);
    if (!cache) return 0;
    cache->limit = (unsigned)limit; cache->budget = (unsigned)budget;
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

static jsref put_entry(texture_cache_t *cache, jsref request)
{
    char key[256]; int slot = -1;
    double bytes = field(request, "bytes"), zoom = field(request, "zoom"), protect = field(request, "protect");
    if (!read_key(request, key) || !isfinite(bytes) || bytes < 1 || bytes > cache->budget || floor(bytes) != bytes) return 0;
    if (dictionary_get(cache->keys, key, &slot)) {
        if (isfinite(zoom)) cache->entries[slot].zoom = fmax(cache->entries[slot].zoom, zoom);
        touch(cache, slot, true);
        return result(cache, slot, 0);
    }
    jsref evicted = lab_dom_object(LAB_VALUE_ARRAY);
    while (dictionary_size(cache->keys) >= cache->limit || cache->bytes + (unsigned)bytes > cache->budget) {
        int victim = cache->oldest;
        if (victim == protect) victim = cache->entries[victim].next;
        if (victim < 0) return 0;
        texture_entry_t *entry = &cache->entries[victim];
        dictionary_remove(cache->keys, entry->key);
        unlink_entry(cache, victim); cache->bytes -= entry->bytes; entry->used = false;
        lab_dom_push(evicted, lab_dom_numeric(victim));
    }
    /* At most 32 native metadata slots; resource lookup itself is dictionary indexed. */
    for (unsigned i = 0; i < cache->limit; ++i) if (!cache->entries[i].used) { slot = (int)i; break; }
    if (slot < 0 || !dictionary_set(cache->keys, key, &slot)) return result(cache, -1, evicted);
    texture_entry_t *entry = &cache->entries[slot];
    memset(entry, 0, sizeof *entry); memcpy(entry->key, key, sizeof key);
    entry->bytes = (unsigned)bytes; entry->used = true; entry->zoom = isfinite(zoom) ? zoom : 0;
    entry->geographic = bounds_read(lab_dom_get(request, "bounds"), entry->bounds);
    cache->bytes += entry->bytes;
    touch(cache, slot, false);
    return result(cache, slot, evicted);
}

static jsref cover_entry(texture_cache_t *cache, jsref request)
{
    double bounds[4], zoom = field(request, "zoom");
    if (cache->limit > 4 || !isfinite(zoom) || zoom <= 0 || !bounds_read(lab_dom_get(request, "bounds"), bounds)) return 0;
    /* Coverage is a spatial predicate over at most four resident GPU textures. */
    for (int slot = cache->newest; slot >= 0; slot = cache->entries[slot].previous) {
        texture_entry_t *entry = &cache->entries[slot];
        double *b = entry->bounds;
        if (entry->geographic && zoom <= entry->zoom * 1.5 && bounds[0] >= b[0] && bounds[1] >= b[1] &&
            bounds[2] <= b[2] && bounds[3] <= b[3]) {
            touch(cache, slot, true); return result(cache, slot, 0);
        }
    }
    return 0;
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

static void cache_dispatch(void *context)
{
    jsref request = *(jsref *)context;
    static jsref (*const operations[])(texture_cache_t *, jsref) = {
        create_cache, get_entry, put_entry, cover_entry, clear_cache, cache_stats
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
