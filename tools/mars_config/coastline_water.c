/**
 * @file coastline_water.c
 * @brief Offline incorporation of reviewed marine-water areas into a shoreline candidate.
 *
 * Subtracts a valid supplied marine-water polygon from intersecting mainland shells,
 * retaining water holes as islands and deriving ring levels through a spatial index.
 * Works on an exclusive database copy, auditing source geometry and replacements.
 * Rejects overlapping nested layers and polar edits rather than guessing their policy.
 * An optional audited source path restores one simplified join within eight metres.
 * No snapping, simplification, validity repair or installation is performed. A fresh
 * global boundary review and native geographic acceptance are required afterwards.
 */
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <geos_c.h>
#include <sqlite3.h>

#include "file-t.h"
#include "string-t.h"

typedef struct _water_piece_t {
    GEOSGeometry *geometry;
    double area;
    bool land;
    unsigned level;
} water_piece_t;

typedef struct _water_nesting_t {
    GEOSContextHandle_t geos;
    water_piece_t *child;
    GEOSGeometry *point;
    unsigned depth;
    bool ok;
} water_nesting_t;

static bool sql(sqlite3 *db, const char *text)
{
    bool ok = sqlite3_exec(db, text, NULL, NULL, NULL) == SQLITE_OK;
    if (!ok) fprintf(stderr, "Marine composition SQL: %s\n", sqlite3_errmsg(db));
    return ok;
}

static void parent(void *item, void *context)
{
    water_piece_t *candidate = item;
    water_nesting_t *state = context;
    if (!state->ok || candidate == state->child || candidate->area <= state->child->area) return;
    char contains = GEOSContains_r(state->geos, candidate->geometry, state->point);
    if (contains == 1) {
        state->ok = GEOSCovers_r(state->geos, candidate->geometry, state->child->geometry) == 1;
        ++state->depth;
    } else if (contains != 0) state->ok = false;
}

static void free_pieces(GEOSContextHandle_t geos, water_piece_t *pieces, unsigned count)
{
    for (unsigned i = 0; i < count; ++i) GEOSGeom_destroy_r(geos, pieces[i].geometry);
    free(pieces);
}

/* Shells and holes receive geometric nesting levels, not assumptions about component order. */
static water_piece_t *split(GEOSContextHandle_t geos, const GEOSGeometry *geometry, unsigned *count)
{
    *count = 0;
    int type = GEOSGeomTypeId_r(geos, geometry);
    int polygons = type == GEOS_POLYGON ? 1 : type == GEOS_MULTIPOLYGON ? GEOSGetNumGeometries_r(geos, geometry) : 0;
    if (polygons < 1 || polygons > 10000 || GEOSisValid_r(geos, geometry) != 1 || GEOSisEmpty_r(geos, geometry) != 0)
        return NULL;
    unsigned capacity = 0;
    for (int i = 0; i < polygons; ++i) {
        const GEOSGeometry *polygon = type == GEOS_POLYGON ? geometry : GEOSGetGeometryN_r(geos, geometry, i);
        int holes = GEOSGetNumInteriorRings_r(geos, polygon);
        if (holes < 0 || holes > 10000 || capacity + 1 + (unsigned)holes > 10000) return NULL;
        capacity += 1 + (unsigned)holes;
    }
    water_piece_t *pieces = calloc(capacity, sizeof *pieces);
    GEOSSTRtree *index = GEOSSTRtree_create_r(geos, 10);
    bool ok = pieces && index;
    for (int i = 0; ok && i < polygons; ++i) {
        const GEOSGeometry *polygon = type == GEOS_POLYGON ? geometry : GEOSGetGeometryN_r(geos, geometry, i);
        int holes = GEOSGetNumInteriorRings_r(geos, polygon);
        for (int j = -1; ok && j < holes; ++j) {
            const GEOSGeometry *ring = j < 0 ? GEOSGetExteriorRing_r(geos, polygon) :
                                             GEOSGetInteriorRingN_r(geos, polygon, j);
            GEOSGeometry *copy = GEOSGeom_clone_r(geos, ring);
            water_piece_t *piece = &pieces[(*count)++];
            piece->geometry = copy ? GEOSGeom_createPolygon_r(geos, copy, NULL, 0) : NULL;
            piece->land = j < 0;
            ok = piece->geometry && GEOSArea_r(geos, piece->geometry, &piece->area) && piece->area > 0;
            if (ok) GEOSSTRtree_insert_r(geos, index, piece->geometry, piece);
        }
    }
    for (unsigned i = 0; ok && i < *count; ++i) {
        water_nesting_t state = {.geos = geos, .child = &pieces[i], .ok = true};
        state.point = GEOSPointOnSurface_r(geos, pieces[i].geometry);
        ok = state.point != NULL;
        if (ok) GEOSSTRtree_query_r(geos, index, state.point, parent, &state);
        pieces[i].level = state.depth + 1;
        ok = ok && state.ok && pieces[i].level <= 4 && (pieces[i].level % 2 != 0) == pieces[i].land;
        GEOSGeom_destroy_r(geos, state.point);
    }
    if (index) GEOSSTRtree_destroy_r(geos, index);
    if (!ok) { free_pieces(geos, pieces, *count); *count = 0; return NULL; }
    return pieces;
}

