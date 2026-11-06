/**
 * @file lab_photo_coverage.c
 * @brief Scoped browser-row adapter for the reusable image alpha-coverage accumulator.
 *
 * The image module owns bitset storage, contiguous-pass validation and exact pixel queries. This adapter copies
 * bounded browser rows and retains the Lab's read-ahead admission policy; no browser handle survives a call.
 */
#include <math.h>

#include "image-t.h"
#include "lab_dom.h"
#include "lab_photo_source.h"

static image_coverage_t *coverage;
static unsigned char coverage_pixels[512 * 64 * 4];

static bool target_rectangle(jsref target, image_rectangle_t *r)
{
    if (lab_dom_count(target) != 4) return false;
    *r = (image_rectangle_t){lab_dom_to_number(lab_dom_item(target, 0)), lab_dom_to_number(lab_dom_item(target, 1)),
                            lab_dom_to_number(lab_dom_item(target, 2)), lab_dom_to_number(lab_dom_item(target, 3))};
    return isfinite(r->x) && isfinite(r->y) && isfinite(r->width) && isfinite(r->height) &&
           r->x >= 0 && r->x < 512 && r->y >= 0 && r->y < 512 && r->width > 0 && r->height > 0 &&
           r->x + r->width <= 512.000001 && r->y + r->height <= 512.000001;
}

/* Copy one browser-owned batch; reusable C code validates and records its alpha. */
void lab_photo_coverage_rows(jsref bytes, unsigned row)
{
    if (!coverage) coverage = image_coverage_new(512, 512);
    int size = lab_dom_bytes_copy(bytes, coverage_pixels, sizeof coverage_pixels);
    if (size <= 0 || size % (512 * 4)) {
        image_coverage_write(coverage, 0, NULL, 0, 0);
        return;
    }
    image_coverage_write(coverage, row, coverage_pixels, (unsigned)size / (512 * 4), 512 * 4);
}

/* Delegate exact conservative fractional-edge queries to the image module. */
bool lab_photo_target_covered(jsref target)
{
    image_rectangle_t r;
    return target_rectangle(target, &r) && image_coverage_contains(coverage, &r);
}

/* Keep invalid targets conservative while delegating rectangle intersection. */
bool lab_photo_targets_overlap(jsref first, jsref second)
{
    image_rectangle_t a, b, intersection;
    if (!target_rectangle(first, &a) || !target_rectangle(second, &b)) return true;
    return image_rectangle_intersection(&a, &b, &intersection);
}

/* Lab scheduling permits one read-ahead with at most a one-sixteenth overlap. */
bool lab_photo_read_ahead(jsref first, jsref second)
{
    image_rectangle_t a, b, intersection;
    if (!target_rectangle(first, &a) || !target_rectangle(second, &b)) return false;
    if (!image_rectangle_intersection(&a, &b, &intersection)) return true;
    return intersection.width * intersection.height <= fmin(a.width * a.height, b.width * b.height) / 16;
}
