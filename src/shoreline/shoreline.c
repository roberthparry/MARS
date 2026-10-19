/**
 * @file shoreline.c
 * @brief Indexed land, sea and inland-water classification from packed shoreline coordinates.
 *
 * Reads only spatially selected chunks from SQLCipher BLOB pages. Ray crossings are accumulated by nesting level;
 * nearby boundaries are deliberately uncertain. Coordinates remain integer microdegrees on disk, independent of
 * machine byte order. This is a geographic planning aid, not a navigational safety or surveyed-coastline service.
 */
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <zstd.h>

#include "shoreline-t.h"
#define MARS_SHORELINE_INTERNAL_ACCESS
#include "shoreline_internal.h"



static int32_t coast_coordinate(const unsigned char *p)
{
    uint32_t value = (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
    return value <= INT32_MAX ? (int32_t)value : (int32_t)((int64_t)value - 4294967296LL);
}

static bool coast_near(double ax, double ay, double bx, double by, double x, double y, double scale)
{
    ax = (ax - x) * scale;
    bx = (bx - x) * scale;
    ay = (ay - y) * 0.111195;
    by = (by - y) * 0.111195;
    double dx = bx - ax, dy = by - ay, length = dx * dx + dy * dy;
    double t = length > 0 ? fmax(0, fmin(1, -(ax * dx + ay * dy) / length)) : 0;
    return hypot(ax + t * dx, ay + t * dy) <= 500;
}

typedef struct {
    double x, y, scale;
    unsigned crossings;
    bool boundary;
} coast_hit_t;

static bool coast_cached_ring(const coast_geometry_t *geometry, const coast_ring_t *ring, void *context)
{
    coast_hit_t *hit = context;
    coast_vertex_t a = geometry->vertices[ring->first];
    for (uint32_t i = 1; i < ring->count; ++i) {
        uint32_t index = ring->first + i;
        coast_vertex_t b = geometry->vertices[index];
        if ((a.y > hit->y) != (b.y > hit->y) &&
            hit->x < a.x + (hit->y - a.y) * ((double)b.x - a.x) / ((double)b.y - a.y))
            hit->crossings ^= 1u << ring->level;
        if (geometry->boundary[index] && coast_near(a.x, a.y, b.x, b.y, hit->x, hit->y, hit->scale)) {
            hit->boundary = true;
            return false;
        }
        a = b;
    }
    return true;
}

static shoreline_surface_t coast_cached_hit(coast_geometry_t *geometry, double latitude, double longitude)
{
    coast_hit_t hit = {.y = latitude * 1000000, .scale = 0.111195 * cos(latitude * 0.017453292519943295)};
    double tx = hit.scale > 0.000001 ? 500 / hit.scale : 360000000, ty = 500 / 0.111195;
    for (int wrap = -1; !hit.boundary && wrap <= 1; ++wrap) {
        hit.x = longitude * 1000000 + wrap * 360000000.0;
        double bounds[] = {hit.x - tx, hit.y - ty, 360000000, hit.y + ty};
        coast_geometry_visit(geometry, bounds, 0, coast_cached_ring, &hit);
    }
    if (hit.boundary) return SHORELINE_COAST;
    for (unsigned level = 4; level; --level)
        if (hit.crossings & (1u << level)) return level % 2 ? SHORELINE_LAND : SHORELINE_INLAND_WATER;
    return SHORELINE_SEA;
}

/* Prepare one reusable spatial query against a borrowed database connection. */
shoreline_t *shoreline_open(sqlite_t *db)
{
    if (!db) return NULL;
    sqlite_stmt_t *metadata = sqlite_stmt_prepare(db,
        "SELECT format_version FROM shoreline_dataset WHERE id=1 AND point_count>0");
    bool ready = metadata && sqlite_stmt_step(metadata) == SQLITE_STEP_ROW && sqlite_stmt_column_int(metadata, 0) == 1;
    sqlite_stmt_finalize(metadata);
    if (!ready) {
        return NULL;
    }
    sqlite_stmt_t *query = sqlite_stmt_prepare(db,
        "SELECT c.level,c.codec,c.point_count,CASE WHEN length(b.vertices)<=65536 "
        "AND c.byte_offset>=0 AND c.byte_count BETWEEN 1 AND 2056 "
        "AND c.byte_offset<=length(b.vertices)-c.byte_count "
        "THEN substr(b.vertices,c.byte_offset+1,c.byte_count) END,c.boundary "
        "FROM shoreline_bounds r JOIN shoreline_chunk c ON c.id=r.id "
        "JOIN shoreline_blob b ON b.id=c.blob_id "
        "WHERE r.max_x>=?1 AND r.min_y<=?2 AND r.max_y>=?3");
    shoreline_t *shoreline = query ? malloc(sizeof *shoreline) : NULL;
    if (!shoreline) {
        sqlite_stmt_finalize(query);
        return NULL;
    }
    shoreline->query = query;
    shoreline->db = db;
    shoreline->local_geometry = NULL;
    return shoreline;
}

/* Release prepared statements without taking ownership of the connection. */
void shoreline_close(shoreline_t *shoreline)
{
    if (!shoreline) return;
    sqlite_stmt_finalize(shoreline->query);
    coast_geometry_release(shoreline->local_geometry);
    free(shoreline);
}

/* Classify a point with bounded decompression of spatially selected chunks. */
shoreline_surface_t shoreline_hit_test(shoreline_t *shoreline, double latitude_degrees, double longitude_degrees)
{
    if (!shoreline || !isfinite(latitude_degrees) || !isfinite(longitude_degrees) ||
        fabs(latitude_degrees) > 90 || fabs(longitude_degrees) > 180)
        return SHORELINE_UNKNOWN;
    coast_geometry_t *geometry = coast_geometry_acquire(shoreline, false);
    if (geometry) {
        shoreline_surface_t surface = coast_cached_hit(geometry, latitude_degrees, longitude_degrees);
        coast_geometry_release(geometry);
        return surface;
    }
    sqlite_stmt_t *query = shoreline->query;
    unsigned crossings = 0;
    bool boundary = false, ok = query != NULL;
    double y = latitude_degrees * 1000000, scale = 0.111195 * cos(latitude_degrees * 0.017453292519943295);
    /* At the poles the east/west scale tends to zero; retain all longitude candidates. */
    double tolerance_x = scale > 0.000001 ? 500 / scale : 360000000;
    double tolerance_y = 500 / 0.111195;
    for (int wrap = -1; ok && wrap <= 1; ++wrap) {
        double x = longitude_degrees * 1000000 + wrap * 360000000.0;
        ok = sqlite_stmt_bind_double(query, 1, x - tolerance_x) &&
             sqlite_stmt_bind_double(query, 2, y + tolerance_y) &&
             sqlite_stmt_bind_double(query, 3, y - tolerance_y);
        sqlite_step_result_t step;
        while (ok && (step = sqlite_stmt_step(query)) == SQLITE_STEP_ROW) {
            int level = sqlite_stmt_column_int(query, 0);
            int codec = sqlite_stmt_column_int(query, 1), points = sqlite_stmt_column_int(query, 2);
            int physical = sqlite_stmt_column_int(query, 4);
            const unsigned char *bytes = sqlite_stmt_column_blob(query, 3);
            size_t length = sqlite_stmt_column_bytes(query, 3);
            unsigned char decoded[2056];
            if (level < 1 || level > 4 || points < 2 || points > 257 || !bytes || !length || length > 2056 ||
                (codec != SHORELINE_CODEC_RAW && codec != SHORELINE_CODEC_ZSTD) || (physical != 0 && physical != 1)) {
                ok = false;
                break;
            }
            if (codec == SHORELINE_CODEC_ZSTD) {
                length = ZSTD_decompress(decoded, sizeof decoded, bytes, length);
                bytes = decoded;
            }
            if (length != (size_t)points * 8) {
                ok = false;
                break;
            }
            double ax = coast_coordinate(bytes), ay = coast_coordinate(bytes + 4);
            for (size_t offset = 8; offset < length; offset += 8) {
                double bx = coast_coordinate(bytes + offset), by = coast_coordinate(bytes + offset + 4);
                if ((ay > y) != (by > y) && x < ax + (y - ay) * (bx - ax) / (by - ay))
                    crossings ^= 1u << level;
                boundary = boundary || (physical && coast_near(ax, ay, bx, by, x, y, scale));
                ax = bx;
                ay = by;
            }
        }
        if (ok)
            ok = step == SQLITE_STEP_DONE;
        sqlite_stmt_reset(query);
    }
    sqlite_stmt_reset(query);
    if (!ok) {
        return SHORELINE_UNKNOWN;
    }
    if (boundary)
        return SHORELINE_COAST;
    for (unsigned level = 4; level; --level)
        if (crossings & (1u << level))
            return level % 2 ? SHORELINE_LAND : SHORELINE_INLAND_WATER;
    return SHORELINE_SEA;
}
