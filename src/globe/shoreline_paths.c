/**
 * @file shoreline_paths.c
 * @brief Spatially selected, streamed vector polygons for shoreline display.
 *
 * Cold regional queries stream indexed candidate rings without decoding the global dataset. Warm queries
 * visit cached rings through their spatial tree, retaining enclosing land and lake polygons even when
 * their boundaries lie outside the viewport. Four streaming half-plane clippers bound output without retaining
 * a full ring. Display-only distance simplification follows clipping; the stored source and hit-tests are unchanged.
 */
#include <math.h>
#include <stdint.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <zstd.h>

#define MARS_GLOBE_INTERNAL_ACCESS
#include "globe_layers.h"
#define MARS_SHORELINE_INTERNAL_ACCESS
#include "shoreline_internal.h"

typedef struct { double x, y; } coast_point_t;
typedef struct {
    coast_point_t first, previous;
    bool present;
} coast_clip_t;
typedef struct {
    coast_clip_t clips[4];
    double limits[4], tolerance;
    coast_point_t last;
    unsigned level;
    bool emitted, ok, antarctic;
    globe_path_fn visit;
    void *context;
} coast_path_t;

static double coast_distance(coast_path_t *path, unsigned stage, coast_point_t point)
{
    double coordinate = stage < 2 ? point.x : point.y;
    return stage % 2 ? path->limits[stage] - coordinate : coordinate - path->limits[stage];
}

static void coast_feed(coast_path_t *path, unsigned stage, coast_point_t point);

static void coast_edge(coast_path_t *path, unsigned stage, coast_point_t a, coast_point_t b)
{
    double da = coast_distance(path, stage, a), db = coast_distance(path, stage, b);
    if ((da >= 0) != (db >= 0)) {
        double fraction = da / (da - db);
        coast_point_t intersection = {a.x + fraction * (b.x - a.x), a.y + fraction * (b.y - a.y)};
        coast_feed(path, stage + 1, intersection);
    }
    if (db >= 0) coast_feed(path, stage + 1, b);
}

static void coast_feed(coast_path_t *path, unsigned stage, coast_point_t point)
{
    if (!path->ok) return;
    if (stage == 4) {
        if (path->emitted && ((point.x == path->last.x && point.y == path->last.y) ||
            hypot(point.x - path->last.x, point.y - path->last.y) < path->tolerance)) return;
        path->ok = path->visit(path->emitted ? GLOBE_PATH_LINE : GLOBE_PATH_MOVE,
                              point.x, point.y, path->level, path->antarctic, path->context);
        path->emitted = true;
        path->last = point;
        return;
    }
    coast_clip_t *clip = &path->clips[stage];
    if (clip->present)
        coast_edge(path, stage, clip->previous, point);
    else {
        clip->first = point;
        clip->present = true;
    }
    clip->previous = point;
}

static void coast_finish(coast_path_t *path)
{
    for (unsigned stage = 0; stage < 4; ++stage) {
        coast_clip_t *clip = &path->clips[stage];
        if (clip->present) coast_edge(path, stage, clip->previous, clip->first);
    }
    if (path->ok && path->emitted)
        path->ok = path->visit(GLOBE_PATH_CLOSE, 0, 0, path->level, path->antarctic, path->context);
    memset(path->clips, 0, sizeof path->clips);
    path->emitted = false;
}

typedef struct {
    coast_path_t path;
    int wrap, lod;
} coast_display_t;

typedef struct {
    coast_point_t point;
    globe_path_event_t event;
    unsigned level;
    bool antarctic;
} coast_region_point_t;

typedef struct {
    coast_region_point_t *points;
    size_t count, capacity;
    double bounds[4];
    string_t *key;
} coast_region_cache_t;

static pthread_mutex_t coast_region_mutex = PTHREAD_MUTEX_INITIALIZER;
static coast_region_cache_t coast_region_cache;

static bool coast_region_append(globe_path_event_t event, double x, double y,
                                unsigned level, bool antarctic, void *context)
{
    coast_region_cache_t *cache = context;
    if (cache->count == cache->capacity) {
        size_t capacity = cache->capacity ? cache->capacity * 2 : 4096;
        if (capacity > 524288) return false;
        coast_region_point_t *points = realloc(cache->points, capacity * sizeof *points);
        if (!points) return false;
        cache->points = points;
        cache->capacity = capacity;
    }
    cache->points[cache->count++] = (coast_region_point_t){{x, y}, event, level, antarctic};
    return true;
}

static bool coast_display_ring(const coast_geometry_t *geometry, const coast_ring_t *ring, void *context)
{
    coast_display_t *display = context;
    coast_path_t *path = &display->path;
    path->level = ring->level;
    path->antarctic = ring->antarctic;
    uint32_t count = display->lod < 0 ? ring->count : ring->lod_count[display->lod];
    for (uint32_t i = 0; path->ok && i < count; ++i) {
        uint32_t index = display->lod < 0 ? ring->first + i : geometry->lod[ring->lod_first[display->lod] + i];
        coast_vertex_t vertex = geometry->vertices[index];
        coast_point_t point = {vertex.x - display->wrap * 360, vertex.y};
        coast_feed(path, 0, point);
    }
    coast_finish(path);
    return path->ok;
}

