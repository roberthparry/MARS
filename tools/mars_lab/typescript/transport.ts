/**
 * @file transport.ts
 * @brief Scoped browser capabilities for the native Lab ABI.
 *
 * Owns transient host handles, DOM access and continuation services. Cross-realm DOM
 * checks, callback binding and structured payload validation protect the browser/native boundary; native
 * code remains responsible for worksheet and presentation policy.
 */

/**
 * @brief Bind the explicitly listed public callbacks once per browser controller.
 * The type parameter checks method names without erasing their signatures.
 * Only this short, fixed list is visited; private methods and accessors are untouched.
 */
function marsBindCallbacks<T extends object>(owner: T, names: readonly (keyof T)[]): void {
    for (const name of names) {
        const method = owner[name];
        if (typeof method !== 'function') throw new TypeError(`Not a callback: ${String(name)}`);
        Object.defineProperty(owner, name, {
            value: method.bind(owner), writable: true, configurable: true, enumerable: true
        });
    }
}

/**
 * @brief Validate a DOM instance against its own realm's constructor, including iframe-owned objects.
 *
 * Reject structural lookalikes rather than relying on a nodeType field alone. The
 * check borrows document/window references and retains no cross-realm resources.
 */
function marsTransportIsDOM<K extends keyof MarsTransportDOMTypes>(
    value: unknown, kind: K
): value is MarsTransportDOMTypes[K] {
    if (typeof value !== 'object' || value === null) return false;
    const localConstructor: unknown = Reflect.get(globalThis, kind);
    if (typeof localConstructor === 'function' && value instanceof localConstructor) return true;

    const owner: unknown = Reflect.get(value, 'ownerDocument');
    const document: unknown = owner ?? (Reflect.get(value, 'nodeType') === 9 ? value : undefined);
    const realm: unknown = typeof document === 'object' && document !== null ?
        Reflect.get(document, 'defaultView') : Reflect.get(value, 'window') === value ? value : undefined;
    if (typeof realm !== 'object' || realm === null) return false;
    const constructor: unknown = Reflect.get(realm, kind);
    return typeof constructor === 'function' && value instanceof constructor;
}

/** @brief Shared numeric field kinds for lab_dom_read/write, matching wasm/include/lab_dom.h. */
const enum LabDOMFieldKind {
    textContent = 0,
    innerHTML = 1,
    attribute = 2,
    dataset = 3,
    style = 4,
    value = 5
}

/** @brief Numeric geometry kinds for lab_dom_measure, matching wasm/include/lab_dom.h. */
const enum LabDOMMeasureKind {
    clientWidth = 0,
    svgWidth = 1,
    svgHeight = 2,
    rectWidth = 3,
    rectHeight = 4,
    viewBoxWidth = 5,
    viewBoxHeight = 6,
    rectLeft = 7,
    rectTop = 8,
    rectBottom = 9,
    clientHeight = 10,
    scrollHeight = 11,
    isVisible = 12,
    ctmA = 13,
    ctmB = 14,
    ctmC = 15,
    ctmD = 16,
    ctmE = 17,
    ctmF = 18
}

/** Scoped host capabilities. Native handles exist only within one synchronous call. */
class LabDOMBridge {
    private values: unknown[] = [null];
    private active = false;
    private effects: (() => void)[] | null = null;
    private returned: unknown;
    private hasReturn = false;
    private eventServices: MarsTransportServices | null = null;
    private readonly subscriptions = new WeakMap<EventTarget, Map<string, Set<unknown>>>();
    private readonly optionVersions = new WeakMap<Element, {observer: MutationObserver; version: number}>();
    private readonly optionObservers = new Set<MutationObserver>();
    private readonly eventLifetime = new AbortController();
    private presentation: LabDOMPresentation | null = null;
    private readonly decoder = new TextDecoder('utf-8', {fatal: true});
    private readonly textDecoder = new TextDecoder('utf-8', {fatal: true, ignoreBOM: true});
    private readonly textEncoder = new TextEncoder();

    /** @brief Create isolated handle storage and bind detachable capabilities for native host adapters. */
    public constructor(private readonly nativeExports: () => MarsTransportExports) {
        // Existing host adapters detach and temporarily replace these capabilities.
        marsBindCallbacks(this, ['call', 'services', 'sync', 'flow', 'bindEvents', 'svgLength']);
    }

    /** Install the worksheet presentation capabilities once after definition scripts have loaded. */
    public bindPresentation(presentation: LabDOMPresentation): void {
        if (this.presentation) throw new Error('Lab presentation is already bound');
        this.presentation = presentation;
    }

    /** Release native event listeners and presentation references at the end of the application lifetime. */
    public dispose(): void {
        this.eventLifetime.abort();
        for (const observer of this.optionObservers) observer.disconnect();
        this.optionObservers.clear();
        this.eventServices = null;
        this.presentation = null;
    }

