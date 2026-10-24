/**
 * @file coastline_reconcile.c
 * @brief Offline source-priority reconciliation and indexed shoreline nesting.
 *
 * Works on an exclusive copy of a composed candidate. Reviewed coastline rings
 * take precedence over crossing legacy rings; redundant legacy coverage is
 * removed only after nesting establishes equal land/water classification.
 * Nested coastline land is redundant only when full-polygon coverage is proved;
 * its descendants remain active and its original geometry stays in the audit.
 * Uncontained coastline shells are classified as land independently of winding,
 * with explicit classification overrides; enclosed water retains its semantics.
 * Original geometry and metadata remain in audit tables. Coordinates are never
 * snapped, smoothed or repaired. A fresh boundary review and geographic acceptance
 * are required afterwards; this executable never opens the installed globe.
 */
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <geos_c.h>
#include <sqlite3.h>

#include "file-t.h"

typedef struct _reconcile_t {
    sqlite3 *db;
    GEOSContextHandle_t geos;
} reconcile_t;

static bool sql(sqlite3 *db, const char *text)
{
    bool ok = sqlite3_exec(db, text, NULL, NULL, NULL) == SQLITE_OK;
    if (!ok) fprintf(stderr, "Reconciliation SQL: %s\n", sqlite3_errmsg(db));
    return ok;
}

static bool scalar(sqlite3 *db, const char *text, int64_t expected)
{
    sqlite3_stmt *row = NULL;
    bool ok = sqlite3_prepare_v2(db, text, -1, &row, NULL) == SQLITE_OK &&
        sqlite3_step(row) == SQLITE_ROW && sqlite3_column_int64(row, 0) == expected;
    sqlite3_finalize(row);
    return ok;
}

static bool done(sqlite3_stmt *stmt)
{
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_reset(stmt);
    sqlite3_clear_bindings(stmt);
    return ok;
}

