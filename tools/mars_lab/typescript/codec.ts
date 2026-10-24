/**
 * @file codec.ts
 * @brief Instance-local browser bridge for the native binary codec.
 *
 * Owns a WebAssembly codec instance and transient host-value handles. Binary parsing
 * remains in C; this ES module supplies bounded browser capabilities to the codec.
 * Container snapshots inspect data descriptors without invoking getters. Named host
 * callbacks retain their owning instance, and each operation releases its handle scope.
 */
/** Instance-local host-value bridge; all Protobuf parsing remains in the bounded C codec. */
import {isRecord} from './codec_types.js';
import type {CodecBuffer, CodecExports, JsonObject, JsonValue, UnknownRecord} from './codec_types.js';
export type {CodecBuffer, CodecExports, JsonValue} from './codec_types.js';

/** @brief Additional native imports supplied alongside the codec's own host capabilities. */
export type CodecEnvironment = WebAssembly.ModuleImports;

/** Own-property descriptors used to copy container members without evaluating getters. */
interface CodecDescriptors {
    [key: string]: PropertyDescriptor;
}

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
        this.encodeStored = this.encodeStored.bind(this);
        this.decodeStored = this.decodeStored.bind(this);
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
            const imports = {env: this.hostImports()};
            const instance = bytes instanceof WebAssembly.Module ? await WebAssembly.instantiate(bytes, imports) :
                (await WebAssembly.instantiate(bytes, imports)).instance;
            const exports = instance.exports;
            if (!this.validExports(exports))
                throw new Error('The Lab codec ABI is incompatible; rebuild and restart');
            this.wasm = exports;
        } finally {
            this.starting = false;
        }
    }

    private validExports(exports: WebAssembly.Exports): exports is CodecExports {
        const functions = ['lab_browser_abi_version', 'lab_browser_handle_capacity', 'lab_browser_member_limit',
            'lab_browser_message_limit', 'lab_browser_encode', 'lab_browser_decode', 'lab_browser_input_buffer',
            'lab_browser_output_buffer', 'lab_browser_precision', 'lab_browser_intervals', 'lab_browser_base64_limit',
            'lab_browser_storage_encode', 'lab_browser_storage_decode',
            'lab_jobs_worker_step', 'lab_jobs_worker_accepts', 'lab_jobs_finish', 'lab_jobs_take', 'lab_jobs_submit',
            'lab_jobs_threshold', 'lab_jobs_capacity', 'lab_jobs_timeout', 'lab_jobs_count', 'lab_jobs_bytes'] as const;
        if (!(exports.memory instanceof WebAssembly.Memory)) return false;
        for (const name of functions) {
            if (typeof exports[name] !== 'function') return false;
        }
        return exports.lab_browser_abi_version instanceof Function && exports.lab_browser_abi_version() === 47;
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
            // Native decoding validates the tree; host callbacks only construct JSON data.
            if (id < 0 || this.types[id] !== 5 || !isRecord(value))
                throw new Error('Malformed or incompatible Lab Protobuf response');
            return value as JsonObject;
        } finally {
            new Uint8Array(wasm.memory.buffer, wasm.lab_browser_input_buffer(), length).fill(0);
            this.leave();
        }
    }

    /** @brief Encode a saved record entirely in native C, returning the existing padded base64 format. */
    encodeStored(value: unknown): string {
        const wasm = this.exports();
        this.enter();
        let length = 0;
        try {
            length = wasm.lab_browser_storage_encode(this.add(value));
            if (!length) throw new Error('Invalid or oversized Lab storage record');
            return this.decoder.decode(new Uint8Array(wasm.memory.buffer, wasm.lab_browser_input_buffer(), length));
        } finally {
            new Uint8Array(wasm.memory.buffer, wasm.lab_browser_input_buffer(), length).fill(0);
            this.leave();
        }
    }

    /** @brief Stage bounded ASCII storage text; C validates base64 and Protobuf before returning the record. */
    decodeStored(text: string): JsonValue {
        const wasm = this.exports();
        if (!text.length || text.length > wasm.lab_browser_base64_limit())
            throw new Error('Invalid or oversized Lab storage record');
        this.enter();
        const bytes = new Uint8Array(wasm.memory.buffer, wasm.lab_browser_input_buffer(), text.length);
        try {
            const staged = this.encoder.encodeInto(text, bytes);
            if (staged.read !== text.length || staged.written !== text.length)
                throw new Error('Lab storage records require ASCII text');
            const id = wasm.lab_browser_storage_decode(text.length);
            const value = this.values[id];
            if (id < 0 || this.types[id] !== 5 || !isRecord(value))
                throw new Error('Malformed or incompatible Lab storage record');
            return value as JsonObject;
        } finally {
            bytes.fill(0);
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

    private classify(value: unknown): number {
        if (value instanceof Uint8Array && value.buffer instanceof ArrayBuffer) return 6;
        if (value === null) return 0;
        if (typeof value === 'boolean') return 1;
        if (typeof value === 'number') return 2;
        if (typeof value === 'string') return 3;
        if (Array.isArray(value)) return 4;
        if (isRecord(value) && (Object.getPrototypeOf(value) === Object.prototype ||
            Object.getPrototypeOf(value) === null)) return 5;
        return -1;
    }

    private add(value: unknown): number {
        const type = this.classify(value);
        if (Array.isArray(value)) return this.snapshotArray(value);
        if (type === 5 && isRecord(value)) return this.snapshotObject(value);
        return this.remember(value, type);
    }

    private checkMemberCount(count: number): void {
        if (count > this.exports().lab_browser_member_limit())
            throw new Error('Lab container has too many members');
    }

    private copyMember(copy: unknown[] | UnknownRecord, name: string, descriptor: PropertyDescriptor | undefined): void {
        if (!descriptor || !Object.hasOwn(descriptor, 'value'))
            throw new Error('Lab messages require data properties, not getters or sparse arrays');
        Object.defineProperty(copy, name,
            {value: descriptor.value, enumerable: true, writable: true, configurable: true});
    }

    private snapshotArray(value: unknown[]): number {
        const descriptors: CodecDescriptors = Object.getOwnPropertyDescriptors(value);
        const length = value.length;
        this.checkMemberCount(length);
        const copy: unknown[] = [];
        for (let index = 0; index < length; ++index) {
            const name = String(index);
            this.copyMember(copy, name, descriptors[name]);
        }
        return this.remember(copy, 4);
    }

    private snapshotObject(value: UnknownRecord): number {
        const descriptors: CodecDescriptors = Object.getOwnPropertyDescriptors(value);
        const names: string[] = [];
        for (const name of Object.keys(descriptors)) {
            if (descriptors[name]?.enumerable) names.push(name);
        }
        this.checkMemberCount(names.length);
        const copy: UnknownRecord = Object.create(null);
        for (const name of names) {
            this.copyMember(copy, name, descriptors[name]);
        }
        return this.remember(copy, 5, names);
    }

    private writeText(value: unknown, pointer: number, capacity: number): number {
        if (typeof value !== 'string' || value.length > capacity)
            return -1;
        const target = new Uint8Array(this.exports().memory.buffer, pointer, capacity);
        const {read, written} = this.encoder.encodeInto(value, target);
        if (read !== value.length || this.decoder.decode(target.subarray(0, written)) !== value)
            return -1;
        return written;
    }

    private hostImports(): WebAssembly.ModuleImports {
        return {
            ...this.environment,
            lab_host_kind: this.hostKind.bind(this),
            lab_host_number: this.hostNumber.bind(this),
            lab_host_count: this.hostCount.bind(this),
            lab_host_child_info: this.hostChildInfo.bind(this),
            lab_host_key_text: this.hostKeyText.bind(this),
            lab_host_text: this.hostText.bind(this),
            lab_host_value: this.receiveValue.bind(this)
        };
    }

    private hostKind(id: number): number {
        return this.types[id] ?? -1;
    }

    private hostNumber(id: number): number {
        return Number(this.values[id]);
    }

    private hostCount(id: number): number {
        const value = this.values[id];
        if (Array.isArray(value)) return value.length;
        return this.keys[id]?.length ?? 0;
    }

    private hostChildInfo(id: number, index: number): number {
        const value = this.values[id];
        const key = this.keys[id]?.[index];
        let member: unknown;
        if (Array.isArray(value)) member = value[index];
        else if (isRecord(value) && key !== undefined) member = value[key];
        const child = this.add(member);
        const type = this.types[child] ?? -1;
        return type < 0 ? -1 : (child << 3) | type;
    }

    private hostKeyText(id: number, index: number, pointer: number, capacity: number): number {
        return this.writeText(this.keys[id]?.[index], pointer, capacity);
    }

    private hostText(id: number, pointer: number, capacity: number): number {
        const bytes = this.values[id];
        if (bytes instanceof Uint8Array && bytes.buffer instanceof ArrayBuffer) {
            if (bytes.byteLength > capacity) return -1;
            new Uint8Array(this.exports().memory.buffer, pointer, bytes.byteLength).set(bytes);
            return bytes.byteLength;
        }
        return this.writeText(this.values[id], pointer, capacity);
    }

    private receiveValue(type: number, number: number, pointer: number, length: number,
        parent: number, keyPointer: number, keyLength: number): number {
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
        else if (type === 6)
            value = new Uint8Array(this.exports().memory.buffer, pointer, length).slice();
        else
            return -1;
        if (parent >= 0) {
            const owner = this.values[parent];
            if (Array.isArray(owner))
                owner.push(value);
            else if (isRecord(owner)) {
                const key = this.decoder.decode(new Uint8Array(this.exports().memory.buffer, keyPointer, keyLength));
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
