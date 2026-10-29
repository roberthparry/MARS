/**
 * @file globe_surface_cache.ts
 * @brief Bounded IndexedDB storage of completed photographic river tiles.
 *
 * Stores disposable, uncompressed 2048-square premultiplied RGBA tiles ready for WebGL upload. One serial transaction evicts
 * the least recently used entries. Storage denial or quota failure is an optional-cache miss, not a drawing error.
 */
interface GlobeSurfaceRecord { key: string; blob: Blob; size: number; used: number; }
class GlobeSurfaceCache {
    static readonly maximumBytes = 512 * 1024 * 1024;
    static readonly maximumEntries = 32;
    static readonly tileBytes = 2048 * 2048 * 4;
    private database: Promise<IDBDatabase | null> | null = null;

    private open(): Promise<IDBDatabase | null> {
        return this.database ??= new Promise(resolve => {
            let unavailable = false;
            try {
                const request = indexedDB.open('mars.globe.surface.v1', 3);
                request.onupgradeneeded = () => {
                    if (request.result.objectStoreNames.contains('tiles')) request.result.deleteObjectStore('tiles');
                    request.result.createObjectStore('tiles', {keyPath: 'key'}).createIndex('used', 'used');
                };
                request.onerror = request.onblocked = () => { unavailable = true; resolve(null); };
                request.onsuccess = () => {
                    if (unavailable) { request.result.close(); return; }
                    request.result.onversionchange = () => request.result.close();
                    resolve(request.result);
                };
            } catch { resolve(null); }
        });
    }

    async get(key: string): Promise<Blob | null> {
        const db = await this.open();
        if (!db) return null;
        return new Promise(resolve => {
            try {
                const transaction = db.transaction('tiles', 'readwrite');
                const store = transaction.objectStore('tiles'), request = store.get(key);
                let blob: Blob | null = null;
                request.onsuccess = () => {
                    const record = request.result as GlobeSurfaceRecord | undefined;
                    if (!record) return;
                    if (!(record.blob instanceof Blob) || record.blob.type !== 'application/octet-stream' ||
                        record.size !== GlobeSurfaceCache.tileBytes || record.blob.size !== record.size) {
                        store.delete(key);
                        return;
                    }
                    blob = record.blob;
                    store.put({...record, used: Date.now()});
                };
                transaction.oncomplete = () => resolve(blob);
                transaction.onabort = transaction.onerror = () => resolve(null);
            } catch { resolve(null); }
        });
    }

    async put(key: string, blob: Blob): Promise<void> {
        if (blob.size !== GlobeSurfaceCache.tileBytes || blob.type !== 'application/octet-stream') return;
        const db = await this.open();
        if (!db) return;
        await new Promise<void>(resolve => {
            try {
                const transaction = db.transaction('tiles', 'readwrite'), store = transaction.objectStore('tiles');
                store.put({key, blob, size: blob.size, used: Date.now()} satisfies GlobeSurfaceRecord);
                // At most 33 metadata records are examined. Pixel payloads remain in browser storage.
                const entries: {key: IDBValidKey; size: number}[] = [];
                let bytes = 0;
                const cursor = store.index('used').openCursor();
                cursor.onsuccess = () => {
                    const current = cursor.result;
                    if (current) {
                        const record = current.value as GlobeSurfaceRecord;
                        if (!(record.blob instanceof Blob) || record.size !== record.blob.size ||
                            record.blob.type !== 'application/octet-stream' || record.size !== GlobeSurfaceCache.tileBytes) current.delete();
                        else {
                            entries.push({key: current.primaryKey, size: record.size});
                            bytes += record.size;
                        }
                        current.continue();
                    } else {
                        let count = entries.length;
                        for (const entry of entries) {
                            if (bytes <= GlobeSurfaceCache.maximumBytes && count <= GlobeSurfaceCache.maximumEntries) break;
                            store.delete(entry.key);
                            bytes -= entry.size;
                            --count;
                        }
                    }
                };
                transaction.oncomplete = transaction.onabort = transaction.onerror = () => resolve();
            } catch { resolve(); }
        });
    }
}
