/**
 * @file coastline_trial.c
 * @brief Isolated native import and byte-for-byte readback of a complete shoreline artifact.
 *
 * Creates an exclusive encrypted trial database, preserves an unrelated sentinel table,
 * compares every stored edge and its boundary flag with the input, then exercises the
 * public spatial reader. Resume mode checks an existing trial with query-only SQL,
 * repeating readback and acceptance without exporting or importing again. Strict
 * containment is checked separately from the reader's 500-metre proximity band.
 * Never reads credentials or opens an installed database.
 */
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "file-t.h"
#include "globe-t.h"

static uint32_t integer(const unsigned char *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static bool read_exact(file_t *file, unsigned char *bytes, size_t length)
{
    size_t count = 0;
    return file_read(file, bytes, length, &count) && count == length;
}

/* Ordered polygon/chunk indexing makes the complete comparison linear in stored edges. */
static bool readback(sqlite_t *db, file_t *source)
{
    sqlite_stmt_t *chunks = sqlite_stmt_prepare(db,
        "SELECT c.polygon_id,c.level,c.boundary,c.point_count,c.codec,"
        "substr(b.vertices,c.byte_offset+1,c.byte_count) FROM shoreline_chunk c "
        "JOIN shoreline_blob b ON b.id=c.blob_id ORDER BY c.polygon_id,c.id");
    unsigned char prefix[8], header[44], input[4096], previous[16];
    bool ok = chunks && read_exact(source, prefix, 8) && !memcmp(prefix, "MARSCF64", 8);
    int64_t rings = 0, points = 0;
    unsigned levels = 0, polar = 0;
    size_t bytes = 0;
    while (ok && file_read(source, header, sizeof header, &bytes) && bytes) {
        unsigned n = integer(header + 4), flag = integer(header + 8), level = flag & 255;
        unsigned first = integer(header + 28), count = integer(header + 32);
        bool explicit_cap = (flag & (1u << 24)) != 0;
        ok = bytes == sizeof header && integer(header) == (uint64_t)rings && n >= 4 && n <= 8000000 &&
             level >= 1 && level <= 4 && read_exact(source, previous, 16);
        if (!ok) break;
        unsigned seen = 1;
        levels |= 1u << level;
        polar += explicit_cap;
        while (ok && seen < n) {
            ok = sqlite_stmt_step(chunks) == SQLITE_STEP_ROW && sqlite_stmt_column_int64(chunks, 0) == rings &&
                 sqlite_stmt_column_int(chunks, 1) == (int)level && sqlite_stmt_column_int(chunks, 4) == 2;
            unsigned vertices = ok ? (unsigned)sqlite_stmt_column_int(chunks, 3) : 0;
            const unsigned char *stored = ok ? sqlite_stmt_column_blob(chunks, 5) : NULL;
            ok = ok && stored && vertices >= 2 && vertices <= 257 && vertices - 1 <= n - seen &&
                 sqlite_stmt_column_bytes(chunks, 5) == (size_t)vertices * 16 && !memcmp(stored, previous, 16) &&
                 read_exact(source, input, (size_t)(vertices - 1) * 16) &&
                 !memcmp(stored + 16, input, (size_t)(vertices - 1) * 16);
            for (unsigned i = 1; ok && i < vertices; ++i) {
                bool boundary = !explicit_cap || ((seen - 1 + n - 1 - first) % (n - 1)) >= count;
                ok = sqlite_stmt_column_int(chunks, 2) == (int)boundary;
                memcpy(previous, input + (size_t)(i - 1) * 16, 16);
                ++seen;
            }
        }
        if (ok) { ++rings; points += n; }
        if (ok && rings % 10000 == 0) {
            fprintf(stderr, "Native readback: %" PRId64 " rings, %" PRId64 " exact points\n", rings, points);
            fflush(stderr);
        }
    }
    ok = ok && !file_last_error(source) && bytes == 0 && levels == 30 && polar == 1 &&
         sqlite_stmt_step(chunks) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(chunks);
    sqlite_stmt_t *meta = sqlite_stmt_prepare(db,
        "SELECT polygon_count,point_count,format_version FROM shoreline_dataset WHERE id=1");
    ok = ok && meta && sqlite_stmt_step(meta) == SQLITE_STEP_ROW &&
         sqlite_stmt_column_int64(meta, 0) == rings && sqlite_stmt_column_int64(meta, 1) == points &&
         sqlite_stmt_column_int(meta, 2) == 2;
    sqlite_stmt_finalize(meta);
    fprintf(stderr, "Exact native readback: %s; %" PRId64 " rings, %" PRId64 " points\n",
            ok ? "PASS" : "FAIL", rings, points);
    return ok;
}

static double decode(const unsigned char *bytes)
{
    uint64_t bits = (uint64_t)integer(bytes) << 32 | integer(bytes + 4);
    double result;
    memcpy(&result, &bits, sizeof result);
    return result;
}

/* Strict polygon membership and boundary proximity are different acceptance criteria. */
static bool probe_geometry(sqlite_t *db, double lat, double lon, globe_surface_t *surface, bool *near)
{
    sqlite_stmt_t *rows = sqlite_stmt_prepare(db,
        "SELECT c.level,c.boundary,c.codec,c.point_count,substr(b.vertices,c.byte_offset+1,c.byte_count) "
        "FROM shoreline_bounds r JOIN shoreline_chunk c ON c.id=r.id JOIN shoreline_blob b ON b.id=c.blob_id "
        "WHERE r.max_x>=?1 AND r.min_y<=?2 AND r.max_y>=?3");
    double scale = 111195 * cos(lat * 0.017453292519943295);
    double dx = scale > 1 ? 500 / scale : 360, dy = 500 / 111195.0;
    bool ok = rows != NULL;
    unsigned parity = 0;
    *near = false;
    for (int wrap = -1; ok && wrap <= 1; ++wrap) {
        double x = lon + wrap * 360.0;
        ok = sqlite_stmt_bind_double(rows, 1, (x - dx) * 1000000) &&
             sqlite_stmt_bind_double(rows, 2, (lat + dy) * 1000000) &&
             sqlite_stmt_bind_double(rows, 3, (lat - dy) * 1000000);
        sqlite_step_result_t step = SQLITE_STEP_DONE;
        while (ok && (step = sqlite_stmt_step(rows)) == SQLITE_STEP_ROW) {
            int level = sqlite_stmt_column_int(rows, 0), physical = sqlite_stmt_column_int(rows, 1);
            unsigned n = (unsigned)sqlite_stmt_column_int(rows, 3);
            const unsigned char *bytes = sqlite_stmt_column_blob(rows, 4);
            ok = bytes && level >= 1 && level <= 4 && (physical == 0 || physical == 1) &&
                 sqlite_stmt_column_int(rows, 2) == 2 && n >= 2 && n <= 257 &&
                 sqlite_stmt_column_bytes(rows, 4) == (size_t)n * 16;
            for (unsigned i = 1; ok && i < n; ++i) {
                const unsigned char *a = bytes + (size_t)(i - 1) * 16, *b = a + 16;
                double ax = decode(a), ay = decode(a + 8), bx = decode(b), by = decode(b + 8);
                if ((ay > lat) != (by > lat) && x < ax + (lat - ay) * (bx - ax) / (by - ay))
                    parity ^= 1u << level;
                if (physical) {
                    double mx = (ax - x) * scale, my = (ay - lat) * 111195;
                    double ex = (bx - ax) * scale, ey = (by - ay) * 111195, length = ex * ex + ey * ey;
                    double t = length > 0 ? fmax(0, fmin(1, -(mx * ex + my * ey) / length)) : 0;
                    *near |= hypot(mx + t * ex, my + t * ey) <= 500;
                }
            }
        }
        ok = ok && step == SQLITE_STEP_DONE;
        sqlite_stmt_reset(rows);
    }
    sqlite_stmt_finalize(rows);
    *surface = GLOBE_SEA;
    for (unsigned level = 4; level; --level) {
        if (parity & (1u << level)) { *surface = level % 2 ? GLOBE_LAND : GLOBE_INLAND_WATER; break; }
    }
    return ok;
}

static bool geography(sqlite_t *db)
{
    const struct { const char *name; double lat, lon; globe_surface_t expected; } sites[] = {
        {"Malaga centre", 36.7202, -4.4203, GLOBE_LAND},
        {"Malaga offshore", 36.69, -4.42, GLOBE_SEA},
        {"Shrewsbury", 52.7077, -2.7541, GLOBE_LAND},
        {"Terschelling", 53.422855, 5.396457, GLOBE_LAND},
        {"Helsinki", 60.1699, 24.9384, GLOBE_LAND},
        {"Sydney", -33.8678, 151.2073, GLOBE_LAND},
        {"Barangaroo", -33.8610, 151.2020, GLOBE_LAND},
        {"Darling Harbour", -33.8665, 151.2010, GLOBE_SEA},
        {"Circular Quay", -33.8550, 151.2130, GLOBE_SEA},
        {"Lake Superior", 47.7, -87.5, GLOBE_INLAND_WATER},
        {"Atlantic", 40, -30, GLOBE_SEA},
        {"Antarctic interior", -89, 0, GLOBE_LAND},
        {"Antarctic east seam", -89, 180, GLOBE_LAND},
        {"Antarctic west seam", -89, -180, GLOBE_LAND},
        {"South Pole", -90, 0, GLOBE_LAND},
        {"North Pole", 90, 0, GLOBE_SEA}
    };
    globe_t *globe = globe_open(db, NULL);
    bool ok = globe && globe_has_layer(globe, GLOBE_LAYER_SHORELINE);
    for (unsigned i = 0; globe && i < sizeof sites / sizeof *sites; ++i) {
        globe_surface_t strict = GLOBE_UNKNOWN;
        bool near = false;
        bool checked = probe_geometry(db, sites[i].lat, sites[i].lon, &strict, &near);
        globe_surface_t actual = globe_hit_test(globe, sites[i].lat, sites[i].lon);
        bool passed = checked && strict == sites[i].expected && actual == (near ? GLOBE_COAST : strict);
        fprintf(stderr, "Geographic probe %s: %s (containment %d, expected %d; within 500 m %s; API %d)\n",
                sites[i].name, passed ? "PASS" : "FAIL", strict, sites[i].expected, near ? "yes" : "no", actual);
        ok = passed && ok;
    }
    if (ok) {
        fputs("Preparing full native surface cache...\n", stderr);
        fflush(stderr);
        ok = globe_surface_prepare(globe);
        fprintf(stderr, "Full native surface cache: %s\n", ok ? "PASS" : "FAIL");
        for (unsigned i = 0; ok && i < sizeof sites / sizeof *sites; ++i) {
            globe_surface_t strict = GLOBE_UNKNOWN;
            bool near = false;
            ok = probe_geometry(db, sites[i].lat, sites[i].lon, &strict, &near) &&
                 globe_hit_test(globe, sites[i].lat, sites[i].lon) == (near ? GLOBE_COAST : strict);
            fprintf(stderr, "Cached geographic probe %s: %s\n", sites[i].name, ok ? "PASS" : "FAIL");
        }
    }
    globe_close(globe);
    return ok;
}

static void put_integer(unsigned char *bytes, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i) bytes[i] = (unsigned char)(value >> (24 - i * 8));
}