static GEOSGeometry *subtract(GEOSContextHandle_t geos, const GEOSGeometry *land, const GEOSGeometry *water)
{
    if (GEOSisValid_r(geos, land) != 1 || GEOSisValid_r(geos, water) != 1) return NULL;
    GEOSGeometry *result = GEOSDifference_r(geos, land, water);
    GEOSGeometry *removed = result ? GEOSDifference_r(geos, land, result) : NULL;
    GEOSGeometry *expected = GEOSIntersection_r(geos, land, water);
    GEOSGeometry *error = removed && expected ? GEOSSymDifference_r(geos, removed, expected) : NULL;
    GEOSGeometry *boundary_check = result ? GEOSBoundary_r(geos, result) : NULL;
    double area = -1;
    bool ok = result && GEOSisValid_r(geos, result) == 1 && error && GEOSArea_r(geos, error, &area) &&
        boundary_check && GEOSisSimple_r(geos, boundary_check) == 1;
    GEOSGeom_destroy_r(geos, boundary_check);
    if (ok && area != 0) {
        /* Repeated floating overlays can leave sub-micrometre slivers at computed intersections.
         * Bound both their total area and their location; never buffer or snap the output geometry. */
        const double roundoff = 1e-9; /* At most 0.12 mm, solely for verifying the overlay. */
        double west, east, south, north;
        bool bounds = GEOSGeom_getXMin_r(geos, error, &west) && GEOSGeom_getXMax_r(geos, error, &east) &&
            GEOSGeom_getYMin_r(geos, error, &south) && GEOSGeom_getYMax_r(geos, error, &north);
        GEOSGeometry *land_boundary = GEOSBoundary_r(geos, land), *water_boundary = GEOSBoundary_r(geos, water);
        GEOSGeometry *a = bounds && land_boundary ? GEOSClipByRect_r(geos, land_boundary,
            west - roundoff * 4, south - roundoff * 4, east + roundoff * 4, north + roundoff * 4) : NULL;
        GEOSGeometry *b = bounds && water_boundary ? GEOSClipByRect_r(geos, water_boundary,
            west - roundoff * 4, south - roundoff * 4, east + roundoff * 4, north + roundoff * 4) : NULL;
        GEOSGeometry *boundary = a && b ? GEOSUnion_r(geos, a, b) : NULL;
        GEOSGeometry *band = boundary ? GEOSBuffer_r(geos, boundary, roundoff, 2) : NULL;
        ok = area < 1e-14 && band && GEOSCovers_r(geos, band, error) == 1;
        if (!ok) {
            char *description = GEOSGeomToWKT_r(geos, error);
            fprintf(stderr, "Overlay discrepancy: %.3000s\n", description ? description : "unavailable");
            GEOSFree_r(geos, description);
        }
        fprintf(stderr, "Overlay round-off check: %s; area %.17g deg^2, boundary guard %.17g degrees\n",
                ok ? "PASS" : "FAIL", area, roundoff);
        GEOSGeom_destroy_r(geos, band); GEOSGeom_destroy_r(geos, boundary);
        GEOSGeom_destroy_r(geos, a); GEOSGeom_destroy_r(geos, b);
        GEOSGeom_destroy_r(geos, land_boundary); GEOSGeom_destroy_r(geos, water_boundary);
    }
    if (!ok) fprintf(stderr, "Marine subtraction proof: result type %d, valid %d, difference area %.17g\n",
        result ? GEOSGeomTypeId_r(geos, result) : -1, result ? GEOSisValid_r(geos, result) : -1, area);
    GEOSGeom_destroy_r(geos, removed); GEOSGeom_destroy_r(geos, expected); GEOSGeom_destroy_r(geos, error);
    if (!ok) { GEOSGeom_destroy_r(geos, result); return NULL; }
    return result;
}

