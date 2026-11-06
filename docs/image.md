# Images and multiresolution blocks

The `image` module supplies owned RGBA images, lossless PNG handling, alpha-aware
resizing, image pyramids and exact alpha coverage. Include `image-t.h`.
The same C sources build into native marslib and WebAssembly. There are no
geographic coordinates, URLs, browser objects, GPU handles or background tasks in
the module. `globe` supplies geographic selection and storage; applications own
transport, scheduling and display.

## Pixels and ownership

`image_new(width, height)` creates transparent, tightly packed, eight-bit RGBA
pixels. `image_from_rgba()` copies caller-owned, possibly padded rows. Release
each owned image with `image_free()`. Dimensions are positive and at most 32768;
individual images are limited to 256 MiB. Allocation failure returns `NULL`.

`image_width()`, `image_height()` and `image_size()` inspect an image.
`image_pixels()` returns mutable pixels, whilst `image_data()` returns a const
view. Both are owned by the image and become invalid when it is freed. Instances
have one serial owner; independent images can be used by separate threads.

`image_resize()` creates an independent image using area filtering. It averages
alpha separately and weights colour by alpha, preventing transparent pixels from
darkening coastlines and other edges. `image_block()` copies a fixed core and
gutters, padding outside the source with transparent black. Input images remain
unchanged after success or failure.

## PNG

`image_png_header()` checks the signature, IHDR marker and bounded dimensions
from at least 24 bytes. It is a preliminary check, not proof of complete validity.
`image_png_validate()` additionally checks chunk ordering, supported format and
every chunk CRC without decompressing pixels. `image_png_decode()` checks the
DEFLATE stream, Adler-32 checksum and exact decoded size, then reverses PNG byte
filters and expands pixels to RGBA. Malformed or unsupported input returns `NULL`.

Supported inputs are non-interlaced, eight-bit greyscale, RGB, indexed,
greyscale-alpha and RGBA PNGs, including their applicable `tRNS` transparency.
Sub-byte, sixteen-bit and interlaced inputs are rejected explicitly. Ancillary
metadata is not preserved when encoding a new image.
Samples are decoded as authored; the module does not perform ICC colour-profile conversion.

`image_png_encode()` returns an owned `array_t` of PNG bytes; release it with
`array_destroy()`. Native encoding uses system zlib. The dependency-free WASM
encoder uses stored DEFLATE blocks: these are valid lossless PNGs, but can occupy
more bytes than native compressed output. Neither encoder writes files or cache
entries. The native photographic preparation tool continues using compression.
Decoding, filters and validation execute in C on both platforms.

The implementation follows the [PNG specification](https://www.w3.org/TR/png-3/)
and [DEFLATE format](https://www.rfc-editor.org/rfc/rfc1951).

## Pyramid geometry

`image_pyramid_new(width, height, core, gutter, minimum)` creates immutable geometry
for rectangular images. It keeps the original longest-side size, then successively
smaller powers of two, stopping at the caller's minimum. Each tier preserves the
aspect ratio, rounding the shorter side upwards. Free it with `image_pyramid_free()`.

`image_pyramid_levels()` returns the tier count, and `image_pyramid_level()` reads
one tier by direct index. `image_pyramid_select()` chooses the smallest tier meeting
the destination's pixel demand, capped at original detail. Its optional native C
cache callback can reuse a sharper tier when the complete level, or all intersecting
blocks, are already resident. A coarser tier cannot satisfy a sharper request.
At most sixteen tiers are considered; block addresses use arithmetic row and column bounds.

`image_pyramid_crop()` maps original source coordinates into a tier.
`image_pyramid_visit()` visits only intersecting cores, supplying exact read,
write, source and destination rectangles through a C callback. Read rectangles
include clipped gutters; write rectangles position them within a fixed padded
block. Source rectangles locate the requested core intersection within that block.
The module does not infer coverage from bounding boxes or invent missing pixels.
Callbacks receive temporary read-only records and must copy anything they retain.

## Coverage and rectangles

`image_coverage_new()` creates a caller-sized bitset; release it with
`image_coverage_free()`. `image_coverage_write()` accepts contiguous RGBA rows.
Row zero starts a fresh pass, and a null, zero-row write explicitly resets it.
Invalid input invalidates the pass. Only alpha 255 counts as opaque.

`image_coverage_contains()` returns true only after a complete pass and when every
pixel touched by the rectangle is opaque. Fractional edges are checked conservatively.
`image_rectangle_intersection()` supplies positive-area overlap; touching edges
have no intersection. Applications choose their own read-ahead and eviction policies.

## Runnable example

This example is also exercised as the last README case in `test_image`.

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "image-t.h"

int main(void)
{
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
    image_free(copy);
    array_destroy(png);
    image_free(small);
    image_free(source);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
```

Output:

```text
RGBA: 255 0 0 128
PNG round-trip: identical
```

## WebAssembly

`make image-wasm` produces `tools/mars_lab/build/release/wasm/image.wasm`, or the
corresponding debug path with `DEBUG=1`. This independent module exports the
public `image_*` functions, its linear memory, allocation/release functions and
the byte-array access needed for encoded PNGs. It has no JavaScript or WASI imports.
Browser code may copy input/output bytes; image processing runs in WASM.

The same library objects are linked into Lab's browser worker module. Its source
planner and coverage adapter call this C API. Prepared photographic PNG blocks
are decoded by the image module before browser graphics objects are created from
their RGBA pixels. Legacy whole-photograph loading retains its existing bounded
browser decoder for older catalogues; prepared blocks use the C path.

Pointers and `size_t` values use the wasm32 ABI. Borrowed pixel pointers remain
valid until the owning image is freed, but hosts must recreate typed-array views
after memory growth. Callbacks are native function pointers for C clients compiled
with the module; optional cache callbacks may be null. Hosts own asynchronous I/O
and must respect each object's serial lifetime.
