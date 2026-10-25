/**
 * @file texture_tiles.c
 * @brief Offline preparation of local photographic surface tiles from NASA reference imagery.
 *
 * Splits the 21600 by 10800 RGB JPEG into a twelve by six grid without resampling.
 * Only one 1800-row strip is decoded at a time. Optional global textures use
 * area-weighted resampling with one decoded source row and one output image.
 * This optional maintainer utility
 * uses libjpeg; the application and ordinary builds do not require its development files.
 * File access uses the MARS file module. Source credit and provenance belong in
 * THIRD_PARTY_NOTICES.md alongside the generated assets.
 */
#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jpeglib.h>

#include "array-t.h"
#include "string-t.h"
#include "file-t.h"

static bool save_image(const string_t *path, unsigned width, unsigned height, size_t stride, unsigned char *pixels)
{
    struct jpeg_compress_struct encoder;
    struct jpeg_error_mgr error;
    encoder.err = jpeg_std_error(&error);
    jpeg_create_compress(&encoder);
    unsigned char *bytes = NULL;
    unsigned long length = 0;
    jpeg_mem_dest(&encoder, &bytes, &length);
    encoder.image_width = width;
    encoder.image_height = height;
    encoder.input_components = 3;
    encoder.in_color_space = JCS_RGB;
    jpeg_set_defaults(&encoder);
    jpeg_set_quality(&encoder, 95, true);
    for (unsigned component = 0; component < 3; ++component) {
        encoder.comp_info[component].h_samp_factor = 1;
        encoder.comp_info[component].v_samp_factor = 1;
    }
    encoder.optimize_coding = true;
    jpeg_start_compress(&encoder, true);
    while (encoder.next_scanline < height) {
        JSAMPROW line = pixels + (size_t)encoder.next_scanline * stride;
        jpeg_write_scanlines(&encoder, &line, 1);
    }
    jpeg_finish_compress(&encoder);
    jpeg_destroy_compress(&encoder);
    file_t *file = path ? file_new(path) : NULL;
    bool ok = file && file_write_all_bytes(file, bytes, length);
    file_free(file);
    free(bytes);
    return ok;
}

static bool save_tile(const char *directory, unsigned column, unsigned row, unsigned char *strip)
{
    string_t *path = string_sprintf("%s/%02u-%02u.jpg", directory, column, row);
    bool ok = path && save_image(path, 1800, 1800, 21600 * 3, strip + column * 1800 * 3);
    string_free(path);
    return ok;
}

static bool save_overview(array_t *bytes, const char *directory, unsigned width)
{
    struct jpeg_decompress_struct decoder;
    struct jpeg_error_mgr error;
    decoder.err = jpeg_std_error(&error);
    jpeg_create_decompress(&decoder);
    jpeg_mem_src(&decoder, array_get(bytes, 0), array_size(bytes));
    jpeg_read_header(&decoder, true);
    decoder.out_color_space = JCS_RGB;
    jpeg_start_decompress(&decoder);
    unsigned height = width / 2;
    unsigned char *pixels = malloc((size_t)width * height * 3), *source = malloc(21600 * 3);
    double *row = malloc(width * 3 * sizeof *row), *sum = malloc(width * 3 * sizeof *sum);
    bool ok = pixels && source && row && sum;
    double ratio = 21600. / width;
    int decoded = -1;
    for (unsigned y = 0; ok && y < height; ++y) {
        memset(sum, 0, width * 3 * sizeof *sum);
        double top = y * ratio, bottom = (y + 1) * ratio;
        for (int sy = (int)floor(top); sy < (int)ceil(bottom); ++sy) {
            if (decoded != sy) {
                JSAMPROW input = source;
                if (jpeg_read_scanlines(&decoder, &input, 1) != 1) { ok = false; break; }
                decoded = sy;
                for (unsigned x = 0; x < width; ++x) {
                    double left = x * ratio, right = (x + 1) * ratio, channels[3] = {0};
                    for (int sx = (int)floor(left); sx < (int)ceil(right); ++sx) {
                        double weight = fmin(right, sx + 1) - fmax(left, sx);
                        for (unsigned c = 0; c < 3; ++c) channels[c] += source[sx * 3 + c] * weight;
                    }
                    for (unsigned c = 0; c < 3; ++c) row[x * 3 + c] = channels[c] / ratio;
                }
            }
            double weight = fmin(bottom, sy + 1) - fmax(top, sy);
            for (unsigned x = 0; x < width * 3; ++x) sum[x] += row[x] * weight;
        }
        for (unsigned x = 0; ok && x < width * 3; ++x)
            pixels[(size_t)y * width * 3 + x] = (unsigned char)fmin(255, floor(sum[x] / ratio + .5));
    }
    if (ok) jpeg_finish_decompress(&decoder);
    jpeg_destroy_decompress(&decoder);
    string_t *path = string_sprintf("%s/earth_surface%s.jpg", directory, width == 2048 ? "" : "_detail");
    ok = ok && path && save_image(path, width, height, width * 3, pixels);
    string_free(path);
    free(pixels);
    free(source);
    free(row);
    free(sum);
    return ok;
}

int main(int argc, char **argv)
{
    if (argc != 3 && argc != 4) {
        fprintf(stderr, "Usage: %s source.jpg tile-directory [overview-directory]\n", argv[0]);
        return 2;
    }
    file_t *source = file_new_cstr(argv[1]), *directory = file_new_cstr(argv[2]);
    file_info_t *info = source ? file_get_info(source) : NULL;
    array_t *bytes = info && file_info_size(info) <= 67108864 ? file_read_all_bytes(source) : NULL;
    bool ok = bytes && array_size(bytes) && directory && file_create_directory(directory, 0755, true);
    struct jpeg_decompress_struct decoder;
    struct jpeg_error_mgr error;
    decoder.err = jpeg_std_error(&error);
    jpeg_create_decompress(&decoder);
    unsigned char *strip = NULL;
    if (ok) {
        jpeg_mem_src(&decoder, array_get(bytes, 0), array_size(bytes));
        ok = jpeg_read_header(&decoder, true) == JPEG_HEADER_OK &&
             decoder.image_width == 21600 && decoder.image_height == 10800;
    }
    if (ok) {
        decoder.out_color_space = JCS_RGB;
        jpeg_start_decompress(&decoder);
        strip = malloc((size_t)1800 * 21600 * 3);
        ok = strip != NULL && decoder.output_components == 3;
    }
    for (unsigned row = 0; ok && row < 6; ++row) {
        for (unsigned line = 0; line < 1800; ++line) {
            JSAMPROW pixels = strip + (size_t)line * 21600 * 3;
            if (jpeg_read_scanlines(&decoder, &pixels, 1) != 1) { ok = false; break; }
        }
        for (unsigned column = 0; ok && column < 12; ++column) ok = save_tile(argv[2], column, row, strip);
        if (ok) fprintf(stderr, "Prepared tile row %u/6\n", row + 1);
    }
    if (ok) jpeg_finish_decompress(&decoder);
    jpeg_destroy_decompress(&decoder);
    free(strip);
    if (ok && argc == 4) {
        ok = save_overview(bytes, argv[3], 2048) && save_overview(bytes, argv[3], 8192);
        if (ok) fprintf(stderr, "Prepared matching global textures\n");
    }
    array_destroy(bytes);
    file_info_free(info);
    file_free(source);
    file_free(directory);
    return ok ? 0 : 1;
}