    private remember(value: unknown): number {
        if (value == null) return 0;
        if (this.values.length >= 4096) throw new Error('Lab DOM call exceeds its handle limit');
        this.values.push(value);
        return this.values.length - 1;
    }

    private literal(pointer: number): string {
        const bytes = new Uint8Array(this.nativeExports().memory.buffer);
        let end = pointer;
        while (end < bytes.length && end - pointer < 4096 && bytes[end]) ++end;
        if (end === bytes.length || end - pointer === 4096) throw new Error('Invalid Lab DOM label');
        return this.decoder.decode(bytes.subarray(pointer, end));
    }

    private object(value: unknown): object {
        if ((typeof value !== 'object' || value === null) && typeof value !== 'function')
            throw new TypeError('Expected an object in a native DOM handle');
        return value;
    }

    private key(value: unknown): PropertyKey {
        if (typeof value === 'string' || typeof value === 'number' || typeof value === 'symbol') return value;
        throw new TypeError('Expected a native property key');
    }

    private get(value: unknown, key: PropertyKey): unknown {
        return value == null ? undefined : Reflect.get(Object(value), key);
    }

    private propertyPresent(object: number, key: number): boolean {
        const value = this.values[object];
        return value !== null && typeof value === 'object' && this.literal(key) in value;
    }

    private put(value: unknown, key: PropertyKey, member: unknown): void {
        if (!Reflect.set(this.object(value), key, member))
            throw new TypeError('Cannot assign a native continuation field');
    }

    private element(id: number): Element {
        const value = this.values[id];
        if (!(marsTransportIsDOM(value, 'Element'))) throw new TypeError('Expected a DOM element');
        return value;
    }

    private styled(id: number): HTMLElement | SVGElement {
        const value = this.values[id];
        if (!(marsTransportIsDOM(value, 'HTMLElement') || marsTransportIsDOM(value, 'SVGElement')))
            throw new TypeError('Expected a styled DOM element');
        return value;
    }

    private node(id: number): Node {
        const value = this.values[id];
        if (!(marsTransportIsDOM(value, 'Node'))) throw new TypeError('Expected a DOM node');
        return value;
    }

    private map(id: number): Map<unknown, unknown> {
        const value = this.values[id];
        if (!(value instanceof Map)) throw new TypeError('Expected a native map handle');
        return value;
    }

    private array(id: number): unknown[] {
        const value = this.values[id];
        if (!Array.isArray(value)) throw new TypeError('Expected a native array handle');
        return value;
    }

    private queryRoot(id: number): ParentNode {
        const value = this.values[id];
        if (!value) return document;
        if (marsTransportIsDOM(value, 'Element') || marsTransportIsDOM(value, 'Document') || marsTransportIsDOM(value, 'DocumentFragment')) return value;
        throw new TypeError('Expected a DOM query root');
    }

    private compare(left: unknown, right: unknown): number {
        if (typeof left === 'string' && typeof right === 'string') return left < right ? -1 : left > right ? 1 : 0;
        const a = Number(left), b = Number(right);
        return a < b ? -1 : a > b ? 1 : 0;
    }

    /** @brief Resolve absolute SVG lengths to CSS pixels; return zero for unsupported or relative units. */
    public svgLength(value: unknown): number {
        const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
        svg.setAttribute('width', String(value || ''));
        const length = svg.width.baseVal;
        return length.unitType === 1 || (length.unitType >= 5 && length.unitType <= 10) ? length.value : 0;
    }

