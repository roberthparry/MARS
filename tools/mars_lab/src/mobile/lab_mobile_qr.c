/**
 * @file lab_mobile_qr.c
 * @brief Fixed-capacity QR encoding for native Lab private-access URLs.
 *
 * Encodes ASCII URLs as version-5/L byte-mode symbols with mask zero and a
 * four-module quiet zone. No network discovery, process execution or filesystem
 * access is performed here. Geometry contains no user-supplied SVG markup.
 */
#include <stdlib.h>
#include <string.h>

#include "lab_mobile.h"

enum { qr_size = 37, qr_data_words = 108, qr_check_words = 26, qr_words = 134 };

typedef struct {
    bool dark[qr_size][qr_size];
    bool reserved[qr_size][qr_size];
} qr_grid_t;

static unsigned char lab_mobile_gf_multiply(unsigned char a, unsigned char b, const unsigned char exp[512],
                                            const unsigned char log[256])
{
    return a && b ? exp[(unsigned)log[a] + log[b]] : 0;
}

static void lab_mobile_qr_checks(unsigned char words[qr_words])
{
    unsigned char exp[512] = {0}, log[256] = {0}, generator[qr_check_words + 1] = {1};
    unsigned value = 1;
    for (unsigned i = 0; i < 255; ++i) {
        exp[i] = (unsigned char)value;
        log[value] = (unsigned char)i;
        value <<= 1;
        if (value & 0x100)
            value ^= 0x11d;
    }
    for (unsigned i = 255; i < 512; ++i)
        exp[i] = exp[i - 255];
    for (unsigned i = 0; i < qr_check_words; ++i) {
        unsigned char next[qr_check_words + 1] = {0};
        for (unsigned j = 0; j <= i; ++j) {
            next[j] ^= generator[j];
            next[j + 1] ^= lab_mobile_gf_multiply(generator[j], exp[i], exp, log);
        }
        memcpy(generator, next, sizeof generator);
    }
    unsigned char *remainder = words + qr_data_words;
    for (unsigned i = 0; i < qr_data_words; ++i) {
        unsigned char factor = words[i] ^ remainder[0];
        memmove(remainder, remainder + 1, qr_check_words - 1);
        remainder[qr_check_words - 1] = 0;
        for (unsigned j = 0; j < qr_check_words; ++j)
            remainder[j] ^= lab_mobile_gf_multiply(generator[j + 1], factor, exp, log);
    }
}

static void lab_mobile_append_bits(unsigned char words[qr_words], unsigned *count, unsigned value, unsigned width)
{
    for (unsigned bit = width; bit > 0; --bit) {
        words[*count / 8] |= ((value >> (bit - 1)) & 1u) << (7 - *count % 8);
        ++*count;
    }
}

static void lab_mobile_set_module(qr_grid_t *grid, int x, int y, bool dark)
{
    if (x >= 0 && x < qr_size && y >= 0 && y < qr_size) {
        grid->dark[y][x] = dark;
        grid->reserved[y][x] = true;
    }
}

static void lab_mobile_finder(qr_grid_t *grid, int x0, int y0)
{
    for (int y = -1; y < 8; ++y)
        for (int x = -1; x < 8; ++x)
            lab_mobile_set_module(grid, x0 + x, y0 + y, false);
    for (int y = 0; y < 7; ++y)
        for (int x = 0; x < 7; ++x)
            lab_mobile_set_module(grid, x0 + x, y0 + y,
                                  x == 0 || x == 6 || y == 0 || y == 6 || (x >= 2 && x <= 4 && y >= 2 && y <= 4));
}

