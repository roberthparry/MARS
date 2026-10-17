/**
 * @file checks.ts
 * @brief Browser transport, native bridge and DOM regression fixtures for the private Lab.
 *
 * Loads the actual transport module in a temporary fixture and checks binary validation, worker
 * ownership, encrypted request sequencing and native-controlled browser effects. Assertions report
 * only to a fixture-owned loopback collector; this module exports no production API.
 */

// Injected only into the temporary browser fixture, after the actual application module.
/** @brief Load the fixture-served transport with its local production module contract. */
function loadTransport(): Promise<typeof import('../../typescript/transport.js')> {
    const url = new URL('./js/transport.js', document.baseURI);
    return import(url.href) as Promise<typeof import('../../typescript/transport.js')>;
}
const {tbaWire, tbaFetch, tbaUI, tbaEncrypted} = await loadTransport();
export {};
/** @brief Loopback collector port supplied by the native browser fixture. */
declare const reportPort: number;
/** @brief Binary-envelope fields inspected by the encrypted transport fixture. */
interface Envelope {cancel?: boolean; sequence: number; final: boolean; ciphertext: string; wrapped_key: string;}
/** @brief Find a required fixture element by identifier, failing if it is absent. */
function fixtureNode<T extends HTMLElement = HTMLElement>(id: string): T {
    const node = document.getElementById(id);
    if (!node) throw new Error('Missing fixture node: ' + id);
    return node as T;
}
/** @brief Find a required fixture element by selector, failing if it is absent. */
function fixtureQuery<T extends Element = HTMLElement>(selector: string): T {
    const node = document.querySelector<T>(selector);
    if (!node) throw new Error('Missing fixture selection: ' + selector);
    return node;
}
/** @brief Resolve a named form control and verify its supported DOM type. */
function fixtureField(form: HTMLFormElement, name: string): HTMLInputElement | HTMLSelectElement {
    const field = form.elements.namedItem(name);
    if (!(field instanceof HTMLInputElement) && !(field instanceof HTMLSelectElement)) throw new Error('Missing fixture control: ' + name);
    return field;
}
/** @brief Require a decoded fixture result to be a non-array object. */
function fixtureRecord(value: unknown): Record<string, unknown> {
    if (typeof value !== 'object' || value === null || Array.isArray(value)) throw new Error('Expected fixture result object');
    return value as Record<string, unknown>;
}

const base = '/to-be-announced';
let checks = 0;
/** @brief Count a successful assertion or terminate the fixture with its diagnostic. */
function assert(condition: unknown, message: string): asserts condition {
    if (!condition) throw new Error(message);
    ++checks;
}
/** @brief Verify that a synchronous fixture operation throws. */
function rejects(callback: () => unknown, message: string) {
    let rejected = false;
    try { callback(); } catch (_) { rejected = true; }
    assert(rejected, message);
}
/** @brief Poll a browser condition for a bounded duration before reporting failure. */
async function waitFor(predicate: () => unknown, message: string) {
    for (let attempt = 0; attempt < 300; ++attempt) {
        if (predicate()) return;
        await new Promise(resolve => setTimeout(resolve, 100));
    }
    throw new Error(message);
}

/** @brief Verify that an asynchronous fixture operation rejects. */
async function rejectsAsync(callback: () => Promise<unknown>, message: string) {
    let rejected = false;
    try { await callback(); } catch (_) { rejected = true; }
    assert(rejected, message);
}