    // Computed numeric keys keep the C ABI explicit without relying on array position.
    private readonly reads: {readonly [kind: number]: (node: number, key: string) => unknown} = {
        [LabDOMFieldKind.textContent]: id => this.node(id).textContent,
        [LabDOMFieldKind.innerHTML]: id => this.element(id).innerHTML,
        [LabDOMFieldKind.attribute]: (id, key) => this.element(id).getAttribute(key),
        [LabDOMFieldKind.dataset]: (id, key) => this.styled(id).dataset[key],
        [LabDOMFieldKind.style]: (id, key) => this.styled(id).style.getPropertyValue(key),
        [LabDOMFieldKind.value]: id => this.get(this.values[id], 'value')
    };
    private readonly writes: {readonly [kind: number]: (node: number, key: string, text: string) => void} = {
        [LabDOMFieldKind.textContent]: (id, _key, text) => { this.node(id).textContent = text; },
        [LabDOMFieldKind.innerHTML]: (id, _key, text) => { this.element(id).innerHTML = text; },
        [LabDOMFieldKind.attribute]: (id, key, text) => this.element(id).setAttribute(key, text),
        [LabDOMFieldKind.dataset]: (id, key, text) => { this.styled(id).dataset[key] = text; },
        [LabDOMFieldKind.style]: (id, key, text) => {
            const style = this.styled(id).style;
            if (style.getPropertyValue(key) !== text || style.getPropertyPriority(key)) style.setProperty(key, text);
        },
        [LabDOMFieldKind.value]: (id, _key, text) => { Reflect.set(this.object(this.values[id]), 'value', text); }
    };
    private readonly measures: {readonly [kind: number]: (node: Element) => number} = {
        [LabDOMMeasureKind.clientWidth]: node => node.clientWidth,
        [LabDOMMeasureKind.svgWidth]: node => this.svgLength(node.getAttribute('width')),
        [LabDOMMeasureKind.svgHeight]: node => this.svgLength(node.getAttribute('height')),
        [LabDOMMeasureKind.rectWidth]: node => node.getBoundingClientRect().width,
        [LabDOMMeasureKind.rectHeight]: node => node.getBoundingClientRect().height,
        [LabDOMMeasureKind.viewBoxWidth]: node => marsTransportIsDOM(node, 'SVGSVGElement') ? node.viewBox.baseVal.width : 0,
        [LabDOMMeasureKind.viewBoxHeight]: node => marsTransportIsDOM(node, 'SVGSVGElement') ? node.viewBox.baseVal.height : 0,
        [LabDOMMeasureKind.rectLeft]: node => node.getBoundingClientRect().left,
        [LabDOMMeasureKind.rectTop]: node => node.getBoundingClientRect().top,
        [LabDOMMeasureKind.rectBottom]: node => node.getBoundingClientRect().bottom,
        [LabDOMMeasureKind.clientHeight]: node => node.clientHeight,
        [LabDOMMeasureKind.scrollHeight]: node => node.scrollHeight,
        [LabDOMMeasureKind.isVisible]: node => Number(node.isConnected && node.clientHeight > 0 && node.getClientRects().length > 0),
        [LabDOMMeasureKind.ctmA]: node => this.screenCoefficient(node, 'a'),
        [LabDOMMeasureKind.ctmB]: node => this.screenCoefficient(node, 'b'),
        [LabDOMMeasureKind.ctmC]: node => this.screenCoefficient(node, 'c'),
        [LabDOMMeasureKind.ctmD]: node => this.screenCoefficient(node, 'd'),
        [LabDOMMeasureKind.ctmE]: node => this.screenCoefficient(node, 'e'),
        [LabDOMMeasureKind.ctmF]: node => this.screenCoefficient(node, 'f')
    };

    /** @brief Read one SVG screen-transform coefficient; non-SVG or unavailable transforms yield NaN. */
    private screenCoefficient(node: Element, key: 'a' | 'b' | 'c' | 'd' | 'e' | 'f'): number {
        return marsTransportIsDOM(node, 'SVGGraphicsElement') ? node.getScreenCTM()?.[key] ?? NaN : NaN;
    }

    /** @brief Execute registered native service calls after handle release; reject nested-scope dispatch and unknown own-property keys. */
    public services(plan: MarsTransportPlan, registry: MarsTransportServices | null): unknown {
        if (this.active) throw new Error('Native services cannot run inside a DOM scope');
        let result: unknown;
        for (const call of plan.calls || []) {
            if (!registry || !Object.hasOwn(registry, call.service))
                throw new Error('Unknown native service');
            // The native continuation ABI supplies heterogeneous argument tuples. The registry preserves
            // each callback's own signature; only this checked dispatch point erases that tuple.
            const invoke = registry[call.service] as (...args: unknown[]) => unknown;
            result = invoke.apply(registry, call.args);
        }
        return result;
    }

    /**
     * @brief Run the synchronous native continuation protocol to completion.
     * Each call returns a plan: done returns its value; otherwise the browser runs
     * its services, stores the last result at plan.save and asks C for the next step.
     * Services run outside the native handle scope. The frame carries progress.
     * On an exception, fail() consumes the frame's own failed stage and records the
     * exception for native recovery. Without that recovery stage the error propagates.
     */
    public sync<T = unknown>(entry: string, kind: number, frame: object, registry: MarsTransportServices): T {
        for (;;) {
            try {
                const plan = this.call<MarsTransportPlan>(entry, kind, frame);
                if (plan.done) return plan.value as T;
                const result = this.services(plan, registry);
                if (plan.save) this.put(frame, plan.save, result);
            } catch (error) { this.fail(frame, error, false); }
        }
    }

    /**
     * @brief Run the asynchronous native continuation protocol without retaining handles.
     * C returns done/value or a service plan, as in sync(). A plan with wait set
     * awaits the final service result before saving it and advancing the frame.
     * Cancellation is checked before native dispatch and after service completion;
     * a pending wait races cancellation so a stopped workflow cannot publish late data.
     * fail() routes exceptions through the frame's own failed stage, or rethrows.
     * Cancellation still ends the loop before another native recovery step can run.
     */
    public async flow<T = unknown>(kind: number, frame: object, view: unknown,
                                   registry: MarsTransportServices, signal?: AbortSignal): Promise<T> {
        for (;;) {
            signal?.throwIfAborted();
            try {
                const plan = this.call<MarsTransportPlan>('lab_flow_step', kind, frame, view);
                if (plan.done) return plan.value as T;
                const pending = this.services(plan, registry);
                const result = plan.wait ? await this.awaitResult(pending, signal) : pending;
                signal?.throwIfAborted();
                if (plan.save) this.put(frame, plan.save, result);
            } catch (error) { this.fail(frame, error, true); }
        }
    }