/* Small nested polygons exercise both sides of the uncertainty band, without a world cache. */
static bool self_test(void)
{
    string_t *parent = string_new_with("/tmp"), *memory = string_new_with(":memory:");
    string_t *key = string_new_with("shoreline-probe-fixture");
    file_t *directory = file_create_temp_directory(parent);
    string_t *path = directory ? string_sprintf("%s/probe.cf64", file_path(directory)) : NULL;
    file_t *input = path ? file_new(path) : NULL;
    sqlite_t *db = sqlite_open_encrypted(memory, key);
    size_t written = 0;
    bool ok = db && input && file_open(input, FILE_MODE_CREATE_NEW, FILE_ACCESS_WRITE) &&
        file_write(input, "MARSCF64", 8, &written) && written == 8;
    for (unsigned level = 1; ok && level <= 3; ++level) {
        double span = level == 1 ? 2 : level == 2 ? 1 : 0.5;
        const double vertices[] = {-span, -span, span, -span, span, span, -span, span, -span, -span};
        unsigned char record[44 + 80] = {0};
        put_integer(record, level); put_integer(record + 4, 5); put_integer(record + 8, (15 << 8) | level);
        put_integer(record + 12, (uint32_t)(int32_t)(-span * 1000000));
        put_integer(record + 16, (uint32_t)(int32_t)(span * 1000000));
        put_integer(record + 20, (uint32_t)(int32_t)(-span * 1000000));
        put_integer(record + 24, (uint32_t)(int32_t)(span * 1000000));
        for (unsigned i = 0; i < 10; ++i) {
            uint64_t bits;
            memcpy(&bits, vertices + i, 8);
            put_integer(record + 44 + i * 8, (uint32_t)(bits >> 32));
            put_integer(record + 48 + i * 8, (uint32_t)bits);
        }
        ok = file_write(input, record, sizeof record, &written) && written == sizeof record;
    }
    ok = ok && file_close(input) && file_open_read(input) && globe_import_shoreline(db, input) && file_close(input);
    globe_t *globe = ok ? globe_open(db, NULL) : NULL;
    ok = ok && globe;
    const struct { double lon; globe_surface_t surface; bool near; } sites[] = {
        {0, GLOBE_LAND, false}, {0.499, GLOBE_LAND, true}, {0.501, GLOBE_INLAND_WATER, true},
        {0.75, GLOBE_INLAND_WATER, false}, {1.001, GLOBE_LAND, true}, {1.5, GLOBE_LAND, false},
        {2.001, GLOBE_SEA, true}, {3, GLOBE_SEA, false}
    };
    for (unsigned phase = 0; ok && phase < 2; ++phase) {
        if (phase) ok = globe_surface_prepare(globe);
        for (unsigned i = 0; ok && i < sizeof sites / sizeof *sites; ++i) {
            globe_surface_t strict = GLOBE_UNKNOWN;
            bool near = false;
            ok = probe_geometry(db, 0, sites[i].lon, &strict, &near) && strict == sites[i].surface &&
                 near == sites[i].near && globe_hit_test(globe, 0, sites[i].lon) == (near ? GLOBE_COAST : strict);
        }
    }
    globe_close(globe); sqlite_close(db);
    if (input && file_is_open(input)) file_close(input);
    if (input && file_exists(input)) ok = file_delete(input) && ok;
    if (directory) ok = file_remove_directory(directory) && ok;
    file_free(input); file_free(directory);
    string_free(parent); string_free(memory); string_free(key); string_free(path);
    fprintf(stderr, "Containment/proximity acceptance self-test: %s\n", ok ? "PASS" : "FAIL");
    return ok;
}

