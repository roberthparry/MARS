/**
 * @file test_image.c
 * @brief Native image ownership, alpha resampling, PNG interoperability and pyramid regressions.
 *
 * Independent system-zlib fixtures exercise stored, fixed and dynamic DEFLATE streams, all PNG byte filters,
 * non-square crops and coverage boundaries. The runnable module-guide example is executed after ordinary tests.
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <zlib.h>

#include "image-t.h"
#include "test_harness.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);

static void put32(unsigned char *p, uint32_t value)
{
    p[0] = value >> 24; p[1] = value >> 16; p[2] = value >> 8; p[3] = value;
}

static bool fixture_chunk(array_t *out, const char *type, const void *bytes, unsigned size)
{
    unsigned char header[8], trailer[4];
    put32(header, size); memcpy(header + 4, type, 4);
    uint32_t crc = crc32(0, (const Bytef *)type, 4);
    if (size) crc = crc32(crc, bytes, size);
    put32(trailer, crc);
    return array_append_carray(out, header, 8) && (!size || array_append_carray(out, bytes, size)) &&
           array_append_carray(out, trailer, 4);
}

static array_t *fixture_png(unsigned colour, unsigned filter, int strategy, int compression)
{
    static const unsigned channels[] = {1,0,3,1,2,0,4};
    unsigned step = channels[colour], stride = 31 * step;
    unsigned char pixels[31 * 4 * 19], raw[(31 * 4 + 1) * 19], encoded[8192];
    for (unsigned y = 0; y < 19; ++y)
        for (unsigned x = 0; x < stride; ++x) pixels[y * stride + x] = (unsigned char)((x + y * 7) % 251);
    for (unsigned y = 0; y < 19; ++y) {
        raw[y * (stride + 1)] = (unsigned char)filter;
        for (unsigned x = 0; x < stride; ++x) {
            int left = x >= step ? pixels[y * stride + x - step] : 0;
            int up = y ? pixels[(y - 1) * stride + x] : 0;
            int corner = y && x >= step ? pixels[(y - 1) * stride + x - step] : 0;
            int estimate = left + up - corner;
            int a = estimate > left ? estimate - left : left - estimate;
            int b = estimate > up ? estimate - up : up - estimate;
            int c = estimate > corner ? estimate - corner : corner - estimate;
            int prediction = filter == 1 ? left : filter == 2 ? up : filter == 3 ? (left + up) / 2 :
                             filter == 4 ? (a <= b && a <= c ? left : b <= c ? up : corner) : 0;
            raw[y * (stride + 1) + x + 1] = (unsigned char)(pixels[y * stride + x] - prediction);
        }
    }
    z_stream stream = {0};
    bool ok = deflateInit2(&stream, compression, Z_DEFLATED, 15, 8, strategy) == Z_OK;
    stream.next_in = raw; stream.avail_in = (stride + 1) * 19;
    stream.next_out = encoded; stream.avail_out = sizeof encoded;
    ok = ok && deflate(&stream, Z_FINISH) == Z_STREAM_END;
    unsigned length = stream.total_out;
    deflateEnd(&stream);
    array_t *out = ok ? array_create(1, NULL, NULL) : NULL;
    unsigned char header[13] = {0}, palette[768], alpha[256];
    put32(header, 31); put32(header + 4, 19); header[8] = 8; header[9] = colour;
    for (unsigned i = 0; i < 256; ++i) {
        palette[i * 3] = i; palette[i * 3 + 1] = 255 - i; palette[i * 3 + 2] = i / 2; alpha[i] = i;
    }
    ok = out && array_append_carray(out, "\211PNG\r\n\032\n", 8) && fixture_chunk(out, "IHDR", header, 13);
    if (colour == 3) ok = ok && fixture_chunk(out, "PLTE", palette, 768) && fixture_chunk(out, "tRNS", alpha, 256);
    ok = ok && fixture_chunk(out, "IDAT", encoded, length / 2) &&
         fixture_chunk(out, "IDAT", encoded + length / 2, length - length / 2) && fixture_chunk(out, "IEND", NULL, 0);
    if (!ok) { array_destroy(out); out = NULL; }
    return out;
}

static void test_image_png_external_streams(void)
{
    const unsigned colours[] = {0,2,3,4,6};
    const int strategies[] = {Z_DEFAULT_STRATEGY,Z_FIXED,Z_HUFFMAN_ONLY};
    bool ok = true;
    for (unsigned c = 0; ok && c < 5; ++c) {
        for (unsigned filter = 0; ok && filter < 5; ++filter) {
            for (unsigned strategy = 0; ok && strategy < 4; ++strategy) {
                array_t *png = fixture_png(colours[c], filter, strategies[strategy % 3], strategy == 3 ? 0 : 6);
                image_t *image = png ? image_png_decode(array_get(png, 0), array_size(png)) : NULL;
                ok = image && image_width(image) == 31 && image_height(image) == 19 &&
                     image_png_validate(array_get(png, 0), array_size(png));
                const unsigned channels[] = {1,0,3,1,2,0,4};
                unsigned step = channels[colours[c]];
                const unsigned char *pixels = image_data(image);
                for (unsigned y = 0; ok && y < 19; ++y) {
                    for (unsigned x = 0; ok && x < 31; ++x) {
                        unsigned char value[4];
                        for (unsigned j = 0; j < step; ++j) value[j] = (unsigned char)((x * step + j + y * 7) % 251);
                        const unsigned char *p = pixels + (y * 31 + x) * 4;
                        unsigned char expected[4] = {value[0],value[0],value[0],255};
                        if (colours[c] == 2 || colours[c] == 6) memcpy(expected, value, step);
                        if (colours[c] == 4) expected[3] = value[1];
                        if (colours[c] == 3) {
                            expected[1] = 255 - value[0]; expected[2] = value[0] / 2; expected[3] = value[0];
                        }
                        ok = !memcmp(p, expected, 4);
                    }
                }
                if (!ok) fprintf(stderr, "PNG colour=%u filter=%u strategy=%u failed\n", colours[c], filter, strategy);
                image_free(image); array_destroy(png);
            }
        }
    }
    TEST_ASSERT_TRUE(ok, "portable decoder accepts independent stored, fixed, dynamic, filtered and palette PNGs");
}

static void test_image_pixels_and_corruption(void)
{
    const unsigned char pixels[] = {255,0,0,255,0,255,0,0};
    image_t *image = image_from_rgba(2, 1, pixels, 8), *small = image_resize(image, 1, 1);
    const unsigned char expected[] = {255,0,0,128};
    TEST_ASSERT_TRUE(small && image_size(small) == 4 && !memcmp(image_data(small), expected, 4),
                     "transparent colour does not darken an opaque neighbour when averaged");
    image_t *block = image_block(image, 2, 1, 0, 0);
    TEST_ASSERT_TRUE(block && image_width(block) == 4 && image_data(block)[(1 * 4 + 1) * 4] == 255 &&
                     image_data(block)[3] == 0, "gutters outside the source contain transparent black");
    array_t *png = image_png_encode(image);
    image_t *copy = png ? image_png_decode(array_get(png, 0), array_size(png)) : NULL;
    TEST_ASSERT_TRUE(copy && image_size(copy) == sizeof pixels && !memcmp(image_data(copy), pixels, sizeof pixels),
                     "lossless encoding retains straight-alpha colours even at alpha zero");
    size_t size = png ? array_size(png) : 0;
    bool rejected = png != NULL;
    for (size_t i = 0; rejected && i < size; ++i) {
        image_t *truncated = image_png_decode(array_get(png, 0), i);
        rejected = !truncated;
        image_free(truncated);
    }
    TEST_ASSERT_TRUE(rejected, "every truncated prefix is rejected without reading outside the supplied bytes");
    if (png) ((unsigned char *)array_get(png, 0))[29] ^= 1;
    TEST_ASSERT_TRUE(png && !image_png_validate(array_get(png, 0), size) && !image_png_decode(array_get(png, 0), size),
                     "chunk checksum corruption is rejected");
    TEST_ASSERT_TRUE(!image_new(0, 1) && !image_new(32768, 32768) && !image_from_rgba(2, 1, pixels, 4) &&
                     !image_block(image, 2, 1, 0, 1), "invalid sizes and out-of-source block addresses fail explicitly");
    image_free(copy); array_destroy(png); image_free(block); image_free(small); image_free(image);
}

typedef struct { unsigned count; double area; } block_count_t;

static bool count_block(const image_block_t *block, void *context)
{
    block_count_t *state = context;
    ++state->count; state->area += block->target.width * block->target.height;
    return true;
}

static bool cached_level(unsigned tier, const image_block_t *block, void *context)
{
    unsigned *wanted = context;
    return tier == *wanted && block != NULL;
}

static void test_image_rectangular_pyramid(void)
{
    image_pyramid_t *p = image_pyramid_new(300, 150, 64, 2, 16);
    image_rectangle_t source = {50,25,200,100}, target = {10,20,100,50}, decoded;
    image_level_t level;
    bool hit = true;
    TEST_ASSERT_TRUE(p && image_pyramid_levels(p) == 6 &&
                     image_pyramid_select(p, &source, &target, NULL, NULL, &level, &hit) &&
                     level.edge == 256 && level.height == 128 && !hit,
                     "non-square images select the smallest tier meeting destination demand");
    block_count_t blocks = {0};
    TEST_ASSERT_TRUE(image_pyramid_visit(p, level.edge, &source, &target, count_block, &blocks) &&
                     blocks.count == 8 && fabs(blocks.area - 5000) < 1e-8 &&
                     image_pyramid_crop(p, level.edge, &source, &decoded) && fabs(decoded.x - 128.0 / 3) < 1e-8,
                     "only intersecting cores are enumerated and their mapped area exactly covers the requested target");
    unsigned cached = 300;
    TEST_ASSERT_TRUE(image_pyramid_select(p, &source, &target, cached_level, &cached, &level, &hit) &&
                     hit && level.edge == 300, "complete sharper cached blocks can satisfy a coarser request");
    TEST_ASSERT_TRUE(!image_pyramid_visit(p, 255, &source, &target, count_block, &blocks) &&
                     !image_pyramid_select(p, &(image_rectangle_t){0,0,NAN,1}, &target, NULL, NULL, &level, NULL),
                     "invalid tiers and non-finite crops cannot produce plans");
    image_pyramid_free(p);
}

static void test_image_coverage_edges(void)
{
    image_coverage_t *coverage = image_coverage_new(65, 2);
    unsigned char rows[65 * 2 * 4];
    memset(rows, 255, sizeof rows); rows[(65 + 64) * 4 + 3] = 254;
    TEST_ASSERT_TRUE(image_coverage_write(coverage, 0, rows, 1, 260) &&
                     !image_coverage_contains(coverage, &(image_rectangle_t){0,0,64,1}),
                     "incomplete row passes never claim complete coverage");
    TEST_ASSERT_TRUE(image_coverage_write(coverage, 1, rows + 260, 1, 260) &&
                     image_coverage_contains(coverage, &(image_rectangle_t){0,0,64,2}) &&
                     !image_coverage_contains(coverage, &(image_rectangle_t){63.5,1,1,1}),
                     "partial alpha at a bitset-word boundary prevents fractional coverage claims");
    TEST_ASSERT_TRUE(!image_coverage_write(coverage, 3, rows, 1, 260) &&
                     !image_coverage_contains(coverage, &(image_rectangle_t){0,0,1,1}) &&
                     image_coverage_write(coverage, 0, NULL, 0, 0), "out-of-order input invalidates the preceding pass");
    image_rectangle_t overlap;
    TEST_ASSERT_TRUE(!image_rectangle_intersection(&(image_rectangle_t){0,0,1,1},
                         &(image_rectangle_t){1,0,1,1}, &overlap) &&
                     image_rectangle_intersection(&(image_rectangle_t){0,0,2,1},
                         &(image_rectangle_t){1,0,2,1}, &overlap) && overlap.width == 1,
                     "merely touching rectangles have no overlap");
    image_coverage_free(coverage);
}

static void example_image_readme(void)
{
    /* README example: alpha-aware resizing and an independently owned PNG round-trip. */
    const unsigned char rgba[] = {255,0,0,255,0,255,0,0};
    image_t *source = image_from_rgba(2, 1, rgba, 8);
    image_t *small = image_resize(source, 1, 1);
    array_t *png = image_png_encode(small);
    image_t *copy = png ? image_png_decode(array_get(png, 0), array_size(png)) : NULL;
    bool ok = copy && image_size(copy) == 4 && !memcmp(image_data(small), image_data(copy), 4);
    if (ok) {
        const unsigned char *p = image_data(copy);
        printf("RGBA: %u %u %u %u\nPNG round-trip: identical\n", p[0],p[1],p[2],p[3]);
    }
    image_free(copy); array_destroy(png); image_free(small); image_free(source);
    TEST_ASSERT_TRUE(ok, "README image example output agrees with docs/image.md");
}

int tests_main(void)
{
    TEST_RUN_IN_GROUP(test_image_png_external_streams, tests, NULL);
    TEST_RUN_IN_GROUP(test_image_pixels_and_corruption, tests, NULL);
    TEST_RUN_IN_GROUP(test_image_rectangular_pyramid, tests, NULL);
    TEST_RUN_IN_GROUP(test_image_coverage_edges, tests, NULL);
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_image_readme, readme_examples, "image,readme,output");
    return TESTS_EXIT_CODE();
}
