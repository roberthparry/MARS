/**
 * @file shoreline_import.c
 * @brief Transactional import of GSHHG shoreline rings into bounded, indexed BLOB pages.
 *
 * Used by the native database installer. Each uncompressed chunk contains at most 256 edges.
 * Integer microdegree bounds use an R-tree; offsets address chunks within
 * 64 KiB pages. The alternative Antarctic grounding line is excluded in favour of the ice-front boundary.
 * Input and database failures roll back the complete replacement, retaining astronomical tables.
 */
#include <stdint.h>
#include <string.h>

#define MARS_GLOBE_INTERNAL_ACCESS
#include "globe_layers.h"
#define MARS_SHORELINE_INTERNAL_ACCESS
#include "shoreline_internal.h"

typedef struct _coast_import_t {
    sqlite_stmt_t *blob;
    sqlite_stmt_t *chunk;
    sqlite_stmt_t *bounds;
    unsigned char page[65536];
    size_t used;
    int64_t page_id;
    int64_t chunk_id;
    int64_t points;
    int64_t polygons;
} coast_import_t;

static uint32_t coast_unsigned(const unsigned char *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static int32_t coast_signed(const unsigned char *p)
{
    uint32_t n = coast_unsigned(p);
    return n <= INT32_MAX ? (int32_t)n : (int32_t)((int64_t)n - 4294967296LL);
}

static bool coast_execute(sqlite_stmt_t *statement)
{
    bool ok = sqlite_stmt_step(statement) == SQLITE_STEP_DONE;
    sqlite_stmt_reset(statement);
    return ok;
}

static bool coast_flush(coast_import_t *state)
{
    if (!state->used)
        return true;
    bool ok = sqlite_stmt_bind_int64(state->blob, 1, state->page_id) &&
              sqlite_stmt_bind_blob(state->blob, 2, state->page, state->used) && coast_execute(state->blob);
    state->used = 0;
    ++state->page_id;
    return ok;
}

static bool coast_chunk(coast_import_t *state, const unsigned char *points, size_t length, int polygon, int level,
                         bool boundary)
{
    size_t size = length;
    if (state->used + size > sizeof state->page && !coast_flush(state))
        return false;
    int32_t min_x = INT32_MAX, max_x = INT32_MIN, min_y = INT32_MAX, max_y = INT32_MIN;
    for (size_t i = 0; i < length; i += 8) {
        int32_t x = coast_signed(points + i), y = coast_signed(points + i + 4);
        if (x < min_x) min_x = x;
        if (x > max_x) max_x = x;
        if (y < min_y) min_y = y;
        if (y > max_y) max_y = y;
    }
    ++state->chunk_id;
    bool ok = sqlite_stmt_bind_int64(state->chunk, 1, state->chunk_id) &&
              sqlite_stmt_bind_int64(state->chunk, 2, state->page_id) &&
              sqlite_stmt_bind_int(state->chunk, 3, (int)state->used) &&
              sqlite_stmt_bind_int(state->chunk, 4, (int)size) &&
              sqlite_stmt_bind_int(state->chunk, 5, (int)(length / 8)) &&
              sqlite_stmt_bind_int(state->chunk, 6, SHORELINE_CODEC_RAW) &&
              sqlite_stmt_bind_int(state->chunk, 7, polygon) &&
              sqlite_stmt_bind_int(state->chunk, 8, level == 5 ? 1 : level) &&
              sqlite_stmt_bind_int(state->chunk, 9, boundary) && coast_execute(state->chunk) &&
              sqlite_stmt_bind_int64(state->bounds, 1, state->chunk_id) &&
              sqlite_stmt_bind_int(state->bounds, 2, min_x) && sqlite_stmt_bind_int(state->bounds, 3, max_x) &&
              sqlite_stmt_bind_int(state->bounds, 4, min_y) && sqlite_stmt_bind_int(state->bounds, 5, max_y) &&
              coast_execute(state->bounds);
    if (ok) {
        memcpy(state->page + state->used, points, size);
        state->used += size;
    }
    return ok;
}

static bool coast_ring(coast_import_t *state, file_t *source, const unsigned char *header)
{
    uint32_t n = coast_unsigned(header + 4), flag = coast_unsigned(header + 8), level = flag & 255;
    int32_t west = coast_signed(header + 12), east = coast_signed(header + 16);
    int32_t south = coast_signed(header + 20), north = coast_signed(header + 24);
    int polygon = coast_signed(header);
    if (polygon < 0 || n < 2 || n > 20000000 || level < 1 || level > 6 || ((flag >> 8) & 255) != 15 ||
        west < -180000000 || east > 360000000 || east < west ||
        south < -90000000 || north > 90000000 || north < south)
        return false;
    unsigned char points[2056], first[8], point[8], input[8192];
    size_t used = 0, count;
    for (uint32_t i = 0; i < n; ++i) {
        if (i % 1024 == 0) {
            size_t remaining = n - i, length = (remaining < 1024 ? remaining : 1024) * 8;
            if (!file_read(source, input, length, &count) || count != length)
                return false;
        }
        memcpy(point, input + (i % 1024) * 8, sizeof point);
        int64_t x = coast_signed(point);
        int32_t y = coast_signed(point + 4);
        if (x > east && x - 360000000 >= west) x -= 360000000;
        if (x < west && x + 360000000 <= east) x += 360000000;
        /* Upstream header bounds can differ from a vertex by a microdegree through rounding.
         * Indexed bounds below are always recomputed from the actual vertices. */
        if (x < (int64_t)west - 2 || x > (int64_t)east + 2 ||
            y < (int64_t)south - 2 || y > (int64_t)north + 2 || y < -90000000 || y > 90000000)
            return false;
        if (level == 6)
            continue;
        uint32_t encoded = (uint32_t)x;
        point[0] = encoded >> 24;
        point[1] = encoded >> 16;
        point[2] = encoded >> 8;
        point[3] = encoded;
        if (!i) memcpy(first, point, sizeof first);
        memcpy(points + used, point, sizeof point);
        used += sizeof point;
        if (used == sizeof points) {
            if (!coast_chunk(state, points, used, polygon, level, true))
                return false;
            memcpy(points, point, sizeof point);
            used = sizeof point;
        }
    }
    if (level == 6)
        return true;
    bool polar = level == 5 && west == -180000000 && east == 180000000 && south == -90000000;
    if (!polar && memcmp(first, point, sizeof first)) {
        memcpy(points + used, first, sizeof first);
        used += sizeof first;
    }
    if (used > 8 && !coast_chunk(state, points, used, polygon, level, true))
        return false;
    if (polar) {
        /* GSHHG's ice-front ring ends at opposite sides of the date line. Close it via the south pole,
         * not across the continent. These artificial edges affect containment, never coastal proximity. */
        unsigned char cap[32];
        memcpy(cap, point, 8);
        memcpy(cap + 8, point, 8);
        memcpy(cap + 16, first, 8);
        memcpy(cap + 24, first, 8);
        uint32_t pole = (uint32_t)-90000000;
        for (size_t i = 12; i <= 20; i += 8) {
            cap[i] = pole >> 24;
            cap[i + 1] = pole >> 16;
            cap[i + 2] = pole >> 8;
            cap[i + 3] = pole;
        }
        if (!coast_chunk(state, cap, sizeof cap, polygon, level, false))
            return false;
    }
    state->points += n;
    ++state->polygons;
    return true;
}

/* Publish a validated shoreline dataset without changing existing astronomical data. */
bool shoreline_import(sqlite_t *db, file_t *source)
{
    if (!db || !source || !sqlite_exec_cstr(db, "begin immediate"))
        return false;
    bool ok = sqlite_exec_cstr(db,
        "drop table if exists shoreline_bounds;"
        "drop table if exists shoreline_chunk;"
        "drop table if exists shoreline_blob;"
        "drop table if exists shoreline_dataset;"
        "create table shoreline_dataset("
        " id integer primary key check( id=1 ),"
        " format_version integer not null,"
        " source text not null,"
        " point_count integer not null,"
        " polygon_count integer not null"
        ");"
        "create table shoreline_blob("
        " id integer primary key,"
        " vertices blob not null"
        ");"
        "create table shoreline_chunk("
        " id integer primary key,"
        " blob_id integer not null,"
        " byte_offset integer not null,"
        " byte_count integer not null,"
        " point_count integer not null,"
        " codec integer not null, "
        " polygon_id integer not null,"
        " level integer not null,"
        " boundary integer not null"
        ");"
        "create index shoreline_polygon_chunks on shoreline_chunk( polygon_id, id );"
        "create virtual table shoreline_bounds using rtree_i32( id,min_x, max_x, min_y, max_y );");
    coast_import_t state = {.page_id = 1};
    if (ok) {
        state.blob = sqlite_stmt_prepare(db, "insert into shoreline_blob values( ?1, ?2 )");
        state.chunk = sqlite_stmt_prepare(db, "insert into shoreline_chunk values( ?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9 )");
        state.bounds = sqlite_stmt_prepare(db, "insert into shoreline_bounds values( ?1, ?2, ?3, ?4, ?5 )");
        ok = state.blob && state.chunk && state.bounds;
    }
    while (ok) {
        unsigned char header[44];
        size_t count;
        ok = file_read(source, header, sizeof header, &count);
        if (!ok || !count)
            break;
        ok = count == sizeof header && coast_ring(&state, source, header);
    }
    ok = ok && state.polygons > 0 && coast_flush(&state);
    sqlite_stmt_finalize(state.blob);
    sqlite_stmt_finalize(state.chunk);
    sqlite_stmt_finalize(state.bounds);
    if (ok) {
        sqlite_stmt_t *metadata = sqlite_stmt_prepare(db,
            "insert into shoreline_dataset values( 1, 1, 'GSHHG 2.3.7', ?1, ?2 )");
        ok = metadata && sqlite_stmt_bind_int64(metadata, 1, state.points) &&
             sqlite_stmt_bind_int64(metadata, 2, state.polygons) && coast_execute(metadata);
        sqlite_stmt_finalize(metadata);
    }
    if (ok) ok = sqlite_exec_cstr(db, "commit");
    if (!ok) sqlite_exec_cstr(db, "rollback");
    return ok;
}
