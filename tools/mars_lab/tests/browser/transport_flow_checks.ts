/**
 * @file transport_flow_checks.ts
 * @brief Transport continuation sequencing and cleanup regressions.
 *
 * Controls browser service completions to verify lazy diagnostics, publication policy and unconditional request cleanup, including failures.
 */

/// <reference path="./transport_flow_checks.d.ts" />

/** @brief Compile-time transport contract checks; deliberately never invoked. */
function checkTransportTypeContracts(native: MarsTransportExports): void {
    const services: MarsTransportServices = {
        text(value: string): string { return value; },
        count(value: number): number { return value; },
        notify(): void {}
    };
    const call: MarsTransportServiceCall = {service: 'text', args: ['ready']};
    const plan: NativeServicePlan = {calls: [call], save: 'result', wait: true};
    const registry: NativeServiceRegistry = services;
    const result: number = native.lab_example(1, 2);
    // @ts-expect-error Native exports accept numeric arguments, not browser strings.
    native.lab_example('1');
    // @ts-expect-error Numeric export declarations must not accept string results.
    const invalidExport: MarsTransportNumericExport = function () { return '1'; };
    // @ts-expect-error Service registries are read-only through their transport contract.
    registry.text = services.count;
    // @ts-expect-error Only the dispatcher may supply arguments to heterogeneous callbacks.
    registry.text('ready');
    // @ts-expect-error Native service arguments must be an ordered argument list.
    const invalidCall: MarsTransportServiceCall = {service: 'text', args: 'ready'};
    void plan;
    void result;
    void invalidExport;
    void invalidCall;
}

/** @brief Exercise native scope cleanup and deferred browser effects without loading another WASM instance. */
function checkTransportDomLifetime(check: LabTestCheck): void {
    const events: string[] = [];
    const first = document.createElement('input');
    const second = document.createElement('input');
    const failure = new Error('synthetic native failure');
    const effectFailure = new Error('synthetic focus failure');
    let listenerFailure: unknown;
    const native: MarsTransportExports = {
        memory: new WebAssembly.Memory({initial: 1}),
        lab_nested() {
            let rejected = false;
            try {
                bridge.call('lab_empty');
            } catch (error) {
                rejected = String(error).includes('Lab DOM calls cannot be nested');
            }
            check(rejected, 'direct native re-entry is rejected');
            return 17;
        },
        lab_empty() {
            check(bridge.env.lab_dom_mark() === 1, 'fresh calls start with an empty handle scope');
            return 23;
        },
        lab_focus(node: number) {
            bridge.env.lab_dom_effect(node, 0);
            events.push('inner native');
            return 29;
        },
        lab_effects(node: number) {
            bridge.env.lab_dom_effect(node, 2);
            bridge.env.lab_dom_effect(node, 0);
            bridge.env.lab_dom_release(1);
            check(bridge.env.lab_dom_object(0) === node, 'released node handle is reused before effects');
            check(events.length === 0, 'effects wait until native execution finishes');
            events.push('outer native');
            return 31;
        },
        lab_fail(node: number) {
            bridge.env.lab_dom_effect(node, 0);
            bridge.env.lab_dom_return(node);
            throw failure;
        },
        lab_throwing_effect(node: number, later: number) {
            bridge.env.lab_dom_effect(node, 0);
            bridge.env.lab_dom_effect(later, 0);
            return 0;
        }
    };
    const bridge = new LabDOMBridge(() => native);
    const call = bridge.call;
    first.focus = function focusFirst() { events.push('first focus'); };
    second.focus = function focusSecond() { events.push('second focus'); };
    function changed(): void {
        // Catch listener failures locally: browser-dispatched exceptions otherwise escape the harness.
        try {
            events.push('change');
            check(call('lab_focus', second) === 29, 'change handlers may start a fresh native call');
        } catch (error) { listenerFailure = error; }
    }
    first.addEventListener('change', changed);
    try {
        check(call('lab_nested') === 17, 'rejected nested call leaves its outer call usable');
        check(call('lab_effects', first) === 31, 'effects preserve the outer native result');
        check(listenerFailure === undefined, 'nested change-handler call completes without exception');
        check(events.join(',') === 'outer native,change,inner native,second focus,first focus',
            'effects retain actual nodes after handle reuse and nested effects finish in order');
        events.length = 0;
        let caught: unknown;
        try {
            call('lab_fail', first);
        } catch (error) {
            caught = error;
        }
        check(caught === failure && events.length === 0, 'native failure drops queued effects and preserves the exception');
        check(call('lab_empty') === 23, 'native failure clears handles and the pending returned value');
        first.focus = function failFocus() { throw effectFailure; };
        caught = undefined;
        try {
            call('lab_throwing_effect', first, second);
        } catch (error) {
            caught = error;
        }
        check(caught === effectFailure && events.length === 0,
            'effect failure propagates directly and prevents remaining effects');
        check(call('lab_empty') === 23, 'effect failure leaves the bridge ready for another call');
        call('lab_focus', second);
        check(events.join(',') === 'inner native,second focus', 'fresh calls still queue effects after an effect failure');
    } finally {
        first.removeEventListener('change', changed);
        bridge.dispose();
    }
}

