/**
 * @file shoreline_cache.c
 * @brief Resident exact shoreline coordinates, display resolutions and spatial ring selection.
 *
 * One immutable file-backed dataset is retained per process and can be prepared before worker processes fork.
 * Forked readers share its read-only pages. File/WAL identity changes replace the shared entry; outstanding users
 * retain the old allocation until release. Anonymous databases are cached only by their owning reader.
 * Full-resolution coordinates retain physical-edge flags for hit-testing. Four radial display resolutions never
 * replace those coordinates. A bounding-volume tree avoids scanning unrelated rings in local views.
 */
#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <zstd.h>

#define MARS_GLOBE_INTERNAL_ACCESS
#include "globe_layers.h"
#define MARS_SHORELINE_INTERNAL_ACCESS
#include "shoreline_internal.h"

typedef struct _coast_node_t {
    int32_t west, south, east, north;
    uint32_t first, count, left, right;
} coast_node_t;

static pthread_mutex_t coast_cache_mutex = PTHREAD_MUTEX_INITIALIZER;
static coast_geometry_t *coast_shared_geometry;
static const double coast_lod_steps[] = {0.005, 0.02, 0.08, 0.32};

bool coast_geometry_budget(size_t vertices, size_t rings, size_t lod_capacity, size_t previous_capacity)
{
    if (vertices < 2 || vertices > 64000000 || rings < 1 || rings > 1000000 ||
        lod_capacity > 4 * vertices || previous_capacity > 4 * vertices) return false;
    /* Reserve tree compaction and old/new LOD allocations together, not only final resident storage. */
    uint64_t bytes = (uint64_t)vertices * (sizeof(coast_vertex_t) + 1) +
        (uint64_t)rings * (sizeof(coast_ring_t) + 4 * sizeof(coast_node_t)) +
        ((uint64_t)lod_capacity + previous_capacity) * sizeof(uint32_t);
    return bytes <= UINT64_C(1536) * 1048576;
}

static void coast_geometry_free(coast_geometry_t *geometry)
{
    if (!geometry) return;
    free(geometry->vertices);
    free(geometry->boundary);
    free(geometry->rings);
    free(geometry->lod);
    free(geometry->nodes);
    string_free(geometry->key);
    free(geometry);
}

static bool coast_key_file(string_t *key, const char *path, const char *suffix, bool required)
{
    string_t *name = string_sprintf("%s%s", path, suffix);
    file_t *file = name ? file_new(name) : NULL;
    file_info_t *info = file ? file_get_info(file) : NULL;
    int64_t seconds = 0;
    long nanos = 0;
    bool ok = info ? file_info_status_change_time(info, &seconds, &nanos) : !required;
    if (ok)
        ok = string_append_format(key, ":%lld:%ld:%llu", (long long)seconds, nanos,
                                   (unsigned long long)file_info_size(info)) >= 0;
    file_info_free(info);
    file_free(file);
    string_free(name);
    return ok;
}

string_t *coast_database_key(sqlite_t *db, bool *shared)
{
    const char *path = sqlite_database_path(db);
    *shared = path && *path;
    string_t *key = string_new_with(*shared ? path : "anonymous");
    if (!key) return NULL;
    if (*shared) {
        if (coast_key_file(key, path, "", true) && coast_key_file(key, path, "-wal", false)) return key;
    } else {
        sqlite_stmt_t *query = sqlite_stmt_prepare(db, "SELECT total_changes()");
        bool ok = query && sqlite_stmt_step(query) == SQLITE_STEP_ROW &&
                  string_append_format(key, ":%lld", (long long)sqlite_stmt_column_int64(query, 0)) >= 0;
        sqlite_stmt_finalize(query);
        query = sqlite_stmt_prepare(db, "PRAGMA data_version");
        ok = ok && query && sqlite_stmt_step(query) == SQLITE_STEP_ROW &&
             string_append_format(key, ":%lld", (long long)sqlite_stmt_column_int64(query, 0)) >= 0;
        sqlite_stmt_finalize(query);
        query = sqlite_stmt_prepare(db, "PRAGMA schema_version");
        ok = ok && query && sqlite_stmt_step(query) == SQLITE_STEP_ROW &&
             string_append_format(key, ":%lld", (long long)sqlite_stmt_column_int64(query, 0)) >= 0;
        sqlite_stmt_finalize(query);
        if (ok) return key;
    }
    string_free(key);
    return NULL;
}

