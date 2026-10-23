/**
 * @file check_shoreline_precision.c
 * @brief Read-only white-box regression for every coordinate in a failed coastline quantisation audit.
 *
 * Opens the offline plaintext staging and diagnostic fixtures read-only. Each
 * rejected ring's original WKB coordinates are encoded and decoded through the
 * native lossless shoreline codec, requiring bit-identical values. No geometry
 * is edited and no installed database is opened. This verifies precision, not
 * topology or installation readiness; the original candidate's global review
 * remains the topology evidence. Run after the ordinary shoreline tests.
 */
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

#define MARS_GLOBE_INTERNAL_ACCESS
#include "globe/globe_layers.h"
#define MARS_SHORELINE_INTERNAL_ACCESS
#include "globe/shoreline_internal.h"

static uint64_t read_bits(const unsigned char *bytes, unsigned width, bool little)
{
    uint64_t bits = 0;
    for (unsigned i = 0; i < width; ++i) bits = (bits << 8) | bytes[little ? width - i - 1 : i];
    return bits;
}

static bool check_ring(const unsigned char *wkb, size_t bytes, int64_t expected, int64_t *points)
{
    if (!wkb || bytes < 13 || wkb[0] > 1 || expected < 4 || expected > 8000000) return false;
    bool little = wkb[0] == 1;
    if (read_bits(wkb + 1, 4, little) != 3 || read_bits(wkb + 5, 4, little) != 1 ||
        read_bits(wkb + 9, 4, little) != (uint64_t)expected || bytes != 13 + (size_t)expected * 16) return false;
    for (int64_t i = 0; i < expected; ++i) {
        unsigned char encoded[16];
        double coordinates[2];
        for (unsigned axis = 0; axis < 2; ++axis) {
            uint64_t bits = read_bits(wkb + 13 + (size_t)i * 16 + axis * 8, 8, little);
            memcpy(&coordinates[axis], &bits, sizeof bits);
            coast_encode_double(encoded + axis * 8, coordinates[axis]);
            if (read_bits(encoded + axis * 8, 8, false) != bits) return false;
        }
        coast_vertex_t decoded;
        if (!coast_decode_vertex(encoded, SHORELINE_CODEC_F64, &decoded) ||
            memcmp(&decoded.x, &coordinates[0], sizeof(double)) ||
            memcmp(&decoded.y, &coordinates[1], sizeof(double))) return false;
        ++*points;
    }
    return true;
}

/* Verify the actual rejected rings without mutating either offline input. */
int main(int argc, char **argv)
{
    if (argc != 4) {
        fprintf(stderr, "Usage: %s stage.sqlite3 invalid-audit.sqlite3 tolerance\n", argv[0]);
        return 2;
    }
    char *end;
    errno = 0;
    double tolerance = strtod(argv[3], &end);
    if (errno || end == argv[3] || *end || !isfinite(tolerance) || tolerance < 0.5 || tolerance > 40) return 2;
    sqlite3 *db = NULL;
    sqlite3_stmt *attach = NULL, *count = NULL, *rows = NULL;
    bool ok = sqlite3_open_v2(argv[1], &db, SQLITE_OPEN_READONLY, NULL) == SQLITE_OK &&
        sqlite3_prepare_v2(db, "ATTACH DATABASE ?1 AS diagnostics", -1, &attach, NULL) == SQLITE_OK &&
        sqlite3_bind_text(attach, 1, argv[2], -1, SQLITE_TRANSIENT) == SQLITE_OK && sqlite3_step(attach) == SQLITE_DONE;
    sqlite3_finalize(attach);
    ok = ok && sqlite3_exec(db, "PRAGMA cache_size=-16384;PRAGMA diagnostics.cache_size=-4096;BEGIN", NULL, NULL, NULL)
                   == SQLITE_OK && sqlite3_prepare_v2(db,
        "SELECT count(*) FROM diagnostics.invalid_rings WHERE EXISTS(SELECT 1 FROM diagnostics.review "
        "WHERE status='quantization_invalid_not_install_approved')", -1, &count, NULL) == SQLITE_OK &&
        sqlite3_step(count) == SQLITE_ROW;
    int64_t expected = ok ? sqlite3_column_int64(count, 0) : 0;
    sqlite3_finalize(count);
    ok = ok && expected > 0 && sqlite3_prepare_v2(db,
        "SELECT s.ring_id,s.points,s.geometry FROM diagnostics.invalid_rings d "
        "JOIN simplified_rings s ON s.ring_id=d.ring_id AND s.tolerance=?1 ORDER BY s.ring_id",
        -1, &rows, NULL) == SQLITE_OK && sqlite3_bind_double(rows, 1, tolerance) == SQLITE_OK;
    int step = SQLITE_DONE;
    int64_t rings = 0, points = 0;
    while (ok && (step = sqlite3_step(rows)) == SQLITE_ROW) {
        ok = check_ring(sqlite3_column_blob(rows, 2), (size_t)sqlite3_column_bytes(rows, 2),
                        sqlite3_column_int64(rows, 1), &points);
        if (!ok) fprintf(stderr, "Precision mismatch in ring %" PRId64 "\n", (int64_t)sqlite3_column_int64(rows, 0));
        ++rings;
    }
    ok = ok && step == SQLITE_DONE && rings == expected;
    sqlite3_finalize(rows);
    if (db) sqlite3_close(db);
    fprintf(stderr, "Lossless coordinate regression: %s; %" PRId64 "/%" PRId64
                    " rejected rings, %" PRId64 " bit-identical coordinate pairs\n",
            ok ? "PASS" : "FAIL", rings, expected, points);
    return ok ? 0 : 1;
}
