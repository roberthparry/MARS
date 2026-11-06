/**
 * @file globe_http_checks.ts
 * @brief Direct native shoreline HTTP ownership and real-worker networking regressions.
 *
 * Supplies test-only Fetch responses to the actual drawing worker, leaving its native receiver, Protobuf
 * decoder and projector intact. No fixture routing or response relay is installed in production code.
 */

/** @brief Serve controlled HTTP responses inside real drawing workers; other worker types are unchanged. */
function globeHTTPFixture(handler: (body: Uint8Array<ArrayBuffer>) => Promise<Response>,
                          options: {endpoint: string; script: string; nativeShore: boolean; storageDenied?: boolean;
                                    surfaceProbe?: boolean; regionalAliases?: boolean; holdPhotograph?: boolean;
                                    denyPhotographs?: boolean; decodeProbe?: boolean; persistProbe?: boolean;
                                    alphaProbe?: boolean; globalAliases?: boolean; blockProbe?: boolean} =
                              {endpoint: '/almanac-globe', script: '/js/globe_worker.js', nativeShore: true}): {
    Worker: typeof Worker; releaseSurfaceCache(): void; waitForCache(key: string): Promise<void>; cacheCount(): number;
    sourcePlanCount(): number;
    waitForPhotograph(): Promise<void>;
    waitForDecode(count?: number): Promise<void>; bypassPhotographs(active: boolean): Promise<void>;
    releasePhotograph(): void; dispose(): void;
} {
    const BrowserWorker = globalThis.Worker;
    const source = `
        const networkFetch = globalThis.fetch.bind(globalThis);
        const loadScripts = globalThis.importScripts.bind(globalThis);
        globalThis.importScripts = (...urls) => loadScripts(...urls.map(url => new URL(url, ${JSON.stringify(location.origin)}).href));
        const replies = new Map();
        const photographs = new Set();
        let releaseCache;
        const cacheReady = new Promise(resolve => { releaseCache = resolve; });
        let releasePhotograph;
        const photographReady = new Promise(resolve => { releasePhotograph = resolve; });
        let next = 0;
        let bypassPhotographs = false, photographLookup;
        addEventListener('message', event => {
            if (typeof event.data.fixtureBypassPhotographs === 'boolean') {
                event.stopImmediatePropagation();
                bypassPhotographs = event.data.fixtureBypassPhotographs;
                void import(${JSON.stringify(new URL('/js/globe_texture_cache.js', location.href).href)}).then(module => {
                    if (!photographLookup) {
                        photographLookup = module.GlobeTextureCache.prototype.getSlot;
                        module.GlobeTextureCache.prototype.getSlot = function(key) {
                            if (bypassPhotographs && /^\\/earth_.*[a-f0-9]{64}:\\d+::$/.test(key)) return null;
                            return photographLookup.call(this, key);
                        };
                    }
                    postMessage({fixturePhotographsBypassed: true});
                });
                return;
            }
            if (event.data.fixturePersist) {
                event.stopImmediatePropagation(); releaseCache(); return;
            }
            if (event.data.fixturePhotographRelease) {
                event.stopImmediatePropagation(); releasePhotograph(); return;
            }
            if (!event.data.fixtureReply) return;
            event.stopImmediatePropagation();
            const pending = replies.get(event.data.id);
            replies.delete(event.data.id);
            if (!pending) return;
            event.data.error ? pending.reject(new Error(event.data.error)) : pending.resolve(new Response(
                event.data.bytes, {status: event.data.status, headers: event.data.headers}));
        });
        globalThis.fetch = (url, options) => {
            if (${Boolean(options.blockProbe)} && String(url).startsWith('/earth-block?v=1&sha=')) {
                return (async () => {
                    const canvas = new OffscreenCanvas(514, 514), context = canvas.getContext('2d');
                    context.fillStyle = 'rgb(180,80,40)'; context.fillRect(0, 0, 514, 514);
                    const blob = await canvas.convertToBlob({type:'image/png'});
                    canvas.width = canvas.height = 1;
                    return new Response(blob, {headers:{ETag:'"sha256-' + 'c'.repeat(64) + '"'}});
                })();
            }
            if (${Boolean(options.globalAliases)} && url === '/earth_tiles_detail/47-23.png')
                return networkFetch(new URL('/earth_tiles/00-00.png', ${JSON.stringify(location.origin)}), options);
            if (${Boolean(options.alphaProbe)} && (url === '/earth_regions/123.png' || url === '/earth_regions/4095.png')) {
                return (async () => {
                    const canvas = new OffscreenCanvas(1800, 1800), context = canvas.getContext('2d');
                    context.fillStyle = url.endsWith('/123.png') ? 'rgb(180,80,40)' : 'rgb(20,80,180)';
                    context.fillRect(0, 0, 1800, url.endsWith('/123.png') ? 1800 : 600);
                    const blob = await canvas.convertToBlob({type: 'image/png'});
                    canvas.width = canvas.height = 1;
                    return new Response(blob, {headers: {'ETag': '"sha256-' + 'c'.repeat(64) + '"'}});
                })();
            }
            if (${Boolean(options.denyPhotographs)} && (String(url).startsWith('/earth_tiles/') ||
                String(url).startsWith('/earth_regions/'))) throw new Error('Photograph decoding is forbidden by this cache test');
            if (${Boolean(options.surfaceProbe)} && (String(url).startsWith('/earth_tiles/') ||
                String(url).startsWith('/earth_regions/'))) {
                if (photographs.has(url)) throw new Error('Photograph decoded repeatedly');
                photographs.add(url);
            }
            if (${Boolean(options.holdPhotograph)} && String(url).startsWith('/earth_tiles/')) {
                postMessage({fixturePhotograph: true});
                return new Promise((resolve, reject) => {
                    options.signal.addEventListener('abort', () => reject(new DOMException('Aborted', 'AbortError')), {once: true});
                    photographReady.then(() => networkFetch(new URL(url, ${JSON.stringify(location.origin)}), options)
                        .then(resolve, reject));
                });
            }
            if (${Boolean(options.regionalAliases)} &&
                (url === '/earth_regions/123.png' || url === '/earth_regions/4095.png'))
                return networkFetch(new URL('/earth_regions/01.png', ${JSON.stringify(location.origin)}), options);
            if (url !== ${JSON.stringify(options.endpoint)}) return networkFetch(new URL(url, ${JSON.stringify(location.origin)}), options);
            return new Promise((resolve, reject) => {
                const id = ++next;
                replies.set(id, {resolve, reject});
                options.signal.addEventListener('abort', () => {
                    replies.delete(id); reject(new DOMException('Aborted', 'AbortError'));
                }, {once: true});
                postMessage({fixtureFetch: true, id, body: options.body});
            });
        };
        if (${Boolean(options.storageDenied)}) Object.defineProperty(globalThis, 'indexedDB', {
            value: {open() { throw new Error('Fixture storage denied'); }}
        });
        if (${Boolean(options.decodeProbe)}) {
            const decode = globalThis.createImageBitmap.bind(globalThis);
            globalThis.createImageBitmap = async (...args) => {
                if (${Boolean(options.blockProbe)} && args[0] instanceof Blob && args[0].type === 'image/png')
                    throw new Error('Prepared PNG blocks must be decoded in WASM');
                const bitmap = await decode(...args);
                if (args[0] instanceof Blob && args[0].type === 'image/png') postMessage({fixtureDecoded: true});
                return bitmap;
            };
        }
        importScripts(${JSON.stringify(new URL(options.script, location.href).href)});
        if (${Boolean(options.surfaceProbe || options.persistProbe)}) {
            const put = GlobeSurfaceCache.prototype.put;
            GlobeSurfaceCache.prototype.put = async function(key, blob) {
                await cacheReady;
                await put.call(this, key, blob);
                postMessage({fixtureCached: key});
            };
        }
        if (${Boolean(options.surfaceProbe || (options.blockProbe && options.decodeProbe))}) {
            const call = labDOM.call;
            labDOM.call = (name, ...args) => {
                if (name === 'lab_texture_cache' && args[0]?.action === 9) postMessage({fixtureSourcePlan: true});
                const result = call(name, ...args);
                if (name === 'lab_photo_decode' && result) postMessage({fixtureDecoded: true});
                return result;
            };
        }
        if (${options.nativeShore}) {
            labWire.decode = labWire.decodeAsync = () => { throw new Error('Shoreline must not construct host response values'); };
            labDOM.env.lab_dom_bytes_copy = () => { throw new Error('Shoreline bytes must not return through host handles'); };
        }
    `;
    const url = URL.createObjectURL(new Blob([source], {type: 'text/javascript'}));
    const workers = new Set<Worker>();
    const cached = new Set<string>();
    let photographStarted: () => void = () => {};
    const photographPending = new Promise<void>(resolve => { photographStarted = resolve; });
    let decodedCount = 0, sourcePlans = 0, bypassed: () => void = () => {};
    const fixtureScript = options.script;
    class FixtureWorker extends BrowserWorker {
        constructor(scriptURL: string | URL, options?: WorkerOptions) {
            const globe = String(scriptURL).endsWith(fixtureScript);
            super(globe ? url : scriptURL, options);
            if (!globe) return;
            workers.add(this);
            this.addEventListener('message', event => {
                if (event.data.fixtureDecoded) { event.stopImmediatePropagation(); ++decodedCount; return; }
                if (event.data.fixtureSourcePlan) { event.stopImmediatePropagation(); ++sourcePlans; return; }
                if (event.data.fixturePhotographsBypassed) { event.stopImmediatePropagation(); bypassed(); return; }
                if (typeof event.data.fixtureCached === 'string') {
                    event.stopImmediatePropagation(); cached.add(event.data.fixtureCached); return;
                }
                if (event.data.fixturePhotograph) {
                    event.stopImmediatePropagation(); photographStarted(); return;
                }
                if (!event.data.fixtureFetch) return;
                event.stopImmediatePropagation();
                const id = event.data.id;
                void handler(event.data.body).then(async response => {
                    const bytes = new Uint8Array(await response.arrayBuffer());
                    this.postMessage({fixtureReply: true, id, status: response.status,
                        headers: [...response.headers], bytes}, [bytes.buffer]);
                }).catch(error => this.postMessage({fixtureReply: true, id, error: String(error)}));
            });
        }
    }
    return {Worker: FixtureWorker, releaseSurfaceCache() {
        for (const worker of workers) worker.postMessage({fixturePersist: true});
    }, cacheCount() { return cached.size; }, sourcePlanCount() { return sourcePlans; }, async waitForCache(key) {
        const deadline = performance.now() + 15000;
        while (!cached.has(key) && performance.now() < deadline)
            await new Promise(resolve => window.setTimeout(resolve, 25));
        if (!cached.has(key)) throw new Error('Fixture cache write did not complete: ' + key);
    }, waitForPhotograph() { return photographPending; }, releasePhotograph() {
        for (const worker of workers) worker.postMessage({fixturePhotographRelease: true});
    }, async waitForDecode(count = 1) {
        const deadline = performance.now() + 5000;
        while (decodedCount < count && performance.now() < deadline)
            await new Promise(resolve => window.setTimeout(resolve, 20));
        if (decodedCount < count) throw new Error('Read-ahead PNG did not decode while the current source was blocked');
    }, bypassPhotographs(active) {
        return new Promise<void>(resolve => {
            bypassed = resolve;
            for (const worker of workers) worker.postMessage({fixtureBypassPhotographs: active});
        });
    }, dispose() {
        for (const worker of workers) {
            worker.dispatchEvent(new ErrorEvent('error', {message: 'Globe HTTP fixture disposed'}));
            worker.terminate();
        }
        URL.revokeObjectURL(url);
    }};
}

