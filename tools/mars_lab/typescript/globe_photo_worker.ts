/**
 * @file globe_photo_worker.ts
 * @brief Off-thread preparation of fixed photographic cells from reusable decoded source blocks.
 *
 * Decodes one PNG at a time and prepares only the native-selected source cores, with one-pixel gutters.
 * A bounded browser-resource queue persists raw blocks independently of drawing. Completed cells transfer directly to WebGL;
 * coarse regional overviews are never published as replacements for existing fine imagery.
 */
interface PhotoSource { url: string; sha256: string; width: number; source: number[]; target: number[]; }
interface PhotoSourcePart { key: string; needed: number; read: number[]; write: number[]; source?: number[]; target?: number[]; }
interface PhotoCellRequest { key: string; bounds: number[]; level: number; id?: number; }
interface PhotoCacheWrite { cache: GlobeSurfaceCache; key: string; blob: Blob; }
class GlobePhotoWorker {
    private readonly ready: Promise<void>;
    private readonly cells: GlobeSurfaceCache;
    private readonly sources: GlobeSurfaceCache;
    private parts: import('./globe_texture_cache.js').GlobeTextureCache<ImageBitmap> | null = null;
    private running = false;
    private active: AbortController | null = null;
    private pending: PhotoCellRequest | null = null;
    // The ring owns browser Blobs only; native dictionaries still own decoded-block lookup and admission.
    private readonly writes: (PhotoCacheWrite | undefined)[] = new Array(24);
    private writeHead = 0;
    private writeCount = 0;
    private writing = false;

    constructor(private readonly port: {
        importScripts(...urls: string[]): void;
        postMessage(message: unknown, transfer?: Transferable[]): void;
        onmessage: ((event: MessageEvent<PhotoCellRequest>) => void) | null;
    }) {
        port.importScripts('/js/transport.js', '/js/globe_surface_cache.js');
        this.ready = labWire.start(false);
        this.cells = new GlobeSurfaceCache(512);
        this.sources = new GlobeSurfaceCache(514);
        port.onmessage = event => {
            this.pending = event.data;
            if (this.active) this.active.abort();
            else void this.load();
        };
    }

    private paint(context: OffscreenCanvasRenderingContext2D, image: CanvasImageSource, part: PhotoSourcePart): void {
        const [sx, sy, sw, sh] = part.source!, [dx, dy, dw, dh] = part.target!;
        context.drawImage(image, sx, sy, sw, sh, dx, dy, dw, dh);
    }

    private save(cache: GlobeSurfaceCache, key: string, blob: Blob): void {
        // Retain at most twenty-four pending blocks plus the active write, below 26 MiB including source gutters.
        if (this.writeCount === this.writes.length) {
            this.writes[this.writeHead] = undefined;
            this.writeHead = (this.writeHead + 1) % this.writes.length;
            --this.writeCount;
        }
        this.writes[(this.writeHead + this.writeCount++) % this.writes.length] = {cache, key, blob};
        if (!this.writing) void this.persist();
    }

    private async persist(): Promise<void> {
        this.writing = true;
        try {
            while (this.writeCount) {
                const write = this.writes[this.writeHead]!;
                this.writes[this.writeHead] = undefined;
                this.writeHead = (this.writeHead + 1) % this.writes.length;
                --this.writeCount;
                try { await write.cache.put(write.key, write.blob); }
                catch { /* Optional browser storage cannot reject a visible photograph. */ }
            }
        } finally { this.writing = false; }
    }

    private async source(context: OffscreenCanvasRenderingContext2D, photo: PhotoSource,
                         signal: AbortSignal): Promise<void> {
        if (!/^\/(?:earth_regions\/\d{2,5}|earth_tiles\/\d{2}-\d{2})\.png$/.test(photo.url) ||
            !/^[a-f0-9]{64}$/.test(photo.sha256)) throw new Error('Invalid photographic source');
        const parts = labDOM.call('lab_photo_source', photo) as PhotoSourcePart[] | null;
        if (!parts || parts.length > 64) throw new Error('Invalid source-cell plan');
        for (let i = 0; i < parts.length; ++i) {
            signal.throwIfAborted();
            const part = parts[i];
            if (!part.needed) continue;
            let image = this.parts!.get(part.key);
            if (!image) {
                const saved = await this.sources.get(part.key, signal);
                signal.throwIfAborted();
                if (saved) {
                    const bytes = new Uint8ClampedArray(await saved.arrayBuffer());
                    image = await createImageBitmap(new ImageData(bytes, 514, 514));
                    if (signal.aborted) { image.close(); signal.throwIfAborted(); }
                    if (!this.parts!.put(part.key, image, 514 * 514 * 4)) { image.close(); throw new Error('Source cache unavailable'); }
                }
            }
            if (image) { this.paint(context, image, part); continue; }
            await this.split(context, photo, parts, i, signal);
            return;
        }
    }

