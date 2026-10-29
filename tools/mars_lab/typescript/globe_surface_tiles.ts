/**
 * @file globe_surface_tiles.ts
 * @brief WebGL resource adapter for native geographic window selection and texture reuse.
 *
 * WASM selects stable bounds, reusable slots and evictions through dictionary_t. JavaScript uploads and binds
 * the corresponding browser textures. Four resident tiles use at most 64 MiB; fallback samplers are never owned here.
 */
import type {GlobeSurfaceTile} from './globe_surface.js';
import {GlobeTextureCache} from './globe_texture_cache.js';

interface SurfaceTexture { texture: WebGLTexture; complete: boolean; }

export class GlobeSurfaceTiles {
    private readonly entries: GlobeTextureCache<SurfaceTexture>;
    private displayed = -1;
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
    bind(bounds: number[], zoom: number): {bounds: number[]; zoom: number; complete: boolean} | null {
        const entry = this.entries.covering(bounds, zoom);
        if (!entry) return null;
        this.gl.activeTexture(this.gl.TEXTURE3);
        this.gl.bindTexture(this.gl.TEXTURE_2D, entry.resource.texture);
        this.displayed = entry.slot;
        return {bounds: entry.bounds, zoom: entry.zoom, complete: entry.resource.complete};
    }
    put(bounds: number[], zoom: number, tile: GlobeSurfaceTile, complete = true): void {
        const gl = this.gl, key = bounds.join(',');
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
                !this.entries.put(key, {texture, complete}, 2048 * 2048 * 4, bounds, zoom, this.displayed))
                throw new Error('Surface texture upload failed');
        } catch (error) { gl.deleteTexture(texture); throw error; }
        finally {
            gl.bindTexture(gl.TEXTURE_2D, previous && gl.isTexture(previous) ? previous :
                gl.isTexture(texture) ? texture : null);
            gl.activeTexture(active);
        }
    }
    dispose(): void { this.entries.dispose(); this.displayed = -1; }
}