/** @brief Pin the numeric native field and measurement ABI independently of the production dispatch tables. */
function checkTransportDomMappings(check: LabTestCheck): void {
    const memory = new WebAssembly.Memory({initial: 1});
    const native: MarsTransportExports = {
        memory,
        lab_read(node: number, kind: number, key: number) {
            bridge.env.lab_dom_return(Number(bridge.env.lab_dom_read(node, kind, key)));
            return 0;
        },
        lab_write(node: number, kind: number, key: number, value: number) {
            bridge.env.lab_dom_write(node, kind, key, value);
            return 0;
        },
        lab_measure(node: number, kind: number) { return Number(bridge.env.lab_dom_measure(node, kind)); }
    };
    const bridge = new LabDOMBridge(() => native);
    const field = document.createElement('div');
    const control = document.createElement('textarea');
    const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
    const keys = ['', '', 'title', 'fixture', 'color', ''];
    const values = ['plain text', '<b>markup</b>', 'attribute', 'dataset', 'red', 'input value'];
    function literal(value: string): number {
        new Uint8Array(memory.buffer, 32, 128).fill(0);
        new Uint8Array(memory.buffer, 32).set(new TextEncoder().encode(value));
        return 32;
    }
    try {
        for (let kind = 0; kind < 6; ++kind) {
            bridge.call('lab_write', kind === 5 ? control : field, kind, literal(keys[kind]), values[kind]);
            const actual = [field.textContent, field.innerHTML, field.getAttribute('title'),
                field.dataset.fixture, field.style.color, control.value];
            check(actual[kind] === values[kind], 'numeric field write maps native kind ' + kind);
        }
        field.textContent = 'read text';
        check(bridge.call('lab_read', field, 0, literal('')) === 'read text', 'text read uses native kind 0');
        field.innerHTML = '<i>read markup</i>';
        field.setAttribute('title', 'read attribute');
        field.dataset.fixture = 'read dataset';
        field.style.color = 'blue';
        control.value = 'read value';
        const reads = [field.textContent, field.innerHTML, field.getAttribute('title'),
            field.dataset.fixture, field.style.color, control.value];
        for (let kind = 0; kind < 6; ++kind)
            check(bridge.call('lab_read', kind === 5 ? control : field, kind, literal(keys[kind])) === reads[kind],
                'numeric field read maps native kind ' + kind);
        svg.setAttribute('width', '31px');
        svg.setAttribute('height', '32px');
        svg.setAttribute('viewBox', '0 0 33 34');
        Object.defineProperties(svg, {
            clientWidth: {value: 101}, clientHeight: {value: 102}, scrollHeight: {value: 103},
            getClientRects: {value: function rectangles() { return [new DOMRect(35, 36, 37, 38)]; }}
        });
        svg.getBoundingClientRect = function rectangle() { return new DOMRect(35, 36, 37, 38); };
        svg.getScreenCTM = function screenMatrix() { return new DOMMatrix([41, 42, 43, 44, 45, 46]); };
        document.body.appendChild(svg);
        const measurements = [101, 31, 32, 37, 38, 33, 34, 35, 36, 74, 102, 103, 1, 41, 42, 43, 44, 45, 46];
        for (let kind = 0; kind < measurements.length; ++kind) {
            check(bridge.call('lab_measure', svg, kind) === measurements[kind],
                'numeric measurement maps native kind ' + kind);
            check(bridge.call('lab_measure', null, kind) === 0, 'absent node measurement is zero for kind ' + kind);
        }
        svg.remove();
        check(bridge.call('lab_measure', svg, 12) === 0, 'detached node is not visibly connected');
        check(bridge.call('lab_measure', field, 5) === 0 && bridge.call('lab_measure', field, 6) === 0,
            'non-SVG elements have no view-box dimensions');
        for (let kind = 13; kind <= 18; ++kind)
            check(Number.isNaN(bridge.call('lab_measure', field, kind)), 'non-SVG screen coefficients are unavailable');
    } finally {
        svg.remove();
        bridge.dispose();
    }
}

