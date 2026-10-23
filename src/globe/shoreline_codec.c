/**
 * @file shoreline_codec.c
 * @brief Machine-independent shoreline coordinate decoding and lossless binary64 encoding.
 *
 * Shared by native shoreline import, indexed queries and resident display readers.
 * Legacy integer microdegrees remain supported; binary64 degree pairs preserve the
 * validated coastline without collapsing sub-microdegree geometry. Spatial bounds
 * remain outward-rounded integer microdegrees, independent of coordinate precision.
 */
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#define MARS_GLOBE_INTERNAL_ACCESS
#include "globe_layers.h"
#define MARS_SHORELINE_INTERNAL_ACCESS
#include "shoreline_internal.h"

_Static_assert(sizeof(double) == 8 && DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024,
               "Lossless shoreline coordinates require IEEE binary64 doubles");

static double coast_decode_double(const unsigned char *bytes)
{
    uint64_t bits = 0;
    for (unsigned i = 0; i < 8; ++i) bits = (bits << 8) | bytes[i];
    double value;
    memcpy(&value, &bits, sizeof value);
    return value;
}

static double coast_decode_integer(const unsigned char *bytes)
{
    uint32_t bits = (uint32_t)bytes[0] << 24 | (uint32_t)bytes[1] << 16 | (uint32_t)bytes[2] << 8 | bytes[3];
    int64_t signed_value = bits <= INT32_MAX ? (int64_t)bits : (int64_t)bits - 4294967296LL;
    return (double)signed_value * 0.000001;
}

/* Reject unknown encodings before accessing any coordinate bytes. */
size_t coast_codec_stride(int codec)
{
    if (codec == SHORELINE_CODEC_RAW || codec == SHORELINE_CODEC_ZSTD) return 8;
    if (codec == SHORELINE_CODEC_F64 || codec == SHORELINE_CODEC_F64_ZSTD) return 16;
    return 0;
}

/* All consumers use degrees; no lossy conversion back through integer microdegrees is permitted. */
bool coast_decode_vertex(const unsigned char *bytes, int codec, coast_vertex_t *vertex)
{
    size_t stride = coast_codec_stride(codec);
    if (!bytes || !vertex || !stride) return false;
    vertex->x = stride == 16 ? coast_decode_double(bytes) : coast_decode_integer(bytes);
    vertex->y = stride == 16 ? coast_decode_double(bytes + 8) : coast_decode_integer(bytes + 4);
    return isfinite(vertex->x) && isfinite(vertex->y) && vertex->x >= -180 && vertex->x <= 360 &&
           vertex->y >= -90 && vertex->y <= 90;
}

/* Preserve the bit pattern independently of host byte order. */
void coast_encode_double(unsigned char *bytes, double value)
{
    uint64_t bits;
    memcpy(&bits, &value, sizeof bits);
    for (unsigned i = 0; i < 8; ++i) bytes[i] = (unsigned char)(bits >> (56 - i * 8));
}