static bool initialise(reconcile_t *state)
{
    sqlite3 *db = state->db;
    bool ok = scalar(db, "SELECT count(*) FROM composition WHERE status="
        "'composed_requires_boundary_and_nesting_review'", 1) &&
        scalar(db, "SELECT count(*) FROM invalid_rings", 0) &&
        scalar(db, "SELECT count(*) FROM review.review WHERE status='boundary_contacts_found'", 1) &&
        scalar(db, "SELECT count(*) FROM review.invalid_rings", 0) &&
        scalar(db, "SELECT count(*) FROM review.totals WHERE rings=(SELECT count(*) FROM rings) "
                   "AND points=(SELECT sum(points) FROM simplified_rings) "
                   "AND contacts=(SELECT count(*) FROM review.contacts)", 1) &&
        /* Reject missing IDs and same-source conflicts; never use this policy for coastline repairs. */
        scalar(db, "SELECT count(*) FROM review.contacts c LEFT JOIN ring_metadata a ON a.id=c.first_ring "
                   "LEFT JOIN ring_metadata b ON b.id=c.second_ring WHERE a.id IS NULL OR b.id IS NULL OR "
                   "NOT ((a.source='GSHHG inland' AND b.source='reviewed coastline') OR "
                   "(b.source='GSHHG inland' AND a.source='reviewed coastline'))", 0) && sql(db,
        "PRAGMA cache_size=-16384;PRAGMA temp_store=FILE;"
        "CREATE TABLE reconciliation(status TEXT);INSERT INTO reconciliation VALUES('incomplete');"
        "CREATE TABLE original_metadata AS SELECT * FROM ring_metadata;"
        "CREATE TABLE superseded_geometry(ring_id INTEGER PRIMARY KEY,geometry BLOB NOT NULL);"
        "CREATE TABLE decisions(id INTEGER PRIMARY KEY,reason TEXT NOT NULL,replacement INTEGER);"
        "CREATE TABLE coverage_proofs(id INTEGER PRIMARY KEY,parent INTEGER NOT NULL,longitude_shift INTEGER NOT NULL,"
        "relation TEXT NOT NULL);"
        "CREATE TABLE source_replacements(legacy_id INTEGER,coast_id INTEGER,PRIMARY KEY(legacy_id,coast_id));"
        "INSERT INTO source_replacements SELECT DISTINCT CASE WHEN a.source='GSHHG inland' THEN a.id ELSE b.id END,"
        "CASE WHEN a.source='reviewed coastline' THEN a.id ELSE b.id END FROM review.contacts c "
        "JOIN ring_metadata a ON a.id=c.first_ring JOIN ring_metadata b ON b.id=c.second_ring;"
        "CREATE TABLE nesting(id INTEGER PRIMARY KEY,parent INTEGER NOT NULL DEFAULT 0,"
        "parent_area REAL NOT NULL DEFAULT 1e100,land INTEGER NOT NULL,coast INTEGER NOT NULL,"
        "x REAL,y REAL,depth INTEGER NOT NULL DEFAULT 0);"
        "INSERT INTO decisions SELECT DISTINCT m.id,'crossing legacy boundary replaced by coastline',NULL "
        "FROM ring_metadata m JOIN (SELECT first_ring AS id FROM review.contacts UNION SELECT second_ring "
        "FROM review.contacts) c ON c.id=m.id WHERE m.source='GSHHG inland';"
        "INSERT INTO nesting(id,land,coast) SELECT id,CASE WHEN source='reviewed coastline' THEN ccw ELSE level%2 END,"
        "source='reviewed coastline' FROM ring_metadata WHERE id NOT IN(SELECT id FROM decisions);"
        "CREATE INDEX nesting_parent ON nesting(parent);BEGIN");
    sqlite3_stmt *rows = NULL, *save = NULL;
    ok = ok && sqlite3_prepare_v2(db, "SELECT n.id,s.geometry FROM nesting n JOIN simplified_rings s ON s.ring_id=n.id",
        -1, &rows, NULL) == SQLITE_OK && sqlite3_prepare_v2(db, "UPDATE nesting SET x=?2,y=?3 WHERE id=?1",
        -1, &save, NULL) == SQLITE_OK;
    int step = SQLITE_DONE;
    while (ok && (step = sqlite3_step(rows)) == SQLITE_ROW) {
        GEOSGeometry *polygon = GEOSGeomFromWKB_buf_r(state->geos, sqlite3_column_blob(rows, 1),
                                                     sqlite3_column_bytes(rows, 1));
        GEOSGeometry *sample = polygon ? GEOSPointOnSurface_r(state->geos, polygon) : NULL;
        double x, y;
        ok = sample && GEOSGeomGetX_r(state->geos, sample, &x) && GEOSGeomGetY_r(state->geos, sample, &y) &&
            sqlite3_bind_int64(save, 1, sqlite3_column_int64(rows, 0)) == SQLITE_OK &&
            sqlite3_bind_double(save, 2, x) == SQLITE_OK && sqlite3_bind_double(save, 3, y) == SQLITE_OK && done(save);
        GEOSGeom_destroy_r(state->geos, sample); GEOSGeom_destroy_r(state->geos, polygon);
    }
    sqlite3_finalize(rows); sqlite3_finalize(save);
    return ok && step == SQLITE_DONE && sql(db, "COMMIT");
}

