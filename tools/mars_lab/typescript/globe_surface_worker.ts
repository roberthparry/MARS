/**
 * @file globe_surface_worker.ts
 * @brief Off-main-thread rasterisation and persistent reuse of photographic river surface tiles.
 *
 * Native code supplies crop rectangles and paths. PNG input is read into bounded encoded-byte buffers. A bounded decoded-photo cache avoids repeat PNG work during
 * panning. Completed pixels are published before optional raw-pixel persistence. River cut-outs retain exact coverage.
 */
interface GlobeSurfacePhoto {
    url: string; sha256: string; source: number[]; target: number[]; regional?: boolean;
    width?: number; decode_size?: number;
}
interface GlobeSurfacePlan {
    ok: boolean; key: string; bounds: number[]; photos: GlobeSurfacePhoto[];
    path?: string; fill?: string; banks?: string[]; complete?: boolean;
}
interface GlobeSurfaceMessage { id: number; bounds?: number[]; cancel?: boolean; }
class GlobeSurfaceWorker {
    private readonly ready: Promise<void>;
    private readonly cache: GlobeSurfaceCache;
    private active: AbortController | null = null;
    private pending: GlobeSurfaceMessage | null = null;
    private running = false;
    private photographs: import('./globe_texture_cache.js').GlobeTextureCache<ImageBitmap> | null = null;
    private saving = false;
    private pendingSave: {key: string; canvas: OffscreenCanvas} | null = null;

    private async photograph(photo: GlobeSurfacePhoto, signal: AbortSignal): Promise<ImageBitmap> {
        const reduced = photo.regional && photo.width && photo.decode_size;
        if (reduced && (![1800, 3600].includes(photo.width!) || !Number.isInteger(photo.decode_size) ||
            photo.decode_size! < 64 || photo.decode_size! > photo.width!)) throw new Error('Invalid photograph size');
        const key = `${photo.url}:${photo.sha256}:${Boolean(photo.regional)}:${photo.decode_size || 0}`;
        if (!this.photographs) {
            const moduleURL = '/js/globe_texture_cache.js';
            const {GlobeTextureCache} = await import(moduleURL) as typeof import('./globe_texture_cache.js');
            this.photographs = new GlobeTextureCache(32, 64 * 1024 * 1024, bitmap => bitmap.close());
        }
        const saved = this.photographs.get(key);
        if (saved) return saved;
        const response = await fetch(photo.url, {signal});
        if (!response.ok || response.headers.get('ETag') !== `"sha256-${photo.sha256}"`)
            throw new Error('Photographic source changed');
        const encoded = await response.arrayBuffer();
        if (!encoded.byteLength || encoded.byteLength > (photo.regional ? 64 : 16) * 1024 * 1024)
            throw new Error('Photographic source exceeds its byte limit');
        let bitmap = await createImageBitmap(new Blob([encoded], {type: 'image/png'}), reduced ? {
            resizeWidth: photo.decode_size, resizeHeight: photo.decode_size, resizeQuality: 'high'
        } : {});
        try {
            signal.throwIfAborted();
            const size = bitmap.width;
            if ((reduced ? size !== photo.decode_size : size !== 1800 && !(photo.regional && size === 3600)) ||
                bitmap.height !== size)
                throw new Error('Invalid photograph');
            const bytes = size * size * 4;
            if (photo.regional) {
                const feathered = new OffscreenCanvas(size, size);
                try {
                    const overlay = feathered.getContext('2d');
                    if (!overlay) throw new Error('Regional image canvas unavailable');
                    overlay.drawImage(bitmap, 0, 0);
                    overlay.globalCompositeOperation = 'destination-in';
                    for (const vertical of [false, true]) {
                        const fade = overlay.createLinearGradient(0, 0, vertical ? 0 : size, vertical ? size : 0);
                        fade.addColorStop(0, 'transparent');
                        fade.addColorStop(0.02, '#000');
                        fade.addColorStop(0.98, '#000');
                        fade.addColorStop(1, 'transparent');
                        overlay.fillStyle = fade;
                        overlay.fillRect(0, 0, size, size);
                    }
                    bitmap.close();
                    bitmap = feathered.transferToImageBitmap();
                } finally { feathered.width = feathered.height = 1; }
            }
            signal.throwIfAborted();
            if (!this.photographs.put(key, bitmap, bytes)) throw new Error('Photograph cache unavailable');
            return bitmap;
        } catch (error) { bitmap.close(); throw error; }
    }

