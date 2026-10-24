/**
 * @file shoreline_places.c
 * @brief Name selected inland waters using indexed shoreline containment.
 *
 * Named reference points identify water polygons without depending on dataset-specific polygon numbers.
 * Two indexed rays establish that the observer and reference occupy the same lake ring. Detailed surface
 * classification excludes land, islands and the uncertain coastal band. Names are geographic labels only;
 * they neither assign nationality nor describe maritime rights. Unknown waters remain unnamed.
 */
#include <math.h>
#include <stdint.h>
#include <zstd.h>

#define MARS_GLOBE_INTERNAL_ACCESS
#include "globe_layers.h"
#define MARS_SHORELINE_INTERNAL_ACCESS
#include "shoreline_internal.h"

static bool coast_shared_water(shoreline_t *shoreline, double latitude, double longitude,
                               double reference_latitude, double reference_longitude)
{
    /* Keep the spatial candidate set outermost; sorting by polygon must not scan the whole chunk index. */
    sqlite_stmt_t *query = sqlite_stmt_prepare(shoreline->db,
        "SELECT c.polygon_id,c.codec,c.point_count,"
        "CASE WHEN length(b.vertices)<=65536 AND c.byte_offset>=0 AND c.byte_count BETWEEN 1 AND 4112 "
        "AND c.byte_offset<=length(b.vertices)-c.byte_count THEN substr(b.vertices,c.byte_offset+1,c.byte_count) END "
        "FROM (SELECT id FROM shoreline_bounds WHERE max_x>=?1 AND min_y<=?2 AND max_y>=?2 "
        "UNION SELECT id FROM shoreline_bounds WHERE max_x>=?3 AND min_y<=?4 AND max_y>=?4) r "
        "CROSS JOIN shoreline_chunk c ON c.id=r.id CROSS JOIN shoreline_blob b ON b.id=c.blob_id "
        "WHERE c.level=2 ORDER BY c.polygon_id LIMIT 8193");
    bool ok = query && sqlite_stmt_bind_double(query, 1, longitude * 1000000) &&
              sqlite_stmt_bind_double(query, 2, latitude * 1000000) &&
              sqlite_stmt_bind_double(query, 3, reference_longitude * 1000000) &&
              sqlite_stmt_bind_double(query, 4, reference_latitude * 1000000);
    const double x[] = {longitude, reference_longitude}, y[] = {latitude, reference_latitude};
    unsigned parity = 0, chunks = 0, matches = 0;
    int64_t previous = -1;
    sqlite_step_result_t step = SQLITE_STEP_DONE;
    while (ok && (step = sqlite_stmt_step(query)) == SQLITE_STEP_ROW) {
        int64_t polygon = sqlite_stmt_column_int64(query, 0);
        if (polygon != previous) {
            matches += parity == 3;
            parity = 0;
            previous = polygon;
        }
        int codec = sqlite_stmt_column_int(query, 1), points = sqlite_stmt_column_int(query, 2);
        const unsigned char *bytes = sqlite_stmt_column_blob(query, 3);
        size_t length = sqlite_stmt_column_bytes(query, 3), stride = coast_codec_stride(codec);
        unsigned char decoded[4112];
        if (++chunks > 8192 || polygon < 0 || points < 2 || points > 257 || !bytes || !length ||
            length > sizeof decoded || !stride) {
            ok = false;
            break;
        }
        if (codec == SHORELINE_CODEC_ZSTD || codec == SHORELINE_CODEC_F64_ZSTD) {
            length = ZSTD_decompress(decoded, sizeof decoded, bytes, length);
            bytes = decoded;
        }
        if (length != (size_t)points * stride) {
            ok = false;
            break;
        }
        coast_vertex_t a;
        ok = coast_decode_vertex(bytes, codec, &a);
        for (size_t offset = stride; ok && offset < length; offset += stride) {
            coast_vertex_t b;
            ok = coast_decode_vertex(bytes + offset, codec, &b);
            if (!ok) break;
            for (unsigned i = 0; i < 2; ++i)
                if ((a.y > y[i]) != (b.y > y[i]) && x[i] < a.x + (y[i] - a.y) * (b.x - a.x) / (b.y - a.y))
                    parity ^= 1u << i;
            a = b;
        }
    }
    matches += parity == 3;
    sqlite_stmt_finalize(query);
    return ok && step == SQLITE_STEP_DONE && matches == 1;
}

/* Reject unrelated regions before querying; a bounding box alone never supplies a name. */
const char *shoreline_water_name(shoreline_t *shoreline, double latitude, double longitude)
{
    if (!shoreline || !isfinite(latitude) || !isfinite(longitude) ||
        latitude < 36 || latitude > 48 || longitude < 46 || longitude > 55)
        return NULL;
    if (shoreline_hit_test(shoreline, latitude, longitude) != GLOBE_INLAND_WATER) return NULL;
    return coast_shared_water(shoreline, latitude, longitude, 41, 51) ? "Caspian Sea" : NULL;
}
