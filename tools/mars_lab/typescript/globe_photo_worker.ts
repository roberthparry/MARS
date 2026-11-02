/**
 * @file globe_photo_worker.ts
 * @brief Off-thread preparation of fixed photographic cells from reusable decoded source blocks.
 *
 * Decodes one PNG at a time, splits it into native-planned 512-pixel cores with one-pixel gutters and persists
 * raw blocks. Only needed small blocks stay decoded. Completed geographic cells are transferred directly to WebGL;
 * coarse regional overviews are never published as replacements for existing fine imagery.
 */
interface PhotoSource { url: string; sha256: string; width: number; source: number[]; target: number[]; }
interface PhotoSourcePart { key: string; needed: number; read: number[]; write: number[]; source?: number[]; target?: number[]; }
interface PhotoCellRequest { key: string; bounds: number[]; level: number; }
class GlobePhotoWorker {
    private readonly ready: Promise<void>;
    private readonly cells: GlobeSurfaceCache;
    private readonly sources: GlobeSurfaceCache;
    private parts: import('./globe_texture_cache.js').GlobeTextureCache<ImageBitmap> | null = null;
    private running = false;

    constructor(private readonly port: {
        importScripts(...urls: string[]): void;
        postMessage(message: unknown, transfer?: Transferable[]): void;
        onmessage: ((event: MessageEvent<PhotoCellRequest>) => void) | null;
    }) {
        port.importScripts('/js/transport.js', '/js/globe_surface_cache.js');
        this.ready = labWire.start(false);
        this.cells = new GlobeSurfaceCache(512);
        this.sources = new GlobeSurfaceCache(514);
        port.onmessage = event => { void this.load(event.data); };
    }

    private paint(context: OffscreenCanvasRenderingContext2D, image: CanvasImageSource, part: PhotoSourcePart): void {
        const [sx, sy, sw, sh] = part.source!, [dx, dy, dw, dh] = part.target!;
        context.drawImage(image, sx, sy, sw, sh, dx, dy, dw, dh);
    }

    private async source(context: OffscreenCanvasRenderingContext2D, photo: PhotoSource): Promise<void> {
        if (!/^\/(?:earth_regions\/\d{2,5}|earth_tiles\/\d{2}-\d{2})\.png$/.test(photo.url) ||
            !/^[a-f0-9]{64}$/.test(photo.sha256)) throw new Error('Invalid photographic source');
        const parts = labDOM.call('lab_photo_source', photo) as PhotoSourcePart[] | null;
        if (!parts || parts.length > 64) throw new Error('Invalid source-cell plan');
        for (let i = 0; i < parts.length; ++i) {
            const part = parts[i];
            if (!part.needed) continue;
            let image = this.parts!.get(part.key);
            if (!image) {
                const saved = await this.sources.get(part.key);
                if (saved) {
                    const bytes = new Uint8ClampedArray(await saved.arrayBuffer());
                    image = await createImageBitmap(new ImageData(bytes, 514, 514));
                    if (!this.parts!.put(part.key, image, 514 * 514 * 4)) { image.close(); throw new Error('Source cache unavailable'); }
                }
            }
            if (image) { this.paint(context, image, part); continue; }
            await this.split(context, photo, parts, i);
            return;
        }
    }

