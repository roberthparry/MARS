/**
 * @file lab_photo_coverage.c
 * @brief Exact native alpha coverage for avoiding hidden photographic source reads.
 *
 * A serial photographic worker submits one 512-square cell in bounded row batches. Native row bitsets answer
 * whether a source rectangle can contribute any pixels, without inspecting browser objects or retaining images.
 * Only fully opaque pixels count; transparent observation gaps still require their underlying photographs.
 */
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "lab_dom.h"
#include "lab_photo_source.h"

static uint64_t coverage[512][8];
static unsigned coverage_rows;
static unsigned char coverage_pixels[512 * 64 * 4];

static bool target_rectangle(jsref target, double rectangle[4])
{
    if (lab_dom_count(target) != 4) return false;
    for (unsigned i = 0; i < 4; ++i) {
        rectangle[i] = lab_dom_to_number(lab_dom_item(target, i));
        if (!isfinite(rectangle[i])) return false;
    }
    return rectangle[0] >= 0 && rectangle[0] < 512 && rectangle[1] >= 0 && rectangle[1] < 512 &&
           rectangle[2] > 0 && rectangle[3] > 0 &&
           rectangle[0] + rectangle[2] <= 512.000001 && rectangle[1] + rectangle[3] <= 512.000001;
}

/* Replace one sequential row batch; row zero also starts a fresh cell's coverage. */
void lab_photo_coverage_rows(jsref bytes, unsigned row)
{
    if (!row) coverage_rows = 0;
    int size = lab_dom_bytes_copy(bytes, coverage_pixels, sizeof coverage_pixels);
    if (row != coverage_rows || size <= 0 || size % (512 * 4) || row + size / (512 * 4) > 512) {
        coverage_rows = 0;
        return;
    }
    unsigned rows = (unsigned)size / (512 * 4);
    memset(coverage + row, 0, rows * sizeof *coverage);
    for (unsigned y = 0; y < rows; ++y)
        for (unsigned x = 0; x < 512; ++x)
            if (coverage_pixels[(y * 512 + x) * 4 + 3] == 255)
                coverage[row + y][x / 64] |= UINT64_C(1) << (x % 64);
    coverage_rows += rows;
}

/* Query only intersecting bitset words, conservatively including fractional edge pixels. */
bool lab_photo_target_covered(jsref target)
{
    double rectangle[4];
    if (coverage_rows != 512 || !target_rectangle(target, rectangle)) return false;
    unsigned left = (unsigned)floor(rectangle[0]), top = (unsigned)floor(rectangle[1]);
    unsigned right = (unsigned)fmin(512, ceil(rectangle[0] + rectangle[2]));
    unsigned bottom = (unsigned)fmin(512, ceil(rectangle[1] + rectangle[3]));
    for (unsigned word = left / 64; word <= (right - 1) / 64; ++word) {
        unsigned first = word == left / 64 ? left % 64 : 0;
        unsigned last = word == (right - 1) / 64 ? (right - 1) % 64 + 1 : 64;
        uint64_t mask = (UINT64_MAX << first) & (last == 64 ? UINT64_MAX : (UINT64_C(1) << last) - 1);
        for (unsigned row = top; row < bottom; ++row)
            if ((coverage[row][word] & mask) != mask) return false;
    }
    return true;
}

/* Defer speculative reads which the current foreground source may make unnecessary. */
bool lab_photo_targets_overlap(jsref first, jsref second)
{
    double a[4], b[4];
    if (!target_rectangle(first, a) || !target_rectangle(second, b)) return true;
    return a[0] < b[0] + b[2] && a[0] + a[2] > b[0] && a[1] < b[1] + b[3] && a[1] + a[3] > b[1];
}
