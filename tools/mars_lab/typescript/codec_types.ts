/**
 * @file codec_types.ts
 * @brief Transport contracts and validation for codec workers.
 *
 * Defines JSON values, native exports and worker messages shared by the codec and
 * job controller. Validation rejects malformed boundary data before it reaches native code.
 * Each JSON check owns an iterative traversal: data descriptors are inspected without
 * invoking property getters, cycles are rejected and completed aliases are accepted.
 */
/** Shared transport types and validation for the browser codec and its dedicated worker. */
export type JsonValue = null | boolean | number | string | Uint8Array<ArrayBuffer> | JsonValue[] | JsonObject;

/** @brief JSON object whose string-named members contain validated JSON values. */
export interface JsonObject { [key: string]: JsonValue; }

/** @brief Unvalidated non-array object at a browser message boundary. */
export interface UnknownRecord { [key: string]: unknown; }

/** @brief Non-shared binary input or a view over non-shared memory. */
export type CodecBuffer = ArrayBuffer | ArrayBufferView<ArrayBuffer>;

/** @brief Native codec operations and memory borrowed by the browser adapter. */
export interface CodecExports extends WebAssembly.Exports, JobsExports {
    memory: WebAssembly.Memory;

    /** @brief Return the native ABI version required before this codec instance can be used. */
    lab_browser_abi_version(): number;

    /** @brief Return the maximum transient host handles admitted during one codec operation. */
    lab_browser_handle_capacity(): number;

    /** @brief Return the native per-container member limit used before allocating host snapshots. */
    lab_browser_member_limit(): number;

    /** @brief Return the maximum binary message length accepted by native input and output buffers. */
    lab_browser_message_limit(): number;

    /** @brief Return the native limit for a base64-encoded message representation. */
    lab_browser_base64_limit(): number;

    /** @brief Encode a transient host-value handle and return the output byte length, or zero on rejection. */
    lab_browser_encode(handle: number): number;

    /** @brief Decode the staged native input and return its host-value handle, or a negative result on rejection. */
    lab_browser_decode(length: number): number;

    /** @brief Encode a host record as base64 in the input buffer; return its length, or zero on rejection. */
    lab_browser_storage_encode(handle: number): number;

    /** @brief Validate and decode staged base64/Protobuf; return a host handle, or a negative result on rejection. */
    lab_browser_storage_decode(length: number): number;

    /** @brief Return the address of the borrowed native input region used for one bounded decode. */
    lab_browser_input_buffer(): number;

    /** @brief Return the address of borrowed encoded bytes, which callers must copy before another operation. */
    lab_browser_output_buffer(): number;

    /** @brief Normalise requested precision with native bounds and fallback policy. */
    lab_browser_precision(value: number, fallback: number): number;

    /** @brief Normalise the integration interval cap with native bounds and fallback policy. */
    lab_browser_intervals(value: number, fallback: number): number;
}

/** @brief Native admission, accounting and worker lifecycle operations. */
export interface JobsExports {

    /** @brief Advance the native worker lifecycle and return its packed phase and browser-effect plan. */
    lab_jobs_worker_step(phase: number, event: number, token: number, active: number): number;

    /** @brief Report whether the supplied lifecycle phase admits another decode request. */
    lab_jobs_worker_accepts(phase: number): number;

    /** @brief Retire a native job token and report whether it still owned a queue entry. */
    lab_jobs_finish(token: number): number;

    /** @brief Select the next queued native token for worker dispatch, or zero when none is available. */
    lab_jobs_take(): number;

    /** @brief Admit a message of the supplied byte length and return its token, or zero when capacity is exhausted. */
    lab_jobs_submit(bytes: number): number;

    /** @brief Return the message-size threshold at which decoding should use the worker queue. */
    lab_jobs_threshold(): number;

    /** @brief Return the maximum number of jobs permitted by native queue admission policy. */
    lab_jobs_capacity(): number;

    /** @brief Return the native job deadline in milliseconds for browser timer scheduling. */
    lab_jobs_timeout(): number;

    /** @brief Return the number of jobs currently retained by the native queue. */
    lab_jobs_count(): number;

    /** @brief Return the total message bytes currently accounted for by the native queue. */
    lab_jobs_bytes(): number;
}

/** @brief Initialise an isolated worker codec from the supplied module URL. */
export interface WorkerInitialiseRequest {
    type: 'init';
    url: string;
}

/** @brief Decode an owned binary message for a positive native job token. */
export interface WorkerDecodeRequest {
    type: 'decode';
    token: number;
    bytes: Uint8Array<ArrayBuffer>;
}

/** @brief Announce successful worker initialisation. */
export interface WorkerReadyResponse {
    type: 'ready';
}

/** @brief Complete a native job with validated JSON data. */
export interface WorkerResultResponse {
    type: 'result';
    token: number;
    value: JsonValue;
}

