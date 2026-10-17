/**
 * @file worker_checks.ts
 * @brief Worker queue, codec boundary and asynchronous lifetime regressions.
 *
 * Checks native worker scheduling alongside codec and job classes, message validation, detached callbacks and resource recovery.
 */

type LabWorkerCodec = import('../../typescript/codec.js').Codec;
type LabWorkerExports = import('../../typescript/codec_types.js').CodecExports;
type LabWorkerFixture = {text: string; values: unknown[]; first: {text: string}; second: {text: string}; independent: boolean; offset: boolean; callback: boolean; small: boolean; recovered: boolean};
/** Isolated workers, bounded C queue policy and asynchronous lifetime regressions. */
function check_lab_worker_policy(native: LabWorkerExports, events: typeof import("../../typescript/jobs.js").worker_events, actions: typeof import("../../typescript/jobs.js").worker_actions, check: LabTestCheck) {
    let phase = 0, active = 0;
    const step = (event: number, token = 0) => {
        const plan = native.lab_jobs_worker_step(phase, event, token, active);
        phase = plan & 255;
        return plan >>> 8;
    };
    check(step(events.pump) === actions.wait, 'idle native worker needs no browser resource');
    const token = native.lab_jobs_submit(1);
    try {
        check(step(events.pump) === actions.start, 'admitted work starts a native worker lifecycle');
        check(step(events.pump) === actions.wait, 'startup cannot create a second worker');
        const starting = phase;
        check((native.lab_jobs_worker_step(starting, events.result, token, 0) >>> 8) === actions.invalid,
            'result before readiness is rejected');
        check((native.lab_jobs_worker_step(starting, events.error, 0, 0) >>> 8) === actions.fail,
            'startup errors fail queued work');
        check(step(events.ready) === actions.wait && step(events.pump) === actions.send,
            'ready worker dispatches admitted work');
        active = native.lab_jobs_take();
        check(active === token && step(events.pump) === actions.wait, 'native lifecycle serialises dispatch');
        check(step(events.cancel, token + 1) === actions.wait, 'queued cancellation leaves running worker intact');
        for (const bad of [0, token + 1, token + 0.5, token + 4294967296, NaN, Infinity, -1])
            check((native.lab_jobs_worker_step(phase, events.result, bad, active) >>> 8) === actions.invalid,
                'native reply token must match exactly: ' + bad);
        for (const event of [events.ready, events.unknown, 99])
            check((native.lab_jobs_worker_step(phase, event, token, active) >>> 8) === actions.invalid,
                'unexpected worker message is rejected');
        check((native.lab_jobs_worker_step(phase, events.cancel, token, active) >>> 8) === actions.stop,
            'active cancellation requests worker termination');
        check((native.lab_jobs_worker_step(phase, events.error, token, active) >>> 8) === actions.finish,
            'job error retires only the matching job');
        check(step(events.result, token) === actions.finish, 'matching result completes the running job');
        check(native.lab_jobs_count() === 1, 'host still owns job retirement');
    } finally {
        native.lab_jobs_finish(token);
        active = 0;
    }
    check(step(events.pump) === actions.wait, 'ready worker stays idle without jobs');
    check(step(events.close) === actions.fail && !native.lab_jobs_worker_accepts(phase), 'close rejects new decoding');
    for (const event of Object.values(events))
        check(step(event, token) === actions.wait, 'late events cannot reopen a closed controller');
    check(!native.lab_jobs_worker_accepts(255), 'invalid controller phase cannot admit work');
}