/* Restore an explicitly supplied source path only across one matching simplified edge. */
static GEOSGeometry *restore_join(GEOSContextHandle_t geos, const GEOSGeometry *land, const GEOSGeometry *join)
{
    if (!join) return GEOSGeom_clone_r(geos, land);
    if (GEOSGeomTypeId_r(geos, land) != GEOS_POLYGON || GEOSGetNumInteriorRings_r(geos, land) != 0 ||
        GEOSGeomTypeId_r(geos, join) != GEOS_LINESTRING || GEOSisSimple_r(geos, join) != 1) return NULL;
    const GEOSCoordSequence *path = GEOSGeom_getCoordSeq_r(geos, join);
    const GEOSCoordSequence *ring = GEOSGeom_getCoordSeq_r(geos, GEOSGetExteriorRing_r(geos, land));
    unsigned n = 0, m = 0;
    if (!GEOSCoordSeq_getSize_r(geos, ring, &n) || !GEOSCoordSeq_getSize_r(geos, path, &m) ||
        n < 4 || m < 3 || m > 4096) return NULL;
    double ax, ay, bx, by;
    if (!GEOSCoordSeq_getXY_r(geos, path, 0, &ax, &ay) ||
        !GEOSCoordSeq_getXY_r(geos, path, m - 1, &bx, &by) || fabs(ay) > 85 || fabs(by) > 85 ||
        fabs(bx - ax) > 1 || fabs(by - ay) > 1) return NULL;
    /* A conservative metres-per-degree bound; no coordinate rounding or displacement. */
    double dx = bx - ax, dy = by - ay, length2 = dx * dx + dy * dy;
    if (length2 == 0) return NULL;
    for (unsigned i = 1; i + 1 < m; ++i) {
        double x, y;
        if (!GEOSCoordSeq_getXY_r(geos, path, i, &x, &y)) return NULL;
        double t = fmax(0, fmin(1, ((x - ax) * dx + (y - ay) * dy) / length2));
        if (!(hypot(x - ax - t * dx, y - ay - t * dy) * 112000 <= 8)) return NULL;
    }
    unsigned edge = n, matches = 0;
    bool reverse = false;
    for (unsigned i = 0; i + 1 < n; ++i) {
        double x, y, xx, yy;
        if (!GEOSCoordSeq_getXY_r(geos, ring, i, &x, &y) ||
            !GEOSCoordSeq_getXY_r(geos, ring, i + 1, &xx, &yy)) return NULL;
        bool forward = x == ax && y == ay && xx == bx && yy == by;
        bool backward = x == bx && y == by && xx == ax && yy == ay;
        if (forward || backward) { edge = i; reverse = backward; ++matches; }
    }
    if (matches != 1) return NULL;
    GEOSCoordSequence *coords = GEOSCoordSeq_create_r(geos, n + m - 2, 2);
    bool ok = coords != NULL;
    unsigned out = 0;
    for (unsigned i = 0; ok && i < n; ++i) {
        double x, y;
        ok = GEOSCoordSeq_getXY_r(geos, ring, i, &x, &y) && GEOSCoordSeq_setXY_r(geos, coords, out++, x, y);
        if (i == edge) for (unsigned j = 1; ok && j + 1 < m; ++j)
            ok = GEOSCoordSeq_getXY_r(geos, path, reverse ? m - 1 - j : j, &x, &y) &&
                GEOSCoordSeq_setXY_r(geos, coords, out++, x, y);
    }
    GEOSGeometry *shell = ok ? GEOSGeom_createLinearRing_r(geos, coords) : NULL;
    if (!ok) GEOSCoordSeq_destroy_r(geos, coords);
    GEOSGeometry *restored = shell ? GEOSGeom_createPolygon_r(geos, shell, NULL, 0) : NULL;
    if (restored && GEOSisValid_r(geos, restored) != 1) {
        GEOSGeom_destroy_r(geos, restored); restored = NULL;
    }
    return restored;
}

