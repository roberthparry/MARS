/**
 * @file globe_photo_atlas.ts
 * @brief WebGL upload and worker transport for native-selected fixed geographic photographs.
 *
 * One bounded atlas retains 512-pixel cells. WASM owns keys, visible-page selection, parent reuse and eviction.
 * A tiny indirection texture lets the existing globe shader sample those cells directly during camera movement.
 */
import {GlobeTextureCache} from './globe_texture_cache.js';
import type {GlobePhotoCell, GlobePhotoPage} from './globe_texture_cache.js';

export class GlobePhotoAtlas {
    private readonly cells: GlobeTextureCache<number>;
    private readonly atlas: WebGLTexture;
    private readonly page: WebGLTexture;
    private readonly columns: number;
    private worker: Worker | null = null;
    private busy = false;
    private closed = false;
    private protect: number[] = [];
    private failed = '';
    private retryAt = 0;
    private retryTimer = 0;
    private current: (GlobePhotoCell & {id: number}) | null = null;
    private serial = 0;
    private destination: {bounds: number[]; pixels: number} | null = null;
    private ahead: number[] | null = null;
    private pace = 1;

    constructor(private readonly gl: WebGLRenderingContext, private readonly redraw: () => void, maximumSize = 4096) {
        const size = Math.min(maximumSize, gl.getParameter(gl.MAX_TEXTURE_SIZE) as number);
        if (size < 2048) throw new Error('Photographic atlas is unavailable');
        this.columns = size >= 4096 ? 8 : 4;
        this.cells = new GlobeTextureCache(this.columns * this.columns,
            this.columns * this.columns * 512 * 512 * 4, () => {});
        const atlas = gl.createTexture(), page = gl.createTexture();
        if (!atlas || !page) {
            gl.deleteTexture(atlas); gl.deleteTexture(page); this.cells.dispose();
            throw new Error('Photographic atlas allocation failed');
        }
        this.atlas = atlas; this.page = page;
        const active = gl.getParameter(gl.ACTIVE_TEXTURE) as number;
        try {
            for (const [texture, unit, width, height, filter] of [
                [atlas, gl.TEXTURE2, this.columns * 512, this.columns * 512, gl.LINEAR],
                [page, gl.TEXTURE7, 16, 16, gl.NEAREST]
            ] as const) {
                gl.activeTexture(unit); gl.bindTexture(gl.TEXTURE_2D, texture);
                gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, filter);
                gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, filter);
                gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
                gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
                gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, width, height, 0, gl.RGBA, gl.UNSIGNED_BYTE, null);
            }
            if (gl.getError() !== gl.NO_ERROR) throw new Error('Photographic atlas exceeds the graphics budget');
        } catch (error) { this.dispose(); throw error; }
        finally { gl.activeTexture(active); }
    }

    bind(bounds: number[], pixels: number): GlobePhotoPage | null {
        if (this.closed || this.gl.isContextLost()) return null;
        const avoid = performance.now() < this.retryAt ? this.failed : '';
        const target = this.destination;
        const size = Math.min(pixels, (this.columns - 2) * 256);
        const plan = target ? this.cells.journey(bounds, size, target.bounds,
            Math.min(target.pixels, (this.columns - 2) * 256), this.ahead, avoid, !this.busy, this.current) :
            this.cells.page(bounds, size, avoid, [], this.current);
        this.protect = plan?.protect || [];
        this.pace = plan?.pace ?? 1;
        const gl = this.gl;
        gl.activeTexture(gl.TEXTURE2); gl.bindTexture(gl.TEXTURE_2D, this.atlas);
        gl.activeTexture(gl.TEXTURE7); gl.bindTexture(gl.TEXTURE_2D, this.page);
        if (plan?.visible) gl.texSubImage2D(gl.TEXTURE_2D, 0, 0, 0, 16, 16, gl.RGBA, gl.UNSIGNED_BYTE,
            new Uint8Array(labWire.exports().memory.buffer, plan.pointer, 1024));
        if (!this.busy || plan?.replace) {
            const next = plan?.next;
            if (next) this.load(next);
        }
        return plan?.visible ? plan : null;
    }

    /** @brief Prefer the current flight's arrival cells without replacing or unpinning the displayed page. */
    prefetch(bounds: number[] | null, pixels = 0): void {
        this.destination = bounds ? {bounds: bounds.slice(), pixels} : null;
        if (!bounds) { this.ahead = null; this.pace = 1; }
    }

    /** @brief Hand the native scheduler its independently predicted coverage; no camera arithmetic lives here. */
    follow(view: number[] | null): void { this.ahead = view?.slice(3) || null; }

    get travelPace(): number { return this.pace; }

    private load(cell: GlobePhotoCell): void {
        this.busy = true; this.current = {...cell, id: ++this.serial};
        try {
            if (!this.worker) {
                const worker = this.worker = new Worker('/js/globe_photo_worker.js');
                worker.onmessage = event => {
                    if (this.closed || this.worker !== worker || !this.current) return;
                    const cell = this.current, pixels: unknown = event.data.pixels;
                    if (event.data.key !== cell.key || event.data.id !== cell.id) return;
                    try {
                        if (!(pixels instanceof ArrayBuffer) || pixels.byteLength !== 512 * 512 * 4)
                            throw new Error('Photographic cell unavailable');
                        const slot = this.cells.putSlot(cell.key, 1, pixels.byteLength, cell.bounds, cell.level, this.protect);
                        if (slot < 0) throw new Error('Photographic cells remain pinned');
                        const gl = this.gl;
                        gl.activeTexture(gl.TEXTURE2); gl.bindTexture(gl.TEXTURE_2D, this.atlas);
                        gl.texSubImage2D(gl.TEXTURE_2D, 0, slot % this.columns * 512,
                            Math.floor(slot / this.columns) * 512, 512, 512, gl.RGBA, gl.UNSIGNED_BYTE, new Uint8Array(pixels));
                        if (gl.getError() !== gl.NO_ERROR) throw new Error('Photographic cell upload failed');
                    } catch { this.cells.remove(cell.key); this.retry(cell.key); }
                    finally { this.busy = false; this.current = null; this.redraw(); }
                };
                worker.onerror = event => {
                    event.preventDefault(); worker.terminate(); this.worker = null;
                    this.retry(this.current?.key || '');
                    this.busy = false; this.current = null; this.redraw();
                };
            }
            this.worker.postMessage(this.current);
        } catch {
            this.retry(cell.key);
            this.busy = false; this.current = null;
        }
    }

    private retry(key: string): void {
        this.failed = key; this.retryAt = performance.now() + 10000;
        window.clearTimeout(this.retryTimer);
        this.retryTimer = window.setTimeout(this.redraw, 10000);
    }

    get atlasColumns(): number { return this.columns; }

    dispose(): void {
        this.closed = true;
        window.clearTimeout(this.retryTimer);
        this.worker?.terminate(); this.worker = null;
        this.cells.dispose();
        this.gl.deleteTexture(this.atlas); this.gl.deleteTexture(this.page);
    }
}
