/**
 * @file coastline_review.c
 * @brief Offline spatial validation of a complete simplified coastline candidate.
 *
 * Opens the source staging database read-only and creates a separate review database.
 * Candidate boundaries are divided into bounded overlapping chunks and indexed with
 * SQLite R-tree bounds. GEOS tests only spatial neighbours belonging to different
 * rings, including periodic copies at the date line. This avoids loading continental
 * boundary indexes or scanning every ring for each island. No installed data is changed.
 * Passing this review establishes individual validity and absence of cross-ring
 * boundary contacts only; nesting, water preservation and export checks remain separate.
 */
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <geos_c.h>
#include <sqlite3.h>

#include "file-t.h"

static bool execute(sqlite3 *db, const char *sql)
{
    char *error = NULL;
    int result = sqlite3_exec(db, sql, NULL, NULL, &error);
    if (result != SQLITE_OK) fprintf(stderr, "%s\n", error ? error : sqlite3_errmsg(db));
    sqlite3_free(error);
    return result == SQLITE_OK;
}

static bool schema(sqlite3 *db)
{
    return execute(db,
        "PRAGMA journal_mode=DELETE;PRAGMA cache_size=-16384;PRAGMA temp_store=FILE;"
        "CREATE TABLE review(status TEXT NOT NULL);INSERT INTO review VALUES('incomplete');"
        "CREATE TABLE chunks(id INTEGER PRIMARY KEY,ring_id INTEGER NOT NULL,geometry BLOB NOT NULL);"
        "CREATE VIRTUAL TABLE bounds USING rtree(id,west,east,south,north);"
        "CREATE TABLE contacts(first_chunk INTEGER,second_chunk INTEGER,first_ring INTEGER,second_ring INTEGER);"
        "CREATE TABLE totals(rings INTEGER,points INTEGER,chunks INTEGER,comparisons INTEGER,contacts INTEGER);");
}

static bool insert_chunk(sqlite3 *db, GEOSContextHandle_t geos, const GEOSCoordSequence *coordinates,
                         unsigned first, unsigned count, int64_t ring, int64_t *chunks)
{
    bool ok = true;
    for (int shift = -360; ok && shift <= 360; shift += 360) {
        GEOSCoordSequence *part = GEOSCoordSeq_create_r(geos, count, 2);
        double west = INFINITY, east = -INFINITY, south = INFINITY, north = -INFINITY;
        ok = part != NULL;
        for (unsigned i = 0; ok && i < count; ++i) {
            double x, y;
            ok = GEOSCoordSeq_getXY_r(geos, coordinates, first + i, &x, &y);
            if (!ok) break;
            x += shift;
            west = fmin(west, x); east = fmax(east, x);
            south = fmin(south, y); north = fmax(north, y);
            ok = GEOSCoordSeq_setXY_r(geos, part, i, x, y);
        }
        if (!ok || east < -180 || west > 180) {
            GEOSCoordSeq_destroy_r(geos, part);
            continue;
        }
        GEOSGeometry *line = GEOSGeom_createLineString_r(geos, part);
        size_t size = 0;
        unsigned char *wkb = line ? GEOSGeomToWKB_buf_r(geos, line, &size) : NULL;
        sqlite3_stmt *insert = NULL, *bounds = NULL;
        ok = wkb && size <= 16384 &&
             sqlite3_prepare_v2(db, "INSERT INTO chunks VALUES(?1,?2,?3)", -1, &insert, NULL) == SQLITE_OK &&
             sqlite3_prepare_v2(db, "INSERT INTO bounds VALUES(?1,?2,?3,?4,?5)", -1, &bounds, NULL) == SQLITE_OK;
        int64_t id = *chunks + 1;
        ok = ok && sqlite3_bind_int64(insert, 1, id) == SQLITE_OK &&
             sqlite3_bind_int64(insert, 2, ring) == SQLITE_OK &&
             sqlite3_bind_blob(insert, 3, wkb, (int)size, SQLITE_TRANSIENT) == SQLITE_OK &&
             sqlite3_step(insert) == SQLITE_DONE &&
             sqlite3_bind_int64(bounds, 1, id) == SQLITE_OK &&
             sqlite3_bind_double(bounds, 2, west) == SQLITE_OK &&
             sqlite3_bind_double(bounds, 3, east) == SQLITE_OK &&
             sqlite3_bind_double(bounds, 4, south) == SQLITE_OK &&
             sqlite3_bind_double(bounds, 5, north) == SQLITE_OK && sqlite3_step(bounds) == SQLITE_DONE;
        sqlite3_finalize(insert); sqlite3_finalize(bounds);
        GEOSFree_r(geos, wkb); GEOSGeom_destroy_r(geos, line);
        if (ok) ++*chunks;
    }
    return ok;
}