static bool store(sqlite3 *db, GEOSContextHandle_t geos, const water_piece_t *piece, int64_t id, int64_t original)
{
    double west, east, south, north;
    char ccw = 0;
    size_t bytes = 0;
    unsigned char *wkb = GEOSGeomToWKB_buf_r(geos, piece->geometry, &bytes);
    bool ok = wkb && bytes <= 256 * 1048576 &&
        GEOSGeom_getXMin_r(geos, piece->geometry, &west) && GEOSGeom_getXMax_r(geos, piece->geometry, &east) &&
        GEOSGeom_getYMin_r(geos, piece->geometry, &south) && GEOSGeom_getYMax_r(geos, piece->geometry, &north) &&
        GEOSCoordSeq_isCCW_r(geos, GEOSGeom_getCoordSeq_r(geos,
            GEOSGetExteriorRing_r(geos, piece->geometry)), &ccw);
    sqlite3_stmt *row = NULL;
    ok = ok && sqlite3_prepare_v2(db, "INSERT OR REPLACE INTO simplified_rings VALUES(?1,8,?2,?3)",
        -1, &row, NULL) == SQLITE_OK && sqlite3_bind_int64(row, 1, id) == SQLITE_OK &&
        sqlite3_bind_int(row, 2, GEOSGetNumCoordinates_r(geos, piece->geometry)) == SQLITE_OK &&
        sqlite3_bind_blob(row, 3, wkb, (int)bytes, SQLITE_TRANSIENT) == SQLITE_OK && sqlite3_step(row) == SQLITE_DONE;
    sqlite3_finalize(row);
    char *update = ok ? sqlite3_mprintf(
        "INSERT OR IGNORE INTO rings VALUES(%lld);"
        "INSERT OR REPLACE INTO ring_metadata VALUES(%lld,%u,'reviewed marine-water composition',%lld,%d,%!.17g,0);"
        "INSERT OR REPLACE INTO ring_bounds VALUES(%lld,%!.17g,%!.17g,%!.17g,%!.17g);"
        "INSERT INTO marine_replacements VALUES(%lld,%lld);",
        (long long)id, (long long)id, piece->level, (long long)original, ccw, piece->area,
        (long long)id, west, east, south, north, (long long)original, (long long)id) : NULL;
    ok = ok && update && sql(db, update);
    sqlite3_free(update); GEOSFree_r(geos, wkb);
    return ok;
}

