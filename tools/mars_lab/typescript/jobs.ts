/**
 * @file jobs.ts
 * @brief Browser resources for the native asynchronous job queue.
 *
 * Owns the dedicated worker, pending promises and cancellation lifetime. Native C
 * selects queue transitions; browser code applies their effects without blocking the UI.
 */
/** Browser resources for the C-owned queue and worker lifecycle; no UI-thread waits. */
import {is_worker_response} from './codec_types.js';
import type {CodecBuffer, JobsExports, JsonValue, WorkerRequest, WorkerResponse} from './codec_types.js';
export type {JobsExports} from './codec_types.js';

// Numeric event/effect identifiers mirror lab_jobs.h and are covered by the module ABI.
export const worker_events = Object.freeze({pump: 0, ready: 1, result: 2, error: 3, failure: 4, cancel: 5,
    close: 6, unknown: 7} as const);
export const worker_actions = Object.freeze({wait: 0, start: 1, send: 2, finish: 3, fail: 4, invalid: 5, stop: 6} as const);
type WorkerEvent = typeof worker_events[keyof typeof worker_events];

export interface JobsCodec {
    /** @brief Borrow the exact bounded, non-shared input range without copying or transferring ownership. */
    bytes(buffer: CodecBuffer): Uint8Array<ArrayBuffer>;

    /** @brief Decode synchronously on the calling thread; malformed messages must be rejected. */
    decode(buffer: CodecBuffer): JsonValue;
}

export interface JobsStatus {
    count: number;
    bytes: number;
    active: number;
    started: number;
    completed: number;
}

interface PendingJob {
    bytes: Uint8Array<ArrayBuffer> | null;
    resolve(value: JsonValue): void;
    reject(error: unknown): void;
    signal: AbortSignal | undefined;
    abort(): void;
    timer: ReturnType<typeof setTimeout> | undefined;
}

/** Owns the worker, transferred buffers, abort subscriptions and deadlines for native queue tokens. */
export class Jobs {
    private readonly jobs = new Map<number, PendingJob>();
    private worker: Worker | null = null;
    private active = 0;
    private phase = 0;
    private started = 0;
    private completed = 0;

    /** @brief Create an idle queue using native admission policy; the dedicated worker starts lazily for eligible jobs. */
    constructor(private readonly native: JobsExports, private readonly codec: JobsCodec,
        private readonly worker_url: string | URL, private readonly wasm_url: string | URL) {}

    // Bound methods retain the original factory's callback-safe API.

    /** @brief Decode small messages locally or queue a copied buffer for worker transfer; reject cancellation, timeout or exhausted capacity. */
    readonly decode = (buffer: CodecBuffer, signal?: AbortSignal): Promise<JsonValue> => {
        return new Promise((resolve, reject) => {
            if (!this.native.lab_jobs_worker_accepts(this.phase))
                throw new Error('Codec queue is closed');
            if (signal?.aborted)
                throw signal.reason || new DOMException('Cancelled', 'AbortError');
            const bytes = this.codec.bytes(buffer);
            if (bytes.length < this.native.lab_jobs_threshold()) {
                resolve(this.codec.decode(bytes));
                return;
            }
            const token = this.native.lab_jobs_submit(bytes.length);
            if (!token)
                throw new Error('Codec queue is full');
            let copy: Uint8Array<ArrayBuffer>;
            try {
                copy = bytes.slice();
            } catch (error) {
                this.native.lab_jobs_finish(token);
                throw error;
            }
            const job: PendingJob = {
                bytes: copy, resolve, reject, signal, timer: undefined,
                abort: () => this.cancel(token, signal?.reason || new DOMException('Cancelled', 'AbortError'))
            };
            this.jobs.set(token, job);
            job.timer = setTimeout(() => this.cancel(token, new DOMException('Codec job timed out', 'TimeoutError')),
                this.native.lab_jobs_timeout());
            signal?.addEventListener('abort', job.abort, {once: true});
            this.pump();
        });
    };

    /** @brief Return a snapshot of native queue totals and this instance's worker activity counters. */
    readonly status = (): JobsStatus => ({
        count: this.native.lab_jobs_count(), bytes: this.native.lab_jobs_bytes(), active: this.active,
        started: this.started, completed: this.completed
    });

