/**
 * @file lab_photo_source.c
 * @brief Browser metadata and URL adapter for the reusable native image-pyramid planner.
 *
 * The image module owns resolution choice, cache-coverage probing, crop arithmetic and block gutters.
 * This adapter translates scoped browser values and constructs Lab-specific content keys and HTTP addresses.
 * No image geometry or resolution algorithm is implemented in JavaScript or duplicated in this adapter.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "image-t.h"
#include "lab_dom.h"
#include "lab_photo_source.h"

static image_t *decoded_block;

/* Release the worker-owned decoded pixels after the host has copied them. */
void lab_photo_decode_release(void)
{
    image_free(decoded_block);
    decoded_block = NULL;
}

/* Decode only small independently addressed blocks; whole-image compatibility remains bounded separately. */
void lab_photo_decode(jsref bytes)
{
    lab_photo_decode_release();
    unsigned count = lab_dom_count(bytes);
    if (count < 57 || count > 2 * 1024 * 1024) { lab_dom_return(0); return; }
    unsigned char *input = malloc(count);
    bool copied = input && lab_dom_bytes_copy(bytes, input, count) == (int)count;
    unsigned width, height;
    bool valid = copied && image_png_header(input, count, &width, &height) && width == 514 && height == 514;
    decoded_block = valid ? image_png_decode(input, count) : NULL;
    free(input);
    if (!decoded_block || image_width(decoded_block) != 514 || image_height(decoded_block) != 514) {
        lab_photo_decode_release(); lab_dom_return(0); return;
    }
    jsref result = lab_dom_object(LAB_VALUE_OBJECT);
    lab_dom_set(result, "pointer", lab_dom_numeric((unsigned long)image_pixels(decoded_block)));
    lab_dom_set(result, "length", lab_dom_numeric(image_size(decoded_block)));
    lab_dom_return(result);
}

static jsref rectangle(const image_rectangle_t *r)
{
    jsref value = lab_dom_object(LAB_VALUE_ARRAY);
    const double coordinates[] = {r->x, r->y, r->width, r->height};
    for (unsigned i = 0; i < 4; ++i) lab_dom_push(value, lab_dom_numeric(coordinates[i]));
    return value;
}

static bool rectangle_read(jsref value, image_rectangle_t *r)
{
    if (lab_dom_count(value) != 4) return false;
    *r = (image_rectangle_t){lab_dom_to_number(lab_dom_item(value, 0)), lab_dom_to_number(lab_dom_item(value, 1)),
                            lab_dom_to_number(lab_dom_item(value, 2)), lab_dom_to_number(lab_dom_item(value, 3))};
    return true;
}

/* Delegate PNG dimension recognition to the shared image module. */
bool lab_photo_source_header(jsref bytes, unsigned expected)
{
    unsigned char header[24];
    unsigned width, height;
    return (expected == 514 || expected == 1800 || expected == 3600) &&
           lab_dom_bytes_copy(bytes, header, sizeof header) == (int)sizeof header &&
           image_png_header(header, sizeof header, &width, &height) && width == expected && height == expected;
}

static jsref source_key(jsref photo, unsigned width)
{
    if (lab_dom_truth(lab_dom_get(photo, "pyramid"))) {
        jsref key = lab_dom_join(lab_dom_string("block-v1:"), lab_dom_get(photo, "sha256"), ":");
        return lab_dom_join(key, lab_dom_format(width, "", ":"), "");
    }
    jsref key = lab_dom_join(lab_dom_get(photo, "url"), lab_dom_get(photo, "sha256"), ":");
    return lab_dom_join(key, lab_dom_format(width, "", ":"), ":");
}

static jsref block_key(jsref source, unsigned row, unsigned column)
{
    jsref key = lab_dom_join(source, lab_dom_format(row, "", ":"), "");
    return lab_dom_join(key, lab_dom_format(column, "", ":"), "");
}

typedef struct {
    jsref photo, parts, key;
    lab_photo_source_cached_fn cached;
    void *context;
    bool pyramid;
} source_adapter_t;

static bool cached_pixels(unsigned tier, const image_block_t *block, void *context)
{
    source_adapter_t *adapter = context;
    unsigned mark = lab_dom_mark();
    jsref key = source_key(adapter->photo, tier);
    if (block) key = block_key(key, block->row, block->column);
    char text[256] = {0};
    int size = lab_dom_utf8_copy(key, (unsigned char *)text, sizeof text - 1);
    bool available = size > 0 && size < (int)sizeof text && adapter->cached(text, adapter->context);
    lab_dom_release(mark);
    return available;
}