static bool find_parents(reconcile_t *state)
{
    sqlite3_stmt *parents = NULL, *children = NULL, *save = NULL;
    bool ok = sql(state->db, "BEGIN") && sqlite3_prepare_v2(state->db,
        "SELECT n.id,m.area,b.west,b.east,b.south,b.north,s.geometry FROM nesting n "
        "JOIN ring_metadata m ON m.id=n.id JOIN ring_bounds b ON b.id=n.id "
        "JOIN simplified_rings s ON s.ring_id=n.id ORDER BY m.area DESC,n.id",
        -1, &parents, NULL) == SQLITE_OK && sqlite3_prepare_v2(state->db,
        "SELECT n.id,n.x,n.y FROM ring_bounds b CROSS JOIN nesting n ON n.id=b.id "
        "JOIN ring_metadata m ON m.id=n.id WHERE b.west>=?1 AND b.east<=?2 AND b.south>=?3 AND b.north<=?4 "
        "AND m.area<?5 AND n.parent_area>?5 AND n.id!=?6",
        -1, &children, NULL) == SQLITE_OK && sqlite3_prepare_v2(state->db,
        "UPDATE nesting SET parent=?2,parent_area=?3 WHERE id=?1", -1, &save, NULL) == SQLITE_OK;
    int step = SQLITE_DONE;
    int64_t count = 0;
    while (ok && (step = sqlite3_step(parents)) == SQLITE_ROW) {
        GEOSGeometry *polygon = NULL;
        const GEOSPreparedGeometry *prepared = NULL;
        int64_t parent = sqlite3_column_int64(parents, 0);
        double area = sqlite3_column_double(parents, 1);
        for (int shift = -360; ok && shift <= 360; shift += 360) {
            ok = sqlite3_bind_double(children, 1, sqlite3_column_double(parents, 2) - shift) == SQLITE_OK &&
                sqlite3_bind_double(children, 2, sqlite3_column_double(parents, 3) - shift) == SQLITE_OK &&
                sqlite3_bind_double(children, 3, sqlite3_column_double(parents, 4)) == SQLITE_OK &&
                sqlite3_bind_double(children, 4, sqlite3_column_double(parents, 5)) == SQLITE_OK &&
                sqlite3_bind_double(children, 5, area) == SQLITE_OK && sqlite3_bind_int64(children, 6, parent) == SQLITE_OK;
            int child_step = SQLITE_DONE;
            while (ok && (child_step = sqlite3_step(children)) == SQLITE_ROW) {
                if (!polygon) {
                    polygon = GEOSGeomFromWKB_buf_r(state->geos, sqlite3_column_blob(parents, 6),
                                                   sqlite3_column_bytes(parents, 6));
                    prepared = polygon ? GEOSPrepare_r(state->geos, polygon) : NULL;
                }
                GEOSGeometry *point = GEOSGeom_createPointFromXY_r(state->geos,
                    sqlite3_column_double(children, 1) + shift, sqlite3_column_double(children, 2));
                char contains = prepared && point ? GEOSPreparedContains_r(state->geos, prepared, point) : 2;
                GEOSGeom_destroy_r(state->geos, point);
                ok = contains != 2;
                if (ok && contains) ok = sqlite3_bind_int64(save, 1, sqlite3_column_int64(children, 0)) == SQLITE_OK &&
                    sqlite3_bind_int64(save, 2, parent) == SQLITE_OK &&
                    sqlite3_bind_double(save, 3, area) == SQLITE_OK && done(save);
            }
            ok = ok && child_step == SQLITE_DONE;
            sqlite3_reset(children);
        }
        GEOSPreparedGeom_destroy_r(state->geos, prepared); GEOSGeom_destroy_r(state->geos, polygon);
        if (++count % 10000 == 0) fprintf(stderr, "Nesting: %" PRId64 " candidate parents checked\n", count);
    }
    sqlite3_finalize(parents); sqlite3_finalize(children); sqlite3_finalize(save);
    return ok && step == SQLITE_DONE && sql(state->db, "COMMIT");
}

static int shift_longitude(double *x, double *y, void *context)
{
    (void)y;
    *x += *(const int *)context;
    return isfinite(*x);
}

/* Full containment, unlike representative-point nesting, proves unchanged union coverage. */
static bool covered_coast(reconcile_t *state, int64_t child, int64_t parent, int *longitude_shift)
{
    sqlite3_stmt *row = NULL;
    bool ok = sqlite3_prepare_v2(state->db,
        "SELECT a.geometry,b.geometry FROM simplified_rings a JOIN simplified_rings b ON b.ring_id=?2 "
        "WHERE a.ring_id=?1 AND a.tolerance=8 AND b.tolerance=8", -1, &row, NULL) == SQLITE_OK &&
        sqlite3_bind_int64(row, 1, parent) == SQLITE_OK && sqlite3_bind_int64(row, 2, child) == SQLITE_OK &&
        sqlite3_step(row) == SQLITE_ROW;
    GEOSGeometry *outer = ok ? GEOSGeomFromWKB_buf_r(state->geos, sqlite3_column_blob(row, 0),
                                                    sqlite3_column_bytes(row, 0)) : NULL;
    GEOSGeometry *inner = ok ? GEOSGeomFromWKB_buf_r(state->geos, sqlite3_column_blob(row, 1),
                                                    sqlite3_column_bytes(row, 1)) : NULL;
    sqlite3_finalize(row);
    const GEOSPreparedGeometry *prepared = outer ? GEOSPrepare_r(state->geos, outer) : NULL;
    ok = prepared && inner && GEOSGeomTypeId_r(state->geos, outer) == GEOS_POLYGON &&
         GEOSGeomTypeId_r(state->geos, inner) == GEOS_POLYGON;
    bool covered = false;
    for (int shift = -360; ok && !covered && shift <= 360; shift += 360) {
        GEOSGeometry *moved = GEOSGeom_transformXY_r(state->geos, inner, shift_longitude, &shift);
        char covers = moved ? GEOSPreparedCovers_r(state->geos, prepared, moved) : 2;
        ok = covers != 2;
        covered = covers == 1;
        if (covered) *longitude_shift = shift;
        GEOSGeom_destroy_r(state->geos, moved);
    }
    GEOSPreparedGeom_destroy_r(state->geos, prepared);
    GEOSGeom_destroy_r(state->geos, inner); GEOSGeom_destroy_r(state->geos, outer);
    return ok && covered;
}

