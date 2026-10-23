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
        "CREATE TABLE chunks(id INTEGER PRIMARY KEY,ring_id INTEGER NOT NULL,geometry BLOB NOT NULL,"
        "ring_west REAL,ring_east REAL);"
        "CREATE VIRTUAL TABLE bounds USING rtree(id,west,east,south,north);"
        "CREATE TABLE contacts(first_chunk INTEGER,second_chunk INTEGER,first_ring INTEGER,second_ring INTEGER);"
        "CREATE TABLE seam_contacts(first_chunk INTEGER,second_chunk INTEGER,first_ring INTEGER,second_ring INTEGER);"
        "CREATE TABLE invalid_rings(ring_id INTEGER PRIMARY KEY,reason TEXT,longitude REAL,latitude REAL);"
        "CREATE TABLE totals(rings INTEGER,points INTEGER,chunks INTEGER,comparisons INTEGER,contacts INTEGER);");
}

static bool insert_chunk(sqlite3 *db, GEOSContextHandle_t geos, const GEOSCoordSequence *coordinates,
                         unsigned first, unsigned count, int64_t ring, double ring_west, double ring_east,
                         int64_t *chunks)
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
             sqlite3_prepare_v2(db, "INSERT INTO chunks VALUES(?1,?2,?3,?4,?5)", -1, &insert, NULL) == SQLITE_OK &&
             sqlite3_prepare_v2(db, "INSERT INTO bounds VALUES(?1,?2,?3,?4,?5)", -1, &bounds, NULL) == SQLITE_OK;
        int64_t id = *chunks + 1;
        ok = ok && sqlite3_bind_int64(insert, 1, id) == SQLITE_OK &&
             sqlite3_bind_int64(insert, 2, ring) == SQLITE_OK &&
             sqlite3_bind_blob(insert, 3, wkb, (int)size, SQLITE_TRANSIENT) == SQLITE_OK &&
             sqlite3_bind_double(insert, 4, ring_west + shift) == SQLITE_OK &&
             sqlite3_bind_double(insert, 5, ring_east + shift) == SQLITE_OK &&
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
    double west, east;
    if (!coordinates || !GEOSCoordSeq_getSize_r(geos, coordinates, &count) || count < 4 || count > 8000000)
        return false;
    if (!GEOSGeom_getXMin_r(geos, geometry, &west) || !GEOSGeom_getXMax_r(geos, geometry, &east)) return false;
    for (unsigned first = 0; first + 1 < count; first += 255) {
        unsigned length = count - first < 256 ? count - first : 256;
        if (!insert_chunk(db, geos, coordinates, first, length, ring, west, east, chunks)) return false;
    }
    *points += count;
    return true;
}

/* Only complementary clipped polygons may share a seam; proximity to the date line is insufficient. */
static int split_seam(GEOSContextHandle_t geos, const GEOSGeometry *a, const GEOSGeometry *b,
                      double a_west, double a_east, double b_west, double b_east)
{
    double seam;
    if (a_east == b_west) seam = a_east;
    else if (b_east == a_west) seam = b_east;
    else return 0;
    if (fabs(seam) != 180 || a_west >= a_east || b_west >= b_east) return 0;
    GEOSGeometry *shared = GEOSIntersection_r(geos, a, b);
    double west, east;
    int result = shared && GEOSisEmpty_r(geos, shared) == 0 &&
        GEOSGeom_getXMin_r(geos, shared, &west) && GEOSGeom_getXMax_r(geos, shared, &east)
        ? west == seam && east == seam : -1;
    GEOSGeom_destroy_r(geos, shared);
    return result;
}