    private async awaitResult(pending: unknown, signal?: AbortSignal): Promise<unknown> {
        if (!signal) return pending;
        signal.throwIfAborted();
        let aborted: () => void = () => {};
        const cancellation = new Promise<never>((_resolve, reject) => {
            aborted = () => reject(signal.reason);
            signal.addEventListener('abort', aborted, {once: true});
        });
        try { return await Promise.race([pending, cancellation]); }
        finally { signal.removeEventListener('abort', aborted); }
    }

    private fail(frame: object, error: unknown, exposeText: boolean): void {
        if (!Object.hasOwn(frame, 'failed')) throw error;
        this.put(frame, 'stage', this.get(frame, 'failed'));
        Reflect.deleteProperty(frame, 'failed');
        this.put(frame, 'exception', error);
        if (exposeText) {
            let text: string | undefined;
            Object.defineProperty(frame, 'error', {
                configurable: true, enumerable: true,
                get() { return text ??= String(error); }
            });
        }
    }

    private subscribe(target: unknown, name: string, entry: string, action: number,
                      context: unknown, passive: number): void {
        if (!target) return;
        if (!(marsTransportIsDOM(target, 'EventTarget'))) throw new TypeError('Expected an event target');
        const key = name + ':' + entry + ':' + action;
        let installed = this.subscriptions.get(target);
        if (!installed) this.subscriptions.set(target, installed = new Map());
        let contexts = installed.get(key);
        if (!contexts) installed.set(key, contexts = new Set());
        if (contexts.has(context)) return;
        contexts.add(context);
        target.addEventListener(name, event => {
            const plan = this.call<MarsTransportPlan>(entry, action, event, context);
            if (plan.prevent) event.preventDefault();
            if (plan.stop) event.stopPropagation();
            this.services(plan, this.eventServices);
        }, {passive: !!passive, signal: this.eventLifetime.signal});
    }

    /** @brief Replace the event capability registry and return its previous value so fixtures can restore it. */
    public bindEvents(services: MarsTransportServices | null): MarsTransportServices | null {
        const previous = this.eventServices;
        this.eventServices = services;
        return previous;
    }

    /**
     * @brief Invoke one native export with scoped handles, release them on exit and run deferred DOM effects only after successful return.
     * Pass C bool options as Number(flag); JavaScript booleans are boxed as scoped browser-value handles.
     */
    public call<T = unknown>(name: string, ...args: unknown[]): T {
        if (this.active) throw new Error('Lab DOM calls cannot be nested');
        this.active = true;
        this.returned = undefined;
        this.hasReturn = false;
        const pending: (() => void)[] = [];
        this.effects = pending;
        let result: unknown;
        try {
            const entry = this.nativeExports()[name as `lab_${string}`];
            if (typeof entry !== 'function') throw new Error('Unknown native entry');
            result = entry(...args.map(value => typeof value === 'number' ? value : this.remember(value)));
            if (this.hasReturn) result = this.returned;
        } finally {
            this.values = [null];
            this.active = false;
            this.effects = null;
            this.returned = undefined;
            this.hasReturn = false;
        }
        // Effects capture browser values, never scoped handle IDs. Release the old
        // scope first: focus/change handlers may synchronously start a fresh call().
        // A native failure skips this queue; an effect failure propagates with the
        // bridge already reset and prevents the remaining effects from running.
        for (const effect of pending) effect();
        // Native export/result correspondence is an ABI contract, not inspectable at runtime.
        return result as T;
    }

    private joinTextFragments(array: number, separator: number): number {
        const parts = this.values[array];
        if (!Array.isArray(parts)) throw new Error('Native text fragments must be an array');
        return this.remember(parts.join(this.literal(separator)));
    }

