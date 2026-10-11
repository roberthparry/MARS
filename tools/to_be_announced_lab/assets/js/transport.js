
// Scoped browser capabilities. Application decisions and markup are supplied by C.
export const tbaUI = (() => {
    let values = [null], context = Object.create(null), active = false, effects = [];
    const timers = new Map(), subscriptions = new WeakMap();
    const remember = value => {
        if (value == null) return 0;
        if (values.length >= 65536) throw new Error('Browser handle limit exceeded');
        values.push(value);
        return values.length - 1;
    };
    const literal = pointer => tbaWire.text(pointer);
    const string = value => String(value ?? '');
    const reads = [
        node => node.textContent, node => node.innerHTML, (node, key) => node.getAttribute(key),
        (node, key) => node.dataset[key], (node, key) => node.style.getPropertyValue(key), node => node.value
    ];
    const writes = [
        (node, key, text) => node.textContent = text, (node, key, text) => node.innerHTML = text,
        (node, key, text) => node.setAttribute(key, text), (node, key, text) => node.dataset[key] = text,
        (node, key, text) => node.style.setProperty(key, text), (node, key, text) => node.value = text
    ];
    function call(entry, ...args) {
        if (active) throw new Error('Recursive browser dispatch');
        active = true;
        values = [null, context];
        effects = [];
        let pending;
        try {
            tbaWire.exports()[entry](...args.map(remember));
            pending = effects;
        } finally {
            values = [null];
            effects = [];
            active = false;
        }
        for (const effect of pending) effect();
    }
    function event(action, source = null) {
        call('tba_ui_event', action, source);
    }
    const define = (object, key, value) =>
        Object.defineProperty(object, key, {value, enumerable: true, writable: true, configurable: true});
    const imports = {
        tba_dom_root: () => 1,
        tba_dom_id: id => remember(document.getElementById(literal(id))),
        tba_dom_query: (node, selector) => remember((values[node] || document).querySelector(literal(selector))),
        tba_dom_all: (node, selector) => remember((values[node] || document).querySelectorAll(literal(selector))),
        tba_dom_get: (object, key) => remember(values[object]?.[literal(key)]),
        tba_dom_key_get: (object, key) => remember(values[object]?.[values[key]]),
        tba_dom_set: (object, key, value) => define(values[object], literal(key), values[value]),
        tba_dom_key_set: (object, key, value) => define(values[object], values[key], values[value]),
        tba_dom_item: (list, index) => remember(values[list]?.[index]),
        tba_dom_count: list => values[list]?.length || 0,
        tba_dom_object: array => remember(array ? [] : Object.create(null)),
        tba_dom_push: (array, value) => values[array].push(values[value]),
        tba_dom_keys: object => remember(Object.keys(values[object] || {})),
        tba_dom_string: pointer => remember(literal(pointer)),
        tba_dom_text: value => remember(string(values[value])),
        tba_dom_clean: (value, lower) => remember(lower ? string(values[value]).trim().toLowerCase() : string(values[value]).trim()),
        tba_dom_split: (text, separator) => remember(string(values[text]).split(literal(separator))),
        tba_dom_join: (array, separator) => remember(values[array].join(literal(separator))),
        tba_dom_equal: (left, right) => values[left] === values[right],
        tba_dom_truth: value => !!values[value],
        tba_dom_type: value => values[value] == null ? 0 : typeof values[value] === 'boolean' ? 1 :
            typeof values[value] === 'number' ? 2 : typeof values[value] === 'string' ? 3 : Array.isArray(values[value]) ? 4 : 5,
        tba_dom_number: value => Number(values[value]),
        tba_dom_numeric: value => remember(value),
        tba_dom_format: (value, digits) => remember(digits <= -2 ?
            value.toLocaleString(undefined, {maximumFractionDigits: -digits - 2}) :
            digits < 0 ? String(value) : value.toFixed(digits)),
        tba_dom_escape: text => remember(string(values[text]).replace(/[&<>"']/g, ch =>
            ({'&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;'})[ch])),
        tba_dom_read: (node, kind, key) => remember(values[node] ? reads[kind](values[node], literal(key)) ?? '' : ''),
        tba_dom_write: (node, kind, key, value) => { if (values[node]) writes[kind](values[node], literal(key), string(values[value])); },
        tba_dom_flag: (node, key, flag) => { if (values[node]) values[node][literal(key)] = !!flag; },
        tba_dom_class: (node, name, enabled) => values[node]?.classList.toggle(literal(name), !!enabled),
        tba_dom_has_class: (node, name) => values[node]?.classList.contains(literal(name)) || false,
        tba_dom_closest: (node, selector) => remember(values[node]?.closest?.(literal(selector))),
        tba_dom_prevent: event => values[event]?.preventDefault(),
        tba_dom_listen: (node, type, action) => {
            const target = values[node], name = literal(type);
            if (!target) return;
            let keys = subscriptions.get(target);
            if (!keys) subscriptions.set(target, keys = new Set());
            const key = name + ':' + action;
            if (keys.has(key)) return;
            keys.add(key);
            target.addEventListener(name, source => event(action, source));
        },
        tba_dom_timer: (action, delay) => effects.push(() => {
            clearTimeout(timers.get(action));
            timers.delete(action);
            if (delay >= 0) timers.set(action, setTimeout(() => { timers.delete(action); event(action); }, delay));
        }),
        tba_dom_request: (url, kind, body, headers, action, serial) => {
            const address = values[url], payload = values[body], requestHeaders = values[headers];
            effects.push(async () => {
                try {
                    let content = payload;
                    if (kind === 2) {
                        content = new FormData();
                        content.append('file', payload);
                    }
                    const response = await tbaFetch(address, {method: kind ? 'POST' : 'GET',
                        ...(kind ? {body: content} : {}), headers: requestHeaders || {}, cache: 'no-store'});
                    const data = await response.tbaData();
                    call('tba_ui_response', action, serial, data, response.ok);
                } catch (error) {
                    call('tba_ui_response', action, serial, {error: error.message || String(error)}, false);
                }
            });
        },
        tba_dom_download: (filename, text, mime) => {
            const name = values[filename], content = values[text], type = literal(mime);
            effects.push(() => {
                const url = URL.createObjectURL(new Blob([content], {type}));
                const link = document.createElement('a');
                link.href = url; link.download = name;
                document.body.append(link); link.click(); link.remove();
                setTimeout(() => URL.revokeObjectURL(url), 0);
            });
        },
        tba_dom_clipboard: (text, action) => {
            const content = values[text];
            effects.push(async () => {
                try {
                    await navigator.clipboard.writeText(content);
                    call('tba_ui_response', action, 0, {}, true);
                } catch (error) {
                    call('tba_ui_response', action, 0, {error: error.message || String(error)}, false);
                }
            });
        },
        tba_dom_measure: (node, kind) => {
            const rect = values[node]?.getBoundingClientRect();
            return rect ? [rect.left, rect.top, rect.width, rect.height][kind] : 0;
        },
        tba_dom_cursor: (svg, event, axis) => {
            const node = values[svg], source = values[event], matrix = node?.getScreenCTM();
            if (!matrix || !source) return NaN;
            const point = node.createSVGPoint();
            point.x = source.clientX; point.y = source.clientY;
            const cursor = point.matrixTransform(matrix.inverse());
            return axis ? cursor.y : cursor.x;
        },
        tba_dom_date: (value, mode) => {
            const input = values[value];
            if (!mode) {
                const date = new Date(string(input) + 'T00:00:00');
                return remember(Number.isNaN(date.getTime()) ? 0 : date.getFullYear() * 10000 + (date.getMonth() + 1) * 100 + date.getDate());
            }
            const date = new Date(mode === 3 ? Number(input) : 0);
            if (mode !== 3) date.setUTCFullYear(Math.trunc(input / 10000), Math.trunc(input / 100) % 100 - 1, input % 100);
            if (Number.isNaN(date.getTime())) return remember('');
            const y = String(date.getUTCFullYear()).padStart(4, '0'), m = String(date.getUTCMonth() + 1).padStart(2, '0'), d = String(date.getUTCDate()).padStart(2, '0');
            return remember(mode === 1 ? y + '-' + m + '-' + d : d + '/' + m + '/' + y);
        },
        tba_dom_mark: () => values.length,
        tba_dom_release: mark => {
            if (mark < 2 || mark > values.length) throw new Error('Invalid handle scope');
            values.length = mark;
        }
    };
    return {imports, call};
})();

// Browser APIs and host-value handles only; the shared C codec owns Protobuf bytes.
// Wire schema: tools/mars_lab/proto/lab.proto (version 1, 4 MiB, depth 32).
export const tbaWire = (() => {
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
            ...tbaUI.imports,
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
        async start(basePath) {
            if (wasm)
                return;
            const response = await fetch(`${basePath}/wasm/tba_browser.wasm`, {cache: 'no-store'});
            if (!response.ok)
                throw new Error(`Could not load the C browser module (${response.status})`);
            const result = await WebAssembly.instantiate(await response.arrayBuffer(), imports);
            wasm = result.instance.exports;
            if (typeof wasm.lab_browser_abi_version !== 'function' || wasm.lab_browser_abi_version() !== 30 ||
                typeof wasm.tba_browser_abi_version !== 'function' || wasm.tba_browser_abi_version() !== 2)
                throw new Error('The Lab browser module is out of date; rebuild and restart the forecasting Lab');
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
            try {
                const backing = ArrayBuffer.isView(buffer) ? buffer.buffer : buffer;
                // The intrinsic validates the backing store across realms and rejects SharedArrayBuffer.
                try {
                    bufferLength.call(backing);
                } catch (_) {
                    throw new Error('Lab codec requires an ArrayBuffer or typed view');
                }
                const bytes = ArrayBuffer.isView(buffer) ?
                    new Uint8Array(backing, buffer.byteOffset, buffer.byteLength) :
                    new Uint8Array(backing);
                if (bytes.length > limit)
                    throw new Error('Lab Protobuf response exceeds 4 MiB');
                new Uint8Array(wasm.memory.buffer, wasm.lab_browser_input_buffer(), bytes.length).set(bytes);
                const id = wasm.lab_browser_decode(bytes.length);
                if (id < 0)
                    throw new Error('Malformed or incompatible Lab Protobuf response');
                return values[id];
            } finally {
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
        }
    };
})();

/** Fetch preserves HTTP status, abort signals and credentials; C owns binary messages. */
export async function tbaFetch(url, options = {}) {
    const headers = new Headers(options.headers);
    headers.set('Accept', 'application/x-protobuf');
    if (options.body !== undefined && !(options.body instanceof FormData)) {
        headers.set('Content-Type', 'application/x-protobuf');
        options = {...options, body: tbaWire.encode(options.body)};
    }
    const response = await fetch(url, {...options, headers});
    const binary = (response.headers.get('Content-Type') || '').split(';')[0].trim() === 'application/x-protobuf';
    Object.defineProperty(response, 'tbaData', {
        value: async () => {
            if (!binary) {
                await response.body?.cancel();
                throw new Error('Expected a Protobuf Lab response');
            }
            const reader = response.body.getReader();
            const parts = [];
            let length = 0;
            try {
                for (;;) {
                    const {done, value} = await reader.read();
                    if (done)
                        break;
                    length += value.length;
                    if (length > 4194304) {
                        await reader.cancel();
                        throw new Error('Lab Protobuf response exceeds 4 MiB');
                    }
                    parts.push(value);
                }
            } finally {
                reader.releaseLock();
            }
            const bytes = new Uint8Array(length);
            let offset = 0;
            for (const part of parts) {
                bytes.set(part, offset);
                offset += part.length;
            }
            return tbaWire.decode(bytes.buffer);
        }
    });
    return response;
}


// Bootstrap only: the native application owns all controls and presentation.
const bootstrap = document.getElementById('tba-config');
if (bootstrap) {
    try {
        await tbaWire.start(bootstrap.dataset.base);
        const config = tbaWire.decode(Uint8Array.from(atob(bootstrap.textContent.trim()), c => c.charCodeAt(0)));
        tbaUI.call('tba_ui_init', config);
    } catch (error) {
        const status = document.getElementById('status');
        status.textContent = 'Startup failed: ' + (error.message || String(error));
        status.className = 'status error';
        document.getElementById('run-button').disabled = true;
    }
}
