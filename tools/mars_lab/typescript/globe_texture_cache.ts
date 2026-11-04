/**
 * @file globe_texture_cache.ts
 * @brief Browser resource slots controlled by the native dictionary-backed texture cache.
 *
 * Only opaque browser objects live here. WASM owns key lookup, recency, byte accounting and eviction.
 * Slots are direct indexed; no JavaScript Map duplicates the native dictionary.
 */
interface TextureSlot { token: number; slot: number; count: number; bytes: number; evicted?: number[]; bounds?: number[]; zoom?: number; }
export interface GlobePhotoCell { key: string; bounds: number[]; level: number; coverage?: boolean; }
export interface GlobePhotoPage {
    bounds: number[]; span: number; pointer: number; revision: number; protect: number[]; next?: GlobePhotoCell;
    visible: boolean; pace: number; coverage: number; replace: boolean;
}
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
        return this.getSlot(key)?.resource ?? null;
    }
    /** @brief Borrow an indexed resource and its native slot so an active drawing operation can pin it. */
    getSlot(key: string): {resource: T; slot: number} | null {
        const result = this.call({action: 1, key});
        const resource = result ? this.resources[result.slot] : null;
        return result && resource !== null ? {resource, slot: result.slot} : null;
    }
    get size(): number { return this.call({action: 5})?.count ?? 0; }
    /** @brief Let native source planning reuse adequate resident or queued pixels across resolution changes. */
    source<U>(photo: unknown, buffered?: GlobeTextureCache<U>, stored?: string[]): unknown {
        return this.token ? labDOM.call('lab_texture_cache',
            {action: 9, token: this.token, photo, buffered: buffered?.token, stored}) : null;
    }
    remove(key: string): void {
        const result = this.call({action: 7, key});
        for (const slot of result?.evicted || []) {
            const resource = this.resources[slot];
            if (resource !== null) this.release(resource);
            this.resources[slot] = null;
        }
    }
    put(key: string, resource: T, bytes: number, bounds?: number[], zoom = 0,
        protect: number | number[] = -1, quality = 0): boolean {
        return this.putSlot(key, resource, bytes, bounds, zoom, protect, quality) >= 0;
    }
    putSlot(key: string, resource: T, bytes: number, bounds?: number[], zoom = 0,
            protect: number | number[] = -1, quality = 0): number {
        const result = this.call({action: 2, key, bytes, bounds, zoom, protect, quality});
        if (!result) return -1;
        for (const slot of result.evicted || []) {
            const previous = this.resources[slot];
            if (previous !== null) this.release(previous);
            this.resources[slot] = null;
        }
        if (result.slot < 0) return -1;
        const previous = this.resources[result.slot];
        if (previous !== null && previous !== resource) this.release(previous);
        this.resources[result.slot] = resource;
        return result.slot;
    }
    page(bounds: number[], pixels: number, avoid = '', protect: number[] = [],
         current: GlobePhotoCell | null = null, ahead: number[] | null = null): GlobePhotoPage | null {
        return this.token ? labDOM.call('lab_texture_cache',
            {action: 6, token: this.token, bounds, pixels, avoid, protect, current, ahead}) as GlobePhotoPage | null : null;
    }
    journey(bounds: number[], pixels: number, arrival: number[], arrivalPixels: number, ahead: number[] | null,
            avoid: string, idle: boolean, current: GlobePhotoCell | null = null): GlobePhotoPage | null {
        return this.token ? labDOM.call('lab_texture_cache', {action: 8, token: this.token, bounds, pixels,
            arrival, arrival_pixels: arrivalPixels, ahead, avoid, idle, current}) as GlobePhotoPage | null : null;
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