    private async split(context: OffscreenCanvasRenderingContext2D, photo: PhotoSource,
                        parts: PhotoSourcePart[], first: number, signal: AbortSignal): Promise<void> {
        const response = await fetch(photo.url, {signal: AbortSignal.any([signal, AbortSignal.timeout(8000)])});
        if (!response.ok || response.headers.get('ETag') !== `"sha256-${photo.sha256}"`)
            throw new Error('Photographic source changed');
        const encoded = await response.arrayBuffer();
        signal.throwIfAborted();
        if (!encoded.byteLength || encoded.byteLength > 64 * 1024 * 1024) throw new Error('Photograph exceeds its byte limit');
        const image = await createImageBitmap(new Blob([encoded], {type: 'image/png'}));
        const canvas = new OffscreenCanvas(514, 514), patch = canvas.getContext('2d')!;
        try {
            signal.throwIfAborted();
            if (image.width !== photo.width || image.height !== photo.width || !patch) throw new Error('Invalid photograph size');
            for (let i = first; i < parts.length; ++i) {
                signal.throwIfAborted();
                const part = parts[i];
                if (!part.needed) continue;
                const [sx, sy, sw, sh] = part.read, [dx, dy, dw, dh] = part.write;
                patch.clearRect(0, 0, 514, 514);
                patch.drawImage(image, sx, sy, sw, sh, dx, dy, dw, dh);
                const pixels = patch.getImageData(0, 0, 514, 514);
                this.paint(context, canvas, part);
                const bitmap = await createImageBitmap(canvas);
                if (signal.aborted) { bitmap.close(); signal.throwIfAborted(); }
                if (!this.parts!.put(part.key, bitmap, 514 * 514 * 4)) bitmap.close();
                this.save(this.sources, part.key, new Blob([pixels.data], {type: 'application/octet-stream'}));
            }
        } finally { image.close(); canvas.width = canvas.height = 1; }
    }

    private async load(): Promise<void> {
        if (this.running || !this.pending) return;
        const request = this.pending;
        this.pending = null;
        this.running = true;
        const controller = this.active = new AbortController(), signal = controller.signal;
        const canvas = new OffscreenCanvas(512, 512);
        try {
            await this.ready;
            signal.throwIfAborted();
            if (!this.parts) {
                const url = '/js/globe_texture_cache.js';
                const {GlobeTextureCache} = await import(url) as typeof import('./globe_texture_cache.js');
                this.parts = new GlobeTextureCache(64, 64 * 1024 * 1024, bitmap => bitmap.close());
            }
            const [west, south, east, north] = request.bounds;
            const response = await fetch('/almanac-rivers', {
                method: 'POST', headers: {'Content-Type': 'application/x-protobuf'},
                body: labWire.encode({west, south, east, north, format: 'photo-plan'}),
                signal: AbortSignal.any([signal, AbortSignal.timeout(15000)])
            });
            const bytes = await response.arrayBuffer();
            signal.throwIfAborted();
            if (!response.ok || bytes.byteLength > 8 * 1024 * 1024) throw new Error('Photographic plan unavailable');
            const plan = labWire.decode(bytes) as {ok: boolean; key: string; photos: PhotoSource[]};
            if (!plan.ok || typeof plan.key !== 'string' || !Array.isArray(plan.photos) || !plan.photos.length || plan.photos.length > 68)
                throw new Error('Invalid photographic plan');
            const saved = await this.cells.get(plan.key, signal);
            signal.throwIfAborted();
            if (saved) {
                const pixels = await saved.arrayBuffer();
                signal.throwIfAborted();
                this.port.postMessage({key: request.key, id: request.id, pixels}, [pixels]);
                return;
            }
            const context = canvas.getContext('2d');
            if (!context) throw new Error('Photographic raster unavailable');
            context.imageSmoothingQuality = 'high';
            for (const photo of plan.photos) await this.source(context, photo, signal);
            signal.throwIfAborted();
            const pixels = context.getImageData(0, 0, 512, 512).data;
            for (let start = 0; start < pixels.byteLength; start += 512 * 64 * 4) {
                const chunk = new Uint8Array(pixels.buffer, start, Math.min(512 * 64 * 4, pixels.byteLength - start));
                const pointer = labDOM.call('lab_texture_premultiply', chunk) as number;
                if (!pointer) throw new Error('Native photographic preparation failed');
                chunk.set(new Uint8Array(labWire.exports().memory.buffer, pointer, chunk.byteLength));
            }
            const blob = new Blob([pixels], {type: 'application/octet-stream'});
            const buffer = pixels.buffer;
            this.port.postMessage({key: request.key, id: request.id, pixels: buffer}, [buffer]);
            this.save(this.cells, plan.key, blob);
        } catch (error) {
            if (!signal.aborted) this.port.postMessage({key: request.key, id: request.id, error: String(error)});
        } finally {
            canvas.width = canvas.height = 1;
            this.active = null; this.running = false;
            if (this.pending) void this.load();
        }
    }
}
new GlobePhotoWorker(globalThis as unknown as ConstructorParameters<typeof GlobePhotoWorker>[0]);