/* Leave failed trial artifacts for diagnosis; never overwrite a previous run. */
int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "--self-test")) return self_test() ? 0 : 1;
    bool resume = argc == 4 && !strcmp(argv[1], "--resume");
    if (argc != 3 && !resume) {
        fprintf(stderr, "Usage: %s [--resume] complete.cf64 trial.db\n", argv[0]); return 2;
    }
    file_t *input = file_new_cstr(argv[resume ? 2 : 1]), *output = file_new_cstr(argv[resume ? 3 : 2]);
    string_t *path = string_new_with(argv[resume ? 3 : 2]), *key = string_new_with("offline-shoreline-trial");
    bool ok = input && output && (resume ? file_exists(output) :
        file_open(output, FILE_MODE_CREATE_NEW, FILE_ACCESS_WRITE) && file_close(output));
    sqlite_t *db = ok ? sqlite_open_encrypted(path, key) : NULL;
    ok = ok && db && sqlite_exec_cstr(db, "PRAGMA cache_size=-8192;");
    if (resume) {
        ok = ok && sqlite_exec_cstr(db, "PRAGMA query_only=ON;");
        fputs("Resuming read-only trial acceptance; no export or import repeated.\n", stderr);
    } else {
        ok = ok && sqlite_exec_cstr(db,
            "CREATE TABLE untouched(value TEXT);INSERT INTO untouched VALUES('preserved');") && file_open_read(input);
        fputs("Native trial import starting; installed databases are untouched.\n", stderr);
        ok = ok && globe_import_shoreline(db, input);
        if (input && file_is_open(input)) ok = file_close(input) && ok;
        fprintf(stderr, "Native trial import: %s\n", ok ? "PASS" : "FAIL");
    }
    ok = ok && file_open_read(input) && readback(db, input);
    if (input && file_is_open(input)) ok = file_close(input) && ok;
    sqlite_stmt_t *check = ok ? sqlite_stmt_prepare(db, "PRAGMA integrity_check") : NULL;
    ok = ok && check && sqlite_stmt_step(check) == SQLITE_STEP_ROW &&
         !strcmp(sqlite_stmt_column_text(check, 0), "ok");
    sqlite_stmt_finalize(check);
    check = ok ? sqlite_stmt_prepare(db, "SELECT value FROM untouched") : NULL;
    ok = ok && check && sqlite_stmt_step(check) == SQLITE_STEP_ROW &&
         !strcmp(sqlite_stmt_column_text(check, 0), "preserved");
    sqlite_stmt_finalize(check);
    if (ok) ok = geography(db);
    sqlite_close(db); file_free(input); file_free(output); string_free(path); string_free(key);
    fprintf(stderr, "Complete shoreline native trial: %s. Not installed.\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