static bool resolve_levels(reconcile_t *state)
{
    sqlite3 *db = state->db;
    /* Remove only redundant coverage, reconnecting descendants by the indexed parent key. */
    sqlite3_stmt *conflict = NULL, *record = NULL, *reparent = NULL, *remove = NULL, *queue = NULL, *pop = NULL, *proof = NULL;
    /* An outer coastline is a land shell, not an inland-water hole, regardless of winding. */
    bool ok = sql(db, "BEGIN;CREATE TABLE classification_overrides(id INTEGER PRIMARY KEY,old_land INTEGER,"
        "new_land INTEGER,reason TEXT NOT NULL);"
        "INSERT INTO classification_overrides SELECT id,land,1,'uncontained coastline shell; winding retained' "
        "FROM nesting WHERE coast=1 AND parent=0 AND land=0;"
        "UPDATE nesting SET land=1 WHERE id IN(SELECT id FROM classification_overrides);"
        "CREATE TEMP TABLE pending_conflicts(id INTEGER PRIMARY KEY);"
        "INSERT INTO pending_conflicts SELECT n.id FROM nesting n LEFT JOIN nesting p ON p.id=n.parent "
        "WHERE n.land=coalesce(p.land,0)") && sqlite3_prepare_v2(db,
        "SELECT n.id,n.parent,n.coast,coalesce(p.coast,1),coalesce(p.parent,0),n.land=coalesce(p.land,0),n.land "
        "FROM pending_conflicts q CROSS JOIN nesting n ON n.id=q.id LEFT JOIN nesting p ON p.id=n.parent "
        "ORDER BY q.id LIMIT 1", -1, &conflict, NULL) == SQLITE_OK && sqlite3_prepare_v2(db,
        "INSERT INTO decisions VALUES(?1,?3,?2)", -1, &record, NULL) == SQLITE_OK &&
        sqlite3_prepare_v2(db, "UPDATE nesting SET parent=?2 WHERE parent=?1", -1, &reparent, NULL) == SQLITE_OK &&
        sqlite3_prepare_v2(db, "DELETE FROM nesting WHERE id=?1", -1, &remove, NULL) == SQLITE_OK &&
        sqlite3_prepare_v2(db, "INSERT OR IGNORE INTO pending_conflicts SELECT id FROM nesting WHERE parent=?1",
                           -1, &queue, NULL) == SQLITE_OK &&
        sqlite3_prepare_v2(db, "DELETE FROM pending_conflicts WHERE id=?1 OR id=?2", -1, &pop, NULL) == SQLITE_OK &&
        sqlite3_prepare_v2(db, "INSERT INTO coverage_proofs VALUES(?1,?2,?3,'parent covers entire child polygon')",
                           -1, &proof, NULL) == SQLITE_OK;
    int step = SQLITE_DONE;
    while (ok && (step = sqlite3_step(conflict)) == SQLITE_ROW) {
        int64_t id = sqlite3_column_int64(conflict, 0), parent = sqlite3_column_int64(conflict, 1);
        bool coast = sqlite3_column_int(conflict, 2), parent_coast = sqlite3_column_int(conflict, 3);
        int64_t removed = coast ? parent : id, replacement = coast ? id : parent;
        int64_t ancestor = coast ? sqlite3_column_int64(conflict, 4) : parent;
        bool same = sqlite3_column_int(conflict, 5);
        bool land = sqlite3_column_int(conflict, 6);
        const char *reason = "redundant legacy land/water coverage";
        sqlite3_reset(conflict);
        if (!same) {
            ok = sqlite3_bind_int64(pop, 1, id) == SQLITE_OK && sqlite3_bind_int64(pop, 2, id) == SQLITE_OK && done(pop);
            continue;
        }
        if (coast && parent_coast) {
            int shift = 0;
            ok = parent != 0 && land && covered_coast(state, id, parent, &shift);
            if (ok) {
                removed = id; replacement = parent; ancestor = parent;
                reason = "redundant coastline land; full-polygon coverage proved";
                ok = sqlite3_bind_int64(proof, 1, id) == SQLITE_OK &&
                    sqlite3_bind_int64(proof, 2, parent) == SQLITE_OK &&
                    sqlite3_bind_int(proof, 3, shift) == SQLITE_OK && done(proof);
                fprintf(stderr, "Covered land ring %" PRId64 " retained in audit; enclosing ring %" PRId64 "\n", id, parent);
            } else fprintf(stderr, "Unresolved coastline orientation/nesting: ring %" PRId64 ", parent %" PRId64 "\n", id, parent);
        }
        ok = ok && sqlite3_bind_int64(record, 1, removed) == SQLITE_OK &&
            sqlite3_bind_text(record, 3, reason, -1, SQLITE_STATIC) == SQLITE_OK &&
            sqlite3_bind_int64(record, 2, replacement) == SQLITE_OK && done(record) &&
            sqlite3_bind_int64(pop, 1, id) == SQLITE_OK && sqlite3_bind_int64(pop, 2, removed) == SQLITE_OK && done(pop) &&
            sqlite3_bind_int64(queue, 1, removed) == SQLITE_OK && done(queue) &&
            sqlite3_bind_int64(reparent, 1, removed) == SQLITE_OK &&
            sqlite3_bind_int64(reparent, 2, ancestor) == SQLITE_OK && done(reparent) &&
            sqlite3_bind_int64(remove, 1, removed) == SQLITE_OK && done(remove);
    }
    sqlite3_finalize(conflict); sqlite3_finalize(record); sqlite3_finalize(reparent); sqlite3_finalize(remove);
    sqlite3_finalize(queue); sqlite3_finalize(pop); sqlite3_finalize(proof);
    ok = ok && step == SQLITE_DONE && sql(db,
        "CREATE TEMP TABLE resolved_depths(id INTEGER PRIMARY KEY,depth INTEGER);"
        "WITH RECURSIVE depths(id,depth) AS (SELECT id,1 FROM nesting WHERE parent=0 UNION ALL "
        "SELECT n.id,d.depth+1 FROM nesting n JOIN depths d ON n.parent=d.id WHERE d.depth<64) "
        "INSERT INTO resolved_depths SELECT * FROM depths;"
        "UPDATE nesting SET depth=coalesce((SELECT depth FROM resolved_depths d WHERE d.id=nesting.id),0);") &&
        scalar(db, "SELECT count(*) FROM nesting WHERE depth NOT BETWEEN 1 AND 4 OR depth%2!=land", 0) && sql(db,
        "INSERT INTO superseded_geometry SELECT s.ring_id,s.geometry FROM simplified_rings s JOIN decisions d ON d.id=s.ring_id;"
        "UPDATE ring_metadata SET level=(SELECT depth FROM nesting n WHERE n.id=ring_metadata.id) "
        "WHERE id IN(SELECT id FROM nesting);"
        "DELETE FROM simplified_rings WHERE ring_id IN(SELECT id FROM decisions);"
        "DELETE FROM rings WHERE id IN(SELECT id FROM decisions);"
        "DELETE FROM ring_bounds WHERE id IN(SELECT id FROM decisions);"
        "DELETE FROM ring_metadata WHERE id IN(SELECT id FROM decisions);"
        "UPDATE reconciliation SET status='reconciled_requires_fresh_boundary_and_geographic_review';COMMIT");
    return ok;
}

