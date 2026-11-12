/**
 * @file globe_surface_cache.ts
 * @brief Bounded asynchronous storage of reusable photographic cells and river masks.
 *
 * Raw RGBA payloads share a 512 MiB budget. Separate small metadata records maintain recency and byte totals,
 * so eviction never reads image blobs. Read-only hits publish before their asynchronous recency update.
 */
interface GlobeSurfaceRecord { key: string; blob: Blob; size: number; used: number; }
interface GlobeSurfaceMetadata { key: string; size: number; used: number; }
interface GlobeSurfaceBudget { key: string; bytes: number; count: number; }
class GlobeSurfaceCache {
    static readonly maximumBytes = 512 * 1024 * 1024;
    static readonly maximumEntries = 512;
    static readonly tileBytes = 2048 * 2048 * 4;
    readonly tileBytes: number;
    private database: Promise<IDBDatabase | null> | null = null;

    constructor(readonly width = 2048, readonly height = width,
                private readonly databaseName = 'mars.globe.surface.v1') {
        if (![512, 514, 2048].includes(width) || height !== width) throw new Error('Invalid raw cache dimensions');
        this.tileBytes = width * height * 4;
    }

    private open(): Promise<IDBDatabase | null> {
        return this.database ??= new Promise(resolve => {
            let unavailable = false;
            try {
                const request = indexedDB.open(this.databaseName, 4);
                request.onupgradeneeded = () => {
                    for (const name of ['tiles', 'metadata', 'state'])
                        if (request.result.objectStoreNames.contains(name)) request.result.deleteObjectStore(name);
                    request.result.createObjectStore('tiles', {keyPath: 'key'}).createIndex('used', 'used');
                    request.result.createObjectStore('metadata', {keyPath: 'key'}).createIndex('used', 'used');
                    request.result.createObjectStore('state', {keyPath: 'key'});
                };
                request.onerror = request.onblocked = () => { unavailable = true; resolve(null); };
                request.onsuccess = () => {
                    if (unavailable) { request.result.close(); return; }
                    request.result.onversionchange = () => { request.result.close(); this.database = null; };
                    resolve(request.result);
                };
            } catch { resolve(null); }
        });
    }

    /** @brief Report optional raw-storage availability before preparing speculative cache pixels. */
    async available(): Promise<boolean> { return Boolean(await this.open()); }

    /** @brief Read bounded content addresses through the metadata key index without loading image Blobs. */
    async keys(prefix: string, signal?: AbortSignal): Promise<string[]> {
        if (signal?.aborted) return [];
        const db = await this.open();
        if (!db || signal?.aborted) return [];
        return new Promise(resolve => {
            try {
                const transaction = db.transaction('metadata', 'readonly');
                const request = transaction.objectStore('metadata').getAllKeys(
                    IDBKeyRange.bound(prefix, prefix + '\uffff'), GlobeSurfaceCache.maximumEntries);
                let finished = false;
                const finish = (keys: string[]) => {
                    if (finished) return;
                    finished = true; signal?.removeEventListener('abort', cancel); resolve(keys);
                };
                const cancel = () => {
                    try { transaction.abort(); } catch { /* The indexed read may already have completed. */ }
                    finish([]);
                };
                transaction.oncomplete = () => finish(signal?.aborted ? [] : request.result as string[]);
                transaction.onabort = transaction.onerror = () => finish([]);
                signal?.addEventListener('abort', cancel, {once: true});
                if (signal?.aborted) cancel();
            } catch { resolve([]); }
        });
    }

