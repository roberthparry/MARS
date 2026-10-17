/**
 * @file codec.ts
 * @brief Instance-local browser bridge for the native binary codec.
 *
 * Owns a WebAssembly codec instance and transient host-value handles. Binary parsing
 * remains in C; this ES module supplies bounded browser capabilities to the codec.
 */
/** Instance-local host-value bridge; all Protobuf parsing remains in the bounded C codec. */
import {is_json_value, is_record} from './codec_types.js';
import type {CodecBuffer, CodecExports, JsonValue} from './codec_types.js';
export type {CodecBuffer, CodecExports, JsonValue} from './codec_types.js';

export type CodecEnvironment = WebAssembly.ModuleImports;

/** Owns one WASM instance and the ephemeral handles used during a single codec operation. */
export class Codec {
    private readonly encoder = new TextEncoder();
    private readonly decoder = new TextDecoder('utf-8', {fatal: true, ignoreBOM: true});
    private wasm: CodecExports | undefined;
    private values: unknown[] = [];
    private keys: (string[] | null)[] = [];
    private types: number[] = [];
    private busy = false;
    private starting = false;

    /** @brief Create an unstarted codec with optional native host imports; bound methods retain this instance when detached. */
    constructor(private readonly environment: CodecEnvironment = {}) {
        // Compatibility factories exposed callbacks that retained their instance through closures.
        this.exports = this.exports.bind(this);
        this.start = this.start.bind(this);
        this.bytes = this.bytes.bind(this);
        this.encode = this.encode.bind(this);
        this.decode = this.decode.bind(this);
        this.text = this.text.bind(this);
        this.precision = this.precision.bind(this);
        this.intervals = this.intervals.bind(this);
    }

    /** @brief Return the validated native exports, or throw until initialisation has completed. */
    exports(): CodecExports {
        if (!this.wasm)
            throw new Error('C browser module is not ready');
        return this.wasm;
    }

    /** @brief Instantiate and validate the native codec; reject overlapping initialisation or codec operations. */
    async start(bytes: BufferSource | WebAssembly.Module): Promise<void> {
        if (this.starting || this.busy)
            throw new Error('Lab codec is busy');
        this.starting = true;
        try {
            const imports = {env: this.host_imports()};
            const instance = bytes instanceof WebAssembly.Module ? await WebAssembly.instantiate(bytes, imports) :
                (await WebAssembly.instantiate(bytes, imports)).instance;
            const exports = instance.exports;
            if (!this.valid_exports(exports))
                throw new Error('The Lab codec ABI is incompatible; rebuild and restart');
            this.wasm = exports;
        } finally {
            this.starting = false;
        }
    }

    private valid_exports(exports: WebAssembly.Exports): exports is CodecExports {
        const functions = ['lab_browser_abi_version', 'lab_browser_handle_capacity', 'lab_browser_member_limit',
            'lab_browser_message_limit', 'lab_browser_encode', 'lab_browser_decode', 'lab_browser_input_buffer',
            'lab_browser_output_buffer', 'lab_browser_precision', 'lab_browser_intervals', 'lab_browser_base64_limit',
            'lab_jobs_worker_step', 'lab_jobs_worker_accepts', 'lab_jobs_finish', 'lab_jobs_take', 'lab_jobs_submit',
            'lab_jobs_threshold', 'lab_jobs_capacity', 'lab_jobs_timeout', 'lab_jobs_count', 'lab_jobs_bytes'] as const;
        return exports.memory instanceof WebAssembly.Memory &&
            functions.every(name => typeof exports[name] === 'function') &&
            exports.lab_browser_abi_version instanceof Function && exports.lab_browser_abi_version() === 37;
    }