    /** @brief Permanently close the queue, terminate its worker and reject pending jobs while releasing timers and abort listeners. */
    readonly close = (): void => {
        this.fail(new DOMException('Codec queue closed', 'AbortError'), worker_events.close);
    };

    private transition(event: WorkerEvent, token = 0): number {
        const plan = this.native.lab_jobs_worker_step(this.phase, event, token, this.active);
        this.phase = plan & 255;
        return plan >>> 8;
    }

    private stop(): void {
        this.worker?.terminate();
        this.worker = null;
    }

    private retire(token: number, outcome: {value: JsonValue} | {error: unknown}): void {
        const job = this.jobs.get(token);
        if (!job || !this.native.lab_jobs_finish(token))
            return;
        this.jobs.delete(token);
        job.bytes?.fill(0);
        job.bytes = null;
        clearTimeout(job.timer);
        job.signal?.removeEventListener('abort', job.abort);
        if (this.active === token)
            this.active = 0;
        if ('error' in outcome)
            job.reject(outcome.error);
        else {
            ++this.completed;
            job.resolve(outcome.value);
        }
    }

    private fail(error: unknown, event: WorkerEvent = worker_events.failure): void {
        this.transition(event);
        this.stop();
        // Map iteration is bounded by the eight-job native admission limit.
        for (const token of this.jobs.keys())
            this.retire(token, {error});
    }

    private cancel(token: number, error: unknown): void {
        if (!this.jobs.has(token))
            return;
        if (this.transition(worker_events.cancel, token) === worker_actions.stop)
            this.stop();
        this.retire(token, {error});
        this.pump();
    }

    private receive(instance: Worker, data: unknown): void {
        if (this.worker !== instance)
            return;
        if (!is_worker_response(data)) {
            this.fail(new Error('Unexpected codec worker response'));
            return;
        }
        const event = worker_events[data.type];
        const token = data.type === 'ready' ? 0 : data.token;
        const effect = this.transition(event, token);
        if (effect === worker_actions.fail) {
            this.fail(new Error(data.type === 'error' ? data.message : 'Codec worker failed'));
            return;
        }
        if (effect === worker_actions.finish) {
            this.finish_response(data);
        } else if (effect === worker_actions.invalid) {
            this.fail(new Error('Unexpected codec worker response'));
            return;
        }
        this.pump();
    }

    private finish_response(message: WorkerResponse): void {
        if (message.type === 'result')
            this.retire(this.active, {value: message.value});
        else if (message.type === 'error')
            this.retire(this.active, {error: new Error(message.message)});
        else
            this.fail(new Error('Unexpected codec worker response'));
    }

    private start_worker(): void {
        try {
            const instance = new Worker(this.worker_url, {type: 'module'});
            this.worker = instance;
            ++this.started;
            instance.onerror = event => {
                event.preventDefault();
                if (this.worker === instance)
                    this.fail(new Error('Codec worker failed to start or execute'));
            };
            instance.onmessageerror = () => {
                if (this.worker === instance)
                    this.fail(new Error('Invalid codec worker response'));
            };
            instance.onmessage = (event: MessageEvent<unknown>) => this.receive(instance, event.data);
            const message: WorkerRequest = {type: 'init', url: String(this.wasm_url)};
            instance.postMessage(message);
        } catch (error) {
            this.fail(error);
        }
    }

    private pump(): void {
        const action = this.transition(worker_events.pump);
        if (action === worker_actions.start) {
            this.start_worker();
            return;
        }
        if (action === worker_actions.invalid) {
            this.fail(new Error('Invalid codec worker state'));
            return;
        }
        if (action !== worker_actions.send)
            return;
        const token = this.native.lab_jobs_take();
        const job = this.jobs.get(token);
        if (!job)
            return;
        this.active = token;
        try {
            if (!this.worker || !job.bytes)
                throw new Error('Invalid codec worker state');
            const message: WorkerRequest = {type: 'decode', token, bytes: job.bytes};
            this.worker.postMessage(message, [job.bytes.buffer]);
            job.bytes = null;
        } catch (error) {
            this.fail(error);
        }
    }
}

/** @brief Create a queue over an initialised codec and native job controller; callers own its eventual close. */
export function createJobs(native: JobsExports, codec: JobsCodec,
    workerURL: string | URL, wasmURL: string | URL): Jobs {
    return new Jobs(native, codec, workerURL, wasmURL);
}
