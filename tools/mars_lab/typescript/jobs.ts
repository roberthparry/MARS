/**
 * @file jobs.ts
 * @brief Browser resources for the native asynchronous job queue.
 *
 * Owns the dedicated worker, pending promises and cancellation lifetime. Each pending
 * job owns its timer, abort subscription and untransferred input copy. Native C
 * selects queue transitions; browser code applies their effects without blocking the UI.
 */
/** Browser resources for the C-owned queue and worker lifecycle; no UI-thread waits. */
import {isWorkerResponse} from './codec_types.js';
import type {CodecBuffer, JobsExports, JsonValue, WorkerRequest, WorkerResponse} from './codec_types.js';
export type {JobsExports} from './codec_types.js';

/** @brief Native worker event identifiers, matching the module ABI in lab_jobs.h. */
export const workerEvents = Object.freeze({pump: 0, ready: 1, result: 2, error: 3, failure: 4, cancel: 5,
    close: 6, unknown: 7} as const);

/** @brief Browser effects selected by the native worker lifecycle controller. */
export const workerActions = Object.freeze({wait: 0, start: 1, send: 2, finish: 3, fail: 4, invalid: 5, stop: 6} as const);

/** Numeric events accepted by the native lifecycle ABI. */
type WorkerEvent = 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7;

/** Browser codec capabilities borrowed by the native queue adapter. */
export interface JobsCodec {
    /** @brief Borrow the exact bounded, non-shared input range without copying or transferring ownership. */
    bytes(buffer: CodecBuffer): Uint8Array<ArrayBuffer>;

    /** @brief Decode synchronously on the calling thread; malformed messages must be rejected. */
    decode(buffer: CodecBuffer): JsonValue;
}

/** Snapshot of native admission totals and browser worker activity. */
export interface JobsStatus {
    count: number;
    bytes: number;
    active: number;
    started: number;
    completed: number;
}

/** Promise completion callback; the standard Promise executor supplies this capability. */
interface JobResolve {
    /** @brief Complete a decode with a validated JSON value. */
    (value: JsonValue): void;
}

/** Promise failure callback retained until native retirement. */
interface JobReject {
    /** @brief Reject a decode with its original failure reason. */
    (error: unknown): void;
}

/** Queue-owned cancellation capability, independent of browser event objects. */
interface JobCancel {
    /** @brief Cancel the exact native token if it is still pending. */
    (token: number, error: unknown): void;
}

/** Owns one pending job's input copy, promise callbacks, abort listener and deadline. */
class PendingJob {
    private timer: number | undefined;

    /** @brief Retain a copied buffer and callbacks; registration occurs only after queue insertion. */
    constructor(public bytes: Uint8Array<ArrayBuffer> | null,
                readonly resolve: JobResolve, readonly reject: JobReject,
                private readonly signal: AbortSignal | undefined,
                private readonly token: number, private readonly cancel: JobCancel) {
        this.abort = this.abort.bind(this);
        this.timeout = this.timeout.bind(this);
    }

    /** @brief Start the native-selected deadline and subscribe to future cancellation. */
    arm(delay: number): void {
        this.timer = setTimeout(this.timeout, delay);
        this.signal?.addEventListener('abort', this.abort, {once: true});
    }

    /** @brief Erase any untransferred copy and release this job's timer and listener. */
    cleanup(): void {
        this.bytes?.fill(0);
        this.bytes = null;
        clearTimeout(this.timer);
        this.timer = undefined;
        this.signal?.removeEventListener('abort', this.abort);
    }

    private abort(): void {
        this.cancel(this.token, this.signal?.reason || new DOMException('Cancelled', 'AbortError'));
    }

