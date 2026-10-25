/**
 * @file countries.c
 * @brief Import administrative polygons and resolve countries with bounded indexed ray intersections.
 *
 * Natural Earth polygon rings,
 * including holes, are split into 256-edge integer-microdegree chunks. An R-tree selects only chunks
 * intersecting an eastward ray; parity is accumulated per feature without loading complete countries.
 * Database streams and returned names follow the public globe API ownership rules. No network is used.
 * ISO identifiers join to jurisdiction-owned presentation metadata. Import also matches shared polygon edges
 * into indexed display chunks, omitting unmatched administrative coastlines. A bounded ordinary-heap buffer holds
 * only public source geometry during matching; no SQLCipher temporary table competes for locked memory.
 * Confirmed detailed land can bridge a gap of at most one kilometre in the generalised country outline,
 * provided only one country occurs within that distance. Water, overlaps and boundary points are not inferred.
 */
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MARS_GLOBE_INTERNAL_ACCESS
#include "globe_layers.h"

static uint32_t country_u32(const unsigned char *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static void country_put(unsigned char *p, int32_t value)
{
    uint32_t n = (uint32_t)value;
    p[0] = n; p[1] = n >> 8; p[2] = n >> 16; p[3] = n >> 24;
}

static double country_coord(const unsigned char *p)
{
    uint64_t bits = (uint64_t)country_u32(p) | (uint64_t)country_u32(p + 4) << 32;
    double value;
    memcpy(&value, &bits, sizeof value);
    return value;
}

static bool country_read(file_t *file, void *bytes, size_t length)
{
    size_t count = 0;
    return file_read(file, bytes, length, &count) && count == length;
}

static bool country_step(sqlite_stmt_t *q)
{
    bool ok = sqlite_stmt_step(q) == SQLITE_STEP_DONE;
    sqlite_stmt_reset(q);
    return ok;
}

static bool country_shape(sqlite_t *db, file_t *source, int id, int64_t *chunk_id)
{
    unsigned char header[8];
    if (!country_read(source, header, sizeof header)) return false;
    uint32_t words = (uint32_t)header[4] << 24 | (uint32_t)header[5] << 16 | (uint32_t)header[6] << 8 | header[7];
    if (words < 22 || words > 8 * 1024 * 1024) return false;
    size_t length = (size_t)words * 2;
    unsigned char *shape = malloc(length);
    bool ok = shape && country_read(source, shape, length) && country_u32(shape) == 5;
    uint32_t rings = ok ? country_u32(shape + 36) : 0, points = ok ? country_u32(shape + 40) : 0;
    ok = ok && rings > 0 && points >= 4 && rings <= points / 4 &&
         (uint64_t)44 + (uint64_t)rings * 4 + (uint64_t)points * 16 == length;
    sqlite_stmt_t *chunk = ok ? sqlite_stmt_prepare(db, "insert into country_chunk values(?1,?2,?3)") : NULL;
    sqlite_stmt_t *bounds = ok ? sqlite_stmt_prepare(db, "insert into country_bounds values(?1,?2,?3,?4,?5)") : NULL;
    ok = ok && chunk && bounds && country_u32(shape + 44) == 0;
    for (uint32_t ring = 0; ok && ring < rings; ++ring) {
        uint32_t first = country_u32(shape + 44 + ring * 4);
        uint32_t end = ring + 1 == rings ? points : country_u32(shape + 48 + ring * 4);
        ok = first < end && end <= points && end - first >= 4;
        const unsigned char *vertices = shape + 44 + rings * 4;
        if (ok) ok = !memcmp(vertices + first * 16, vertices + (end - 1) * 16, 16);
        for (uint32_t start = first; ok && start + 1 < end;) {
            uint32_t count = end - start > 257 ? 257 : end - start;
            unsigned char blob[257 * 8];
            int32_t west = INT32_MAX, east = INT32_MIN, south = INT32_MAX, north = INT32_MIN;
            for (uint32_t i = 0; ok && i < count; ++i) {
                double x = country_coord(vertices + (start + i) * 16);
                double y = country_coord(vertices + (start + i) * 16 + 8);
                ok = isfinite(x) && isfinite(y) && fabs(x) <= 180 && fabs(y) <= 90;
                if (!ok) break;
                int32_t ix = (int32_t)llround(x * 1000000), iy = (int32_t)llround(y * 1000000);
                country_put(blob + i * 8, ix); country_put(blob + i * 8 + 4, iy);
                if (ix < west) west = ix;
                if (ix > east) east = ix;
                if (iy < south) south = iy;
                if (iy > north) north = iy;
            }
            ++*chunk_id;
            ok = ok && sqlite_stmt_bind_int64(chunk, 1, *chunk_id) && sqlite_stmt_bind_int(chunk, 2, id) &&
                 sqlite_stmt_bind_blob(chunk, 3, blob, count * 8) && country_step(chunk) &&
                 sqlite_stmt_bind_int64(bounds, 1, *chunk_id) && sqlite_stmt_bind_int(bounds, 2, west) &&
                 sqlite_stmt_bind_int(bounds, 3, east) && sqlite_stmt_bind_int(bounds, 4, south) &&
                 sqlite_stmt_bind_int(bounds, 5, north) && country_step(bounds);
            start += count - 1;
        }
    }
    sqlite_stmt_finalize(chunk); sqlite_stmt_finalize(bounds); free(shape);
    return ok;
}

static bool country_border_chunk(sqlite_t *db, int id, const unsigned char *bytes, size_t length)
{
    int32_t west = INT32_MAX, east = INT32_MIN, south = INT32_MAX, north = INT32_MIN;
    for (size_t i = 0; i < length; i += 8) {
        int32_t x = (int32_t)country_u32(bytes + i), y = (int32_t)country_u32(bytes + i + 4);
        if (x < west) west = x;
        if (x > east) east = x;
        if (y < south) south = y;
        if (y > north) north = y;
    }
    sqlite_stmt_t *chunk = sqlite_stmt_prepare(db, "insert into country_border_chunk values(?1,?2)");
    sqlite_stmt_t *bounds = sqlite_stmt_prepare(db, "insert into country_border_bounds values(?1,?2,?3,?4,?5)");
    bool ok = chunk && bounds && sqlite_stmt_bind_int(chunk, 1, id) &&
              sqlite_stmt_bind_blob(chunk, 2, bytes, length) && country_step(chunk) &&
              sqlite_stmt_bind_int(bounds, 1, id) && sqlite_stmt_bind_int(bounds, 2, west) &&
              sqlite_stmt_bind_int(bounds, 3, east) && sqlite_stmt_bind_int(bounds, 4, south) &&
              sqlite_stmt_bind_int(bounds, 5, north) && country_step(bounds);
    sqlite_stmt_finalize(chunk); sqlite_stmt_finalize(bounds);
    return ok;
}

typedef struct country_edge {
    int32_t coordinates[4];
    int country;
} country_edge;

static int country_edge_compare(const void *left, const void *right)
{
    const country_edge *a = left, *b = right;
    for (unsigned i = 0; i < 4; ++i)
        if (a->coordinates[i] != b->coordinates[i]) return a->coordinates[i] < b->coordinates[i] ? -1 : 1;
    return 0;
}

static int country_edge_tile_compare(const void *left, const void *right)
{
    const country_edge *a = left, *b = right;
    for (unsigned i = 0; i < 2; ++i) {
        int x = a->coordinates[i] / 1000000, y = b->coordinates[i] / 1000000;
        if (x != y) return x < y ? -1 : 1;
    }
    return country_edge_compare(left, right);
}

/* Sort once, then compare adjacent equal edges; never scan the world for each segment. */
static bool country_import_borders(sqlite_t *db)
{
    sqlite_stmt_t *count = sqlite_stmt_prepare(db, "select sum(length(points)/8-1) from country_chunk");
    int64_t capacity = count && sqlite_stmt_step(count) == SQLITE_STEP_ROW ? sqlite_stmt_column_int64(count, 0) : 0;
    sqlite_stmt_finalize(count);
    /* At most 20 MiB of edge records, plus the C library sort workspace; never database contents or keys. */
    if (capacity <= 0 || capacity > 1048576) return false;
    country_edge *edges = malloc((size_t)capacity * sizeof(*edges));
    bool ok = edges && sqlite_exec_cstr(db,
        "drop table if exists country_border_bounds;drop table if exists country_border_chunk;"
        "create table country_border_chunk(id integer primary key,points blob not null);"
        "create virtual table country_border_bounds using rtree_i32(id,west,east,south,north)");
    sqlite_stmt_t *read = ok ? sqlite_stmt_prepare(db, "select country,points from country_chunk") : NULL;
    ok = ok && read;
    sqlite_step_result_t step = SQLITE_STEP_ERROR;
    size_t length_used = 0;
    while (ok && (step = sqlite_stmt_step(read)) == SQLITE_STEP_ROW) {
        int country = sqlite_stmt_column_int(read, 0);
        const unsigned char *bytes = sqlite_stmt_column_blob(read, 1);
        size_t length = sqlite_stmt_column_bytes(read, 1);
        ok = bytes && length >= 16 && length <= 257 * 8 && length % 8 == 0;
        for (size_t i = 8; ok && i < length; i += 8) {
            int32_t a[2] = {(int32_t)country_u32(bytes + i - 8), (int32_t)country_u32(bytes + i - 4)};
            int32_t b[2] = {(int32_t)country_u32(bytes + i), (int32_t)country_u32(bytes + i + 4)};
            if (a[0] == b[0] && a[1] == b[1]) continue;
            bool reverse = a[0] > b[0] || (a[0] == b[0] && a[1] > b[1]);
            const int32_t *first = reverse ? b : a, *last = reverse ? a : b;
            if (length_used >= (size_t)capacity) { ok = false; break; }
            edges[length_used++] = (country_edge){{first[0], first[1], last[0], last[1]}, country};
        }
    }
    ok = ok && step == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(read);
    size_t shared = 0;
    if (ok) {
        qsort(edges, length_used, sizeof(*edges), country_edge_compare);
        /* Equal-edge runs are contiguous; repeated edges within one country do not count as borders. */
        for (size_t i = 0; i < length_used;) {
            size_t end = i + 1;
            bool different = false;
            while (end < length_used && !country_edge_compare(edges + i, edges + end)) {
                different = different || edges[i].country != edges[end].country;
                ++end;
            }
            if (different) edges[shared++] = edges[i];
            i = end;
        }
        qsort(edges, shared, sizeof(*edges), country_edge_tile_compare);
    }
    unsigned char buffer[256 * 16];
    size_t used = 0;
    int id = 0, tile_x = INT32_MIN, tile_y = INT32_MIN;
    for (size_t n = 0; ok && n < shared; ++n) {
        int x = edges[n].coordinates[0] / 1000000, y = edges[n].coordinates[1] / 1000000;
        if (used && (used == sizeof buffer || x != tile_x || y != tile_y)) {
            ok = country_border_chunk(db, ++id, buffer, used); used = 0;
        }
        tile_x = x; tile_y = y;
        for (unsigned i = 0; i < 4; ++i) country_put(buffer + used + i * 4, edges[n].coordinates[i]);
        used += 16;
    }
    if (ok && used) ok = country_border_chunk(db, ++id, buffer, used);
    free(edges);
    return ok && id > 0;
}

/* Import the pinned polygon/attribute pair without retaining a global geometry tree. */
bool globe_import_countries(sqlite_t *db, file_t *shapes, file_t *attributes)
{
    unsigned char shp[100], dbf[32];
    if (!db || !country_read(shapes, shp, sizeof shp) || !country_read(attributes, dbf, sizeof dbf) ||
        shp[0] || shp[1] || shp[2] != 0x27 || shp[3] != 0x0a ||
        country_u32(shp + 28) != 1000 || country_u32(shp + 32) != 5) return false;
    uint32_t records = country_u32(dbf + 4);
    unsigned header = dbf[8] | (unsigned)dbf[9] << 8, width = dbf[10] | (unsigned)dbf[11] << 8;
    if (!records || records > 1023 || header < 65 || header % 32 != 1 || !width || width > 16384) return false;
    unsigned offset = 1, name_offset = 0, name_length = 0, code_offset = 0, unit_offset = 0;
    for (unsigned pos = 32; pos + 32 < header; pos += 32) {
        unsigned char field[32];
        if (!country_read(attributes, field, sizeof field)) return false;
        if (!memcmp(field, "NAME_EN\0", 8) && field[11] == 'C') {
            name_offset = offset; name_length = field[16];
        }
        if (!memcmp(field, "ISO_A2_EH\0", 10) && field[11] == 'C' && field[16] >= 2) code_offset = offset;
        if (!memcmp(field, "GU_A3\0", 6) && field[11] == 'C' && field[16] >= 3) unit_offset = offset;
        offset += field[16];
    }
    unsigned char terminator;
    if (!country_read(attributes, &terminator, 1) || terminator != 13 || offset != width || !name_length ||
        name_offset + name_length > width || !code_offset || code_offset + 2 > width ||
        (unit_offset && unit_offset + 3 > width)) return false;
    unsigned char *row = malloc(width);
    if (!row) return false;
    bool transaction = sqlite_exec_cstr(db, "savepoint country_import");
    bool ok = transaction && sqlite_exec_cstr(db,
        "drop table if exists country_bounds; drop table if exists country_chunk; drop table if exists country_name;"
        "create table country_name(id integer primary key,name text not null,iso_country_code text not null);"
        "create table country_chunk(id integer primary key,country integer not null,points blob not null);"
        "create virtual table country_bounds using rtree_i32(id,west,east,south,north);");
    sqlite_stmt_t *insert = ok ? sqlite_stmt_prepare(db, "insert into country_name values(?1,?2,?3)") : NULL;
    ok = ok && insert;
    int64_t chunks = 0;
    for (uint32_t i = 1; ok && i <= records; ++i) {
        ok = country_read(attributes, row, width) && row[0] == ' ';
        if (!ok) break;
        char name[256];
        memcpy(name, row + name_offset, name_length);
        unsigned n = name_length;
        while (n && name[n - 1] == ' ') --n;
        name[n] = 0;
        char code[7] = {(char)row[code_offset], (char)row[code_offset + 1], 0};
        if (code[0] < 'A' || code[0] > 'Z' || code[1] < 'A' || code[1] > 'Z') code[0] = 0;
        /* A fixed four-entry source-code mapping, not a per-point scan or nearest-town guess. */
        if (!strcmp(code, "GB") && unit_offset) {
            static const char *const units[] = {"ENG", "NIR", "SCT", "WLS"};
            for (unsigned u = 0; u < sizeof units / sizeof *units; ++u)
                if (!memcmp(row + unit_offset, units[u], 3)) {
                    code[2] = '-'; memcpy(code + 3, units[u], 4); break;
                }
        }
        ok = ok && n && sqlite_stmt_bind_int(insert, 1, (int)i) && sqlite_stmt_bind_text(insert, 2, name) &&
             sqlite_stmt_bind_text(insert, 3, code) && country_step(insert) && country_shape(db, shapes, (int)i, &chunks);
    }
    size_t extra = 0;
    ok = ok && file_read(shapes, shp, 1, &extra) && !extra;
    sqlite_stmt_finalize(insert); free(row);
    if (ok) ok = country_import_borders(db);
    if (ok) ok = sqlite_exec_cstr(db, "release country_import");
    if (!ok && transaction) sqlite_exec_cstr(db, "rollback to country_import; release country_import");
    return ok;
}

/* Accumulate parity across outer rings and holes, rejecting overlaps and boundary points. */
static string_t *country_lookup(sqlite_t *db, double latitude, double longitude, bool code, bool regional, bool *uncovered)
{
    if (uncovered) *uncovered = false;
    if (!db || !isfinite(latitude) || !isfinite(longitude) || fabs(latitude) > 90 || fabs(longitude) > 180) return NULL;
    double x = longitude * 1000000, y = latitude * 1000000;
    sqlite_stmt_t *q = sqlite_stmt_prepare(db, regional ?
        "select c.country,c.points from region_bounds b join region_chunk c on c.id=b.id "
        "where b.east>=?1 and b.south<=?2 and b.north>=?3 limit 10001" :
        "select c.country,c.points from country_bounds b join country_chunk c on c.id=b.id "
        "where b.east>=?1 and b.south<=?2 and b.north>=?3 limit 10001");
    bool ok = q && sqlite_stmt_bind_int(q, 1, (int)floor(x)) && sqlite_stmt_bind_int(q, 2, (int)ceil(y)) &&
              sqlite_stmt_bind_int(q, 3, (int)floor(y));
    bool parity[1024] = {false};
    unsigned rows = 0;
    sqlite_step_result_t step = SQLITE_STEP_ERROR;
    while (ok && (step = sqlite_stmt_step(q)) == SQLITE_STEP_ROW) {
        int id = sqlite_stmt_column_int(q, 0);
        const unsigned char *points = sqlite_stmt_column_blob(q, 1);
        size_t length = sqlite_stmt_column_bytes(q, 1);
        ok = ++rows <= 10000 && id > 0 && id < 1024 && points && length >= 16 && length <= 257 * 8 && length % 8 == 0;
        for (size_t i = 8; ok && i < length; i += 8) {
            double ax = (int32_t)country_u32(points + i - 8), ay = (int32_t)country_u32(points + i - 4);
            double bx = (int32_t)country_u32(points + i), by = (int32_t)country_u32(points + i + 4);
            double dx = bx - ax, dy = by - ay, squared = dx * dx + dy * dy;
            double t = squared ? fmax(0, fmin(1, ((x - ax) * dx + (y - ay) * dy) / squared)) : 0;
            if (hypot(x - ax - t * dx, y - ay - t * dy) <= 1) { ok = false; break; }
            if ((ay > y) != (by > y) && x < ax + (y - ay) * dx / dy) parity[id] = !parity[id];
        }
    }
    if (ok) ok = step == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(q);
    int found = 0;
    /* Fixed 1024-slot feature space; independent of the number of stored edges. */
    for (int id = 1; ok && id < 1024; ++id) if (parity[id]) {
        if (found) ok = false;
        found = id;
    }
    if (uncovered) *uncovered = ok && !found;
    q = ok && found ? sqlite_stmt_prepare(db, regional ? "select iso_country_code from region_name where id=?1" :
                                                  code ? "select iso_country_code from country_name where id=?1" :
                                                        "select name from country_name where id=?1") : NULL;
    string_t *name = q && sqlite_stmt_bind_int(q, 1, found) && sqlite_stmt_step(q) == SQLITE_STEP_ROW ?
                     string_new_with(sqlite_stmt_column_text(q, 0)) : NULL;
    sqlite_stmt_finalize(q);
    return name;
}

/* Search only the one-kilometre neighbourhood; competing country codes make the gap ambiguous. */
static string_t *country_land_gap(sqlite_t *db, double latitude, double longitude, bool code)
{
    const double radius = 1000, scale_y = 111195;
    double scale_x = fmax(1, scale_y * cos(latitude * 0.017453292519943295));
    double latitude_radius = radius / scale_y, longitude_radius = radius / scale_x;
    sqlite_stmt_t *q = sqlite_stmt_prepare(db,
        "select n.iso_country_code,n.name,c.points from country_bounds b "
        "join country_chunk c on c.id=b.id join country_name n on n.id=c.country "
        "where b.west<=?1 and b.east>=?2 and b.south<=?3 and b.north>=?4 limit 10001");
    bool ok = q != NULL;
    unsigned rows = 0;
    char selected[7] = {0};
    double nearest = INFINITY;
    string_t *result = NULL;
    for (int wrap = -1; ok && wrap <= 1; ++wrap) {
        double x = longitude + wrap * 360.0;
        ok = sqlite_stmt_bind_double(q, 1, ceil((x + longitude_radius) * 1000000)) &&
             sqlite_stmt_bind_double(q, 2, floor((x - longitude_radius) * 1000000)) &&
             sqlite_stmt_bind_double(q, 3, ceil((latitude + latitude_radius) * 1000000)) &&
             sqlite_stmt_bind_double(q, 4, floor((latitude - latitude_radius) * 1000000));
        sqlite_step_result_t step = SQLITE_STEP_ERROR;
        while (ok && (step = sqlite_stmt_step(q)) == SQLITE_STEP_ROW) {
            const unsigned char *points = sqlite_stmt_column_blob(q, 2);
            size_t length = sqlite_stmt_column_bytes(q, 2);
            ok = ++rows <= 10000 && points && length >= 16 && length <= 257 * 8 && length % 8 == 0;
            double distance = INFINITY;
            for (size_t i = 8; ok && i < length; i += 8) {
                double ax = (int32_t)country_u32(points + i - 8) * 0.000001;
                double ay = (int32_t)country_u32(points + i - 4) * 0.000001;
                double bx = (int32_t)country_u32(points + i) * 0.000001;
                double by = (int32_t)country_u32(points + i + 4) * 0.000001;
                if (fabs(bx - ax) > 180) continue;
                double dx = (bx - ax) * scale_x, dy = (by - ay) * scale_y;
                double px = (ax - x) * scale_x, py = (ay - latitude) * scale_y;
                double squared = dx * dx + dy * dy;
                double t = squared ? fmax(0, fmin(1, -(px * dx + py * dy) / squared)) : 0;
                distance = fmin(distance, hypot(px + t * dx, py + t * dy));
            }
            if (!ok || distance > radius) continue;
            const char *iso = sqlite_stmt_column_text(q, 0), *name = sqlite_stmt_column_text(q, 1);
            size_t size = iso ? strlen(iso) : 0;
            ok = (size == 2 || size == 6) && (!*selected || !strcmp(selected, iso)) && name && *name;
            if (!ok) break;
            memcpy(selected, iso, size + 1);
            if (distance < nearest) {
                nearest = distance;
                string_free(result);
                result = string_new_with(code ? iso : name);
                ok = result != NULL;
            }
        }
        ok = ok && step == SQLITE_STEP_DONE;
        sqlite_stmt_reset(q);
    }
    sqlite_stmt_finalize(q);
    if (!ok) { string_free(result); result = NULL; }
    return result;
}

/* Reconcile only uncovered administrative points that the detailed shoreline confirms are land. */
string_t *countries_resolve(sqlite_t *db, shoreline_t *shoreline, double latitude, double longitude, bool code)
{
    bool uncovered = false;
    string_t *result = country_lookup(db, latitude, longitude, code, false, &uncovered);
    if (uncovered && shoreline_hit_test_radius(shoreline, latitude, longitude, 0) == GLOBE_LAND)
        return country_land_gap(db, latitude, longitude, code);
    return result;
}

/* Keep regional display overlays separate from administrative country identification. */
string_t *regions_lookup(sqlite_t *db, double latitude, double longitude)
{
    return country_lookup(db, latitude, longitude, true, true, NULL);
}

/* Reuse bounded indexed parity for the independent country layer. */
string_t *countries_lookup(sqlite_t *db, double latitude, double longitude, bool code)
{
    return country_lookup(db, latitude, longitude, code, false, NULL);
}

/* Retain the descriptive lookup for existing callers and data without ISO metadata. */
string_t *countries_at(sqlite_t *db, double latitude, double longitude)
{
    return countries_lookup(db, latitude, longitude, false);
}

/* Query only viewport chunks, with three bounded longitude shifts to cover the date line. */
bool countries_borders(sqlite_t *db, double west, double south, double east, double north,
                        globe_border_segment_fn visit, void *context)
{
    if (!db || !visit || !isfinite(west) || !isfinite(east) || !isfinite(south) || !isfinite(north) ||
        west < -360 || west > 180 || east <= west || east - west > 360 || south < -90 || north > 90 || north <= south)
        return false;
    sqlite_stmt_t *q = sqlite_stmt_prepare(db,
        "select c.points from country_border_bounds b join country_border_chunk c on c.id=b.id "
        "where b.west<=?1 and b.east>=?2 and b.south<=?3 and b.north>=?4 limit 10001");
    bool ok = q != NULL;
    unsigned count = 0, chunks = 0;
    for (int shift = -360; ok && shift <= 360; shift += 360) {
        double w = fmax(-180, west - shift), e = fmin(180, east - shift);
        if (e <= w) continue;
        ok = sqlite_stmt_bind_int(q, 1, (int)ceil(e * 1000000)) &&
             sqlite_stmt_bind_int(q, 2, (int)floor(w * 1000000)) &&
             sqlite_stmt_bind_int(q, 3, (int)ceil(north * 1000000)) &&
             sqlite_stmt_bind_int(q, 4, (int)floor(south * 1000000));
        sqlite_step_result_t step = SQLITE_STEP_ERROR;
        while (ok && (step = sqlite_stmt_step(q)) == SQLITE_STEP_ROW) {
            const unsigned char *bytes = sqlite_stmt_column_blob(q, 0);
            size_t length = sqlite_stmt_column_bytes(q, 0);
            ok = ++chunks <= 10000 && bytes && length && length <= 4096 && length % 16 == 0;
            for (size_t i = 0; ok && i < length; i += 16) {
                double ax = (int32_t)country_u32(bytes + i) / 1000000.0 + shift;
                double ay = (int32_t)country_u32(bytes + i + 4) / 1000000.0;
                double bx = (int32_t)country_u32(bytes + i + 8) / 1000000.0 + shift;
                double by = (int32_t)country_u32(bytes + i + 12) / 1000000.0;
                if (fmax(ax, bx) < west || fmin(ax, bx) > east || fmax(ay, by) < south || fmin(ay, by) > north) continue;
                ok = ++count <= 250000 && visit(ax, ay, bx, by, context);
            }
        }
        ok = ok && step == SQLITE_STEP_DONE;
        sqlite_stmt_reset(q);
    }
    sqlite_stmt_finalize(q);
    return ok;
}
