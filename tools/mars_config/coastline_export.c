/**
 * @file coastline_export.c
 * @brief Offline lossless export of exactly the coastline geometry accepted by a boundary review.
 *
 * Streams effective staging polygons and the review's ordered boundary chunks together,
 * rejecting stale or incomplete reviews without quadratic ring lookups. Writes an exclusive
 * MARSCF64 candidate using binary64 coordinates, never repairing or simplifying geometry.
 * This coastline-only intermediate still requires inland-water composition and geographic
 * acceptance before packaging. It never opens the installed globe database.
 */
#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sqlite3.h>

#include "file-t.h"
#include "globe-t.h"

_Static_assert(sizeof(double) == 8 && DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024, "IEEE binary64 required");

typedef struct _export_ring_t {
    const unsigned char *coordinates;
    unsigned count;
    bool little;
    double west, east, south, north;
} export_ring_t;

static uint64_t read_integer(const unsigned char *p, unsigned width, bool little)
{
    uint64_t value = 0;
    for (unsigned i = 0; i < width; ++i) value = (value << 8) | p[little ? width - i - 1 : i];
    return value;
}

static void write_integer(unsigned char *p, uint64_t value, unsigned width)
{
    for (unsigned i = 0; i < width; ++i) p[i] = (unsigned char)(value >> ((width - i - 1) * 8));
}

static double read_coordinate(const unsigned char *p, bool little)
{
    uint64_t bits = read_integer(p, 8, little);
    double value;
    memcpy(&value, &bits, sizeof value);
    return value;
}

static bool parse_ring(const unsigned char *wkb, size_t bytes, int64_t expected, export_ring_t *ring)
{
    if (!wkb || bytes < 13 || wkb[0] > 1 || expected < 4 || expected > 8000000) return false;
    bool little = wkb[0] == 1;
    if (read_integer(wkb + 1, 4, little) != 3 || read_integer(wkb + 5, 4, little) != 1 ||
        read_integer(wkb + 9, 4, little) != (uint64_t)expected || bytes != 13 + (size_t)expected * 16) return false;
    *ring = (export_ring_t){.coordinates = wkb + 13, .count = (unsigned)expected, .little = little,
                           .west = INFINITY, .east = -INFINITY, .south = INFINITY, .north = -INFINITY};
    for (unsigned i = 0; i < ring->count; ++i) {
        double x = read_coordinate(ring->coordinates + (size_t)i * 16, little);
        double y = read_coordinate(ring->coordinates + (size_t)i * 16 + 8, little);
        if (!isfinite(x) || !isfinite(y) || x < -180 || x > 180 || y < -90 || y > 90) return false;
        ring->west = fmin(ring->west, x); ring->east = fmax(ring->east, x);
        ring->south = fmin(ring->south, y); ring->north = fmax(ring->north, y);
    }
    return !memcmp(ring->coordinates, ring->coordinates + (size_t)(ring->count - 1) * 16, 16);
}

static bool same_chunk(const export_ring_t *ring, unsigned first, unsigned count, double shift,
                       const unsigned char *wkb, size_t bytes)
{
    if (!wkb || bytes != 9 + (size_t)count * 16 || wkb[0] > 1) return false;
    bool little = wkb[0] == 1;
    if (read_integer(wkb + 1, 4, little) != 2 || read_integer(wkb + 5, 4, little) != count) return false;
    for (unsigned i = 0; i < count; ++i) {
        const unsigned char *original = ring->coordinates + (size_t)(first + i) * 16;
        double x = read_coordinate(original, ring->little) + shift;
        double y = read_coordinate(original + 8, ring->little);
        if (read_coordinate(wkb + 9 + (size_t)i * 16, little) != x ||
            read_coordinate(wkb + 17 + (size_t)i * 16, little) != y) return false;
    }
    return true;
}

static bool reviewed_ring(sqlite3_stmt *chunks, const export_ring_t *ring, int64_t id, int64_t *checked)
{
    for (unsigned first = 0; first + 1 < ring->count; first += 255) {
        unsigned count = ring->count - first < 256 ? ring->count - first : 256;
        double west = INFINITY, east = -INFINITY;
        for (unsigned i = 0; i < count; ++i) {
            double x = read_coordinate(ring->coordinates + (size_t)(first + i) * 16, ring->little);
            west = fmin(west, x); east = fmax(east, x);
        }
        for (int shift = -360; shift <= 360; shift += 360) {
            if (east + shift < -180 || west + shift > 180) continue;
            if (sqlite3_step(chunks) != SQLITE_ROW || sqlite3_column_int64(chunks, 0) != id ||
                !same_chunk(ring, first, count, shift, sqlite3_column_blob(chunks, 1),
                            (size_t)sqlite3_column_bytes(chunks, 1))) return false;
            ++*checked;
        }
    }
    return true;
}

