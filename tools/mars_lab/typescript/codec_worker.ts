/**
 * @file codec_worker.ts
 * @brief Dedicated-worker entry point for native codec operations.
 *
 * Initialises an isolated codec instance and validates incoming messages. It has no
 * DOM access or worksheet state; results travel through the typed worker protocol.
 */
/** Dedicated codec instance: no DOM imports, shared memory, network payloads or persistence. */
import {createCodec} from './codec.js';
import type {Codec} from './codec.js';
import {errorMessage, isRecord, isWorkerRequest} from './codec_types.js';
import type {WorkerDecodeRequest, WorkerInitialiseRequest, WorkerResponse} from './codec_types.js';

/** Browser-delivered message callback owned by one codec worker. */
interface CodecWorkerMessageHandler {
    /** @brief Dispatch one message without blocking the worker event loop during startup. */
    (event: MessageEvent<unknown>): void;
}

/** Minimal dedicated-worker port; independent of the browser window declaration library. */
export interface CodecWorkerScope {
    onmessage: CodecWorkerMessageHandler | null;

    /** @brief Send a typed protocol response to the owning thread without exposing native handles. */
    postMessage(message: WorkerResponse): void;
}

/** Owns worker initialisation and refuses overlapping or out-of-order protocol operations. */
export class CodecWorker {
    private readonly codec: Codec = createCodec();
    private phase: 'new' | 'starting' | 'ready' | 'failed' = 'new';

    /** @brief Attach the worker message handler; each instance owns an isolated codec and its protocol phase. */
    constructor(private readonly scope: CodecWorkerScope) {
        scope.onmessage = this.onMessage.bind(this);
    }

    private onMessage(event: MessageEvent<unknown>): void {
        void this.receive(event.data);
    }

    private requestToken(data: unknown): number {
        if (!isRecord(data) || typeof data.token !== 'number') return 0;
        if (!Number.isInteger(data.token) || data.token <= 0 || data.token > 0xffffffff) return 0;
        return data.token;
    }

    private async initialise(request: WorkerInitialiseRequest): Promise<void> {
        this.phase = 'starting';
        const response = await fetch(request.url, {cache: 'no-store'});
        if (!response.ok) throw new Error('Could not load the worker codec');
        await this.codec.start(await response.arrayBuffer());
        this.phase = 'ready';
        this.scope.postMessage({type: 'ready'});
    }

    private decode(request: WorkerDecodeRequest): void {
        const value = this.codec.decode(request.bytes);
        this.scope.postMessage({type: 'result', token: request.token, value});
    }

    private reportError(token: number, error: unknown): void {
        if (this.phase === 'starting') this.phase = 'failed';
        this.scope.postMessage({type: 'error', token, message: errorMessage(error)});
    }

    private releaseInput(data: unknown): void {
        if (isRecord(data) && data.type === 'decode' && data.bytes instanceof Uint8Array)
            data.bytes.fill(0);
    }

    private async receive(data: unknown): Promise<void> {
        const token = this.requestToken(data);
        try {
            if (!isWorkerRequest(data))
                throw new Error('Invalid codec worker message');
            if (data.type === 'init' && this.phase === 'new') {
                await this.initialise(data);
            } else if (data.type === 'decode' && this.phase === 'ready') {
                this.decode(data);
            } else {
                throw new Error('Invalid codec worker message');
            }
        } catch (error) {
            this.reportError(token, error);
        } finally {
            this.releaseInput(data);
        }
    }
}

/** @brief Worker-lifetime codec owner; this entry point is never loaded into the browser window. */
const codecWorker: CodecWorker = new CodecWorker(globalThis as unknown as CodecWorkerScope);
