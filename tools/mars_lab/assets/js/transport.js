/** Scoped DOM capabilities for C/WASM presentation. No DOM handles survive a synchronous call. */
const labDOM = (() => {
    let values = [null], active = false;
    let effects = null, returned, hasReturn = false;
    let eventServices = null;
    const subscriptions = new WeakMap();
    const decoder = new TextDecoder('utf-8', {fatal: true});
    const textDecoder = new TextDecoder('utf-8', {fatal: true, ignoreBOM: true});
    const textEncoder = new TextEncoder();
    const remember = value => {
        if (value == null)
            return 0;
        if (values.length >= 4096)
            throw new Error('Lab DOM call exceeds its handle limit');
        values.push(value);
        return values.length - 1;
    };
    const literal = pointer => {
        const bytes = new Uint8Array(labWire.exports().memory.buffer);
        let end = pointer;
        while (end < bytes.length && end - pointer < 4096 && bytes[end]) ++end;
        if (end === bytes.length || end - pointer === 4096)
            throw new Error('Invalid Lab DOM label');
        return decoder.decode(bytes.subarray(pointer, end));
    };
    const reads = [
        node => node.textContent, node => node.innerHTML, (node, key) => node.getAttribute(key),
        (node, key) => node.dataset[key], (node, key) => node.style.getPropertyValue(key), node => node.value
    ];
    const writes = [
        (node, key, text) => node.textContent = text, (node, key, text) => node.innerHTML = text,
        (node, key, text) => node.setAttribute(key, text), (node, key, text) => node.dataset[key] = text,
        (node, key, text) => node.style.setProperty(key, text), (node, key, text) => node.value = text
    ];
    function svgLength(value) {
        const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
        svg.setAttribute('width', String(value || ''));
        const length = svg.width.baseVal;
        return length.unitType === 1 || (length.unitType >= 5 && length.unitType <= 10) ? length.value : 0;
    }
    const measures = [
        node => node.clientWidth, node => svgLength(node.getAttribute('width')),
        node => svgLength(node.getAttribute('height')), node => node.getBoundingClientRect().width,
        node => node.getBoundingClientRect().height, node => node.viewBox?.baseVal.width || 0,
        node => node.viewBox?.baseVal.height || 0, node => node.getBoundingClientRect().left,
        node => node.getBoundingClientRect().top, node => node.getBoundingClientRect().bottom,
        node => node.clientHeight, node => node.scrollHeight,
        node => Number(node.isConnected && node.clientHeight > 0 && node.getClientRects().length > 0)
    ];
    function services(plan, registry) {
        if (active)
            throw new Error('Native services cannot run inside a DOM scope');
        let result;
        for (const call of plan.calls || []) {
            if (!registry || !Object.hasOwn(registry, call.service))
                throw new Error('Unknown native service');
            result = registry[call.service](...call.args);
        }
        return result;
    }
    function sync(entry, kind, frame, registry) {
        for (;;) {
            try {
                const plan = labDOM.call(entry, kind, frame);
                if (plan.done)
                    return plan.value;
                const result = services(plan, registry);
                if (plan.save)
                    frame[plan.save] = result;
            } catch (error) {
                if (!Object.hasOwn(frame, 'failed'))
                    throw error;
                frame.stage = frame.failed;
                delete frame.failed;
                frame.exception = error;
            }
        }
    }
    async function flow(kind, frame, view, registry) {
        for (;;) {
            try {
                const plan = labDOM.call('lab_flow_step', kind, frame, view);
                if (plan.done)
                    return plan.value;
                const pending = services(plan, registry);
                const result = plan.wait ? await pending : pending;
                if (plan.save)
                    frame[plan.save] = result;
            } catch (error) {
                if (!Object.hasOwn(frame, 'failed'))
                    throw error;
                frame.stage = frame.failed;
                delete frame.failed;
                frame.exception = error;
                let converted = false, text;
                Object.defineProperty(frame, 'error', {
                    configurable: true,
                    enumerable: true,
                    get() {
                        if (!converted) {
                            text = String(error);
                            converted = true;
                        }
                        return text;
                    }
                });
            }
        }
    }
    function subscribe(target, name, entry, action, context, passive) {
        if (!target)
            return;
        const key = name + ':' + entry + ':' + action;
        let installed = subscriptions.get(target);
        if (!installed)
            subscriptions.set(target, installed = new Map());
        let contexts = installed.get(key);
        if (!contexts)
            installed.set(key, contexts = new Set());
        if (contexts.has(context))
            return;
        contexts.add(context);
        target.addEventListener(name, event => {
            const plan = labDOM.call(entry, action, event, context);
            if (plan.prevent)
                event.preventDefault();
            if (plan.stop)
                event.stopPropagation();
            services(plan, eventServices);
        }, {passive: !!passive});
    }
    return {
        sync,
        flow,
        services,
        bindEvents(services) {
            const previous = eventServices;
            eventServices = services;
            return previous;
        },
        call(name, ...args) {
            if (active)
                throw new Error('Lab DOM calls cannot be nested');
            active = true;
            returned = undefined;
            hasReturn = false;
            const pending = [];
            effects = pending;
            let result;
            try {
                result =
                    labWire.exports()[name](...args.map(value => typeof value === 'number' ? value : remember(value)));
                if (hasReturn)
                    result = returned;
            } finally {
                values = [null];
                active = false;
                effects = null;
                returned = undefined;
                hasReturn = false;
            }
            for (const effect of pending) effect();
            return result;
        },
        svgLength,
        env: {
            lab_dom_get: (object, key) => remember(values[object]?.[literal(key)]),
            lab_dom_key_get: (object, key) => remember(
                values[object] && Object.hasOwn(values[object], values[key]) ? values[object][values[key]] : null),
            lab_dom_type: id => values[id] == null ? 0 :
                typeof values[id] === 'boolean'    ? 1 :
                typeof values[id] === 'number'     ? 2 :
                typeof values[id] === 'string'     ? 3 :
                Array.isArray(values[id])          ? 4 :
                                                     5,
            lab_dom_truth: id => !!values[id],
            lab_dom_index_present: (array, index) => Array.isArray(values[array]) && index in values[array],
            lab_dom_object: kind => remember(kind === 4 ? [] : {}),
            lab_dom_numeric: number => remember(number),
            lab_dom_scalar: (kind, number) => remember(
                kind === 1     ? !!number :
                    kind === 2 ? number :
                                 null),
            lab_dom_push: (array, value) => values[array].push(values[value]),
            lab_dom_text: value => remember(String(values[value])),
            lab_dom_utf8_copy: (value, target, capacity) => {
                const text = String(values[value]);
                if (text.length > capacity)
                    return -1;
                const bytes = textEncoder.encode(text);
                if (bytes.length > capacity || textDecoder.decode(bytes) !== text)
                    return -1;
                new Uint8Array(labWire.exports().memory.buffer, target, bytes.length).set(bytes);
                return bytes.length;
            },
            lab_dom_utf8_string: (source, length) =>
                remember(textDecoder.decode(new Uint8Array(labWire.exports().memory.buffer, source, length))),
            lab_dom_to_number: value => Number(values[value]),
            lab_dom_parse_int: (value, radix) => Number.parseInt(String(values[value]), radix),
            lab_dom_slice: (value, start, end) => remember(String(values[value] || '').slice(start, end)),
            lab_dom_split: (value, separator) => remember(String(values[value] || '').split(literal(separator))),
            lab_dom_starts_with: (value, prefix) => String(values[value] || '').startsWith(literal(prefix)),
            lab_dom_locale_compare: (left, right, numeric) =>
                String(values[left])
                    .localeCompare(
                        String(values[right]), undefined, numeric ? {numeric: true, sensitivity: 'base'} : undefined),
            lab_dom_member: (set, value) => values[set]?.has(values[value]) || false,
            lab_dom_delete: (object, key) => values[object] && delete values[object][literal(key)],
            lab_dom_key_delete: (object, key) => values[object] && delete values[object][values[key]],
            lab_dom_map_new: () => remember(new Map()),
            lab_dom_map_get: (map, key) => remember(values[map]?.get(values[key])),
            lab_dom_map_set: (map, key, value) => values[map].set(values[key], values[value]),
            lab_dom_map_size: map => values[map]?.size || 0,
            lab_dom_map_first: map => remember(values[map]?.keys().next().value),
            lab_dom_map_delete: (map, key) => values[map]?.delete(values[key]),
            lab_dom_keys: object => remember(Object.keys(values[object] || {})),
            lab_dom_set: (object, key, value) => Object.defineProperty(
                values[object], literal(key),
                {value: values[value], enumerable: true, writable: true, configurable: true}),
            lab_dom_key_set: (object, key, value) => Object.defineProperty(
                values[object], values[key],
                {value: values[value], enumerable: true, writable: true, configurable: true}),
            lab_dom_return: value => {
                returned = values[value];
                hasReturn = true;
            },
            lab_dom_mark: () => values.length,
            lab_dom_release: mark => {
                if (!active || mark < 1 || mark > values.length)
                    throw new Error('Invalid Lab DOM handle scope');
                values.length = mark;
            },
            lab_dom_normalize: value => remember(
                String(values[value] || '').normalize('NFD').replace(/[\u0300-\u036f]/g, '').toLowerCase().trim()),
            lab_dom_contains: (text, query) => String(values[text] || '').includes(String(values[query] || '')),
            lab_dom_selected: select => remember(values[select]?.selectedOptions[0]),
            lab_dom_active: () => remember(document.activeElement),
            lab_dom_is_disabled: node => values[node]?.matches(':disabled') || false,
            lab_dom_wrap: (node, shell) => values[node].parentNode.insertBefore(values[shell], values[node]),
            lab_dom_effect: (node, action) => {
                const target = values[node];
                if (!target)
                    return;
                const actions = [
                    () => target.focus(), () => target.scrollIntoView({block: 'nearest', inline: 'nearest'}),
                    () => target.dispatchEvent(new Event('change', {bubbles: true})), () => target.select(),
                    () => target.blur()
                ];
                if (!effects || !actions[action])
                    throw new Error('Invalid Lab DOM effect');
                effects.push(actions[action]);
            },
            lab_dom_string: pointer => remember(literal(pointer)),
            lab_dom_length: id => String(values[id] || '').length,
            lab_dom_property_copy: (target, targetKey, source, sourceKey) => Object.defineProperty(
                values[target], literal(targetKey),
                {value: values[source]?.[literal(sourceKey)], writable: true, configurable: true, enumerable: true}),
            lab_dom_properties_equal: (left, leftKey, right, rightKey) =>
                values[left]?.[literal(leftKey)] === values[right]?.[literal(rightKey)],
            lab_dom_equal: (left, right) => values[left] === values[right],
            lab_dom_compare: (left, right) => values[left] < values[right] ? -1 :
                values[left] > values[right]                               ? 1 :
                                                                             0,
            lab_dom_clean: (text, kind) => remember(
                kind === 1     ? String(values[text] || '').trim().toLowerCase() :
                    kind === 2 ? String(values[text] || '').trim().replace(/\s+/g, ' ') :
                    kind === 3 ? String(values[text] || '').trimEnd() :
                                 String(values[text] || '').trim()),
            lab_dom_has: (node, key) => values[node]?.hasAttribute(literal(key)) || false,
            lab_dom_remove: (node, key) => values[node]?.removeAttribute(literal(key)),
            lab_dom_number: id => Number(values[id]) || 0,
            lab_dom_format: (number, prefix, suffix) => remember(literal(prefix) + number + literal(suffix)),
            lab_dom_join: (left, right, suffix) =>
                remember(String(values[left] || '') + String(values[right] || '') + literal(suffix)),
            lab_dom_query: (parent, selector) =>
                remember((values[parent] || document).querySelector(literal(selector))),
            lab_dom_id: id => remember(document.getElementById(literal(id))),
            lab_dom_value_id: id => remember(document.getElementById(String(values[id] || ''))),
            lab_dom_listen: (node, type, action, passive) =>
                subscribe(values[node], literal(type), 'lab_events_dispatch', action, null, passive),
            lab_dom_subscribe: (node, type, entry, action, context, passive) =>
                subscribe(values[node], literal(type), literal(entry), action, values[context] || null, passive),
            lab_dom_node_contains: (parent, child) =>
                !!(values[parent] && values[child] && values[parent].contains(values[child])),
            lab_dom_all: (parent, selector) =>
                remember((values[parent] || document).querySelectorAll(literal(selector))),
            lab_dom_count: list => values[list]?.length || 0,
            lab_dom_item: (list, index) => remember(values[list]?.[index]),
            lab_dom_closest: (node, selector) => remember(values[node]?.closest?.(literal(selector))),
            lab_dom_create: tag => remember(document.createElement(literal(tag))),
            lab_dom_append: (parent, child) => values[parent].appendChild(values[child]),
            lab_dom_prepend: (parent, child) => values[parent]?.prepend(values[child]),
            lab_dom_read: (node, kind, key) =>
                remember(values[node] ? reads[kind](values[node], literal(key)) || '' : ''),
            lab_dom_write: (node, kind, key, text) => {
                if (values[node])
                    writes[kind](values[node], literal(key), String(values[text] || ''));
            },
            lab_dom_class: (node, name, enabled) => values[node]?.classList.toggle(literal(name), !!enabled),
            lab_dom_has_class: (node, name) => values[node]?.classList.contains(literal(name)) || false,
            lab_dom_disabled: (node, disabled) => {
                if (values[node])
                    values[node].disabled = !!disabled;
            },
            lab_dom_measure: (node, kind) => values[node] ? measures[kind](values[node]) : 0,
            lab_dom_css: (node, key) =>
                Number.parseFloat(getComputedStyle(values[node]).getPropertyValue(literal(key))) || 0,
            lab_dom_card_id: node => resultCardIds.get(values[node]) ?? -1,
            lab_dom_schedule: (action, card) => {
                const node = values[card];
                if (action === 0)
                    scheduleRenderedTeXFit();
                else
                    requestAnimationFrame(() => labDOM.call('lab_layout_zoom', node));
            }
        }
    };
})();