    /** @brief Borrow the exact byte range of a non-shared buffer or view, rejecting inputs above the native message limit. */
    bytes(buffer: CodecBuffer): Uint8Array<ArrayBuffer> {
        const backing = ArrayBuffer.isView(buffer) ? buffer.buffer : buffer;
        // Use the intrinsic brand check: reject shared buffers even when supplied through a view.
        const length = Object.getOwnPropertyDescriptor(ArrayBuffer.prototype, 'byteLength')?.get;
        try {
            if (!length)
                throw new Error('Missing ArrayBuffer intrinsic');
            length.call(backing);
        } catch {
            throw new Error('Lab codec requires an ArrayBuffer or typed view');
        }
        const bytes = ArrayBuffer.isView(buffer) ?
            new Uint8Array(backing, buffer.byteOffset, buffer.byteLength) : new Uint8Array(backing);
        if (bytes.length > this.exports().lab_browser_message_limit())
            throw new Error('Lab Protobuf response exceeds 8 MiB');
        return bytes;
    }

    /** @brief Encode through native C and return an independent byte copy; reject re-entry and release transient handles on failure. */
    encode(value: unknown): Uint8Array<ArrayBuffer> {
        const wasm = this.exports();
        this.enter();
        try {
            const size = wasm.lab_browser_encode(this.add(value));
            if (!size)
                throw new Error('Invalid or oversized Lab Protobuf message');
            return new Uint8Array(wasm.memory.buffer, wasm.lab_browser_output_buffer(), size).slice();
        } finally {
            this.leave();
        }
    }

    /** @brief Decode through native C into validated JSON values, then erase the native input buffer and release transient handles. */
    decode(buffer: CodecBuffer): JsonValue {
        const wasm = this.exports();
        this.enter();
        let length = 0;
        try {
            const bytes = this.bytes(buffer);
            length = bytes.length;
            new Uint8Array(wasm.memory.buffer, wasm.lab_browser_input_buffer(), length).set(bytes);
            const id = wasm.lab_browser_decode(length);
            const value = this.values[id];
            if (id < 0 || !is_json_value(value))
                throw new Error('Malformed or incompatible Lab Protobuf response');
            return value;
        } finally {
            new Uint8Array(wasm.memory.buffer, wasm.lab_browser_input_buffer(), length).fill(0);
            this.leave();
        }
    }

    /** @brief Copy a NUL-terminated UTF-8 string from native memory, rejecting invalid pointers and unterminated text. */
    text(pointer: number): string {
        const bytes = new Uint8Array(this.exports().memory.buffer);
        if (!Number.isInteger(pointer) || pointer < 0 || pointer >= bytes.length)
            throw new Error('Invalid C text');
        const end = bytes.indexOf(0, pointer);
        if (end < 0)
            throw new Error('Invalid C text');
        return this.decoder.decode(bytes.subarray(pointer, end));
    }

    /** @brief Ask native C to normalise requested precision using its bounds and the supplied fallback. */
    precision(value: number, fallback: number): number {
        return this.exports().lab_browser_precision(value, fallback);
    }

    /** @brief Ask native C to normalise the integration interval limit using the supplied fallback. */
    intervals(value: number, fallback: number): number {
        return this.exports().lab_browser_intervals(value, fallback);
    }

    private remember(value: unknown, type: number, names: string[] | null = null): number {
        if (this.values.length >= this.exports().lab_browser_handle_capacity())
            throw new Error('Lab message has too many values');
        this.values.push(value);
        this.keys.push(names);
        this.types.push(type);
        return this.values.length - 1;
    }