static bool reconcile(reconcile_t *state)
{
    bool ok = initialise(state) && find_parents(state) && resolve_levels(state);
    sqlite3_stmt *summary = NULL;
    if (ok && sqlite3_prepare_v2(state->db,
        "SELECT (SELECT count(*) FROM rings),(SELECT count(*) FROM decisions),"
        "(SELECT count(*) FROM ring_metadata m JOIN original_metadata o ON o.id=m.id WHERE m.level!=o.level),"
        "(SELECT count(*) FROM classification_overrides)",
        -1, &summary, NULL) == SQLITE_OK && sqlite3_step(summary) == SQLITE_ROW)
        fprintf(stderr, "Reconciled candidate: %" PRId64 " retained rings, %" PRId64 " audited replacements, %" PRId64
                        " nesting-level changes, %" PRId64 " outer-shell classifications\n",
                (int64_t)sqlite3_column_int64(summary, 0), (int64_t)sqlite3_column_int64(summary, 1),
                (int64_t)sqlite3_column_int64(summary, 2), (int64_t)sqlite3_column_int64(summary, 3));
    sqlite3_finalize(summary);
    fprintf(stderr, "Coastline source reconciliation: %s; not installed or geographically approved.\n", ok ? "PASS" : "FAIL");
    return ok;
}

