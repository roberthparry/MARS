/**
 * @file coastline_repair.c
 * @brief Reversible offline overlay for overlapping original coastline polygons.
 *
 * Reads a completed contact review and unchanged source-fallback candidates. Only
 * small, equally oriented polygons with overlapping interiors can be merged. The
 * exact union preserves their combined coverage; holes, disconnected results and
 * overlapping repair groups are rejected. Original WKB is retained for provenance
 * and stale-overlay detection. Neither staging nor installed databases are modified.
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
    bool ok = sqlite3_exec(db, sql, NULL, NULL, &error) == SQLITE_OK;
    if (!ok) fprintf(stderr, "%s\n", error ? error : sqlite3_errmsg(db));
    sqlite3_free(error);
    return ok;
}

static GEOSGeometry *coverage_union(GEOSContextHandle_t geos, const GEOSGeometry *a, const GEOSGeometry *b)
{
    if (!a || !b || GEOSGeomTypeId_r(geos, a) != GEOS_POLYGON || GEOSGeomTypeId_r(geos, b) != GEOS_POLYGON ||
        GEOSisValid_r(geos, a) != 1 || GEOSisValid_r(geos, b) != 1 ||
        GEOSGetNumInteriorRings_r(geos, a) != 0 || GEOSGetNumInteriorRings_r(geos, b) != 0) return NULL;
    char a_ccw, b_ccw;
    if (!GEOSCoordSeq_isCCW_r(geos, GEOSGeom_getCoordSeq_r(geos, GEOSGetExteriorRing_r(geos, a)), &a_ccw) ||
        !GEOSCoordSeq_isCCW_r(geos, GEOSGeom_getCoordSeq_r(geos, GEOSGetExteriorRing_r(geos, b)), &b_ccw) ||
        a_ccw != b_ccw) return NULL;
    GEOSGeometry *overlap = GEOSIntersection_r(geos, a, b);
    double area = 0;
    bool ok = overlap && GEOSArea_r(geos, overlap, &area) && area > 0;
    GEOSGeom_destroy_r(geos, overlap);
    GEOSGeometry *merged = ok ? GEOSUnion_r(geos, a, b) : NULL;
    ok = merged && GEOSGeomTypeId_r(geos, merged) == GEOS_POLYGON &&
         GEOSGetNumInteriorRings_r(geos, merged) == 0 && GEOSisValid_r(geos, merged) == 1;
    char ccw;
    ok = ok && GEOSCoordSeq_isCCW_r(geos, GEOSGeom_getCoordSeq_r(geos, GEOSGetExteriorRing_r(geos, merged)), &ccw);
    if (ok && ccw != a_ccw) {
        GEOSGeometry *reversed = GEOSReverse_r(geos, merged);
        GEOSGeom_destroy_r(geos, merged);
        merged = reversed;
    } else if (!ok) {
        GEOSGeom_destroy_r(geos, merged);
        merged = NULL;
    }
    return merged;
}

static bool attach(sqlite3 *db, const char *path, const char *sql)
{
    if (strchr(path, '?') || strchr(path, '#')) return false;
    char *uri = sqlite3_mprintf("file:%s?mode=ro", path);
    sqlite3_stmt *stmt = NULL;
    bool ok = uri && sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK &&
        sqlite3_bind_text(stmt, 1, uri, -1, SQLITE_TRANSIENT) == SQLITE_OK && sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    sqlite3_free(uri);
    return ok;
}

static bool repair(sqlite3 *db, double tolerance)
{
    sqlite3_stmt *rows = NULL, *save = NULL, *original = NULL;
    bool ok = execute(db,
        "CREATE TABLE status(value TEXT);INSERT INTO status VALUES('incomplete');"
        "CREATE TABLE originals(ring_id INTEGER PRIMARY KEY,tolerance REAL,geometry BLOB NOT NULL);"
        "CREATE TABLE replacements(ring_id INTEGER PRIMARY KEY,tolerance REAL,geometry BLOB NOT NULL,points INTEGER);"
        "CREATE TABLE absorbed(ring_id INTEGER PRIMARY KEY,retained_id INTEGER NOT NULL);BEGIN") &&
        sqlite3_prepare_v2(db,
        "SELECT DISTINCT min(c.first_ring,c.second_ring),max(c.first_ring,c.second_ring),a.geometry,b.geometry "
        "FROM review.contacts c JOIN stage.simplified_rings a ON a.ring_id=min(c.first_ring,c.second_ring) "
        "JOIN stage.simplified_rings b ON b.ring_id=max(c.first_ring,c.second_ring) "
        "WHERE a.tolerance=?1 AND b.tolerance=?1 AND a.fallback=1 AND b.fallback=1 "
        "AND a.points<=500000 AND b.points<=500000 "
        "AND EXISTS(SELECT 1 FROM review.review WHERE status='boundary_contacts_found')", -1, &rows, NULL) == SQLITE_OK &&
        sqlite3_bind_double(rows, 1, tolerance) == SQLITE_OK && sqlite3_prepare_v2(db,
        "INSERT INTO originals VALUES(?1,?2,?3)", -1, &original, NULL) == SQLITE_OK && sqlite3_prepare_v2(db,
        "INSERT INTO replacements VALUES(?1,?2,?3,?4)", -1, &save, NULL) == SQLITE_OK;
    GEOSContextHandle_t geos = GEOS_init_r();
    ok = ok && geos;
    unsigned repaired = 0;
    int step = SQLITE_DONE;
    while (ok && (step = sqlite3_step(rows)) == SQLITE_ROW) {
        GEOSGeometry *a = GEOSGeomFromWKB_buf_r(geos, sqlite3_column_blob(rows, 2), sqlite3_column_bytes(rows, 2));
        GEOSGeometry *b = GEOSGeomFromWKB_buf_r(geos, sqlite3_column_blob(rows, 3), sqlite3_column_bytes(rows, 3));
        GEOSGeometry *merged = coverage_union(geos, a, b);
        if (merged) {
            /* A primary-key collision rejects chained/ambiguous repair groups instead of dropping coverage. */
            for (int i = 0; ok && i < 2; ++i) {
                ok = sqlite3_bind_int64(original, 1, sqlite3_column_int64(rows, i)) == SQLITE_OK &&
                     sqlite3_bind_double(original, 2, tolerance) == SQLITE_OK &&
                     sqlite3_bind_blob(original, 3, sqlite3_column_blob(rows, i + 2),
                                       sqlite3_column_bytes(rows, i + 2), SQLITE_TRANSIENT) == SQLITE_OK &&
                     sqlite3_step(original) == SQLITE_DONE;
                sqlite3_reset(original);
            }
            size_t bytes = 0;
            unsigned char *wkb = GEOSGeomToWKB_buf_r(geos, merged, &bytes);
            int points = GEOSGetNumCoordinates_r(geos, merged);
            ok = ok && wkb && bytes <= 32 * 1048576 && points >= 4 && points <= 1000000 &&
                 sqlite3_bind_int64(save, 1, sqlite3_column_int64(rows, 0)) == SQLITE_OK &&
                 sqlite3_bind_double(save, 2, tolerance) == SQLITE_OK &&
                 sqlite3_bind_blob(save, 3, wkb, (int)bytes, SQLITE_TRANSIENT) == SQLITE_OK &&
                 sqlite3_bind_int(save, 4, points) == SQLITE_OK && sqlite3_step(save) == SQLITE_DONE;
            sqlite3_reset(save);
            GEOSFree_r(geos, wkb);
            sqlite3_stmt *absorbed = NULL;
            ok = ok && sqlite3_prepare_v2(db, "INSERT INTO absorbed VALUES(?1,?2)", -1, &absorbed, NULL) == SQLITE_OK &&
                 sqlite3_bind_int64(absorbed, 1, sqlite3_column_int64(rows, 1)) == SQLITE_OK &&
                 sqlite3_bind_int64(absorbed, 2, sqlite3_column_int64(rows, 0)) == SQLITE_OK &&
                 sqlite3_step(absorbed) == SQLITE_DONE;
            sqlite3_finalize(absorbed);
            if (ok) {
                ++repaired;
                fprintf(stderr, "Coverage union: %" PRId64 " + %" PRId64 ", %d points; originals preserved\n",
                        (int64_t)sqlite3_column_int64(rows, 0), (int64_t)sqlite3_column_int64(rows, 1), points);
            }
        }
        GEOSGeom_destroy_r(geos, merged); GEOSGeom_destroy_r(geos, a); GEOSGeom_destroy_r(geos, b);
    }
    sqlite3_finalize(rows); sqlite3_finalize(save); sqlite3_finalize(original);
    if (geos) GEOS_finish_r(geos);
    ok = ok && step == SQLITE_DONE && repaired > 0 &&
         execute(db, "UPDATE status SET value='coverage_union_requires_global_review';COMMIT");
    if (!ok) execute(db, "ROLLBACK");
    return ok;
}