/** Native transport sequencing, lazy diagnostics, publication policy and unconditional request cleanup. */
window.checkLabTransportFlows = async function checkLabTransportFlows() {
    function check(condition: unknown, message: string): void {
        if (!condition)
            throw new Error('Transport flow: ' + message);
    }
    function equal(actual: unknown, expected: unknown, message: string): void {
        check(Object.is(actual, expected), message);
    }
    checkTransportDomLifetime(check);
    checkTransportDomMappings(check);
    const cancellation = new AbortController();
    const requestHeaders = new Headers({'X-Fixture': 'preserved'});
    const body = new Uint8Array([1, 2, 3]);
    let decoded = 0;
    const http = new LabHttpClient({
        messageLimit: () => body.length,
        async decode(buffer, signal) {
            ++decoded;
            equal(signal, cancellation.signal, 'HTTP decoder receives cancellation identity');
            equal(Array.from(new Uint8Array(buffer)).join(','), '1,2,3', 'HTTP decoder receives owned bytes');
            return {fixture: true};
        }
    }, async function fetchResponse(url, options) {
        equal(url, '/isolated', 'HTTP client uses its injected network adapter');
        const headers = new Headers(options?.headers);
        equal(headers.get('Accept'), 'application/x-protobuf', 'HTTP client requests native wire format');
        equal(headers.get('Content-Type'), 'application/x-protobuf', 'HTTP client declares native request format');
        equal(headers.get('X-Fixture'), 'preserved', 'HTTP client preserves caller headers');
        equal(options?.signal, cancellation.signal, 'HTTP network request receives cancellation identity');
        return new Response(body, {status: 422, headers: {'Content-Type': 'application/x-protobuf; charset=binary'}});
    });
    const isolated = await http.fetch('/isolated', {method: 'POST', body, headers: requestHeaders, signal: cancellation.signal});
    equal(isolated.status, 422, 'HTTP client preserves non-success status for native policy');
    equal(decoded, 0, 'HTTP client decodes lazily');
    equal(requestHeaders.get('Accept'), null, 'HTTP client does not mutate caller headers');
    check((await isolated.labData() as {fixture: boolean}).fixture, 'HTTP client returns the injected decoded value');
    equal(decoded, 1, 'HTTP client consumes the body once');
    let consumed = false;
    try {
        await isolated.labData();
    } catch {
        consumed = true;
    }
    check(consumed, 'consumed HTTP bodies cannot be decoded twice');
    equal(decoded, 1, 'second body read does not invoke decoder');
    let boundedDecode = false, bodyCancelled = false;
    const boundedHttp = new LabHttpClient({
        messageLimit: () => 2,
        async decode() {
            boundedDecode = true;
            return null;
        }
    }, async function streamResponse() {
        const stream = new ReadableStream<Uint8Array>({
            start(controller) { controller.enqueue(body); },
            cancel() { bodyCancelled = true; }
        });
        return new Response(stream, {headers: {'Content-Type': 'application/x-protobuf'}});
    });
    let boundedError = '';
    try {
        await (await boundedHttp.fetch('/bounded')).labData();
    } catch (error) {
        boundedError = String(error);
    }
    check(boundedError.includes('2 bytes') && bodyCancelled && !boundedDecode,
        'HTTP instances enforce their own limit and cancel oversized streams before decoding');
    const flowCancellation = new AbortController();
    const bridge = new LabDOMBridge(() => ({memory: new WebAssembly.Memory({initial: 1})}));
    let stages = 0;
    const wait = LabTestBrowser.signal();
    // Preserve the production bridge's generic result contract at this fixture boundary.
    bridge.call = function callFixture<T = unknown>(): T {
        ++stages;
        if (stages === 1)
            return {calls: [{service: 'wait', args: []}], wait: true} as T;
        return {done: true} as T;
    };
    const reason = new Error('fixture lifetime ended');
    const cancelledFlow = bridge.flow(0, {}, null, {wait: () => wait.promise}, flowCancellation.signal)
        .then(() => false, error => error === reason);
    flowCancellation.abort(reason);
    check(await cancelledFlow, 'continuation cancellation rejects without awaiting the external operation');
    wait.resolve();
    await Promise.resolve();
    equal(stages, 1, 'cancelled continuations never resume native dispatch');
    bridge.dispose();
    async function complete(kind: number, frame: LabTransportTestFrame, registry: object): Promise<LabTransportTestResult> {
        await labDOM.flow(kind, frame, null, registry as unknown as MarsTransportServices);
        return frame.result as LabTransportTestResult;
    }
    async function rejected(task: Promise<unknown>, expected: unknown, message: string): Promise<void> {
        let threw = false, caught;
        try {
            await task;
        } catch (error) {
            threw = true;
            caught = error;
        }
        check(threw, message + ': must reject');
        equal(caught, expected, message + ': exact thrown value');
    }
    const events: string[] = [], owner = {};
    let current = true;
    let admitted: object | null = owner, finishFailure: unknown, ownershipFailure: unknown;
    const execution = {
        begin(frame: {operation: string}) {
            events.push('begin');
            equal(frame.operation, 'evaluate', 'operation reaches admission');
            return admitted;
        },
        callback(frame: {request: unknown; callback(request: unknown): unknown}) {
            events.push('callback');
            equal(frame.request, owner, 'callback receives admitted request identity');
            return frame.callback(frame.request);
        },
        current(request: unknown) {
            events.push('current');
            equal(request, owner, 'ownership check receives admitted identity');
            if (ownershipFailure)
                throw ownershipFailure;
            return current;
        },
        finish(request: unknown) {
            events.push('finish');
            equal(request, owner, 'finaliser receives admitted identity');
            if (finishFailure)
                throw finishFailure;
        },
        rethrow(frame: {exception: unknown}) { throw frame.exception; },
        renderError: (error: unknown) => events.push('error:' + error),
        resetDigits: () => events.push('digits'),
        clearResults(options: {keepBindings: boolean}) {
            equal(options.keepBindings, true, 'UI recovery preserves binding controls');
            events.push('clear');
        },
        status: (value: unknown) => events.push('status:' + value)
    };
    for (const kind of [66, 67]) {
        for (const value of [undefined, null, false, 0, NaN, '', Symbol('callback value'), {exact: true}]) {
            events.length = 0;
            equal(await complete(kind, {operation: 'evaluate', callback: () => value}, execution), value,
                  'successful callback value remains uncoerced');
            equal(events.join(','), 'begin,callback,finish', 'successful request finalisation order');
        }
        events.length = 0;
        admitted = null;
        equal(await complete(kind, {operation: 'evaluate', callback: () => 'must not run'}, execution), undefined,
              'rejected admission returns undefined');
        equal(events.join(','), 'begin', 'rejected admission does not callback or finalise');
        admitted = owner;
    }

    const beginFailure = new Error('admission failed'), originalBegin = execution.begin;
    execution.begin = function failAdmission(): never {
        events.push('begin-failed');
        throw beginFailure;
    };
    for (const kind of [66, 67]) {
        events.length = 0;
        await rejected(complete(kind, {operation: 'evaluate', callback: () => 1}, execution), beginFailure,
                       'failed admission preserves exception');
        equal(events.join(','), 'begin-failed', 'failed admission has no allocated request to finalise');
    }
    execution.begin = originalBegin;

    for (const failure of [undefined, null, false, 0, '', Symbol('thrown value'), {exact: true}]) {
        events.length = 0;
        await rejected(complete(66, {operation: 'evaluate', callback: () => { throw failure; }}, execution), failure,
                       'ordinary callback rejection');
        equal(events.join(','), 'begin,callback,current,finish', 'ordinary error checks ownership before cleanup');
    }
    let errorReads = 0;
    const unreadError = {
        get name() { throw new Error('Abort name must not be inspected'); },
        get message() { throw new Error('Message must not be inspected'); },
        toString() {
            ++errorReads;
            throw new Error('Stale error must not be formatted');
        }
    };
    current = false;
    for (const kind of [66, 67]) {
        events.length = 0;
        equal(await complete(kind, {operation: 'evaluate', callback: () => { throw unreadError; }}, execution), undefined,
              'stale rejection is suppressed');
        equal(events.join(','), 'begin,callback,current,finish', 'stale rejection still finalises');
        equal(errorReads, 0, 'stale exception getters are lazy');
    }
    current = true;
    events.length = 0;
    const displayError = {
        get name() { throw new Error('UI path must not inspect abort name'); },
        get message() { throw new Error('UI path must not inspect message separately'); },
        toString() { ++errorReads; return 'display error'; }
    };
    equal(await complete(67, {operation: 'evaluate', callback: () => { throw displayError; }}, execution), undefined,
          'handled UI rejection returns undefined');
    equal(errorReads, 1, 'current UI error is stringified exactly once');
    equal(events.join(','), 'begin,callback,current,error:display error,digits,clear,status:Error,finish',
          'UI recovery order precedes finalisation');

    const recoveryFailure = new Error('recovery failed'), originalRender = execution.renderError;
    execution.renderError = function failRecovery(): never {
        events.push('render-failed');
        throw recoveryFailure;
    };
    events.length = 0;
    await rejected(complete(67, {operation: 'evaluate', callback: () => { throw displayError; }}, execution),
                   recoveryFailure, 'recovery failure reaches outer request catch');
    equal(events.join(','), 'begin,callback,current,render-failed,current,finish',
          'recovery failure checks ownership again and still finalises');
    execution.renderError = function supersedeRecovery(): never {
        events.push('render-stale');
        current = false;
        throw recoveryFailure;
    };
    events.length = 0;
    equal(await complete(67, {operation: 'evaluate', callback: () => { throw displayError; }}, execution), undefined,
          'superseded recovery failure is suppressed by ordinary outer guard');
    equal(events.join(','), 'begin,callback,current,render-stale,current,finish',
          'superseded recovery still finalises');
    current = true;
    execution.renderError = originalRender;

    const conversionFailure = new Error('conversion failed');
    function failConversion(): never {
        throw {
            toString(): never {
                throw conversionFailure;
            }
        };
    }
    events.length = 0;
    await rejected(complete(67, {operation: 'evaluate', callback: failConversion}, execution),
                   conversionFailure, 'error conversion failure is preserved');
    equal(events.join(','), 'begin,callback,current,current,finish', 'conversion failure still finalises');

    ownershipFailure = new Error('ownership failed');
    for (const kind of [66, 67]) {
        events.length = 0;
        await rejected(complete(kind, {operation: 'evaluate', callback: () => { throw unreadError; }}, execution),
                       ownershipFailure, 'ownership-check failure');
        equal(events.filter(value => value === 'finish').length, 1, 'ownership failure finalises exactly once');
    }
    ownershipFailure = null;
    finishFailure = new Error('finish failed');
    for (const kind of [66, 67]) {
        events.length = 0;
        await rejected(complete(kind, {operation: 'evaluate', callback: () => 42}, execution), finishFailure,
                       'finaliser failure overrides successful callback');
        equal(events.join(','), 'begin,callback,finish', 'failed finaliser is not retried');
    }
    events.length = 0;
    await rejected(complete(66, {operation: 'evaluate', callback: () => { throw unreadError; }}, execution),
                   finishFailure, 'finaliser failure overrides callback rejection');
    finishFailure = null;

    let response: {ok: boolean}, data: unknown, assertion = 0, failAssertion = 0;
    const obsolete = new DOMException('Obsolete Lab request', 'AbortError');
    const posting = {
        assertCurrent() {
            events.push('assert');
            if (++assertion === failAssertion)
                throw obsolete;
        },
        endpoint(request: unknown) {
            equal(request, owner, 'POST endpoint resolves admitted request');
            events.push('endpoint');
            return '/native-endpoint';
        },
        async fetch(endpoint: string, frame: {request?: unknown}) {
            equal(endpoint, frame.request ? '/native-endpoint' : '/fallback', 'POST endpoint selection');
            events.push('fetch');
            return response;
        },
        async decode(value: unknown) {
            equal(value, response, 'decode receives exact response');
            events.push('decode');
            return data;
        },
        presentation(frame: {data: unknown}) {
            equal(frame.data, data, 'presentation retains payload identity');
            events.push('presentation');
        },
        syntax(frame: {data: unknown}) {
            equal(frame.data, data, 'syntax retains payload identity');
            events.push('syntax');
        },
        result: (frame: {response: unknown; data: unknown}) => ({response: frame.response, data: frame.data}),
        invalidEndpoint() { throw new Error('Invalid native request endpoint'); }
    };
    for (const ok of [undefined, null, false, true, 0, '', 'no']) {
        for (const httpOk of [false, true]) {
            events.length = 0;
            assertion = 0;
            response = {ok: httpOk};
            data = {ok};
            const result = await complete(65, {request: owner, payload: {}}, posting);
            equal(result.response, response, 'POST preserves Response identity');
            equal(result.data, data, 'POST preserves decoded object identity');
            equal(events.join(','), 'assert,endpoint,fetch,assert,decode,assert' +
                  (httpOk && ok !== false ? ',presentation,syntax' : ''), 'strict false metadata publication policy');
        }
    }
    for (const missing of [undefined, null]) {
        data = missing;
        response = {ok: true};
        events.length = 0;
        assertion = 0;
        const result = await complete(65, {request: null, payload: {}, fallback: '/fallback'}, posting);
        equal(result.data, missing, 'null and undefined decoded values remain distinct');
        equal(events.join(','), 'assert,fetch,assert,decode,assert,presentation,syntax', 'fallback POST order');
    }
    response = {ok: false};
    data = {get ok() { throw new Error('HTTP failure must not inspect data.ok'); }};
    await complete(65, {request: owner}, posting);
    response = {ok: true};
    data = {ok: true};
    for (failAssertion = 1; failAssertion <= 3; ++failAssertion) {
        events.length = 0;
        assertion = 0;
        await rejected(complete(65, {request: owner}, posting), obsolete, 'POST stale guard');
        check(!events.includes('presentation') && !events.includes('syntax'), 'stale POST never publishes metadata');
        equal(events.includes('decode'), failAssertion === 3, 'staleness is checked before and after decoding');
    }
    failAssertion = 0;
    const native = labWire.exports();
    equal(native.lab_transport_policy(0, 2, 2), 3, 'Evaluate on main selects button and invalidation');
    equal(native.lab_transport_policy(1, 2, 2), 2, 'other main request selects invalidation only');
    equal(native.lab_transport_policy(0, 3, 2), 1, 'Evaluate button policy is independent of channel');
    equal(native.lab_transport_policy(999, 3, 2), 0, 'background cancellation selects no main UI effects');
};
