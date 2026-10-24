/**
 * @file coastline_polar.c
 * @brief Offline completion of a clipped Antarctic polygon using the packaged ice-front source.
 *
 * Copies a reconciled candidate and adds legacy Antarctic coverage only south of
 * the newer polygon's coverage limit. Exact set comparisons protect the newer
 * northern coverage and the legacy southern coverage. Originals and cutoff remain
 * in the output audit; fresh boundary review is mandatory. No normal import or
 * installed database is involved.
 */
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <geos_c.h>
#include <sqlite3.h>

#include "file-t.h"

static int64_t integer(const unsigned char *p)
{
    uint32_t v = (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
    return v <= INT32_MAX ? (int64_t)v : (int64_t)v - 4294967296LL;
}

static bool sql(sqlite3 *db, const char *command)
{
    bool ok = sqlite3_exec(db, command, NULL, NULL, NULL) == SQLITE_OK;
    if (!ok) fprintf(stderr, "Polar preparation: %s\n", sqlite3_errmsg(db));
    return ok;
}

/* Clip before polygon validation: irrelevant northern legacy defects must not enter the southern fallback. */
static GEOSCoordSequence *clip_south(GEOSContextHandle_t geos, const GEOSCoordSequence *source, double cutoff)
{
    unsigned count = 0;
    if (!source || !GEOSCoordSeq_getSize_r(geos, source, &count) || count < 4 || count > 20000003) return NULL;
    double *points = malloc(((size_t)count * 2 + 1) * 2 * sizeof *points);
    if (!points) return NULL;
    size_t used = 0;
    double previous_x, previous_y;
    bool ok = GEOSCoordSeq_getXY_r(geos, source, 0, &previous_x, &previous_y);
    for (unsigned i = 1; ok && i < count; ++i) {
        double x, y;
        ok = GEOSCoordSeq_getXY_r(geos, source, i, &x, &y);
        if (!ok) break;
        bool before = previous_y <= cutoff, after = y <= cutoff;
        if (before != after) {
            points[used * 2] = previous_x + (x - previous_x) * (cutoff - previous_y) / (y - previous_y);
            points[used * 2 + 1] = cutoff; ++used;
        }
        if (after) { points[used * 2] = x; points[used * 2 + 1] = y; ++used; }
        previous_x = x; previous_y = y;
    }
    if (ok && used >= 3 && (points[0] != points[(used - 1) * 2] || points[1] != points[(used - 1) * 2 + 1])) {
        points[used * 2] = points[0]; points[used * 2 + 1] = points[1]; ++used;
    }
    GEOSCoordSequence *result = ok && used >= 4 ? GEOSCoordSeq_create_r(geos, (unsigned)used, 2) : NULL;
    ok = result != NULL;
    for (unsigned i = 0; ok && i < used; ++i)
        ok = GEOSCoordSeq_setXY_r(geos, result, i, points[i * 2], points[i * 2 + 1]);
    free(points);
    if (!ok) { GEOSCoordSeq_destroy_r(geos, result); result = NULL; }
    return result;
}

static GEOSGeometry *legacy_cap(GEOSContextHandle_t geos, file_t *file, int64_t *source_id, double cutoff)
{
    unsigned char header[44], buffer[8192];
    GEOSGeometry *result = NULL;
    bool ok = file_open_read(file);
    size_t bytes = 0;
    while (ok) {
        ok = file_read(file, header, sizeof header, &bytes);
        if (!ok || !bytes) break;
        int64_t count = bytes == sizeof header ? integer(header + 4) : 0;
        bool selected = bytes == sizeof header && (integer(header + 8) & 255) == 5 &&
            integer(header + 12) == -180000000 && integer(header + 16) == 180000000 &&
            integer(header + 20) == -90000000;
        ok = bytes == sizeof header && count >= 2 && count <= 20000000 && (!selected || !result);
        if (!ok) break;
        GEOSCoordSequence *seq = selected ? GEOSCoordSeq_create_r(geos, (unsigned)count + 3, 2) : NULL;
        ok = !selected || seq;
        double first_x = 0, first_y = 0, last_x = 0;
        for (int64_t first = 0; ok && first < count; first += 1024) {
            size_t n = count - first < 1024 ? (size_t)(count - first) : 1024;
            ok = file_read(file, buffer, n * 8, &bytes) && bytes == n * 8;
            for (size_t i = 0; selected && ok && i < n; ++i) {
                double x = (double)integer(buffer + i * 8) * 0.000001;
                double y = (double)integer(buffer + i * 8 + 4) * 0.000001;
                if (x > 180) x -= 360;
                ok = x >= -180 && x <= 180 && y >= -90 && y <= 0 &&
                    GEOSCoordSeq_setXY_r(geos, seq, (unsigned)(first + i), x, y);
                if (!first && !i) { first_x = x; first_y = y; }
                last_x = x;
            }
        }
        if (selected) {
            fprintf(stderr, "Legacy Antarctic endpoints: %.9f to %.9f; %" PRId64 " source points\n",
                    first_x, last_x, count);
            ok = ok && fabs(first_x) == 180 && fabs(last_x) == 180 && first_x != last_x &&
                GEOSCoordSeq_setXY_r(geos, seq, (unsigned)count, last_x, -90) &&
                GEOSCoordSeq_setXY_r(geos, seq, (unsigned)count + 1, first_x, -90) &&
                GEOSCoordSeq_setXY_r(geos, seq, (unsigned)count + 2, first_x, first_y);
            GEOSCoordSequence *clipped = ok ? clip_south(geos, seq, cutoff) : NULL;
            GEOSCoordSeq_destroy_r(geos, seq);
            GEOSGeometry *shell = clipped ? GEOSGeom_createLinearRing_r(geos, clipped) : NULL;
            result = shell ? GEOSGeom_createPolygon_r(geos, shell, NULL, 0) : NULL;
            ok = result && GEOSisValid_r(geos, result) == 1;
            if (result && !ok) {
                char *reason = GEOSisValidReason_r(geos, result);
                fprintf(stderr, "Legacy Antarctic geometry rejected: %s\n", reason ? reason : "unknown");
                GEOSFree_r(geos, reason);
            }
            *source_id = integer(header);
        }
    }
    ok = ok && bytes == 0 && file_close(file) && result;
    if (!ok) { GEOSGeom_destroy_r(geos, result); result = NULL; }
    return result;
}

static bool equal_region(GEOSContextHandle_t geos, const GEOSGeometry *actual, const GEOSGeometry *expected,
                         const GEOSGeometry *region)
{
    GEOSGeometry *a = GEOSIntersection_r(geos, actual, region);
    GEOSGeometry *b = GEOSIntersection_r(geos, expected, region);
    /* The joining latitude is shared; only positive-area differences alter coverage. */
    GEOSGeometry *difference = a && b ? GEOSSymDifference_r(geos, a, b) : NULL;
    double area = -1;
    bool ok = difference && GEOSArea_r(geos, difference, &area) && area == 0;
    if (!ok) fprintf(stderr, "Polar coverage comparison failed: difference area %.17g\n", area);
    GEOSGeom_destroy_r(geos, difference); GEOSGeom_destroy_r(geos, a); GEOSGeom_destroy_r(geos, b);
    return ok;
}

/* Recognise the full-width synthetic Mercator closure, not an arbitrary southern coastal vertex. */
static bool coverage_cutoff(GEOSContextHandle_t geos, const GEOSGeometry *polygon, double *cutoff)
{
    const GEOSGeometry *shell = polygon ? GEOSGetExteriorRing_r(geos, polygon) : NULL;
    const GEOSCoordSequence *seq = shell ? GEOSGeom_getCoordSeq_r(geos, shell) : NULL;
    unsigned count = 0, selected = 0, transitions = 0;
    if (!seq || !GEOSCoordSeq_getSize_r(geos, seq, &count) || count < 4) return false;
    double west = INFINITY, east = -INFINITY, south = INFINITY, north = -INFINITY, x, y;
    if (!GEOSCoordSeq_getXY_r(geos, seq, count - 2, &x, &y)) return false;
    bool previous = y <= -85;
    for (unsigned i = 0; i + 1 < count; ++i) {
        if (!GEOSCoordSeq_getXY_r(geos, seq, i, &x, &y)) return false;
        bool below = y <= -85;
        transitions += below != previous; previous = below;
        if (below) {
            ++selected;
            west = fmin(west, x); east = fmax(east, x);
            south = fmin(south, y); north = fmax(north, y);
        }
    }
    bool ok = selected >= 2 && transitions == 2 && west <= -179.999999 && east >= 179.999999 &&
              south >= -85.1 && north - south <= 0.01;
    if (ok) {
        *cutoff = north;
        fprintf(stderr, "Synthetic polar closure: %.9f to %.9f; coverage cutoff %.9f\n", south, north, north);
    } else fprintf(stderr, "Polar coverage cutoff rejected: unrecognised synthetic closure\n");
    return ok;
}

static GEOSGeometry *complete_cap(GEOSContextHandle_t geos, const GEOSGeometry *newer,
                                  const GEOSGeometry *legacy, double cutoff)
{
    if (!newer || !legacy || !isfinite(cutoff) || cutoff <= -90 || cutoff >= -60 ||
        GEOSisValid_r(geos, newer) != 1 || GEOSisValid_r(geos, legacy) != 1) return NULL;
    GEOSGeometry *south = GEOSGeom_createRectangle_r(geos, -180, -90, 180, cutoff);
    GEOSGeometry *north = GEOSGeom_createRectangle_r(geos, -180, cutoff, 180, 90);
    GEOSGeometry *fallback = south ? GEOSIntersection_r(geos, legacy, south) : NULL;
    GEOSGeometry *merged = fallback ? GEOSUnion_r(geos, newer, fallback) : NULL;
    fprintf(stderr, "Polar union: type %d, valid %d, interior rings %d\n",
            merged ? GEOSGeomTypeId_r(geos, merged) : -1, merged ? GEOSisValid_r(geos, merged) : -1,
            merged && GEOSGeomTypeId_r(geos, merged) == GEOS_POLYGON ? GEOSGetNumInteriorRings_r(geos, merged) : -1);
    bool ok = merged && north && GEOSisValid_r(geos, merged) == 1 &&
        GEOSGeomTypeId_r(geos, merged) == GEOS_POLYGON && GEOSGetNumInteriorRings_r(geos, merged) == 0 &&
        equal_region(geos, merged, newer, north) && equal_region(geos, merged, legacy, south);
    char ccw = 0;
    ok = ok && GEOSCoordSeq_isCCW_r(geos, GEOSGeom_getCoordSeq_r(geos,
        GEOSGetExteriorRing_r(geos, merged)), &ccw);
    if (ok && !ccw) {
        GEOSGeometry *reverse = GEOSReverse_r(geos, merged);
        GEOSGeom_destroy_r(geos, merged); merged = reverse; ok = merged != NULL;
    }
    GEOSGeom_destroy_r(geos, south); GEOSGeom_destroy_r(geos, north); GEOSGeom_destroy_r(geos, fallback);
    if (!ok) { GEOSGeom_destroy_r(geos, merged); merged = NULL; }
    return merged;
}

static bool prepare_polar(sqlite3 *db, GEOSContextHandle_t geos, file_t *source)
{
    sqlite3_stmt *row = NULL;
    bool ok = sql(db, "PRAGMA cache_size=-16384;PRAGMA temp_store=FILE;BEGIN") &&
        sqlite3_prepare_v2(db, "SELECT m.id,s.geometry FROM ring_bounds b JOIN ring_metadata m ON m.id=b.id "
            "JOIN simplified_rings s ON s.ring_id=m.id WHERE m.level=1 AND b.west=-180 AND b.east=180 "
            "AND b.south<-85 AND EXISTS(SELECT 1 FROM reconciliation WHERE "
            "status='reconciled_requires_fresh_boundary_and_geographic_review')", -1, &row, NULL) == SQLITE_OK &&
        sqlite3_step(row) == SQLITE_ROW;
    int64_t id = ok ? sqlite3_column_int64(row, 0) : 0, legacy_id = 0;
    GEOSGeometry *newer = ok ? GEOSGeomFromWKB_buf_r(geos, sqlite3_column_blob(row, 1), sqlite3_column_bytes(row, 1)) : NULL;
    double cutoff = 0;
    ok = newer && coverage_cutoff(geos, newer, &cutoff) && sqlite3_step(row) == SQLITE_DONE;
    sqlite3_finalize(row); row = NULL;
    GEOSGeometry *legacy = ok ? legacy_cap(geos, source, &legacy_id, cutoff) : NULL;
    GEOSGeometry *merged = legacy ? complete_cap(geos, newer, legacy, cutoff) : NULL;
    size_t bytes = 0;
    unsigned char *wkb = merged ? GEOSGeomToWKB_buf_r(geos, merged, &bytes) : NULL;
    double west, east, south, north, area;
    ok = wkb && bytes <= 256 * 1048576 && GEOSGeom_getXMin_r(geos, merged, &west) &&
        GEOSGeom_getXMax_r(geos, merged, &east) && GEOSGeom_getYMin_r(geos, merged, &south) &&
        GEOSGeom_getYMax_r(geos, merged, &north) && south == -90 && GEOSArea_r(geos, merged, &area) && sql(db,
        "CREATE TABLE polar_completion(id INTEGER PRIMARY KEY,legacy_id INTEGER,cutoff REAL,original_geometry BLOB,"
        "status TEXT);UPDATE reconciliation SET status='polar_completed_requires_fresh_review';") &&
        sqlite3_prepare_v2(db, "INSERT INTO polar_completion SELECT ?1,?2,?3,geometry,'coverage_equal_requires_boundary_review' "
            "FROM simplified_rings WHERE ring_id=?1", -1, &row, NULL) == SQLITE_OK &&
        sqlite3_bind_int64(row, 1, id) == SQLITE_OK && sqlite3_bind_int64(row, 2, legacy_id) == SQLITE_OK &&
        sqlite3_bind_double(row, 3, cutoff) == SQLITE_OK && sqlite3_step(row) == SQLITE_DONE;
    sqlite3_finalize(row); row = NULL;
    ok = ok && sqlite3_prepare_v2(db, "UPDATE simplified_rings SET geometry=?2,points=?3 WHERE ring_id=?1",
        -1, &row, NULL) == SQLITE_OK && sqlite3_bind_int64(row, 1, id) == SQLITE_OK &&
        sqlite3_bind_blob(row, 2, wkb, (int)bytes, SQLITE_TRANSIENT) == SQLITE_OK &&
        sqlite3_bind_int(row, 3, GEOSGetNumCoordinates_r(geos, merged)) == SQLITE_OK && sqlite3_step(row) == SQLITE_DONE;
    sqlite3_finalize(row); row = NULL;
    char *update = ok ? sqlite3_mprintf("UPDATE ring_bounds SET west=%!.17g,east=%!.17g,south=-90,north=%!.17g WHERE id=%lld;"
        "UPDATE ring_metadata SET polar=1,ccw=1,area=%!.17g WHERE id=%lld;COMMIT", west, east, north,
        (long long)id, area, (long long)id) : NULL;
    ok = ok && update && sql(db, update);
    sqlite3_free(update); GEOSFree_r(geos, wkb);
    GEOSGeom_destroy_r(geos, merged); GEOSGeom_destroy_r(geos, legacy); GEOSGeom_destroy_r(geos, newer);
    fprintf(stderr, "Polar completion: %s; ring %" PRId64 ", legacy %" PRId64 ", cutoff %.9f. Not installed.\n",
            ok ? "PASS" : "FAIL", id, legacy_id, cutoff);
    return ok;
}

static bool self_test(void)
{
    GEOSContextHandle_t geos = GEOS_init_r();
    GEOSGeometry *newer = GEOSGeom_createRectangle_r(geos, -180, -85, 180, -65);
    GEOSGeometry *legacy = GEOSGeom_createRectangle_r(geos, -180, -90, 180, -70);
    GEOSGeometry *merged = complete_cap(geos, newer, legacy, -85);
    double south = 0, north = 0;
    bool ok = merged && GEOSGeom_getYMin_r(geos, merged, &south) && south == -90 &&
        GEOSGeom_getYMax_r(geos, merged, &north) && north == -65;
    GEOSGeometry *sloped = GEOSGeomFromWKT_r(geos,
        "POLYGON((-179.9999996 -85.0511289,-150 -85.05,180 -85.05,180 -65,-180 -65,-179.9999996 -85.0511289))");
    double cutoff = 0;
    GEOSGeometry *wrong = complete_cap(geos, sloped, legacy, -85.0511289);
    ok = ok && !wrong && coverage_cutoff(geos, sloped, &cutoff) && cutoff == -85.05;
    GEOSGeometry *fixed = ok ? complete_cap(geos, sloped, legacy, cutoff) : NULL;
    ok = ok && fixed;
    GEOSGeometry *unrecognised = GEOSGeom_createRectangle_r(geos, -20, -85.05, 20, -65);
    ok = ok && !coverage_cutoff(geos, unrecognised, &cutoff);
    GEOSGeom_destroy_r(geos, wrong); GEOSGeom_destroy_r(geos, fixed);
    GEOSGeom_destroy_r(geos, sloped); GEOSGeom_destroy_r(geos, unrecognised);
    GEOSGeometry *bad = complete_cap(geos, newer, legacy, -60);
    ok = ok && !bad;
    GEOSGeometry *northern_defect = GEOSGeomFromWKT_r(geos,
        "POLYGON((-180 -90,180 -90,180 -70,0 -65,20 -75,0 -75,20 -65,-180 -70,-180 -90))");
    const GEOSGeometry *edge = northern_defect ? GEOSGetExteriorRing_r(geos, northern_defect) : NULL;
    GEOSCoordSequence *clipped = edge ? clip_south(geos, GEOSGeom_getCoordSeq_r(geos, edge), -85) : NULL;
    GEOSGeometry *shell = clipped ? GEOSGeom_createLinearRing_r(geos, clipped) : NULL;
    GEOSGeometry *southern = shell ? GEOSGeom_createPolygon_r(geos, shell, NULL, 0) : NULL;
    double area = 0;
    ok = ok && northern_defect && GEOSisValid_r(geos, northern_defect) == 0 && southern &&
        GEOSisValid_r(geos, southern) == 1 && GEOSArea_r(geos, southern, &area) && area == 1800;
    GEOSGeom_destroy_r(geos, southern); GEOSGeom_destroy_r(geos, northern_defect);
    GEOSGeom_destroy_r(geos, bad); GEOSGeom_destroy_r(geos, merged);
    GEOSGeom_destroy_r(geos, newer); GEOSGeom_destroy_r(geos, legacy); GEOS_finish_r(geos);
    fprintf(stderr, "Polar completion self-test: %s\n", ok ? "PASS" : "FAIL");
    return ok;
}

/* Create a single complete candidate; the installed globe is never opened. */
int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "--self-test")) return self_test() ? 0 : 1;
    if (argc == 4 && !strcmp(argv[1], "--check-merge")) {
        GEOSContextHandle_t context = GEOS_init_r();
        sqlite3 *db = NULL;
        sqlite3_stmt *row = NULL;
        file_t *file = file_new_cstr(argv[3]);
        bool ok = context && file && sqlite3_open_v2(argv[2], &db, SQLITE_OPEN_READONLY, NULL) == SQLITE_OK &&
            sqlite3_prepare_v2(db, "SELECT geometry FROM simplified_rings s JOIN ring_bounds b ON b.id=s.ring_id "
                "WHERE b.west=-180 AND b.east=180 AND b.south<-85", -1, &row, NULL) == SQLITE_OK &&
            sqlite3_step(row) == SQLITE_ROW;
        GEOSGeometry *newer = ok ? GEOSGeomFromWKB_buf_r(context, sqlite3_column_blob(row, 0),
                                                        sqlite3_column_bytes(row, 0)) : NULL;
        double cutoff = 0;
        ok = newer && coverage_cutoff(context, newer, &cutoff) && sqlite3_step(row) == SQLITE_DONE;
        int64_t id = 0;
        GEOSGeometry *old = ok ? legacy_cap(context, file, &id, cutoff) : NULL;
        GEOSGeometry *merged = old ? complete_cap(context, newer, old, cutoff) : NULL;
        ok = merged != NULL;
        fprintf(stderr, "Read-only polar merge check: %s\n", ok ? "PASS" : "FAIL");
        if (context) {
            GEOSGeom_destroy_r(context, newer); GEOSGeom_destroy_r(context, old);
            GEOSGeom_destroy_r(context, merged); GEOS_finish_r(context);
        }
        sqlite3_finalize(row); if (db) sqlite3_close(db); file_free(file);
        return ok ? 0 : 1;
    }
    if (argc == 3 && !strcmp(argv[1], "--check-legacy")) {
        GEOSContextHandle_t context = GEOS_init_r();
        file_t *file = file_new_cstr(argv[2]);
        int64_t id = 0;
        GEOSGeometry *cap = context && file ? legacy_cap(context, file, &id, -85.05) : NULL;
        bool valid = cap != NULL;
        fprintf(stderr, "Legacy polar source: %s; id %" PRId64 ", %d coordinates\n",
                valid ? "PASS" : "FAIL", id, cap ? GEOSGetNumCoordinates_r(context, cap) : 0);
        if (context) { GEOSGeom_destroy_r(context, cap); GEOS_finish_r(context); }
        file_free(file);
        return valid ? 0 : 1;
    }
    if (argc != 4) { fprintf(stderr, "Usage: %s reconciled.sqlite3 gshhg.bin NEW-complete.sqlite3\n", argv[0]); return 2; }
    file_t *output = file_new_cstr(argv[3]), *legacy = file_new_cstr(argv[2]);
    sqlite3 *source = NULL, *target = NULL;
    GEOSContextHandle_t geos = GEOS_init_r();
    bool ok = output && legacy && geos && file_open(output, FILE_MODE_CREATE_NEW, FILE_ACCESS_WRITE) && file_close(output) &&
        sqlite3_open_v2(argv[1], &source, SQLITE_OPEN_READONLY, NULL) == SQLITE_OK &&
        sqlite3_open_v2(argv[3], &target, SQLITE_OPEN_READWRITE, NULL) == SQLITE_OK;
    sqlite3_backup *copy = ok ? sqlite3_backup_init(target, "main", source, "main") : NULL;
    ok = copy && sqlite3_backup_step(copy, -1) == SQLITE_DONE;
    if (copy) ok = sqlite3_backup_finish(copy) == SQLITE_OK && ok;
    if (source) sqlite3_close(source);
    ok = ok && prepare_polar(target, geos, legacy);
    if (target) sqlite3_close(target);
    if (geos) GEOS_finish_r(geos);
    file_free(output); file_free(legacy);
    return ok ? 0 : 1;
}
