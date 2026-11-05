/**
 * @file globe_photo_checks.ts
 * @brief Geographic coverage and bounded local photographic tile loading checks.
 *
 * Uses real bundled PNGs and WebGL uploads, including seam wrapping, cache reuse,
 * cancellation, unavailable tiles and rejection of obsolete asynchronous completions.
 * Probes the production photographic lookup against independently interpolated texels
 * at close-view coordinates, with both current cells and cached ancestors.
 * Surveyed land remains corrected through stroke fading, mask replacements and incomplete previews.
 */
async function checkGlobePhoto(check: (condition: boolean, message: string) => void): Promise<void> {
    checkGlobeComposite(check);
    const plan = (latitude: number, longitude: number): number[] =>
        labDOM.call('lab_globe_photo_patch', latitude, longitude) as number[];
    for (const [latitude, longitude] of [[0, 0], [52, -3], [-33, 18], [89, 179.9], [-89, -179.9]]) {
        const patch = plan(latitude, longitude);
        const wrapped = patch[3] + ((longitude - patch[3] + 360) % 360);
        check(patch[0] >= 0 && patch[0] < 12 && patch[1] === (patch[0] + 1) % 12 &&
              patch[2] >= 0 && patch[2] <= 4 && patch[5] - patch[3] === 60 && patch[6] - patch[4] === 60 &&
              wrapped >= patch[3] && wrapped <= patch[5] && latitude >= patch[4] && latitude <= patch[6],
              'photo patch covers its centre at ordinary, polar and date-line locations');
    }
    check(!labDOM.call('lab_globe_photo_patch', 91, 0) && !labDOM.call('lab_globe_photo_patch', 0, NaN),
          'invalid photographic coordinates are rejected');
    const moduleURL = '/js/globe_photo.js';
    const {GlobePhoto} = await import(moduleURL) as typeof import('../../typescript/globe_photo.js');
    const canvas = document.createElement('canvas'), gl = canvas.getContext('webgl')!;
    check(Boolean(gl), 'photo test obtains a GPU context');
    if (gl.getParameter(gl.MAX_TEXTURE_SIZE) < 3600) {
        gl.getExtension('WEBGL_lose_context')?.loseContext();
        return;
    }
    let published = 0, downloads = 0;
    const photo = new GlobePhoto(gl, () => ++published);
    const originalFetch = window.fetch;
    window.fetch = (input, init) => {
        if (String(input).startsWith('/earth_tiles/')) ++downloads;
        return originalFetch(input, init);
    };
    try {
        const europe = plan(52, -3);
        const started = performance.now();
        await photo.load(europe);
        check(photo.ready && published === 1 && downloads === 4 && photo.bounds.join() === europe.slice(3).join(),
              `four local source tiles publish one complete full-resolution patch (ready=${photo.ready}, published=${published}, downloads=${downloads}, ` +
              `cache=${(photo as unknown as {cache: {size: number}}).cache.size}, elapsed=${Math.round(performance.now() - started)}ms, GL=${gl.getError()})`);
        await photo.load(europe);
        check(downloads === 4 && published === 1, 'unchanged view does not re-decode or re-upload tiles');
        const state = photo as unknown as {cache: {readonly size: number}; texture: WebGLTexture | null};
        check(state.cache.size === 4 && state.texture !== null, 'decoded image cache is bounded to four tiles');
        const framebuffer = gl.createFramebuffer()!;
        gl.bindFramebuffer(gl.FRAMEBUFFER, framebuffer);
        gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, state.texture, 0);
        check(gl.checkFramebufferStatus(gl.FRAMEBUFFER) === gl.FRAMEBUFFER_COMPLETE,
              'published photographic texture is complete');
        const sample = new Uint8Array(4);
        gl.readPixels(1800, 1800, 1, 1, gl.RGBA, gl.UNSIGNED_BYTE, sample);
        check(sample[3] === 255 && sample[0] + sample[1] + sample[2] > 0, 'real imagery reaches the GPU');
        gl.bindFramebuffer(gl.FRAMEBUFFER, null);
        gl.deleteFramebuffer(framebuffer);

        let release: (() => void) | undefined;
        window.fetch = async (input, init) => {
            if (String(input).startsWith('/earth_tiles/')) {
                await new Promise<void>(resolve => { release = resolve; });
                // Deliberately ignore abort, exercising late decode ownership too.
                return originalFetch(input);
            }
            return originalFetch(input, init);
        };
        const obsolete = photo.load(plan(-33, 120));
        check(Boolean(release), 'a replacement download is pending');
        photo.cancel();
        release!();
        await obsolete;
        check(published === 1 && photo.bounds.join() === europe.slice(3).join(),
              'cancelled late imagery cannot replace the displayed patch');
        window.fetch = async () => new Response('', {status: 503});
        await photo.load(plan(-33, 120));
        check(published === 1 && photo.ready && state.cache.size <= 4 && gl.getError() === gl.NO_ERROR,
              'missing tiles preserve valid existing imagery and bounded memory');
    } finally {
        window.fetch = originalFetch;
        photo.dispose();
        gl.getExtension('WEBGL_lose_context')?.loseContext();
    }
}