static bool intersections(sqlite3 *db, GEOSContextHandle_t geos, int64_t *comparisons, int64_t *contacts)
{
    sqlite3_stmt *outer = NULL, *nearby = NULL, *record = NULL, *seam_record = NULL;
    bool ok = sqlite3_prepare_v2(db,
        "SELECT c.id,c.ring_id,c.geometry,b.west,b.east,b.south,b.north,c.ring_west,c.ring_east "
        "FROM chunks c JOIN bounds b ON b.id=c.id ORDER BY c.id", -1, &outer, NULL) == SQLITE_OK &&
        sqlite3_prepare_v2(db,
        "SELECT c.id,c.ring_id,c.geometry,c.ring_west,c.ring_east FROM bounds b JOIN chunks c ON c.id=b.id "
        "WHERE b.west<=?1 AND b.east>=?2 AND b.south<=?3 AND b.north>=?4 AND c.id<?5 AND c.ring_id!=?6",
        -1, &nearby, NULL) == SQLITE_OK && sqlite3_prepare_v2(db,
        "INSERT INTO contacts VALUES(?1,?2,?3,?4)", -1, &record, NULL) == SQLITE_OK && sqlite3_prepare_v2(db,
        "INSERT INTO seam_contacts VALUES(?1,?2,?3,?4)", -1, &seam_record, NULL) == SQLITE_OK;
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
                int seam = split_seam(geos, a, b, sqlite3_column_double(outer, 7),
                    sqlite3_column_double(outer, 8), sqlite3_column_double(nearby, 3), sqlite3_column_double(nearby, 4));
                sqlite3_stmt *target = seam == 1 ? seam_record : record;
                if (seam == 0) ++*contacts;
                ok = seam >= 0 && sqlite3_bind_int64(target, 1, id) == SQLITE_OK &&
                     sqlite3_bind_int64(target, 2, sqlite3_column_int64(nearby, 0)) == SQLITE_OK &&
                     sqlite3_bind_int64(target, 3, ring) == SQLITE_OK &&
                     sqlite3_bind_int64(target, 4, sqlite3_column_int64(nearby, 1)) == SQLITE_OK &&
                     sqlite3_step(target) == SQLITE_DONE;
                sqlite3_reset(target);
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
    sqlite3_finalize(outer); sqlite3_finalize(nearby); sqlite3_finalize(record); sqlite3_finalize(seam_record);
    return ok && step == SQLITE_DONE;
}

static bool validate_overlay(sqlite3 *source, double tolerance)
{
    sqlite3_stmt *check = NULL;
    bool ok = sqlite3_prepare_v2(source,
        "SELECT (SELECT count(*) FROM repair.replacements),"
        "(SELECT count(*) FROM repair.absorbed),(SELECT count(*) FROM repair.originals),"
        "(SELECT count(*) FROM repair.originals o JOIN simplified_rings s "
        "ON s.ring_id=o.ring_id AND s.tolerance=o.tolerance WHERE o.tolerance=?1 AND s.geometry=o.geometry),"
        "(SELECT count(*) FROM repair.status WHERE value='coverage_union_requires_global_review'),"
        "(SELECT count(*) FROM repair.absorbed a JOIN repair.replacements r ON r.ring_id=a.retained_id "
        "JOIN repair.originals o ON o.ring_id=a.ring_id JOIN repair.originals p ON p.ring_id=r.ring_id "
        "WHERE r.tolerance=?1 AND o.tolerance=?1 AND p.tolerance=?1 AND a.ring_id!=r.ring_id)",
        -1, &check, NULL) == SQLITE_OK && sqlite3_bind_double(check, 1, tolerance) == SQLITE_OK &&
        sqlite3_step(check) == SQLITE_ROW;
    if (ok) {
        int64_t count = sqlite3_column_int64(check, 0);
        ok = count > 0 && sqlite3_column_int64(check, 1) == count &&
             sqlite3_column_int64(check, 2) == count * 2 && sqlite3_column_int64(check, 3) == count * 2 &&
             sqlite3_column_int(check, 4) == 1 && sqlite3_column_int64(check, 5) == count;
    }
    sqlite3_finalize(check);
    if (!ok) fprintf(stderr, "Repair overlay incomplete or stale; refusing review\n");
    return ok && execute(source,
        "CREATE TEMP VIEW candidate_rings AS SELECT s.ring_id,s.tolerance,"
        "coalesce(r.geometry,s.geometry) AS geometry,coalesce(r.points,s.points) AS points "
        "FROM simplified_rings s LEFT JOIN repair.replacements r ON r.ring_id=s.ring_id AND r.tolerance=s.tolerance "
        "WHERE NOT EXISTS(SELECT 1 FROM repair.absorbed a JOIN repair.replacements p ON p.ring_id=a.retained_id "
        "WHERE a.ring_id=s.ring_id AND p.tolerance=s.tolerance)");
}