static void lab_mobile_qr_matrix(qr_grid_t *grid, const unsigned char words[qr_words])
{
    lab_mobile_finder(grid, 0, 0);
    lab_mobile_finder(grid, qr_size - 7, 0);
    lab_mobile_finder(grid, 0, qr_size - 7);
    for (int y = -2; y <= 2; ++y)
        for (int x = -2; x <= 2; ++x)
            lab_mobile_set_module(grid, 30 + x, 30 + y, abs(x) == 2 || abs(y) == 2 || (x == 0 && y == 0));
    for (int i = 0; i < qr_size; ++i) {
        if (!grid->reserved[6][i])
            lab_mobile_set_module(grid, i, 6, i % 2 == 0);
        if (!grid->reserved[i][6])
            lab_mobile_set_module(grid, 6, i, i % 2 == 0);
    }
    unsigned format = 8, remainder = format;
    for (unsigned i = 0; i < 10; ++i)
        remainder = (remainder << 1) ^ ((remainder >> 9) & 1u ? 0x537 : 0);
    format = ((format << 10) | (remainder & 0x3ff)) ^ 0x5412;
    for (int i = 0; i < 15; ++i) {
        bool bit = ((format >> i) & 1) != 0;
        if (i < 6)
            lab_mobile_set_module(grid, 8, i, bit);
        else if (i < 8)
            lab_mobile_set_module(grid, 8, i + 1, bit);
        else if (i == 8)
            lab_mobile_set_module(grid, 7, 8, bit);
        else
            lab_mobile_set_module(grid, 14 - i, 8, bit);
        if (i < 8)
            lab_mobile_set_module(grid, qr_size - 1 - i, 8, bit);
        else
            lab_mobile_set_module(grid, 8, qr_size - 15 + i, bit);
    }
    lab_mobile_set_module(grid, 8, qr_size - 8, true);
    unsigned index = 0;
    bool upward = true;
    for (int x = qr_size - 1; x > 0; x -= 2) {
        if (x == 6)
            --x;
        for (int row = 0; row < qr_size; ++row) {
            int y = upward ? qr_size - 1 - row : row;
            for (int dx = 0; dx < 2; ++dx) {
                int xx = x - dx;
                if (grid->reserved[y][xx])
                    continue;
                bool bit = index < qr_words * 8 && ((words[index / 8] >> (7 - index % 8)) & 1);
                grid->dark[y][xx] = bit ^ ((xx + y) % 2 == 0);
                ++index;
            }
        }
        upward = !upward;
    }
}

/* Encode only generated ASCII URLs; SVG never contains input text or attributes. */
string_t *lab_mobile_qr(const string_t *url)
{
    size_t length = url ? string_byte_length(url) : 0;
    if (!length || length > 106)
        return string_new();
    string_cursor_t *cursor = string_cursor_new(url);
    if (!cursor)
        return NULL;
    unsigned char words[qr_words] = {0}, ch;
    unsigned count = 0;
    lab_mobile_append_bits(words, &count, 4, 4);
    lab_mobile_append_bits(words, &count, (unsigned)length, 8);
    while (!string_cursor_done(cursor)) {
        if (!string_cursor_peek_ascii(cursor, &ch) || ch < 0x21 || ch > 0x7e) {
            string_cursor_free(cursor);
            return string_new();
        }
        lab_mobile_append_bits(words, &count, ch, 8);
        string_cursor_next(cursor);
    }
    string_cursor_free(cursor);
    lab_mobile_append_bits(words, &count, 0, 4);
    unsigned used = (count + 7) / 8;
    for (unsigned i = used; i < qr_data_words; ++i)
        words[i] = (i - used) % 2 ? 0x11 : 0xec;
    lab_mobile_qr_checks(words);
    qr_grid_t grid = {0};
    lab_mobile_qr_matrix(&grid, words);
    string_t *svg = string_new_with("<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 45 45\" "
                                    "role=\"img\" aria-label=\"Mobile access QR code\">"
                                    "<rect width=\"45\" height=\"45\" fill=\"#fff\"/>"
                                    "<path fill=\"#0f2f5f\" d=\"");
    if (!svg)
        return NULL;
    for (int y = 0; y < qr_size; ++y)
        for (int x = 0; x < qr_size; ++x)
            if (grid.dark[y][x] && string_append_format(svg, "M%d,%dh1v1h-1z", x + 4, y + 4) < 0)
                goto fail;
    if (string_append_cstr(svg, "\"/></svg>"))
        goto fail;
    return svg;
fail:
    string_free(svg);
    return NULL;
}