static bool coast_geometry_read(sqlite_t *db, coast_geometry_t *geometry)
{
    sqlite_stmt_t *query = sqlite_stmt_prepare(db,
        "SELECT sum(point_count),count(DISTINCT polygon_id) FROM shoreline_chunk");
    bool ok = query && sqlite_stmt_step(query) == SQLITE_STEP_ROW;
    int64_t vertices = ok ? sqlite_stmt_column_int64(query, 0) : 0;
    int64_t rings = ok ? sqlite_stmt_column_int64(query, 1) : 0;
    sqlite_stmt_finalize(query);
    if (vertices < 2 || vertices > 64000000 || rings < 1 || rings > 1000000 ||
        !coast_geometry_budget((size_t)vertices, (size_t)rings, 0, 0)) return false;
    geometry->vertices = malloc((size_t)vertices * sizeof *geometry->vertices);
    geometry->boundary = malloc((size_t)vertices);
    geometry->rings = calloc((size_t)rings, sizeof *geometry->rings);
    if (!geometry->vertices || !geometry->boundary || !geometry->rings) return false;
    query = sqlite_stmt_prepare(db,
        "SELECT polygon_id,level,codec,point_count,blob_id,byte_offset,byte_count,boundary "
        "FROM shoreline_chunk ORDER BY id");
    sqlite_stmt_t *page_query = sqlite_stmt_prepare(db, "SELECT vertices FROM shoreline_blob WHERE id=?1");
    ok = query && page_query;
    unsigned char page[65536], decoded[4112];
    size_t page_length = 0;
    int64_t page_id = -1, polygon = -1;
    sqlite_step_result_t step = SQLITE_STEP_DONE;
    while (ok && (step = sqlite_stmt_step(query)) == SQLITE_STEP_ROW) {
        int64_t id = sqlite_stmt_column_int64(query, 0), next_page = sqlite_stmt_column_int64(query, 4);
        int level = sqlite_stmt_column_int(query, 1), codec = sqlite_stmt_column_int(query, 2);
        int count = sqlite_stmt_column_int(query, 3), offset = sqlite_stmt_column_int(query, 5);
        int length = sqlite_stmt_column_int(query, 6), boundary = sqlite_stmt_column_int(query, 7);
        size_t stride = coast_codec_stride(codec);
        if (id < 0 || level < 1 || level > 4 || count < 2 || count > 257 || offset < 0 || length < 1 ||
            length > (int)sizeof decoded || next_page < 0 || (boundary != 0 && boundary != 1) || !stride ||
            geometry->vertex_count + (size_t)count > (size_t)vertices) { ok = false; break; }
        if (page_id != next_page) {
            ok = sqlite_stmt_bind_int64(page_query, 1, next_page) && sqlite_stmt_step(page_query) == SQLITE_STEP_ROW;
            const void *data = ok ? sqlite_stmt_column_blob(page_query, 0) : NULL;
            page_length = ok ? sqlite_stmt_column_bytes(page_query, 0) : 0;
            if (!data || !page_length || page_length > sizeof page) { ok = false; break; }
            memcpy(page, data, page_length);
            sqlite_stmt_reset(page_query);
            page_id = next_page;
        }
        if ((size_t)offset > page_length || (size_t)length > page_length - (size_t)offset) { ok = false; break; }
        const unsigned char *bytes = page + offset;
        size_t size = (size_t)length;
        if (codec == SHORELINE_CODEC_ZSTD || codec == SHORELINE_CODEC_F64_ZSTD) {
            size = ZSTD_decompress(decoded, sizeof decoded, bytes, size); bytes = decoded;
        }
        if (size != (size_t)count * stride) { ok = false; break; }
        if (id != polygon) {
            if (geometry->ring_count == (size_t)rings) { ok = false; break; }
            coast_ring_t *ring = &geometry->rings[geometry->ring_count++];
            ring->first = (uint32_t)geometry->vertex_count;
            ring->west = ring->south = INT32_MAX;
            ring->east = ring->north = INT32_MIN;
            ring->level = (unsigned)level;
            polygon = id;
        }
        coast_ring_t *ring = &geometry->rings[geometry->ring_count - 1];
        if (ring->level != (unsigned)level) { ok = false; break; }
        ring->antarctic |= !boundary;
        for (size_t at = 0; ok && at < size; at += stride) {
            coast_vertex_t vertex;
            if (!coast_decode_vertex(bytes + at, codec, &vertex)) { ok = false; break; }
            geometry->vertices[geometry->vertex_count] = vertex;
            geometry->boundary[geometry->vertex_count++] = (unsigned char)boundary;
            ++ring->count;
            int32_t west = (int32_t)floor(vertex.x * 1000000), east = (int32_t)ceil(vertex.x * 1000000);
            int32_t south = (int32_t)floor(vertex.y * 1000000), north = (int32_t)ceil(vertex.y * 1000000);
            if (west < ring->west) ring->west = west;
            if (east > ring->east) ring->east = east;
            if (south < ring->south) ring->south = south;
            if (north > ring->north) ring->north = north;
        }
    }
    sqlite_stmt_finalize(query);
    sqlite_stmt_finalize(page_query);
    return ok && step == SQLITE_STEP_DONE && geometry->vertex_count == (size_t)vertices &&
           geometry->ring_count == (size_t)rings;
}