static bool review_overlay(sqlite3 *source, const char *path, double tolerance)
{
    if (!path) return execute(source,
        "CREATE TEMP VIEW candidate_rings AS SELECT ring_id,tolerance,geometry,points FROM simplified_rings");
    sqlite3_stmt *attach = NULL;
    bool ok = sqlite3_prepare_v2(source, "ATTACH DATABASE ?1 AS repair", -1, &attach, NULL) == SQLITE_OK &&
        sqlite3_bind_text(attach, 1, path, -1, SQLITE_TRANSIENT) == SQLITE_OK && sqlite3_step(attach) == SQLITE_DONE;
    sqlite3_finalize(attach);
    return ok && validate_overlay(source, tolerance);
}

static int microdegree_coordinate(double *x, double *y, void *context)
{
    (void)context;
    if (!isfinite(*x) || !isfinite(*y) || *x < -180 || *x > 360 || *y < -90 || *y > 90) return 0;
    *x = (double)llround(*x * 1000000) / 1000000;
    *y = (double)llround(*y * 1000000) / 1000000;
    return 1;
}

static bool save_quantized(sqlite3_stmt *save, GEOSContextHandle_t geos, const GEOSGeometry *geometry,
                            int64_t ring, int64_t points)
{
    size_t bytes = 0;
    unsigned char *wkb = GEOSGeomToWKB_buf_r(geos, geometry, &bytes);
    bool ok = wkb && bytes <= 256 * 1048576 &&
        sqlite3_bind_int64(save, 1, ring) == SQLITE_OK &&
        sqlite3_bind_int64(save, 2, points) == SQLITE_OK &&
        sqlite3_bind_blob(save, 3, wkb, (int)bytes, SQLITE_TRANSIENT) == SQLITE_OK && sqlite3_step(save) == SQLITE_DONE;
    sqlite3_reset(save);
    GEOSFree_r(geos, wkb);
    return ok;
}

/* Preserve precise diagnostics for every invalid rounded ring without treating it as accepted geometry. */
static bool record_invalid(sqlite3 *db, GEOSContextHandle_t geos, const GEOSGeometry *geometry, int64_t ring)
{
    if (!geometry) return false;
    char *reason = NULL;
    GEOSGeometry *location = NULL;
    char valid = GEOSisValidDetail_r(geos, geometry, 0, &reason, &location);
    sqlite3_stmt *insert = NULL;
    double x, y;
    bool located = location && GEOSGeomGetX_r(geos, location, &x) && GEOSGeomGetY_r(geos, location, &y);
    bool ok = valid == 0 && reason && sqlite3_prepare_v2(db,
        "INSERT INTO invalid_rings VALUES(?1,?2,?3,?4)", -1, &insert, NULL) == SQLITE_OK &&
        sqlite3_bind_int64(insert, 1, ring) == SQLITE_OK &&
        sqlite3_bind_text(insert, 2, reason, -1, SQLITE_TRANSIENT) == SQLITE_OK;
    if (ok && located) ok = sqlite3_bind_double(insert, 3, x) == SQLITE_OK &&
                           sqlite3_bind_double(insert, 4, y) == SQLITE_OK;
    ok = ok && sqlite3_step(insert) == SQLITE_DONE;
    if (ok) {
        fprintf(stderr, "Invalid ring %" PRId64 ": %s", ring, reason);
        if (located) fprintf(stderr, " at %.9f, %.9f", x, y);
        fputc('\n', stderr);
    }
    sqlite3_finalize(insert);
    GEOSFree_r(geos, reason); GEOSGeom_destroy_r(geos, location);
    return ok;
}