async function check_lab_codec_classes(check: LabTestCheck, Codec: typeof import("../../typescript/codec.js").Codec, codec: LabWorkerCodec, wasm_bytes: ArrayBuffer) {
    check(codec instanceof Codec, 'codec compatibility factory returns the lifecycle-owning class');
    const independent = new Codec();
    await independent.start(await WebAssembly.compile(wasm_bytes));
    check(independent.exports().memory !== codec.exports().memory, 'codec classes own separate WASM memories');
    const leaf = {text: 'shared but not cyclic'};
    const source = {first: leaf, second: leaf};
    const restored = independent.decode(independent.encode(source)) as LabWorkerFixture;
    check(restored.first.text === leaf.text && restored.second.text === leaf.text,
        'codec still serialises repeated noncyclic references as ordinary JSON values');
    check((codec.decode(codec.encode({independent: true})) as LabWorkerFixture).independent,
        'using another codec instance does not replace the first instance handles');
    const offset = codec.encode({offset: true});
    const storage = new Uint8Array(offset.length + 8);
    storage.set(offset, 4);
    check((codec.decode(new DataView(storage.buffer, 4, offset.length)) as LabWorkerFixture).offset,
        'class codec preserves typed-view offsets and lengths');

    // These methods were closure-backed in the original factory and may be passed as callbacks.
    const {exports: exports_callback, encode, precision, intervals, text} = independent;
    try {
        check(exports_callback() === independent.exports() && precision(NaN, 53) === 53 &&
            intervals(NaN, 5000) === 5000 && (independent.decode(encode({callback: true})) as LabWorkerFixture).callback,
            'formerly closure-backed codec methods remain callback-safe');
        const pointer = independent.exports().lab_browser_input_buffer();
        new Uint8Array(independent.exports().memory.buffer, pointer, 1).fill(0);
        check(text(pointer) === '', 'detached text callback retains the owning codec');
    } catch (caught) {
            const error = caught instanceof Error ? caught : new Error(String(caught));
        check(false, 'detached codec compatibility callback failed: ' + error.message);
    }
}

async function check_lab_worker_validation(check: LabTestCheck) {
    const {is_json_value, is_worker_request, is_worker_response} = await (import('/js/codec_types.js' as string) as Promise<typeof import('../../typescript/codec_types.js')>);
    const leaf = {value: [null, false, 0, 'exact']};
    const shared = {first: leaf, second: leaf};
    check(is_json_value(shared), 'JSON validation accepts shared noncyclic references');
    check(is_worker_response({type: 'result', token: 1, value: shared}),
        'worker result validation accepts a JSON-compatible shared-reference graph');
    const cycle: {self?: unknown} = {};
    cycle.self = cycle;
    const indirect: {next: {previous?: unknown}} = {next: {}};
    indirect.next.previous = indirect;
    for (const value of [cycle, indirect, undefined, NaN, Infinity, 1n, new Date(), new Map(), new Array(2)]) {
        check(!is_json_value(value), 'JSON validator rejects cycles and non-JSON values');
        check(!is_worker_response({type: 'result', token: 1, value}), 'worker rejects invalid result values');
    }
    let getter_reads = 0;
    const accessor = {get value() { ++getter_reads; return 1; }};
    check(!is_json_value(accessor) && getter_reads === 0, 'JSON validation rejects accessors without invoking them');
    check(is_worker_request({type: 'init', url: '/wasm/lab_codec.wasm'}) &&
        is_worker_request({type: 'decode', token: 1, bytes: new Uint8Array(0)}),
        'worker request validation accepts both protocol variants');
    for (const value of [null, [], {type: '__proto__'}, {type: 'init', url: 1},
        {type: 'decode', token: 1, bytes: []}, {type: 'decode', token: 0, bytes: new Uint8Array(0)}])
        check(!is_worker_request(value), 'worker validation rejects malformed requests');
    for (const value of [null, [], {type: '__proto__'}, {type: 'ready', token: 1},
        {type: 'error', token: 1, message: {}}, {type: 'result', token: 1},
        {type: 'result', token: 1.5, value: null}])
        check(!is_worker_response(value), 'worker validation rejects malformed responses');
    check(is_worker_response({type: 'ready'}) &&
        is_worker_response({type: 'error', token: 0, message: 'startup failure'}),
        'readiness and startup errors retain their compatibility shapes');
}

async function check_lab_codec_worker_requests(check: LabTestCheck, worker_url: string, wasm_url: string, codec: LabWorkerCodec) {
    const worker = new Worker(worker_url, {type: 'module'});
    const exchange = (message: unknown) => new Promise<import("../../typescript/codec_types.js").WorkerResponse>((resolve, reject) => {
        const cleanup = () => {
            clearTimeout(timer);
            worker.onmessage = null;
            worker.onerror = null;
            worker.onmessageerror = null;
        };
        const timer = setTimeout(() => {
            cleanup();
            reject(new Error('Codec protocol regression worker did not reply'));
        }, 5000);
        worker.onmessage = event => { cleanup(); resolve(event.data); };
        worker.onerror = event => {
            event.preventDefault();
            cleanup();
            reject(new Error('Codec protocol regression worker failed'));
        };
        worker.onmessageerror = () => { cleanup(); reject(new Error('Uncloneable codec response')); };
        try {
            worker.postMessage(message);
        } catch (caught) {
            const error = caught instanceof Error ? caught : new Error(String(caught));
            cleanup();
            reject(error);
        }
    });
    try {
        for (const request of [null, {type: 'decode', token: 1, bytes: codec.encode({early: true})}]) {
            const reply = await exchange(request);
            check(reply.type === 'error' && typeof reply.message === 'string',
                'dedicated worker rejects malformed or premature requests with a structured error');
        }
        check((await exchange({type: 'init', url: wasm_url})).type === 'ready',
            'dedicated worker initialises after rejected pre-init requests');
        const duplicate = await exchange({type: 'init', url: wasm_url});
        check(duplicate.type === 'error' && duplicate.token === 0, 'worker refuses duplicate initialisation');
        const malformed = await exchange({type: 'decode', token: 7, bytes: new Uint8Array([255])});
        check(malformed.type === 'error' && malformed.token === 7, 'decode failure retains the request token');
        const valid = await exchange({type: 'decode', token: 8, bytes: codec.encode({recovered: true})});
        check(valid.type === 'result' && valid.token === 8 && (valid.value as {recovered: boolean}).recovered,
            'worker remains usable after malformed input and duplicate initialisation');
    } finally {
        worker.terminate();
    }
}