static bool compose(sqlite3 *db, GEOSContextHandle_t geos, const GEOSGeometry *water,
                    const string_t *json, const GEOSGeometry *join, const string_t *join_json, bool check)
{
    double west, east, south, north;
    bool ok = GEOSisValid_r(geos, water) == 1 && GEOSGeom_getXMin_r(geos, water, &west) &&
        GEOSGeom_getXMax_r(geos, water, &east) && GEOSGeom_getYMin_r(geos, water, &south) &&
        GEOSGeom_getYMax_r(geos, water, &north) && west >= -180 && east <= 180 && east - west < 180 &&
        south > -85 && north < 85;
    sqlite3_stmt *rows = NULL, *meta = NULL;
    ok = ok && sqlite3_prepare_v2(db, "SELECT max(id) FROM rings", -1, &meta, NULL) == SQLITE_OK &&
        sqlite3_step(meta) == SQLITE_ROW;
    int64_t next = ok ? sqlite3_column_int64(meta, 0) : 0;
    sqlite3_finalize(meta);
    if (ok && !check) ok = sql(db,
        "BEGIN;CREATE TABLE marine_source(geojson TEXT NOT NULL,status TEXT NOT NULL);"
        "CREATE TABLE marine_original(id INTEGER PRIMARY KEY,geometry BLOB,level INTEGER,source TEXT,source_id INTEGER);"
        "CREATE TABLE marine_replacements(original_id INTEGER,new_id INTEGER PRIMARY KEY);");
    /* Materialise the small spatial selection before replacing its R-tree entries. */
    ok = ok && sql(db, "CREATE TEMP TABLE selected(id INTEGER PRIMARY KEY,level INTEGER,polar INTEGER,geometry BLOB)") &&
        sqlite3_prepare_v2(db, "INSERT INTO selected SELECT m.id,m.level,m.polar,s.geometry FROM ring_bounds b "
            "CROSS JOIN ring_metadata m ON m.id=b.id JOIN simplified_rings s ON s.ring_id=m.id "
            "WHERE b.east>=?1 AND b.west<=?2 AND b.north>=?3 AND b.south<=?4 AND s.tolerance=8",
            -1, &rows, NULL) == SQLITE_OK &&
        sqlite3_bind_double(rows, 1, west) == SQLITE_OK && sqlite3_bind_double(rows, 2, east) == SQLITE_OK &&
        sqlite3_bind_double(rows, 3, south) == SQLITE_OK && sqlite3_bind_double(rows, 4, north) == SQLITE_OK &&
        sqlite3_step(rows) == SQLITE_DONE;
    sqlite3_finalize(rows); rows = NULL;
    ok = ok && sqlite3_prepare_v2(db, "SELECT id,level,polar,geometry FROM selected ORDER BY id",
        -1, &rows, NULL) == SQLITE_OK;
    int step = SQLITE_DONE;
    unsigned changed = 0;
    while (ok && (step = sqlite3_step(rows)) == SQLITE_ROW) {
        int64_t id = sqlite3_column_int64(rows, 0);
        GEOSGeometry *land = GEOSGeomFromWKB_buf_r(geos, sqlite3_column_blob(rows, 3), sqlite3_column_bytes(rows, 3));
        char intersects = land ? GEOSIntersects_r(geos, land, water) : 2;
        if (!intersects) { GEOSGeom_destroy_r(geos, land); continue; }
        ok = intersects == 1 && sqlite3_column_int(rows, 1) == 1 && !sqlite3_column_int(rows, 2);
        GEOSGeometry *restored = ok ? restore_join(geos, land, join) : NULL;
        GEOSGeometry *result = restored ? subtract(geos, restored, water) : NULL;
        GEOSGeom_destroy_r(geos, restored);
        unsigned count = 0;
        water_piece_t *pieces = result ? split(geos, result, &count) : NULL;
        ok = ok && pieces;
        fprintf(stderr, "Marine subtraction ring %" PRId64 ": %s; %u resulting rings\n", id, ok ? "PASS" : "FAIL", count);
        if (ok && !check) {
            char *audit = sqlite3_mprintf("INSERT INTO marine_original SELECT m.id,s.geometry,m.level,m.source,m.source_id "
                "FROM ring_metadata m JOIN simplified_rings s ON s.ring_id=m.id WHERE m.id=%lld AND s.tolerance=8;",
                (long long)id);
            ok = audit && sql(db, audit);
            sqlite3_free(audit);
            for (unsigned i = 0; ok && i < count; ++i) ok = store(db, geos, &pieces[i], i ? ++next : id, id);
        }
        for (unsigned i = 0; ok && i < count; ++i)
            fprintf(stderr, "  component %u: level %u, %d points\n", i, pieces[i].level,
                    GEOSGetNumCoordinates_r(geos, pieces[i].geometry));
        free_pieces(geos, pieces, count); GEOSGeom_destroy_r(geos, result); GEOSGeom_destroy_r(geos, land);
        if (ok) ++changed;
    }
    sqlite3_finalize(rows); rows = NULL;
    ok = ok && step == SQLITE_DONE && changed > 0 && (!join || changed == 1);
    if (ok && !check && join) {
        ok = sql(db, "CREATE TABLE marine_join(geojson TEXT NOT NULL)") &&
            sqlite3_prepare_v2(db, "INSERT INTO marine_join VALUES(?1)", -1, &rows, NULL) == SQLITE_OK &&
            sqlite3_bind_text(rows, 1, string_c_str(join_json), -1, SQLITE_TRANSIENT) == SQLITE_OK &&
            sqlite3_step(rows) == SQLITE_DONE;
        sqlite3_finalize(rows); rows = NULL;
    }
    if (ok && !check) {
        ok = sqlite3_prepare_v2(db, "INSERT INTO marine_source VALUES(?1,'requires_fresh_boundary_and_geographic_review')",
            -1, &rows, NULL) == SQLITE_OK && sqlite3_bind_text(rows, 1, string_c_str(json), -1, SQLITE_TRANSIENT) == SQLITE_OK &&
            sqlite3_step(rows) == SQLITE_DONE && sql(db,
            "UPDATE reconciliation SET status='marine_composed_requires_fresh_review';COMMIT;");
        sqlite3_finalize(rows);
    }
    if (!ok && !check) sql(db, "ROLLBACK");
    fprintf(stderr, "Marine-water composition: %s; %u original rings. Not installed.\n", ok ? "PASS" : "FAIL", changed);
    return ok;
}

