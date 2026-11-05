/**
 * @file globe_photo_worker.ts
 * @brief Off-thread preparation of fixed photographic cells from reusable decoded source blocks.
 *
 * Reads at most two PNGs concurrently at native-selected resolutions, with one bounded read-ahead bitmap.
 * Photograph handles and source cores share one native-accounted 64 MiB cache, with one-pixel gutters.
 * A bounded browser-resource queue exposes queued raw cells immediately and persists one asynchronous block at a time.
 * Ready source crops transfer directly to WebGL before the whole cell finishes; only completed cells enter persistent storage.
 * Coarse regional overviews are never published as replacements for existing fine imagery. One bounded decoded snapshot
 * supplies unused source cores during idle gaps, avoiding another PNG decode after neighbouring crops or restarts.
 * Native alpha bitsets skip fully hidden source rectangles; only disjoint sources are read ahead concurrently.
 */
interface PhotoSource { url: string; sha256: string; width: number; source: number[]; target: number[]; }
interface PhotoSourcePart { key: string; needed: number; read: number[]; write: number[]; source?: number[]; target?: number[]; }
interface PhotoSourcePlan {
    key: string; prefix: string; width: number; retain: boolean; cached: boolean;
    source: number[]; parts: PhotoSourcePart[];
}
interface PhotoCellRequest { key: string; bounds: number[]; level: number; id?: number; }
interface PhotoCacheWrite { cache: GlobeSurfaceCache; key: string; blob: Blob; }
interface PhotoWarmSource { key: string; image: ImageBitmap; parts: PhotoSourcePart[]; next: number; }
interface PhotoReadAhead {
    plan: PhotoSourcePlan; pending: Promise<ImageBitmap | null>; image: ImageBitmap | null;
    closed: boolean; failed?: unknown;
}
class GlobePhotoWorker {
    private readonly ready: Promise<void>;
    private readonly cells: GlobeSurfaceCache;
    private readonly sources: GlobeSurfaceCache;
    private parts: import('./globe_texture_cache.js').GlobeTextureCache<ImageBitmap> | null = null;
    private savedParts: import('./globe_texture_cache.js').GlobeTextureCache<boolean> | null = null;
    private savedPhotos: import('./globe_texture_cache.js').GlobeTextureCache<boolean> | null = null;
    private bufferedSources: import('./globe_texture_cache.js').GlobeTextureCache<Blob> | null = null;
    private running = false;
    private active: AbortController | null = null;
    private pending: PhotoCellRequest | null = null;
    // The ring owns browser Blobs only; native dictionaries still own decoded-block lookup and admission.
    private readonly writes: (PhotoCacheWrite | undefined)[] = new Array(24);
    private writeHead = 0;
    private writeCount = 0;
    private writing = false;
    private writeTimer: ReturnType<typeof setTimeout> | null = null;
    private warmSource: PhotoWarmSource | null = null;
    private warmCloning = false;
    private warmTimer: ReturnType<typeof setTimeout> | null = null;
    private warmCanvas: OffscreenCanvas | null = null;
    private opaque = false;

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
            if (this.warmTimer !== null) clearTimeout(this.warmTimer);
            this.warmTimer = null;
            this.pending = event.data;
            if (this.active) this.active.abort();
            else void this.load();
        };
    }

    private paint(context: OffscreenCanvasRenderingContext2D, image: CanvasImageSource,
                  part: Pick<PhotoSourcePart, 'source' | 'target'>): void {
        const [sx, sy, sw, sh] = part.source!, [dx, dy, dw, dh] = part.target!;
        context.drawImage(image, sx, sy, sw, sh, dx, dy, dw, dh);
    }

    private save(cache: GlobeSurfaceCache, key: string, blob: Blob): void {
        // Retain at most twenty-four pending blocks plus the active write, below 26 MiB including source gutters.
        if (this.writeCount === this.writes.length) {
            const dropped = this.writes[this.writeHead];
            if (dropped?.cache === this.sources) {
                this.savedParts!.remove(dropped.key);
            }
            if (dropped) this.bufferedSources!.remove(dropped.key);
            this.writes[this.writeHead] = undefined;
            this.writeHead = (this.writeHead + 1) % this.writes.length;
            --this.writeCount;
        }
        if (cache === this.sources) {
            this.savedParts!.put(key, true, 1);
        }
        this.bufferedSources!.put(key, blob, blob.size);
        this.writes[(this.writeHead + this.writeCount++) % this.writes.length] = {cache, key, blob};
        this.schedulePersist();
    }

    private schedulePersist(): void {
        if (this.writing || !this.writeCount || this.writeTimer !== null) return;
        this.writeTimer = setTimeout(() => {
            this.writeTimer = null;
            void this.persist();
        }, this.running || this.pending ? 100 : 0);
    }

    private async persist(): Promise<void> {
        if (this.writing || !this.writeCount) return;
        this.writing = true;
        try {
            const write = this.writes[this.writeHead]!;
            this.writes[this.writeHead] = undefined;
            this.writeHead = (this.writeHead + 1) % this.writes.length;
            --this.writeCount;
            try { await write.cache.put(write.key, write.blob); }
            catch { /* Optional browser storage cannot reject a visible photograph. */ }
            finally { this.bufferedSources!.remove(write.key); }
        } finally { this.writing = false; this.schedulePersist(); this.scheduleWarm(); }
    }

    private async retainPhotograph(photo: PhotoSource, plan: PhotoSourcePlan, image: ImageBitmap): Promise<void> {
        if (this.warmSource || this.warmCloning || this.savedPhotos!.get(plan.key)) return;
        this.warmCloning = true;
        let copy: ImageBitmap | null = null;
        try {
            if (!await this.sources.available()) return;
            copy = await createImageBitmap(image);
            const whole = labDOM.call('lab_photo_source', {...photo,
                source: [0, 0, photo.width, photo.width], target: [0, 0, plan.width, plan.width]}) as PhotoSourcePlan | null;
            if (!whole || whole.key !== plan.key || !whole.parts.length || whole.parts.length > 64) return;
            this.warmSource = {key: plan.key, image: copy, parts: whole.parts, next: 0};
            copy = null;
            this.scheduleWarm();
        } catch { /* Optional background storage never rejects visible imagery. */ }
        finally { copy?.close(); this.warmCloning = false; }
    }

    private scheduleWarm(): void {
        if (!this.warmSource || this.warmTimer !== null || this.running || this.pending ||
            this.writing || this.writeCount) return;
        this.warmTimer = setTimeout(() => this.warm(), 100);
    }

    private warm(): void {
        this.warmTimer = null;
        const source = this.warmSource;
        if (!source || this.running || this.pending || this.writing || this.writeCount) return;
        try {
            const part = source.parts[source.next++];
            if (part && !this.savedParts!.get(part.key)) {
                const canvas = this.warmCanvas ??= new OffscreenCanvas(514, 514);
                const context = canvas.getContext('2d');
                if (!context) throw new Error('Source cache raster unavailable');
                const [sx, sy, sw, sh] = part.read, [dx, dy, dw, dh] = part.write;
                context.clearRect(0, 0, 514, 514);
                context.drawImage(source.image, sx, sy, sw, sh, dx, dy, dw, dh);
                const pixels = context.getImageData(0, 0, 514, 514);
                this.save(this.sources, part.key, new Blob([pixels.data], {type: 'application/octet-stream'}));
            }
            if (source.next >= source.parts.length) {
                this.savedPhotos!.put(source.key, true, 1);
                source.image.close(); this.warmSource = null;
                if (this.warmCanvas) this.warmCanvas.width = this.warmCanvas.height = 1;
                this.warmCanvas = null;
            }
        } catch {
            source.image.close(); this.warmSource = null;
            if (this.warmCanvas) this.warmCanvas.width = this.warmCanvas.height = 1;
            this.warmCanvas = null;
        }
        this.scheduleWarm();
    }

    private async source(context: OffscreenCanvasRenderingContext2D, photo: PhotoSource,
                         signal: AbortSignal, ahead: PhotoReadAhead | null): Promise<void> {
        if (ahead) { await ahead.pending; if (ahead.failed) throw ahead.failed; }
        const plan = ahead?.plan ?? await this.storedPlan(photo, this.sourcePlan(photo), signal);
        signal.throwIfAborted();
        const photograph = this.parts!.get(plan.key);
        if (photograph) {
            this.paint(context, photograph, {source: plan.source, target: photo.target});
            await this.split(context, photo, plan, 0, signal, false);
            return;
        }
        const parts = plan.parts;
        for (let i = 0; i < parts.length; ++i) {
            signal.throwIfAborted();
            const part = parts[i];
            if (!part.needed) continue;
            let image = this.parts!.get(part.key);
            if (!image) {
                const saved = this.bufferedSources!.get(part.key) ?? await this.sources.get(part.key, signal);
                signal.throwIfAborted();
                if (saved) {
                    this.savedParts!.put(part.key, true, 1);
                    const bytes = new Uint8ClampedArray(await saved.arrayBuffer());
                    image = await createImageBitmap(new ImageData(bytes, 514, 514));
                    if (signal.aborted) { image.close(); signal.throwIfAborted(); }
                    if (!this.parts!.put(part.key, image, 514 * 514 * 4)) { image.close(); throw new Error('Source cache unavailable'); }
                } else {
                    this.savedParts!.remove(part.key);
                    this.savedPhotos!.remove(plan.key);
                }
            }
            if (image) { this.paint(context, image, part); continue; }
            await this.split(context, photo, plan, i, signal, true, ahead);
            return;
        }
    }

    private sourcePlan(photo: PhotoSource, stored?: string[]): PhotoSourcePlan {
        if (!/^\/(?:earth_regions\/\d{2,5}|earth_tiles(?:_detail)?\/\d{2}-\d{2})\.png$/.test(photo.url) ||
            !/^[a-f0-9]{64}$/.test(photo.sha256)) throw new Error('Invalid photographic source');
        const plan = this.parts!.source(photo, this.bufferedSources!, stored) as PhotoSourcePlan | null;
        if (!plan || plan.parts.length > 64) throw new Error('Invalid source-cell plan');
        return plan;
    }

    private async storedPlan(photo: PhotoSource, plan: PhotoSourcePlan, signal: AbortSignal): Promise<PhotoSourcePlan> {
        if (plan.cached) return plan;
        const keys = await this.sources.keys(plan.prefix, signal);
        signal.throwIfAborted();
        return keys.length ? this.sourcePlan(photo, keys) : plan;
    }

    private async readPhotograph(photo: PhotoSource, plan: PhotoSourcePlan, signal: AbortSignal): Promise<ImageBitmap> {
        const response = await fetch(photo.url, {signal: AbortSignal.any([signal, AbortSignal.timeout(8000)])});
        if (!response.ok || response.headers.get('ETag') !== `"sha256-${photo.sha256}"`)
            throw new Error('Photographic source changed');
        const encoded = await response.arrayBuffer();
        signal.throwIfAborted();
        if (encoded.byteLength < 24 || encoded.byteLength > 64 * 1024 * 1024 ||
            !labDOM.call('lab_photo_source_header', new Uint8Array(encoded, 0, 24), photo.width))
            throw new Error('Invalid photograph header or byte limit');
        const image = await createImageBitmap(new Blob([encoded], {type: 'image/png'}),
            {resizeWidth: plan.width, resizeHeight: plan.width, resizeQuality: 'high'});
        if (signal.aborted) { image.close(); signal.throwIfAborted(); }
        return image;
    }

    private readAhead(photo: PhotoSource, signal: AbortSignal): PhotoReadAhead | null {
        const plan = this.sourcePlan(photo);
        if (!plan.retain || this.parts!.get(plan.key)) return null;
        const ahead: PhotoReadAhead = {plan, pending: Promise.resolve(null), image: null, closed: false};
        ahead.pending = (async () => {
            ahead.plan = await this.storedPlan(photo, plan, signal);
            if (!ahead.plan.retain) return null;
            for (const part of ahead.plan.parts) {
                if (!part.needed || this.parts!.get(part.key)) continue;
                if (this.bufferedSources!.get(part.key) || await this.sources.get(part.key, signal)) continue;
                signal.throwIfAborted();
                const image = await this.readPhotograph(photo, ahead.plan, signal);
                if (ahead.closed) { image.close(); return null; }
                ahead.image = image;
                return image;
            }
            return null;
        })().catch(error => { ahead.failed = error; return null; });
        return ahead;
    }

    private releaseAhead(ahead: PhotoReadAhead | null): void {
        if (!ahead) return;
        ahead.closed = true;
        ahead.image?.close(); ahead.image = null;
    }

    private pixels(context: OffscreenCanvasRenderingContext2D): Uint8ClampedArray<ArrayBuffer> {
        const pixels = context.getImageData(0, 0, 512, 512).data;
        this.opaque = true;
        for (let start = 0; start < pixels.byteLength; start += 512 * 64 * 4) {
            const chunk = new Uint8Array(pixels.buffer, start, Math.min(512 * 64 * 4, pixels.byteLength - start));
            labDOM.call('lab_photo_coverage_rows', chunk, start / (512 * 4));
            const pointer = labDOM.call('lab_texture_premultiply', chunk) as number;
            if (!pointer) throw new Error('Native photographic preparation failed');
            this.opaque = Boolean(labDOM.call('lab_texture_opaque')) && this.opaque;
            chunk.set(new Uint8Array(labWire.exports().memory.buffer, pointer, chunk.byteLength));
        }
        return pixels;
    }

    private async split(context: OffscreenCanvasRenderingContext2D, photo: PhotoSource,
                        plan: PhotoSourcePlan, first: number, signal: AbortSignal, paint = true,
                        ahead: PhotoReadAhead | null = null): Promise<void> {
        let resident = this.parts!.getSlot(plan.key);
        let image = resident?.resource;
        if (!image) {
            if (ahead) {
                image = await ahead.pending ?? undefined;
                ahead.image = null;
                if (ahead.failed) throw ahead.failed;
            }
            image ??= await this.readPhotograph(photo, plan, signal);
            const slot = plan.retain ? this.parts!.putSlot(plan.key, image!, plan.width * plan.width * 4) : -1;
            if (slot >= 0) resident = {resource: image!, slot};
        }
        const canvas = new OffscreenCanvas(514, 514), patch = canvas.getContext('2d')!;
        try {
            signal.throwIfAborted();
            if (!patch) throw new Error('Photographic block unavailable');
            if (paint && first === 0) {
                // Paint a newly decoded photograph once. Reassembling its fractional block rectangles
                // adds unnecessary drawing and can leave partly transparent seams between opaque crops.
                this.paint(context, image!, {source: plan.source, target: photo.target});
                paint = false;
            }
            const parts = plan.parts;
            for (let i = first; i < parts.length; ++i) {
                signal.throwIfAborted();
                const part = parts[i];
                if (!part.needed) continue;
                const persist = !this.savedParts!.get(part.key);
                if (!paint && !persist) continue;
                const [sx, sy, sw, sh] = part.read, [dx, dy, dw, dh] = part.write;
                patch.clearRect(0, 0, 514, 514);
                patch.drawImage(image!, sx, sy, sw, sh, dx, dy, dw, dh);
                if (paint) this.paint(context, canvas, part);
                if (!persist) continue;
                const pixels = patch.getImageData(0, 0, 514, 514);
                if (!resident) {
                    const bitmap = await createImageBitmap(canvas);
                    if (signal.aborted) { bitmap.close(); signal.throwIfAborted(); }
                    if (!this.parts!.put(part.key, bitmap, 514 * 514 * 4)) bitmap.close();
                }
                this.save(this.sources, part.key, new Blob([pixels.data], {type: 'application/octet-stream'}));
            }
        } finally {
            if (resident) void this.retainPhotograph(photo, plan, image!);
            else image!.close();
            canvas.width = canvas.height = 1;
        }
    }

    private async load(): Promise<void> {
        if (this.running || !this.pending) return;
        const request = this.pending;
        this.pending = null;
        this.running = true;
        const controller = this.active = new AbortController(), signal = controller.signal;
        const canvas = new OffscreenCanvas(512, 512);
        let ahead: PhotoReadAhead | null = null;
        try {
            await this.ready;
            signal.throwIfAborted();
            if (!this.parts) {
                const url = '/js/globe_texture_cache.js';
                const {GlobeTextureCache} = await import(url) as typeof import('./globe_texture_cache.js');
                this.parts = new GlobeTextureCache(64, 64 * 1024 * 1024, bitmap => bitmap.close());
                this.savedParts = new GlobeTextureCache(64, 64, () => {});
                this.savedPhotos = new GlobeTextureCache(64, 64, () => {});
                this.bufferedSources = new GlobeTextureCache(25, 26 * 1024 * 1024, () => {});
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
            const plan = labWire.decode(bytes) as {ok: boolean; key: string; alpha?: boolean; photos: PhotoSource[]};
            if (!plan.ok || typeof plan.key !== 'string' || !Array.isArray(plan.photos) || !plan.photos.length || plan.photos.length > 68)
                throw new Error('Invalid photographic plan');
            const saved = this.bufferedSources!.get(plan.key) ?? await this.cells.get(plan.key, signal);
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
            if (plan.alpha) context.globalCompositeOperation = 'destination-over';
            labDOM.call('lab_photo_coverage_rows', new Uint8Array(0), 0);
            let completed: Uint8ClampedArray<ArrayBuffer> | null = null;
            for (let position = 0; position < plan.photos.length; ++position) {
                const i = plan.alpha ? plan.photos.length - 1 - position : position;
                const next = i + (plan.alpha ? -1 : 1);
                const current = ahead;
                ahead = null;
                if (plan.alpha && labDOM.call('lab_photo_target_covered', plan.photos[i].target)) {
                    this.releaseAhead(current);
                    continue;
                }
                if (next >= 0 && next < plan.photos.length && (!plan.alpha ||
                    (!labDOM.call('lab_photo_target_covered', plan.photos[next].target) &&
                     !labDOM.call('lab_photo_targets_overlap', plan.photos[i].target, plan.photos[next].target))))
                    ahead = this.readAhead(plan.photos[next], signal);
                try { await this.source(context, plan.photos[i], signal, current); }
                finally { this.releaseAhead(current); }
                signal.throwIfAborted();
                const pixels = this.pixels(context);
                if (position + 1 === plan.photos.length || (plan.alpha && this.opaque)) {
                    completed = pixels; break;
                }
                const buffer = pixels.buffer;
                // Older atlas controllers ignore this distinct progress key and still await the final cell.
                this.port.postMessage({key: request.key + ':partial', id: request.id, partial: true, pixels: buffer}, [buffer]);
            }
            signal.throwIfAborted();
            // The remaining sources may all be hidden although the cell still contains genuine observation gaps.
            completed ??= this.pixels(context);
            const pixels = completed;
            const blob = new Blob([pixels], {type: 'application/octet-stream'});
            const buffer = pixels.buffer;
            this.port.postMessage({key: request.key, id: request.id, pixels: buffer}, [buffer]);
            this.save(this.cells, plan.key, blob);
        } catch (error) {
            if (!signal.aborted) this.port.postMessage({key: request.key, id: request.id, error: String(error)});
        } finally {
            controller.abort();
            this.releaseAhead(ahead);
            canvas.width = canvas.height = 1;
            this.active = null; this.running = false;
            if (this.pending) void this.load();
            else { this.schedulePersist(); this.scheduleWarm(); }
        }
    }
}
new GlobePhotoWorker(globalThis as unknown as ConstructorParameters<typeof GlobePhotoWorker>[0]);
