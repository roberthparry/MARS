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
    private current: GlobePhotoCell | null = null;

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
                [page, gl.TEXTURE7, 8, 8, gl.NEAREST]
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
        const plan = this.cells.page(bounds, Math.min(pixels, (this.columns - 2) * 256),
            performance.now() < this.retryAt ? this.failed : '');
        if (!plan) return null;
        this.protect = plan.protect;
        const gl = this.gl;
        gl.activeTexture(gl.TEXTURE2); gl.bindTexture(gl.TEXTURE_2D, this.atlas);
        gl.activeTexture(gl.TEXTURE7); gl.bindTexture(gl.TEXTURE_2D, this.page);
        gl.texSubImage2D(gl.TEXTURE_2D, 0, 0, 0, 8, 8, gl.RGBA, gl.UNSIGNED_BYTE,
            new Uint8Array(labWire.exports().memory.buffer, plan.pointer, 256));
        if (!this.busy && plan.next && performance.now() >= this.retryAt) this.load(plan.next);
        return plan;
    }

    private load(cell: GlobePhotoCell): void {
        this.busy = true; this.current = cell;
        try {
            if (!this.worker) {
                const worker = this.worker = new Worker('/js/globe_photo_worker.js');
                worker.onmessage = event => {
                    if (this.closed || this.worker !== worker || !this.current) return;
                    const cell = this.current, pixels: unknown = event.data.pixels;
                    try {
                        if (event.data.key !== cell.key || !(pixels instanceof ArrayBuffer) || pixels.byteLength !== 512 * 512 * 4)
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
            this.worker.postMessage(cell);
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
