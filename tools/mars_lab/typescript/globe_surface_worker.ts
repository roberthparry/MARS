/**
 * @file globe_surface_worker.ts
 * @brief Off-main-thread rasterisation and persistent reuse of photographic river surface tiles.
 *
 * Native code supplies crop rectangles and paths. A bounded decoded-photo cache avoids repeat PNG work during
 * panning. Completed pixels are published before optional raw-pixel persistence. River cut-outs retain exact coverage.
 */
interface GlobeSurfacePhoto { url: string; sha256: string; source: number[]; target: number[]; regional?: boolean; }
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
    private readonly photographs = new Map<string, ImageBitmap>();
    private photographBytes = 0;
    private saving = false;

    private async photograph(photo: GlobeSurfacePhoto, signal: AbortSignal): Promise<ImageBitmap> {
        const key = `${photo.url}:${photo.sha256}:${Boolean(photo.regional)}`;
        const saved = this.photographs.get(key);
        if (saved) {
            this.photographs.delete(key);
            this.photographs.set(key, saved);
            return saved;
        }
        const response = await fetch(photo.url, {signal});
        if (!response.ok || response.headers.get('ETag') !== `"sha256-${photo.sha256}"`)
            throw new Error('Photographic source changed');
        let bitmap = await createImageBitmap(await response.blob());
        try {
            signal.throwIfAborted();
            const size = bitmap.width;
            if ((size !== 1800 && !(photo.regional && size === 3600)) || bitmap.height !== size)
                throw new Error('Invalid photograph');
            const bytes = size * size * 4;
            // Map insertion order provides constant-time oldest-entry eviction; no scan of stored photographs.
            while (this.photographBytes + bytes > 64 * 1024 * 1024) {
                const oldest = this.photographs.entries().next().value;
                if (!oldest) break;
                this.photographBytes -= oldest[1].width * oldest[1].height * 4;
                oldest[1].close();
                this.photographs.delete(oldest[0]);
            }
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
            this.photographs.set(key, bitmap);
            this.photographBytes += bytes;
            return bitmap;
        } catch (error) { bitmap.close(); throw error; }
    }

    private async persist(key: string, canvas: OffscreenCanvas): Promise<void> {
        try {
            // Yield so the ready bitmap can reach the globe before cache readback starts.
            await new Promise(resolve => setTimeout(resolve, 0));
            const pixels = canvas.getContext('2d')!.getImageData(0, 0, 2048, 2048);
            // Match ImageBitmap's premultiplied upload once, before persistence, not on every cache hit.
            for (let i = 0; i < pixels.data.length; i += 4) {
                const alpha = pixels.data[i + 3] / 255;
                pixels.data[i] = Math.round(pixels.data[i] * alpha);
                pixels.data[i + 1] = Math.round(pixels.data[i + 1] * alpha);
                pixels.data[i + 2] = Math.round(pixels.data[i + 2] * alpha);
            }
            await this.cache.put(key, new Blob([pixels.data], {type: 'application/octet-stream'}));
        } catch { /* Optional persistence must never delay or reject a visible tile. */ }
        finally { canvas.width = canvas.height = 1; this.saving = false; }
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

    private async tile(bounds: number[], signal: AbortSignal): Promise<ImageBitmap | ArrayBuffer> {
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
        const data = await this.request(bounds, 'surface-data', signal, plan.key);
        if (data.key !== plan.key || typeof data.path !== 'string') throw new Error('River surface changed');
        const canvas = new OffscreenCanvas(2048, 2048), context = canvas.getContext('2d');
        let retained = false;
        try {
            if (!context) throw new Error('Surface canvas unavailable');
            context.imageSmoothingQuality = 'high';
            // Unpainted antialiased crop edges must not masquerade as river coverage.
            context.fillStyle = '#183a32';
            context.fillRect(0, 0, 2048, 2048);
            for (const photo of plan.photos) {
                if (!(photo.regional ? /^\/earth_regions\/\d{2}\.png$/ : /^\/earth_tiles\/\d{2}-\d{2}\.png$/).test(photo.url) ||
                    !Array.isArray(photo.source) || photo.source.length !== 4 ||
                    !Array.isArray(photo.target) || photo.target.length !== 4 ||
                    ![...photo.source, ...photo.target].every(Number.isFinite)) throw new Error('Invalid photo crop');
                const bitmap = await this.photograph(photo, signal);
                signal.throwIfAborted();
                const [sx, sy, sw, sh] = photo.source, [dx, dy, dw, dh] = photo.target;
                context.drawImage(bitmap, sx, sy, sw, sh, dx, dy, dw, dh);
            }
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
            // At most one extra output canvas is retained for optional background storage.
            if (!this.saving) {
                this.saving = retained = true;
                void this.persist(plan.key, canvas);
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
                    const bitmap = await this.tile(message.bounds, controller.signal);
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