static bool self_test(void)
{
    GEOSContextHandle_t geos = GEOS_init_r();
    if (!geos) return false;
    GEOSGeometry *a = GEOSGeomFromWKT_r(geos, "POLYGON((0 0,2 0,2 2,0 2,0 0))");
    GEOSGeometry *b = GEOSGeomFromWKT_r(geos, "POLYGON((1 1,3 1,3 3,1 3,1 1))");
    GEOSGeometry *result = coverage_union(geos, a, b);
    double area = 0;
    bool ok = result && GEOSArea_r(geos, result, &area) && area == 7 &&
              GEOSCovers_r(geos, result, a) == 1 && GEOSCovers_r(geos, result, b) == 1;
    GEOSGeom_destroy_r(geos, result);
    GEOSGeometry *reverse = GEOSReverse_r(geos, b);
    result = coverage_union(geos, a, reverse);
    ok = ok && !result;
    GEOSGeom_destroy_r(geos, result); GEOSGeom_destroy_r(geos, reverse);
    GEOSGeometry *apart = GEOSGeomFromWKT_r(geos, "POLYGON((3 3,4 3,4 4,3 4,3 3))");
    result = coverage_union(geos, a, apart);
    ok = ok && !result;
    GEOSGeom_destroy_r(geos, result); GEOSGeom_destroy_r(geos, apart);
    sqlite3 *db = NULL;
    ok = ok && sqlite3_open(":memory:", &db) == SQLITE_OK && execute(db,
        "ATTACH ':memory:' AS stage;ATTACH ':memory:' AS review;"
        "CREATE TABLE stage.simplified_rings(ring_id INTEGER,tolerance REAL,geometry BLOB,points INTEGER,fallback INTEGER);"
        "CREATE TABLE review.contacts(first_ring INTEGER,second_ring INTEGER);"
        "INSERT INTO review.contacts VALUES(1,2),(2,1);"
        "CREATE TABLE review.review(status TEXT);INSERT INTO review.review VALUES('boundary_contacts_found')");
    sqlite3_stmt *insert = NULL;
    ok = ok && sqlite3_prepare_v2(db, "INSERT INTO stage.simplified_rings VALUES(?1,8,?2,5,1)",
                                  -1, &insert, NULL) == SQLITE_OK;
    for (int i = 0; ok && i < 2; ++i) {
        size_t bytes = 0;
        unsigned char *wkb = GEOSGeomToWKB_buf_r(geos, i ? b : a, &bytes);
        ok = wkb && sqlite3_bind_int(insert, 1, i + 1) == SQLITE_OK &&
             sqlite3_bind_blob(insert, 2, wkb, (int)bytes, SQLITE_TRANSIENT) == SQLITE_OK &&
             sqlite3_step(insert) == SQLITE_DONE;
        sqlite3_reset(insert);
        GEOSFree_r(geos, wkb);
    }
    sqlite3_finalize(insert);
    ok = ok && repair(db, 8);
    sqlite3_stmt *check = NULL;
    ok = ok && sqlite3_prepare_v2(db,
        "SELECT (SELECT count(*) FROM replacements),(SELECT count(*) FROM absorbed),"
        "(SELECT count(*) FROM originals o JOIN stage.simplified_rings s "
        "ON o.ring_id=s.ring_id AND o.geometry=s.geometry),"
        "(SELECT value FROM status)", -1, &check, NULL) == SQLITE_OK && sqlite3_step(check) == SQLITE_ROW &&
        sqlite3_column_int(check, 0) == 1 && sqlite3_column_int(check, 1) == 1 &&
        sqlite3_column_int(check, 2) == 2 &&
        !strcmp((const char *)sqlite3_column_text(check, 3), "coverage_union_requires_global_review");
    sqlite3_finalize(check);
    if (db) sqlite3_close(db);
    GEOSGeom_destroy_r(geos, a); GEOSGeom_destroy_r(geos, b);
    GEOS_finish_r(geos);
    fprintf(stderr, "Coastline coverage repair self-test: %s\n", ok ? "PASS" : "FAIL");
    return ok;
}

