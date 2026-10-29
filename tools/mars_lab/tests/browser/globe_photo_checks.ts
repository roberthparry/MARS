/**
 * @file globe_photo_checks.ts
 * @brief Geographic coverage and bounded local photographic tile loading checks.
 *
 * Uses real bundled PNGs and WebGL uploads, including seam wrapping, cache reuse,
 * cancellation, unavailable tiles and rejection of obsolete asynchronous completions.
 */
async function checkGlobePhoto(check: (condition: boolean, message: string) => void): Promise<void> {
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