static bool review(sqlite3 *source, sqlite3 *output, double tolerance, const char *overlay, bool microdegrees)
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
    ok = ok && review_overlay(source, overlay, tolerance);
    GEOSContextHandle_t geos = GEOS_init_r();
    int64_t rings = 0, points = 0, chunks = 0, comparisons = 0, contacts = 0;
    int64_t invalid = 0;
    ok = ok && geos && schema(output) && execute(output, "BEGIN") && sqlite3_prepare_v2(source,
        "SELECT ring_id,geometry,points FROM candidate_rings WHERE tolerance=?1 ORDER BY ring_id",
        -1, &rows, NULL) == SQLITE_OK &&
        sqlite3_bind_double(rows, 1, tolerance) == SQLITE_OK;
    int step = SQLITE_DONE;
    sqlite3_stmt *quantized = NULL;
    if (ok && microdegrees) ok = execute(output,
        "CREATE TABLE quantized_rings(ring_id INTEGER PRIMARY KEY,points INTEGER NOT NULL,geometry BLOB NOT NULL);") &&
        sqlite3_prepare_v2(output, "INSERT INTO quantized_rings VALUES(?1,?2,?3)", -1, &quantized, NULL) == SQLITE_OK;
    while (ok && (step = sqlite3_step(rows)) == SQLITE_ROW) {
        GEOSGeometry *geometry = GEOSGeomFromWKB_buf_r(geos, sqlite3_column_blob(rows, 1),
                                                     (size_t)sqlite3_column_bytes(rows, 1));
        if (microdegrees) {
            GEOSGeometry *rounded = geometry ? GEOSGeom_transformXY_r(geos, geometry, microdegree_coordinate, NULL) : NULL;
            GEOSGeom_destroy_r(geos, geometry);
            geometry = rounded;
        }
        int64_t previous_points = points;
        ok = index_ring(output, geos, geometry, sqlite3_column_int64(rows, 0), &points, &chunks) &&
             points - previous_points == sqlite3_column_int64(rows, 2);
        if (ok && microdegrees) ok = save_quantized(quantized, geos, geometry,
            sqlite3_column_int64(rows, 0), sqlite3_column_int64(rows, 2));
        if (!ok) fprintf(stderr, "Invalid or unreadable %s ring %" PRId64 "; refusing publication\n",
                         microdegrees ? "microdegree" : "candidate", (int64_t)sqlite3_column_int64(rows, 0));
        if (!ok && microdegrees && points == previous_points &&
            record_invalid(output, geos, geometry, sqlite3_column_int64(rows, 0))) {
            ++invalid;
            ok = true;
        }
        GEOSGeom_destroy_r(geos, geometry);
        if (++rings % 10000 == 0) {
            ok = ok && execute(output, "COMMIT;BEGIN");
            fprintf(stderr, "Indexed: %" PRId64 " rings, %" PRId64 " points, %" PRId64 " chunks\n", rings, points, chunks);
            fflush(stderr);
        }
    }
    sqlite3_finalize(rows);
    sqlite3_finalize(quantized);
    if (invalid) {
        bool complete_audit = ok && step == SQLITE_DONE &&
            execute(output, "UPDATE review SET status='quantization_invalid_not_install_approved';COMMIT");
        fprintf(stderr, "Microdegree audit: %" PRId64 " rings, %" PRId64 " invalid; %s. "
                        "Boundary comparison and installation refused.\n",
                rings, invalid, complete_audit ? "diagnostics complete" : "diagnostics incomplete");
        if (geos) GEOS_finish_r(geos);
        return false;
    }
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

