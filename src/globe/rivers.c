/**
 * @file rivers.c
 * @brief Bounded R-tree river queries and rectangular segment clipping.
 *
 * Streams only chunks intersecting the requested window, including wrapped longitude windows. Each query owns no
 * global geometry cache. Clipping retains short waterways and never changes shoreline land/water classification.
 */
#include <math.h>
#include <stdlib.h>

#define MARS_GLOBE_INTERNAL_ACCESS
#include "globe_layers.h"
#define MARS_RIVERS_INTERNAL_ACCESS
#include "rivers_internal.h"

typedef struct _rivers_t {
    sqlite_stmt_t *query;
    sqlite_stmt_t *width_query;
    sqlite_stmt_t *bank_query;
    sqlite_stmt_t *coverage_query;
    int64_t width_id;
    double width;
    bool busy;
    bool anchors;
} rivers_t;

/* Only an explicit complete-survey rectangle establishes that unpainted pixels are land. */
bool rivers_water_complete(rivers_t *r, double west, double south, double east, double north)
{
    if (!r || !r->coverage_query || !isfinite(west) || !isfinite(south) || !isfinite(east) || !isfinite(north) ||
        west < -180 || east > 180 || south < -90 || north > 90 || west >= east || south >= north) return false;
    sqlite_stmt_t *q = r->coverage_query;
    bool covered = sqlite_stmt_bind_int64(q, 1, (int64_t)floor(west * 1000000)) &&
                   sqlite_stmt_bind_int64(q, 2, (int64_t)floor(south * 1000000)) &&
                   sqlite_stmt_bind_int64(q, 3, (int64_t)ceil(east * 1000000)) &&
                   sqlite_stmt_bind_int64(q, 4, (int64_t)ceil(north * 1000000)) &&
                   sqlite_stmt_step(q) == SQLITE_STEP_ROW;
    sqlite_stmt_reset(q);
    return covered;
}

