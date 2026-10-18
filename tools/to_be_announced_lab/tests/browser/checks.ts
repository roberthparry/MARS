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
interface Envelope {
    cancel?: boolean;
    sequence: number;
    final: boolean;
    ciphertext: string;
    wrapped_key: string;
}

/** @brief Encrypted response field modified by the transport fixture. */
interface EncryptedResponse {
    ciphertext: string;
}

/** @brief Decoded object whose fields are checked by individual assertions. */
interface FixtureRecord {
    [key: string]: unknown;
}

/** @brief Text returned by the isolated codec worker. */
interface TextResult {
    text: string;
}

/** @brief Find a required fixture element by identifier, failing if it is absent. */
function fixtureNode(id: string): HTMLElement {
    const node = document.getElementById(id);
    if (!node) {
        throw new Error('Missing fixture node: ' + id);
    }
    return node;
}

/** @brief Find a required fixture element by selector, failing if it is absent. */
function fixtureQuery(selector: string): Element {
    const node = document.querySelector(selector);
    if (!node) {
        throw new Error('Missing fixture selection: ' + selector);
    }
    return node;
}

/** @brief Resolve a named form control and verify its supported DOM type. */
function fixtureField(form: HTMLFormElement, name: string): HTMLInputElement | HTMLSelectElement {
    const field = form.elements.namedItem(name);
    if (!(field instanceof HTMLInputElement) && !(field instanceof HTMLSelectElement)) {
        throw new Error('Missing fixture control: ' + name);
    }
    return field;
}

/** @brief Require a decoded fixture result to be a non-array object. */
function fixtureRecord(value: unknown): FixtureRecord {
    if (typeof value !== 'object' || value === null || Array.isArray(value)) {
        throw new Error('Expected fixture result object');
    }
    return value as FixtureRecord;
}

const base = '/to-be-announced';
let checks = 0;

/** @brief Count a successful assertion or terminate the fixture with its diagnostic. */
function assert(condition: unknown, message: string): asserts condition {
    if (!condition) {
        throw new Error(message);
    }
    ++checks;
}

/** @brief Verify that a synchronous fixture operation throws. */
function rejects(callback: () => unknown, message: string): void {
    let rejected = false;
    try {
        callback();
    } catch {
        rejected = true;
    }
    assert(rejected, message);
}

/** @brief Wait for the fixture's existing browser timer interval. */
function delay(milliseconds: number): Promise<void> {
    return new Promise(resolve => setTimeout(resolve, milliseconds));
}

/** @brief Poll a browser condition for a bounded duration before reporting failure. */
async function waitFor(predicate: () => unknown, message: string): Promise<void> {
    for (let attempt = 0; attempt < 300; ++attempt) {
        if (predicate()) {
            return;
        }
        await delay(100);
    }
    throw new Error(message);
}

/** @brief Verify that an asynchronous fixture operation rejects. */
async function rejectsAsync(callback: () => Promise<unknown>, message: string): Promise<void> {
    let rejected = false;
    try {
        await callback();
    } catch {
        rejected = true;
    }
    assert(rejected, message);
}

/** @brief Change the first encoded character while preserving the rest of the string. */
function corruptCiphertext(ciphertext: string): string {
    let firstCharacter = 'A';
    if (ciphertext[0] === 'A') {
        firstCharacter = 'B';
    }
    return firstCharacter + ciphertext.slice(1);
}