static bool write_bytes(file_t *file, const unsigned char *bytes, size_t count)
{
    size_t written = 0;
    return file_write(file, bytes, count, &written) && written == count;
}

static bool write_ring(file_t *output, const export_ring_t *ring, unsigned id)
{
    unsigned char header[44] = {0}, buffer[16384];
    write_integer(header, id, 4); write_integer(header + 4, ring->count, 4);
    write_integer(header + 8, 1 | (15 << 8), 4);
    write_integer(header + 12, (uint32_t)(int32_t)floor(ring->west * 1000000), 4);
    write_integer(header + 16, (uint32_t)(int32_t)ceil(ring->east * 1000000), 4);
    write_integer(header + 20, (uint32_t)(int32_t)floor(ring->south * 1000000), 4);
    write_integer(header + 24, (uint32_t)(int32_t)ceil(ring->north * 1000000), 4);
    write_integer(header + 36, UINT32_MAX, 4); write_integer(header + 40, UINT32_MAX, 4);
    if (!write_bytes(output, header, sizeof header)) return false;
    for (unsigned first = 0; first < ring->count; first += 1024) {
        unsigned count = ring->count - first < 1024 ? ring->count - first : 1024;
        for (unsigned i = 0; i < count * 2; ++i)
            write_integer(buffer + i * 8,
                read_integer(ring->coordinates + (size_t)first * 16 + i * 8, 8, ring->little), 8);
        if (!write_bytes(output, buffer, (size_t)count * 16)) return false;
    }
    return true;
}