int32_t rivers_coordinate(const unsigned char *p)
{
    uint32_t n = (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
    return n <= INT32_MAX ? (int32_t)n : (int32_t)((int64_t)n - 4294967296LL);
}

/* Prepare an indexed query without changing the borrowed database. */
rivers_t *rivers_open(sqlite_t *db)
{
    if (!db) return NULL;
    sqlite_stmt_t *meta = sqlite_stmt_prepare(db,
        "select format_version from rivers_dataset where id=1 and point_count>0");
    int version = meta && sqlite_stmt_step(meta) == SQLITE_STEP_ROW ? sqlite_stmt_column_int(meta, 0) : 0;
    bool ready = version == 1 || version == 2;
    sqlite_stmt_finalize(meta);
    if (!ready) return NULL;
    rivers_t *r = calloc(1, sizeof *r);
    if (!r) return NULL;
    r->anchors = version == 2;
    r->query = sqlite_stmt_prepare(db, r->anchors ?
        "select c.feature_id,c.kind,case when length(c.vertices) between 16 and 2056 then c.vertices end,c.anchors "
        "from rivers_bounds b join rivers_chunk c on c.id=b.id "
        "where b.max_x>=?1 and b.min_x<=?2 and b.max_y>=?3 and b.min_y<=?4" :
        "select c.feature_id,c.kind,case when length(c.vertices) between 16 and 2056 "
        "then c.vertices end from rivers_bounds b join rivers_chunk c on c.id=b.id "
        "where b.max_x>=?1 and b.min_x<=?2 and b.max_y>=?3 and b.min_y<=?4");
    sqlite_stmt_t *tables = sqlite_stmt_prepare(db,
        "select 1 from sqlite_master where type='table' and name='rivers_width'");
    bool widths = tables && sqlite_stmt_step(tables) == SQLITE_STEP_ROW;
    sqlite_stmt_finalize(tables);
    if (widths) r->width_query = sqlite_stmt_prepare(db, "select metres from rivers_width where feature_id=?1");
    tables = sqlite_stmt_prepare(db, "select 1 from sqlite_master where type='table' and name='rivers_bank'");
    bool banks = tables && sqlite_stmt_step(tables) == SQLITE_STEP_ROW;
    sqlite_stmt_finalize(tables);
    if (banks) r->bank_query = sqlite_stmt_prepare(db,
        "select case when length(c.rings) between 40 and 4194304 then c.rings end "
        "from rivers_bank_bounds b join rivers_bank c on c.id=b.id "
        "where b.max_x>=?1 and b.min_x<=?2 and b.max_y>=?3 and b.min_y<=?4");
    r->width_id = -1;
    tables = sqlite_stmt_prepare(db, "select 1 from sqlite_master where name='rivers_water_coverage'");
    bool coverage = tables && sqlite_stmt_step(tables) == SQLITE_STEP_ROW;
    sqlite_stmt_finalize(tables);
    if (coverage) r->coverage_query = sqlite_stmt_prepare(db,
        "select 1 from rivers_water_coverage where min_x<=?1 and min_y<=?2 and max_x>=?3 and max_y>=?4 limit 1");
    if (coverage && !r->coverage_query) { rivers_close(r); return NULL; }
    if (!r->query || (widths && !r->width_query) || (banks && !r->bank_query)) { rivers_close(r); return NULL; }
    return r;
}

/* Release query ownership, retaining the caller's connection. */
void rivers_close(rivers_t *r)
{
    if (!r) return;
    sqlite_stmt_finalize(r->query);
    sqlite_stmt_finalize(r->width_query);
    sqlite_stmt_finalize(r->bank_query);
    sqlite_stmt_finalize(r->coverage_query);
    free(r);
}

/* Adjacent segments share a feature: one indexed lookup serves all of its vertices. */
bool rivers_width_metres(rivers_t *r, int64_t id, double *width)
{
    if (!r || !width || id < 0) return false;
    if (r->width_id != id) {
        double value = 0;
        bool ok = true;
        if (r->width_query) {
            sqlite_stmt_t *q = r->width_query;
            ok = sqlite_stmt_bind_int64(q, 1, id);
            sqlite_step_result_t step = ok ? sqlite_stmt_step(q) : SQLITE_STEP_ERROR;
            if (step == SQLITE_STEP_ROW) {
                value = sqlite_stmt_column_double(q, 0);
                ok = isfinite(value) && value > 0 && value <= 100000;
            } else ok = step == SQLITE_STEP_DONE;
            sqlite_stmt_reset(q);
        }
        if (!ok) return false;
        r->width_id = id;
        r->width = value;
    }
    *width = r->width;
    return true;
}

static bool rivers_clip(double p, double q, double *start, double *end)
{
    if (p == 0) return q >= 0;
    double t = q / p;
    if (p < 0) {
        if (t > *end) return false;
        if (t > *start) *start = t;
    } else {
        if (t < *start) return false;
        if (t < *end) *end = t;
    }
    return true;
}

static bool bank_polygon(const unsigned char *blob, size_t bytes, double shift,
                         globe_river_bank_fn visit, void *context)
{
    if (!blob || bytes < 40 || bytes > 4194304) return false;
    int32_t rings = rivers_coordinate(blob);
    if (rings < 1 || rings > 4096 || (size_t)rings > (bytes - 4) / 36) return false;
    size_t *sizes = calloc((size_t)rings, sizeof *sizes);
    double *coordinates = malloc(bytes * 2);
    bool ok = sizes && coordinates;
    size_t offset = 4, points = 0;
    for (int32_t ring = 0; ok && ring < rings; ++ring) {
        if (bytes - offset < 4) { ok = false; break; }
        int32_t count = rivers_coordinate(blob + offset);
        offset += 4;
        if (count < 4 || (size_t)count > (bytes - offset) / 8) { ok = false; break; }
        sizes[ring] = (size_t)count;
        size_t first = points;
        for (int32_t i = 0; i < count; ++i) {
            double x = rivers_coordinate(blob + offset) / 1e6;
            double y = rivers_coordinate(blob + offset + 4) / 1e6;
            if (fabs(x) > 180 || fabs(y) > 90) ok = false;
            coordinates[2 * points] = x + shift;
            coordinates[2 * points + 1] = y;
            ++points;
            offset += 8;
        }
        if (coordinates[2 * first] != coordinates[2 * (points - 1)] ||
            coordinates[2 * first + 1] != coordinates[2 * (points - 1) + 1]) ok = false;
    }
    ok = ok && offset == bytes && visit(coordinates, sizes, (size_t)rings, context);
    free(coordinates); free(sizes);
    return ok;
}

/* Polygon queries use the same wrapped longitude frames as centreline queries. */
bool rivers_banks(rivers_t *r, double west, double south, double east, double north,
                   globe_river_bank_fn visit, void *context)
{
    if (!r || r->busy || !visit || !isfinite(west) || !isfinite(east) || !isfinite(south) || !isfinite(north) ||
        west < -360 || west > 180 || east <= west || east - west > 360 || south < -90 || north > 90 || north <= south)
        return false;
    if (!r->bank_query) return true;
    r->busy = true;
    bool ok = true;
    size_t total = 0;
    sqlite_stmt_t *q = r->bank_query;
    for (int wrap = -2; ok && wrap <= 2; ++wrap) {
        double shift = wrap * 360.;
        if (east - shift < -180 || west - shift > 180) continue;
        ok = sqlite_stmt_bind_double(q, 1, (west - shift) * 1e6) &&
             sqlite_stmt_bind_double(q, 2, (east - shift) * 1e6) &&
             sqlite_stmt_bind_double(q, 3, south * 1e6) && sqlite_stmt_bind_double(q, 4, north * 1e6);
        sqlite_step_result_t step = SQLITE_STEP_DONE;
        while (ok && (step = sqlite_stmt_step(q)) == SQLITE_STEP_ROW) {
            size_t bytes = sqlite_stmt_column_bytes(q, 0);
            total += bytes;
            ok = total <= 8 * 1024 * 1024 && bank_polygon(sqlite_stmt_column_blob(q, 0), bytes, shift, visit, context);
        }
        if (ok) ok = step == SQLITE_STEP_DONE;
        sqlite_stmt_reset(q);
    }
    r->busy = false;
    return ok;
}

/* Stream exact segments; clip only to the requested display window. */
bool rivers_segments(rivers_t *r, double west, double south, double east, double north,
                     rivers_segment_fn visit, void *context)
{
    return rivers_display_segments(r, west, south, east, north, 0, visit, context);
}

static void rivers_reduce(const double x[257], const double y[257], unsigned char keep[257],
                           unsigned first, unsigned last, double tolerance)
{
    if (last <= first + 1) return;
    double dx = x[last] - x[first], dy = y[last] - y[first], square = dx * dx + dy * dy;
    double maximum = tolerance * tolerance;
    unsigned split = 0;
    for (unsigned i = first + 1; i < last; ++i) {
        if (keep[i]) { split = i; break; }
        double t = square ? ((x[i] - x[first]) * dx + (y[i] - y[first]) * dy) / square : 0;
        t = fmax(0, fmin(1, t));
        double ex = x[i] - x[first] - t * dx, ey = y[i] - y[first] - t * dy;
        double distance = ex * ex + ey * ey;
        if (distance > maximum) { maximum = distance; split = i; }
    }
    if (split) {
        keep[split] = 1;
        rivers_reduce(x, y, keep, first, split, tolerance);
        rivers_reduce(x, y, keep, split, last, tolerance);
    }
}

/* Reduce only bounded chunks; junction anchors and all exact stored coordinates survive. */
bool rivers_display_segments(rivers_t *r, double west, double south, double east, double north,
                             double tolerance, rivers_segment_fn visit, void *context)
{
    if (!r || r->busy || !visit || !isfinite(west) || !isfinite(south) || !isfinite(east) || !isfinite(north) ||
        !isfinite(tolerance) || tolerance < 0 || tolerance > 1 ||
        west < -360 || west > 180 || east <= west || east - west > 360 || south < -90 || north > 90 || north <= south)
        return false;
    r->busy = true;
    bool ok = true;
    sqlite_stmt_t *q = r->query;
    for (int wrap = -2; ok && wrap <= 2; ++wrap) {
        double shift = wrap * 360., left = west - shift, right = east - shift;
        if (right < -180 || left > 360) continue;
        ok = sqlite_stmt_bind_double(q, 1, left * 1e6) && sqlite_stmt_bind_double(q, 2, right * 1e6) &&
             sqlite_stmt_bind_double(q, 3, south * 1e6) && sqlite_stmt_bind_double(q, 4, north * 1e6);
        sqlite_step_result_t step = SQLITE_STEP_DONE;
        while (ok && (step = sqlite_stmt_step(q)) == SQLITE_STEP_ROW) {
            const unsigned char *p = sqlite_stmt_column_blob(q, 2);
            size_t length = sqlite_stmt_column_bytes(q, 2);
            int64_t id = sqlite_stmt_column_int64(q, 0);
            int kind = sqlite_stmt_column_int(q, 1);
            if (!p || length < 16 || length > 2056 || length % 8 || id < 0 || kind < 0 || kind > 255) {
                ok = false;
                break;
            }
            unsigned count = (unsigned)(length / 8);
            double x[257], y[257];
            unsigned char keep[257];
            const unsigned char *anchors = r->anchors ? sqlite_stmt_column_blob(q, 3) : NULL;
            if (r->anchors && (!anchors || sqlite_stmt_column_bytes(q, 3) != count)) { ok = false; break; }
            for (unsigned i = 0; i < count; ++i) {
                x[i] = rivers_coordinate(p + i * 8) / 1e6 + shift;
                y[i] = rivers_coordinate(p + i * 8 + 4) / 1e6;
                keep[i] = !tolerance || !anchors || anchors[i] || !i || i + 1 == count;
                if (fabs(y[i]) > 90 || x[i] - shift < -180 || x[i] - shift > 360 ||
                    (i && fabs(x[i] - x[i - 1]) > 180) || (anchors && anchors[i] > 1)) ok = false;
            }
            if (!ok) break;
            if (tolerance && anchors) rivers_reduce(x, y, keep, 0, count - 1, tolerance);
            double ax = x[0], ay = y[0];
            for (unsigned i = 1; ok && i < count; ++i) {
                if (!keep[i]) continue;
                double bx = x[i], by = y[i];
                double dx = bx - ax, dy = by - ay, start = 0, end = 1;
                if (rivers_clip(-dx, ax - west, &start, &end) && rivers_clip(dx, east - ax, &start, &end) &&
                    rivers_clip(-dy, ay - south, &start, &end) && rivers_clip(dy, north - ay, &start, &end) && end > start)
                    ok = visit(id, (unsigned)kind, ax + start * dx, ay + start * dy,
                               ax + end * dx, ay + end * dy, context);
                ax = bx;
                ay = by;
            }
        }
        if (ok) ok = step == SQLITE_STEP_DONE;
        sqlite_stmt_reset(q);
    }
    r->busy = false;
    return ok;
}
