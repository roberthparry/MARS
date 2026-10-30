/**
 * @file globe_surface_tiles.ts
 * @brief WebGL resource adapter for native geographic window selection and texture reuse.
 *
 * WASM selects stable bounds, reusable slots and evictions through dictionary_t. JavaScript uploads and binds
 * the corresponding browser textures. The outgoing and incoming geographic layers stay pinned during handoff.
 * Four resident tiles use at most 64 MiB; fallback samplers are never owned here.
 */
import type {GlobeSurfaceTile} from './globe_surface.js';
import {GlobeTextureCache} from './globe_texture_cache.js';

interface SurfaceTexture { texture: WebGLTexture; complete: boolean; }
interface ResidentSurface { resource: SurfaceTexture; slot: number; bounds: number[]; zoom: number; }

export class GlobeSurfaceTiles {
    private readonly entries: GlobeTextureCache<SurfaceTexture>;
    private displayed: ResidentSurface | null = null;
    private previous: ResidentSurface | null = null;
    private changed = 0;
    private revision = 0;
    constructor(private readonly gl: WebGLRenderingContext) {
        this.entries = new GlobeTextureCache(4, 64 * 1024 * 1024, entry => gl.deleteTexture(entry.texture));
    }
    static bounds(bounds: number[]): number[] {
        return labDOM.call('lab_globe_surface_bounds', bounds) as number[] || bounds;
    }
    covering(bounds: number[], zoom: number): {texture: WebGLTexture; complete: boolean; bounds: number[]; zoom: number} | null {
        const entry = this.entries.covering(bounds, zoom);
        return entry ? {...entry.resource, bounds: entry.bounds, zoom: entry.zoom} : null;
    }
    bind(bounds: number[], zoom: number, now = performance.now()): {
        bounds: number[]; zoom: number; complete: boolean; blend: number;
        previous: {bounds: number[]; zoom: number; complete: boolean} | null;
    } | null {
        const preferred = this.displayed?.slot ?? -1;
        const entry = this.entries.covering(bounds, zoom, false, preferred) || this.entries.covering(bounds, zoom, true);
        if (entry && entry.slot !== this.displayed?.slot) {
            this.previous = this.displayed;
            this.displayed = entry;
            this.changed = now;
        }
        const current = this.displayed;
        if (!current || !this.gl.isTexture(current.resource.texture)) return null;
        let previous = this.previous;
        if (previous && !this.gl.isTexture(previous.resource.texture)) previous = this.previous = null;
        const blend = Math.min(1, Math.max(0, (now - this.changed) / 500));
        if (previous && blend === 1 && previous.bounds.every((value, i) => value === current.bounds[i]))
            previous = this.previous = null;
        this.gl.activeTexture(this.gl.TEXTURE3);
        this.gl.bindTexture(this.gl.TEXTURE_2D, current.resource.texture);
        if (previous) {
            // The always-photographic renderer reuses the dormant coarse-map unit; eight texture units still suffice.
            this.gl.activeTexture(this.gl.TEXTURE2);
            this.gl.bindTexture(this.gl.TEXTURE_2D, previous.resource.texture);
        }
        return {bounds: current.bounds, zoom: current.zoom, complete: current.resource.complete, blend,
            previous: previous ? {bounds: previous.bounds, zoom: previous.zoom, complete: previous.resource.complete} : null};
    }
    put(bounds: number[], zoom: number, tile: GlobeSurfaceTile, complete = true): void {
        const gl = this.gl, key = `${bounds.join(',')}:${zoom}:${++this.revision}`;
        if (gl.isContextLost()) return;
        const texture = gl.createTexture();
        if (!texture) throw new Error('Surface texture unavailable');
        const active = gl.getParameter(gl.ACTIVE_TEXTURE) as number;
        gl.activeTexture(gl.TEXTURE3);
        const previous = gl.getParameter(gl.TEXTURE_BINDING_2D) as WebGLTexture | null;
        try {
            gl.bindTexture(gl.TEXTURE_2D, texture);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
            if ('pixels' in tile)
                gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, tile.width, tile.height, 0, gl.RGBA, gl.UNSIGNED_BYTE, tile.pixels);
            else gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, tile);
            if (gl.getError() !== gl.NO_ERROR ||
                !this.entries.put(key, {texture, complete}, 2048 * 2048 * 4, bounds, zoom,
                    [this.displayed?.slot ?? -1, this.previous?.slot ?? -1], complete ? 2 : 1))
                throw new Error('Surface texture upload failed');
        } catch (error) { gl.deleteTexture(texture); throw error; }
        finally {
            gl.bindTexture(gl.TEXTURE_2D, previous && gl.isTexture(previous) ? previous :
                gl.isTexture(texture) ? texture : null);
            gl.activeTexture(active);
        }
    }
    dispose(): void { this.entries.dispose(); this.displayed = this.previous = null; }
}