static bool attach(sqlite3 *db, const char *path, const char *sql)
{
    sqlite3_stmt *stmt = NULL;
    bool ok = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK &&
        sqlite3_bind_text(stmt, 1, path, -1, SQLITE_TRANSIENT) == SQLITE_OK && sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

static bool export_coast(sqlite3 *db, file_t *output)
{
    sqlite3_stmt *totals = NULL, *rows = NULL, *chunks = NULL;
    bool ok = sqlite3_exec(db, "PRAGMA cache_size=-8192;PRAGMA review.cache_size=-8192;BEGIN", NULL, NULL, NULL) == SQLITE_OK &&
        sqlite3_prepare_v2(db,
        "SELECT rings,points,chunks FROM review.totals WHERE contacts=0 "
        "AND (SELECT count(*) FROM review.review)=1 "
        "AND EXISTS(SELECT 1 FROM review.review WHERE status='boundary_contacts_clear_not_install_approved') "
        "AND NOT EXISTS(SELECT 1 FROM review.contacts) AND NOT EXISTS(SELECT 1 FROM review.invalid_rings)",
        -1, &totals, NULL) == SQLITE_OK && sqlite3_step(totals) == SQLITE_ROW;
    int64_t expected_rings = ok ? sqlite3_column_int64(totals, 0) : 0;
    int64_t expected_points = ok ? sqlite3_column_int64(totals, 1) : 0;
    int64_t expected_chunks = ok ? sqlite3_column_int64(totals, 2) : 0;
    ok = ok && expected_rings > 0 && expected_rings <= 1000000 && expected_points > 0 &&
         expected_points <= 64000000 && sqlite3_step(totals) == SQLITE_DONE;
    sqlite3_finalize(totals);
    ok = ok && sqlite3_prepare_v2(db,
        "SELECT s.ring_id,coalesce(r.points,s.points),coalesce(r.geometry,s.geometry) "
        "FROM simplified_rings s LEFT JOIN repair.replacements r ON r.ring_id=s.ring_id AND r.tolerance=s.tolerance "
        "WHERE s.tolerance=8 AND NOT EXISTS(SELECT 1 FROM repair.absorbed a JOIN repair.replacements p "
        "ON p.ring_id=a.retained_id WHERE a.ring_id=s.ring_id AND p.tolerance=s.tolerance) ORDER BY s.ring_id",
        -1, &rows, NULL) == SQLITE_OK && sqlite3_prepare_v2(db,
        "SELECT ring_id,geometry FROM review.chunks ORDER BY id", -1, &chunks, NULL) == SQLITE_OK;
    ok = ok && write_bytes(output, (const unsigned char *)"MARSCF64", 8);
    int step = SQLITE_DONE;
    int64_t rings = 0, points = 0, checked = 0;
    while (ok && (step = sqlite3_step(rows)) == SQLITE_ROW) {
        export_ring_t ring;
        int64_t id = sqlite3_column_int64(rows, 0);
        ok = rings < expected_rings && parse_ring(sqlite3_column_blob(rows, 2),
            (size_t)sqlite3_column_bytes(rows, 2), sqlite3_column_int64(rows, 1), &ring) &&
            reviewed_ring(chunks, &ring, id, &checked) && write_ring(output, &ring, (unsigned)rings);
        if (!ok) { fprintf(stderr, "Export rejected at source ring %" PRId64 "; candidate incomplete\n", id); break; }
        ++rings; points += ring.count;
        if (rings % 10000 == 0) {
            fprintf(stderr, "Export: %" PRId64 " rings, %" PRId64 " points verified against review\n", rings, points);
            fflush(stderr);
        }
    }
    ok = ok && step == SQLITE_DONE && sqlite3_step(chunks) == SQLITE_DONE && rings == expected_rings &&
         points == expected_points && checked == expected_chunks && file_sync(output, false);
    sqlite3_finalize(rows); sqlite3_finalize(chunks);
    fprintf(stderr, "Lossless coastline export: %s; %" PRId64 " rings, %" PRId64 " points, %" PRId64
                    " reviewed chunks. Inland-water composition and installation approval still required.\n",
            ok ? "PASS" : "FAIL", rings, points, checked);
    return ok;
}

static bool export_fixture(sqlite3 *db, const char *directory, unsigned attempt, bool expected)
{
    string_t *path = string_sprintf("%s/candidate-%u.cf64", directory, attempt);
    file_t *output = path ? file_new(path) : NULL;
    bool opened = output && file_open(output, FILE_MODE_CREATE_NEW, FILE_ACCESS_WRITE);
    bool result = opened && export_coast(db, output);
    bool ok = opened && result == expected;
    sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
    if (opened) ok = file_close(output) && ok;
    if (ok && expected) {
        unsigned char bytes[116];
        size_t count = 0;
        ok = file_open_read(output) && file_read(output, bytes, sizeof bytes, &count) && count == sizeof bytes &&
             !memcmp(bytes, "MARSCF64", 8) && read_integer(bytes + 12, 4, false) == 4 &&
             read_integer(bytes + 16, 4, false) == (1 | (15 << 8)) && file_close(output);
        string_t *memory = string_new_with(":memory:"), *key = string_new_with("export-test");
        sqlite_t *target = sqlite_open_encrypted(memory, key);
        ok = ok && target && file_open_read(output) && globe_import_shoreline(target, output) && file_close(output);
        sqlite_stmt_t *check = target ? sqlite_stmt_prepare(target,
            "SELECT format_version,point_count FROM shoreline_dataset") : NULL;
        ok = ok && check && sqlite_stmt_step(check) == SQLITE_STEP_ROW &&
             sqlite_stmt_column_int(check, 0) == 2 && sqlite_stmt_column_int(check, 1) == 4;
        sqlite_stmt_finalize(check); sqlite_close(target); string_free(memory); string_free(key);
        /* An existing candidate is never overwritten, even when it was exported successfully. */
        ok = !file_open(output, FILE_MODE_CREATE_NEW, FILE_ACCESS_WRITE) && ok;
    }
    if (output && file_is_open(output)) file_close(output);
    if (output && file_exists(output)) ok = file_delete(output) && ok;
    file_free(output); string_free(path);
    return ok;
}

static bool export_tests(const unsigned char *polygon, size_t polygon_size, const unsigned char *line, size_t line_size)
{
    sqlite3 *db = NULL;
    bool ok = sqlite3_open(":memory:", &db) == SQLITE_OK && sqlite3_exec(db,
        "ATTACH ':memory:' AS repair;ATTACH ':memory:' AS review;"
        "CREATE TABLE simplified_rings(ring_id INTEGER,tolerance REAL,points INTEGER,geometry BLOB);"
        "CREATE TABLE repair.replacements(ring_id INTEGER PRIMARY KEY,tolerance REAL,points INTEGER,geometry BLOB);"
        "CREATE TABLE repair.absorbed(ring_id INTEGER PRIMARY KEY,retained_id INTEGER);"
        "CREATE TABLE review.review(status TEXT);"
        "INSERT INTO review.review VALUES('boundary_contacts_clear_not_install_approved');"
        "CREATE TABLE review.totals(rings INTEGER,points INTEGER,chunks INTEGER,contacts INTEGER);"
        "INSERT INTO review.totals VALUES(1,4,1,0);CREATE TABLE review.contacts(id INTEGER);"
        "CREATE TABLE review.invalid_rings(id INTEGER);"
        "CREATE TABLE review.chunks(id INTEGER PRIMARY KEY,ring_id INTEGER,geometry BLOB);"
        "INSERT INTO simplified_rings VALUES(10,8,4,x'00'),(11,8,4,x'00');"
        "INSERT INTO repair.absorbed VALUES(11,10)", NULL, NULL, NULL) == SQLITE_OK;
    sqlite3_stmt *insert = NULL;
    ok = ok && sqlite3_prepare_v2(db, "INSERT INTO repair.replacements VALUES(10,8,4,?1)",
        -1, &insert, NULL) == SQLITE_OK && sqlite3_bind_blob(insert, 1, polygon, (int)polygon_size,
        SQLITE_TRANSIENT) == SQLITE_OK && sqlite3_step(insert) == SQLITE_DONE;
    sqlite3_finalize(insert); insert = NULL;
    ok = ok && sqlite3_prepare_v2(db, "INSERT INTO review.chunks VALUES(1,10,?1)", -1, &insert, NULL) == SQLITE_OK &&
         sqlite3_bind_blob(insert, 1, line, (int)line_size, SQLITE_TRANSIENT) == SQLITE_OK &&
         sqlite3_step(insert) == SQLITE_DONE;
    sqlite3_finalize(insert);
    string_t *parent = string_new_with("/tmp");
    file_t *directory = file_create_temp_directory(parent);
    ok = ok && directory && export_fixture(db, file_path(directory), 0, true);
    ok = ok && sqlite3_exec(db, "UPDATE review.chunks SET geometry=x'00'", NULL, NULL, NULL) == SQLITE_OK &&
         export_fixture(db, file_path(directory), 1, false);
    ok = ok && sqlite3_exec(db, "UPDATE review.review SET status='incomplete'", NULL, NULL, NULL) == SQLITE_OK &&
         export_fixture(db, file_path(directory), 2, false);
    if (directory) ok = file_remove_directory(directory) && ok;
    file_free(directory); string_free(parent);
    if (db) sqlite3_close(db);
    return ok;
}

static bool self_test(void)
{
    unsigned char polygon[77] = {0}, line[73] = {0};
    write_integer(polygon + 1, 3, 4); write_integer(polygon + 5, 1, 4); write_integer(polygon + 9, 4, 4);
    write_integer(line + 1, 2, 4); write_integer(line + 5, 4, 4);
    double coordinates[] = {100.5934961, 2.9196721, 100.5934962, 2.9196721,
                            100.5934962, 2.9196722, 100.5934961, 2.9196721};
    for (unsigned i = 0; i < 8; ++i) {
        uint64_t bits;
        memcpy(&bits, &coordinates[i], 8);
        write_integer(polygon + 13 + i * 8, bits, 8); write_integer(line + 9 + i * 8, bits, 8);
    }
    export_ring_t ring;
    bool ok = parse_ring(polygon, sizeof polygon, 4, &ring) && same_chunk(&ring, 0, 4, 0, line, sizeof line);
    ok = ok && export_tests(polygon, sizeof polygon, line, sizeof line);
    line[24] ^= 1;
    ok = ok && !same_chunk(&ring, 0, 4, 0, line, sizeof line) && !parse_ring(polygon, sizeof polygon - 1, 4, &ring);
    polygon[76] ^= 1;
    ok = ok && !parse_ring(polygon, sizeof polygon, 4, &ring);
    fprintf(stderr, "Lossless export self-test: %s\n", ok ? "PASS" : "FAIL");
    return ok;
}

/* Export an exclusive offline intermediate; success is not permission to install it. */
int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "--self-test")) return self_test() ? 0 : 1;
    if (argc != 5) {
        fprintf(stderr, "Usage: %s stage.sqlite3 repair.sqlite3 review.sqlite3 NEW-coastline.cf64\n", argv[0]);
        return 2;
    }
    sqlite3 *db = NULL;
    file_t *output = file_new_cstr(argv[4]);
    bool ok = sqlite3_open_v2(argv[1], &db, SQLITE_OPEN_READONLY, NULL) == SQLITE_OK &&
        attach(db, argv[2], "ATTACH DATABASE ?1 AS repair") && attach(db, argv[3], "ATTACH DATABASE ?1 AS review") &&
        output && file_open(output, FILE_MODE_CREATE_NEW, FILE_ACCESS_WRITE) && export_coast(db, output);
    if (output && file_is_open(output)) ok = file_close(output) && ok;
    file_free(output);
    if (db) sqlite3_close(db);
    return ok ? 0 : 1;
}