    private async split(context: OffscreenCanvasRenderingContext2D, photo: PhotoSource,
                        parts: PhotoSourcePart[], first: number): Promise<void> {
        const response = await fetch(photo.url, {signal: AbortSignal.timeout(8000)});
        if (!response.ok || response.headers.get('ETag') !== `"sha256-${photo.sha256}"`)
            throw new Error('Photographic source changed');
        const encoded = await response.arrayBuffer();
        if (!encoded.byteLength || encoded.byteLength > 64 * 1024 * 1024) throw new Error('Photograph exceeds its byte limit');
        const image = await createImageBitmap(new Blob([encoded], {type: 'image/png'}));
        const canvas = new OffscreenCanvas(514, 514), patch = canvas.getContext('2d')!;
        try {
            if (image.width !== photo.width || image.height !== photo.width || !patch) throw new Error('Invalid photograph size');
            for (let i = 0; i < parts.length; ++i) {
                const part = parts[i];
                const [sx, sy, sw, sh] = part.read, [dx, dy, dw, dh] = part.write;
                patch.clearRect(0, 0, 514, 514);
                patch.drawImage(image, sx, sy, sw, sh, dx, dy, dw, dh);
                const pixels = patch.getImageData(0, 0, 514, 514);
                if (part.needed) {
                    if (i >= first) this.paint(context, canvas, part);
                    const bitmap = await createImageBitmap(canvas);
                    if (!this.parts!.put(part.key, bitmap, 514 * 514 * 4)) bitmap.close();
                }
                await this.sources.put(part.key, new Blob([pixels.data], {type: 'application/octet-stream'}));
                // A bounded source read may finish after its originating camera has moved; all cells remain reusable.
                await new Promise(resolve => setTimeout(resolve, 0));
            }
        } finally { image.close(); canvas.width = canvas.height = 1; }
    }

    private async load(request: PhotoCellRequest): Promise<void> {
        if (this.running) return;
        this.running = true;
        const canvas = new OffscreenCanvas(512, 512);
        try {
            await this.ready;
            if (!this.parts) {
                const url = '/js/globe_texture_cache.js';
                const {GlobeTextureCache} = await import(url) as typeof import('./globe_texture_cache.js');
                this.parts = new GlobeTextureCache(64, 64 * 1024 * 1024, bitmap => bitmap.close());
            }
            const [west, south, east, north] = request.bounds;
            const response = await fetch('/almanac-rivers', {
                method: 'POST', headers: {'Content-Type': 'application/x-protobuf'},
                body: labWire.encode({west, south, east, north, format: 'photo-plan'}), signal: AbortSignal.timeout(15000)
            });
            const bytes = await response.arrayBuffer();
            if (!response.ok || bytes.byteLength > 8 * 1024 * 1024) throw new Error('Photographic plan unavailable');
            const plan = labWire.decode(bytes) as {ok: boolean; key: string; photos: PhotoSource[]};
            if (!plan.ok || typeof plan.key !== 'string' || !Array.isArray(plan.photos) || !plan.photos.length || plan.photos.length > 68)
                throw new Error('Invalid photographic plan');
            const saved = await this.cells.get(plan.key);
            if (saved) {
                const pixels = await saved.arrayBuffer();
                this.port.postMessage({key: request.key, pixels}, [pixels]);
                return;
            }
            const context = canvas.getContext('2d');
            if (!context) throw new Error('Photographic raster unavailable');
            context.imageSmoothingQuality = 'high';
            for (const photo of plan.photos) await this.source(context, photo);
            const pixels = context.getImageData(0, 0, 512, 512).data;
            for (let start = 0; start < pixels.byteLength; start += 512 * 64 * 4) {
                const chunk = new Uint8Array(pixels.buffer, start, Math.min(512 * 64 * 4, pixels.byteLength - start));
                const pointer = labDOM.call('lab_texture_premultiply', chunk) as number;
                if (!pointer) throw new Error('Native photographic preparation failed');
                chunk.set(new Uint8Array(labWire.exports().memory.buffer, pointer, chunk.byteLength));
            }
            const blob = new Blob([pixels], {type: 'application/octet-stream'});
            const buffer = pixels.buffer;
            this.port.postMessage({key: request.key, pixels: buffer}, [buffer]);
            void this.cells.put(plan.key, blob);
        } catch (error) { this.port.postMessage({key: request.key, error: String(error)}); }
        finally { canvas.width = canvas.height = 1; this.running = false; }
    }
}
new GlobePhotoWorker(globalThis as unknown as ConstructorParameters<typeof GlobePhotoWorker>[0]);
