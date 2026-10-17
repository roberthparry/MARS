/**
 * @file codec_types.ts
 * @brief Transport contracts and validation for codec workers.
 *
 * Defines JSON values, native exports and worker messages shared by the codec and
 * job controller. Validation rejects malformed boundary data before it reaches native code.
 */
/** Shared transport types and validation for the browser codec and its dedicated worker. */
export type JsonValue = null | boolean | number | string | JsonValue[] | JsonObject;
export interface JsonObject { [key: string]: JsonValue; }
export type CodecBuffer = ArrayBuffer | ArrayBufferView<ArrayBuffer>;

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

    /** @brief Return the address of the borrowed native input region used for one bounded decode. */
    lab_browser_input_buffer(): number;

    /** @brief Return the address of borrowed encoded bytes, which callers must copy before another operation. */
    lab_browser_output_buffer(): number;

    /** @brief Normalise requested precision with native bounds and fallback policy. */
    lab_browser_precision(value: number, fallback: number): number;

    /** @brief Normalise the integration interval cap with native bounds and fallback policy. */
    lab_browser_intervals(value: number, fallback: number): number;
}

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

export type WorkerRequest = { type: 'init'; url: string } |
    { type: 'decode'; token: number; bytes: Uint8Array<ArrayBuffer> };
export type WorkerResponse = { type: 'ready' } |
    { type: 'result'; token: number; value: JsonValue } |
    { type: 'error'; token: number; message: string };

/** @brief Narrow non-null, non-array objects without requiring a particular prototype or inspecting their properties. */
export function is_record(value: unknown): value is Record<string, unknown> {
    return typeof value === 'object' && value !== null && !Array.isArray(value);
}

function is_token(value: unknown, allow_zero = false): value is number {
    return typeof value === 'number' && Number.isInteger(value) && value >= (allow_zero ? 0 : 1) &&
        value <= 0xffffffff;
}

/** Iterative validation rejects cycles, but permits aliases to fully validated noncyclic containers. */
export function is_json_value(value: unknown): value is JsonValue {
    const pending: ({value: unknown} | {completed: object})[] = [{value}];
    const active = new Set<object>();
    const completed = new Set<object>();
    while (pending.length) {
        const frame = pending.pop();
        if (!frame)
            break;
        if ('completed' in frame) {
            active.delete(frame.completed);
            completed.add(frame.completed);
            continue;
        }
        const item = frame.value;
        if (item === null || typeof item === 'boolean' || typeof item === 'string')
            continue;
        if (typeof item === 'number') {
            if (!Number.isFinite(item))
                return false;
            continue;
        }
        if (typeof item !== 'object' || item === null || active.has(item))
            return false;
        if (completed.has(item))
            continue;
        if (!Array.isArray(item) && Object.getPrototypeOf(item) !== Object.prototype &&
            Object.getPrototypeOf(item) !== null)
            return false;
        const descriptors = Object.getOwnPropertyDescriptors(item);
        active.add(item);
        pending.push({completed: item});
        if (Array.isArray(item)) {
            for (let index = 0; index < item.length; ++index) {
                const descriptor = descriptors[String(index)];
                if (!descriptor || !Object.hasOwn(descriptor, 'value'))
                    return false;
                pending.push({value: descriptor.value});
            }
        } else {
            for (const descriptor of Object.values(descriptors)) {
                if (!descriptor.enumerable)
                    continue;
                if (!Object.hasOwn(descriptor, 'value'))
                    return false;
                pending.push({value: descriptor.value});
            }
        }
    }
    return true;
}

/** @brief Validate initialisation or decode messages, requiring a positive native token and non-shared byte array for decoding. */
export function is_worker_request(value: unknown): value is WorkerRequest {
    if (!is_record(value))
        return false;
    return (value.type === 'init' && typeof value.url === 'string') ||
        (value.type === 'decode' && is_token(value.token) && value.bytes instanceof Uint8Array &&
            value.bytes.buffer instanceof ArrayBuffer);
}

/** @brief Validate worker reply tags and tokens, checking decoded results before the main thread accepts them. */
export function is_worker_response(value: unknown): value is WorkerResponse {
    if (!is_record(value))
        return false;
    return (value.type === 'ready' && (value.token === undefined || value.token === 0)) ||
        (value.type === 'result' && is_token(value.token) && is_json_value(value.value)) ||
        (value.type === 'error' && is_token(value.token, true) && typeof value.message === 'string');
}

/** @brief Extract an Error diagnostic or stringify another thrown value for the worker protocol. */
export function error_message(error: unknown): string {
    return error instanceof Error ? error.message : String(error);
}