static bool index_ring(sqlite3 *db, GEOSContextHandle_t geos, const GEOSGeometry *geometry,
                       int64_t ring, int64_t *points, int64_t *chunks)
{
    if (!geometry || GEOSGeomTypeId_r(geos, geometry) != GEOS_POLYGON ||
        GEOSGetNumInteriorRings_r(geos, geometry) != 0 || GEOSisValid_r(geos, geometry) != 1) return false;
    const GEOSGeometry *shell = GEOSGetExteriorRing_r(geos, geometry);
    const GEOSCoordSequence *coordinates = shell ? GEOSGeom_getCoordSeq_r(geos, shell) : NULL;
    unsigned count = 0;
    if (!coordinates || !GEOSCoordSeq_getSize_r(geos, coordinates, &count) || count < 4 || count > 8000000)
        return false;
    for (unsigned first = 0; first + 1 < count; first += 255) {
        unsigned length = count - first < 256 ? count - first : 256;
        if (!insert_chunk(db, geos, coordinates, first, length, ring, chunks)) return false;
    }
    *points += count;
    return true;
}

static bool intersections(sqlite3 *db, GEOSContextHandle_t geos, int64_t *comparisons, int64_t *contacts)
{
    sqlite3_stmt *outer = NULL, *nearby = NULL, *record = NULL;
    bool ok = sqlite3_prepare_v2(db,
        "SELECT c.id,c.ring_id,c.geometry,b.west,b.east,b.south,b.north "
        "FROM chunks c JOIN bounds b ON b.id=c.id ORDER BY c.id", -1, &outer, NULL) == SQLITE_OK &&
        sqlite3_prepare_v2(db,
        "SELECT c.id,c.ring_id,c.geometry FROM bounds b JOIN chunks c ON c.id=b.id "
        "WHERE b.west<=?1 AND b.east>=?2 AND b.south<=?3 AND b.north>=?4 AND c.id<?5 AND c.ring_id!=?6",
        -1, &nearby, NULL) == SQLITE_OK && sqlite3_prepare_v2(db,
        "INSERT INTO contacts VALUES(?1,?2,?3,?4)", -1, &record, NULL) == SQLITE_OK;
    int step = SQLITE_DONE;
    while (ok && (step = sqlite3_step(outer)) == SQLITE_ROW) {
        int64_t id = sqlite3_column_int64(outer, 0), ring = sqlite3_column_int64(outer, 1);
        GEOSGeometry *a = GEOSGeomFromWKB_buf_r(geos, sqlite3_column_blob(outer, 2),
                                               (size_t)sqlite3_column_bytes(outer, 2));
        ok = a && sqlite3_bind_double(nearby, 1, sqlite3_column_double(outer, 4)) == SQLITE_OK &&
             sqlite3_bind_double(nearby, 2, sqlite3_column_double(outer, 3)) == SQLITE_OK &&
             sqlite3_bind_double(nearby, 3, sqlite3_column_double(outer, 6)) == SQLITE_OK &&
             sqlite3_bind_double(nearby, 4, sqlite3_column_double(outer, 5)) == SQLITE_OK &&
             sqlite3_bind_int64(nearby, 5, id) == SQLITE_OK && sqlite3_bind_int64(nearby, 6, ring) == SQLITE_OK;
        int found = SQLITE_DONE;
        while (ok && (found = sqlite3_step(nearby)) == SQLITE_ROW) {
            GEOSGeometry *b = GEOSGeomFromWKB_buf_r(geos, sqlite3_column_blob(nearby, 2),
                                                   (size_t)sqlite3_column_bytes(nearby, 2));
            char contact = a && b ? GEOSIntersects_r(geos, a, b) : 2;
            ++*comparisons;
            ok = contact != 2;
            if (contact == 1) {
                ++*contacts;
                ok = sqlite3_bind_int64(record, 1, id) == SQLITE_OK &&
                     sqlite3_bind_int64(record, 2, sqlite3_column_int64(nearby, 0)) == SQLITE_OK &&
                     sqlite3_bind_int64(record, 3, ring) == SQLITE_OK &&
                     sqlite3_bind_int64(record, 4, sqlite3_column_int64(nearby, 1)) == SQLITE_OK &&
                     sqlite3_step(record) == SQLITE_DONE;
                sqlite3_reset(record);
            }
            GEOSGeom_destroy_r(geos, b);
        }
        ok = ok && found == SQLITE_DONE;
        sqlite3_reset(nearby);
        GEOSGeom_destroy_r(geos, a);
        if (id % 10000 == 0) {
            if (!sqlite3_get_autocommit(db)) ok = ok && execute(db, "COMMIT;BEGIN");
            fprintf(stderr, "Review: %" PRId64 " chunks, %" PRId64 " comparisons, %" PRId64 " contacts\n",
                    id, *comparisons, *contacts);
            fflush(stderr);
        }
    }
    sqlite3_finalize(outer); sqlite3_finalize(nearby); sqlite3_finalize(record);
    return ok && step == SQLITE_DONE;
}