static bool storage_test(GEOSContextHandle_t geos, const GEOSGeometry *land, const GEOSGeometry *water)
{
    sqlite3 *db = NULL;
    bool ok = sqlite3_open(":memory:", &db) == SQLITE_OK && sql(db,
        "CREATE TABLE rings(id INTEGER PRIMARY KEY);"
        "CREATE TABLE simplified_rings(ring_id INTEGER,tolerance REAL,points INTEGER,geometry BLOB,"
        "PRIMARY KEY(ring_id,tolerance));"
        "CREATE TABLE ring_metadata(id INTEGER PRIMARY KEY,level INTEGER,source TEXT,source_id INTEGER,"
        "ccw INTEGER,area REAL,polar INTEGER);"
        "CREATE VIRTUAL TABLE ring_bounds USING rtree(id,west,east,south,north);"
        "CREATE TABLE reconciliation(status TEXT);INSERT INTO reconciliation VALUES('fixture');"
        "CREATE TABLE marine_replacements(original_id INTEGER,new_id INTEGER PRIMARY KEY);");
    water_piece_t piece = {.geometry = (GEOSGeometry *)land, .level = 1, .land = true};
    ok = ok && GEOSArea_r(geos, land, &piece.area) && store(db, geos, &piece, 1, 1) &&
        sql(db, "DROP TABLE marine_replacements;CREATE TABLE untouched(value INTEGER);INSERT INTO untouched VALUES(42);");
    string_t *json = string_new_with("synthetic marine composition regression");
    ok = ok && compose(db, geos, water, json, NULL, NULL, false);
    sqlite3_stmt *row = NULL;
    ok = ok && sqlite3_prepare_v2(db,
        "SELECT (SELECT count(*) FROM rings),(SELECT count(*) FROM marine_original),"
        "(SELECT count(*) FROM marine_replacements),(SELECT value FROM untouched),"
        "(SELECT count(*) FROM ring_metadata WHERE level=1)", -1, &row, NULL) == SQLITE_OK &&
        sqlite3_step(row) == SQLITE_ROW && sqlite3_column_int(row, 0) == 2 &&
        sqlite3_column_int(row, 1) == 1 && sqlite3_column_int(row, 2) == 2 &&
        sqlite3_column_int(row, 3) == 42 && sqlite3_column_int(row, 4) == 2;
    sqlite3_finalize(row);
    /* An overlapping nested-water layer is not silently overwritten or reclassified. */
    ok = ok && sql(db, "DROP TABLE marine_source;DROP TABLE marine_original;DROP TABLE marine_replacements;"
        "DROP TABLE selected;UPDATE ring_metadata SET level=2 WHERE id=1;") &&
        !compose(db, geos, water, json, NULL, NULL, false);
    row = NULL;
    ok = ok && sqlite3_prepare_v2(db,
        "SELECT (SELECT count(*) FROM sqlite_master WHERE name='marine_original'),"
        "(SELECT level FROM ring_metadata WHERE id=1),(SELECT value FROM untouched)",
        -1, &row, NULL) == SQLITE_OK && sqlite3_step(row) == SQLITE_ROW &&
        sqlite3_column_int(row, 0) == 0 && sqlite3_column_int(row, 1) == 2 && sqlite3_column_int(row, 2) == 42;
    sqlite3_finalize(row); sqlite3_close(db); string_free(json);
    return ok;
}