/** @brief Exercise encrypted chunk sequencing and failure handling with a restored fetch adapter. */
async function checkEncryption() {
    const payload = {operation: 'metadata', date_column: 'DATE', value_column: 'value',
        data: {headers: ['DATE', 'value'], rows: [] as string[][]}};
    for (let i = 0; i < 1400; ++i)
        payload.data.rows.push([`${2020 + Math.floor(i / 12)}-${String(i % 12 + 1).padStart(2, '0')}-01`, String(i)]);
    const nativeFetch = globalThis.fetch;
    let mode = '', inFlight = 0, maximum = 0;
    const packets: Envelope[] = [];
    globalThis.fetch = async (url, options = {}) => {
        if (!String(url).endsWith('/stream/chunk')) return nativeFetch(url, options);
        if (!(options.body instanceof Uint8Array)) throw new Error('Expected binary fixture body');
        const frame = tbaWire.decode(options.body as Uint8Array<ArrayBuffer>) as unknown as Envelope;
        if (frame.cancel) return nativeFetch(url, options);
        packets.push(frame);
        assert(!('data' in frame) && !('rows' in frame) && typeof frame.ciphertext === 'string',
               'only encrypted envelopes cross the network');
        if (mode === 'sequence') frame.sequence += 1;
        if (mode === 'final') frame.final = !frame.final;
        if (mode === 'tag') frame.ciphertext = (frame.ciphertext[0] === 'A' ? 'B' : 'A') + frame.ciphertext.slice(1);
        if (mode === 'key') frame.wrapped_key = btoa(String.fromCharCode(...crypto.getRandomValues(new Uint8Array(256))));
        ++inFlight;
        maximum = Math.max(maximum, inFlight);
        try {
            const response = await nativeFetch(url, {...options, body: tbaWire.encode(frame)});
            if (mode === 'response' && frame.final && response.ok) {
                const data = tbaWire.decode(await response.arrayBuffer()) as {ciphertext: string};
                data.ciphertext = (data.ciphertext[0] === 'A' ? 'B' : 'A') + data.ciphertext.slice(1);
                return new Response(tbaWire.encode(data), {headers: {'Content-Type': 'application/x-protobuf'}});
            }
            if (mode === 'truncate' && frame.final && response.ok) {
                const bytes = new Uint8Array(await response.arrayBuffer());
                return new Response(bytes.slice(0, -1), {headers: {'Content-Type': 'application/x-protobuf'}});
            }
            return response;
        } finally {
            --inFlight;
        }
    };
    try {
        const result = fixtureRecord(await tbaEncrypted(`${base}/stream`, payload));
        assert(result.ok && packets.length > 1 && maximum === 1,
               'multiple chunks interoperate with native crypto and apply backpressure');
        const replay = await tbaFetch(`${base}/stream/chunk`, {method: 'POST', body: packets.at(-1)});
        assert(replay.status === 400, 'completed envelopes cannot be replayed');
        for (mode of ['sequence', 'final', 'tag', 'key', 'response', 'truncate']) {
            await rejectsAsync(() => tbaEncrypted(`${base}/stream`, payload), 'reject modified ' + mode);
        }
        mode = '';
        const recovered = fixtureRecord(await tbaEncrypted(`${base}/stream`, payload));
        assert(recovered.ok, 'rejected streams release capacity for a clean request');
        const aborted = new AbortController();
        aborted.abort();
        await rejectsAsync(() => tbaEncrypted(`${base}/stream`, payload, {signal: aborted.signal}),
                           'cancelled calls never begin transmission');
        await rejectsAsync(() => tbaEncrypted(`${base}/stream`, {text: 'x'.repeat(2097153)}),
                           'oversized plaintext is rejected before transmission');
        await rejectsAsync(() => tbaEncrypted('http://example.invalid/stream', payload),
                           'nonlocal plaintext and cross-origin destinations are prohibited');
    } finally {
        globalThis.fetch = nativeFetch;
    }
    for (const path of ['/upload-target', '/upload-xreg', '/forecast', '/state', '/target-columns']) {
        const response = await tbaFetch(base + path, {method: 'POST', body: {data: 'must not be accepted'}});
        assert(response.status === 410, 'retired data route rejects input: ' + path);
    }
    for (const path of ['/download-summary', '/fetch-forecast']) {
        const response = await tbaFetch(base + path);
        assert(response.status === 410, 'no server-side saved result: ' + path);
    }
}
let report;
try {
    await loadTransport();
    assert(!fixtureNode('status').textContent!.includes('Startup failed'), 'native UI initialisation succeeds');
    const wasm = tbaWire.exports();
    assert(wasm.tba_browser_abi_version() === 3 && wasm.tba_codec_abi_version() === 1,
           'WASM instantiated with the supported application and codec imports');
    assert(wasm.tba_model_season(2, 4, 6) === 6, 'native suggestion helper returns the expected value');
    assert(wasm.tba_model_order(5, 4, 0) === 2, 'native ordering helper returns the expected value');
    assert(wasm.tba_period_end(20240110, 3, 1) === 20240331, 'native date helper returns the expected boundary');
    assert(wasm.tba_period_add(20240228, 1, 2, 0) === 20240301, 'leap day in WASM');
    const sample = {text: '£ – <tag> Ελληνικά', values: [null, true, false, 1.25], nested: {empty: ''}};
    const workerBytes = tbaWire.encode({text: 'μ'.repeat(100000)});
    const workerCopy = await tbaWire.decodeAsync(workerBytes) as {text: string};
    assert(workerCopy.text.length === 100000 && workerBytes.byteLength > 0, 'isolated codec preserves owned input');
    assert(tbaWire.background().status().completed > 0, 'large response uses an isolated worker');
    const workerAbort = new AbortController();
    const workerPending = tbaWire.decodeAsync(workerBytes, workerAbort.signal).then(() => false,
        error => error.name === 'AbortError');
    workerAbort.abort();
    assert(await workerPending, 'worker cancellation rejects pending completion');
    assert((await tbaWire.decodeAsync(workerBytes) as {text: string}).text.length === 100000, 'worker recovers after cancellation');
    assert(!tbaWire.background().status().count && !tbaWire.background().status().bytes, 'worker budget released');
    const copy = tbaWire.decode(tbaWire.encode(sample)) as typeof sample;
    assert(copy.text === sample.text && copy.values[3] === 1.25 && copy.nested.empty === '', 'typed wire round trip');
    rejects(() => tbaWire.decode(new Uint8Array([8, 2])), 'wrong protocol version rejected');
    rejects(() => tbaWire.decode(tbaWire.encode(sample).slice(0, -1)), 'truncated envelope rejected');
    rejects(() => tbaWire.encode({value: NaN}), 'non-finite value rejected');
    rejects(() => tbaWire.encode({text: 'x'.repeat(wasm.tba_browser_message_limit() + 1)}), 'oversized encode rejected');
    const pollution: Record<string, unknown> = Object.create(null);
    pollution.__proto__ = {polluted: true};
    const safe = tbaWire.decode(tbaWire.encode(pollution));
    assert(Object.getPrototypeOf(safe) === null && !Object.hasOwn(Object.prototype, 'polluted'), 'prototype keys cannot mutate prototypes');
    const response = await tbaFetch(`${base}/state`);
    const state = fixtureRecord(await response.tbaData());
    assert(response.ok && typeof state.models === 'string', 'native state decodes through C WASM');
    const rejected = await fetch(`${base}/state`, {
        method: 'POST', headers: {'Content-Type': 'application/json'}, body: '{}',
    });
    assert(rejected.status === 410, 'legacy JSON request rejected');
    await checkEncryption();
    const csv = ['DATE,value'];
    for (let i = 0; i < 36; ++i) {
        csv.push(`${2023 + Math.floor(i / 12)}-${String(i % 12 + 1).padStart(2, '0')}-01,${100 + i * 2 + i % 3}`);
    }
    const upload = fixtureNode<HTMLInputElement>('target-upload');
    const transfer = new DataTransfer();
    transfer.items.add(new File([csv.join('\n') + '\n'], 'local.csv', {type: 'text/csv'}));
    upload.files = transfer.files;
    upload.dispatchEvent(new Event('change', {bubbles: true}));
    const form = fixtureNode<HTMLFormElement>('forecast-form');
    await waitFor(() => fixtureField(form, 'target_path').value === 'local', 'Local selection did not complete');
    await waitFor(() => fixtureNode('status').textContent!.includes('Encrypted request verified'), 'Encrypted metadata did not complete');
    assert(fixtureField(form, 'target_date_column').value === 'DATE', 'local field selection is rendered');
    assert(Array.from((fixtureField(form, 'target_value_column') as HTMLSelectElement).options).some(option => option.value === 'value'),
           'local columns remain available after encrypted metadata');
    const fields = {
        target_value_column: 'value', xreg_path: '', xreg_columns: '', models: 'arima',
        frequency: 'monthly', year_type: 'calendar', forecast_end_date: '2026-01-31',
        p: '1', d: '1', q: '0', P: '0', D: '0', Q: '0', season_period: '0', criterion: 'aic', level: '0.95',
    };
    for (const [name, value] of Object.entries(fields)) fixtureField(form, name).value = value;
    document.querySelectorAll<HTMLInputElement>('input[name="model-choice"]').forEach(input => { input.checked = input.value === 'arima'; });
    document.querySelectorAll<HTMLInputElement>('input[name="xreg-column-choice"]').forEach(input => { input.checked = false; });
    form.dispatchEvent(new Event('submit', {bubbles: true, cancelable: true}));
    await waitFor(() => !fixtureNode<HTMLButtonElement>('run-button').disabled, 'native request did not finish');
    assert(!fixtureNode('status').classList.contains('error'),
           'native request failed: ' + fixtureNode('status').textContent);
    assert(fixtureNode('forecast-body').children.length > 0, 'native result rendered after Protobuf response');
    assert(fixtureNode('summary-box').textContent!.trim().length > 0, 'native summary preserved');
    assert(document.querySelectorAll<HTMLElement>('.forecast-chart-svg .chart-model-line').length === 1,
           'typed values render a chart through WASM geometry');
    const lower = fixtureQuery<HTMLInputElement>('input[data-series-id="confidence-lower"]');
    lower.checked = true;
    lower.dispatchEvent(new Event('change', {bubbles: true}));
    assert(Array.from(document.querySelectorAll<HTMLElement>('[data-confidence-kind="lower"]')).some(node => node.style.display === ''),
           'secondary series controls retain browser interaction');
    assert(wasm.tba_model_warning(8, 1, 0, 12, 1, 0, 0) === 1, 'native warning helper returns the expected value');
    assert(wasm.tba_period_options(20251231, 20260331, 2, 0) === 3, 'native option helper returns the expected value');
    document.querySelectorAll<HTMLInputElement>('input[name="model-choice"]').forEach(input => {
        input.checked = input.value === 'arima' || input.value === 'sarima';
    });
    form.dispatchEvent(new Event('submit', {bubbles: true, cancelable: true}));
    await waitFor(() => !fixtureNode<HTMLButtonElement>('run-button').disabled, 'multi-series request did not finish');
    assert(!fixtureNode('status').classList.contains('error'),
           'native workers complete without a scratch executable');
    assert(document.querySelectorAll<HTMLElement>('.comparison-tab').length === 2 &&
           document.querySelectorAll<HTMLElement>('.comparison-table-wrap tbody tr').length > 0,
           'native grouped summaries and joined tables render');
    assert(document.querySelectorAll<HTMLElement>('.forecast-chart-svg .chart-model-line').length === 2,
           'two series share the WASM chart scale');
    const secondTab = document.querySelectorAll<HTMLElement>('.comparison-tab')[1];
    secondTab.click();
    assert(secondTab.getAttribute('aria-selected') === 'true', 'comparison tabs remain interactive');
    const retired = await fetch(`${base}/js/app.js`);
    assert(retired.status === 404, 'the retired application JavaScript is not served');
    fixtureNode('chart-toggle').click();
    assert(!fixtureNode('chart-pane').classList.contains('hidden') &&
           fixtureQuery('.layout').classList.contains('chart-mode'), 'C switches to the chart pane');
    const modelToggle = fixtureQuery<HTMLInputElement>('input[data-series-id="model-0"]');
    const lowerToggle = fixtureQuery<HTMLInputElement>('input[data-series-id="confidence-lower"]');
    lowerToggle.checked = true;
    lowerToggle.dispatchEvent(new Event('change', {bubbles: true}));
    modelToggle.checked = false;
    modelToggle.dispatchEvent(new Event('change', {bubbles: true}));
    assert(Array.from(document.querySelectorAll<HTMLElement>('[data-chart-parent-id="model-0"]')).every(node =>
        node.style.display === 'none'), 'hidden parent series also hides its child paths');
    modelToggle.checked = true;
    modelToggle.dispatchEvent(new Event('change', {bubbles: true}));
    const hover = fixtureQuery('.chart-hover-path[data-chart-series-id="model-0"]');
    const rect = hover.getBoundingClientRect();
    hover.dispatchEvent(new MouseEvent('mousemove', {bubbles: true, clientX: rect.x + 4, clientY: rect.y + 4}));
    assert(!fixtureQuery('.chart-tooltip').classList.contains('hidden') &&
           fixtureQuery('.chart-hover-marker').getAttribute('visibility') === 'visible',
           'C hover hit testing displays a tooltip and marker');
    fixtureNode('forecast-chart-body').dispatchEvent(new MouseEvent('mouseleave'));
    assert(fixtureQuery('.chart-tooltip').classList.contains('hidden'), 'C clears the hover tooltip');
    fixtureNode('help-toggle').click();
    assert(!fixtureNode('help-pane').classList.contains('hidden') &&
           !fixtureQuery('.layout').classList.contains('chart-mode'), 'C switches from chart to help');
    fixtureNode('help-toggle').click();
    assert(!fixtureQuery('.result-grid').classList.contains('hidden'), 'C returns to result cards');
    const order = fixtureField(form, 'p');
    order.value = '5';
    order.dispatchEvent(new Event('input', {bubbles: true}));
    modelToggle.dispatchEvent(new Event('change', {bubbles: true}));
    assert(order.value === '5', 'editing a numeric control does not lose its value');
    fixtureNode('suggest-button').click();
    assert(String(order.value) === '1' && fixtureNode('status').textContent!.includes('Suggested'),
           'explicit C suggestions replace edited orders');
    const priorTarget = fixtureField(form, 'target_path').value;
    tbaUI.call('tba_ui_response', 22, 999999, {ok: true, value_columns: ['stale']}, true);
    assert(fixtureField(form, 'target_path').value === priorTarget &&
           fixtureField(form, 'target_value_column').value === 'value', 'stale metadata replies are ignored');
    const escaped = '<img src=x onerror=alert(1)>';
    tbaUI.call('tba_view_chart', {labels: [escaped, 'next', 'last'], dates: [1, 2, 3],
        actual: [1, null, 3], series: [{label: escaped, values: [2, null, 4], lower: [], upper: []}], end: 3});
    assert(!fixtureNode('forecast-chart-body').querySelector('img') &&
           fixtureQuery('.chart-legend').textContent!.includes(escaped), 'C chart labels are escaped');
    const gapPath = fixtureQuery('.chart-model-line').getAttribute('d') ?? '';
    assert((gapPath.match(/M/g) || []).length === 2, 'missing chart observations break the path');
    tbaUI.call('tba_view_chart', {labels: [], dates: [], actual: [], series: []});
    assert(fixtureNode('forecast-chart-body').textContent!.includes('No plottable'),
           'empty chart produces a readable native message');
    await new Promise(resolve => setTimeout(resolve, 600));
    const saved = fixtureRecord(await (await tbaFetch(`${base}/state`)).tbaData());
    assert(saved.p === 1 && saved.target_value_column === '', 'server defaults never retain browser settings');
    const driverUpload = fixtureNode<HTMLInputElement>('xreg-upload');
    const drivers = new DataTransfer();
    drivers.items.add(new File([csv.join('\n') + '\n'], 'drivers.csv', {type: 'text/csv'}));
    driverUpload.files = drivers.files;
    driverUpload.dispatchEvent(new Event('change', {bubbles: true}));
    await waitFor(() => fixtureNode('status').textContent!.includes('Encrypted request verified'),
                  'Second local selection did not complete');
    const driverChoice = fixtureQuery<HTMLInputElement>('input[name="xreg-column-choice"]');
    assert(driverChoice && !driverChoice.disabled && fixtureField(form, 'xreg_date_column').value === 'DATE',
           'C renders local metadata and selectable columns');
    driverChoice.checked = true;
    driverChoice.dispatchEvent(new Event('change', {bubbles: true}));
    assert(fixtureField(form, 'xreg_columns').value === 'value', 'C synchronises dependent selections');
    fixtureField(form, 'criterion').value = 'bic';
    fixtureField(form, 'level').value = '0.9';
    const arima = fixtureQuery<HTMLInputElement>('input[name="model-choice"][value="arima"]');
    document.querySelectorAll<HTMLInputElement>('input[name="model-choice"]').forEach(input => { input.checked = input === arima; });
    arima.dispatchEvent(new Event('change', {bubbles: true}));
    assert(fixtureField(form, 'criterion').value === 'bic' && fixtureField(form, 'level').value === '0.9',
           'automatic suggestions preserve explicitly selected settings');
    await new Promise(resolve => setTimeout(resolve, 600));
    report = `BROWSER PASS: ${checks} WASM, Protobuf and application checks`;
} catch (error) {
    report = 'BROWSER FAIL: ' + (error instanceof Error ? error.message + '\n' + (error.stack || '') : String(error));
}
// Report to a fixture-only loopback collector, never an application persistence route.
try {
    await fetch(`http://127.0.0.1:${reportPort}/result`, {method: 'POST', mode: 'no-cors', body: report});
} catch (error) {
    console.error(report, error);
}
