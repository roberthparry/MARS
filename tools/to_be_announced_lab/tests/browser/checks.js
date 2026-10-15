// Injected only into the temporary browser fixture, after the actual application module.
import {tbaWire, tbaFetch, tbaUI, tbaEncrypted} from './js/transport.js';

const base = '/to-be-announced';
let checks = 0;
function assert(condition, message) {
    if (!condition) throw new Error(message);
    ++checks;
}
function rejects(callback, message) {
    let rejected = false;
    try { callback(); } catch (_) { rejected = true; }
    assert(rejected, message);
}
async function waitFor(predicate, message) {
    for (let attempt = 0; attempt < 300; ++attempt) {
        if (predicate()) return;
        await new Promise(resolve => setTimeout(resolve, 100));
    }
    throw new Error(message);
}

async function rejectsAsync(callback, message) {
    let rejected = false;
    try { await callback(); } catch (_) { rejected = true; }
    assert(rejected, message);
}

async function checkEncryption() {
    const payload = {operation: 'metadata', date_column: 'DATE', value_column: 'value',
        data: {headers: ['DATE', 'value'], rows: []}};
    for (let i = 0; i < 1400; ++i)
        payload.data.rows.push([`${2020 + Math.floor(i / 12)}-${String(i % 12 + 1).padStart(2, '0')}-01`, String(i)]);
    const nativeFetch = globalThis.fetch;
    let mode = '', packets = [], inFlight = 0, maximum = 0;
    globalThis.fetch = async (url, options = {}) => {
        if (!String(url).endsWith('/stream/chunk')) return nativeFetch(url, options);
        let frame = tbaWire.decode(options.body);
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
                const data = tbaWire.decode(await response.arrayBuffer());
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
        const result = await tbaEncrypted(`${base}/stream`, payload);
        assert(result.ok && packets.length > 1 && maximum === 1,
               'multiple chunks interoperate with native crypto and apply backpressure');
        const replay = await tbaFetch(`${base}/stream/chunk`, {method: 'POST', body: packets.at(-1)});
        assert(replay.status === 400, 'completed envelopes cannot be replayed');
        for (mode of ['sequence', 'final', 'tag', 'key', 'response', 'truncate']) {
            await rejectsAsync(() => tbaEncrypted(`${base}/stream`, payload), 'reject modified ' + mode);
        }
        mode = '';
        const recovered = await tbaEncrypted(`${base}/stream`, payload);
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
    for (const path of ['/download-summary', '/download-forecast']) {
        const response = await tbaFetch(base + path);
        assert(response.status === 410, 'no server-side saved result: ' + path);
    }
}
let report;
try {
    await import('./js/transport.js');
    assert(!document.getElementById('status').textContent.includes('Startup failed'), 'native UI initialisation succeeds');
    const wasm = tbaWire.exports();
    assert(wasm.tba_browser_abi_version() === 3, 'WASM instantiated with the supported imports');
    assert(wasm.tba_model_season(2, 4, 6) === 6, 'season suggestions run in WASM');
    assert(wasm.tba_model_order(5, 4, 0) === 2, 'model orders run in WASM');
    assert(wasm.tba_period_end(20240110, 3, 1) === 20240331, 'fiscal quarter in WASM');
    assert(wasm.tba_period_add(20240228, 1, 2, 0) === 20240301, 'leap day in WASM');
    const sample = {text: '£ – <tag> Ελληνικά', values: [null, true, false, 1.25], nested: {empty: ''}};
    const workerBytes = tbaWire.encode({text: 'μ'.repeat(100000)});
    const workerCopy = await tbaWire.decodeAsync(workerBytes);
    assert(workerCopy.text.length === 100000 && workerBytes.byteLength > 0, 'isolated codec preserves owned input');
    assert(tbaWire.background().status().completed > 0, 'large response uses an isolated worker');
    const workerAbort = new AbortController();
    const workerPending = tbaWire.decodeAsync(workerBytes, workerAbort.signal).then(() => false,
        error => error.name === 'AbortError');
    workerAbort.abort();
    assert(await workerPending, 'worker cancellation rejects pending completion');
    assert((await tbaWire.decodeAsync(workerBytes)).text.length === 100000, 'worker recovers after cancellation');
    assert(!tbaWire.background().status().count && !tbaWire.background().status().bytes, 'worker budget released');
    const copy = tbaWire.decode(tbaWire.encode(sample));
    assert(copy.text === sample.text && copy.values[3] === 1.25 && copy.nested.empty === '', 'typed wire round trip');
    rejects(() => tbaWire.decode(new Uint8Array([8, 2])), 'wrong protocol version rejected');
    rejects(() => tbaWire.decode(tbaWire.encode(sample).slice(0, -1)), 'truncated envelope rejected');
    rejects(() => tbaWire.encode({value: NaN}), 'non-finite value rejected');
    rejects(() => tbaWire.encode({text: 'x'.repeat(wasm.lab_browser_message_limit() + 1)}), 'oversized encode rejected');
    const pollution = Object.create(null);
    pollution.__proto__ = {polluted: true};
    const safe = tbaWire.decode(tbaWire.encode(pollution));
    assert(Object.getPrototypeOf(safe) === null && !({}).polluted, 'prototype keys cannot mutate prototypes');
    const response = await tbaFetch(`${base}/state`);
    const state = await response.tbaData();
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
    const upload = document.getElementById('target-upload');
    const transfer = new DataTransfer();
    transfer.items.add(new File([csv.join('\n') + '\n'], 'local.csv', {type: 'text/csv'}));
    upload.files = transfer.files;
    upload.dispatchEvent(new Event('change', {bubbles: true}));
    const form = document.getElementById('forecast-form');
    await waitFor(() => form.elements.namedItem('target_path').value === 'local', 'Local selection did not complete');
    await waitFor(() => document.getElementById('status').textContent.includes('Encrypted request verified'), 'Encrypted metadata did not complete');
    assert(form.elements.namedItem('target_date_column').value === 'DATE', 'Local date column rendered');
    assert(Array.from(form.elements.namedItem('target_value_column').options).some(option => option.value === 'value'),
           'local columns remain available after encrypted metadata');
    const fields = {
        target_value_column: 'value', xreg_path: '', xreg_columns: '', models: 'arima',
        frequency: 'monthly', year_type: 'calendar', forecast_end_date: '2026-01-31',
        p: '1', d: '1', q: '0', P: '0', D: '0', Q: '0', season_period: '0', criterion: 'aic', level: '0.95',
    };
    for (const [name, value] of Object.entries(fields)) form.elements.namedItem(name).value = value;
    document.querySelectorAll('input[name="model-choice"]').forEach(input => { input.checked = input.value === 'arima'; });
    document.querySelectorAll('input[name="xreg-column-choice"]').forEach(input => { input.checked = false; });
    form.dispatchEvent(new Event('submit', {bubbles: true, cancelable: true}));
    await waitFor(() => !document.getElementById('run-button').disabled, 'forecast did not finish');
    assert(!document.getElementById('status').classList.contains('error'),
           'forecast failed: ' + document.getElementById('status').textContent);
    assert(document.getElementById('forecast-body').children.length > 0, 'forecast result rendered after Protobuf response');
    assert(document.getElementById('summary-box').textContent.trim().length > 0, 'forecast summary preserved');
    assert(document.querySelectorAll('.forecast-chart-svg .chart-model-line').length === 1,
           'typed forecast values render a chart through WASM geometry');
    const lower = document.querySelector('input[data-series-id="confidence-lower"]');
    lower.checked = true;
    lower.dispatchEvent(new Event('change', {bubbles: true}));
    assert(Array.from(document.querySelectorAll('[data-confidence-kind="lower"]')).some(node => node.style.display === ''),
           'confidence controls retain browser interaction');
    assert(wasm.tba_model_warning(8, 1, 0, 12, 1, 0, 0) === 1, 'seasonal warnings run in WASM');
    assert(wasm.tba_period_options(20251231, 20260331, 2, 0) === 3, 'horizon option generation runs in WASM');
    document.querySelectorAll('input[name="model-choice"]').forEach(input => {
        input.checked = input.value === 'arima' || input.value === 'sarima';
    });
    form.dispatchEvent(new Event('submit', {bubbles: true, cancelable: true}));
    await waitFor(() => !document.getElementById('run-button').disabled, 'multi-model forecast did not finish');
    assert(!document.getElementById('status').classList.contains('error'),
           'server-owned multi-model workers complete without a scratch executable');
    assert(document.querySelectorAll('.comparison-tab').length === 2 &&
           document.querySelectorAll('.comparison-table-wrap tbody tr').length > 0,
           'native multi-model summaries and joined tables render');
    assert(document.querySelectorAll('.forecast-chart-svg .chart-model-line').length === 2,
           'two model series share the WASM chart scale');
    const secondTab = document.querySelectorAll('.comparison-tab')[1];
    secondTab.click();
    assert(secondTab.getAttribute('aria-selected') === 'true', 'comparison tabs remain interactive');
    const retired = await fetch(`${base}/js/app.js`);
    assert(retired.status === 404, 'the retired application JavaScript is not served');
    document.getElementById('chart-toggle').click();
    assert(!document.getElementById('chart-pane').classList.contains('hidden') &&
           document.querySelector('.layout').classList.contains('chart-mode'), 'C switches to the chart pane');
    const modelToggle = document.querySelector('input[data-series-id="model-0"]');
    const lowerToggle = document.querySelector('input[data-series-id="confidence-lower"]');
    lowerToggle.checked = true;
    lowerToggle.dispatchEvent(new Event('change', {bubbles: true}));
    modelToggle.checked = false;
    modelToggle.dispatchEvent(new Event('change', {bubbles: true}));
    assert(Array.from(document.querySelectorAll('[data-chart-parent-id="model-0"]')).every(node =>
        node.style.display === 'none'), 'hidden model also hides its confidence paths');
    modelToggle.checked = true;
    modelToggle.dispatchEvent(new Event('change', {bubbles: true}));
    const hover = document.querySelector('.chart-hover-path[data-chart-series-id="model-0"]');
    const rect = hover.getBoundingClientRect();
    hover.dispatchEvent(new MouseEvent('mousemove', {bubbles: true, clientX: rect.x + 4, clientY: rect.y + 4}));
    assert(!document.querySelector('.chart-tooltip').classList.contains('hidden') &&
           document.querySelector('.chart-hover-marker').getAttribute('visibility') === 'visible',
           'C hover hit testing displays a tooltip and marker');
    document.getElementById('forecast-chart-body').dispatchEvent(new MouseEvent('mouseleave'));
    assert(document.querySelector('.chart-tooltip').classList.contains('hidden'), 'C clears the hover tooltip');
    document.getElementById('help-toggle').click();
    assert(!document.getElementById('help-pane').classList.contains('hidden') &&
           !document.querySelector('.layout').classList.contains('chart-mode'), 'C switches from chart to help');
    document.getElementById('help-toggle').click();
    assert(!document.querySelector('.result-grid').classList.contains('hidden'), 'C returns to result cards');
    const order = form.elements.namedItem('p');
    order.value = '5';
    order.dispatchEvent(new Event('input', {bubbles: true}));
    modelToggle.dispatchEvent(new Event('change', {bubbles: true}));
    assert(order.value === '5', 'editing a model order does not lose its value');
    document.getElementById('suggest-button').click();
    assert(order.value === '1' && document.getElementById('status').textContent.includes('Suggested'),
           'explicit C suggestions replace edited orders');
    const priorTarget = form.elements.namedItem('target_path').value;
    tbaUI.call('tba_ui_response', 22, 999999, {ok: true, value_columns: ['stale']}, true);
    assert(form.elements.namedItem('target_path').value === priorTarget &&
           form.elements.namedItem('target_value_column').value === 'value', 'stale metadata replies are ignored');
    const escaped = '<img src=x onerror=alert(1)>';
    tbaUI.call('tba_view_chart', {labels: [escaped, 'next', 'last'], dates: [1, 2, 3],
        actual: [1, null, 3], series: [{label: escaped, values: [2, null, 4], lower: [], upper: []}], end: 3});
    assert(!document.getElementById('forecast-chart-body').querySelector('img') &&
           document.querySelector('.chart-legend').textContent.includes(escaped), 'C chart labels are escaped');
    const gapPath = document.querySelector('.chart-model-line').getAttribute('d');
    assert((gapPath.match(/M/g) || []).length === 2, 'missing chart observations break the path');
    tbaUI.call('tba_view_chart', {labels: [], dates: [], actual: [], series: []});
    assert(document.getElementById('forecast-chart-body').textContent.includes('No plottable'),
           'empty chart produces a readable native message');
    await new Promise(resolve => setTimeout(resolve, 600));
    const saved = await (await tbaFetch(`${base}/state`)).tbaData();
    assert(saved.p === 1 && saved.target_value_column === '', 'server defaults never retain browser settings');
    const driverUpload = document.getElementById('xreg-upload');
    const drivers = new DataTransfer();
    drivers.items.add(new File([csv.join('\n') + '\n'], 'drivers.csv', {type: 'text/csv'}));
    driverUpload.files = drivers.files;
    driverUpload.dispatchEvent(new Event('change', {bubbles: true}));
    await waitFor(() => document.getElementById('status').textContent.includes('Encrypted request verified'),
                  'Second local selection did not complete');
    const driverChoice = document.querySelector('input[name="xreg-column-choice"]');
    assert(driverChoice && !driverChoice.disabled && form.elements.namedItem('xreg_date_column').value === 'DATE',
           'C renders local metadata and selectable columns');
    driverChoice.checked = true;
    driverChoice.dispatchEvent(new Event('change', {bubbles: true}));
    assert(form.elements.namedItem('xreg_columns').value === 'value', 'C synchronises driver selections');
    form.elements.namedItem('criterion').value = 'bic';
    form.elements.namedItem('level').value = '0.9';
    const arima = document.querySelector('input[name="model-choice"][value="arima"]');
    document.querySelectorAll('input[name="model-choice"]').forEach(input => { input.checked = input === arima; });
    arima.dispatchEvent(new Event('change', {bubbles: true}));
    assert(form.elements.namedItem('criterion').value === 'bic' && form.elements.namedItem('level').value === '0.9',
           'automatic suggestions preserve explicit criterion and confidence settings');
    await new Promise(resolve => setTimeout(resolve, 600));
    report = `BROWSER PASS: ${checks} WASM, Protobuf and application checks`;
} catch (error) {
    report = 'BROWSER FAIL: ' + (error.message || String(error)) + '\n' + (error.stack || '');
}
// Report to a fixture-only loopback collector, never an application persistence route.
try {
    await fetch(`http://127.0.0.1:${reportPort}/result`, {method: 'POST', mode: 'no-cors', body: report});
} catch (error) {
    console.error(report, error);
}
