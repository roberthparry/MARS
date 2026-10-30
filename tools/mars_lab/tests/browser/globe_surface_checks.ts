/**
 * @file globe_surface_checks.ts
 * @brief Real-worker river compositing, persistent cache limits and cancellation checks.
 *
 * Uses actual local photographs, OffscreenCanvas, ImageBitmap and IndexedDB in the disposable browser profile.
 * Only river HTTP responses are controlled, so codecs and rasterisation remain the production implementation.
 */
async function checkGlobeSurface(check: (condition: boolean, message: string) => void): Promise<void> {
    await new Promise<void>((resolve, reject) => {
        const script = document.createElement('script');
        script.src = '/js/globe_surface_cache.js';
        script.onload = () => resolve();
        script.onerror = () => reject(new Error('Surface cache script unavailable'));
        document.head.appendChild(script);
    });
    const database = await new Promise<IDBDatabase>((resolve, reject) => {
        const request = indexedDB.open('mars.globe.surface.v1', 3);
        request.onupgradeneeded = () => request.result.createObjectStore('tiles', {keyPath: 'key'}).createIndex('used', 'used');
        request.onsuccess = () => resolve(request.result);
        request.onerror = () => reject(request.error);
    });
    const clear = () => new Promise<void>((resolve, reject) => {
        const transaction = database.transaction('tiles', 'readwrite');
        transaction.objectStore('tiles').clear();
        transaction.oncomplete = () => resolve();
        transaction.onabort = () => reject(transaction.error);
    });
    const records = () => new Promise<GlobeSurfaceRecord[]>((resolve, reject) => {
        const request = database.transaction('tiles').objectStore('tiles').getAll();
        request.onsuccess = () => resolve(request.result as GlobeSurfaceRecord[]);
        request.onerror = () => reject(request.error);
    });
    const cache = new GlobeSurfaceCache();
    try {
        await clear();
        const raw = new Blob([new Uint8Array(2048 * 2048 * 4)], {type: 'application/octet-stream'});
        for (let i = 0; i < GlobeSurfaceCache.maximumEntries; ++i) await cache.put(`tile-${i}`, raw);
        await new Promise(resolve => setTimeout(resolve, 5));
        check(Boolean(await cache.get('tile-0')), 'a raw cache hit refreshes its recency');
        await cache.put('newest', raw);
        const saved = await records();
        check(saved.length === 32 && saved.reduce((sum, item) => sum + item.size, 0) === 512 * 1024 * 1024 &&
              Boolean(await cache.get('tile-0')) && !await cache.get('tile-1'),
              'raw disk cache retains 32 views within 512 MiB and evicts the least recently used tile');
        await cache.put('invalid-size', new Blob(['short'], {type: 'application/octet-stream'}));
        await cache.put('invalid-type', new Blob([raw], {type: 'image/png'}));
        check(!await cache.get('invalid-size') && !await cache.get('invalid-type'),
              'raw tiles require the exact pixel count and format');
        await clear();
        const tilesURL = '/js/globe_surface_tiles.js';
        const {GlobeSurfaceTiles} = await import(tilesURL) as typeof import('../../typescript/globe_surface_tiles.js');
        const cacheURL = '/js/globe_texture_cache.js';
        const {GlobeTextureCache} = await import(cacheURL) as typeof import('../../typescript/globe_texture_cache.js');
        const released: number[] = [], indexed = new GlobeTextureCache<number>(4, 10, value => released.push(value));
        try {
            check(indexed.put('one', 1, 4) && indexed.put('two', 2, 4) && indexed.get('one') === 1,
                  'native dictionary retrieves resources by copied key and refreshes recency');
            check(indexed.put('three', 3, 4) && indexed.get('two') === null && released.includes(2) && indexed.size === 2,
                  'native byte-budget eviction removes the least recently used resource');
            check(!indexed.put('oversize', 4, 11) && indexed.get('one') === 1,
                  'an oversized insertion leaves useful native cache entries intact');
        } finally { indexed.dispose(); }
        check(released.includes(1) && released.includes(3) && indexed.size === 0,
              'native cache disposal releases each remaining host slot');
        const spatial = new GlobeTextureCache<number>(4, 16, () => {});
        try {
            spatial.put('displayed', 1, 4, [0, 0, 1, 1], 100, -1, 2);
            const original = spatial.covering([.1, .1, .9, .9], 100)!;
            spatial.put('prefetched', 2, 4, [-.5, -.5, 1.5, 1.5], 100, -1, 2);
            spatial.covering([-.4, -.4, 1.4, 1.4], 100);
            check(spatial.covering([.1, .1, .9, .9], 100, false, original.slot)?.resource === 1,
                  'prefetch recency cannot oscillate the displayed tile while equivalent coverage remains valid');
        } finally { spatial.dispose(); }
        const firstBounds = GlobeSurfaceTiles.bounds([-2.8, 52.7, -2.7, 52.8]);
        const shiftedBounds = GlobeSurfaceTiles.bounds([-2.80000001, 52.70000001, -2.70000001, 52.80000001]);
        check(firstBounds.every((value, i) => value === shiftedBounds[i]),
              'tiny camera differences reuse the same geographic cache key');
        for (const bounds of [[-2.8, 52.7, -2.7, 52.8], [-181, -2, -179, 2], [1, 89.9, 2, 90]]) {
            const padded = GlobeSurfaceTiles.bounds(bounds);
            check(padded[0] <= bounds[0] && padded[1] <= bounds[1] &&
                  padded[2] >= bounds[2] && padded[3] >= bounds[3] && padded[3] <= 90,
                  'stable windows cover the view, including poles and the antimeridian');
        }
        const gpuCanvas = document.createElement('canvas'), gpu = gpuCanvas.getContext('webgl')!;
        const resident = new GlobeSurfaceTiles(gpu);
        const sample = await createImageBitmap(new ImageData(2, 2));
        try {
            resident.put([0, 0, 1, 1], 100, sample, false);
            let first = resident.covering([0.1, 0.1, 0.9, 0.9], 100)!;
            check(!first.complete, 'a resident preview remains distinguishable from completed river coverage');
            resident.bind([0.1, 0.1, 0.9, 0.9], 100);
            resident.put([0, 0, 1, 1], 100, sample);
            const refined = resident.covering([0.1, 0.1, 0.9, 0.9], 100)!;
            check(refined.complete && refined.texture !== first.texture &&
                  gpu.isTexture(first.texture) && gpu.isTexture(refined.texture),
                  'a completed tile keeps its displayed preview alive for the photographic handoff');
            const start = resident.bind([0.1, 0.1, 0.9, 0.9], 100, 100)!;
            const middle = resident.bind([0.1, 0.1, 0.9, 0.9], 100, 350)!;
            for (let i = 1; i <= 4; ++i) resident.put([i * 2, 0, i * 2 + 1, 1], 100, sample);
            check(gpu.isTexture(first.texture) && gpu.isTexture(refined.texture),
                  'cache pressure cannot evict either GPU layer during a geographic handoff');
            const finish = resident.bind([0.1, 0.1, 0.9, 0.9], 100, 600)!;
            check(start.blend === 0 && Boolean(start.previous) && middle.blend === .5 && finish.blend === 1,
                  'resident photographs crossfade over half a second without deleting the outgoing GPU texture');
            first = refined;
            resident.put([-.5, -.5, 1.5, 1.5], 100, sample, false);
            check(resident.covering([0.1, 0.1, 0.9, 0.9], 100)?.texture === first.texture,
                  'an unfinished wider preview cannot replace completed photography that still covers the view');
            for (let i = 1; i <= 4; ++i) resident.put([i * 2, 0, i * 2 + 1, 1], 100, sample);
            check(resident.covering([0.1, 0.1, 0.9, 0.9], 100)?.texture === first.texture && gpu.isTexture(first.texture),
                  'destination preloading retains and reuses the displayed GPU texture');
            check(!resident.covering([2, 0, 3, 1], 100) && resident.covering([8, 0, 9, 1], 100) !== null,
                  'the GPU working set evicts older non-visible regions after four textures');
            check(!resident.covering([0, 0, 1, 1], 200), 'a deep zoom requests adequate detail');
            const overlap = resident.bind([-.2, -.2, 1.2, 1.2], 200, 700);
            check(Boolean(overlap?.complete && overlap.bounds.join() === '0,0,1,1'),
                  'drawing retains partial magnified photography while strict loading requests a larger replacement');
            resident.put([179, -1, 181, 1], 100, sample);
            check(Boolean(resident.covering([-180.5, -.5, -179.5, .5], 100)),
                  'native cache coverage reuses equivalent longitudes across the date line');
            resident.dispose();
            check(!gpu.isTexture(first.texture), 'disposing the working set releases GPU textures');
        } finally { sample.close(); resident.dispose(); gpu.getExtension('WEBGL_lose_context')?.loseContext(); }
        const moduleURL = '/js/globe_surface.js';
        const {GlobeSurface} = await import(moduleURL) as typeof import('../../typescript/globe_surface.js');
        const photograph = await fetch('/earth_tiles/05-01.png');
        const sha256 = photograph.headers.get('ETag')!.replace(/^"sha256-|"$/g, '');
        await photograph.body?.cancel();
        const regionalPhoto = await fetch('/earth_regions/01.png');
        const regionalSha = regionalPhoto.headers.get('ETag')!.replace(/^"sha256-|"$/g, '');
        const regionalBitmap = await createImageBitmap(new Blob([await regionalPhoto.arrayBuffer()], {type: 'image/png'}));
        const reference = document.createElement('canvas');
        reference.width = reference.height = 2048;
        const referenceContext = reference.getContext('2d')!;
        referenceContext.drawImage(regionalBitmap, 0, 0, 2048, 2048);
        const expectedRegional = referenceContext.getImageData(1800, 1500, 1, 1).data;
        regionalBitmap.close();
        reference.width = reference.height = 1;
        let plans = 0, paths = 0, revision = 1, held: (() => void) | null = null;
        let holdNext = false, deny = false, complete = false;
        let regionalURL = '/earth_regions/123.png';
        const key = () => `surface-browser-${revision}`;
        const respond = async (bytes: Uint8Array<ArrayBuffer>): Promise<Response> => {
            const payload = labWire.decode(bytes) as {format: string; key: string};
            if (deny) return new Response('', {status: 503});
            if (payload.format === 'surface-plan') {
                ++plans;
                return new Response(labWire.encode({ok: true, key: key(), bounds: [0, 0, 1, 1], photos: [
                    {url: '/earth_tiles/05-01.png', sha256, source: [0, 0, 1800, 1800], target: [0, 0, 1024.3, 2048]},
                    {url: '/earth_tiles/05-01.png', sha256, source: [0, 0, 1800, 1800], target: [1024.3, 0, 1023.7, 2048]},
                    {url: regionalURL, sha256: regionalSha, regional: true, width: 3600, decode_size: 3600,
                     source: [0, 0, 3600, 3600], target: [0, 0, 2048, 2048]}
                ]}));
            }
            ++paths;
            if (holdNext) { holdNext = false; await new Promise<void>(resolve => { held = resolve; }); }
            return new Response(labWire.encode({ok: true, key: payload.key, complete, path: 'M0 1024L2048 1024',
                fill: 'M100 300L700 300L700 700L100 700Z',
                banks: ['M900 300L1500 300L1500 900L900 900ZM1100 500L1100 700L1300 700L1300 500Z',
                    'M1450 600L1650 600L1650 900L1450 900Z']}));
        };
        const fixture = globeHTTPFixture(respond,
            {endpoint: '/almanac-rivers', script: '/js/globe_surface_worker.js', nativeShore: false, surfaceProbe: true, regionalAliases: true});
        const persisted = async () => {
            for (let attempt = 0; attempt < 200 && !await cache.get(key()); ++attempt)
                await new Promise(resolve => setTimeout(resolve, 25));
            check(Boolean(await cache.get(key())), 'background persistence finishes after publication');
        };
        const OriginalWorker = globalThis.Worker;
        let surface = new GlobeSurface();
        globalThis.Worker = fixture.Worker;
        const draw = (context: CanvasRenderingContext2D, tile: import('../../typescript/globe_surface.js').GlobeSurfaceTile) => {
            if (!('pixels' in tile)) { context.drawImage(tile, 0, 0); return; }
            const gpuCanvas = document.createElement('canvas'), gl = gpuCanvas.getContext('webgl')!;
            const texture = gl.createTexture(), framebuffer = gl.createFramebuffer();
            gl.bindTexture(gl.TEXTURE_2D, texture);
            gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, tile.width, tile.height, 0, gl.RGBA, gl.UNSIGNED_BYTE, tile.pixels);
            gl.bindFramebuffer(gl.FRAMEBUFFER, framebuffer);
            gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, texture, 0);
            check(gl.checkFramebufferStatus(gl.FRAMEBUFFER) === gl.FRAMEBUFFER_COMPLETE,
                  'cached RGBA uploads directly into a complete WebGL texture');
            const pixels = new Uint8Array(tile.width * tile.height * 4);
            gl.readPixels(0, 0, tile.width, tile.height, gl.RGBA, gl.UNSIGNED_BYTE, pixels);
            check(gl.getError() === gl.NO_ERROR && pixels.every((value, i) => value === tile.pixels[i]),
                  'direct upload preserves every premultiplied colour and alpha byte');
            gl.deleteFramebuffer(framebuffer); gl.deleteTexture(texture);
            gl.getExtension('WEBGL_lose_context')?.loseContext();
            const straight = new Uint8ClampedArray(pixels);
            for (let i = 0; i < straight.length; i += 4) {
                const alpha = straight[i + 3];
                if (alpha) for (let channel = 0; channel < 3; ++channel)
                    straight[i + channel] = Math.round(straight[i + channel] * 255 / alpha);
            }
            context.putImageData(new ImageData(straight, tile.width, tile.height), 0, 0);
        };
        const inspect = (bitmap: import('../../typescript/globe_surface.js').GlobeSurfaceTile) => {
            check(bitmap.width === 2048 && bitmap.height === 2048, 'worker publishes a complete regional bitmap');
            const canvas = document.createElement('canvas');
            canvas.width = canvas.height = 2048;
            const context = canvas.getContext('2d')!;
            draw(context, bitmap);
            const river = context.getImageData(512, 1024, 1, 1).data;
            const land = context.getImageData(512, 100, 1, 1).data;
            const seam = context.getImageData(1024, 100, 1, 1).data;
            const detailed = context.getImageData(1800, 1500, 1, 1).data;
            check([0, 1, 2].every(i => Math.abs(detailed[i] - expectedRegional[i]) <= 3),
                  'regional imagery replaces the coarse photograph inside its feathered boundary');
            check(river[3] === 0 && land[3] === 255 && seam[3] === 255 && land[0] + land[1] + land[2] > 0,
                  'photography stays opaque across crop seams and only river centrelines cut through it');
            const alpha = (x: number, y: number) => context.getImageData(x, y, 1, 1).data[3];
            check(alpha(400, 500) === 0 && alpha(400, 250) === 255,
                  'recorded river widths fill ribbons without flooding adjacent land');
            check(alpha(1000, 600) === 0 && alpha(1200, 600) === 255 && alpha(1475, 750) === 0,
                  'mapped banks preserve islands while overlapping polygons remain water');
            bitmap.close();
            canvas.width = canvas.height = 1;
        };
        try {
            inspect(await surface.load([0, 0, 1, 1], new AbortController().signal));
            check(plans === 1 && paths === 1, 'first view fetches one plan and one geometry payload');
            check(!await cache.get(key()), 'pixels reach the globe even while raw cache storage is blocked');
            ++revision;
            inspect(await surface.load([0, 0, 1, 1], new AbortController().signal));
            check(plans === 2 && paths === 2 && !await cache.get(key()),
                  'a second destination publishes while the previous disk write remains blocked');
            fixture.releaseSurfaceCache();
            await persisted();
            surface.dispose(); surface = new GlobeSurface();
            const cached = await surface.load([0, 0, 1, 1], new AbortController().signal);
            check('pixels' in cached, 'a disk cache hit transfers raw pixels without rebuilding an ImageBitmap');
            inspect(cached);
            check(plans === 3 && paths === 2, 'a new worker reuses the persisted tile without fetching river geometry');
            fixture.releaseSurfaceCache();
            ++revision;
            inspect(await surface.load([0, 0, 1, 1], new AbortController().signal));
            check(paths === 3, 'a new database identity replaces cached imagery');
            regionalURL = '/earth_regions/4095.png';
            ++revision;
            inspect(await surface.load([0, 0, 1, 1], new AbortController().signal));
            check(paths === 4, 'three- and four-digit regional image addresses render through the real worker');
            await persisted();
            await new Promise<void>((resolve, reject) => {
                const transaction = database.transaction('tiles', 'readwrite');
                transaction.objectStore('tiles').put({key: key(), blob: new Blob(['damaged']),
                    size: 7, used: Date.now()});
                transaction.oncomplete = () => resolve();
                transaction.onabort = () => reject(transaction.error);
            });
            inspect(await surface.load([0, 0, 1, 1], new AbortController().signal));
            check(paths === 5, 'a damaged cache entry is rebuilt without losing the region');
            ++revision; holdNext = true;
            let previews = 0, completed = false;
            const previewColours: Uint8ClampedArray[] = [];
            const previewDetail: boolean[] = [];
            const refining = surface.load([0, 0, 1, 1], new AbortController().signal, (tile, detailed) => {
                check(tile.width === 2048 && tile.height === 2048,
                      'an independent photographic preview has the complete geographic extent');
                const canvas = document.createElement('canvas');
                canvas.width = canvas.height = 2048;
                const context = canvas.getContext('2d')!;
                draw(context, tile);
                previewColours.push(context.getImageData(1800, 1500, 1, 1).data);
                previewDetail.push(detailed);
                canvas.width = canvas.height = 1;
                ++previews;
            }).then(tile => { completed = true; return tile; });
            for (let i = 0; i < 200 && !previews; ++i) await new Promise(resolve => setTimeout(resolve, 25));
            for (let i = 0; i < 200 && previews < 2; ++i) await new Promise(resolve => setTimeout(resolve, 25));
            check(previews === 2 && !completed && Boolean(held),
                  'photography is visible while the river query is still pending');
            check(previewDetail.join() === 'false,true',
                  'worker previews distinguish coarse NASA coverage from completed regional photography');
            check(previewColours[0][3] === 255 && previewColours[1][3] === 255 &&
                  [0, 1, 2].some(i => Math.abs(previewColours[0][i] - previewColours[1][i]) > 3) &&
                  [0, 1, 2].every(i => Math.abs(previewColours[1][i] - expectedRegional[i]) <= 3),
                  'opaque NASA coverage appears first and regional refinement follows before geometry is ready');
            (held as (() => void) | null)?.();
            held = null;
            inspect(await refining);
            ++revision; holdNext = true;
            const abort = new AbortController();
            const obsolete = surface.load([0, 0, 1, 1], abort.signal).then(bitmap => {
                bitmap.close(); return false;
            }, error => error.name === 'AbortError');
            for (let i = 0; i < 200 && !held; ++i) await new Promise(resolve => setTimeout(resolve, 5));
            check(Boolean(held), 'obsolete geometry request reached the worker');
            abort.abort();
            check(await obsolete, 'cancelling travel rejects the pending tile promptly');
            ++revision;
            inspect(await surface.load([0, 0, 1, 1], new AbortController().signal));
            (held as (() => void) | null)?.();
            deny = true;
            const rejected = await surface.load([0, 0, 1, 1], new AbortController().signal).then(bitmap => {
                bitmap.close(); return false;
            }, () => true);
            check(rejected, 'unavailable native imagery rejects cleanly so the globe can use its overlay fallback');
            deny = false; complete = true; ++revision;
            const surveyed = await surface.load([0, 0, 1, 1], new AbortController().signal);
            const canvas = document.createElement('canvas');
            canvas.width = canvas.height = 2048;
            const context = canvas.getContext('2d')!;
            draw(context, surveyed);
            const alpha = (x: number, y: number) => context.getImageData(x, y, 1, 1).data[3];
            check(Math.abs(alpha(1200, 600) - 128) <= 1 && Math.abs(alpha(512, 100) - 128) <= 1 &&
                  alpha(1000, 600) === 0,
                  'complete mapped coverage distinguishes islands and land from both water and unknown coverage');
            surveyed.close();
            await persisted();
            const cachedSurveyed = await surface.load([0, 0, 1, 1], new AbortController().signal);
            draw(context, cachedSurveyed);
            check(Math.abs(alpha(1200, 600) - 128) <= 1 && alpha(1000, 600) === 0,
                  'cached GPU-ready pixels retain half-alpha surveyed land and transparent water');
            cachedSurveyed.close(); canvas.width = canvas.height = 1;
            complete = false; ++revision;
        } finally {
            (held as (() => void) | null)?.();
            surface.dispose(); fixture.dispose(); globalThis.Worker = OriginalWorker;
        }
        deny = false;
        const denied = globeHTTPFixture(respond, {endpoint: '/almanac-rivers', script: '/js/globe_surface_worker.js',
                                                  nativeShore: false, storageDenied: true, regionalAliases: true});
        globalThis.Worker = denied.Worker;
        surface = new GlobeSurface();
        const before = paths;
        try {
            inspect(await surface.load([0, 0, 1, 1], new AbortController().signal));
            inspect(await surface.load([0, 0, 1, 1], new AbortController().signal));
            check(paths === before + 2, 'denied browser storage still renders each requested region');
        } finally { surface.dispose(); denied.dispose(); globalThis.Worker = OriginalWorker; }
    } finally { await clear(); database.close(); }
}
