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
        const request = indexedDB.open('mars.globe.surface.v1', 2);
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
        for (let i = 0; i < 4; ++i) await cache.put(`tile-${i}`, raw);
        await new Promise(resolve => setTimeout(resolve, 5));
        check(Boolean(await cache.get('tile-0')), 'a raw cache hit refreshes its recency');
        await cache.put('newest', raw);
        const saved = await records();
        check(saved.length === 4 && saved.reduce((sum, item) => sum + item.size, 0) === 64 * 1024 * 1024 &&
              Boolean(await cache.get('tile-0')) && !await cache.get('tile-1'),
              'raw cache stays within 64 MiB and evicts the least recently used tile');
        await cache.put('invalid-size', new Blob(['short'], {type: 'application/octet-stream'}));
        await cache.put('invalid-type', new Blob([raw], {type: 'image/png'}));
        check(!await cache.get('invalid-size') && !await cache.get('invalid-type'),
              'raw tiles require the exact pixel count and format');
        await clear();
        const moduleURL = '/js/globe_surface.js';
        const {GlobeSurface} = await import(moduleURL) as typeof import('../../typescript/globe_surface.js');
        const photograph = await fetch('/earth_tiles/05-01.jpg');
        const sha256 = photograph.headers.get('ETag')!.replace(/^"sha256-|"$/g, '');
        await photograph.body?.cancel();
        const regionalPhoto = await fetch('/earth_regions/01.jpg');
        const regionalSha = regionalPhoto.headers.get('ETag')!.replace(/^"sha256-|"$/g, '');
        const regionalBitmap = await createImageBitmap(await regionalPhoto.blob());
        const reference = document.createElement('canvas');
        reference.width = reference.height = 2048;
        const referenceContext = reference.getContext('2d')!;
        referenceContext.drawImage(regionalBitmap, 0, 0, 2048, 2048);
        const expectedRegional = referenceContext.getImageData(1800, 1500, 1, 1).data;
        regionalBitmap.close();
        reference.width = reference.height = 1;
        let plans = 0, paths = 0, revision = 1, held: (() => void) | null = null;
        let holdNext = false, deny = false, complete = false;
        const key = () => `surface-browser-${revision}`;
        const respond = async (bytes: Uint8Array<ArrayBuffer>): Promise<Response> => {
            const payload = labWire.decode(bytes) as {format: string; key: string};
            if (deny) return new Response('', {status: 503});
            if (payload.format === 'surface-plan') {
                ++plans;
                return new Response(labWire.encode({ok: true, key: key(), bounds: [0, 0, 1, 1], photos: [
                    {url: '/earth_tiles/05-01.jpg', sha256, source: [0, 0, 1800, 1800], target: [0, 0, 1024.3, 2048]},
                    {url: '/earth_tiles/05-01.jpg', sha256, source: [0, 0, 1800, 1800], target: [1024.3, 0, 1023.7, 2048]},
                    {url: '/earth_regions/01.jpg', sha256: regionalSha, regional: true,
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
            {endpoint: '/almanac-rivers', script: '/js/globe_surface_worker.js', nativeShore: false, surfaceProbe: true});
        const persisted = async () => {
            for (let attempt = 0; attempt < 200 && !await cache.get(key()); ++attempt)
                await new Promise(resolve => setTimeout(resolve, 25));
            check(Boolean(await cache.get(key())), 'background persistence finishes after publication');
        };
        const OriginalWorker = globalThis.Worker;
        let surface = new GlobeSurface();
        globalThis.Worker = fixture.Worker;
        const inspect = (bitmap: ImageBitmap) => {
            check(bitmap.width === 2048 && bitmap.height === 2048, 'worker publishes a complete regional bitmap');
            const canvas = document.createElement('canvas');
            canvas.width = canvas.height = 2048;
            const context = canvas.getContext('2d')!;
            context.drawImage(bitmap, 0, 0);
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
            fixture.releaseSurfaceCache();
            await persisted();
            surface.dispose(); surface = new GlobeSurface();
            inspect(await surface.load([0, 0, 1, 1], new AbortController().signal));
            check(plans === 2 && paths === 1, 'a new worker reuses the persisted tile without fetching river geometry');
            fixture.releaseSurfaceCache();
            ++revision;
            inspect(await surface.load([0, 0, 1, 1], new AbortController().signal));
            check(paths === 2, 'a new database identity replaces cached imagery');
            await persisted();
            await new Promise<void>((resolve, reject) => {
                const transaction = database.transaction('tiles', 'readwrite');
                transaction.objectStore('tiles').put({key: key(), blob: new Blob(['damaged']),
                    size: 7, used: Date.now()});
                transaction.oncomplete = () => resolve();
                transaction.onabort = () => reject(transaction.error);
            });
            inspect(await surface.load([0, 0, 1, 1], new AbortController().signal));
            check(paths === 3, 'a damaged cache entry is rebuilt without losing the region');
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
            context.drawImage(surveyed, 0, 0);
            const alpha = (x: number, y: number) => context.getImageData(x, y, 1, 1).data[3];
            check(Math.abs(alpha(1200, 600) - 128) <= 1 && Math.abs(alpha(512, 100) - 128) <= 1 &&
                  alpha(1000, 600) === 0,
                  'complete mapped coverage distinguishes islands and land from both water and unknown coverage');
            surveyed.close(); canvas.width = canvas.height = 1;
            complete = false; ++revision;
        } finally {
            (held as (() => void) | null)?.();
            surface.dispose(); fixture.dispose(); globalThis.Worker = OriginalWorker;
        }
        deny = false;
        const denied = globeHTTPFixture(respond, {endpoint: '/almanac-rivers', script: '/js/globe_surface_worker.js',
                                                  nativeShore: false, storageDenied: true});
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