/** Browser APIs and JavaScript-value access for the bounded C/WebAssembly codec. */
const labWire = (() => {
    let codec, jobs;
    return {
        async start() {
            const [{createCodec}, {createJobs}] = await Promise.all([
                import('/js/codec.js'), import('/js/jobs.js')
            ]);
            const response = await fetch('/wasm/lab_browser.wasm', {cache: 'no-store'});
            if (!response.ok)
                throw new Error('Could not load the C browser module');
            const instance = createCodec(labDOM.env);
            await instance.start(await response.arrayBuffer());
            codec = instance;
            jobs = createJobs(codec.exports(), codec,
                new URL('/js/codec_worker.js', location.href).href,
                new URL('/wasm/lab_codec.wasm', location.href).href);
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
async function labFetch(url, options = {}) {
    const headers = new Headers(options.headers);
    headers.set('Accept', 'application/x-protobuf');
    if (options.body !== undefined)
        headers.set('Content-Type', 'application/x-protobuf');
    const response = await fetch(url, {...options, headers});
    const binary = (response.headers.get('Content-Type') || '').split(';')[0].trim() === 'application/x-protobuf';
    Object.defineProperty(response, 'labData', {
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
            return labWire.decodeAsync(bytes.buffer, options.signal);
        }
    });
    return response;
}
