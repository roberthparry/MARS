
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
        tba_dom_length: value => string(values[value]).length,
        tba_dom_char: (value, index) => string(values[value]).charCodeAt(index) || 0,
        tba_dom_slice: (value, begin, end) => remember(string(values[value]).slice(begin, end)),
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
                    if (kind === 2) {
                        if (!(payload instanceof File) || payload.size > 16777216)
                            throw new Error('Local file exceeds 16 MiB');
                        const data = {ok: true, text: await payload.text(), name: payload.name};
                        call('tba_ui_response', action, serial, data, true);
                    } else if (kind === 3) {
                        const data = await tbaEncrypted(address, payload);
                        call('tba_ui_response', action, serial, data, true);
                    } else {
                        const response = await tbaFetch(address, {method: kind ? 'POST' : 'GET',
                            ...(kind ? {body: payload} : {}), headers: requestHeaders || {}, cache: 'no-store'});
                        call('tba_ui_response', action, serial, await response.tbaData(), response.ok);
                    }
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
    let codec, jobs;
    return {
        async start(basePath) {
            if (codec) return;
            const [{createCodec}, {createJobs}] = await Promise.all([
                import('./codec.js'), import('./jobs.js')
            ]);
            const response = await fetch(`${basePath}/wasm/tba_browser.wasm`, {cache: 'no-store'});
            if (!response.ok)
                throw new Error('Could not load the C browser module');
            const instance = createCodec(tbaUI.imports);
            await instance.start(await response.arrayBuffer());
            if (instance.exports().tba_browser_abi_version() !== 3)
                throw new Error('The Lab browser ABI is incompatible; rebuild and restart');
            codec = instance;
            jobs = createJobs(codec.exports(), codec,
                new URL(`${basePath}/js/codec_worker.js`, location.href).href,
                new URL(`${basePath}/wasm/lab_codec.wasm`, location.href).href);
            addEventListener('pagehide', event => { if (!event.persisted) jobs.close(); });
        },
        exports: () => codec.exports(),
        encode: value => codec.encode(value),
        decode: buffer => codec.decode(buffer),
        decodeAsync: (buffer, signal) => jobs.decode(buffer, signal),
        background: () => jobs,
        text: pointer => codec.text(pointer),
        precision: (value, fallback) => codec.precision(value, fallback),
        intervals: (value, fallback) => codec.intervals(value, fallback)
    };
})();

/** Fetch preserves HTTP status, abort signals and credentials; C owns binary messages. */
export async function tbaFetch(url, options = {}) {
    const headers = new Headers(options.headers);
    headers.set('Accept', 'application/x-protobuf');
    if (options.body !== undefined) {
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
            return tbaWire.decodeAsync(bytes.buffer, options.signal);
        }
    });
    return response;
}


// Web Crypto and fetch are browser capabilities; C owns message values and application policy.
export async function tbaEncrypted(address, payload, options = {}) {
    if (!globalThis.isSecureContext || !crypto.subtle)
        throw new Error('Use trusted HTTPS or local loopback; plaintext transfer is disabled');
    const url = new URL(address, location.href);
    const local = ['localhost', '127.0.0.1', '[::1]'].includes(url.hostname);
    if (url.origin !== location.origin || (url.protocol !== 'https:' && !local))
        throw new Error('Encrypted streams require a trusted same-origin connection');
    const encode64 = bytes => {
        let text = '';
        for (let offset = 0; offset < bytes.length; offset += 8192)
            text += String.fromCharCode(...bytes.subarray(offset, offset + 8192));
        return btoa(text);
    };
    const decode64 = text => {
        if (typeof text !== 'string' || text.length > 2796228)
            throw new Error('Invalid encrypted response');
        return Uint8Array.from(atob(text), ch => ch.charCodeAt(0));
    };
    const bytes = tbaWire.encode(payload);
    if (!bytes.length || bytes.length > 2097152) {
        bytes.fill(0);
        throw new Error('Selected data exceeds the 2 MiB message limit');
    }
    const controller = new AbortController();
    const abort = () => controller.abort();
    options.signal?.addEventListener('abort', abort, {once: true});
    if (options.signal?.aborted) abort();
    const timeout = setTimeout(abort, 90000);
    const rawKey = crypto.getRandomValues(new Uint8Array(32));
    let session, finished = false;
    const request = async (path, body, signal = controller.signal) => {
        const response = await tbaFetch(url.href + path, {method: 'POST', body, signal, cache: 'no-store',
            redirect: 'error', credentials: 'same-origin'});
        const data = await response.tbaData();
        if (!response.ok || !data.ok)
            throw new Error(data.error || 'Encrypted request rejected');
        return data;
    };
    try {
        session = await request('/begin', {});
        if (!Number.isInteger(session.slot) || session.slot < 0 || session.slot > 3 ||
            !/^[0-9a-f]{64}$/.test(session.token) || session.chunk_limit !== 32768 ||
            session.message_limit !== 2097152)
            throw new Error('Incompatible encrypted stream');
        const publicKey = await crypto.subtle.importKey('spki', decode64(session.public_key),
            {name: 'RSA-OAEP', hash: 'SHA-256'}, false, ['encrypt']);
        const wrappedKey = encode64(new Uint8Array(await crypto.subtle.encrypt({name: 'RSA-OAEP'}, publicKey, rawKey)));
        const key = await crypto.subtle.importKey('raw', rawKey, {name: 'AES-GCM'}, false, ['encrypt', 'decrypt']);
        rawKey.fill(0);
        let result;
        for (let offset = 0, sequence = 0; offset < bytes.length; offset += 32768, ++sequence) {
            const final = offset + 32768 >= bytes.length;
            const iv = new Uint8Array(12);
            new DataView(iv.buffer).setUint32(8, sequence);
            const additionalData = new TextEncoder().encode('MARS-TBA-1:' + session.token + ':0:' + sequence + ':' + Number(final));
            const ciphertext = new Uint8Array(await crypto.subtle.encrypt(
                {name: 'AES-GCM', iv, additionalData, tagLength: 128}, key, bytes.subarray(offset, offset + 32768)));
            result = await request('/chunk', {slot: session.slot, token: session.token, wrapped_key: wrappedKey,
                sequence, final, ciphertext: encode64(ciphertext)});
            if (!final && result.ciphertext !== undefined)
                throw new Error('Premature stream completion');
        }
        const cipher = decode64(result.ciphertext);
        if (cipher.length < 16 || cipher.length > 2097168)
            throw new Error('Invalid encrypted result size');
        const iv = new Uint8Array(12);
        iv[0] = 1;
        const additionalData = new TextEncoder().encode('MARS-TBA-1:' + session.token + ':1:0:1');
        const plain = new Uint8Array(await crypto.subtle.decrypt({name: 'AES-GCM', iv, additionalData, tagLength: 128},
            key, cipher));
        try {
            const result = await tbaWire.decodeAsync(plain, controller.signal);
            finished = true;
            return result;
        } finally {
            plain.fill(0);
        }
    } finally {
        bytes.fill(0);
        rawKey.fill(0);
        clearTimeout(timeout);
        options.signal?.removeEventListener('abort', abort);
        if (session && !finished) {
            try {
                await request('/chunk', {slot: session.slot, token: session.token, sequence: 0, cancel: true},
                    AbortSignal.timeout(2000));
            } catch (_) {
                // The native service also expires abandoned ciphertext streams.
            }
        }
    }
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