/* Opposing ray candidates retain enclosing polygons whose edges miss the window,
 * but exclude rings wholly east or west of it before reading their coordinates. */
static bool coast_region_paths(shoreline_t *shoreline, coast_display_t *display)
{
    coast_path_t *path = &display->path;
    double west = path->limits[0] + display->wrap * 360, east = path->limits[1] + display->wrap * 360;
    if (east < -180 || west > 360) return true;
    sqlite_stmt_t *query = sqlite_stmt_prepare(shoreline->db,
        "select c.polygon_id,c.level,c.codec,c.point_count,c.blob_id,c.byte_offset,c.byte_count "
        "from shoreline_chunk c "
        "where c.polygon_id in (select distinct s.polygon_id from shoreline_bounds r "
        "join shoreline_chunk s on s.id=r.id where r.max_x>=?1 and r.min_y<=?2 and r.max_y>=?3 "
        "intersect select s.polygon_id from shoreline_bounds r join shoreline_chunk s on s.id=r.id "
        "where r.min_x<=?4 and r.min_y<=?2 and r.max_y>=?3) "
        "order by c.polygon_id,c.id");
    sqlite_stmt_t *metadata = sqlite_stmt_prepare(shoreline->db,
        "select min(boundary) from shoreline_chunk where polygon_id=?1");
    sqlite_stmt_t *page_query = sqlite_stmt_prepare(shoreline->db,
        "select vertices from shoreline_blob where id=?1");
    unsigned char page[65536];
    size_t page_length = 0;
    int64_t page_id = -1;
    bool ok = query && metadata && page_query && sqlite_stmt_bind_double(query, 1, west * 1000000) &&
        sqlite_stmt_bind_double(query, 2, path->limits[3] * 1000000) &&
        sqlite_stmt_bind_double(query, 3, path->limits[2] * 1000000) &&
        sqlite_stmt_bind_double(query, 4, east * 1000000);
    int64_t polygon = -1;
    sqlite_step_result_t step = SQLITE_STEP_ERROR;
    while (ok && path->ok && (step = sqlite_stmt_step(query)) == SQLITE_STEP_ROW) {
        int64_t id = sqlite_stmt_column_int64(query, 0);
        int level = sqlite_stmt_column_int(query, 1), codec = sqlite_stmt_column_int(query, 2);
        int count = sqlite_stmt_column_int(query, 3);
        int64_t next_page = sqlite_stmt_column_int64(query, 4);
        int offset = sqlite_stmt_column_int(query, 5), byte_count = sqlite_stmt_column_int(query, 6);
        size_t length = byte_count > 0 ? (size_t)byte_count : 0, stride = coast_codec_stride(codec);
        unsigned char decoded[4112];
        if (id < 0 || next_page < 0 || offset < 0 || level < 1 || level > 4 || count < 2 || count > 257 || !stride ||
            !length || length > sizeof decoded) { ok = false; break; }
        if (page_id != next_page) {
            ok = sqlite_stmt_bind_int64(page_query, 1, next_page) && sqlite_stmt_step(page_query) == SQLITE_STEP_ROW;
            const void *data = ok ? sqlite_stmt_column_blob(page_query, 0) : NULL;
            page_length = ok ? sqlite_stmt_column_bytes(page_query, 0) : 0;
            if (!data || !page_length || page_length > sizeof page) { ok = false; break; }
            memcpy(page, data, page_length);
            sqlite_stmt_reset(page_query);
            page_id = next_page;
        }
        if ((size_t)offset > page_length || length > page_length - (size_t)offset) { ok = false; break; }
        const unsigned char *bytes = page + offset;
        if (id != polygon) {
            if (polygon >= 0) coast_finish(path);
            ok = sqlite_stmt_bind_int64(metadata, 1, id) && sqlite_stmt_step(metadata) == SQLITE_STEP_ROW;
            int physical = ok ? sqlite_stmt_column_int(metadata, 0) : -1;
            sqlite_stmt_reset(metadata);
            if (!ok || (physical != 0 && physical != 1)) { ok = false; break; }
            path->antarctic = !physical;
            path->level = (unsigned)level;
            polygon = id;
        }
        if (path->level != (unsigned)level) { ok = false; break; }
        if (codec == SHORELINE_CODEC_ZSTD || codec == SHORELINE_CODEC_F64_ZSTD) {
            length = ZSTD_decompress(decoded, sizeof decoded, bytes, length);
            bytes = decoded;
        }
        if (length != (size_t)count * stride) { ok = false; break; }
        for (size_t at = 0; ok && path->ok && at < length; at += stride) {
            coast_vertex_t vertex;
            ok = coast_decode_vertex(bytes + at, codec, &vertex);
            if (ok) coast_feed(path, 0, (coast_point_t){vertex.x - display->wrap * 360, vertex.y});
        }
    }
    if (ok && path->ok && step == SQLITE_STEP_DONE && polygon >= 0) coast_finish(path);
    sqlite_stmt_finalize(metadata);
    sqlite_stmt_finalize(page_query);
    sqlite_stmt_finalize(query);
    return ok && path->ok && step == SQLITE_STEP_DONE;
}