static bool join_test(GEOSContextHandle_t geos, GEOSWKTReader *reader)
{
    const char *paths[] = {
        "LINESTRING(0 0,0.005 0.00001,0.01 0)",
        "LINESTRING(0.01 0,0.005 0.00001,0 0)",
        "LINESTRING(0 0,0.005 0.001,0.01 0)",
        "LINESTRING(0 0,0.005 0.00001,0.009 0)"
    };
    GEOSGeometry *land = GEOSWKTReader_read_r(geos, reader,
        "POLYGON((0 0,0.01 0,0.01 0.01,0 0.01,0 0))");
    GEOSGeometry *expected = GEOSWKTReader_read_r(geos, reader,
        "POLYGON((0 0,0.005 0.00001,0.01 0,0.01 0.01,0 0.01,0 0))");
    bool ok = land && expected;
    /* Bounded fixture enumeration, including direction reversal and rejected source paths. */
    for (unsigned i = 0; ok && i < sizeof paths / sizeof *paths; ++i) {
        GEOSGeometry *path = GEOSWKTReader_read_r(geos, reader, paths[i]);
        GEOSGeometry *result = path ? restore_join(geos, land, path) : NULL;
        ok = path && (i < 2 ? result && GEOSEqualsExact_r(geos, result, expected, 0) == 1 : !result);
        GEOSGeom_destroy_r(geos, result); GEOSGeom_destroy_r(geos, path);
    }
    GEOSGeom_destroy_r(geos, expected); GEOSGeom_destroy_r(geos, land);
    /* A valid polygon overlay can still create touching components, which must fail locally. */
    land = GEOSWKTReader_read_r(geos, reader, "POLYGON((0 0,10 0,10 10,0 10,0 0))");
    GEOSGeometry *water = GEOSWKTReader_read_r(geos, reader,
        "POLYGON((0 5,5 0,10 5,5 10,0 5))");
    GEOSGeometry *result = land && water ? subtract(geos, land, water) : NULL;
    ok = ok && land && water && !result;
    GEOSGeom_destroy_r(geos, result); GEOSGeom_destroy_r(geos, water); GEOSGeom_destroy_r(geos, land);
    fprintf(stderr, "Source-join restoration and contact rejection self-test: %s\n", ok ? "PASS" : "FAIL");
    return ok;
}

static bool self_test(void)
{
    GEOSContextHandle_t geos = GEOS_init_r();
    GEOSWKTReader *reader = GEOSWKTReader_create_r(geos);
    GEOSGeometry *land = GEOSWKTReader_read_r(geos, reader, "POLYGON((0 0,10 0,10 10,0 10,0 0))");
    GEOSGeometry *water = GEOSWKTReader_read_r(geos, reader,
        "POLYGON((2 2,8 2,8 8,2 8,2 2),(4 4,4 6,6 6,6 4,4 4))");
    GEOSGeometry *result = land && water ? subtract(geos, land, water) : NULL;
    unsigned count = 0, levels = 0;
    water_piece_t *pieces = result ? split(geos, result, &count) : NULL;
    bool ok = pieces && count == 3 && join_test(geos, reader);
    for (unsigned i = 0; i < count; ++i) levels |= 1u << pieces[i].level;
    ok = ok && levels == 14;
    free_pieces(geos, pieces, count); GEOSGeom_destroy_r(geos, result); GEOSGeom_destroy_r(geos, water);
    water = GEOSWKTReader_read_r(geos, reader,
        "POLYGON((2 -1,8 -1,8 8,2 8,2 -1),(4 4,4 6,6 6,6 4,4 4))");
    result = land && water ? subtract(geos, land, water) : NULL;
    pieces = result ? split(geos, result, &count) : NULL;
    ok = ok && pieces && count == 2;
    for (unsigned i = 0; i < count; ++i) ok = ok && pieces[i].level == 1;
    if (ok) ok = storage_test(geos, land, water);
    free_pieces(geos, pieces, count); GEOSGeom_destroy_r(geos, result); GEOSGeom_destroy_r(geos, water);
    GEOSGeom_destroy_r(geos, land); GEOSWKTReader_destroy_r(geos, reader); GEOS_finish_r(geos);
    fprintf(stderr, "Marine-water composition self-test: %s\n", ok ? "PASS" : "FAIL");
    return ok;
}

