/**
 * @file globe_atlas_checks.ts
 * @brief Fixed-cell geography, multi-source cache pressure and real GPU upload regressions.
 *
 * Exercises the production WASM policy and photographic worker with two independently addressed 3600-square
 * source photographs, exceeding the former decoded-image budget. Revisits must use small raw source blocks.
 */
async function checkGlobeAtlas(check: (condition: boolean, message: string) => void): Promise<void> {
    const cacheURL = '/js/globe_texture_cache.js';
    const {GlobeTextureCache} = await import(cacheURL) as typeof import('../../typescript/globe_texture_cache.js');
    const spatial = new GlobeTextureCache<number>(64, 64 * 1024 * 1024, () => {});
    try {
        const view = [-2.82, 52.62, -2.68, 52.70];
        const low = spatial.page(view, 400)!;
        check(Boolean(low?.next), 'a fixed photographic page supplies a centre-first missing geographic cell');
        spatial.put(low.next!.key, 1, 512 * 512 * 4, low.next!.bounds, low.next!.level);
        const high = spatial.page(view, 800)!;
        const pixels = new Uint8Array(labWire.exports().memory.buffer, high.pointer, 256).slice();
        check(pixels.some((value, i) => i % 4 === 0 && value > 0),
              'zooming towards finer cells retains a resident lower-resolution photographic ancestor');
        const moving = spatial.page([-2.81999, 52.62001, -2.67999, 52.70001], 800)!;
        check(moving.bounds.join() === high.bounds.join() && spatial.get(low.next!.key) === 1,
              'small pans preserve geographic cell keys and their GPU resource slots');
        let admitted = 1;
        for (; admitted < 32; ++admitted) {
            const plan = spatial.page(view, 400)!;
            if (!plan.next) break;
            const bounds = plan.next.bounds, step = bounds[2] - bounds[0];
            check(bounds[0] <= view[2] + step * .5 && bounds[2] >= view[0] - step * .5 &&
                  bounds[3] >= view[1] - step * .5 && bounds[1] <= view[3] + step * .5,
                  'prefetch remains within half a photographic cell of the current view');
            spatial.put(plan.next.key, admitted + 1, 512 * 512 * 4, bounds, plan.next.level);
        }
        check(admitted < 32, 'stationary views stop requesting cells after filling their bounded neighbourhood');
        check(!spatial.page([-180, -90, 180, 90], 800) && !spatial.page([0, 0, 1, 1], NaN),
              'continental and invalid pages fail safely without unbounded tile admission');
        for (const bounds of [[179.9, -.1, 180.1, .1], [-180.1, -.1, -179.9, .1], [1, 89.8, 2, 90]]) {
            const page = spatial.page(bounds, 800)!;
            check(Boolean(page?.next) && page.bounds[0] <= bounds[0] && page.bounds[1] <= bounds[1] &&
                  page.bounds[2] >= bounds[2] && page.bounds[3] >= bounds[3],
                  'fixed geographic pages cover polar and date-line views');
        }
    } finally { spatial.dispose(); }
    const source = {url: '/earth_regions/01.png', sha256: 'a'.repeat(64), width: 3600,
        source: [500, 500, 40, 40], target: [0, 0, 512, 512]};
    const parts = labDOM.call('lab_photo_source', source) as {needed: number; target?: number[]; key: string}[];
    const needed = parts.filter(part => part.needed);
    check(parts.length === 64 && needed.length === 4 &&
          needed.reduce((area, part) => area + part.target![2] * part.target![3], 0) === 512 * 512,
          'source-cell gutters retain exact coverage where a crop crosses four decoded blocks');
    check(parts[0].key.endsWith(':0:0:') && new Set(parts.map(part => part.key)).size === 64,
          'source keys preserve zero coordinates and uniquely address every decoded block');
    const shifted = labDOM.call('lab_photo_source', {...source, source: [501, 501, 40, 40]}) as typeof parts;
    check(shifted.filter(part => part.needed).every((part, i) => part.key === needed[i].key),
          'source block identities are independent of camera-dependent crop rectangles');

    const image = await fetch('/earth_regions/01.png');
    const digest = image.headers.get('ETag')!.replace(/^"sha256-|"$/g, '');
    const bitmap = await createImageBitmap(new Blob([await image.arrayBuffer()], {type: 'image/png'}));
    const reference = document.createElement('canvas');
    reference.width = reference.height = 512;
    const context = reference.getContext('2d')!;
    context.imageSmoothingQuality = 'high';
    context.drawImage(bitmap, 1200, 900, 1024, 1024, 0, 0, 512, 512);
    context.drawImage(bitmap, 1288, 988, 1024, 1024, 0, 0, 512, 512);
    const expected = context.getImageData(256, 256, 1, 1).data;
    bitmap.close(); reference.width = reference.height = 1;
    let plans = 0;
    const respond = async (bytes: Uint8Array<ArrayBuffer>) => {
        const payload = labWire.decode(bytes) as {west: number; south: number; format: string};
        check(payload.format === 'photo-plan', 'photographic cells request no river geometry');
        ++plans;
        const offset = payload.west > 0 ? 128 : 0;
        return new Response(labWire.encode({ok: true, key: `atlas-source-test:${payload.west}:${payload.south}`, photos: [
            {url: '/earth_regions/123.png', sha256: digest, width: 3600,
             source: [1200 + offset, 900, 1024, 1024], target: [0, 0, 512, 512]},
            {url: '/earth_regions/4095.png', sha256: digest, width: 3600,
             source: [1288 + offset, 988, 1024, 1024], target: [0, 0, 512, 512]}
        ]}));
    };
    const OriginalWorker = globalThis.Worker;
    const fixture = globeHTTPFixture(respond, {endpoint: '/almanac-rivers', script: '/js/globe_photo_worker.js',
        nativeShore: false, surfaceProbe: true, regionalAliases: true});
    let worker: Worker | null = null;
    const receive = (key: string, west: number) => new Promise<ArrayBuffer>((resolve, reject) => {
        const timeout = window.setTimeout(() => reject(new Error('Photographic worker timed out')), 90000);
        worker!.onmessage = event => {
            if (event.data.key !== key) return;
            window.clearTimeout(timeout);
            event.data.error ? reject(new Error(event.data.error)) : resolve(event.data.pixels as ArrayBuffer);
        };
        worker!.onerror = event => {
            window.clearTimeout(timeout); event.preventDefault(); reject(new Error(event.message));
        };
        worker!.postMessage({key, level: 10, bounds: [west, 0, west + .1, .1]});
    });
    try {
        worker = new fixture.Worker('/js/globe_photo_worker.js');
        fixture.releaseSurfaceCache();
        const first = await receive('first', 0);
        const firstPixels = new Uint8Array(first);
        const centre = (256 * 512 + 256) * 4;
        check(first.byteLength === 512 * 512 * 4 &&
              expected.every((value, i) => Math.abs(firstPixels[centre + i] - value) <= 3),
              'two full 3600-square sources produce accurate 512-pixel GPU-ready geography');
        const second = await receive('adjacent', .01);
        check(second.byteLength === first.byteLength && plans === 2,
              'an adjacent geographic cell reuses both source photographs without fetching or decoding either again');
    } finally { worker?.terminate(); fixture.dispose(); }

    const persistent = globeHTTPFixture(respond, {endpoint: '/almanac-rivers', script: '/js/globe_photo_worker.js',
        nativeShore: false, denyPhotographs: true, regionalAliases: true});
    try {
        worker = new persistent.Worker('/js/globe_photo_worker.js');
        const revisit = await receive('persisted-source', .02);
        check(revisit.byteLength === 512 * 512 * 4,
              'a new worker renders another cell from persisted raw source blocks with all PNG reads forbidden');
    } finally { worker?.terminate(); persistent.dispose(); }

    const fixtureGPU = globeHTTPFixture(respond, {endpoint: '/almanac-rivers', script: '/js/globe_photo_worker.js',
        nativeShore: false, denyPhotographs: true, regionalAliases: true});
    globalThis.Worker = fixtureGPU.Worker;
    const atlasURL = '/js/globe_photo_atlas.js';
    const {GlobePhotoAtlas} = await import(atlasURL) as typeof import('../../typescript/globe_photo_atlas.js');
    const canvas = document.createElement('canvas'), gl = canvas.getContext('webgl')!;
    let published = 0;
    const atlas = new GlobePhotoAtlas(gl, () => ++published, 2048);
    const framebuffer = gl.createFramebuffer();
    try {
        atlas.bind([-2.82, 52.62, -2.68, 52.70], 800);
        for (let i = 0; i < 1200 && !published; ++i) await new Promise(resolve => setTimeout(resolve, 25));
        check(published === 1, 'a fixed photographic cell reaches the GPU independently of drawing the next frame');
        const page = atlas.bind([-2.82, 52.62, -2.68, 52.70], 800)!;
        const addresses = new Uint8Array(labWire.exports().memory.buffer, page.pointer, 256);
        let slot = -1;
        for (let i = 0; i < addresses.length; i += 4) if (addresses[i]) { slot = addresses[i] - 1; break; }
        check(slot >= 0, 'the native page references the resident photographic slot');
        gl.bindFramebuffer(gl.FRAMEBUFFER, framebuffer);
        gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, Reflect.get(atlas, 'atlas'), 0);
        const pixels = new Uint8Array(4);
        gl.readPixels(slot % 4 * 512 + 256, Math.floor(slot / 4) * 512 + 256, 1, 1, gl.RGBA, gl.UNSIGNED_BYTE, pixels);
        check(gl.getError() === gl.NO_ERROR && expected.every((value, i) => Math.abs(pixels[i] - value) <= 3),
              'the atlas preserves photographic colour and alpha during direct RGBA upload');
    } finally {
        gl.bindFramebuffer(gl.FRAMEBUFFER, null); gl.deleteFramebuffer(framebuffer);
        atlas.dispose(); fixtureGPU.dispose(); globalThis.Worker = OriginalWorker;
        gl.getExtension('WEBGL_lose_context')?.loseContext();
    }
}