    /** @brief Host imports for the native DOM ABI; captured handles are valid only during the owning synchronous call. */
    public readonly env: Record<string, (...args: number[]) => unknown> = {
        // --- Handle lifecycle ---
        lab_dom_mark: () => this.values.length,
        lab_dom_release: mark => {
            if (!this.active || mark < 1 || mark > this.values.length)
                throw new Error('Invalid Lab DOM handle scope');
            this.values.length = mark;
        },
        lab_dom_object: kind => this.remember(kind === 4 ? [] : {}),
        lab_dom_numeric: number => this.remember(number),
        lab_dom_scalar: (kind, number) => this.remember(
            kind === 1     ? !!number :
                kind === 2 ? number :
                             null),
        lab_dom_return: value => {
            this.returned = this.values[value];
            this.hasReturn = true;
        },

        // --- Property access ---
        lab_dom_get: (object, key) => this.remember(this.get(this.values[object], this.literal(key))),
        lab_dom_set: (object, key, value) => Object.defineProperty(
            this.object(this.values[object]), this.literal(key),
            {value: this.values[value], enumerable: true, writable: true, configurable: true}),
        lab_dom_key_get: (object, key) => this.remember(
            this.values[object] && Object.hasOwn(this.object(this.values[object]), this.key(this.values[key])) ?
                this.get(this.values[object], this.key(this.values[key])) : null),
        lab_dom_key_set: (object, key, value) => Object.defineProperty(
            this.object(this.values[object]), this.key(this.values[key]),
            {value: this.values[value], enumerable: true, writable: true, configurable: true}),
        lab_dom_property_present: this.propertyPresent.bind(this),
        lab_dom_key_delete: (object, key) => this.values[object] && Reflect.deleteProperty(this.object(this.values[object]), this.key(this.values[key])),
        lab_dom_delete: (object, key) => this.values[object] && Reflect.deleteProperty(this.object(this.values[object]), this.literal(key)),
        lab_dom_property_copy: (target, targetKey, source, sourceKey) => Object.defineProperty(
            this.object(this.values[target]), this.literal(targetKey),
            {value: this.get(this.values[source], this.literal(sourceKey)), writable: true, configurable: true, enumerable: true}),
        lab_dom_properties_equal: (left, leftKey, right, rightKey) =>
            this.get(this.values[left], this.literal(leftKey)) === this.get(this.values[right], this.literal(rightKey)),

        // --- Type inspection ---
        lab_dom_type: id => this.values[id] == null ? 0 :
            typeof this.values[id] === 'boolean'    ? 1 :
            typeof this.values[id] === 'number'     ? 2 :
            typeof this.values[id] === 'string'     ? 3 :
            this.values[id] instanceof Uint8Array  ? 6 :
            Array.isArray(this.values[id])          ? 4 :
                                                 5,
        lab_dom_truth: id => !!this.values[id],
        lab_dom_equal: (left, right) => this.values[left] === this.values[right],
        lab_dom_compare: (left, right) => this.compare(this.values[left], this.values[right]),

        // --- String and numeric conversion ---
        lab_dom_text: value => this.remember(String(this.values[value])),
        lab_dom_bytes_copy: (value, target, capacity) => {
            const bytes = this.values[value];
            if (!(bytes instanceof Uint8Array) || !(bytes.buffer instanceof ArrayBuffer) || bytes.byteLength > capacity)
                return -1;
            new Uint8Array(this.nativeExports().memory.buffer, target, bytes.byteLength).set(bytes);
            return bytes.byteLength;
        },
        lab_dom_utf8_copy: (value, target, capacity) => {
            const text = String(this.values[value]);
            if (text.length > capacity)
                return -1;
            const bytes = this.textEncoder.encode(text);
            if (bytes.length > capacity || this.textDecoder.decode(bytes) !== text)
                return -1;
            new Uint8Array(this.nativeExports().memory.buffer, target, bytes.length).set(bytes);
            return bytes.length;
        },
        lab_dom_utf8_string: (source, length) =>
            this.remember(this.textDecoder.decode(new Uint8Array(this.nativeExports().memory.buffer, source, length))),
        lab_dom_to_number: value => Number(this.values[value]),
        lab_dom_number: id => Number(this.values[id]) || 0,
        lab_dom_parse_int: (value, radix) => Number.parseInt(String(this.values[value]), radix),
        lab_dom_slice: (value, start, end) => this.remember(String(this.values[value] || '').slice(start, end)),
        lab_dom_split: (value, separator) => this.remember(String(this.values[value] || '').split(this.literal(separator))),
        lab_dom_starts_with: (value, prefix) => String(this.values[value] || '').startsWith(this.literal(prefix)),
        lab_dom_locale_compare: (left, right, numeric) =>
            String(this.values[left])
                .localeCompare(
                    String(this.values[right]), undefined, numeric ? {numeric: true, sensitivity: 'base'} : undefined),
        lab_dom_contains: (text, query) => String(this.values[text] || '').includes(String(this.values[query] || '')),
        lab_dom_normalize: value => this.remember(
            String(this.values[value] || '').normalize('NFD').replace(/[\u0300-\u036f]/g, '').toLowerCase().trim()),
        lab_dom_clean: (text, kind) => this.remember(
            kind === 1     ? String(this.values[text] || '').trim().toLowerCase() :
                kind === 2 ? String(this.values[text] || '').trim().replace(/\s+/g, ' ') :
                kind === 3 ? String(this.values[text] || '').trimEnd() :
                             String(this.values[text] || '').trim()),
        lab_dom_length: id => String(this.values[id] || '').length,
        lab_dom_string: pointer => this.remember(this.literal(pointer)),
        lab_dom_join: (left, right, suffix) =>
            this.remember(String(this.values[left] || '') + String(this.values[right] || '') + this.literal(suffix)),
        lab_dom_join_array: this.joinTextFragments.bind(this),
        lab_dom_format: (number, prefix, suffix) => this.remember(this.literal(prefix) + number + this.literal(suffix)),
        lab_dom_precision: (number, digits) => this.remember(String(Number(number.toPrecision(digits)))),

        // --- Collection operations ---
        lab_dom_push: (array, value) => this.array(array).push(this.values[value]),
        lab_dom_index_present: (array, index) => Array.isArray(this.values[array]) && index in this.array(array),
        lab_dom_keys: object => this.remember(Object.keys(this.values[object] || {})),
        lab_dom_member: (set, value) => (this.values[set] instanceof Set || this.values[set] instanceof Map) && this.values[set].has(this.values[value]),
        lab_dom_map_new: () => this.remember(new Map()),
        lab_dom_map_get: (map, key) => this.remember(this.values[map] ? this.map(map).get(this.values[key]) : undefined),
        lab_dom_map_set: (map, key, value) => this.map(map).set(this.values[key], this.values[value]),
        lab_dom_map_size: map => this.values[map] ? this.map(map).size : 0,
        lab_dom_map_first: map => this.remember(this.values[map] ? this.map(map).keys().next().value : undefined),
        lab_dom_map_delete: (map, key) => this.values[map] ? this.map(map).delete(this.values[key]) : false,

        // --- DOM queries and tree operations ---
        lab_dom_query: (parent, selector) =>
            this.remember(this.queryRoot(parent).querySelector(this.literal(selector))),
        lab_dom_all: (parent, selector) =>
            this.remember(this.queryRoot(parent).querySelectorAll(this.literal(selector))),
        lab_dom_id: id => this.remember(document.getElementById(this.literal(id))),
        lab_dom_value_id: id => this.remember(document.getElementById(String(this.values[id] || ''))),
        lab_dom_closest: (node, selector) => this.remember(marsTransportIsDOM(this.values[node], 'Element') ? this.values[node].closest(this.literal(selector)) : undefined),
        lab_dom_count: list => Number(this.get(this.values[list], 'length')) || 0,
        lab_dom_select_index: (select, index) => {
            const node = this.values[select];
            if (marsTransportIsDOM(node, 'HTMLSelectElement')) node.selectedIndex = index;
        },
        lab_dom_option_version: select => {
            const node = this.element(select);
            let state = this.optionVersions.get(node);
            if (!state) {
                const entry = {observer: null as unknown as MutationObserver, version: 1};
                entry.observer = new MutationObserver(() => { ++entry.version; });
                entry.observer.observe(node, {
                    subtree: true, childList: true, characterData: true, attributes: true,
                    attributeFilter: ['value', 'label', 'disabled', 'data-latitude', 'data-longitude', 'data-detail']
                });
                this.optionVersions.set(node, state = entry);
                this.optionObservers.add(entry.observer);
            }
            if (state.observer.takeRecords().length) ++state.version;
            return state.version;
        },
        lab_dom_item: (list, index) => this.remember(this.get(this.values[list], index)),
        lab_dom_node_contains: (parent, child) =>
            !!(this.values[parent] && this.values[child] && this.node(parent).contains(this.node(child))),
        lab_dom_create: tag => this.remember(document.createElement(this.literal(tag))),
        lab_dom_append: (parent, child) => this.node(parent).appendChild(this.node(child)),
        lab_dom_prepend: (parent, child) => this.values[parent] ? this.element(parent).prepend(this.node(child)) : undefined,
        lab_dom_wrap: (node, shell) => this.node(node).parentNode?.insertBefore(this.node(shell), this.node(node)),
        lab_dom_active: () => this.remember(document.activeElement),
        lab_dom_selected: select => this.remember(marsTransportIsDOM(this.values[select], 'HTMLSelectElement') ? this.values[select].selectedOptions[0] : undefined),

        // --- DOM reads and writes ---
        lab_dom_read: (node, kind, key) =>
            this.remember(this.values[node] ? this.reads[kind](node, this.literal(key)) || '' : ''),
        lab_dom_write: (node, kind, key, text) => {
            if (this.values[node])
                this.writes[kind](node, this.literal(key), String(this.values[text] || ''));
        },
        lab_dom_has: (node, key) => this.values[node] ? this.element(node).hasAttribute(this.literal(key)) : false,
        lab_dom_remove: (node, key) => this.values[node] ? this.element(node).removeAttribute(this.literal(key)) : undefined,
        lab_dom_class: (node, name, enabled) => this.values[node] ? this.element(node).classList.toggle(this.literal(name), !!enabled) : false,
        lab_dom_has_class: (node, name) => this.values[node] ? this.element(node).classList.contains(this.literal(name)) : false,
        lab_dom_disabled: (node, disabled) => {
            if (this.values[node] && this.get(this.values[node], 'disabled') !== !!disabled)
                Reflect.set(this.object(this.values[node]), 'disabled', !!disabled);
        },
        lab_dom_checked: (node, checked) => {
            if (this.values[node])
                Reflect.set(this.object(this.values[node]), 'checked', !!checked);
        },
        lab_dom_is_disabled: node => this.values[node] ? this.element(node).matches(':disabled') : false,

        // --- Measurement ---
        lab_dom_measure: (node, kind) => this.values[node] ? this.measures[kind](this.element(node)) : 0,
        lab_dom_css: (node, key) =>
            Number.parseFloat(getComputedStyle(this.element(node)).getPropertyValue(this.literal(key))) || 0,

        // --- Presentation and deferred effects ---
        lab_dom_effect: (node, action) => {
            // Resolve and validate inside the native scope; queued closures retain the node, not its handle.
            const target = this.values[node] ? this.styled(node) : null;
            if (!target)
                return;
            const actions = [
                () => target.focus(), () => target.scrollIntoView({block: 'nearest', inline: 'nearest'}),
                () => target.dispatchEvent(new Event('change', {bubbles: true})), () => { if (marsTransportIsDOM(target, 'HTMLInputElement') || marsTransportIsDOM(target, 'HTMLTextAreaElement')) target.select(); },
                () => target.blur()
            ];
            if (!this.effects || !actions[action])
                throw new Error('Invalid Lab DOM effect');
            this.effects.push(actions[action]);
        },
        lab_dom_schedule: (action, card) => {
            const node = this.values[card];
            if (action === 0)
                this.presentation?.scheduleFit();
            else
                requestAnimationFrame(() => this.call('lab_layout_zoom', node));
        },
        lab_dom_card_id: node => {
            const element = this.values[node];
            return marsTransportIsDOM(element, 'HTMLElement') ? this.presentation?.cardId(element) ?? -1 : -1;
        },

        // --- Events ---
        lab_dom_listen: (node, type, action, passive) =>
            this.subscribe(this.values[node], this.literal(type), 'lab_events_dispatch', action, null, passive),
        lab_dom_subscribe: (node, type, entry, action, context, passive) =>
            this.subscribe(this.values[node], this.literal(type), this.literal(entry), action, this.values[context] || null, passive),
    };

}