static bool fixture_ring(reconcile_t *state, int id, unsigned level, bool coast, const char *wkt)
{
    GEOSGeometry *polygon = GEOSGeomFromWKT_r(state->geos, wkt);
    double west, east, south, north, area;
    char ccw;
    bool ok = polygon && GEOSGeom_getXMin_r(state->geos, polygon, &west) &&
        GEOSGeom_getXMax_r(state->geos, polygon, &east) && GEOSGeom_getYMin_r(state->geos, polygon, &south) &&
        GEOSGeom_getYMax_r(state->geos, polygon, &north) && GEOSArea_r(state->geos, polygon, &area) &&
        GEOSCoordSeq_isCCW_r(state->geos, GEOSGeom_getCoordSeq_r(state->geos,
            GEOSGetExteriorRing_r(state->geos, polygon)), &ccw);
    size_t bytes = 0;
    unsigned char *wkb = ok ? GEOSGeomToWKB_buf_r(state->geos, polygon, &bytes) : NULL;
    char *command = ok ? sqlite3_mprintf(
        "INSERT INTO rings VALUES(%d);INSERT INTO ring_metadata VALUES(%d,%u,'%s',%d,%d,%!.17g,0);"
        "INSERT INTO ring_bounds VALUES(%d,%!.17g,%!.17g,%!.17g,%!.17g)", id, id, level,
        coast ? "reviewed coastline" : "GSHHG inland", id, ccw, area, id, west, east, south, north) : NULL;
    sqlite3_stmt *save = NULL;
    ok = ok && wkb && command && sql(state->db, command) && sqlite3_prepare_v2(state->db,
        "INSERT INTO simplified_rings VALUES(?1,8,?2,?3)", -1, &save, NULL) == SQLITE_OK &&
        sqlite3_bind_int(save, 1, id) == SQLITE_OK &&
        sqlite3_bind_int(save, 2, GEOSGetNumCoordinates_r(state->geos, polygon)) == SQLITE_OK &&
        sqlite3_bind_blob(save, 3, wkb, (int)bytes, SQLITE_TRANSIENT) == SQLITE_OK && done(save);
    sqlite3_finalize(save); sqlite3_free(command); GEOSFree_r(state->geos, wkb);
    GEOSGeom_destroy_r(state->geos, polygon);
    return ok;
}