/** @brief Exercise encrypted chunk sequencing and failure handling with a restored fetch adapter. */
async function checkEncryption(): Promise<void> {
    const rows: string[][] = [];
    const payload = {
        operation: 'metadata',
        date_column: 'DATE',
        value_column: 'value',
        data: {headers: ['DATE', 'value'], rows},
    };
    for (let i = 0; i < 1400; ++i) {
        const date = `${2020 + Math.floor(i / 12)}-${String(i % 12 + 1).padStart(2, '0')}-01`;
        rows.push([date, String(i)]);
    }
    const nativeFetch = globalThis.fetch;
    let mode = '';
    let inFlight = 0;
    let maximum = 0;
    const packets: Envelope[] = [];

    /** @brief Intercept chunk traffic and apply the selected failure before forwarding it. */
    async function interceptChunk(url: RequestInfo | URL, options: RequestInit = {}): Promise<Response> {
        if (!String(url).endsWith('/stream/chunk')) {
            return nativeFetch(url, options);
        }
        if (!(options.body instanceof Uint8Array)) {
            throw new Error('Expected binary fixture body');
        }
        const frame = tbaWire.decode(options.body as Uint8Array<ArrayBuffer>) as unknown as Envelope;
        if (frame.cancel) {
            return nativeFetch(url, options);
        }
        packets.push(frame);
        assert(!('data' in frame) && !('rows' in frame) && typeof frame.ciphertext === 'string',
               'only encrypted envelopes cross the network');
        if (mode === 'sequence') {
            frame.sequence += 1;
        }
        if (mode === 'final') {
            frame.final = !frame.final;
        }
        if (mode === 'tag') {
            frame.ciphertext = corruptCiphertext(frame.ciphertext);
        }
        if (mode === 'key') {
            const key = crypto.getRandomValues(new Uint8Array(256));
            frame.wrapped_key = btoa(String.fromCharCode(...key));
        }
        ++inFlight;
        maximum = Math.max(maximum, inFlight);
        try {
            const response = await nativeFetch(url, {...options, body: tbaWire.encode(frame)});
            if (mode === 'response' && frame.final && response.ok) {
                const data = tbaWire.decode(await response.arrayBuffer()) as unknown as EncryptedResponse;
                data.ciphertext = corruptCiphertext(data.ciphertext);
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
    }

    globalThis.fetch = interceptChunk;
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

/** @brief Check startup and the native entry points used by the fixture. */
function checkNativeBridge(): void {
    assert(!fixtureNode('status').textContent!.includes('Startup failed'), 'native UI initialisation succeeds');
    const wasm = tbaWire.exports();
    assert(wasm.tba_browser_abi_version() === 3 && wasm.tba_codec_abi_version() === 1,
           'WASM instantiated with the supported application and codec imports');
    assert(wasm.tba_model_season(2, 4, 6) === 6, 'native suggestion helper returns the expected value');
    assert(wasm.tba_model_order(5, 4, 0) === 2, 'native ordering helper returns the expected value');
    assert(wasm.tba_period_end(20240110, 3, 1) === 20240331, 'native date helper returns the expected boundary');
    assert(wasm.tba_period_add(20240228, 1, 2, 0) === 20240301, 'leap day in WASM');
}

/** @brief Mark an unexpected worker completion as a failed cancellation. */
function decodeCompleted(): boolean {
    return false;
}

/** @brief Recognise the existing worker cancellation diagnostic. */
function decodeAborted(error: Error): boolean {
    return error.name === 'AbortError';
}

/** @brief Check binary round trips, worker ownership and malformed input handling. */
async function checkCodec(): Promise<void> {
    const wasm = tbaWire.exports();
    const sample = {text: '£ – <tag> Ελληνικά', values: [null, true, false, 1.25], nested: {empty: ''}};
    const workerBytes = tbaWire.encode({text: 'μ'.repeat(100000)});
    const workerCopy = await tbaWire.decodeAsync(workerBytes) as unknown as TextResult;
    assert(workerCopy.text.length === 100000 && workerBytes.byteLength > 0, 'isolated codec preserves owned input');
    assert(tbaWire.background().status().completed > 0, 'large response uses an isolated worker');
    const workerAbort = new AbortController();
    const workerPending = tbaWire.decodeAsync(workerBytes, workerAbort.signal).then(decodeCompleted, decodeAborted);
    workerAbort.abort();
    assert(await workerPending, 'worker cancellation rejects pending completion');
    const recovered = await tbaWire.decodeAsync(workerBytes) as unknown as TextResult;
    assert(recovered.text.length === 100000, 'worker recovers after cancellation');
    assert(!tbaWire.background().status().count && !tbaWire.background().status().bytes, 'worker budget released');
    const copy = tbaWire.decode(tbaWire.encode(sample)) as typeof sample;
    assert(copy.text === sample.text && copy.values[3] === 1.25 && copy.nested.empty === '', 'typed wire round trip');
    rejects(() => tbaWire.decode(new Uint8Array([8, 2])), 'wrong protocol version rejected');
    rejects(() => tbaWire.decode(tbaWire.encode(sample).slice(0, -1)), 'truncated envelope rejected');
    rejects(() => tbaWire.encode({value: NaN}), 'non-finite value rejected');
    rejects(() => tbaWire.encode({text: 'x'.repeat(wasm.tba_browser_message_limit() + 1)}), 'oversized encode rejected');
    const pollution: FixtureRecord = Object.create(null);
    pollution.__proto__ = {polluted: true};
    const safe = tbaWire.decode(tbaWire.encode(pollution));
    assert(Object.getPrototypeOf(safe) === null && !Object.hasOwn(Object.prototype, 'polluted'),
           'prototype keys cannot mutate prototypes');
}

/** @brief Check the binary state response and rejection of the retired request format. */
async function checkInitialState(): Promise<void> {
    const response = await tbaFetch(`${base}/state`);
    const state = fixtureRecord(await response.tbaData());
    assert(response.ok && typeof state.models === 'string', 'native state decodes through C WASM');
    const rejected = await fetch(`${base}/state`, {
        method: 'POST', headers: {'Content-Type': 'application/json'}, body: '{}',
    });
    assert(rejected.status === 410, 'legacy JSON request rejected');
}

/** @brief Build the local file contents shared by both browser selections. */
function createLocalCsv(): string[] {
    const csv = ['DATE,value'];
    for (let i = 0; i < 36; ++i) {
        csv.push(`${2023 + Math.floor(i / 12)}-${String(i % 12 + 1).padStart(2, '0')}-01,${100 + i * 2 + i % 3}`);
    }
    return csv;
}

/** @brief Read the native completion message for either local selection. */
function encryptedRequestIsReady(): boolean {
    return fixtureNode('status').textContent!.includes('Encrypted request verified');
}

/** @brief Read the native request completion state without retaining an old button. */
function requestIsFinished(): boolean {
    const button = fixtureNode('run-button') as HTMLButtonElement;
    return !button.disabled;
}

/** @brief Select the first local file and verify its native-rendered controls. */
async function checkLocalSelection(csv: string[]): Promise<HTMLFormElement> {
    const upload = fixtureNode('target-upload') as HTMLInputElement;
    const transfer = new DataTransfer();
    transfer.items.add(new File([csv.join('\n') + '\n'], 'local.csv', {type: 'text/csv'}));
    upload.files = transfer.files;
    upload.dispatchEvent(new Event('change', {bubbles: true}));
    const form = fixtureNode('forecast-form') as HTMLFormElement;

    /** @brief Check whether the native selection has updated the form. */
    function selectionIsReady(): boolean {
        return fixtureField(form, 'target_path').value === 'local';
    }

    await waitFor(selectionIsReady, 'Local selection did not complete');
    await waitFor(encryptedRequestIsReady, 'Encrypted metadata did not complete');
    assert(fixtureField(form, 'target_date_column').value === 'DATE', 'local field selection is rendered');
    const select = fixtureField(form, 'target_value_column') as HTMLSelectElement;
    assert(Array.from(select.options).some(option => option.value === 'value'),
           'local columns remain available after encrypted metadata');
    return form;
}

/** @brief Submit the existing fixture settings and check native-rendered results. */
async function checkRequestResults(form: HTMLFormElement): Promise<void> {
    const fields = {
        target_value_column: 'value',
        xreg_path: '',
        xreg_columns: '',
        models: 'arima',
        frequency: 'monthly',
        year_type: 'calendar',
        forecast_end_date: '2026-01-31',
        p: '1',
        d: '1',
        q: '0',
        P: '0',
        D: '0',
        Q: '0',
        season_period: '0',
        criterion: 'aic',
        level: '0.95',
    };
    for (const [name, value] of Object.entries(fields)) {
        fixtureField(form, name).value = value;
    }
    for (const input of document.querySelectorAll<HTMLInputElement>('input[name="model-choice"]')) {
        input.checked = input.value === 'arima';
    }
    for (const input of document.querySelectorAll<HTMLInputElement>('input[name="xreg-column-choice"]')) {
        input.checked = false;
    }
    form.dispatchEvent(new Event('submit', {bubbles: true, cancelable: true}));
    await waitFor(requestIsFinished, 'native request did not finish');
    assert(!fixtureNode('status').classList.contains('error'),
           'native request failed: ' + fixtureNode('status').textContent);
    assert(fixtureNode('forecast-body').children.length > 0, 'native result rendered after Protobuf response');
    assert(fixtureNode('summary-box').textContent!.trim().length > 0, 'native summary preserved');
    assert(document.querySelectorAll<HTMLElement>('.forecast-chart-svg .chart-model-line').length === 1,
           'typed values render a chart through WASM geometry');
    const lower = fixtureQuery('input[data-series-id="confidence-lower"]') as HTMLInputElement;
    lower.checked = true;
    lower.dispatchEvent(new Event('change', {bubbles: true}));
    const secondaryPaths = document.querySelectorAll<HTMLElement>('[data-confidence-kind="lower"]');
    assert(Array.from(secondaryPaths).some(node => node.style.display === ''),
           'secondary series controls retain browser interaction');
    const wasm = tbaWire.exports();
    assert(wasm.tba_model_warning(8, 1, 0, 12, 1, 0, 0) === 1, 'native warning helper returns the expected value');
    assert(wasm.tba_period_options(20251231, 20260331, 2, 0) === 3, 'native option helper returns the expected value');
    for (const input of document.querySelectorAll<HTMLInputElement>('input[name="model-choice"]')) {
        input.checked = input.value === 'arima' || input.value === 'sarima';
    }
    form.dispatchEvent(new Event('submit', {bubbles: true, cancelable: true}));
    await waitFor(requestIsFinished, 'multi-series request did not finish');
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
}

/** @brief Check chart controls, pointer feedback and pane navigation. */
function checkChartControls(): HTMLInputElement {
    fixtureNode('chart-toggle').click();
    assert(!fixtureNode('chart-pane').classList.contains('hidden') &&
           fixtureQuery('.layout').classList.contains('chart-mode'), 'C switches to the chart pane');
    const modelToggle = fixtureQuery('input[data-series-id="model-0"]') as HTMLInputElement;
    const lowerToggle = fixtureQuery('input[data-series-id="confidence-lower"]') as HTMLInputElement;
    lowerToggle.checked = true;
    lowerToggle.dispatchEvent(new Event('change', {bubbles: true}));
    modelToggle.checked = false;
    modelToggle.dispatchEvent(new Event('change', {bubbles: true}));
    const childPaths = document.querySelectorAll<HTMLElement>('[data-chart-parent-id="model-0"]');
    assert(Array.from(childPaths).every(node => node.style.display === 'none'),
           'hidden parent series also hides its child paths');
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
    return modelToggle;
}

/** @brief Check edited controls, stale replies and safe rendering of unusual values. */
function checkFormUpdates(form: HTMLFormElement, modelToggle: HTMLInputElement): void {
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
    tbaUI.call('tba_view_chart', {
        labels: [escaped, 'next', 'last'],
        dates: [1, 2, 3],
        actual: [1, null, 3],
        series: [{label: escaped, values: [2, null, 4], lower: [], upper: []}],
        end: 3,
    });
    assert(!fixtureNode('forecast-chart-body').querySelector('img') &&
           fixtureQuery('.chart-legend').textContent!.includes(escaped), 'C chart labels are escaped');
    const gapPath = fixtureQuery('.chart-model-line').getAttribute('d') ?? '';
    assert((gapPath.match(/M/g) || []).length === 2, 'missing chart observations break the path');
    tbaUI.call('tba_view_chart', {labels: [], dates: [], actual: [], series: []});
    assert(fixtureNode('forecast-chart-body').textContent!.includes('No plottable'),
           'empty chart produces a readable native message');
}

/** @brief Allow pending browser saves to finish before checking the server defaults. */
async function checkSavedState(): Promise<void> {
    await delay(600);
    const response = await tbaFetch(`${base}/state`);
    const saved = fixtureRecord(await response.tbaData());
    assert(saved.p === 1 && saved.target_value_column === '', 'server defaults never retain browser settings');
}

/** @brief Check the second local selection and preservation of explicit control values. */
async function checkSecondarySelection(form: HTMLFormElement, csv: string[]): Promise<void> {
    const upload = fixtureNode('xreg-upload') as HTMLInputElement;
    const transfer = new DataTransfer();
    transfer.items.add(new File([csv.join('\n') + '\n'], 'drivers.csv', {type: 'text/csv'}));
    upload.files = transfer.files;
    upload.dispatchEvent(new Event('change', {bubbles: true}));
    await waitFor(encryptedRequestIsReady, 'Second local selection did not complete');
    const choice = fixtureQuery('input[name="xreg-column-choice"]') as HTMLInputElement;
    assert(choice && !choice.disabled && fixtureField(form, 'xreg_date_column').value === 'DATE',
           'C renders local metadata and selectable columns');
    choice.checked = true;
    choice.dispatchEvent(new Event('change', {bubbles: true}));
    assert(fixtureField(form, 'xreg_columns').value === 'value', 'C synchronises dependent selections');
    fixtureField(form, 'criterion').value = 'bic';
    fixtureField(form, 'level').value = '0.9';
    const selectedInput = fixtureQuery('input[name="model-choice"][value="arima"]') as HTMLInputElement;
    for (const input of document.querySelectorAll<HTMLInputElement>('input[name="model-choice"]')) {
        input.checked = input === selectedInput;
    }
    selectedInput.dispatchEvent(new Event('change', {bubbles: true}));
    assert(fixtureField(form, 'criterion').value === 'bic' && fixtureField(form, 'level').value === '0.9',
           'automatic suggestions preserve explicitly selected settings');
    await delay(600);
}

let report: string;
try {
    await loadTransport();
    checkNativeBridge();
    await checkCodec();
    await checkInitialState();
    await checkEncryption();
    const csv = createLocalCsv();
    const form = await checkLocalSelection(csv);
    await checkRequestResults(form);
    const modelToggle = checkChartControls();
    checkFormUpdates(form, modelToggle);
    await checkSavedState();
    await checkSecondarySelection(form, csv);
    report = `BROWSER PASS: ${checks} WASM, Protobuf and application checks`;
} catch (error) {
    if (error instanceof Error) {
        report = 'BROWSER FAIL: ' + error.message + '\n' + (error.stack || '');
    } else {
        report = 'BROWSER FAIL: ' + String(error);
    }
}

// Report to a fixture-only loopback collector, never an application persistence route.
try {
    await fetch(`http://127.0.0.1:${reportPort}/result`, {method: 'POST', mode: 'no-cors', body: report});
} catch (error) {
    console.error(report, error);
}