/** Lazily initialised codec and background worker resources. */
class LabWireTransport {
    private codec: MarsTransportCodec | null = null;
    private jobs: MarsTransportJobs | null = null;
    private readonly lifetime = new AbortController();

    /** @brief Create an unstarted wire transport with callback-safe bound methods. */
    public constructor(private readonly environment: () => WebAssembly.ModuleImports,
                       private readonly fetchResponse: typeof fetch = globalThis.fetch.bind(globalThis)) {
        marsBindCallbacks(this, [
            'start', 'exports', 'encode', 'decode', 'encodeStored', 'decodeStored',
            'decodeAsync', 'background', 'text', 'precision', 'intervals'
        ]);
    }

    /** @brief Load codec and job modules, initialise WASM and arrange worker cleanup when the page leaves outside the back-forward cache. */
    public async start(background = true): Promise<void> {
        this.lifetime.signal.throwIfAborted();
        // URLs remain runtime module locations; no static import changes the classic-script scope.
        const codecURL = '/js/codec.js', jobsURL = '/js/jobs.js';
        const [codecModule, jobsModule]: [MarsTransportCodecModule, MarsTransportJobsModule | null] =
            await Promise.all([import(codecURL), background ? import(jobsURL) : Promise.resolve(null)]);
        this.lifetime.signal.throwIfAborted();
        const response = await this.fetchResponse('/wasm/lab_browser.wasm', {cache: 'no-store', signal: this.lifetime.signal});
        this.lifetime.signal.throwIfAborted();
        if (!response.ok) throw new Error('Could not load the C browser module');
        const bytes = await response.arrayBuffer();
        this.lifetime.signal.throwIfAborted();
        const instance = codecModule.createCodec(this.environment());
        await instance.start(bytes);
        this.lifetime.signal.throwIfAborted();
        this.codec = instance;
        if (!jobsModule) return; // Drawing workers decode their own HTTP responses directly in native memory.
        const jobs = jobsModule.createJobs(instance.exports(), instance,
            new URL('/js/codec_worker.js', location.href).href,
            new URL('/wasm/lab_codec.wasm', location.href).href);
        this.jobs = jobs;
    }