static bool review(sqlite3 *source, sqlite3 *output, double tolerance)
{
    sqlite3_stmt *complete = NULL, *rows = NULL;
    bool ok = sqlite3_prepare_v2(source,
        "SELECT (SELECT count(*) FROM rings),count(*),sum(points) FROM simplified_rings WHERE tolerance=?1",
        -1, &complete, NULL) == SQLITE_OK && sqlite3_bind_double(complete, 1, tolerance) == SQLITE_OK &&
        sqlite3_step(complete) == SQLITE_ROW && sqlite3_column_int64(complete, 0) > 0 &&
        sqlite3_column_int64(complete, 0) == sqlite3_column_int64(complete, 1);
    if (ok) fprintf(stderr, "Complete candidate: %" PRId64 " rings, %" PRId64 " points\n",
                    (int64_t)sqlite3_column_int64(complete, 1), (int64_t)sqlite3_column_int64(complete, 2));
    else fprintf(stderr, "Candidate unavailable or incomplete; refusing review\n");
    sqlite3_finalize(complete);
    GEOSContextHandle_t geos = GEOS_init_r();
    int64_t rings = 0, points = 0, chunks = 0, comparisons = 0, contacts = 0;
    ok = ok && geos && schema(output) && execute(output, "BEGIN") && sqlite3_prepare_v2(source,
        "SELECT ring_id,geometry,points FROM simplified_rings WHERE tolerance=?1 ORDER BY ring_id",
        -1, &rows, NULL) == SQLITE_OK &&
        sqlite3_bind_double(rows, 1, tolerance) == SQLITE_OK;
    int step = SQLITE_DONE;
    while (ok && (step = sqlite3_step(rows)) == SQLITE_ROW) {
        GEOSGeometry *geometry = GEOSGeomFromWKB_buf_r(geos, sqlite3_column_blob(rows, 1),
                                                     (size_t)sqlite3_column_bytes(rows, 1));
        int64_t previous_points = points;
        ok = index_ring(output, geos, geometry, sqlite3_column_int64(rows, 0), &points, &chunks) &&
             points - previous_points == sqlite3_column_int64(rows, 2);
        GEOSGeom_destroy_r(geos, geometry);
        if (++rings % 10000 == 0) {
            ok = ok && execute(output, "COMMIT;BEGIN");
            fprintf(stderr, "Indexed: %" PRId64 " rings, %" PRId64 " points, %" PRId64 " chunks\n", rings, points, chunks);
            fflush(stderr);
        }
    }
    sqlite3_finalize(rows);
    ok = ok && step == SQLITE_DONE && execute(output, "COMMIT;BEGIN") &&
         intersections(output, geos, &comparisons, &contacts);
    sqlite3_stmt *totals = NULL;
    ok = ok && sqlite3_prepare_v2(output, "INSERT INTO totals VALUES(?1,?2,?3,?4,?5)", -1, &totals, NULL) == SQLITE_OK &&
         sqlite3_bind_int64(totals, 1, rings) == SQLITE_OK && sqlite3_bind_int64(totals, 2, points) == SQLITE_OK &&
         sqlite3_bind_int64(totals, 3, chunks) == SQLITE_OK && sqlite3_bind_int64(totals, 4, comparisons) == SQLITE_OK &&
         sqlite3_bind_int64(totals, 5, contacts) == SQLITE_OK && sqlite3_step(totals) == SQLITE_DONE &&
         execute(output, contacts ? "UPDATE review SET status='boundary_contacts_found';COMMIT" :
                                    "UPDATE review SET status='boundary_contacts_clear_not_install_approved';COMMIT");
    sqlite3_finalize(totals);
    if (geos) GEOS_finish_r(geos);
    fprintf(stderr, "Review finished: %" PRId64 " rings, %" PRId64 " contacts; %s\n",
            rings, contacts, ok ? "recorded, not approved for installation" : "incomplete");
    return ok && contacts == 0;
}