    private timeout(): void {
        this.cancel(this.token, new DOMException('Codec job timed out', 'TimeoutError'));
    }
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
        private readonly workerUrl: string | URL, private readonly wasmUrl: string | URL) {
        this.decode = this.decode.bind(this);
        this.status = this.status.bind(this);
        this.close = this.close.bind(this);
        this.cancel = this.cancel.bind(this);
    }

    // Bound methods retain the original factory's callback-safe API.

    /** @brief Decode small messages locally or queue a copied buffer for worker transfer; reject cancellation, timeout or exhausted capacity. */
    decode(buffer: CodecBuffer, signal?: AbortSignal): Promise<JsonValue> {
        // The executor runs immediately; thrown admission errors become promise rejections.
        return new Promise<JsonValue>(this.enqueue.bind(this, buffer, signal));
    }

    private enqueue(buffer: CodecBuffer, signal: AbortSignal | undefined,
                    resolve: JobResolve, reject: JobReject): void {
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
        const job = new PendingJob(copy, resolve, reject, signal, token, this.cancel);
        this.jobs.set(token, job);
        job.arm(this.native.lab_jobs_timeout());
        this.pump();
    }

    /** @brief Return a snapshot of native queue totals and this instance's worker activity counters. */
    status(): JobsStatus {
        return {count: this.native.lab_jobs_count(), bytes: this.native.lab_jobs_bytes(), active: this.active,
            started: this.started, completed: this.completed};
    }

    /** @brief Permanently close the queue, terminate its worker and reject pending jobs while releasing timers and abort listeners. */
    close(): void {
        this.fail(new DOMException('Codec queue closed', 'AbortError'), workerEvents.close);
    }

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
        job.cleanup();
        if (this.active === token)
            this.active = 0;
        if ('error' in outcome)
            job.reject(outcome.error);
        else {
            ++this.completed;
            job.resolve(outcome.value);
        }
    }

    private fail(error: unknown, event: WorkerEvent = workerEvents.failure): void {
        this.transition(event);
        this.stop();
        // Map iteration is bounded by the eight-job native admission limit.
        for (const token of this.jobs.keys())
            this.retire(token, {error});
    }

    private cancel(token: number, error: unknown): void {
        if (!this.jobs.has(token))
            return;
        if (this.transition(workerEvents.cancel, token) === workerActions.stop)
            this.stop();
        this.retire(token, {error});
        this.pump();
    }

    private receive(instance: Worker, data: unknown): void {
        if (this.worker !== instance)
            return;
        if (!isWorkerResponse(data)) {
            this.fail(new Error('Unexpected codec worker response'));
            return;
        }
        const event = workerEvents[data.type];
        const token = data.type === 'ready' ? 0 : data.token;
        const effect = this.transition(event, token);
        if (effect === workerActions.fail) {
            this.fail(new Error(data.type === 'error' ? data.message : 'Codec worker failed'));
            return;
        }
        if (effect === workerActions.finish) {
            this.finishResponse(data);
        } else if (effect === workerActions.invalid) {
            this.fail(new Error('Unexpected codec worker response'));
            return;
        }
        this.pump();
    }

    private finishResponse(message: WorkerResponse): void {
        if (message.type === 'result')
            this.retire(this.active, {value: message.value});
        else if (message.type === 'error')
            this.retire(this.active, {error: new Error(message.message)});
        else
            this.fail(new Error('Unexpected codec worker response'));
    }

    private startWorker(): void {
        try {
            const instance = new Worker(this.workerUrl, {type: 'module'});
            this.worker = instance;
            ++this.started;
            instance.onerror = this.workerError.bind(this, instance);
            instance.onmessageerror = this.workerMessageError.bind(this, instance);
            instance.onmessage = this.workerMessage.bind(this, instance);
            const message: WorkerRequest = {type: 'init', url: String(this.wasmUrl)};
            instance.postMessage(message);
        } catch (error) {
            this.fail(error);
        }
    }

    private workerError(instance: Worker, event: ErrorEvent): void {
        event.preventDefault();
        if (this.worker === instance)
            this.fail(new Error('Codec worker failed to start or execute'));
    }

    private workerMessageError(instance: Worker): void {
        if (this.worker === instance)
            this.fail(new Error('Invalid codec worker response'));
    }

    private workerMessage(instance: Worker, event: MessageEvent<unknown>): void {
        this.receive(instance, event.data);
    }

    private pump(): void {
        const action = this.transition(workerEvents.pump);
        if (action === workerActions.start) {
            this.startWorker();
            return;
        }
        if (action === workerActions.invalid) {
            this.fail(new Error('Invalid codec worker state'));
            return;
        }
        if (action !== workerActions.send)
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