    /** Close owned worker resources; repeated disposal is harmless. */
    public dispose(): void {
        this.lifetime.abort();
        this.jobs?.close();
        this.jobs = null;
        this.codec = null;
    }

    private ready(): MarsTransportCodec {
        if (!this.codec) throw new Error('C browser module is not ready');
        return this.codec;
    }

    /** @brief Expose the validated application ABI; throw until the native browser module is ready. */
    public exports(): MarsTransportExports {
        // The codec validates the native ABI on startup. Its general WebAssembly export map
        // does not describe application entry points; this bridge exposes their numeric ABI.
        return this.ready().exports() as unknown as MarsTransportExports;
    }

    /** @brief Return an independent Protobuf encoding through the initialised native codec. */
    public encode(value: unknown): Uint8Array<ArrayBuffer> { return this.ready().encode(value); }

    /** @brief Decode a bounded non-shared buffer synchronously through the initialised native codec. */
    public decode(buffer: MarsTransportBuffer): unknown { return this.ready().decode(buffer); }

    /** @brief Encode a local storage record with the native base64/Protobuf codec. */
    public encodeStored(value: unknown): string { return this.ready().encodeStored(value); }

    /** @brief Validate and decode local storage text through the native codec. */
    public decodeStored(text: string): unknown { return this.ready().decodeStored(text); }