static bool classify_contacts(sqlite3 *db, GEOSContextHandle_t geos)
{
    sqlite3_stmt *rows = NULL;
    bool ok = sqlite3_prepare_v2(db,
        "SELECT a.geometry,b.geometry FROM contacts c "
        "JOIN chunks a ON a.id=c.first_chunk JOIN chunks b ON b.id=c.second_chunk",
        -1, &rows, NULL) == SQLITE_OK;
    unsigned touches = 0, crossings = 0, overlaps = 0, other = 0;
    int step = SQLITE_DONE;
    while (ok && (step = sqlite3_step(rows)) == SQLITE_ROW) {
        GEOSGeometry *a = GEOSGeomFromWKB_buf_r(geos, sqlite3_column_blob(rows, 0),
                                               (size_t)sqlite3_column_bytes(rows, 0));
        GEOSGeometry *b = GEOSGeomFromWKB_buf_r(geos, sqlite3_column_blob(rows, 1),
                                               (size_t)sqlite3_column_bytes(rows, 1));
        char touch = a && b ? GEOSTouches_r(geos, a, b) : 2;
        char cross = a && b ? GEOSCrosses_r(geos, a, b) : 2;
        char overlap = a && b ? GEOSOverlaps_r(geos, a, b) : 2;
        ok = touch != 2 && cross != 2 && overlap != 2;
        if (ok) {
            if (touch) ++touches;
            else if (cross) ++crossings;
            else if (overlap) ++overlaps;
            else ++other;
        }
        GEOSGeom_destroy_r(geos, a);
        GEOSGeom_destroy_r(geos, b);
    }
    sqlite3_finalize(rows);
    if (ok && step == SQLITE_DONE)
        fprintf(stderr, "Contact classification: %u touches, %u crossings, %u overlaps, %u other; "
                        "chunk-level diagnostics, not installation approval\n", touches, crossings, overlaps, other);
    return ok && step == SQLITE_DONE;
}

