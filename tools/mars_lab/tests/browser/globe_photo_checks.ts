/**
 * @file globe_photo_checks.ts
 * @brief Geographic coverage and bounded local photographic tile loading checks.
 *
 * Uses real bundled PNGs and WebGL uploads, including seam wrapping, cache reuse,
 * cancellation, unavailable tiles and rejection of obsolete asynchronous completions.
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
            gl.shaderSource(shader, labDOM.call('lab_globe_shader', fragment, 0) as string);
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
        check(gl.getError() === gl.NO_ERROR, 'photographic handoffs complete without GPU errors');
    } finally {
        textures.forEach(texture => gl.deleteTexture(texture));
        shaders.forEach(shader => gl.deleteShader(shader));
        gl.deleteBuffer(buffer); gl.deleteProgram(program);
        gl.getExtension('WEBGL_lose_context')?.loseContext();
    }
}
