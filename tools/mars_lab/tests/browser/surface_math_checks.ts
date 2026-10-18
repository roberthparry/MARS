/**
 * @file surface_math_checks.ts
 * @brief Linked WASM maths and viewport safeguard regressions.
 *
 * Exercises the actual browser module, comparing ordinary display arithmetic with
 * browser maths and checking exact landmarks, extreme inputs and intentional fallbacks.
 * The import check prevents the linked library from introducing a host maths or WASI dependency.
 */
interface Window {
    /** @brief Check linked maths, viewport limits and the browser module's imports. */
    checkLabSurfaceMath(): Promise<void>;
}

/** @brief Exercise linked maths through native exports and the scoped camera and gesture APIs. */
window.checkLabSurfaceMath = async function checkLabSurfaceMath(): Promise<void> {
    const native = labWire.exports();

    /** @brief Report one failed maths or linkage assertion. */
    function check(condition: boolean, message: string): void {
        if (!condition) throw new Error('Surface maths: ' + message);
    }

    /** @brief Allow small library rounding differences while retaining NaN and signed-zero checks. */
    function close(actual: number, expected: number, message: string): void {
        if (Number.isNaN(expected)) check(Number.isNaN(actual), message);
        else if (expected === 0 || !Number.isFinite(expected)) check(Object.is(actual, expected), message);
        else check(Math.abs(actual - expected) <= 4e-15 * Math.abs(expected), message);
    }

    // At -90 degrees elevation and zero roll, only the azimuth half-angle remains.
    // Exercise the actual camera API rather than exporting library functions for tests.
    const angles = [-1e20, -720, -180, -90, -1, 0, 1, 90, 180, 720, 1e20];
    for (const angle of angles) {
        const quaternion = labSurfaceCamera.fromAngles([angle, -90, 0]);
        const half = angle * Math.PI / 360;
        check(quaternion[0] === 0 && quaternion[1] === 0, 'pure azimuth rotation at ' + angle);
        close(quaternion[2], Math.sin(half), 'camera sine at ' + angle);
        close(quaternion[3], Math.cos(half), 'camera cosine at ' + angle);
    }
    for (const angle of [-720, -361, -360, -1, -0, 0, 1, 359, 360, 361, 720]) {
        const input: MarsSurfaceAngles = [angle, 30, 27];
        const decoded = labSurfaceCamera.angles(labSurfaceCamera.fromAngles(input), input);
        const expected = ((angle % 360) + 360) % 360;
        const difference = Math.abs(decoded[0] - expected);
        check(decoded[0] >= 0 && decoded[0] <= 360 && Math.min(difference, 360 - difference) < 1e-10,
              'camera azimuth wraps whole turns and negative angles: ' + angle);
        check(Math.abs(decoded[1] - 30) < 1e-10 && Math.abs(decoded[2] - 27) < 1e-10,
              'angle wrapping preserves elevation and roll');
    }
    // A surface pinch needs only a valid separation here; no chart or range controls are changed.
    // The large finite fixture would overflow if its coordinates were squared before taking a root.
    for (const [x, y] of [[3, 4], [0, 8], [6, 8], [1e200, 1e200], [1e-200, 1e-200],
                          [Infinity, 1], [NaN, 1], [Number.MAX_VALUE, Number.MAX_VALUE]]) {
        const state = {pointerId: 1, touchX: 0, touchY: 0, secondX: x, secondY: y, pan: false};
        const event = {pointerId: 1, clientX: 0, clientY: 0};
        const accepted = labDOM.call('lab_plot_pinch_move', state, event);
        const distance = Math.hypot(x, y);
        check(Boolean(accepted) === (Number.isFinite(distance) && distance >= 8),
              'pinch separation is bounded without intermediate overflow: ' + x + ', ' + y);
    }
    const vectors = [[1, 1], [1, -1], [-1, -1], [-1, 1], [1, 0], [-1, 0], [0, 1], [-0, 1],
                     [0, -1], [-0, -1], [Number.MAX_VALUE, Number.MIN_VALUE],
                     [Number.MIN_VALUE, Number.MAX_VALUE], [Infinity, Infinity], [NaN, 1]];
    for (const [y, x] of vectors)
        close(native.lab_surface_atan2(y, x), Math.atan2(y, x), 'direction at ' + y + ', ' + x);
    for (const x of [0, -0]) {
        for (const y of [0, -0])
            check(Object.is(native.lab_surface_atan2(y, x), 0), 'zero-vector fallback');
    }
    const exponents = [-Infinity, -2000, -1022, -10.5, -1, -0, 0, 0.5, 1, 10.5, 1023, 2000, Infinity, NaN];
    for (const exponent of exponents) {
        const clamped = Math.max(-1022, Math.min(1023, exponent));
        close(native.lab_surface_exp2(exponent), 2 ** clamped, 'binary scale at ' + exponent);
    }
    for (const value of [Number.MIN_VALUE, 2 ** -1022, 0.125, 0.5, 1, 1.5, 2, 3, 1024, Number.MAX_VALUE])
        close(native.lab_surface_log2(value), Math.log2(value), 'binary logarithm at ' + value);
    for (const invalid of [-1, -0, 0, NaN, Infinity, -Infinity])
        check(Object.is(native.lab_surface_log2(invalid), 0), 'invalid ratio returns zero');
    for (let exponent = -1000; exponent <= 1000; exponent += 25)
        check(native.lab_surface_log2(native.lab_surface_exp2(exponent)) === exponent, 'exact power-of-two round trip');

    for (const filename of ['lab_browser.wasm', 'lab_codec.wasm']) {
        const response = await fetch('/wasm/' + filename);
        check(response.ok, filename + ' is available');
        const module = await WebAssembly.compile(await response.arrayBuffer());
        for (const entry of WebAssembly.Module.exports(module)) {
            check(!entry.name.startsWith('protobuf_') && !entry.name.startsWith('pthread_') &&
                  !['malloc', 'calloc', 'realloc', 'free'].includes(entry.name),
                  filename + ' exposes a library implementation detail: ' + entry.name);
        }
        for (const entry of WebAssembly.Module.imports(module)) {
            const allowed = entry.name.startsWith('lab_host_') ||
                (filename === 'lab_browser.wasm' && entry.name.startsWith('lab_dom_'));
            check(entry.kind === 'function' && entry.module === 'env' && allowed,
                  filename + ' has unexpected host import: ' + entry.module + '.' + entry.name);
        }
    }
};