/* Retain one bounded, exact local window across readers. The bounded stream is
 * re-clipped at the caller's tolerance, never reusing a coarse zoom's vertices. */
static bool coast_region_reuse(shoreline_t *shoreline, coast_display_t *display, bool *handled)
{
    coast_path_t *path = &display->path;
    double west = path->limits[0], east = path->limits[1], south = path->limits[2], north = path->limits[3];
    *handled = false;
    bool shared = false;
    string_t *key = coast_database_key(shoreline->db, &shared);
    if (!key || !shared || east - west > 45 || north - south > 45) {
        string_free(key);
        return true;
    }
    pthread_mutex_lock(&coast_region_mutex);
    coast_region_cache_t *cache = &coast_region_cache;
    bool covered = cache->key && !strcmp(string_c_str(key), string_c_str(cache->key)) &&
        west >= cache->bounds[0] && south >= cache->bounds[1] && east <= cache->bounds[2] && north <= cache->bounds[3];
    if (!covered) {
        double dx = (east - west) / 8, dy = (north - south) / 8;
        coast_region_cache_t next = {.bounds = {fmax(-360, west - dx), fmax(-90, south - dy),
                                               fmin(west + 360, east + dx), fmin(90, north + dy)}};
        coast_display_t loader = {.path = {.limits = {next.bounds[0], next.bounds[2], next.bounds[1], next.bounds[3]},
                                           .ok = true, .visit = coast_region_append, .context = &next}};
        for (loader.wrap = -1; loader.path.ok && loader.wrap <= 1; ++loader.wrap)
            loader.path.ok = coast_region_paths(shoreline, &loader);
        bool after_shared = false;
        string_t *after = coast_database_key(shoreline->db, &after_shared);
        covered = loader.path.ok && after && after_shared && !strcmp(string_c_str(key), string_c_str(after));
        string_free(after);
        if (covered) {
            free(cache->points);
            string_free(cache->key);
            next.key = key;
            key = NULL;
            *cache = next;
        } else free(next.points);
    }
    if (covered) {
        *handled = true;
        /* At most 524288 local events, not a scan of the global dataset. */
        for (size_t i = 0; path->ok && i < cache->count; ++i) {
            const coast_region_point_t *point = &cache->points[i];
            path->level = point->level;
            path->antarctic = point->antarctic;
            if (point->event == GLOBE_PATH_CLOSE) coast_finish(path);
            else coast_feed(path, 0, point->point);
        }
    }
    pthread_mutex_unlock(&coast_region_mutex);
    string_free(key);
    return path->ok;
}

/* Clip indexed regional or resident geometry, retaining exact coordinates for close views and hit-testing. */
bool shoreline_paths(shoreline_t *shoreline, double west, double south, double east, double north,
                     double tolerance, globe_path_fn visit, void *context)
{
    if (!shoreline || !visit || !isfinite(west) || !isfinite(east) || !isfinite(south) || !isfinite(north) ||
        !isfinite(tolerance) || tolerance < 0 || tolerance > 1 || west < -360 || west > 180 ||
        east <= west || east - west > 360 || south < -90 || north > 90 || north <= south)
        return false;
    bool regional = east - west <= 90 && north - south <= 90;
    coast_geometry_t *geometry = coast_geometry_acquire(shoreline, !regional);
    if (!geometry && !regional) return false;
    coast_display_t display = {
        .path = {.limits = {west, east, south, north}, .tolerance = tolerance,
                 .ok = true, .visit = visit, .context = context},
        .lod = coast_geometry_lod(tolerance)
    };
    if (!geometry) {
        bool handled = false;
        bool ok = coast_region_reuse(shoreline, &display, &handled);
        if (handled) return ok;
    }
    for (display.wrap = -1; display.path.ok && display.wrap <= 1; ++display.wrap) {
        double bounds[] = {(west + display.wrap * 360) * 1000000, south * 1000000,
                           (east + display.wrap * 360) * 1000000, north * 1000000};
        display.path.ok = geometry
            ? coast_geometry_visit(geometry, bounds, tolerance * 1000000, coast_display_ring, &display)
            : coast_region_paths(shoreline, &display);
    }
    coast_geometry_release(geometry);
    return display.path.ok;
}