static bool fixture(bool legacy_encloses_coast, bool bad_review, bool bad_orientation, bool clockwise_outer)
{
    reconcile_t state = {0};
    state.geos = GEOS_init_r();
    bool ok = state.geos && sqlite3_open(":memory:", &state.db) == SQLITE_OK && sql(state.db,
        "CREATE TABLE composition(status);INSERT INTO composition VALUES('composed_requires_boundary_and_nesting_review');"
        "CREATE TABLE invalid_rings(id);CREATE TABLE rings(id INTEGER PRIMARY KEY);"
        "CREATE TABLE ring_metadata(id INTEGER PRIMARY KEY,level,source,source_id,ccw,area,polar);"
        "CREATE TABLE simplified_rings(ring_id INTEGER PRIMARY KEY,tolerance,points,geometry);"
        "CREATE VIRTUAL TABLE ring_bounds USING rtree(id,west,east,south,north);"
        "ATTACH ':memory:' AS review;CREATE TABLE review.review(status);"
        "INSERT INTO review.review VALUES('boundary_contacts_found');CREATE TABLE review.invalid_rings(id);"
        "CREATE TABLE review.contacts(first_ring,second_ring);INSERT INTO review.contacts VALUES(4,2);"
        "CREATE TABLE review.totals(rings,points,contacts)") &&
        fixture_ring(&state, 1, 1, true, "POLYGON((0 0,20 0,20 20,0 20,0 0))") &&
        fixture_ring(&state, 2, 1, true, "POLYGON((2 2,2 18,18 18,18 2,2 2))") &&
        fixture_ring(&state, 3, 1, true, "POLYGON((3 3,5 3,5 5,3 5,3 3))") &&
        fixture_ring(&state, 4, 2, false, "POLYGON((1 3,1 19,19 19,19 3,1 3))") &&
        fixture_ring(&state, 5, 3, false, "POLYGON((10 10,12 10,12 12,10 12,10 10))") &&
        fixture_ring(&state, 6, 4, false, "POLYGON((10.2 10.2,10.2 10.5,10.5 10.5,10.5 10.2,10.2 10.2))") &&
        fixture_ring(&state, 7, 2, false, "POLYGON((3.2 3.2,3.2 4.8,4.8 4.8,4.8 3.2,3.2 3.2))") &&
        fixture_ring(&state, 8, 3, false, legacy_encloses_coast ?
            "POLYGON((2.5 2.5,5.5 2.5,5.5 5.5,2.5 5.5,2.5 2.5))" :
            "POLYGON((3.1 3.1,4.9 3.1,4.9 4.9,3.1 4.9,3.1 3.1))") &&
        fixture_ring(&state, 9, 2, false, "POLYGON((30 30,30 31,31 31,31 30,30 30))") &&
        /* Opposite longitude representations must still nest. */
        fixture_ring(&state, 10, 1, true, clockwise_outer ? "POLYGON((-10 0,-10 5,-5 5,-5 0,-10 0))" :
                      "POLYGON((-10 0,-5 0,-5 5,-10 5,-10 0))") &&
        fixture_ring(&state, 11, 2, false, "POLYGON((351 1,351 2,352 2,352 1,351 1))") &&
        fixture_ring(&state, 12, 1, true, "POLYGON((-7.8 1,-6 1,-6 4,-7.8 4,-7.8 1))") &&
        fixture_ring(&state, 13, 1, true, "POLYGON((-7.7 1.1,-7.7 3.9,-6.1 3.9,-6.1 1.1,-7.7 1.1))") &&
        fixture_ring(&state, 14, 1, true, "POLYGON((-7.5 2,-7 2,-7 3,-7.5 3,-7.5 2))") && sql(state.db,
        "INSERT INTO review.totals SELECT count(*),sum(points),1 FROM simplified_rings;"
        "CREATE TABLE fixture_original AS SELECT * FROM simplified_rings");
    if (bad_review) ok = ok && sql(state.db, "UPDATE review.contacts SET first_ring=1");
    if (bad_orientation) ok = ok && sql(state.db, "UPDATE ring_metadata SET ccw=0 WHERE id=3");
    int shift = 0;
    ok = ok && covered_coast(&state, 11, 10, &shift) && shift == -360 &&
        !covered_coast(&state, 10, 1, &shift) && !covered_coast(&state, 4, 2, &shift);
    bool expected = !bad_review && !bad_orientation;
    if (ok) ok = reconcile(&state) == expected;
    if (ok && expected) ok = scalar(state.db, "SELECT count(*) FROM rings", 10) &&
        scalar(state.db, "SELECT count(*) FROM classification_overrides WHERE id=10", clockwise_outer ? 1 : 0) &&
        scalar(state.db, "SELECT ccw FROM ring_metadata WHERE id=10", clockwise_outer ? 0 : 1) &&
        scalar(state.db, "SELECT level FROM ring_metadata WHERE id=10", 1) &&
        scalar(state.db, "SELECT count(*) FROM decisions WHERE id IN(4,8,9)", 3) &&
        scalar(state.db, "SELECT parent FROM coverage_proofs WHERE id=12", 10) &&
        scalar(state.db, "SELECT parent FROM nesting WHERE id=13", 10) &&
        scalar(state.db, "SELECT level FROM ring_metadata WHERE id=13", 2) &&
        scalar(state.db, "SELECT level FROM ring_metadata WHERE id=14", 3) &&
        scalar(state.db, "SELECT level FROM ring_metadata WHERE id=2", 2) &&
        scalar(state.db, "SELECT level FROM ring_metadata WHERE id=3", 3) &&
        scalar(state.db, "SELECT level FROM ring_metadata WHERE id=7", 4) &&
        scalar(state.db, "SELECT parent FROM nesting WHERE id=11", 10) &&
        scalar(state.db, "SELECT count(*) FROM simplified_rings s JOIN fixture_original f ON f.ring_id=s.ring_id "
                        "WHERE s.geometry!=f.geometry", 0) &&
        scalar(state.db, "SELECT count(*) FROM superseded_geometry s JOIN fixture_original f ON f.ring_id=s.ring_id "
                        "WHERE s.geometry=f.geometry", 4);
    if (state.geos) GEOS_finish_r(state.geos);
    if (state.db) sqlite3_close(state.db);
    return ok;
}