/* Create a separate repair overlay; installation always requires a subsequent global review. */
int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "--self-test")) return self_test() ? 0 : 1;
    if (argc != 5) {
        fprintf(stderr, "Usage: %s stage.sqlite3 tolerance review.sqlite3 NEW-overlay.sqlite3\n", argv[0]);
        return 2;
    }
    char *end;
    errno = 0;
    double tolerance = strtod(argv[2], &end);
    if (errno || end == argv[2] || *end || !isfinite(tolerance) || tolerance < 0.5 || tolerance > 40) return 2;
    file_t *file = file_new_cstr(argv[4]);
    sqlite3 *db = NULL;
    bool ok = file && file_open(file, FILE_MODE_CREATE_NEW, FILE_ACCESS_WRITE) && file_close(file) &&
        sqlite3_open_v2(argv[4], &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_URI, NULL) == SQLITE_OK &&
        attach(db, argv[1], "ATTACH DATABASE ?1 AS stage") && attach(db, argv[3], "ATTACH DATABASE ?1 AS review") &&
        execute(db, "PRAGMA cache_size=-16384;PRAGMA stage.cache_size=-16384;PRAGMA review.cache_size=-16384") &&
        repair(db, tolerance);
    if (db) sqlite3_close(db);
    file_free(file);
    return ok ? 0 : 1;
}