static bool emit_block(const image_block_t *block, void *context)
{
    source_adapter_t *adapter = context;
    jsref part = lab_dom_object(LAB_VALUE_OBJECT);
    lab_dom_set(part, "key", block_key(adapter->key, block->row, block->column));
    if (adapter->pyramid) {
        jsref address = lab_dom_join(lab_dom_string("/earth-block?v=1&sha="),
                                    lab_dom_get(adapter->photo, "sha256"), "&tier=");
        address = lab_dom_join(address, lab_dom_format(block->tier, "", "&row="), "");
        address = lab_dom_join(address, lab_dom_format(block->row, "", "&column="), "");
        lab_dom_set(part, "url", lab_dom_join(address, lab_dom_format(block->column, "", ""), ""));
    }
    lab_dom_set(part, "read", rectangle(&block->read));
    lab_dom_set(part, "write", rectangle(&block->write));
    lab_dom_set(part, "source", rectangle(&block->source));
    lab_dom_set(part, "target", rectangle(&block->target));
    lab_dom_set(part, "needed", lab_dom_numeric(1));
    lab_dom_push(adapter->parts, part);
    return true;
}

/* Supply plain C rectangles and a native cache callback to the library. */
jsref lab_photo_source_plan(jsref photo, lab_photo_source_cached_fn cached, void *context)
{
    double width = lab_dom_to_number(lab_dom_get(photo, "width"));
    image_rectangle_t source, target, decoded;
    char url[64] = {0}, digest[65] = {0};
    int u = lab_dom_utf8_copy(lab_dom_get(photo, "url"), (unsigned char *)url, sizeof url - 1);
    int d = lab_dom_utf8_copy(lab_dom_get(photo, "sha256"), (unsigned char *)digest, sizeof digest - 1);
    if ((width != 1800 && width != 3600) || u <= 0 || d != 64 ||
        !rectangle_read(lab_dom_get(photo, "source"), &source) ||
        !rectangle_read(lab_dom_get(photo, "target"), &target)) return 0;
    image_pyramid_t *pyramid = image_pyramid_new((unsigned)width, (unsigned)width, 512, 1, 64);
    image_level_t level;
    bool reused = false;
    source_adapter_t adapter = {.photo = photo, .cached = cached, .context = context,
                                .pyramid = lab_dom_truth(lab_dom_get(photo, "pyramid"))};
    bool ok = image_pyramid_select(pyramid, &source, &target, cached ? cached_pixels : NULL, &adapter, &level, &reused);
    jsref plan = ok ? lab_dom_object(LAB_VALUE_OBJECT) : 0;
    if (ok) {
        adapter.key = source_key(photo, level.edge);
        adapter.parts = lab_dom_object(LAB_VALUE_ARRAY);
        ok = image_pyramid_crop(pyramid, level.edge, &source, &decoded) &&
             image_pyramid_visit(pyramid, level.edge, &source, &target, emit_block, &adapter);
    }
    image_pyramid_free(pyramid);
    if (!ok) return 0;
    lab_dom_set(plan, "key", adapter.key);
    lab_dom_set(plan, "width", lab_dom_numeric(level.edge));
    lab_dom_set(plan, "cached", lab_dom_scalar(LAB_VALUE_BOOLEAN, reused));
    lab_dom_set(plan, "prefix", adapter.pyramid ? lab_dom_join(lab_dom_string("block-v1:"),
        lab_dom_get(photo, "sha256"), ":") : lab_dom_join(lab_dom_get(photo, "url"), lab_dom_get(photo, "sha256"), ":"));
    lab_dom_set(plan, "retain", lab_dom_scalar(LAB_VALUE_BOOLEAN, !adapter.pyramid && level.edge <= 2048));
    lab_dom_set(plan, "source", rectangle(&decoded));
    lab_dom_set(plan, "parts", adapter.parts);
    return plan;
}

/* Expose the same library planner through the scoped browser transport. */
void lab_photo_source(jsref photo)
{
    lab_dom_return(lab_photo_source_plan(photo, NULL, NULL));
}
