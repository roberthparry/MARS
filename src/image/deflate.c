/**
 * @file deflate.c
 * @brief Bounded zlib/DEFLATE decoding and native or dependency-free WASM encoding.
 *
 * Implements RFC 1950/1951 decoding with canonical Huffman counts, bounded back-references and Adler-32 checks.
 * The same decoder runs natively and in WASM. Native encoding uses system zlib; the portable encoder emits
 * stored DEFLATE blocks, retaining lossless byte fidelity without host compression callbacks or dependencies.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifndef __wasm__
#include <zlib.h>
#endif

#define MARS_IMAGE_INTERNAL_ACCESS
#include "image_internal.h"

typedef struct {
    const unsigned char *input;
    size_t size, at;
    uint32_t bits;
    unsigned count;
    bool failed;
} bit_reader_t;

typedef struct {
    unsigned count[16], symbols[288];
} huffman_t;

static unsigned take(bit_reader_t *reader, unsigned count)
{
    while (reader->count < count && !reader->failed) {
        if (reader->at == reader->size) { reader->failed = true; break; }
        reader->bits |= (uint32_t)reader->input[reader->at++] << reader->count;
        reader->count += 8;
    }
    unsigned value = reader->bits & ((1u << count) - 1);
    reader->bits >>= count;
    reader->count = reader->count >= count ? reader->count - count : 0;
    return value;
}

static bool huffman_build(huffman_t *table, const unsigned char *lengths, unsigned size)
{
    memset(table, 0, sizeof *table);
    if (size > 288) return false;
    for (unsigned i = 0; i < size; ++i) {
        if (lengths[i] > 15) return false;
        ++table->count[lengths[i]];
    }
    int available = 1;
    unsigned offsets[16] = {0};
    for (unsigned i = 1; i <= 15; ++i) {
        available = available * 2 - (int)table->count[i];
        if (available < 0) return false;
        if (i < 15) offsets[i + 1] = offsets[i] + table->count[i];
    }
    for (unsigned i = 0; i < size; ++i)
        if (lengths[i]) table->symbols[offsets[lengths[i]]++] = i;
    return true;
}

static int huffman_symbol(bit_reader_t *reader, const huffman_t *table)
{
    unsigned code = 0, first = 0, offset = 0;
    for (unsigned length = 1; length <= 15 && !reader->failed; ++length) {
        code |= take(reader, 1);
        unsigned count = table->count[length];
        if (!reader->failed && code >= first && code - first < count)
            return (int)table->symbols[offset + code - first];
        offset += count;
        first = (first + count) * 2;
        code *= 2;
    }
    reader->failed = true;
    return -1;
}

static bool dynamic_tables(bit_reader_t *reader, huffman_t *literal, huffman_t *distance)
{
    static const unsigned order[] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
    unsigned literals = take(reader, 5) + 257, distances = take(reader, 5) + 1, codes = take(reader, 4) + 4;
    unsigned char lengths[318] = {0}, code_lengths[19] = {0};
    if (reader->failed || literals > 286) return false;
    for (unsigned i = 0; i < codes; ++i) code_lengths[order[i]] = (unsigned char)take(reader, 3);
    huffman_t code_table;
    if (reader->failed || !huffman_build(&code_table, code_lengths, 19)) return false;
    unsigned total = literals + distances;
    for (unsigned at = 0; at < total && !reader->failed;) {
        int symbol = huffman_symbol(reader, &code_table);
        if (symbol < 0) return false;
        if (symbol < 16) { lengths[at++] = (unsigned char)symbol; continue; }
        if (symbol == 16 && !at) return false;
        unsigned count = symbol == 16 ? take(reader, 2) + 3 :
                         symbol == 17 ? take(reader, 3) + 3 : take(reader, 7) + 11;
        unsigned char value = symbol == 16 ? lengths[at - 1] : 0;
        if (reader->failed || count > total - at) return false;
        memset(lengths + at, value, count);
        at += count;
    }
    return !reader->failed && lengths[256] && huffman_build(literal, lengths, literals) &&
           huffman_build(distance, lengths + literals, distances);
}

static bool decode_block(bit_reader_t *reader, const huffman_t *literal, const huffman_t *distance,
                         unsigned char *output, size_t capacity, size_t *written)
{
    static const unsigned short lengths[] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,
                                             35,43,51,59,67,83,99,115,131,163,195,227,258};
    static const unsigned char length_bits[] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
    static const unsigned short offsets[] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,
                                             257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
    static const unsigned char offset_bits[] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
    while (!reader->failed) {
        int symbol = huffman_symbol(reader, literal);
        if (symbol < 0 || symbol > 285) return false;
        if (symbol == 256) return true;
        if (symbol < 256) {
            if (*written == capacity) return false;
            output[(*written)++] = (unsigned char)symbol;
            continue;
        }
        unsigned index = (unsigned)symbol - 257;
        unsigned length = lengths[index] + take(reader, length_bits[index]);
        int code = huffman_symbol(reader, distance);
        if (code < 0 || code >= 30) return false;
        unsigned offset = offsets[code] + take(reader, offset_bits[code]);
        if (reader->failed || offset > *written || length > capacity - *written) return false;
        for (unsigned i = 0; i < length; ++i) {
            output[*written] = output[*written - offset];
            ++*written;
        }
    }
    return false;
}

static uint32_t adler32_bytes(const unsigned char *bytes, size_t size)
{
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < size; ++i) { a = (a + bytes[i]) % 65521; b = (b + a) % 65521; }
    return b * 65536 + a;
}

bool image_inflate(const unsigned char *input, size_t size, unsigned char *output, size_t expected)
{
    if (!input || !output || size < 6 || size > IMAGE_BYTE_LIMIT || expected > IMAGE_BYTE_LIMIT ||
        (input[0] & 15) != 8 || input[0] >> 4 > 7 || (input[1] & 32) ||
        ((unsigned)input[0] * 256 + input[1]) % 31) return false;
    bit_reader_t reader = {.input = input + 2, .size = size - 6};
    size_t written = 0;
    bool last = false;
    while (!last && !reader.failed) {
        last = take(&reader, 1) != 0;
        unsigned kind = take(&reader, 2);
        if (reader.failed || kind == 3) return false;
        if (!kind) {
            unsigned padding = reader.count % 8;
            if (padding) take(&reader, padding);
            unsigned length = take(&reader, 16), inverse = take(&reader, 16);
            if (reader.failed || (length ^ inverse) != 65535 || length > expected - written) return false;
            for (unsigned i = 0; i < length; ++i) output[written++] = (unsigned char)take(&reader, 8);
        } else {
            huffman_t literal, distance;
            if (kind == 1) {
                unsigned char lengths[288], distances[32];
                for (unsigned i = 0; i < 288; ++i) lengths[i] = i < 144 ? 8 : i < 256 ? 9 : i < 280 ? 7 : 8;
                memset(distances, 5, sizeof distances);
                if (!huffman_build(&literal, lengths, 288) || !huffman_build(&distance, distances, 32)) return false;
            } else if (!dynamic_tables(&reader, &literal, &distance)) return false;
            if (!decode_block(&reader, &literal, &distance, output, expected, &written)) return false;
        }
    }
    uint32_t checksum = (uint32_t)input[size - 4] << 24 | (uint32_t)input[size - 3] << 16 |
                        (uint32_t)input[size - 2] << 8 | input[size - 1];
    return !reader.failed && reader.at == reader.size && reader.count < 8 && written == expected &&
           adler32_bytes(output, written) == checksum;
}

array_t *image_deflate(const unsigned char *input, size_t size)
{
    if (!input || !size || size > IMAGE_BYTE_LIMIT) return NULL;
    array_t *bytes = array_create(1, NULL, NULL);
    bool ok = bytes != NULL;
#ifndef __wasm__
    uLongf length = compressBound(size);
    unsigned char *encoded = malloc(length);
    ok = ok && encoded && compress2(encoded, &length, input, size, 3) == Z_OK &&
         array_append_carray(bytes, encoded, length);
    free(encoded);
#else
    const unsigned char header[] = {0x78, 0x01};
    ok = ok && array_append_carray(bytes, header, sizeof header);
    for (size_t at = 0; ok && at < size;) {
        unsigned length = size - at > 65535 ? 65535 : (unsigned)(size - at);
        unsigned inverse = length ^ 65535;
        unsigned char block[] = {at + length == size, length, length >> 8, inverse, inverse >> 8};
        ok = array_append_carray(bytes, block, sizeof block) && array_append_carray(bytes, input + at, length);
        at += length;
    }
    uint32_t checksum = adler32_bytes(input, size);
    const unsigned char trailer[] = {checksum >> 24, checksum >> 16, checksum >> 8, checksum};
    ok = ok && array_append_carray(bytes, trailer, sizeof trailer);
#endif
    if (!ok) { array_destroy(bytes); bytes = NULL; }
    return bytes;
}