static bool coast_lod_append(coast_geometry_t *geometry, size_t *capacity, uint32_t vertex)
{
    if (geometry->lod_count == *capacity) {
        size_t next = *capacity ? *capacity * 2 : 4096;
        if (next > 4 * geometry->vertex_count) next = 4 * geometry->vertex_count;
        if (next <= *capacity) return false;
        if (!coast_geometry_budget(geometry->vertex_count, geometry->ring_count, next, *capacity)) return false;
        uint32_t *items = realloc(geometry->lod, next * sizeof *items);
        if (!items) return false;
        geometry->lod = items;
        *capacity = next;
    }
    geometry->lod[geometry->lod_count++] = vertex;
    return true;
}

static bool coast_geometry_resolutions(coast_geometry_t *geometry)
{
    size_t capacity = 0;
    for (size_t i = 0; i < geometry->ring_count; ++i) {
        coast_ring_t *ring = &geometry->rings[i];
        for (unsigned lod = 0; lod < 4; ++lod) {
            ring->lod_first[lod] = (uint32_t)geometry->lod_count;
            coast_vertex_t last = geometry->vertices[ring->first];
            double distance = coast_lod_steps[lod];
            for (uint32_t n = 0; n < ring->count; ++n) {
                uint32_t index = ring->first + n;
                coast_vertex_t vertex = geometry->vertices[index];
                if (n && n + 1 != ring->count && geometry->boundary[index] &&
                    hypot((double)vertex.x - last.x, (double)vertex.y - last.y) < distance) continue;
                if (!coast_lod_append(geometry, &capacity, index)) return false;
                last = vertex;
            }
            ring->lod_count[lod] = (uint32_t)geometry->lod_count - ring->lod_first[lod];
        }
    }
    uint32_t *tight = coast_geometry_budget(geometry->vertex_count, geometry->ring_count, geometry->lod_count, capacity)
        ? realloc(geometry->lod, geometry->lod_count * sizeof *tight) : NULL;
    if (tight) geometry->lod = tight;
    return true;
}

static int coast_ring_x(const void *a, const void *b)
{
    const coast_ring_t *left = a, *right = b;
    int64_t x = (int64_t)left->west + left->east, y = (int64_t)right->west + right->east;
    return (x > y) - (x < y);
}

static int coast_ring_y(const void *a, const void *b)
{
    const coast_ring_t *left = a, *right = b;
    int64_t x = (int64_t)left->south + left->north, y = (int64_t)right->south + right->north;
    return (x > y) - (x < y);
}

static uint32_t coast_geometry_tree(coast_geometry_t *geometry, uint32_t first, uint32_t count)
{
    uint32_t index = (uint32_t)geometry->node_count++;
    coast_node_t *node = &geometry->nodes[index];
    *node = (coast_node_t){.west = INT32_MAX, .south = INT32_MAX, .east = INT32_MIN, .north = INT32_MIN,
                           .first = first, .count = count};
    for (uint32_t i = first; i < first + count; ++i) {
        coast_ring_t *ring = &geometry->rings[i];
        if (ring->west < node->west) node->west = ring->west;
        if (ring->east > node->east) node->east = ring->east;
        if (ring->south < node->south) node->south = ring->south;
        if (ring->north > node->north) node->north = ring->north;
    }
    /* At most sixteen local ring checks per leaf; higher levels reject whole unrelated regions. */
    if (count > 16) {
        qsort(geometry->rings + first, count, sizeof *geometry->rings,
              (int64_t)node->east - node->west >= (int64_t)node->north - node->south ? coast_ring_x : coast_ring_y);
        node->left = coast_geometry_tree(geometry, first, count / 2);
        node->right = coast_geometry_tree(geometry, first + count / 2, count - count / 2);
        node->count = 0;
    }
    return index;
}