static bool classify_contacts(sqlite3 *db, GEOSContextHandle_t geos, sqlite3 *source, double tolerance)
{
    sqlite3_stmt *rows = NULL, *original = NULL;
    bool ok = sqlite3_prepare_v2(db,
        "SELECT a.geometry,b.geometry,c.first_ring,c.second_ring FROM contacts c "
        "JOIN chunks a ON a.id=c.first_chunk JOIN chunks b ON b.id=c.second_chunk",
        -1, &rows, NULL) == SQLITE_OK;
    if (source) ok = ok && sqlite3_prepare_v2(source,
        "SELECT count(*) FROM simplified_rings WHERE ring_id IN (?1,?2) AND tolerance=?3 AND fallback=1",
        -1, &original, NULL) == SQLITE_OK;
    unsigned touches = 0, crossings = 0, overlaps = 0, other = 0;
    int step = SQLITE_DONE;
    while (ok && (step = sqlite3_step(rows)) == SQLITE_ROW) {
        if (source) {
            ok = sqlite3_bind_int64(original, 1, sqlite3_column_int64(rows, 2)) == SQLITE_OK &&
                 sqlite3_bind_int64(original, 2, sqlite3_column_int64(rows, 3)) == SQLITE_OK &&
                 sqlite3_bind_double(original, 3, tolerance) == SQLITE_OK && sqlite3_step(original) == SQLITE_ROW;
            bool both = ok && sqlite3_column_int(original, 0) == 2;
            sqlite3_reset(original);
            if (!ok) break;
            if (!both) continue;
        }
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
            {
                GEOSGeometry *intersection = GEOSIntersection_r(geos, a, b);
                char *wkt = intersection ? GEOSGeomToWKT_r(geos, intersection) : NULL;
                fprintf(stderr, "%s contact %" PRId64 "/%" PRId64 " [%s]: %s\n",
                        source ? "Source" : "Candidate",
                        (int64_t)sqlite3_column_int64(rows, 2), (int64_t)sqlite3_column_int64(rows, 3),
                        touch ? "touch" : cross ? "cross" : overlap ? "overlap" : "other", wkt ? wkt : "unavailable");
                ok = wkt != NULL;
                GEOSFree_r(geos, wkt); GEOSGeom_destroy_r(geos, intersection);
            }
        }
        GEOSGeom_destroy_r(geos, a);
        GEOSGeom_destroy_r(geos, b);
    }
    sqlite3_finalize(rows);
    sqlite3_finalize(original);
    if (ok && step == SQLITE_DONE)
        fprintf(stderr, "Contact classification: %u touches, %u crossings, %u overlaps, %u other; "
                        "chunk-level diagnostics, not installation approval\n", touches, crossings, overlaps, other);
    return ok && step == SQLITE_DONE;
}

/* Preserve every earlier repair region; a later pass must never discard an earlier contact. */
static bool copy_contacts(sqlite3 *source, sqlite3 *output)
{
    sqlite3_stmt *rows = NULL, *chunk = NULL, *contact = NULL, *status = NULL;
    bool ok = sqlite3_prepare_v2(source,
        "SELECT 1 FROM review WHERE status='boundary_contacts_found'", -1, &status, NULL) == SQLITE_OK &&
        sqlite3_step(status) == SQLITE_ROW;
    sqlite3_finalize(status);
    ok = ok && sqlite3_prepare_v2(source,
        "SELECT c.first_ring,c.second_ring,a.geometry,b.geometry FROM contacts c "
        "JOIN chunks a ON a.id=c.first_chunk JOIN chunks b ON b.id=c.second_chunk",
        -1, &rows, NULL) == SQLITE_OK && sqlite3_prepare_v2(output,
        "INSERT INTO chunks(ring_id,geometry) VALUES(?1,?2)", -1, &chunk, NULL) == SQLITE_OK &&
        sqlite3_prepare_v2(output, "INSERT INTO contacts VALUES(?1,?2,?3,?4)", -1, &contact, NULL) == SQLITE_OK;
    int step = SQLITE_DONE;
    while (ok && (step = sqlite3_step(rows)) == SQLITE_ROW) {
        for (int i = 0; ok && i < 2; ++i) {
            int bytes = sqlite3_column_bytes(rows, i + 2);
            ok = bytes > 0 && bytes <= 16384 &&
                 sqlite3_bind_int64(chunk, 1, sqlite3_column_int64(rows, i)) == SQLITE_OK &&
                 sqlite3_bind_blob(chunk, 2, sqlite3_column_blob(rows, i + 2), bytes, SQLITE_TRANSIENT) == SQLITE_OK &&
                 sqlite3_step(chunk) == SQLITE_DONE &&
                 sqlite3_bind_int64(contact, i + 1, sqlite3_last_insert_rowid(output)) == SQLITE_OK &&
                 sqlite3_bind_int64(contact, i + 3, sqlite3_column_int64(rows, i)) == SQLITE_OK;
            sqlite3_reset(chunk);
        }
        ok = ok && sqlite3_step(contact) == SQLITE_DONE;
        sqlite3_reset(contact);
    }
    sqlite3_finalize(rows); sqlite3_finalize(chunk); sqlite3_finalize(contact);
    return ok && step == SQLITE_DONE;
}

