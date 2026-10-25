/**
 * @file globe_worker.ts
 * @brief Isolated WASM shoreline admission, projection and travel-border preparation.
 *
 * Reuses the native host bridge without accessing the DOM. Fetch delivers opaque chunks directly into
 * native HTTP receiver storage; requests, Protobuf decoding and geometry admission remain native.
 * Message identifiers allow projections of retained geometry while an asynchronous download is pending.
 * World-border meshes use separate native storage and transfer packed vertices without main-thread parsing.
 */
interface GlobeWorkerRequest {
    id: number;
    entry: 'fetch' | 'cancel' | 'lab_globe_shore_svg' | 'lab_globe_shore_frame' | 'lab_globe_borders_mesh';
    args: (number | number[] | string)[];
    geometry?: number;
}

/** @brief Own the dedicated worker's isolated native instance and message lifetime. */
class GlobeProjectionWorker {
    private readonly ready: Promise<void>;
    private active: {id: number; token: number; controller: AbortController} | null = null;
    private geometry = 0;

    constructor(private readonly port: {
        importScripts(url: string): void;
        postMessage(message: unknown, transfer?: Transferable[]): void;
        onmessage: ((event: MessageEvent<GlobeWorkerRequest>) => void) | null;
    }) {
        port.importScripts('/js/transport.js');
        this.ready = labWire.start(false);
        port.onmessage = event => { void this.receive(event.data); };
    }

    private async receive(request: GlobeWorkerRequest): Promise<void> {
        try {
            await this.ready;
            if (!request || !Number.isSafeInteger(request.id) || request.id < 1 || !Array.isArray(request.args) ||
                !['fetch', 'cancel', 'lab_globe_shore_svg', 'lab_globe_shore_frame', 'lab_globe_borders_mesh'].includes(request.entry))
                throw new Error('Invalid shoreline worker operation');
            if (request.entry === 'cancel') {
                if (this.active?.id === request.id) this.cancel();
                return;
            }
            const started = performance.now();
            if (request.entry === 'lab_globe_borders_mesh') {
                const count = labDOM.call('lab_globe_borders_mesh', ...request.args) as number;
                if (!count) throw new Error('Travel borders unavailable');
                const native = labWire.exports();
                const vertices = new Float32Array(native.memory.buffer, native.lab_globe_borders_vertices(), count * 3).slice();
                labDOM.call('lab_globe_borders_clear');
                this.port.postMessage({id: request.id, value: vertices}, [vertices.buffer]);
                return;
            }
            const value = request.entry === 'fetch' ? await this.fetchRegion(request) :
                request.geometry === this.geometry ? labDOM.call(request.entry, ...request.args) : null;
            if (request.entry === 'lab_globe_shore_frame' && typeof value === 'string') {
                const native = labWire.exports();
                const vertices = new Float32Array(native.memory.buffer, native.lab_globe_mask_vertices(),
                    native.lab_globe_mask_vertex_count() * 2).slice();
                const batches = new Uint32Array(native.memory.buffer, native.lab_globe_mask_batches(),
                    native.lab_globe_mask_batch_count() * 3).slice();
                this.port.postMessage({id: request.id, value: {svg: value, vertices, batches, projectMs: performance.now() - started}},
                    [vertices.buffer, batches.buffer]);
                return;
            }
            this.port.postMessage({id: request.id, value});
        } catch (error) { this.port.postMessage({id: request?.id, error: String(error)}); }
    }

    private cancel(): void {
        if (!this.active) return;
        this.active.controller.abort();
        labWire.exports().lab_globe_fetch_cancel(this.active.token);
        this.active = null;
    }

    private async fetchRegion(request: GlobeWorkerRequest): Promise<number> {
        if (request.args.length !== 4 || !request.args.every(value => typeof value === 'number' && Number.isFinite(value)))
            throw new Error('Invalid shoreline bounds');
        this.cancel();
        const native = labWire.exports();
        const token = native.lab_globe_fetch_begin(...request.args as number[]);
        if (!token) throw new Error('Could not start native shoreline request');
        const controller = new AbortController();
        const active = this.active = {id: request.id, token, controller};
        const timeout = setTimeout(() => controller.abort(), 29000);
        let reader: ReadableStreamDefaultReader<Uint8Array<ArrayBuffer>> | undefined;
        try {
            // Fetch snapshots the small request body. No view into growable WASM memory crosses await.
            const body = new Uint8Array(native.memory.buffer, native.lab_globe_fetch_request(token),
                native.lab_globe_fetch_request_size(token)).slice();
            const response = await fetch('/almanac-globe', {
                method: 'POST', body, signal: controller.signal, credentials: 'same-origin', redirect: 'error',
                headers: {'Content-Type': 'application/x-protobuf', 'Accept': 'application/x-protobuf'}
            });
            controller.signal.throwIfAborted();
            if (!response.ok || response.headers.get('Content-Type')?.split(';')[0].trim().toLowerCase() !==
                'application/x-protobuf' || !response.body) throw new Error('Shoreline HTTP response unavailable');
            reader = response.body.getReader();
            for (;;) {
                const {value, done} = await reader.read();
                controller.signal.throwIfAborted();
                if (done) break;
                if (!value.byteLength) continue;
                const pointer = native.lab_globe_fetch_reserve(token, value.byteLength);
                if (!pointer) throw new Error('Native shoreline response budget exceeded');
                // Reacquire after reserve: allocation or intervening projection can grow native memory.
                new Uint8Array(native.memory.buffer, pointer, value.byteLength).set(value);
                if (!native.lab_globe_fetch_commit(token, value.byteLength)) throw new Error('Shoreline chunk rejected');
            }
            if (!native.lab_globe_fetch_finish(token, response.status)) throw new Error('Native shoreline response rejected');
            this.geometry = request.id;
            return this.geometry;
        } finally {
            clearTimeout(timeout);
            controller.abort();
            await reader?.cancel().catch(() => {});
            reader?.releaseLock();
            native.lab_globe_fetch_cancel(token);
            if (this.active === active) this.active = null;
        }
    }
}

new GlobeProjectionWorker(globalThis as unknown as ConstructorParameters<typeof GlobeProjectionWorker>[0]);