/** @brief Probe the production shader through mask handoffs and retained photographic patch edges. */
function checkGlobeComposite(check: (condition: boolean, message: string) => void): void {
    const canvas = document.createElement('canvas');
    canvas.width = canvas.height = 1;
    const gl = canvas.getContext('webgl', {antialias: false, premultipliedAlpha: false})!;
    check(Boolean(gl), 'photographic handoff probe obtains WebGL');
    const program = gl.createProgram()!, shaders: WebGLShader[] = [], textures: WebGLTexture[] = [];
    const buffer = gl.createBuffer()!;
    try {
        for (const [fragment, type] of [[0, gl.VERTEX_SHADER], [1, gl.FRAGMENT_SHADER]]) {
            const shader = gl.createShader(type)!;
            shaders.push(shader);
            let source = labDOM.call('lab_globe_shader', fragment, 0) as string;
            if (fragment) source = source.replace('void main(){', 'void globe_original_main(){') +
                '\nuniform bool geographicSample; uniform vec2 sampleLocation;\n' +
                'void main(){if(geographicSample) gl_FragColor=vec4(mapColour(vec3(70.,85.,34.)/255.,sampleLocation),1.);' +
                'else globe_original_main();}\n';
            gl.shaderSource(shader, source);
            gl.compileShader(shader);
            check(Boolean(gl.getShaderParameter(shader, gl.COMPILE_STATUS)), 'photographic handoff shader compiles');
            gl.attachShader(program, shader);
        }
        gl.linkProgram(program);
        check(Boolean(gl.getProgramParameter(program, gl.LINK_STATUS)), 'photographic handoff shader links');
        gl.useProgram(program);
        gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
        gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([-1, -1, 1, -1, -1, 1, -1, 1, 1, -1, 1, 1]), gl.STATIC_DRAW);
        const position = gl.getAttribLocation(program, 'position');
        gl.enableVertexAttribArray(position);
        gl.vertexAttribPointer(position, 2, gl.FLOAT, false, 0, 0);
        for (const [unit, name, rgba] of [[0, 'world', [80, 100, 40, 255]],
                                         [1, 'rivers', [180, 150, 80, 255]]] as const) {
            const texture = gl.createTexture()!;
            textures.push(texture);
            gl.activeTexture(gl.TEXTURE0 + unit);
            gl.bindTexture(gl.TEXTURE_2D, texture);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
            gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE, new Uint8Array(rgba));
            gl.uniform1i(gl.getUniformLocation(program, name), unit);
        }
        gl.uniform3f(gl.getUniformLocation(program, 'camera'), 0, 0, 16);
        gl.uniformMatrix3fv(gl.getUniformLocation(program, 'cameraToWorld'), false, [1, 0, 0, 0, 1, 0, 0, 0, 1]);
        gl.uniform1f(gl.getUniformLocation(program, 'viewport'), 1);
        gl.uniform1i(gl.getUniformLocation(program, 'vectorReady'), 1);
        gl.uniform1i(gl.getUniformLocation(program, 'naturalStyle'), 1);
        const pixel = () => {
            gl.drawArrays(gl.TRIANGLES, 0, 6);
            const result = new Uint8Array(4);
            gl.readPixels(0, 0, 1, 1, gl.RGBA, gl.UNSIGNED_BYTE, result);
            return result;
        };
        const base = pixel(), light = .8 + .2 / Math.hypot(.35, .45, 1);
        check(base[3] === 255 && [80, 100, 40].every((value, i) => Math.abs(base[i] - value * light) <= 2),
              'a retained vector frame without a GPU coastline mask keeps opaque photographic colour');
        gl.uniform1i(gl.getUniformLocation(program, 'riverReady'), 1);
        gl.uniform1i(gl.getUniformLocation(program, 'riverComposite'), 1);
        gl.uniform1f(gl.getUniformLocation(program, 'riverBlend'), 1);
        gl.uniform1f(gl.getUniformLocation(program, 'riverInk'), 1);
        let previous = base;
        for (const u of [-.005, 0, .01, .02, .03, .04, .5]) {
            gl.uniform4f(gl.getUniformLocation(program, 'riverBounds'), -u, -1, 1 - u, 1);
            const next = pixel();
            check(next[3] === 255 && next[0] >= previous[0] && next[0] - previous[0] <= 37,
                  'retained patch edges blend continuously into the underlying photograph');
            if (u <= 0) check(next.every((value, i) => Math.abs(value - base[i]) <= 1),
                              'outside and on the patch boundary preserve the original photograph');
            previous = next;
        }
        check(Math.abs(previous[0] - 180 * light) <= 2, 'patch interiors retain full regional detail');
        gl.uniformMatrix3fv(gl.getUniformLocation(program, 'cameraToWorld'), false, [-1, 0, 0, 0, 1, 0, 0, 0, -1]);
        gl.uniform4f(gl.getUniformLocation(program, 'riverBounds'), 179, -1, 181, 1);
        check(pixel().every((value, i) => Math.abs(value - previous[i]) <= 1),
              'photographic blending retains date-line wrapping');
        const outgoing = gl.createTexture()!;
        textures.push(outgoing);
        gl.activeTexture(gl.TEXTURE2);
        gl.bindTexture(gl.TEXTURE_2D, outgoing);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
        gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE,
                      new Uint8Array([40, 180, 160, 255]));
        gl.uniform1i(gl.getUniformLocation(program, 'previousRivers'), 2);
        gl.uniform1i(gl.getUniformLocation(program, 'previousRiverReady'), 1);
        gl.uniform4f(gl.getUniformLocation(program, 'previousRiverBounds'), -1, -1, 1, 1);
        gl.uniform4f(gl.getUniformLocation(program, 'riverBounds'), -1, -1, 1, 1);
        gl.uniformMatrix3fv(gl.getUniformLocation(program, 'cameraToWorld'), false, [1, 0, 0, 0, 1, 0, 0, 0, 1]);
        for (const time of [0, .25, .5, .75, 1]) {
            gl.uniform1f(gl.getUniformLocation(program, 'riverBlend'), time);
            const colour = pixel(), weight = time * time * (3 - 2 * time);
            check(colour[3] === 255 && [40, 180, 160].every((value, i) =>
                Math.abs(colour[i] - (value * (1 - weight) + [180, 150, 80][i] * weight) * light) <= 2),
                'actual GPU pixels blend monotonically between outgoing and incoming photographs');
        }
        gl.uniform4f(gl.getUniformLocation(program, 'riverBounds'), 1, -1, 2, 1);
        check(pixel().every((value, i) => i === 3 ? value === 255 : Math.abs(value - [40, 180, 160][i] * light) <= 2),
              'outgoing detail remains visible beyond the replacement tile instead of flashing to the world overview');
        const page = gl.createTexture()!;
        textures.push(page);
        gl.activeTexture(gl.TEXTURE7); gl.bindTexture(gl.TEXTURE_2D, page);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
        const addresses = new Uint8Array(64 * 64 * 4);
        for (let i = 0; i < addresses.length; i += 4) addresses[i] = 1;
        gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 64, 64, 0, gl.RGBA, gl.UNSIGNED_BYTE, addresses);
        gl.uniform1i(gl.getUniformLocation(program, 'photoPatch'), 7);
        gl.uniform1i(gl.getUniformLocation(program, 'photoAtlasEnabled'), 1);
        gl.uniform1i(gl.getUniformLocation(program, 'photoAtlasReady'), 1);
        gl.uniform1i(gl.getUniformLocation(program, 'previousRiverReady'), 0);
        gl.uniform4f(gl.getUniformLocation(program, 'photoAtlasBounds'), -1, -1, 1, 1);
        gl.uniform1f(gl.getUniformLocation(program, 'photoAtlasSpan'), .125);
        gl.uniform1f(gl.getUniformLocation(program, 'photoAtlasColumns'), 1);
        for (const blend of [0, .5, 1]) {
            gl.uniform1f(gl.getUniformLocation(program, 'riverBlend'), blend);
            check(pixel().every((value, i) => i === 3 ? value === 255 : Math.abs(value - [40, 180, 160][i] * light) <= 2),
                  'fixed photographic pixels remain sharp through river-mask and coarse-preview handoffs');
        }
        gl.activeTexture(gl.TEXTURE2);
        gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE,
                      new Uint8Array([20, 90, 80, 128]));
        check(pixel().every((value, i) => i === 3 ? value === 255 :
            Math.abs(value - ([80, 100, 40][i] * (1 - 128 / 255) + [20, 90, 80][i]) * light) <= 2),
            'transparent regional shore edges blend with the resident world image using premultiplied pixels');
        gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE, new Uint8Array(4));
        check(pixel().every((value, i) => i === 3 ? value === 255 : Math.abs(value - base[i]) <= 1),
              'unobserved regional gaps retain the world image without decoding a separate NASA underlay');
        const joined = new Uint8Array(1024 * 1024 * 4);
        for (let y = 0; y < 512; ++y) {
            for (let x = 0; x < 1024; ++x) {
                const offset = (y * 1024 + x) * 4;
                joined[offset] = x >= 511 && x !== 512 ? 255 : 0;
                joined[offset + 3] = 255;
            }
        }
        gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1024, 1024, 0, gl.RGBA, gl.UNSIGNED_BYTE, joined);
        for (let y = 0; y < 16; ++y) for (let x = 0; x < 16; ++x) {
            const at = (y * 16 + x) * 4;
            addresses[at] = x < 8 ? 1 : 2;
            addresses[at + 1] = 3;
        }
        gl.activeTexture(gl.TEXTURE7);
        gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 64, 64, 0, gl.RGBA, gl.UNSIGNED_BYTE, addresses);
        gl.uniform1f(gl.getUniformLocation(program, 'photoAtlasColumns'), 2);
        const seam: number[] = [];
        for (const longitude of [-.00002, .00002]) {
            const angle = longitude * Math.PI / 180;
            gl.uniformMatrix3fv(gl.getUniformLocation(program, 'cameraToWorld'), false,
                [0, 0, 0, 0, 0, 0, Math.sin(angle), 0, Math.cos(angle)]);
            seam.push(pixel()[0]);
        }
        check(seam.every(value => value > 100 && value < 150) && Math.abs(seam[0] - seam[1]) < 24,
              `geographic gutters interpolate across adjacent atlas cells without a bright or dark seam (${seam})`);
        for (const [unit, name, rgba] of [[3, 'shoreMask', [255, 0, 0, 255]],
                                         [5, 'landPalette', [80, 100, 40, 255]],
                                         [6, 'waterPalette', [7, 20, 64, 255]]] as const) {
            const texture = gl.createTexture()!;
            textures.push(texture);
            gl.activeTexture(gl.TEXTURE0 + unit);
            gl.bindTexture(gl.TEXTURE_2D, texture);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
            gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE, new Uint8Array(rgba));
            gl.uniform1i(gl.getUniformLocation(program, name), unit);
        }
        gl.uniform1i(gl.getUniformLocation(program, 'riverReady'), 0);
        gl.uniform1i(gl.getUniformLocation(program, 'naturalShore'), 1);
        gl.uniform1i(gl.getUniformLocation(program, 'palettesReady'), 1);
        gl.uniform1f(gl.getUniformLocation(program, 'shoreBlend'), 1);
        gl.uniform4f(gl.getUniformLocation(program, 'shoreBounds'), -1, -1, 1, 1);
        gl.uniform4f(gl.getUniformLocation(program, 'shoreTransform'), 1, 0, 0, 1);
        gl.uniform3f(gl.getUniformLocation(program, 'shoreOffset'), 0, 0, 1);
        gl.uniform1f(gl.getUniformLocation(program, 'photoAtlasColumns'), 1);
        gl.uniformMatrix3fv(gl.getUniformLocation(program, 'cameraToWorld'), false, [1, 0, 0, 0, 1, 0, 0, 0, 1]);
        gl.activeTexture(gl.TEXTURE7);
        for (let i = 0; i < addresses.length; i += 4) { addresses[i] = 1; addresses[i + 1] = 0; }
        gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 64, 64, 0, gl.RGBA, gl.UNSIGNED_BYTE, addresses);
        for (const rgb of [[96, 96, 96], [150, 150, 150], [160, 150, 140]]) {
            gl.activeTexture(gl.TEXTURE2);
            gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE,
                          new Uint8Array([...rgb, 255]));
            const actual = pixel();
            check(actual[3] === 255 && rgb.every((value, i) => Math.abs(actual[i] - value * light) <= 2),
                  `surveyed land preserves neutral roofs and roads instead of the blurred green palette (${rgb}: ${actual})`);
        }
        for (const [land, rgb, expected] of [[true, [40, 80, 160], [80, 100, 40]],
                                            [false, [40, 80, 160], [40, 80, 160]],
                                            [false, [96, 96, 96], [7, 20, 64]]] as const) {
            gl.activeTexture(gl.TEXTURE3);
            gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE,
                          new Uint8Array([land ? 255 : 0, 0, 0, 255]));
            gl.activeTexture(gl.TEXTURE2);
            gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE,
                          new Uint8Array([...rgb, 255]));
            const actual = pixel();
            check(actual[3] === 255 && expected.every((value, i) => Math.abs(actual[i] - value * light) <= 2),
                  `shoreline correction retains water colours and repairs genuine surface mismatches (${land}: ${actual})`);
        }
        const photograph = new Uint8Array(512 * 512 * 4);
        for (let y = 0; y < 512; ++y) for (let x = 0; x < 512; ++x) {
            const at = (y * 512 + x) * 4;
            photograph[at] = x % 13 < 6 ? 48 : 176;
            photograph[at + 1] = y % 11 < 5 ? 80 : 208;
            photograph[at + 2] = 16;
            photograph[at + 3] = 255;
        }
        gl.activeTexture(gl.TEXTURE2);
        gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 512, 512, 0, gl.RGBA, gl.UNSIGNED_BYTE, photograph);
        gl.uniform1i(gl.getUniformLocation(program, 'geographicSample'), 1);
        let maximumError = 0, worstSample = '';
        for (const [longitude, latitude] of [[-2.7541, 52.7077], [151.21, -33.87], [179.9, 10], [-179.9, -20]]) {
            const lon = Math.fround(longitude), lat = Math.fround(latitude);
            for (const level of [14, 18]) {
                const span = 360 / 2 ** level;
                const left = Math.floor((lon + 180) / span) - 8, top = Math.floor((90 - lat) / span) - 8;
                gl.uniform4f(gl.getUniformLocation(program, 'photoAtlasBounds'),
                    left * span - 180, 90 - (top + 16) * span, (left + 16) * span - 180, 90 - top * span);
                gl.uniform1f(gl.getUniformLocation(program, 'photoAtlasSpan'), span);
                gl.uniform2f(gl.getUniformLocation(program, 'sampleLocation'), lon, lat);
                for (const ancestor of [0, 3]) {
                    for (let i = 0; i < addresses.length; i += 4) addresses[i + 1] = ancestor;
                    gl.activeTexture(gl.TEXTURE7);
                    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 64, 64, 0, gl.RGBA, gl.UNSIGNED_BYTE, addresses);
                    const extent = span * 2 ** ancestor;
                    const x = .5 + ((lon + 180) / extent % 1) * 510;
                    const y = .5 + ((90 - lat) / extent % 1) * 510;
                    const ix = Math.floor(x), iy = Math.floor(y), fx = x - ix, fy = y - iy;
                    const expected = [0, 1, 2].map(channel => {
                        const a = photograph[(iy * 512 + ix) * 4 + channel];
                        const b = photograph[(iy * 512 + ix + 1) * 4 + channel];
                        const c = photograph[((iy + 1) * 512 + ix) * 4 + channel];
                        const d = photograph[((iy + 1) * 512 + ix + 1) * 4 + channel];
                        return (a + (b - a) * fx) * (1 - fy) + (c + (d - c) * fx) * fy;
                    });
                    const actual = pixel();
                    check(actual[3] === 255, 'precision probes retain opaque photographic coverage');
                    const error = Math.max(...expected.map((value, i) => Math.abs(actual[i] - value)));
                    if (error > maximumError) {
                        maximumError = error;
                        worstSample = `${lon},${lat}, level ${level}, ancestor ${ancestor}, ` +
                            `actual ${actual}, expected ${expected}`;
                    }
                }
            }
        }
        // Allow byte rounding in GPU interpolation, while rejecting sampling from a different source texel.
        check(maximumError <= 3.01, `GPU photographic addressing retains subpixel detail: ${maximumError}; ${worstSample}`);
        const cache = labDOM.call('lab_texture_cache', {action: 0, limit: 64, bytes: 64 * 1024 * 1024}) as {token: number};
        try {
            const close = labDOM.call('lab_texture_cache', {action: 6, token: cache.token,
                bounds: [-2.76, 52.70, -2.75, 52.71], pixels: 800}) as {
                    next: {key: string; bounds: number[]; level: number};
                };
            const fine = close.next;
            labDOM.call('lab_texture_cache', {action: 2, token: cache.token, key: fine.key,
                bounds: fine.bounds, zoom: fine.level, bytes: 512 * 512 * 4});
            const lon = Math.fround((fine.bounds[0] + fine.bounds[2]) / 2);
            const lat = Math.fround((fine.bounds[1] + fine.bounds[3]) / 2);
            const extent = (fine.bounds[2] - fine.bounds[0]) * 64;
            const wide = labDOM.call('lab_texture_cache', {action: 6, token: cache.token,
                bounds: [lon - extent, lat - extent, lon + extent, lat + extent], pixels: 200}) as {
                    bounds: number[]; span: number; pointer: number;
                };
            gl.activeTexture(gl.TEXTURE7);
            gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 64, 64, 0, gl.RGBA, gl.UNSIGNED_BYTE,
                new Uint8Array(labWire.exports().memory.buffer, wide.pointer, 64 * 64 * 4));
            gl.uniform4fv(gl.getUniformLocation(program, 'photoAtlasBounds'), wide.bounds);
            gl.uniform1f(gl.getUniformLocation(program, 'photoAtlasSpan'), wide.span);
            gl.uniform2f(gl.getUniformLocation(program, 'sampleLocation'), lon, lat);
            const actual = pixel();
            check(actual[3] === 255 && actual[2] === 16 && Math.abs(actual[0] - 176) <= 3 && actual[1] === 80,
                  `native sparse photographic descendants reach the GPU after a large zoom-out (${actual})`);
            gl.uniform2f(gl.getUniformLocation(program, 'sampleLocation'), lon + extent / 4, lat);
            const outside = pixel();
            check(outside[3] === 255 && [80, 100, 40].every((value, i) => Math.abs(outside[i] - value) <= 1),
                  'a sparse child never stretches its photograph across an uncached sibling');
        } finally { labDOM.call('lab_texture_cache', {action: 4, token: cache.token}); }
        const dense = labDOM.call('lab_texture_cache', {action: 0, limit: 64, bytes: 64 * 1024 * 1024}) as {token: number};
        try {
            const view = [-2.8, 46, 5.2, 54];
            const request = {action: 6, token: dense.token, bounds: view, pixels: 1536};
            type DensePage = {bounds: number[]; span: number; pointer: number;
                next?: {key: string; bounds: number[]; level: number}};
            let page = labDOM.call('lab_texture_cache', request) as DensePage;
            for (let count = 0; count < 65 && page.next; ++count) {
                const next = page.next;
                labDOM.call('lab_texture_cache', {action: 2, token: dense.token, key: next.key,
                    bounds: next.bounds, zoom: next.level, bytes: 512 * 512 * 4});
                page = labDOM.call('lab_texture_cache', request) as DensePage;
            }
            check(!page.next, 'the high-density viewport finishes its bounded photographic requests');
            gl.activeTexture(gl.TEXTURE7);
            gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 64, 64, 0, gl.RGBA, gl.UNSIGNED_BYTE,
                new Uint8Array(labWire.exports().memory.buffer, page.pointer, 64 * 64 * 4));
            gl.activeTexture(gl.TEXTURE2);
            // A uniform atlas isolates spatial admission from the already tested per-slot pixel addressing.
            gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE,
                new Uint8Array([144, 96, 48, 255]));
            gl.uniform1f(gl.getUniformLocation(program, 'photoAtlasColumns'), 8);
            gl.uniform4fv(gl.getUniformLocation(program, 'photoAtlasBounds'), page.bounds);
            gl.uniform1f(gl.getUniformLocation(program, 'photoAtlasSpan'), page.span);
            for (const lon of [view[0] + .01, (view[0] + view[2]) / 2, view[2] - .01]) {
                for (const lat of [view[1] + .01, (view[1] + view[3]) / 2, view[3] - .01]) {
                    gl.uniform2f(gl.getUniformLocation(program, 'sampleLocation'), lon, lat);
                    const actual = pixel();
                    check(actual[3] === 255 && [144, 96, 48].every((value, i) => Math.abs(actual[i] - value) <= 1),
                          `high-density photography reaches every viewport edge and corner (${lon},${lat}: ${actual})`);
                }
            }
        } finally { labDOM.call('lab_texture_cache', {action: 4, token: dense.token}); }
        gl.uniform1i(gl.getUniformLocation(program, 'geographicSample'), 0);
        gl.uniform1i(gl.getUniformLocation(program, 'photoAtlasEnabled'), 1);
        gl.uniform1i(gl.getUniformLocation(program, 'photoAtlasReady'), 0);
        gl.uniformMatrix3fv(gl.getUniformLocation(program, 'cameraToWorld'), false, [1, 0, 0, 0, 1, 0, 0, 0, 1]);
        gl.uniform1i(gl.getUniformLocation(program, 'naturalShore'), 1);
        gl.uniform1f(gl.getUniformLocation(program, 'shoreBlend'), 1);
        gl.uniform4f(gl.getUniformLocation(program, 'shoreBounds'), -1, -1, 1, 1);
        gl.uniform4f(gl.getUniformLocation(program, 'shoreTransform'), 1, 0, 0, 1);
        gl.uniform3f(gl.getUniformLocation(program, 'shoreOffset'), 0, 0, 1);
        gl.activeTexture(gl.TEXTURE3);
        gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE,
                      new Uint8Array([0, 0, 0, 255]));
        gl.activeTexture(gl.TEXTURE1);
        gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE,
                      new Uint8Array([128, 128, 128, 128]));
        gl.uniform1i(gl.getUniformLocation(program, 'riverReady'), 1);
        gl.uniform1i(gl.getUniformLocation(program, 'riverComposite'), 1);
        gl.uniform1i(gl.getUniformLocation(program, 'riverCorrectionsReady'), 1);
        gl.uniform1i(gl.getUniformLocation(program, 'previousRiverReady'), 0);
        gl.uniform4f(gl.getUniformLocation(program, 'riverBounds'), -1, -1, 1, 1);
        gl.uniform1f(gl.getUniformLocation(program, 'riverInk'), 1);
        gl.uniform1f(gl.getUniformLocation(program, 'riverBlend'), 1);
        const correctedLand = pixel();
        check(correctedLand[3] === 255 && [80, 100, 40].every((value, i) =>
            Math.abs(correctedLand[i] - value * light) <= 2), 'surveyed land replaces an obsolete coarse lake mask');
        for (const ink of [.5, 0]) {
            gl.uniform1f(gl.getUniformLocation(program, 'riverInk'), ink);
            const actual = pixel();
            check(actual.every((value, i) => Math.abs(value - correctedLand[i]) <= 1),
                  `surveyed land survives zoom-dependent river-stroke fading (${ink}: ${actual})`);
        }
        for (const blend of [0, .25, .5, 1]) {
            gl.uniform1f(gl.getUniformLocation(program, 'riverBlend'), blend);
            const actual = pixel();
            check(actual.every((value, i) => Math.abs(value - correctedLand[i]) <= 1),
                  `atlas mask replacements never fade surveyed land back to the coarse lake (${blend}: ${actual})`);
        }
        gl.uniform1i(gl.getUniformLocation(program, 'riverCorrectionsReady'), 0);
        const previewLand = pixel();
        check(previewLand[3] === 255 && [7, 20, 64].every((value, i) =>
            Math.abs(previewLand[i] - value * light) <= 2), 'photographic previews cannot declare surveyed land');
        gl.uniform1i(gl.getUniformLocation(program, 'riverCorrectionsReady'), 1);
        gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE,
                      new Uint8Array([80, 100, 40, 255]));
        check(pixel().every((value, i) => Math.abs(value - previewLand[i]) <= 1),
              'completed masks cannot infer land outside their recorded survey coverage');
        gl.uniform1i(gl.getUniformLocation(program, 'riverReady'), 0);
        gl.uniform1i(gl.getUniformLocation(program, 'previousRiverReady'), 1);
        gl.uniform1i(gl.getUniformLocation(program, 'previousRiverCorrectionsReady'), 1);
        gl.uniform1f(gl.getUniformLocation(program, 'previousRiverInk'), 0);
        gl.uniform1i(gl.getUniformLocation(program, 'photoAtlasEnabled'), 0);
        gl.activeTexture(gl.TEXTURE2);
        gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE,
                      new Uint8Array([40, 50, 20, 128]));
        const previousLand = pixel();
        check(previousLand.every((value, i) => Math.abs(value - correctedLand[i]) <= 1),
              'a retained completed mask preserves surveyed land independently of its faded river strokes');
        gl.uniform1i(gl.getUniformLocation(program, 'riverReady'), 1);
        gl.uniform1i(gl.getUniformLocation(program, 'riverCorrectionsReady'), 0);
        gl.activeTexture(gl.TEXTURE1);
        gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE,
                      new Uint8Array([40, 50, 20, 128]));
        check(pixel().every((value, i) => Math.abs(value - previousLand[i]) <= 1),
              'an unfinished replacement preview preserves the retained surveyed land correction');
        check(gl.getError() === gl.NO_ERROR, 'photographic handoffs complete without GPU errors');
    } finally {
        textures.forEach(texture => gl.deleteTexture(texture));
        shaders.forEach(shader => gl.deleteShader(shader));
        gl.deleteBuffer(buffer); gl.deleteProgram(program);
        gl.getExtension('WEBGL_lose_context')?.loseContext();
    }
}