    async get(key: string, signal?: AbortSignal): Promise<Blob | null> {
        if (signal?.aborted) return null;
        const db = await this.open();
        if (!db || signal?.aborted) return null;
        return new Promise(resolve => {
            try {
                const transaction = db.transaction('tiles', 'readonly');
                const request = transaction.objectStore('tiles').get(key);
                let blob: Blob | null = null, damaged = false;
                let finished = false;
                const finish = (value: Blob | null) => {
                    if (finished) return;
                    finished = true;
                    signal?.removeEventListener('abort', cancel);
                    resolve(value);
                };
                const cancel = () => {
                    try { transaction.abort(); } catch { /* Completion may already be queued. */ }
                    finish(null);
                };
                request.onsuccess = () => {
                    const record = request.result as GlobeSurfaceRecord | undefined;
                    if (!record) return;
                    damaged = !(record.blob instanceof Blob) || record.blob.type !== 'application/octet-stream' ||
                        record.size !== this.tileBytes || record.blob.size !== record.size;
                    if (!damaged) blob = record.blob;
                };
                transaction.oncomplete = () => {
                    if (signal?.aborted) { finish(null); return; }
                    if (blob || damaged) this.touch(db, key, damaged);
                    finish(blob);
                };
                transaction.onabort = transaction.onerror = () => finish(null);
                signal?.addEventListener('abort', cancel, {once: true});
                if (signal?.aborted) cancel();
            } catch { resolve(null); }
        });
    }

    private touch(db: IDBDatabase, key: string, remove: boolean): void {
        try {
            const transaction = db.transaction(remove ? ['tiles', 'metadata', 'state'] : ['metadata'], 'readwrite');
            const metadata = transaction.objectStore('metadata'), request = metadata.get(key);
            request.onsuccess = () => {
                const record = request.result as GlobeSurfaceMetadata | undefined;
                if (!record) { if (remove) transaction.objectStore('tiles').delete(key); return; }
                if (!remove) { metadata.put({...record, used: Date.now()}); return; }
                metadata.delete(key); transaction.objectStore('tiles').delete(key);
                const state = transaction.objectStore('state'), budget = state.get('budget');
                budget.onsuccess = () => {
                    const value = budget.result as GlobeSurfaceBudget | undefined;
                    if (value) state.put({...value, bytes: Math.max(0, value.bytes - record.size),
                        count: Math.max(0, value.count - 1)});
                };
            };
        } catch { /* Cache recency never holds up drawing. */ }
    }

    async put(key: string, blob: Blob): Promise<void> {
        if (blob.size !== this.tileBytes || blob.type !== 'application/octet-stream') return;
        const db = await this.open();
        if (!db) return;
        await new Promise<void>(resolve => {
            try {
                const transaction = db.transaction(['tiles', 'metadata', 'state'], 'readwrite');
                const tiles = transaction.objectStore('tiles'), metadata = transaction.objectStore('metadata');
                const state = transaction.objectStore('state'), old = metadata.get(key), budget = state.get('budget');
                let oldReady = false, budgetReady = false;
                const insert = () => {
                    if (!oldReady || !budgetReady) return;
                    const previous = old.result as GlobeSurfaceMetadata | undefined;
                    const value = (budget.result as GlobeSurfaceBudget | undefined) || {key: 'budget', bytes: 0, count: 0};
                    value.bytes += blob.size - (previous?.size || 0);
                    value.count += previous ? 0 : 1;
                    const used = Date.now();
                    tiles.put({key, blob, size: blob.size, used} satisfies GlobeSurfaceRecord);
                    metadata.put({key, size: blob.size, used} satisfies GlobeSurfaceMetadata);
                    if (value.bytes <= GlobeSurfaceCache.maximumBytes && value.count <= GlobeSurfaceCache.maximumEntries) {
                        state.put(value); return;
                    }
                    const cursor = metadata.index('used').openCursor();
                    cursor.onsuccess = () => {
                        const current = cursor.result;
                        if (!current || (value.bytes <= GlobeSurfaceCache.maximumBytes &&
                            value.count <= GlobeSurfaceCache.maximumEntries)) { state.put(value); return; }
                        if (current.primaryKey !== key) {
                            const expired = current.value as GlobeSurfaceMetadata;
                            tiles.delete(expired.key); current.delete();
                            value.bytes -= expired.size; --value.count;
                        }
                        current.continue();
                    };
                };
                old.onsuccess = () => { oldReady = true; insert(); };
                budget.onsuccess = () => { budgetReady = true; insert(); };
                transaction.oncomplete = transaction.onabort = transaction.onerror = () => resolve();
            } catch { resolve(); }
        });
    }
}
