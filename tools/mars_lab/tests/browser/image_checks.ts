/**
 * @file image_checks.ts
 * @brief Direct public image-library WASM regressions without host image-processing imports.
 *
 * JavaScript supplies input bytes and checks golden results. Allocation, PNG encoding/decoding, alpha resizing,
 * coverage, pyramid selection and block creation all execute in the independent C-built WASM module.
 */
async function checkImageModule(check: (condition: boolean, message: string) => void): Promise<void> {
    const response = await fetch('/wasm/image.wasm');
    check(response.ok, 'standalone image WASM asset is served');
    const module = await WebAssembly.compile(await response.arrayBuffer());
    check(WebAssembly.Module.imports(module).length === 0, 'image algorithms require no JavaScript or WASI imports');
    const instance = await WebAssembly.instantiate(module, {});
    type ImageExports = {
        memory: WebAssembly.Memory;
        image_new(width: number, height: number): number;
        image_free(image: number): void;
        image_pixels(image: number): number;
        image_width(image: number): number;
        image_height(image: number): number;
        image_size(image: number): number;
        image_resize(image: number, width: number, height: number): number;
        image_block(image: number, core: number, gutter: number, row: number, column: number): number;
        image_png_encode(image: number): number;
        image_png_decode(bytes: number, count: number): number;
        image_png_validate(bytes: number, count: number): number;
        image_pyramid_new(width: number, height: number, core: number, gutter: number, minimum: number): number;
        image_pyramid_free(pyramid: number): void;
        image_pyramid_select(pyramid: number, source: number, target: number, cached: number, context: number,
                             level: number, reused: number): number;
        image_coverage_new(width: number, height: number): number;
        image_coverage_free(coverage: number): void;
        image_coverage_write(coverage: number, row: number, pixels: number, rows: number, stride: number): number;
        image_coverage_contains(coverage: number, rectangle: number): number;
        array_get(array: number, index: number): number;
        array_size(array: number): number;
        array_destroy(array: number): void;
        malloc(size: number): number;
        free(pointer: number): void;
    };
    const e = instance.exports as unknown as ImageExports;
    const image = e.image_new(2, 1), scratch = e.malloc(96);
    check(Boolean(image && scratch), 'public image allocations are available through WASM');
    let small = 0, png = 0, copy = 0, block = 0, pyramid = 0, coverage = 0;
    try {
        new Uint8Array(e.memory.buffer, e.image_pixels(image), 8).set([255, 0, 0, 255, 0, 255, 0, 0]);
        small = e.image_resize(image, 1, 1);
        check(Boolean(small) && new Uint8Array(e.memory.buffer, e.image_pixels(small), 4).join(',') === '255,0,0,128',
              'shared C resampling excludes invisible green and averages alpha');
        png = e.image_png_encode(small);
        check(Boolean(png), 'public WASM PNG encoding succeeds without host compression');
        const encoded = e.array_get(png, 0), count = e.array_size(png);
        check(Boolean(e.image_png_validate(encoded, count)), 'WASM validates its complete lossless PNG');
        copy = e.image_png_decode(encoded, count);
        check(Boolean(copy) && e.image_width(copy) === 1 && e.image_height(copy) === 1 && e.image_size(copy) === 4 &&
              new Uint8Array(e.memory.buffer, e.image_pixels(copy), 4).join(',') === '255,0,0,128',
              'PNG decoding and its pixel round-trip happen entirely in WASM');
        check(!e.image_png_decode(encoded, count - 1), 'WASM rejects truncated PNG bytes');
        block = e.image_block(image, 2, 1, 0, 0);
        check(Boolean(block) && e.image_width(block) === 4 &&
              new Uint8Array(e.memory.buffer, e.image_pixels(block), 64)[3] === 0,
              'WASM constructs fixed padded blocks with transparent outer gutters');
        pyramid = e.image_pyramid_new(300, 150, 64, 2, 16);
        new Float64Array(e.memory.buffer, scratch, 4).set([50, 25, 200, 100]);
        new Float64Array(e.memory.buffer, scratch + 32, 4).set([10, 20, 100, 50]);
        check(Boolean(e.image_pyramid_select(pyramid, scratch, scratch + 32, 0, 0, scratch + 64, scratch + 84)) &&
              new Uint32Array(e.memory.buffer, scratch + 64, 5).join(',') === '256,256,128,4,2',
              'direct public WASM planning selects non-square pyramid geometry');
        coverage = e.image_coverage_new(2, 1);
        check(Boolean(e.image_coverage_write(coverage, 0, e.image_pixels(image), 1, 8)),
              'the public coverage accumulator accepts C-memory pixel rows');
        new Float64Array(e.memory.buffer, scratch, 4).set([0, 0, 1, 1]);
        check(Boolean(e.image_coverage_contains(coverage, scratch)), 'opaque source pixels count as covered');
        new Float64Array(e.memory.buffer, scratch, 4).set([.5, 0, 1, 1]);
        check(!e.image_coverage_contains(coverage, scratch), 'a fractional edge touching transparent pixels stays uncovered');
    } finally {
        e.image_coverage_free(coverage); e.image_pyramid_free(pyramid); e.image_free(block);
        e.image_free(copy); e.array_destroy(png); e.image_free(small); e.free(scratch); e.image_free(image);
    }
}