    /** @brief Decode through the job controller, which selects local or worker execution and observes the optional abort signal. */
    public decodeAsync(buffer: ArrayBuffer, signal?: AbortSignal | null): Promise<unknown> {
        return this.background().decode(buffer, signal ?? undefined);
    }

    /** @brief Return the owned job controller, or throw before worker resources have been initialised. */
    public background(): MarsTransportJobs {
        if (!this.jobs) throw new Error('Codec workers are not ready');
        return this.jobs;
    }

    /** @brief Copy a validated NUL-terminated UTF-8 string from the current native memory. */
    public text(pointer: number): string { return this.ready().text(pointer); }

    /** @brief Apply native precision bounds and fallback policy through the ready codec. */
    public precision(value: number, fallback: number): number { return this.ready().precision(value, fallback); }

    /** @brief Apply native integration-interval bounds and fallback policy through the ready codec. */
    public intervals(value: number, fallback: number): number { return this.ready().intervals(value, fallback); }
}

// Mutual dependency: the DOM bridge needs native exports, while the wire transport
// needs its import table. Constructors retain these callbacks without invoking them;
// both bindings are initialised before either callback is used.

/** @brief Application-owned scoped DOM bridge, resolved lazily against the wire transport. */
const labDOM: LabDOMBridge = new LabDOMBridge(() => labWire.exports());

/** @brief Application-owned native transport, borrowing the DOM imports when started. */
const labWire: LabWireTransport = new LabWireTransport(() => labDOM.env);

/** HTTP boundary with explicit native decoding and browser network dependencies. */
class LabHttpClient {
    /** Retain response capabilities; construction performs no network access. */
    public constructor(private readonly decoder: LabResponseDecoder,
                       private readonly fetchResponse: typeof fetch = (input, init) => globalThis.fetch(input, init)) {}

    /**
     * @brief Fetch a native response without changing its HTTP success or failure status.
     *
     * The returned Response gains a labData method that consumes its body once,
     * requires the Protobuf content type and enforces the native message-size limit
     * while streaming. Decoding uses the owned job controller and the request's abort
     * signal; callers remain responsible for interpreting HTTP and native status.
     */
    public async fetch(url: RequestInfo | URL, options: RequestInit = {}): Promise<MarsTransportResponse> {
        const headers = new Headers(options.headers);
        headers.set('Accept', 'application/x-protobuf');
        if (options.body !== undefined) headers.set('Content-Type', 'application/x-protobuf');
        const response = await this.fetchResponse(url, {...options, headers});
        const binary = (response.headers.get('Content-Type') || '').split(';')[0].trim() === 'application/x-protobuf';
        const labData = async (): Promise<unknown> => {
            if (response.bodyUsed) throw new Error('Lab response body has already been consumed');
            if (!binary) {
                await response.body?.cancel();
                throw new Error('Expected a Protobuf Lab response');
            }
            if (!response.body) throw new Error('Missing Protobuf Lab response body');
            const limit = this.decoder.messageLimit();
            const reader = response.body.getReader();
            const parts: Uint8Array[] = [];
            let length = 0;
            try {
                for (;;) {
                    const {done, value} = await reader.read();
                    if (done) break;
                    length += value.length;
                    if (length > limit) {
                        await reader.cancel();
                        throw new Error(`Lab Protobuf response exceeds the native message limit (${limit} bytes)`);
                    }
                    parts.push(value);
                }
            } finally { reader.releaseLock(); }
            const bytes = new Uint8Array(length);
            let offset = 0;
            for (const part of parts) {
                bytes.set(part, offset);
                offset += part.length;
            }
            return this.decoder.decode(bytes.buffer, options.signal);
        };
        return Object.defineProperty(response, 'labData', {value: labData}) as MarsTransportResponse;
    }


}

/** @brief Application HTTP boundary, decoding native responses through the shared wire transport. */
const labHttp: LabHttpClient = new LabHttpClient({
    messageLimit: () => labWire.exports().lab_browser_message_limit(),
    decode: (buffer, signal) => labWire.decodeAsync(buffer, signal)
});