/* Prepare an exclusive candidate, or inspect the bounded spatial operation without modifying its source. */
int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "--self-test")) return self_test() ? 0 : 1;
    bool check = (argc == 4 || argc == 5) && !strcmp(argv[1], "--check");
    if (argc != 4 && argc != 5) {
        fprintf(stderr, "Usage: %s candidate.sqlite3 marine.geojson [source-join.geojson] NEW-candidate.sqlite3\n"
            "       %s --check candidate.sqlite3 marine.geojson [source-join.geojson]\n", argv[0], argv[0]); return 2;
    }
    file_t *water_file = file_new_cstr(argv[check ? 3 : 2]);
    file_info_t *info = water_file ? file_get_info(water_file) : NULL;
    bool ok = info && file_info_size(info) > 0 && file_info_size(info) <= 8 * 1048576;
    file_info_free(info);
    string_t *json = ok ? file_read_all_text(water_file) : NULL;
    GEOSContextHandle_t geos = GEOS_init_r();
    GEOSGeoJSONReader *reader = geos ? GEOSGeoJSONReader_create_r(geos) : NULL;
    GEOSGeometry *collection = reader && json ? GEOSGeoJSONReader_readGeometry_r(geos, reader, string_c_str(json)) : NULL;
    GEOSGeometry *water = collection && GEOSisValid_r(geos, collection) == 1 ? GEOSUnaryUnion_r(geos, collection) : NULL;
    file_t *join_file = argc == 5 ? file_new_cstr(argv[check ? 4 : 3]) : NULL;
    file_info_t *join_info = join_file ? file_get_info(join_file) : NULL;
    bool join_size = join_info && file_info_size(join_info) > 0 && file_info_size(join_info) <= 1048576;
    string_t *join_json = join_size ? file_read_all_text(join_file) : NULL;
    GEOSGeometry *join = reader && join_json ?
        GEOSGeoJSONReader_readGeometry_r(geos, reader, string_c_str(join_json)) : NULL;
    file_info_free(join_info);
    sqlite3 *source = NULL, *target = NULL;
    file_t *output = check ? NULL : file_new_cstr(argv[argc - 1]);
    ok = water && (argc != 5 || join) &&
        sqlite3_open_v2(argv[check ? 2 : 1], &source, SQLITE_OPEN_READONLY, NULL) == SQLITE_OK;
    if (ok && !check) {
        ok = output && file_open(output, FILE_MODE_CREATE_NEW, FILE_ACCESS_WRITE) && file_close(output) &&
            sqlite3_open_v2(argv[argc - 1], &target, SQLITE_OPEN_READWRITE, NULL) == SQLITE_OK;
        sqlite3_backup *copy = ok ? sqlite3_backup_init(target, "main", source, "main") : NULL;
        ok = copy && sqlite3_backup_step(copy, -1) == SQLITE_DONE;
        if (copy) ok = sqlite3_backup_finish(copy) == SQLITE_OK && ok;
    }
    sqlite3 *db = check ? source : target;
    ok = ok && sql(db, "PRAGMA cache_size=-8192;PRAGMA temp_store=FILE;") &&
        compose(db, geos, water, json, join, join_json, check);
    if (target) sqlite3_close(target);
    if (source) sqlite3_close(source);
    if (geos) {
        GEOSGeom_destroy_r(geos, water); GEOSGeom_destroy_r(geos, collection); GEOSGeom_destroy_r(geos, join);
        if (reader) GEOSGeoJSONReader_destroy_r(geos, reader);
        GEOS_finish_r(geos);
    }
    file_free(output); file_free(water_file); string_free(json); file_free(join_file); string_free(join_json);
    return ok ? 0 : 1;
}