/** @brief Convert the existing readable UI fixtures to the production binary response format, only in tests. */
function globeFixtureBytes(text: string): Uint8Array<ArrayBuffer> {
    const records = text.trim().split('\n').filter(Boolean);
    const bytes = new Uint8Array(4 + records.length * 17);
    bytes.set([77, 83, 72, 49]);
    const view = new DataView(bytes.buffer);
    records.forEach((line, index) => {
        const fields = line.slice(1).trim().split(/\s+/).map(Number);
        const move = line[0] === 'M', close = line[0] === 'Z';
        bytes[4 + index * 17] = close ? 0 : move ? fields[0] | (fields[1] << 3) : 16;
        view.setFloat64(5 + index * 17, close ? 0 : fields[move ? 2 : 0], true);
        view.setFloat64(13 + index * 17, close ? 0 : fields[move ? 3 : 1], true);
    });
    return bytes;
}

/** @brief Check streamed native admission, atomic failure, stale tokens and projection during an unfinished fetch. */
async function checkGlobeHTTP(check: (condition: boolean, message: string) => void): Promise<void> {
    const native = labWire.exports();
    const packed = globeFixtureBytes('M1 0 -5 -5\nL5 -5\nL5 5\nL-5 5\nZ');
    const bytes = labWire.encode({ok: true, vectors: packed});
    const begin = () => native.lab_globe_fetch_begin(-10, -10, 10, 10);
    const deliver = (token: number, response: Uint8Array<ArrayBuffer>, status = 200): boolean => {
        for (let offset = 0; offset < response.length; offset += 7) {
            const chunk = response.subarray(offset, offset + 7);
            const pointer = native.lab_globe_fetch_reserve(token, chunk.length);
            check(Boolean(pointer), 'native receiver reserves split chunks');
            new Uint8Array(native.memory.buffer, pointer, chunk.length).set(chunk);
            check(Boolean(native.lab_globe_fetch_commit(token, chunk.length)), 'native receiver commits split chunks');
        }
        return Boolean(native.lab_globe_fetch_finish(token, status));
    };
    let token = begin();
    const request = labWire.decode(new Uint8Array(native.memory.buffer, native.lab_globe_fetch_request(token),
        native.lab_globe_fetch_request_size(token)).slice()) as {west: number; south: number; east: number; north: number; format: string};
    check(request.west === -10 && request.south === -10 && request.east === 10 && request.north === 10 &&
        request.format === 'vectors', 'native bounds encode into the existing Protobuf HTTP request');
    check(deliver(token, bytes), 'native HTTP response decoding admits binary geometry across arbitrary chunk boundaries');
    check(deliver(begin(), new Uint8Array([...bytes, 24, 7])), 'unknown envelope fields remain forward compatible');
    const metadata = {presentation: {title: 'Surface', fields: []}, diagnostic: {revision: 1}};
    const serverBytes = labWire.encode({ok: true, vectors: packed, ...metadata});
    check(deliver(begin(), serverBytes), 'shared server presentation metadata does not reject valid shoreline geometry');
    check(deliver(begin(), labWire.encode({...metadata, vectors: packed, ok: true})),
        'unrelated metadata may precede required shoreline members');
    const svg = () => labDOM.call('lab_globe_shore_svg', [0, 0, 0, 1], 1, 800);
    const before = svg();
    check(typeof before === 'string' && before.includes('<path'), 'direct native HTTP receipt produces shoreline SVG');
    for (const response of [bytes.slice(0, -1), labWire.encode({ok: false, vectors: packed}),
        labWire.encode({ok: true, vectors: 'MSH1'}), labWire.encode({ok: true, vectors: packed.slice(0, -1)}),
        new Uint8Array([...bytes, 8, 1]), labWire.encode({vectors: packed}),
        labWire.encode({...metadata, ok: false, vectors: packed}), labWire.encode({...metadata, ok: true}),
        labWire.encode({...metadata, vectors: packed})]) {
        check(!deliver(begin(), response) && svg() === before, 'rejected Protobuf replacement retains published geometry');
    }
    check(!deliver(begin(), bytes, 503) && svg() === before, 'HTTP failure cannot publish geometry');
    token = begin();
    const next = begin();
    check(!native.lab_globe_fetch_reserve(token, 1) && !native.lab_globe_fetch_finish(token, 200),
        'obsolete transfer tokens cannot allocate or publish');
    native.lab_globe_fetch_cancel(token);
    check(deliver(next, bytes), 'cancelling an obsolete request leaves its successor live');
    token = begin();
    native.lab_globe_fetch_cancel(token);
    check(!native.lab_globe_fetch_reserve(token, 1) && svg() === before, 'cancellation keeps old geometry');
    token = begin();
    check(!native.lab_globe_fetch_reserve(token, 8 * 1048576 + 1) &&
        !native.lab_globe_fetch_finish(token, 200) && svg() === before, 'response byte budget enforced in native code');
    token = begin();
    check(Boolean(native.lab_globe_fetch_reserve(token, 1)) && !native.lab_globe_fetch_commit(token, 2) &&
        !native.lab_globe_fetch_finish(token, 200), 'over-committed response chunks are rejected');
    check(!native.lab_globe_fetch_begin(NaN, 0, 1, 1), 'invalid request bounds rejected natively');
    check(deliver(begin(), labWire.encode({ok: true, vectors: packed.slice(0, 4)})), 'empty sea is valid binary geometry');

    let release: ((response: Response) => void) | undefined;
    let requests = 0;
    const reply = () => new Response(serverBytes, {headers: {'Content-Type': 'application/x-protobuf'}});
    const fixture = globeHTTPFixture(async body => {
        check((labWire.decode(body) as {format: string}).format === 'vectors', 'real worker sends native-encoded request');
        if (++requests === 2) return new Promise(resolve => { release = resolve; });
        return reply();
    });
    const worker = new fixture.Worker('/js/globe_worker.js');
    const pending = new Map<number, {resolve(value: unknown): void; reject(error: Error): void}>();
    worker.onmessage = event => {
        const job = pending.get(event.data.id);
        pending.delete(event.data.id);
        if (event.data.error) job?.reject(new Error(event.data.error));
        else job?.resolve(event.data.value);
    };
    const job = (request: GlobeWorkerRequest): Promise<unknown> => new Promise((resolve, reject) => {
        pending.set(request.id, {resolve, reject}); worker.postMessage(request);
    });
    const timeout = new Promise<never>((_resolve, reject) => {
        worker.onerror = event => reject(new Error(event.message));
    });
    let deadline = 0;
    try {
        await Promise.race([(async () => {
            check(await job({id: 1, entry: 'fetch', args: [-10, -10, 10, 10]}) === 1,
                'real worker receives and decodes HTTP directly in WASM without host byte handles');
            const held = job({id: 2, entry: 'fetch', args: [-10, -10, 10, 10]}).catch(() => null);
            for (let i = 0; i < 100 && !release; ++i) await new Promise(resolve => setTimeout(resolve, 10));
            check(Boolean(release), 'second worker fetch is pending');
            const projected = await job({id: 3, entry: 'lab_globe_shore_svg', geometry: 1, args: [[0, 0, 0, 1], 1, 800]});
            check(typeof projected === 'string' && projected.includes('<path'), 'pending HTTP does not block retained projection');
            worker.postMessage({id: 2, entry: 'cancel', args: []});
            check(await job({id: 4, entry: 'fetch', args: [-10, -10, 10, 10]}) === 4, 'replacement fetch survives cancellation');
            release!(reply());
            await held;
            check(await job({id: 5, entry: 'lab_globe_shore_svg', geometry: 1, args: [[0, 0, 0, 1], 1, 800]}) === null,
                'stale projection revision cannot publish against newer geometry');
        })(), timeout, new Promise<never>((_resolve, reject) => {
            deadline = window.setTimeout(() => reject(new Error('Shoreline HTTP worker regression timed out')), 10000);
        })]);
    } finally {
        window.clearTimeout(deadline);
        release?.(reply());
        fixture.dispose();
        labDOM.call('lab_globe_shore_clear');
    }
}
