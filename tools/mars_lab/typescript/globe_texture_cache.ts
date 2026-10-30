/**
 * @file globe_texture_cache.ts
 * @brief Browser resource slots controlled by the native dictionary-backed texture cache.
 *
 * Only opaque browser objects live here. WASM owns key lookup, recency, byte accounting and eviction.
 * Slots are direct indexed; no JavaScript Map duplicates the native dictionary.
 */
interface TextureSlot { token: number; slot: number; count: number; bytes: number; evicted?: number[]; bounds?: number[]; zoom?: number; }
export class GlobeTextureCache<T> {
    private token: number;
    private readonly resources: (T | null)[];
    constructor(limit: number, bytes: number, private readonly release: (resource: T) => void) {
        const result = labDOM.call('lab_texture_cache', {action: 0, limit, bytes}) as TextureSlot | null;
        if (!result) throw new Error('Native texture cache unavailable');
        this.token = result.token;
        this.resources = Array.from({length: limit}, () => null);
    }
    get(key: string): T | null {
        const result = this.call({action: 1, key});
        return result ? this.resources[result.slot] : null;
    }
    get size(): number { return this.call({action: 5})?.count ?? 0; }
    put(key: string, resource: T, bytes: number, bounds?: number[], zoom = 0,
        protect: number | number[] = -1, quality = 0): boolean {
        const result = this.call({action: 2, key, bytes, bounds, zoom, protect, quality});
        if (!result) return false;
        for (const slot of result.evicted || []) {
            const previous = this.resources[slot];
            if (previous !== null) this.release(previous);
            this.resources[slot] = null;
        }
        if (result.slot < 0) return false;
        const previous = this.resources[result.slot];
        if (previous !== null && previous !== resource) this.release(previous);
        this.resources[result.slot] = resource;
        return true;
    }
    covering(bounds: number[], zoom: number, partial = false, prefer = -1): {
        resource: T; slot: number; bounds: number[]; zoom: number;
    } | null {
        const result = this.call({action: 3, bounds, zoom, partial, prefer});
        const resource = result ? this.resources[result.slot] : null;
        return result && resource !== null && result.bounds && result.zoom !== undefined
            ? {resource, slot: result.slot, bounds: result.bounds, zoom: result.zoom} : null;
    }
    dispose(): void {
        this.call({action: 4}); this.token = 0;
        for (let slot = 0; slot < this.resources.length; ++slot) {
            const resource = this.resources[slot];
            if (resource !== null) this.release(resource);
            this.resources[slot] = null;
        }
    }
    private call(request: object): TextureSlot | null {
        return this.token ? labDOM.call('lab_texture_cache', {...request, token: this.token}) as TextureSlot | null : null;
    }
}