static bool self_test(void)
{
    bool ok = fixture(false, false, false, false) && fixture(true, false, false, false) &&
              fixture(false, false, false, true) && fixture(false, true, false, false) &&
              fixture(false, false, true, false);
    fprintf(stderr, "Coastline reconciliation self-test: %s\n", ok ? "PASS" : "FAIL");
    return ok;
}

/* Preserve the input candidate and every removed geometry in a separate offline artifact. */
int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "--self-test")) return self_test() ? 0 : 1;
    if (argc != 4) {
        fprintf(stderr, "Usage: %s composed.sqlite3 boundary-review.sqlite3 NEW-reconciled.sqlite3\n", argv[0]);
        return 2;
    }
    file_t *output = file_new_cstr(argv[3]);
    sqlite3 *source = NULL;
    reconcile_t state = {0};
    bool ok = output && file_open(output, FILE_MODE_CREATE_NEW, FILE_ACCESS_WRITE) && file_close(output) &&
        sqlite3_open_v2(argv[1], &source, SQLITE_OPEN_READONLY, NULL) == SQLITE_OK &&
        sqlite3_open_v2(argv[3], &state.db, SQLITE_OPEN_READWRITE, NULL) == SQLITE_OK;
    sqlite3_backup *copy = ok ? sqlite3_backup_init(state.db, "main", source, "main") : NULL;
    ok = copy && sqlite3_backup_step(copy, -1) == SQLITE_DONE;
    if (copy) ok = sqlite3_backup_finish(copy) == SQLITE_OK && ok;
    if (source) sqlite3_close(source);
    sqlite3_stmt *attach = NULL;
    char *uri = strchr(argv[2], '?') || strchr(argv[2], '#') ? NULL : sqlite3_mprintf("file:%s?mode=ro", argv[2]);
    ok = ok && uri && sqlite3_prepare_v2(state.db, "ATTACH DATABASE ?1 AS review", -1, &attach, NULL) == SQLITE_OK &&
        sqlite3_bind_text(attach, 1, uri, -1, SQLITE_TRANSIENT) == SQLITE_OK && sqlite3_step(attach) == SQLITE_DONE;
    sqlite3_finalize(attach); sqlite3_free(uri);
    state.geos = GEOS_init_r();
    ok = ok && state.geos && reconcile(&state);
    if (state.geos) GEOS_finish_r(state.geos);
    if (state.db) sqlite3_close(state.db);
    file_free(output);
    return ok ? 0 : 1;
}