    private add(value: unknown): number {
        const type = value === null ? 0 : typeof value === 'boolean' ? 1 : typeof value === 'number' ? 2 :
            typeof value === 'string' ? 3 : Array.isArray(value) ? 4 :
            is_record(value) && (Object.getPrototypeOf(value) === Object.prototype ||
                Object.getPrototypeOf(value) === null) ? 5 : -1;
        let names: string[] | null = null;
        if (Array.isArray(value) || (type === 5 && is_record(value))) {
            const descriptors = Object.getOwnPropertyDescriptors(value);
            const length = Array.isArray(value) ? value.length : 0;
            names = type === 4 ? null : Object.keys(descriptors).filter(key => descriptors[key]?.enumerable);
            if ((names ? names.length : length) > this.exports().lab_browser_member_limit())
                throw new Error('Lab container has too many members');
            const copy: unknown[] | Record<string, unknown> = type === 4 ? [] : Object.create(null);
            const members = names ?? Array.from({length}, (_, index) => String(index));
            for (const name of members) {
                const descriptor = descriptors[name];
                if (!descriptor || !Object.hasOwn(descriptor, 'value'))
                    throw new Error('Lab messages require data properties, not getters or sparse arrays');
                Object.defineProperty(copy, name,
                    {value: descriptor.value, enumerable: true, writable: true, configurable: true});
            }
            value = copy;
        }
        return this.remember(value, type, names);
    }

    private write_text(value: unknown, pointer: number, capacity: number): number {
        if (typeof value !== 'string' || value.length > capacity)
            return -1;
        const target = new Uint8Array(this.exports().memory.buffer, pointer, capacity);
        const {read, written} = this.encoder.encodeInto(value, target);
        if (read !== value.length || this.decoder.decode(target.subarray(0, written)) !== value)
            return -1;
        return written;
    }

    private host_imports(): WebAssembly.ModuleImports {
        return {
            ...this.environment,
            lab_host_kind: (id: number) => this.types[id] ?? -1,
            lab_host_number: (id: number) => Number(this.values[id]),
            lab_host_count: (id: number) => {
                const value = this.values[id];
                return Array.isArray(value) ? value.length : (this.keys[id]?.length ?? 0);
            },
            lab_host_child_info: (id: number, index: number) => {
                const value = this.values[id];
                const key = this.keys[id]?.[index];
                const child = this.add(Array.isArray(value) ? value[index] :
                    is_record(value) && key !== undefined ? value[key] : undefined);
                const type = this.types[child] ?? -1;
                return type < 0 ? -1 : (child << 3) | type;
            },
            lab_host_key_text: (id: number, index: number, pointer: number, capacity: number) =>
                this.write_text(this.keys[id]?.[index], pointer, capacity),
            lab_host_text: (id: number, pointer: number, capacity: number) =>
                this.write_text(this.values[id], pointer, capacity),
            lab_host_value: (type: number, number: number, pointer: number, length: number,
                parent: number, key_pointer: number, key_length: number) =>
                this.receive_value(type, number, pointer, length, parent, key_pointer, key_length)
        };
    }

    private receive_value(type: number, number: number, pointer: number, length: number,
        parent: number, key_pointer: number, key_length: number): number {
        let value: JsonValue;
        if (type === 0)
            value = null;
        else if (type === 1)
            value = Boolean(number);
        else if (type === 2)
            value = number;
        else if (type === 3)
            value = this.decoder.decode(new Uint8Array(this.exports().memory.buffer, pointer, length));
        else if (type === 4)
            value = [];
        else if (type === 5)
            value = Object.create(null);
        else
            return -1;
        if (parent >= 0) {
            const owner = this.values[parent];
            if (Array.isArray(owner))
                owner.push(value);
            else if (is_record(owner)) {
                const key = this.decoder.decode(new Uint8Array(this.exports().memory.buffer, key_pointer, key_length));
                if (Object.hasOwn(owner, key))
                    return -1;
                Object.defineProperty(owner, key, {value, enumerable: true, writable: true, configurable: true});
            } else {
                return -1;
            }
        }
        return this.remember(value, type);
    }

    private enter(): void {
        if (this.busy || this.starting)
            throw new Error('Lab codec cannot be called recursively');
        this.busy = true;
        this.values = [];
        this.keys = [];
        this.types = [];
    }

    private leave(): void {
        this.values = [];
        this.keys = [];
        this.types = [];
        this.busy = false;
    }
}

/** @brief Allocate an isolated, unstarted codec; callers must await start before using native operations. */
export function createCodec(environment: CodecEnvironment = {}): Codec {
    return new Codec(environment);
}
