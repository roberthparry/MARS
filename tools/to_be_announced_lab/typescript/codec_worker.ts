/**
 * @file codec_worker.ts
 * @brief Private Lab dedicated-worker entry point for native codec operations.
 *
 * Initialises an isolated codec instance and validates incoming messages. It has no
 * DOM access or worksheet state; results travel through the typed worker protocol.
 */
/** Dedicated codec instance: no DOM imports, shared memory, network payloads or persistence. */
import {createCodec} from './codec.js';
import type {Codec} from './codec.js';
import {error_message, is_record, is_worker_request} from './codec_types.js';
import type {WorkerResponse} from './codec_types.js';

/** Minimal dedicated-worker port; independent of the browser window declaration library. */
export interface CodecWorkerScope {
    onmessage: ((event: MessageEvent<unknown>) => void) | null;

    /** @brief Send a typed protocol response to the owning thread without exposing native handles. */
    postMessage(message: WorkerResponse): void;
}

/** Owns worker initialisation and refuses overlapping or out-of-order protocol operations. */
export class CodecWorker {
    private readonly codec: Codec = createCodec();
    private phase: 'new' | 'starting' | 'ready' | 'failed' = 'new';

    /** @brief Attach the worker message handler; each instance owns an isolated codec and its protocol phase. */
    constructor(private readonly scope: CodecWorkerScope) {
        scope.onmessage = event => { void this.receive(event.data); };
    }

    private async receive(data: unknown): Promise<void> {
        const token = is_record(data) && typeof data.token === 'number' && Number.isInteger(data.token) &&
            data.token > 0 && data.token <= 0xffffffff ? data.token : 0;
        try {
            if (!is_worker_request(data))
                throw new Error('Invalid codec worker message');
            if (data.type === 'init' && this.phase === 'new') {
                this.phase = 'starting';
                const response = await fetch(data.url, {cache: 'no-store'});
                if (!response.ok)
                    throw new Error('Could not load the worker codec');
                await this.codec.start(await response.arrayBuffer());
                this.phase = 'ready';
                this.scope.postMessage({type: 'ready'});
            } else if (data.type === 'decode' && this.phase === 'ready') {
                const value = this.codec.decode(data.bytes);
                this.scope.postMessage({type: 'result', token: data.token, value});
            } else {
                throw new Error('Invalid codec worker message');
            }
        } catch (error) {
            if (this.phase === 'starting')
                this.phase = 'failed';
            this.scope.postMessage({type: 'error', token, message: error_message(error)});
        } finally {
            if (is_record(data) && data.type === 'decode' && data.bytes instanceof Uint8Array)
                data.bytes.fill(0);
        }
    }
}

// This module is only loaded as a dedicated worker, never as a window entry point.
new CodecWorker(globalThis as unknown as CodecWorkerScope);