/** @brief Report a job failure; token zero identifies a worker initialisation failure. */
export interface WorkerErrorResponse {
    type: 'error';
    token: number;
    message: string;
}

/** @brief Requests admitted by the isolated codec worker. */
export type WorkerRequest = WorkerInitialiseRequest | WorkerDecodeRequest;

/** @brief Replies admitted by the browser job controller. */
export type WorkerResponse = WorkerReadyResponse | WorkerResultResponse | WorkerErrorResponse;

/** @brief Narrow non-null, non-array objects without requiring a particular prototype or inspecting their properties. */
export function isRecord(value: unknown): value is UnknownRecord {
    return typeof value === 'object' && value !== null && !Array.isArray(value);
}

function isToken(value: unknown, allowZero = false): value is number {
    return typeof value === 'number' && Number.isInteger(value) && value >= (allowZero ? 0 : 1) &&
        value <= 0xffffffff;
}

/** A value awaiting descriptor-based validation. */
interface JsonPendingValue { value: unknown; }

/** Marks completion only after all descendants have been validated. */
interface JsonCompletedValue { completed: object; }

/** String-named descriptors; symbol members retain the existing ignored semantics. */
interface JsonDescriptors { [key: string]: PropertyDescriptor; }

/** Owns one depth-first traversal without consuming the JavaScript call stack. */
class JsonValueValidator {
    private readonly pending: (JsonPendingValue | JsonCompletedValue)[] = [];
    private readonly active = new Set<object>();
    private readonly completed = new Set<object>();

    /** @brief Validate one input graph using fresh, invocation-local traversal state. */
    validate(value: unknown): boolean {
        this.pending.push({value});
        while (this.pending.length) {
            const frame = this.pending.pop();
            if (!frame) break;
            if ('completed' in frame) {
                this.active.delete(frame.completed);
                this.completed.add(frame.completed);
            } else if (!this.visit(frame.value)) {
                return false;
            }
        }
        return true;
    }

    private visit(item: unknown): boolean {
        if (item instanceof Uint8Array && item.buffer instanceof ArrayBuffer) return true;
        if (item === null || typeof item === 'boolean' || typeof item === 'string')
            return true;
        if (typeof item === 'number')
            return Number.isFinite(item);
        if (typeof item !== 'object' || item === null || this.active.has(item))
            return false;
        if (this.completed.has(item))
            return true;
        if (!Array.isArray(item) && Object.getPrototypeOf(item) !== Object.prototype &&
            Object.getPrototypeOf(item) !== null)
            return false;
        const descriptors = Object.getOwnPropertyDescriptors(item);
        this.active.add(item);
        this.pending.push({completed: item});
        if (Array.isArray(item))
            return this.queueArray(item, descriptors);
        return this.queueObject(descriptors);
    }

    private queueArray(item: unknown[], descriptors: JsonDescriptors): boolean {
        for (let index = 0; index < item.length; ++index) {
            if (!this.queueDescriptor(descriptors[String(index)])) return false;
        }
        return true;
    }

    private queueObject(descriptors: JsonDescriptors): boolean {
        for (const descriptor of Object.values(descriptors)) {
            if (!descriptor.enumerable) continue;
            if (!this.queueDescriptor(descriptor)) return false;
        }
        return true;
    }

    private queueDescriptor(descriptor: PropertyDescriptor | undefined): boolean {
        if (!descriptor || !Object.hasOwn(descriptor, 'value')) return false;
        this.pending.push({value: descriptor.value});
        return true;
    }
}

/** @brief Reject cycles and sparse arrays without invoking getters; permit completed noncyclic aliases. */
export function isJsonValue(value: unknown): value is JsonValue {
    return new JsonValueValidator().validate(value);
}

/** @brief Validate initialisation or decode messages, requiring a positive native token and non-shared byte array for decoding. */
export function isWorkerRequest(value: unknown): value is WorkerRequest {
    if (!isRecord(value))
        return false;
    return (value.type === 'init' && typeof value.url === 'string') ||
        (value.type === 'decode' && isToken(value.token) && value.bytes instanceof Uint8Array &&
            value.bytes.buffer instanceof ArrayBuffer);
}

/** @brief Validate worker reply tags and tokens, checking decoded results before the main thread accepts them. */
export function isWorkerResponse(value: unknown): value is WorkerResponse {
    if (!isRecord(value))
        return false;
    return (value.type === 'ready' && (value.token === undefined || value.token === 0)) ||
        (value.type === 'result' && isToken(value.token) && isJsonValue(value.value)) ||
        (value.type === 'error' && isToken(value.token, true) && typeof value.message === 'string');
}

/** @brief Extract an Error diagnostic or stringify another thrown value for the worker protocol. */
export function errorMessage(error: unknown): string {
    return error instanceof Error ? error.message : String(error);
}