static bool merge_contacts(const char *path, int count, char **inputs)
{
    file_t *file = file_new_cstr(path);
    sqlite3 *output = NULL;
    bool ok = file && file_open(file, FILE_MODE_CREATE_NEW, FILE_ACCESS_WRITE) && file_close(file) &&
              sqlite3_open_v2(path, &output, SQLITE_OPEN_READWRITE, NULL) == SQLITE_OK &&
              schema(output) && execute(output, "BEGIN");
    for (int i = 0; ok && i < count; ++i) {
        sqlite3 *source = NULL;
        ok = sqlite3_open_v2(inputs[i], &source, SQLITE_OPEN_READONLY, NULL) == SQLITE_OK &&
             copy_contacts(source, output);
        if (source) sqlite3_close(source);
    }
    ok = ok && execute(output,
        "CREATE TABLE repair_regions_only(note TEXT);"
        "INSERT INTO repair_regions_only VALUES('Cumulative repair inputs, not a candidate validation');"
        "UPDATE review SET status='boundary_contacts_found';COMMIT");
    if (output) sqlite3_close(output);
    file_free(file);
    return ok;
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
    ok = ok && intersections(db, geos, &comparisons, &contacts) && contacts == 3 && classify_contacts(db, geos, NULL, 0);
    sqlite3 *merged = NULL;
    ok = ok && sqlite3_open(":memory:", &merged) == SQLITE_OK && schema(merged) &&
         !copy_contacts(db, merged) && execute(db, "UPDATE review SET status='boundary_contacts_found'") &&
         copy_contacts(db, merged) && copy_contacts(db, merged);
    sqlite3_stmt *merged_count = NULL;
    ok = ok && sqlite3_prepare_v2(merged,
        "SELECT (SELECT count(*) FROM contacts),(SELECT count(*) FROM chunks)", -1, &merged_count, NULL) == SQLITE_OK &&
         sqlite3_step(merged_count) == SQLITE_ROW && sqlite3_column_int(merged_count, 0) == 6 &&
         sqlite3_column_int(merged_count, 1) == 12;
    sqlite3_finalize(merged_count);
    if (merged) sqlite3_close(merged);
    ok = ok && execute(db, "DELETE FROM chunks;DELETE FROM bounds;DELETE FROM contacts");
    points = chunks = comparisons = contacts = 0;
    const char *separate[] = {"POLYGON((0 0,2 0,2 2,0 2,0 0))", "POLYGON((3 3,4 3,4 4,3 4,3 3))"};
    for (unsigned i = 0; ok && i < sizeof separate / sizeof *separate; ++i) {
        GEOSGeometry *geometry = GEOSGeomFromWKT_r(geos, separate[i]);
        ok = index_ring(db, geos, geometry, i + 1, &points, &chunks);
        GEOSGeom_destroy_r(geos, geometry);
    }
    ok = ok && intersections(db, geos, &comparisons, &contacts) && contacts == 0 && comparisons == 0;
    ok = ok && execute(db, "DELETE FROM chunks;DELETE FROM bounds;DELETE FROM contacts;DELETE FROM seam_contacts");
    points = chunks = comparisons = contacts = 0;
    const char *clipped[] = {"POLYGON((179 10,180 10,180 12,179 12,179 10))",
                            "POLYGON((-180 10,-179 10,-179 12,-180 12,-180 10))"};
    for (unsigned i = 0; ok && i < sizeof clipped / sizeof *clipped; ++i) {
        GEOSGeometry *geometry = GEOSGeomFromWKT_r(geos, clipped[i]);
        ok = index_ring(db, geos, geometry, i + 1, &points, &chunks);
        GEOSGeom_destroy_r(geos, geometry);
    }
    sqlite3_stmt *seams = NULL;
    ok = ok && intersections(db, geos, &comparisons, &contacts) && contacts == 0 && comparisons == 2 &&
         sqlite3_prepare_v2(db, "SELECT count(*) FROM seam_contacts", -1, &seams, NULL) == SQLITE_OK &&
         sqlite3_step(seams) == SQLITE_ROW && sqlite3_column_int(seams, 0) == 2;
    sqlite3_finalize(seams);
    GEOSGeometry *edge = GEOSGeomFromWKT_r(geos, "LINESTRING(180 10,180 12)");
    GEOSGeometry *near = GEOSGeomFromWKT_r(geos, "LINESTRING(179.999999 10,179.999999 12)");
    ok = ok && edge && near && split_seam(geos, edge, edge, 179, 180, 180, 181) == 1 &&
         split_seam(geos, edge, edge, 179, 180, 179, 180) == 0 &&
         split_seam(geos, near, near, 179, 180, 180, 181) == 0 &&
         split_seam(geos, edge, edge, 179, 181, 180, 181) == 0;
    GEOSGeom_destroy_r(geos, edge); GEOSGeom_destroy_r(geos, near);
    GEOSGeometry *invalid = geos ? GEOSGeomFromWKT_r(geos, "POLYGON((0 0,2 2,0 2,2 0,0 0))") : NULL;
    ok = ok && invalid && !index_ring(db, geos, invalid, 3, &points, &chunks);
    GEOSGeom_destroy_r(geos, invalid);
    GEOSGeometry *precise = GEOSGeomFromWKT_r(geos,
        "POLYGON((-1.0000004 1.0000004,2.0000004 1.0000004,2.0000004 2.0000004,-1.0000004 1.0000004))");
    GEOSGeometry *rounded = precise ? GEOSGeom_transformXY_r(geos, precise, microdegree_coordinate, NULL) : NULL;
    GEOSGeometry *expected = GEOSGeomFromWKT_r(geos, "POLYGON((-1 1,2 1,2 2,-1 1))");
    ok = ok && rounded && expected && GEOSEqualsExact_r(geos, rounded, expected, 0) == 1;
    GEOSGeom_destroy_r(geos, precise); GEOSGeom_destroy_r(geos, rounded); GEOSGeom_destroy_r(geos, expected);
    precise = GEOSGeomFromWKT_r(geos, "POLYGON((0 0,0.0000001 0,0.0000001 0.0000001,0 0))");
    rounded = precise ? GEOSGeom_transformXY_r(geos, precise, microdegree_coordinate, NULL) : NULL;
    ok = ok && rounded && GEOSisValid_r(geos, rounded) == 0;
    ok = ok && record_invalid(db, geos, rounded, 999);
    sqlite3_stmt *issue = NULL;
    ok = ok && sqlite3_prepare_v2(db, "SELECT count(*) FROM invalid_rings WHERE ring_id=999 AND reason IS NOT NULL",
                                 -1, &issue, NULL) == SQLITE_OK && sqlite3_step(issue) == SQLITE_ROW &&
         sqlite3_column_int(issue, 0) == 1;
    sqlite3_finalize(issue);
    GEOSGeom_destroy_r(geos, precise); GEOSGeom_destroy_r(geos, rounded);
    const char *rounding_contacts[] = {
        "POLYGON((0 0,1.0000002 0,1.0000002 1,0 1,0 0))",
        "POLYGON((1.0000004 0,2 0,2 1,1.0000004 1,1.0000004 0))"
    };
    for (unsigned quantize = 0; ok && quantize < 2; ++quantize) {
        ok = execute(db, "DELETE FROM chunks;DELETE FROM bounds;DELETE FROM contacts;DELETE FROM seam_contacts");
        points = chunks = comparisons = contacts = 0;
        for (unsigned i = 0; ok && i < 2; ++i) {
            precise = GEOSGeomFromWKT_r(geos, rounding_contacts[i]);
            rounded = precise && quantize ? GEOSGeom_transformXY_r(geos, precise, microdegree_coordinate, NULL) : NULL;
            ok = index_ring(db, geos, quantize ? rounded : precise, i + 1, &points, &chunks);
            GEOSGeom_destroy_r(geos, precise); GEOSGeom_destroy_r(geos, rounded);
        }
        ok = ok && intersections(db, geos, &comparisons, &contacts) && contacts == quantize;
    }
    ok = ok && execute(db,
        "ATTACH ':memory:' AS repair;"
        "CREATE TABLE simplified_rings(ring_id INTEGER,tolerance REAL,geometry BLOB,points INTEGER);"
        "INSERT INTO simplified_rings VALUES(1,8,x'01',5),(2,8,x'02',5);"
        "CREATE TABLE repair.status(value TEXT);INSERT INTO repair.status VALUES('coverage_union_requires_global_review');"
        "CREATE TABLE repair.originals(ring_id INTEGER PRIMARY KEY,tolerance REAL,geometry BLOB);"
        "INSERT INTO repair.originals SELECT ring_id,tolerance,geometry FROM simplified_rings;"
        "CREATE TABLE repair.replacements(ring_id INTEGER PRIMARY KEY,tolerance REAL,geometry BLOB,points INTEGER);"
        "INSERT INTO repair.replacements VALUES(1,8,x'03',9);"
        "CREATE TABLE repair.absorbed(ring_id INTEGER PRIMARY KEY,retained_id INTEGER);"
        "INSERT INTO repair.absorbed VALUES(2,1)") && validate_overlay(db, 8);
    sqlite3_stmt *candidate = NULL;
    ok = ok && sqlite3_prepare_v2(db, "SELECT count(*),sum(points),hex(geometry) FROM candidate_rings",
                                  -1, &candidate, NULL) == SQLITE_OK && sqlite3_step(candidate) == SQLITE_ROW &&
         sqlite3_column_int(candidate, 0) == 1 && sqlite3_column_int(candidate, 1) == 9 &&
         !strcmp((const char *)sqlite3_column_text(candidate, 2), "03");
    sqlite3_finalize(candidate);
    ok = ok && execute(db, "DROP VIEW candidate_rings;UPDATE simplified_rings SET geometry=x'04' WHERE ring_id=2") &&
         !validate_overlay(db, 8);
    if (db) sqlite3_close(db);
    if (geos) GEOS_finish_r(geos);
    fprintf(stderr, "Coastline boundary review self-test: %s\n", ok ? "PASS" : "FAIL");
    return ok;
}

