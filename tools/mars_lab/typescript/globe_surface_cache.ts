/**
 * @file globe_surface_cache.ts
 * @brief Bounded asynchronous storage of reusable photographic cells and river masks.
 *
 * Raw RGBA payloads share a 512 MiB budget. Separate small metadata records maintain recency and byte totals,
 * so eviction never reads image blobs. Read-only hits publish before their asynchronous recency update.
 * Optional cache opens and transactions have deadlines; stalled storage becomes a cache miss so imagery can load.
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
    private disabled = false;

    constructor(readonly width = 2048, readonly height = width,
                private readonly databaseName = 'mars.globe.surface.v1', private readonly timeoutMs = 2000) {
        if (![512, 514, 2048].includes(width) || height !== width) throw new Error('Invalid raw cache dimensions');
        this.tileBytes = width * height * 4;
    }

    private open(): Promise<IDBDatabase | null> {
        if (this.disabled) return Promise.resolve(null);
        return this.database ??= new Promise(resolve => {
            let unavailable = false, finished = false;
            let request: IDBOpenDBRequest | null = null;
            const finish = (database: IDBDatabase | null): void => {
                if (finished) return;
                finished = true;
                clearTimeout(timer);
                resolve(database);
            };
            const fail = (): void => {
                if (finished) return;
                unavailable = true;
                finish(null);
                try { request?.transaction?.abort(); } catch { /* An upgrade may already have finished. */ }
            };
            const timer = globalThis.setTimeout(fail, this.timeoutMs);
            try {
                const opening = request = indexedDB.open(this.databaseName, 4);
                opening.onupgradeneeded = () => {
                    if (unavailable) { opening.transaction?.abort(); return; }
                    for (const name of ['tiles', 'metadata', 'state'])
                        if (opening.result.objectStoreNames.contains(name)) opening.result.deleteObjectStore(name);
                    opening.result.createObjectStore('tiles', {keyPath: 'key'}).createIndex('used', 'used');
                    opening.result.createObjectStore('metadata', {keyPath: 'key'}).createIndex('used', 'used');
                    opening.result.createObjectStore('state', {keyPath: 'key'});
                };
                opening.onerror = opening.onblocked = fail;
                opening.onsuccess = () => {
                    if (unavailable) { opening.result.close(); return; }
                    opening.result.onversionchange = () => {
                        opening.result.close();
                        if (!this.disabled) this.database = null;
                    };
                    finish(opening.result);
                };
            } catch { fail(); }
        });
    }

    /** @brief Stop a stalled transaction and bypass this cache for the rest of its owner's lifetime. */
    private deadline(db: IDBDatabase, transaction: IDBTransaction, finish: () => void): () => void {
        const timer = globalThis.setTimeout(() => {
            this.disabled = true;
            this.database = Promise.resolve(null);
            db.close();
            try { transaction.abort(); } catch { /* The transaction may already be inactive. */ }
            finish();
        }, this.timeoutMs);
        return () => clearTimeout(timer);
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
                    finished = true; stopDeadline(); signal?.removeEventListener('abort', cancel); resolve(keys);
                };
                const cancel = () => {
                    try { transaction.abort(); } catch { /* The indexed read may already have completed. */ }
                    finish([]);
                };
                const stopDeadline = this.deadline(db, transaction, () => finish([]));
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
                    stopDeadline();
                    signal?.removeEventListener('abort', cancel);
                    resolve(value);
                };
                const cancel = () => {
                    try { transaction.abort(); } catch { /* Completion may already be queued. */ }
                    finish(null);
                };
                const stopDeadline = this.deadline(db, transaction, () => finish(null));
                request.onsuccess = () => {
                    if (finished) return;
                    const record = request.result as GlobeSurfaceRecord | undefined;
                    if (!record) return;
                    damaged = !(record.blob instanceof Blob) || record.blob.type !== 'application/octet-stream' ||
                        record.size !== this.tileBytes || record.blob.size !== record.size;
                    if (!damaged) blob = record.blob;
                };
                transaction.oncomplete = () => {
                    if (finished) return;
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
        let stopDeadline = (): void => {};
        try {
            const transaction = db.transaction(remove ? ['tiles', 'metadata', 'state'] : ['metadata'], 'readwrite');
            let finished = false;
            const finish = (): void => {
                if (finished) return;
                finished = true;
                stopDeadline();
            };
            stopDeadline = this.deadline(db, transaction, finish);
            transaction.oncomplete = transaction.onabort = transaction.onerror = finish;
            const metadata = transaction.objectStore('metadata'), request = metadata.get(key);
            request.onsuccess = () => {
                if (finished) return;
                const record = request.result as GlobeSurfaceMetadata | undefined;
                if (!record) { if (remove) transaction.objectStore('tiles').delete(key); return; }
                if (!remove) { metadata.put({...record, used: Date.now()}); return; }
                metadata.delete(key); transaction.objectStore('tiles').delete(key);
                const state = transaction.objectStore('state'), budget = state.get('budget');
                budget.onsuccess = () => {
                    if (finished) return;
                    const value = budget.result as GlobeSurfaceBudget | undefined;
                    if (value) state.put({...value, bytes: Math.max(0, value.bytes - record.size),
                        count: Math.max(0, value.count - 1)});
                };
            };
        } catch { stopDeadline(); /* Cache recency never holds up drawing. */ }
    }

    async put(key: string, blob: Blob): Promise<void> {
        if (blob.size !== this.tileBytes || blob.type !== 'application/octet-stream') return;
        const db = await this.open();
        if (!db) return;
        await new Promise<void>(resolve => {
            let finished = false, stopDeadline = (): void => {};
            const finish = (): void => {
                if (finished) return;
                finished = true;
                stopDeadline();
                resolve();
            };
            try {
                const transaction = db.transaction(['tiles', 'metadata', 'state'], 'readwrite');
                stopDeadline = this.deadline(db, transaction, finish);
                const tiles = transaction.objectStore('tiles'), metadata = transaction.objectStore('metadata');
                const state = transaction.objectStore('state'), old = metadata.get(key), budget = state.get('budget');
                let oldReady = false, budgetReady = false;
                const insert = () => {
                    if (finished || !oldReady || !budgetReady) return;
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
                        if (finished) return;
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
                transaction.oncomplete = transaction.onabort = transaction.onerror = finish;
            } catch { finish(); }
        });
    }
}