static bool self_test(void)
{
    sqlite3 *db = NULL;
    GEOSContextHandle_t geos = GEOS_init_r();
    bool ok = geos && sqlite3_open(":memory:", &db) == SQLITE_OK && schema(db);
    int64_t points = 0, chunks = 0, comparisons = 0, contacts = 0;
    const char *fixtures[] = {"POLYGON((0 0,2 0,2 2,0 2,0 0))", "POLYGON((1 1,3 1,3 3,1 3,1 1))",
                              "POLYGON((179 10,181 10,181 12,179 12,179 10))",
                              "POLYGON((-180 11,-178 11,-178 13,-180 13,-180 11))"};
    for (unsigned i = 0; ok && i < sizeof fixtures / sizeof *fixtures; ++i) {
        GEOSGeometry *geometry = GEOSGeomFromWKT_r(geos, fixtures[i]);
        ok = index_ring(db, geos, geometry, i + 1, &points, &chunks);
        GEOSGeom_destroy_r(geos, geometry);
    }
    ok = ok && intersections(db, geos, &comparisons, &contacts) && contacts == 3 && classify_contacts(db, geos);
    ok = ok && execute(db, "DELETE FROM chunks;DELETE FROM bounds;DELETE FROM contacts");
    points = chunks = comparisons = contacts = 0;
    const char *separate[] = {"POLYGON((0 0,2 0,2 2,0 2,0 0))", "POLYGON((3 3,4 3,4 4,3 4,3 3))"};
    for (unsigned i = 0; ok && i < sizeof separate / sizeof *separate; ++i) {
        GEOSGeometry *geometry = GEOSGeomFromWKT_r(geos, separate[i]);
        ok = index_ring(db, geos, geometry, i + 1, &points, &chunks);
        GEOSGeom_destroy_r(geos, geometry);
    }
    ok = ok && intersections(db, geos, &comparisons, &contacts) && contacts == 0 && comparisons == 0;
    GEOSGeometry *invalid = geos ? GEOSGeomFromWKT_r(geos, "POLYGON((0 0,2 2,0 2,2 0,0 0))") : NULL;
    ok = ok && invalid && !index_ring(db, geos, invalid, 3, &points, &chunks);
    GEOSGeom_destroy_r(geos, invalid);
    if (db) sqlite3_close(db);
    if (geos) GEOS_finish_r(geos);
    fprintf(stderr, "Coastline boundary review self-test: %s\n", ok ? "PASS" : "FAIL");
    return ok;
}

/* Run a separate read-only-source review; never replace an existing review database. */
int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "--self-test")) return self_test() ? 0 : 1;
    if (argc == 3 && !strcmp(argv[1], "--classify-contacts")) {
        sqlite3 *db = NULL;
        GEOSContextHandle_t geos = GEOS_init_r();
        bool ok = geos && sqlite3_open_v2(argv[2], &db, SQLITE_OPEN_READONLY, NULL) == SQLITE_OK &&
                  classify_contacts(db, geos);
        if (db) sqlite3_close(db);
        if (geos) GEOS_finish_r(geos);
        return ok ? 0 : 1;
    }
    if (argc != 4) {
        fprintf(stderr, "Usage: %s candidate.sqlite3 tolerance NEW-review.sqlite3\n", argv[0]);
        return 2;
    }
    char *end;
    errno = 0;
    double tolerance = strtod(argv[2], &end);
    if (errno || end == argv[2] || *end || !isfinite(tolerance) || tolerance < 0.5 || tolerance > 40) return 2;
    file_t *file = file_new_cstr(argv[3]);
    sqlite3 *source = NULL, *output = NULL;
    bool ok = sqlite3_open_v2(argv[1], &source, SQLITE_OPEN_READONLY, NULL) == SQLITE_OK && file &&
              file_open(file, FILE_MODE_CREATE_NEW, FILE_ACCESS_WRITE) && file_close(file) &&
              sqlite3_open_v2(argv[3], &output, SQLITE_OPEN_READWRITE, NULL) == SQLITE_OK &&
              execute(source, "PRAGMA cache_size=-16384;BEGIN") && review(source, output, tolerance);
    if (source) sqlite3_close(source);
    if (output) sqlite3_close(output);
    file_free(file);
    return ok ? 0 : 1;
}