/* Run a separate read-only-source review; never replace an existing review database. */
int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "--self-test")) return self_test() ? 0 : 1;
    if (argc >= 5 && !strcmp(argv[1], "--merge-contacts"))
        return merge_contacts(argv[2], argc - 3, argv + 3) ? 0 : 1;
    if (argc == 5 && !strcmp(argv[1], "--source-contacts")) {
        char *end;
        errno = 0;
        double tolerance = strtod(argv[3], &end);
        if (errno || end == argv[3] || *end || !isfinite(tolerance) || tolerance < 0.5 || tolerance > 40) return 2;
        sqlite3 *source = NULL, *db = NULL;
        GEOSContextHandle_t geos = GEOS_init_r();
        bool ok = geos && sqlite3_open_v2(argv[2], &source, SQLITE_OPEN_READONLY, NULL) == SQLITE_OK &&
                  sqlite3_open_v2(argv[4], &db, SQLITE_OPEN_READONLY, NULL) == SQLITE_OK &&
                  classify_contacts(db, geos, source, tolerance);
        if (source) sqlite3_close(source);
        if (db) sqlite3_close(db);
        if (geos) GEOS_finish_r(geos);
        return ok ? 0 : 1;
    }
    if (argc == 3 && !strcmp(argv[1], "--classify-contacts")) {
        sqlite3 *db = NULL;
        GEOSContextHandle_t geos = GEOS_init_r();
        bool ok = geos && sqlite3_open_v2(argv[2], &db, SQLITE_OPEN_READONLY, NULL) == SQLITE_OK &&
                  classify_contacts(db, geos, NULL, 0);
        if (db) sqlite3_close(db);
        if (geos) GEOS_finish_r(geos);
        return ok ? 0 : 1;
    }
    bool microdegrees = argc >= 5 && !strcmp(argv[argc - 1], "--microdegrees");
    int positional = argc - (microdegrees ? 1 : 0);
    if (positional != 4 && positional != 5) {
        fprintf(stderr, "Usage: %s candidate.sqlite3 tolerance NEW-review.sqlite3 [repair-overlay.sqlite3] "
                        "[--microdegrees]\n", argv[0]);
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
              execute(source, "PRAGMA cache_size=-16384;BEGIN") &&
              review(source, output, tolerance, positional == 5 ? argv[4] : NULL, microdegrees);
    if (source) sqlite3_close(source);
    if (output) sqlite3_close(output);
    file_free(file);
    return ok ? 0 : 1;
}
