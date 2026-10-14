/** Instance-local host-value bridge; all Protobuf parsing remains in the bounded C codec. */
export function createCodec(environment = {}) {
    const limit = 4194304;
    const encoder = new TextEncoder();
    const decoder = new TextDecoder('utf-8', {fatal: true, ignoreBOM: true});
    const bufferLength = Object.getOwnPropertyDescriptor(ArrayBuffer.prototype, 'byteLength').get;
    const wellFormed = String.prototype.isWellFormed;
    let wasm;
    let values = [];
    let keys = [];
    let types = [];
    let busy = false;
    function remember(value, type, names = null) {
        if (values.length > 131072)
            throw new Error('Lab message has too many values');
        values.push(value);
        keys.push(names);
        types.push(type);
        return values.length - 1;
    }
    function add(value) {
        const type = value === null    ? 0 :
            typeof value === 'boolean' ? 1 :
            typeof value === 'number'  ? 2 :
            typeof value === 'string'  ? 3 :
            Array.isArray(value)       ? 4 :
            typeof value === 'object' && value &&
                (Object.getPrototypeOf(value) === Object.prototype || Object.getPrototypeOf(value) === null) ?
                                   5 :
                                   -1;
        let names = null;
        if (type >= 4) {
            const descriptors = Object.getOwnPropertyDescriptors(value);
            const length = type === 4 ? descriptors.length.value : 0;
            names = type === 4 ? null : Object.keys(descriptors).filter(key => descriptors[key].enumerable);
            if ((type === 4 ? length : names.length) > 65536)
                throw new Error('Lab container has too many members');
            const copy = type === 4 ? [] : Object.create(null);
            const members = type === 4 ? Array.from({length}, (_, i) => String(i)) : names;
            for (const name of members) {
                const descriptor = descriptors[name];
                if (!descriptor || !Object.hasOwn(descriptor, 'value'))
                    throw new Error('Lab messages require data properties, not getters or sparse arrays');
                Object.defineProperty(
                    copy, name, {value: descriptor.value, enumerable: true, writable: true, configurable: true});
            }
            value = copy;
        }
        return remember(value, type, names);
    }
    function writeText(value, pointer, capacity) {
        if (typeof value !== 'string' || value.length > capacity || (wellFormed && !wellFormed.call(value)))
            return -1;
        const target = new Uint8Array(wasm.memory.buffer, pointer, capacity);
        const {read, written} = encoder.encodeInto(value, target);
        if (read !== value.length || (!wellFormed && decoder.decode(target.subarray(0, written)) !== value))
            return -1;
        return written;
    }
    const imports = {
        env: {
            ...environment,
            lab_host_kind: id => types[id],
            lab_host_number: id => Number(values[id]),
            lab_host_count: id => {
                if (Array.isArray(values[id]))
                    return values[id].length;
                return keys[id].length;
            },
            lab_host_child_info: (id, index) => {
                const child = add(values[id][Array.isArray(values[id]) ? index : keys[id][index]]);
                return types[child] < 0 ? -1 : (child << 3) | types[child];
            },
            lab_host_key_text: (id, index, pointer, capacity) => writeText(keys[id][index], pointer, capacity),
            lab_host_text: (id, pointer, capacity) => writeText(values[id], pointer, capacity),
            lab_host_value: (type, number, pointer, length, parent, keyPointer, keyLength) => {
                let value;
                if (type === 0)
                    value = null;
                else if (type === 1)
                    value = Boolean(number);
                else if (type === 2)
                    value = number;
                else if (type === 3)
                    value = decoder.decode(new Uint8Array(wasm.memory.buffer, pointer, length));
                else if (type === 4)
                    value = [];
                else if (type === 5)
                    value = Object.create(null);
                else
                    return -1;
                if (parent >= 0) {
                    const owner = values[parent];
                    if (Array.isArray(owner))
                        owner.push(value);
                    else {
                        const key = decoder.decode(new Uint8Array(wasm.memory.buffer, keyPointer, keyLength));
                        if (Object.hasOwn(owner, key))
                            return -1;
                        Object.defineProperty(
                            owner, key, {value, enumerable: true, writable: true, configurable: true});
                    }
                }
                // Decoded values are already validated by C; do not snapshot fresh containers again.
                return remember(value, type);
            }
        }
    };
    function reset() {
        values = [];
        keys = [];
        types = [];
    }
    function enter() {
        if (busy)
            throw new Error('Lab codec cannot be called recursively');
        busy = true;
        reset();
    }
    return {
        exports() {
            if (!wasm)
                throw new Error('C browser module is not ready');
            return wasm;
        },
        async start(bytes) {
            const result = await WebAssembly.instantiate(bytes, imports);
            wasm = (result.instance || result).exports;
            if (typeof wasm.lab_browser_abi_version !== 'function' || wasm.lab_browser_abi_version() !== 32)
                throw new Error('The Lab codec ABI is incompatible; rebuild and restart');
        },
        bytes(buffer) {
            const backing = ArrayBuffer.isView(buffer) ? buffer.buffer : buffer;
            try {
                bufferLength.call(backing);
            } catch (_) {
                throw new Error('Lab codec requires an ArrayBuffer or typed view');
            }
            const bytes = ArrayBuffer.isView(buffer) ?
                new Uint8Array(backing, buffer.byteOffset, buffer.byteLength) : new Uint8Array(backing);
            if (bytes.length > limit)
                throw new Error('Lab Protobuf response exceeds 4 MiB');
            return bytes;
        },
        encode(value) {
            if (!wasm)
                throw new Error('C browser module is not ready');
            enter();
            try {
                const size = wasm.lab_browser_encode(add(value));
                if (!size)
                    throw new Error('Invalid or oversized Lab Protobuf message');
                return new Uint8Array(wasm.memory.buffer, wasm.lab_browser_output_buffer(), size).slice();
            } finally {
                reset();
                busy = false;
            }
        },
        decode(buffer) {
            if (!wasm)
                throw new Error('C browser module is not ready');
            enter();
            let length = 0;
            try {
                const bytes = this.bytes(buffer);
                length = bytes.length;
                new Uint8Array(wasm.memory.buffer, wasm.lab_browser_input_buffer(), bytes.length).set(bytes);
                const id = wasm.lab_browser_decode(bytes.length);
                if (id < 0)
                    throw new Error('Malformed or incompatible Lab Protobuf response');
                return values[id];
            } finally {
                new Uint8Array(wasm.memory.buffer, wasm.lab_browser_input_buffer(), length).fill(0);
                reset();
                busy = false;
            }
        },
        text(pointer) {
            const bytes = new Uint8Array(wasm.memory.buffer);
            const end = bytes.indexOf(0, pointer);
            if (end < 0)
                throw new Error('Invalid C text');
            return decoder.decode(bytes.subarray(pointer, end));
        },
        precision: (value, fallback) => wasm.lab_browser_precision(value, fallback),
        intervals: (value, fallback) => wasm.lab_browser_intervals(value, fallback)
    };
}