    private async persist(key: string, canvas: OffscreenCanvas): Promise<void> {
        try {
            // Yield so the ready bitmap can reach the globe before cache readback starts.
            await new Promise(resolve => setTimeout(resolve, 0));
            const pixels = canvas.getContext('2d')!.getImageData(0, 0, 2048, 2048);
            // Match ImageBitmap's premultiplied upload once, before persistence, not on every cache hit.
            for (let start = 0; start < pixels.data.length; start += 2048 * 64 * 4) {
                const end = Math.min(pixels.data.length, start + 2048 * 64 * 4);
                const chunk = new Uint8Array(pixels.data.buffer, start, end - start);
                const pointer = labDOM.call('lab_texture_premultiply', chunk) as number;
                if (!pointer) throw new Error('Native pixel preparation failed');
                chunk.set(new Uint8Array(labWire.exports().memory.buffer, pointer, chunk.length));
                // New travel requests and cancellation can run between small batches of cache preparation.
                await new Promise(resolve => setTimeout(resolve, 0));
            }
            await this.cache.put(key, new Blob([pixels.data], {type: 'application/octet-stream'}));
        } catch { /* Optional persistence must never delay or reject a visible tile. */ }
        finally {
            canvas.width = canvas.height = 1;
            this.saving = false;
            const next = this.pendingSave;
            this.pendingSave = null;
            if (next) { this.saving = true; void this.persist(next.key, next.canvas); }
        }
    }

    constructor(private readonly port: {
        importScripts(...urls: string[]): void;
        postMessage(message: unknown, transfer?: Transferable[]): void;
        onmessage: ((event: MessageEvent<GlobeSurfaceMessage>) => void) | null;
    }) {
        port.importScripts('/js/transport.js', '/js/globe_surface_cache.js');
        this.ready = labWire.start(false);
        this.cache = new GlobeSurfaceCache();
        port.onmessage = event => {
            this.active?.abort();
            this.pending = event.data.cancel ? null : event.data;
            void this.drain();
        };
    }

    private async request(bounds: number[], format: string, signal: AbortSignal, key = ''): Promise<GlobeSurfacePlan> {
        const [west, south, east, north] = bounds;
        const response = await fetch('/almanac-rivers', {
            method: 'POST', headers: {'Content-Type': 'application/x-protobuf'},
            body: labWire.encode({west, south, east, north, format, key}), signal
        });
        const bytes = await response.arrayBuffer();
        signal.throwIfAborted();
        if (!response.ok || bytes.byteLength > 8 * 1024 * 1024) throw new Error('River surface request failed');
        const plan = labWire.decode(bytes) as GlobeSurfacePlan;
        if (!plan.ok || typeof plan.key !== 'string') throw new Error('Invalid river surface response');
        return plan;
    }