/* Retain one immutable snapshot; loading and publication are serialised, traversal is lock-free. */
coast_geometry_t *coast_geometry_acquire(shoreline_t *shoreline, bool load)
{
    if (!shoreline) return NULL;
    bool shared = false;
    string_t *key = coast_database_key(shoreline->db, &shared);
    if (!key) return NULL;
    pthread_mutex_lock(&coast_cache_mutex);
    coast_geometry_t **slot = shared ? &coast_shared_geometry : &shoreline->local_geometry;
    coast_geometry_t *geometry = *slot;
    if (!geometry || strcmp(string_c_str(key), string_c_str(geometry->key))) {
        geometry = NULL;
        if (load) {
            geometry = calloc(1, sizeof *geometry);
            if (geometry && coast_geometry_read(shoreline->db, geometry) && coast_geometry_resolutions(geometry)) {
                bool same_kind = false;
                string_t *after = coast_database_key(shoreline->db, &same_kind);
                bool unchanged = after && same_kind == shared && !strcmp(string_c_str(after), string_c_str(key));
                string_free(after);
                if (!unchanged) {
                    coast_geometry_free(geometry);
                    pthread_mutex_unlock(&coast_cache_mutex);
                    string_free(key);
                    return NULL;
                }
                geometry->nodes = calloc(2 * geometry->ring_count, sizeof *geometry->nodes);
                if (geometry->nodes) {
                    coast_geometry_tree(geometry, 0, (uint32_t)geometry->ring_count);
                    coast_node_t *tight = realloc(geometry->nodes, geometry->node_count * sizeof *tight);
                    if (tight) geometry->nodes = tight;
                    geometry->key = key;
                    key = NULL;
                    geometry->references = 1;
                    if (*slot && !--(*slot)->references) coast_geometry_free(*slot);
                    *slot = geometry;
                } else { coast_geometry_free(geometry); geometry = NULL; }
            } else { coast_geometry_free(geometry); geometry = NULL; }
        }
    }
    if (geometry) ++geometry->references;
    pthread_mutex_unlock(&coast_cache_mutex);
    string_free(key);
    return geometry;
}

/* Release one reader while retaining the shared or reader-local owner reference. */
void coast_geometry_release(coast_geometry_t *geometry)
{
    if (!geometry) return;
    pthread_mutex_lock(&coast_cache_mutex);
    if (!--geometry->references) coast_geometry_free(geometry);
    pthread_mutex_unlock(&coast_cache_mutex);
}

static bool coast_tree_visit(const coast_geometry_t *geometry, uint32_t index, const double bounds[4],
                             double span, coast_ring_fn visit, void *context)
{
    const coast_node_t *node = &geometry->nodes[index];
    if (node->east < bounds[0] || node->west > bounds[2] || node->north < bounds[1] || node->south > bounds[3]) return true;
    if (!node->count)
        return coast_tree_visit(geometry, node->left, bounds, span, visit, context) &&
               coast_tree_visit(geometry, node->right, bounds, span, visit, context);
    for (uint32_t i = node->first; i < node->first + node->count; ++i) {
        const coast_ring_t *ring = &geometry->rings[i];
        if (ring->east < bounds[0] || ring->west > bounds[2] || ring->north < bounds[1] || ring->south > bounds[3] ||
            ((double)ring->east - ring->west < span && (double)ring->north - ring->south < span)) continue;
        if (!visit(geometry, ring, context)) return false;
    }
    return true;
}

/* Select candidate rings without decoding or accessing the database. */
bool coast_geometry_visit(const coast_geometry_t *geometry, const double bounds[4], double span,
                          coast_ring_fn visit, void *context)
{
    return geometry && visit && coast_tree_visit(geometry, 0, bounds, span, visit, context);
}

/* Reserve most of the display tolerance for clipping and the final screen-space reduction. */
int coast_geometry_lod(double tolerance)
{
    int result = -1;
    for (unsigned i = 0; i < 4; ++i) if (coast_lod_steps[i] <= tolerance / 4) result = (int)i;
    return result;
}

/* Warm an immutable dataset before native request workers are forked. */
bool shoreline_prepare(shoreline_t *shoreline)
{
    coast_geometry_t *geometry = coast_geometry_acquire(shoreline, true);
    bool ok = geometry != NULL;
    coast_geometry_release(geometry);
    return ok;
}