async function labCheckWorkers(check: LabTestCheck) {
    for (const name of ['currentMode', 'evaluateCurrentMode', 'setStatus', 'fetchEvaluation',
        'currentExpressionText', 'setExpressionEditor', 'saveLabState', 'setRenderedResult',
        'formatAlmanacTimeInput', 'labFetch', 'labStart', 'labLoadScript', 'labStartEventBindings',
        'labInitialiseBrowserWidgets', 'labInitialiseWorksheet', 'markDatetimeOffsetTouched'])
        check(!Object.hasOwn(window, name), 'domain API is not published as a window global: ' + name);
    const firstSession = new WorksheetSession();
    const secondSession = new WorksheetSession();
    firstSession.editor.lastExpressionUpdatedAt = 123;
    firstSession.calendar.datetimeGmtOffsetTouched = true;
    check(secondSession.editor.lastExpressionUpdatedAt === 0 && !secondSession.calendar.datetimeGmtOffsetTouched,
        'worksheet sessions own independent residual editor and calendar data');
    const firstPresentation = new WorksheetPresentationState(), secondPresentation = new WorksheetPresentationState();
    firstPresentation.lastTex = 'isolated';
    firstPresentation.resultInputBindings.push({name: 'x', value: '1'});
    check(secondPresentation.lastTex === '' && secondPresentation.resultInputBindings.length === 0,
        'presentation instances retain independent native metadata');
    const resizeTarget = new EventTarget();
    const application = new LabApplication(resizeTarget);
    let resizeCount = 0;
    const resized = (): void => { ++resizeCount; };
    application.listenResize(resized);
    application.listenResize(resized);
    resizeTarget.dispatchEvent(new Event('resize'));
    check(resizeCount === 1, 'application owns one subscription per resize callback');
    application.dispose();
    application.dispose();
    resizeTarget.dispatchEvent(new Event('resize'));
    check(resizeCount === 1, 'application disposal releases fallback resize subscriptions idempotently');
    let disposedLoadRejected = false;
    try { await application.loadScript('/js/unused-after-disposal.js'); }
    catch (error) { disposedLoadRejected = error instanceof DOMException && error.name === 'AbortError'; }
    check(disposedLoadRejected && !document.querySelector('script[src="/js/unused-after-disposal.js"]'),
        'disposed application refuses new script work before touching the DOM');

    let frameId = 0, fitted = 0;
    const frames = new Map<number, FrameRequestCallback>();
    const fit = new WorksheetResults.WorksheetFitScheduler(
        callback => { frames.set(++frameId, callback); return frameId; },
        frame => { frames.delete(frame); }, () => { ++fitted; });
    fit.schedule();
    check(frames.size === 1, 'fit owner retains one pending animation frame');
    fit.fit();
    check(frames.size === 0 && fitted === 1, 'synchronous fitting cancels an older queued fit');
    fit.schedule();
    fit.dispose();
    fit.dispose();
    check(frames.size === 0 && fitted === 1, 'fit disposal cancels without rendering');

    const flowHost = {
        native: labDOM,
        workspace: {currentMode: () => 'expression' as const},
        results: {labEvaluationView: WorksheetResults.labEvaluationView},
        requests: WorksheetRequests.labRequests
    };
    const isolatedRuntime = new WorksheetRequests.NativeFlowRuntime(flowHost);
    let unboundRejected = false, duplicateRejected = false, reboundRejected = false;
    try { isolatedRuntime.getRegistry(); } catch { unboundRejected = true; }
    isolatedRuntime.bind([() => ({fixture: () => 1}), () => ({fixture: () => 2})]);
    try { isolatedRuntime.getRegistry(); } catch { duplicateRejected = true; }
    try { isolatedRuntime.bind([]); } catch { reboundRejected = true; }
    check(unboundRejected && duplicateRejected && reboundRejected,
        'native runtime rejects unbound execution, duplicate service ownership and rebinding');
    isolatedRuntime.dispose();
    check(!!WorksheetRequests.runtime.getRegistry(), 'isolated runtime disposal leaves application capabilities intact');

    const buffer = new WorksheetNativeBuffer();
    const decodeBefore = labWire.decode;
    try {
        for (const invalid of [{}, {state: []}, {state: {mode: 'invalid', text: 'x'}},
                               {state: {mode: 'expression', text: 7}}]) {
            labWire.decode = () => invalid;
            let rejected = false;
            try { buffer.readSnapshot(1); } catch { rejected = true; }
            check(rejected, 'native snapshot boundary rejects structurally invalid records');
        }
        const snapshot: WorksheetSnapshot = {mode: 'expression', text: 'x', marker: 'retained'};
        labWire.decode = () => ({state: snapshot});
        check(buffer.readSnapshot(1) === snapshot, 'typed snapshot boundary preserves native object identity');
        labWire.decode = () => ({state: null});
        check(buffer.readSnapshot(1) === null, 'typed snapshot boundary retains native empty state');
    } finally {
        labWire.decode = decodeBefore;
    }
    const native = labWire.exports();
    check(native.lab_jobs_count() === 0, 'worker queue starts idle');
    const tokens = Array.from({length: native.lab_jobs_capacity()}, () => native.lab_jobs_submit(1));
    check(tokens.every(Boolean) && !native.lab_jobs_submit(1), 'job count is bounded');
    check(native.lab_jobs_take() === tokens[0], 'native admission order is FIFO');
    check(native.lab_jobs_finish(tokens[0]) && !native.lab_jobs_finish(tokens[0]), 'stale completion rejected');
    const replacement = native.lab_jobs_submit(1);
    check(replacement !== tokens[0] && !native.lab_jobs_finish(tokens[0]), 'slot reuse has a fresh generation');
    check(native.lab_jobs_take() === tokens[1], 'replacement does not jump the queue');
    for (const token of [...tokens, replacement]) native.lab_jobs_finish(token);
    const full = [native.lab_jobs_submit(4194304), native.lab_jobs_submit(4194304)];
    check(full.every(Boolean) && !native.lab_jobs_submit(1), 'retained input budget is bounded');
    for (const token of full) native.lab_jobs_finish(token);
    check(!native.lab_jobs_submit(0) && !native.lab_jobs_submit(8388609), 'invalid message sizes rejected');

    const {Codec, createCodec} = await (import('/js/codec.js' as string) as Promise<typeof import('../../typescript/codec.js')>);
    const {Jobs, createJobs, worker_events, worker_actions} = await (import('/js/jobs.js' as string) as Promise<typeof import('../../typescript/jobs.js')>);
    const codec = createCodec();
    const wasm_bytes = await (await fetch('/wasm/lab_codec.wasm')).arrayBuffer();
    await codec.start(wasm_bytes);
    await check_lab_codec_classes(check, Codec, codec, wasm_bytes);
    await check_lab_worker_validation(check);
    const policy = codec.exports();
    const message_limit = policy.lab_browser_message_limit();
    check(message_limit === native.lab_browser_message_limit(), 'browser and worker codec bounds agree');
    check(policy.lab_browser_member_limit() === native.lab_browser_member_limit() &&
        policy.lab_browser_handle_capacity() === native.lab_browser_handle_capacity() &&
        policy.lab_browser_base64_limit() === native.lab_browser_base64_limit(),
        'browser and worker host admission policies agree');
    check(codec.bytes(new Uint8Array(message_limit)).length === message_limit,
        'host buffer admission accepts the exact native bound');
    let oversized_rejected = false;
    try {
        codec.bytes(new Uint8Array(message_limit + 1));
    } catch (caught) {
            const error = caught instanceof Error ? caught : new Error(String(caught));
        oversized_rejected = error.message.includes('8 MiB');
    }
    check(oversized_rejected, 'host buffer admission rejects one byte above the native bound');
    check_lab_worker_policy(policy, worker_events, worker_actions, check);
    check(policy.memory.buffer instanceof ArrayBuffer, 'codec worker uses private memory');
    const url = (path: string) => new URL(path, location.href).href;
    const workerURL = url('/js/codec_worker.js'), wasmURL = url('/wasm/lab_codec.wasm');
    await check_lab_codec_worker_requests(check, workerURL, wasmURL, codec);
    const pool = createJobs(policy, codec, workerURL, wasmURL);
    check(pool instanceof Jobs, 'jobs compatibility factory returns the worker lifecycle class');
    const rejects = async (promise: Promise<unknown>, name?: string) => {
        try {
            await promise;
            return false;
        } catch (caught) {
            const error = caught instanceof Error ? caught : new Error(String(caught));
            return !name || error.name === name;
        }
    };
    const value = {text: 'μ\u0000'.repeat(100000), values: [null, false, 0, '1/3']};
    const bytes = codec.encode(value);
    try {
        const {decode, status} = pool;
        check((await decode(codec.encode({callback: true})) as LabWorkerFixture).callback && status().started === 0,
            'jobs decode and status remain callback-safe without starting a worker for small values');
        check((await pool.decode(codec.encode({small: true})) as LabWorkerFixture).small && pool.status().started === 0,
            'small messages need no worker');
        const cancelled = new AbortController();
        cancelled.abort();
        check(await rejects(pool.decode(bytes, cancelled.signal), 'AbortError'), 'pre-cancelled work rejected');
        const queued = new AbortController();
        let timerRan = false;
        const timer = new Promise<void>(resolve => setTimeout(() => { timerRan = true; resolve(); }, 0));
        const first = pool.decode(bytes);
        const dropped = rejects(pool.decode(bytes, queued.signal), 'AbortError');
        queued.abort();
        const copy = await first as LabWorkerFixture;
        await timer;
        check(timerRan && await dropped && copy.text === value.text, 'UI timer and queued cancellation stay live');
        check(bytes.byteLength > 0 && copy.values[3] === '1/3', 'input remains owned and exact text survives');
        check(pool.status().started === 1 && pool.status().completed === 1, 'a dedicated worker handled the job');
        const responses = await Promise.all([pool.decode(bytes), pool.decode(bytes), pool.decode(bytes)]);
        check(responses.every(reply => (reply as LabWorkerFixture).text === value.text), 'queued jobs retain separate results');
        check(pool.status().count === 0 && pool.status().bytes === 0, 'completed jobs release admission budgets');
        const malformed = bytes.slice();
        malformed[1] = 99;
        check(await rejects(pool.decode(malformed)), 'worker rejects malformed Protobuf');
        check((await pool.decode(bytes) as LabWorkerFixture).text === value.text, 'worker recovers after codec rejection');
        const polluted: Record<string, unknown> = Object.create(null);
        polluted.__proto__ = {safe: true};
        polluted.text = value.text;
        const safe = await pool.decode(codec.encode(polluted)) as object;
        check(Object.hasOwn(safe, '__proto__') && !Object.hasOwn(Object.prototype, 'safe'), 'worker cloning preserves safe own keys');
        const tasks = Array.from({length: 9}, () => pool.decode(bytes).then(() => true, () => false));
        const outcomes = await Promise.all(tasks);
        check(outcomes.slice(0, 8).every(Boolean) && !outcomes[8], 'host applies native queue backpressure');
    } finally {
        pool.close();
    }
    check(await rejects(pool.decode(bytes)), 'closed queue rejects');

    const direct = new Jobs(policy, codec, workerURL, wasmURL);
    try {
        const {close} = direct;
        try {
            close();
            check(await rejects(direct.decode(bytes)), 'detached close callback closes a directly constructed jobs class');
        } catch (caught) {
            const error = caught instanceof Error ? caught : new Error(String(caught));
            check(false, 'detached jobs close compatibility callback failed: ' + error.message);
        }
    } finally {
        direct.close();
    }

    const hungURL = URL.createObjectURL(new Blob([
        'self.onmessage = ({data}) => { if (data.type === "init") self.postMessage({type:"ready"}); };'
    ], {type: 'text/javascript'}));
    const hung = createJobs(policy, codec, hungURL, wasmURL);
    try {
        const controller = new AbortController();
        const pending = rejects(hung.decode(bytes, controller.signal), 'AbortError');
        for (let i = 0; i < 1000 && !hung.status().active; ++i)
            await new Promise(resolve => setTimeout(resolve, 10));
        check(hung.status().active !== 0, 'controlled worker started a job');
        controller.abort();
        check(await pending && !hung.status().count && !hung.status().bytes, 'active cancellation releases the job');
        const waiting = rejects(hung.decode(bytes), 'AbortError');
        hung.close();
        check(await waiting && !hung.status().count, 'shutdown rejects pending work');
        const timed = createJobs({...policy, lab_jobs_timeout: () => 100}, codec, hungURL, wasmURL);
        try {
            check(await rejects(timed.decode(bytes), 'TimeoutError'), 'deadline includes worker startup');
            check(!timed.status().count && !timed.status().bytes, 'timeout releases budget');
        } finally {
            timed.close();
        }
    } finally {
        hung.close();
        URL.revokeObjectURL(hungURL);
    }
    const failed = createJobs(policy, codec, workerURL, url('/missing-codec.wasm'));
    try {
        check(await rejects(failed.decode(bytes)), 'worker startup failure rejects');
        check(!failed.status().count && !failed.status().bytes, 'startup failure releases budget');
    } finally {
        failed.close();
    }
    const unavailable = createJobs(policy, codec, url('/js/missing-worker.js'), wasmURL);
    try {
        check(await rejects(unavailable.decode(bytes)), 'worker script failure rejects');
        check(!unavailable.status().count && !unavailable.status().bytes, 'script failure releases budget');
    } finally {
        unavailable.close();
    }
    for (const reply of [
        'null', '"invalid"', '{type:"ready"}', '{type:"__proto__"}',
        '{type:"result",token:data.token+0.5}', '{type:"result",token:data.token+4294967296}',
        '{type:"result",token:String(data.token)}', '{type:"result",token:data.token+8}',
        '{type:"result",token:data.token,value:undefined}', '{type:"result",token:data.token,value:NaN}',
        '{type:"result",token:data.token,value:new Map()}', '{type:"error",token:data.token,message:17}',
        '(() => { const value = {}; value.self = value; return {type:"result",token:data.token,value}; })()'
    ]) {
        const bad_url = URL.createObjectURL(new Blob([
            'self.onmessage = ({data}) => { if (data.type === "init") self.postMessage({type:"ready"});' +
            'else self.postMessage(' + reply + '); };'
        ], {type: 'text/javascript'}));
        const bad_pool = createJobs(policy, codec, bad_url, wasmURL);
        try {
            const outcomes = await Promise.all([rejects(bad_pool.decode(bytes)), rejects(bad_pool.decode(bytes))]);
            check(outcomes.every(Boolean), 'malformed worker reply rejects running and queued jobs: ' + reply);
            check(!bad_pool.status().count && !bad_pool.status().bytes && !bad_pool.status().active,
                'malformed reply releases all browser and native job ownership');
        } finally {
            bad_pool.close();
            URL.revokeObjectURL(bad_url);
        }
    }
    const restart_url = URL.createObjectURL(new Blob([
        'self.onmessage = ({data}) => { if (data.type === "init") self.postMessage({type:"ready"});' +
        'else setTimeout(() => self.postMessage({type:"result",token:data.token,value:"restarted"}),100); };'
    ], {type: 'text/javascript'}));
    const restart_pool = createJobs(policy, codec, restart_url, wasmURL);
    try {
        const controller = new AbortController();
        const cancelled = rejects(restart_pool.decode(bytes, controller.signal), 'AbortError');
        const waiting = restart_pool.decode(bytes);
        for (let i = 0; i < 1000 && !restart_pool.status().active; ++i)
            await new Promise(resolve => setTimeout(resolve, 1));
        check(restart_pool.status().active !== 0, 'restart fixture dispatches a job');
        controller.abort();
        check(await cancelled && await waiting === 'restarted', 'queued work survives active cancellation');
        check(restart_pool.status().started === 2 && restart_pool.status().completed === 1,
            'native cancellation policy starts one replacement worker');
        check(!restart_pool.status().count && !restart_pool.status().bytes, 'replacement releases all budgets');
    } finally {
        restart_pool.close();
        URL.revokeObjectURL(restart_url);
    }

    const originalFetch = window.fetch;
    try {
        window.fetch = async () => new Response(bytes.slice(), {headers: {'Content-Type': 'application/x-protobuf'}});
        const before = labWire.background().status().completed;
        const response = await labHttp.fetch('/worker-fixture');
        check((await response.labData() as LabWorkerFixture).text === value.text, 'production transport uses background decoding');
        check(labWire.background().status().completed === before + 1, 'production path reached a worker');
    } finally {
        window.fetch = originalFetch;
    }
}