    private async tile(bounds: number[], signal: AbortSignal,
                       preview: (bitmap: ImageBitmap, detailed: boolean) => void): Promise<ImageBitmap | ArrayBuffer> {
        await this.ready;
        signal.throwIfAborted();
        const plan = await this.request(bounds, 'surface-plan', signal);
        const saved = await this.cache.get(plan.key);
        if (saved) {
            try {
                const pixels = await saved.arrayBuffer();
                signal.throwIfAborted();
                if (pixels.byteLength === GlobeSurfaceCache.tileBytes) return pixels;
            } catch { signal.throwIfAborted(); }
        }
        if (!Array.isArray(plan.photos) || !plan.photos.length || plan.photos.length > 68)
            throw new Error('Invalid photographic tile plan');
        // Geometry and photographic reads are independent. Publish imagery while the native
        // river query runs; its latency must not leave the destination on the global overview.
        const geometry = this.request(bounds, 'surface-data', signal, plan.key)
            .then(data => ({data, error: null}), error => ({data: null, error}));
        const canvas = new OffscreenCanvas(2048, 2048), context = canvas.getContext('2d');
        let retained = false;
        try {
            if (!context) throw new Error('Surface canvas unavailable');
            context.imageSmoothingQuality = 'high';
            // Unpainted antialiased crop edges must not masquerade as river coverage.
            context.fillStyle = '#183a32';
            context.fillRect(0, 0, 2048, 2048);
            const publish = async (detailed: boolean) => {
                signal.throwIfAborted();
                const photograph = await createImageBitmap(canvas, {premultiplyAlpha: 'premultiply'});
                if (signal.aborted) { photograph.close(); signal.throwIfAborted(); }
                preview(photograph, detailed);
            };
            let baseDrawn = false, basePublished = false;
            for (const photo of plan.photos) {
                if (!(photo.regional ? /^\/earth_regions\/\d{2,5}\.png$/ : /^\/earth_tiles\/\d{2}-\d{2}\.png$/).test(photo.url) ||
                    !Array.isArray(photo.source) || photo.source.length !== 4 ||
                    !Array.isArray(photo.target) || photo.target.length !== 4 ||
                    ![...photo.source, ...photo.target].every(Number.isFinite)) throw new Error('Invalid photo crop');
                // The native plan lists its complete NASA coverage before regional detail.
                // A slow regional decode must not hide that already usable overview.
                if (photo.regional && baseDrawn && !basePublished) {
                    await publish(false);
                    basePublished = true;
                }
                const bitmap = await this.photograph(photo, signal);
                signal.throwIfAborted();
                const [sx, sy, sw, sh] = photo.source, [dx, dy, dw, dh] = photo.target;
                const scale = photo.width ? bitmap.width / photo.width : 1;
                context.drawImage(bitmap, sx * scale, sy * scale, sw * scale, sh * scale, dx, dy, dw, dh);
                if (!photo.regional) baseDrawn = true;
            }
            await publish(true);
            const {data, error} = await geometry;
            if (!data) throw error;
            signal.throwIfAborted();
            if (data.key !== plan.key || typeof data.path !== 'string') throw new Error('River surface changed');
            context.globalCompositeOperation = 'destination-out';
            // Half alpha denotes surveyed land; zero denotes water, full alpha leaves the base mask intact.
            if (data.complete === true) {
                context.fillStyle = 'rgba(0,0,0,0.5)';
                context.fillRect(0, 0, 2048, 2048);
            }
            context.fillStyle = '#000';
            context.strokeStyle = '#000';
            context.lineWidth = 2;
            context.lineCap = 'round';
            context.stroke(new Path2D(data.path));
            if (typeof data.fill === 'string') context.fill(new Path2D(data.fill));
            if (Array.isArray(data.banks)) {
                const coverage = new Path2D();
                for (const bank of data.banks) {
                    if (typeof bank !== 'string') throw new Error('Invalid river bank path');
                    coverage.addPath(new Path2D(bank));
                }
                context.fill(coverage);
            }
            signal.throwIfAborted();
            const bitmap = await createImageBitmap(canvas, {premultiplyAlpha: 'premultiply'});
            if (signal.aborted) { bitmap.close(); signal.throwIfAborted(); }
            // Retain the newest destination while a previous write is busy; never wait before publishing it.
            retained = true;
            if (!this.saving) {
                this.saving = true;
                void this.persist(plan.key, canvas);
            } else {
                if (this.pendingSave) this.pendingSave.canvas.width = this.pendingSave.canvas.height = 1;
                this.pendingSave = {key: plan.key, canvas};
            }
            return bitmap;
        } finally { if (!retained) canvas.width = canvas.height = 1; }
    }

    private async drain(): Promise<void> {
        if (this.running) return;
        this.running = true;
        try {
            while (this.pending) {
                const message = this.pending;
                this.pending = null;
                const controller = this.active = new AbortController();
                try {
                    if (!message.bounds || !Number.isSafeInteger(message.id)) throw new Error('Invalid surface job');
                    const bitmap = await this.tile(message.bounds, controller.signal, (bitmap, detailed) => {
                        if (controller.signal.aborted) bitmap.close();
                        else this.port.postMessage({id: message.id, bitmap, preview: true, detailed}, [bitmap]);
                    });
                    if (controller.signal.aborted) {
                        if (bitmap instanceof ImageBitmap) bitmap.close();
                    } else if (bitmap instanceof ArrayBuffer) this.port.postMessage({id: message.id, pixels: bitmap}, [bitmap]);
                    else this.port.postMessage({id: message.id, bitmap}, [bitmap]);
                } catch (error) {
                    this.port.postMessage({id: message.id, error: String(error)});
                } finally { if (this.active === controller) this.active = null; }
            }
        } finally { this.running = false; }
    }
}
new GlobeSurfaceWorker(globalThis as unknown as ConstructorParameters<typeof GlobeSurfaceWorker>[0]);
