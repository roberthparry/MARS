/**
 * @file worker_checks.ts
 * @brief Worker queue, codec boundary and asynchronous lifetime regressions.
 *
 * Checks native worker scheduling alongside codec and job classes, message validation, detached callbacks and resource recovery.
 */

type LabWorkerCodec = import('../../typescript/codec.js').Codec;
type LabWorkerExports = import('../../typescript/codec_types.js').CodecExports;
type LabWorkerResponse = import('../../typescript/codec_types.js').WorkerResponse;
type LabWorkerJsonValue = import('../../typescript/codec_types.js').JsonValue;

/** Decoded fields inspected by the worker fixtures. */
interface LabWorkerFixture {
    [name: string]: LabWorkerJsonValue;
    text: string;
    values: LabWorkerJsonValue[];
    first: {text: string};
    second: {text: string};
    independent: boolean;
    offset: boolean;
    callback: boolean;
    small: boolean;
    recovered: boolean;
}

/** @brief Check native storage compatibility, strict validation, buffer bounds and recovery in an isolated codec. */
function checkLabStoredCodec(check: LabTestCheck, codec: LabWorkerCodec): void {
    const native = codec.exports();
    const input = new Uint8Array(native.memory.buffer, native.lab_browser_input_buffer(), native.lab_browser_base64_limit());
    const {encodeStored, decodeStored} = codec;
    const fixtures = [{}, {text: ''}, {text: 'a'}, {text: 'ab'}, {text: 'abc'},
        {text: '\ufeffμ\u0000🪐'.repeat(4000), values: [false, null, 1.25, '1/3']}];
    for (const source of fixtures) {
        const bytes = codec.encode(source);
        let binary = '';
        for (const byte of bytes) binary += String.fromCharCode(byte);
        const legacy = btoa(binary);
        check(encodeStored(source) === legacy, 'native storage preserves the legacy base64 representation');
        check(JSON.stringify(decodeStored(legacy)) === JSON.stringify(source),
            'native storage reads legacy records, including Unicode and every padding length');
        check(input.subarray(0, legacy.length).every(byte => byte === 0), 'storage decoding clears its borrowed buffer');
    }
    for (const text of ['', 'A', 'AAA', '=AAA', 'A===', 'AA=A', 'AB==', 'AAF=', 'AAAA====',
                        'CAESAggF\n', 'CAESAgg_', 'μAAA', btoa('{}')]) {
        let rejected = false;
        try {
            decodeStored(text);
        } catch {
            rejected = true;
        }
        check(rejected, 'native storage rejects malformed base64 or envelopes: ' + text);
        check(input.subarray(0, text.length).every(byte => byte === 0), 'rejected storage input is cleared');
    }

    // Fill the binary budget exactly, including the Protobuf envelope's own overhead.
    const limit = native.lab_browser_message_limit();
    let source = {text: 'x'.repeat(limit - 64)};
    source = {text: 'x'.repeat(source.text.length + limit - codec.encode(source).length)};
    check(codec.encode(source).length === limit, 'storage boundary fixture fills the native message budget');
    const encoded = encodeStored(source);
    check(encoded.length === native.lab_browser_base64_limit(), 'full binary budget fits the padded storage budget');
    const restored = decodeStored(encoded) as {text: string};
    check(restored.text === source.text, 'exact storage maximum round trips');
    check(input.every(byte => byte === 0), 'the entire maximum-size staged record is cleared');

    let rejected = false;
    try {
        encodeStored({text: source.text + 'x'});
    } catch {
        rejected = true;
    }
    check(rejected, 'storage encoding rejects one byte above the binary budget');
    input[0] = 77;
    rejected = false;
    try {
        decodeStored(encoded + 'A');
    } catch {
        rejected = true;
    }
    check(rejected && input[0] === 77, 'oversized storage text is rejected before copying to native memory');
    check(native.lab_browser_storage_decode(input.length + 1) < 0 && input[0] === 77,
        'native storage admission independently rejects oversized input');
    input[0] = 0;
    // The same base64 character count can encode one byte too many if its final padding is removed.
    rejected = false;
    try {
        decodeStored(encoded.slice(0, -1) + 'A');
    } catch {
        rejected = true;
    }
    check(rejected, 'native storage also enforces the decoded byte budget');
    check(JSON.stringify(decodeStored(encodeStored({recovered: true}))) === '{"recovered":true}',
        'storage codec recovers after all rejected operations');
}

/** Isolated workers, bounded C queue policy and asynchronous lifetime regressions. */
function checkLabWorkerPolicy(native: LabWorkerExports, events: typeof import("../../typescript/jobs.js").workerEvents,
                              actions: typeof import("../../typescript/jobs.js").workerActions, check: LabTestCheck): void {
    let phase = 0, active = 0;
    function step(event: number, token = 0): number {
        const plan = native.lab_jobs_worker_step(phase, event, token, active);
        phase = plan & 255;
        return plan >>> 8;
    }
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

/** @brief Check isolated codec ownership and detached callback compatibility. */
async function checkLabCodecClasses(check: LabTestCheck, Codec: typeof import("../../typescript/codec.js").Codec,
                                   codec: LabWorkerCodec, wasmBytes: ArrayBuffer): Promise<void> {
    check(codec instanceof Codec, 'codec compatibility factory returns the lifecycle-owning class');
    const independent = new Codec();
    await independent.start(await WebAssembly.compile(wasmBytes));
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

    const native = independent.exports();
    const inputPointer = native.lab_browser_input_buffer();
    const input = independent.encode({cleanup: 'μ🙂'});
    const originalInput = input.slice();
    independent.decode(input);
    const cleared = new Uint8Array(native.memory.buffer, inputPointer, input.length);
    let inputCleared = true, callerPreserved = true;
    for (let index = 0; index < input.length; ++index) {
        if (cleared[index] !== 0) inputCleared = false;
        if (input[index] !== originalInput[index]) callerPreserved = false;
    }
    check(inputCleared && callerPreserved, 'Successful decoding clears native staging without erasing caller bytes');
    let malformedRejected = false;
    try {
        independent.decode(new Uint8Array([255]));
    } catch {
        malformedRejected = true;
    }
    check(malformedRejected && new Uint8Array(native.memory.buffer, inputPointer, 1)[0] === 0,
          'Failed decoding also clears its native staging bytes');
    check((independent.decode(independent.encode({recovered: true})) as LabWorkerFixture).recovered,
          'Codec releases transient handles and the re-entry guard after decode failure');

    // These methods were closure-backed in the original factory and may be passed as callbacks.
    const {exports: exportsCallback, encode, precision, intervals, text} = independent;
    try {
        check(exportsCallback() === independent.exports() && precision(NaN, 53) === 53 &&
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

/** @brief Check protocol validation without executing untrusted accessors. */
async function checkLabWorkerValidation(check: LabTestCheck): Promise<void> {
    const {isJsonValue, isWorkerRequest, isWorkerResponse} = await (import('/js/codec_types.js' as string) as Promise<typeof import('../../typescript/codec_types.js')>);
    const leaf = {value: [null, false, 0, 'exact']};
    const shared = {first: leaf, second: leaf};
    check(isJsonValue(shared), 'JSON validation accepts shared noncyclic references');
    check(isWorkerResponse({type: 'result', token: 1, value: shared}),
        'worker result validation accepts a JSON-compatible shared-reference graph');
    const cycle: {self?: unknown} = {};
    cycle.self = cycle;
    const indirect: {next: {previous?: unknown}} = {next: {}};
    indirect.next.previous = indirect;
    for (const value of [cycle, indirect, undefined, NaN, Infinity, 1n, new Date(), new Map(), new Array(2)]) {
        check(!isJsonValue(value), 'JSON validator rejects cycles and non-JSON values');
        check(!isWorkerResponse({type: 'result', token: 1, value}), 'worker rejects invalid result values');
    }
    let getterReads = 0;
    const accessor = {
        get value() {
            ++getterReads;
            return 1;
        }
    };
    check(!isJsonValue(accessor) && getterReads === 0, 'JSON validation rejects accessors without invoking them');
    const accessorArray = [0];
    Object.defineProperty(accessorArray, '0', {get: function readElement(): number {
        ++getterReads;
        return 1;
    }});
    check(!isJsonValue(accessorArray) && getterReads === 0,
          'Array validation also rejects element accessors without executing them');
    let deep: unknown = null;
    for (let level = 0; level < 4096; ++level) deep = {child: deep};
    check(isJsonValue(deep), 'JSON validation uses an explicit stack for deeply nested containers');
    let aliases: unknown = {value: 1};
    for (let level = 0; level < 32; ++level) aliases = {left: aliases, right: aliases};
    check(isJsonValue(aliases), 'Completed shared containers are not repeatedly traversed');
    const plain = Object.create(null);
    plain.text = 'μ🙂';
    Object.defineProperty(plain, 'ignored', {get: function hiddenProperty(): never {
        throw new Error('Non-enumerable getter must not run');
    }});
    check(isJsonValue(plain) && !isJsonValue(cycle) && isJsonValue(shared),
          'Each validation owns fresh traversal state and preserves null-prototype record rules');
    check(isWorkerRequest({type: 'init', url: '/wasm/lab_codec.wasm'}) &&
        isWorkerRequest({type: 'decode', token: 1, bytes: new Uint8Array(0)}),
        'worker request validation accepts both protocol variants');
    for (const value of [null, [], {type: '__proto__'}, {type: 'init', url: 1},
        {type: 'decode', token: 1, bytes: []}, {type: 'decode', token: 0, bytes: new Uint8Array(0)}])
        check(!isWorkerRequest(value), 'worker validation rejects malformed requests');
    for (const value of [null, [], {type: '__proto__'}, {type: 'ready', token: 1},
        {type: 'error', token: 1, message: {}}, {type: 'result', token: 1},
        {type: 'result', token: 1.5, value: null}])
        check(!isWorkerResponse(value), 'worker validation rejects malformed responses');
    check(isWorkerResponse({type: 'ready'}) &&
        isWorkerResponse({type: 'error', token: 0, message: 'startup failure'}),
        'readiness and startup errors retain their compatibility shapes');
}

/** @brief Exchange controlled requests with a dedicated worker and retain protocol failures. */
async function checkLabCodecWorkerRequests(check: LabTestCheck, workerUrl: string, wasmUrl: string,
                                         codec: LabWorkerCodec): Promise<void> {
    const worker = new Worker(workerUrl, {type: 'module'});
    function exchange(message: unknown): Promise<LabWorkerResponse> {
        return new Promise<LabWorkerResponse>(function awaitReply(resolve, reject) {
            function cleanup(): void {
                clearTimeout(timer);
                worker.onmessage = null;
                worker.onerror = null;
                worker.onmessageerror = null;
            }
            function timeout(): void {
                cleanup();
                reject(new Error('Codec protocol regression worker did not reply'));
            }
            const timer = setTimeout(timeout, 5000);
            worker.onmessage = function receiveReply(event): void {
                cleanup();
                resolve(event.data);
            };
            worker.onerror = function rejectWorker(event): void {
                event.preventDefault();
                cleanup();
                reject(new Error('Codec protocol regression worker failed'));
            };
            worker.onmessageerror = function rejectMessage(): void {
                cleanup();
                reject(new Error('Uncloneable codec response'));
            };
            try {
                worker.postMessage(message);
            } catch (caught) {
                const error = caught instanceof Error ? caught : new Error(String(caught));
                cleanup();
                reject(error);
            }
        });
    }
    try {
        for (const request of [null, {type: 'decode', token: 1, bytes: codec.encode({early: true})}]) {
            const reply = await exchange(request);
            check(reply.type === 'error' && typeof reply.message === 'string',
                'dedicated worker rejects malformed or premature requests with a structured error');
        }
        check((await exchange({type: 'init', url: wasmUrl})).type === 'ready',
            'dedicated worker initialises after rejected pre-init requests');
        for (const token of [0, -1, 1.5, 0x100000000, '1']) {
            const reply = await exchange({type: 'decode', token, bytes: codec.encode({probe: true})});
            check(reply.type === 'error' && reply.token === 0,
                  'Invalid request tokens are never echoed as valid native identifiers');
        }
        const duplicate = await exchange({type: 'init', url: wasmUrl});
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

/** @brief Isolate redraw coalescing, ownership snapshots and remote cleanup from real plotting. */
async function checkLabPlotRedrawLifetime(check: LabTestCheck): Promise<void> {
    const fixtureDocument = document.implementation.createHTMLDocument('Redraw fixture');
    const rendered = fixtureDocument.createElement('div');
    rendered.id = 'plotRendered';
    const chart = fixtureDocument.createElementNS('http://www.w3.org/2000/svg', 'svg');
    chart.dataset.surface = 'true';
    rendered.appendChild(chart);
    fixtureDocument.body.appendChild(rendered);
    for (const id of ['plotAzimuth', 'plotElevation', 'plotRoll']) {
        const input = fixtureDocument.createElement('input');
        input.id = id;
        input.value = '0';
        fixtureDocument.body.appendChild(input);
    }
    let token = 10;
    let localAction = function localPreview(): boolean { return false; };
    let remote: Promise<unknown> = Promise.resolve();
    let projection: unknown;
    const requests: string[] = [], completions: number[] = [];
    let followups: WorksheetRequests.MarsApiRedrawArguments[] = [];
    function unexpected(): never { throw new Error('Unexpected redraw fixture capability'); }
    function gesture(pan = false): WorksheetRequests.MarsApiGesture {
        return {x: 0, y: 0, touchX: 0, touchY: 0, revision: 1, active: true, pan, panProjection: 'complex'};
    }
    const controller = new WorksheetRequests.PlotRequestController({
        workspace: {
            currentMode() { return 'expression'; },
            variableNamesFromBindings: unexpected,
            renderDerivativeButtons: unexpected
        },
        bindings: {visibleBindingValues: unexpected, expressionWithVisibleBindings: unexpected},
        results: {displayCalculusResult: unexpected, setMatrixPrettyResult: unexpected},
        context: {rightPaneTitle: rendered, valueTitle: rendered},
        presentationState: {lastMatrixScalarExpression: ''},
        bindingState: {currentVariables: [], currentDifferentiable: false},
        native: {
            call(entry, ...args) {
                if (entry === 'lab_plot_drag_complete') {
                    completions.push(Number(args[1]));
                    return {};
                }
                if (entry === 'lab_layout_plot') {
                    projection = (args[0] as {gestureProjection?: string}).gestureProjection;
                    return null;
                }
                return unexpected();
            },
            services(_plan, registry) {
                const queued = followups;
                followups = [];
                const redraw = registry!.redrawPlotDrag as (...args: WorksheetRequests.MarsApiRedrawArguments) => unknown;
                for (const args of queued) redraw(...args);
            }
        },
        volume: {
            requesting: false,
            cancel() {},
            slider: unexpected,
            set: unexpected,
            schedule: unexpected,
            accept(saved) { return saved; }
        },
        preview: {
            prepare() { return null; },
            draw: unexpected,
            pinch() { return localAction(); },
            rotate() { return localAction(); },
            dispose() {},
            clear() {},
            snapshot() { return null; }
        },
        document: fixtureDocument,
        flow: {continue: unexpected, run: unexpected},
        requests: {
            begin: unexpected, current: unexpected, finish: unexpected, cancel: unexpected,
            latestMain() { return token; }, context: unexpected, timedOut: unexpected,
            outcome: unexpected, input: unexpected, post: unexpected, run: unexpected,
            runUI: unexpected, modeChanged: unexpected
        }
    });
    controller.takeCalculus = function remotePlot(variable: string): Promise<unknown> {
        requests.push(variable);
        ++token;
        return remote;
    };
    try {
        const coalesced = gesture(true);
        followups = [[coalesced, 'superseded', false], [coalesced, 'last', false]];
        const redraw = controller.redrawPlotDrag;
        await redraw(coalesced, 'first', false);
        check(requests.join(',') === 'first,last' && completions.join(',') === '1,1',
              'Detached redraw retains only the final native coalesced request');
        check(coalesced.pendingToken === undefined, 'Accepted remote redraw releases its pending token');

        localAction = function changeGeneration(): boolean {
            ++token;
            return true;
        };
        await redraw(gesture(), 'local', true);
        check(completions[completions.length - 1] === 0 && requests.length === 2,
              'Local redraw snapshots latestMain before preview work changes request ownership');

        const changed = gesture();
        const beforeRevision = completions.length;
        localAction = function changeRevision(): boolean {
            changed.revision = (changed.revision || 0) + 1;
            return true;
        };
        await redraw(changed, 'changed', true);
        check(completions.length === beforeRevision,
              'Revision is captured before preview work and stale completion is suppressed');

        localAction = function noPreview(): boolean { return false; };
        const failure = new Error('Redraw transport fixture failure');
        remote = Promise.reject(failure);
        const rejected = gesture(true);
        let caught: unknown;
        try {
            await redraw(rejected, 'reject', false);
        } catch (error) {
            caught = error;
        }
        check(caught === failure && rejected.pendingToken === token && completions[completions.length - 1] === 0,
              'Remote rejection preserves error identity and pending token while completing as unaccepted');
        controller.capabilities().displayPlot({});
        check(projection === undefined, 'Remote rejection removes its gesture projection token');

        let release!: (value: unknown) => void;
        remote = new Promise(function retain(resolve) { release = resolve; });
        const stale = gesture(true);
        const beforeStale = completions.length;
        const staleWork = redraw(stale, 'stale', false);
        const staleToken = token;
        controller.capabilities().displayPlot({});
        check(projection === 'complex', 'Pending remote redraw owns its targeted projection');
        stale.revision = (stale.revision || 0) + 1;
        release(undefined);
        await staleWork;
        check(stale.pendingToken === staleToken && completions.length === beforeStale,
              'Stale remote completion retains pending ownership and skips native completion');
        controller.capabilities().displayPlot({});
        check(projection === undefined, 'Stale remote completion still removes its projection token');

        remote = new Promise(function retain(resolve) { release = resolve; });
        const disposed = gesture(true);
        const disposedWork = redraw(disposed, 'disposed', false);
        const disposedToken = token;
        controller.dispose();
        release(undefined);
        await disposedWork;
        check(disposed.pendingToken === disposedToken && completions.length === beforeStale,
              'Disposal during remote work suppresses native completion without rewriting gesture ownership');
    } finally {
        controller.dispose();
    }
}

/** @brief Isolated workspace geometry-frame ownership and detachable callbacks. */
function checkWorkspaceLayoutLifetime(check: LabTestCheck): void {
    const frames = new Map<number, FrameRequestCallback>();
    const cancelled: number[] = [], operations: string[] = [];
    const editors = [document.createElement('textarea')];
    let nextFrame = 0, viewport = 800, reschedule = false;
    const layout = new WorksheetWorkspace.WorksheetEditorLayout({
        controls: {labTextareas: editors},
        get viewportHeight() { return viewport; },
        requestFrame(callback) {
            frames.set(++nextFrame, callback);
            return nextFrame;
        },
        cancelFrame(handle) {
            cancelled.push(handle);
            frames.delete(handle);
        },
        dom: {
            call(entry, ...args) {
                operations.push(entry);
                check(args[0] === editors, 'Workspace layout retains the editor collection');
                if (entry === 'lab_workspace_dom_editor_resize') {
                    check(args[1] === viewport && layout.editorResizeFrame === 0,
                          'Workspace frame reads current geometry and releases its handle before projection');
                    if (reschedule) {
                        reschedule = false;
                        layout.scheduleEditorResizeGrip();
                    }
                }
            }
        }
    });
    const schedule = layout.scheduleEditorResizeGrip, dispose = layout.dispose;
    const reset = layout.resetEditorManualSize;
    try {
        reset();
        check(operations[0] === 'lab_workspace_dom_editor_reset', 'Detached layout reset retains its owner');
        schedule();
        const first = layout.editorResizeFrame;
        schedule();
        check(cancelled.includes(first) && frames.size === 1, 'Repeated layout scheduling replaces only its old frame');
        const current = layout.editorResizeFrame, callback = frames.get(current)!;
        frames.delete(current);
        viewport = 620;
        reschedule = true;
        callback(0);
        check(frames.size === 1 && layout.editorResizeFrame !== 0,
              'A new frame scheduled during projection survives the completed callback');
        const unrelated = ++nextFrame;
        frames.set(unrelated, function unrelatedFrame() {});
        dispose();
        dispose();
        check(frames.size === 1 && frames.has(unrelated) && layout.editorResizeFrame === 0,
              'Detached disposal is repeatable and leaves unrelated frames untouched');
    } finally {
        dispose();
        frames.clear();
    }
}

/** @brief Run worker, callback-lifetime and resource-ownership regressions in harness order. */
async function labCheckWorkers(check: LabTestCheck): Promise<void> {
    checkWorkspaceLayoutLifetime(check);
    await checkLabPlotRedrawLifetime(check);
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
    function resized(): void {
        ++resizeCount;
    }
    application.listenResize(resized);
    application.listenResize(resized);
    resizeTarget.dispatchEvent(new Event('resize'));
    check(resizeCount === 1, 'application owns one subscription per resize callback');
    application.dispose();
    application.dispose();
    resizeTarget.dispatchEvent(new Event('resize'));
    check(resizeCount === 1, 'application disposal releases fallback resize subscriptions idempotently');
    let disposedLoadRejected = false;
    try {
        await application.loadScript('/js/unused-after-disposal.js');
    } catch (error) {
        disposedLoadRejected = error instanceof DOMException && error.name === 'AbortError';
    }
    check(disposedLoadRejected && !document.querySelector('script[src="/js/unused-after-disposal.js"]'),
        'disposed application refuses new script work before touching the DOM');

    const scriptUrl = URL.createObjectURL(new Blob(['void 0;'], {type: 'text/javascript'}));
    const loader = new LabApplication(new EventTarget());
    const cancellingLoader = new LabApplication(new EventTarget());
    try {
        await loader.loadScript(scriptUrl);
        const loaded = document.querySelector('script[src="' + scriptUrl + '"]') as HTMLScriptElement | null;
        check(loaded !== null && loaded.onload === null && loaded.onerror === null,
              'Successful script loading releases temporary element callbacks');
        loaded?.remove();
        const singleLoad = new LabScriptLoad(scriptUrl, new AbortController().signal);
        const firstLoad = singleLoad.start();
        let repeatedStartRejected = false;
        try {
            await singleLoad.start();
        } catch {
            repeatedStartRejected = true;
        }
        await firstLoad;
        check(repeatedStartRejected, 'A script-load owner cannot replace an in-flight load with a second start');
        document.querySelector('script[src="' + scriptUrl + '"]')?.remove();
        let loadError = '';
        const failedLoad = loader.loadScript(scriptUrl);
        const failedScript = document.querySelector('script[src="' + scriptUrl + '"]') as HTMLScriptElement | null;
        // Invoke the installed handler directly: the suite treats real resource errors as fatal.
        const errorHandler = failedScript?.onerror;
        if (failedScript && typeof errorHandler === 'function') errorHandler.call(failedScript, new Event('error'));
        try {
            await failedLoad;
        } catch (error) {
            if (error instanceof Error) loadError = error.message;
        }
        check(loadError.includes(scriptUrl) && failedScript?.onload === null && failedScript.onerror === null,
              'Script load-error callback reports its path and releases temporary callbacks');
        failedScript?.remove();
        const pendingLoad = cancellingLoader.loadScript(scriptUrl);
        const cancelledScript = document.querySelector('script[src="' + scriptUrl + '"]') as HTMLScriptElement | null;
        cancellingLoader.dispose();
        let cancellationReported = false;
        try {
            await pendingLoad;
        } catch (error) {
            cancellationReported = error instanceof DOMException && error.name === 'AbortError';
        }
        check(cancellationReported && cancelledScript !== null && !cancelledScript.isConnected &&
              cancelledScript.onload === null && cancelledScript.onerror === null,
              'Disposal cancels an in-flight script load and removes its node and callbacks');
    } finally {
        loader.dispose();
        cancellingLoader.dispose();
        for (const script of document.querySelectorAll('script[src="' + scriptUrl + '"]')) script.remove();
        URL.revokeObjectURL(scriptUrl);
    }

    let frameId = 0, fitted = 0;
    const frames = new Map<number, FrameRequestCallback>();
    function requestFrame(callback: FrameRequestCallback): number {
        frames.set(++frameId, callback);
        return frameId;
    }
    function cancelFrame(frame: number): void {
        frames.delete(frame);
    }
    function fitFrame(): void {
        ++fitted;
    }
    const fit = new WorksheetResults.WorksheetFitScheduler(requestFrame, cancelFrame, fitFrame);
    fit.schedule();
    check(frames.size === 1, 'fit owner retains one pending animation frame');
    fit.fit();
    check(frames.size === 0 && fitted === 1, 'synchronous fitting cancels an older queued fit');
    fit.schedule();
    fit.dispose();
    fit.dispose();
    check(frames.size === 0 && fitted === 1, 'fit disposal cancels without rendering');
    const {schedule, complete, dispose: disposeFit} = fit;
    function runFrame(time: number): void {
        const queued = frames.entries().next().value;
        if (!queued) throw new Error('Fit fixture: missing frame');
        frames.delete(queued[0]);
        queued[1](time);
    }
    schedule();
    runFrame(17);
    check(fitted === 2 && frames.size === 0, 'detached scheduling performs the default fit when its frame fires');
    let customTime = 0;
    function customFit(time: number): void { customTime = time; }
    schedule(customFit);
    const replacedFrame = frameId;
    schedule(customFit);
    check(!frames.has(replacedFrame) && frames.size === 1, 'rescheduling replaces rather than accumulates frames');
    runFrame(23);
    check(customTime === 23 && fitted === 2, 'custom frame receives its timestamp without also performing default fit');
    function reentrantFit(): void { schedule(customFit); }
    schedule(reentrantFit);
    runFrame(29);
    check(frames.size === 1, 'frame completion permits reentrant scheduling without losing the new frame');
    runFrame(31);
    check(customTime === 31 && frames.size === 0, 'reentrantly scheduled callback runs once');
    schedule(customFit);
    complete();
    check(frames.size === 0, 'detached completion cancels pending custom work');
    schedule(customFit);
    disposeFit();
    disposeFit();
    check(frames.size === 0 && fitted === 2, 'detached disposal cancels custom work without fitting');

    const flowHost = {
        native: labDOM,
        workspace: {currentMode: () => 'expression' as const},
        results: {labEvaluationView: WorksheetResults.labEvaluationView},
        requests: WorksheetRequests.labRequests
    };
    const isolatedRuntime = new WorksheetRequests.NativeFlowRuntime(flowHost);
    let unboundRejected = false, duplicateRejected = false, reboundRejected = false;
    try {
        isolatedRuntime.getRegistry();
    } catch {
        unboundRejected = true;
    }
    function firstRegistry() {
        return {fixture: () => 1};
    }
    function secondRegistry() {
        return {fixture: () => 2};
    }
    isolatedRuntime.bind([firstRegistry, secondRegistry]);
    try {
        isolatedRuntime.getRegistry();
    } catch {
        duplicateRejected = true;
    }
    try {
        isolatedRuntime.bind([]);
    } catch {
        reboundRejected = true;
    }
    check(unboundRejected && duplicateRejected && reboundRejected,
        'native runtime rejects unbound execution, duplicate service ownership and rebinding');
    isolatedRuntime.dispose();
    check(!!WorksheetRequests.runtime.getRegistry(), 'isolated runtime disposal leaves application capabilities intact');

    let coldAccesses = 0;
    function unexpectedBufferAccess(): never {
        ++coldAccesses;
        throw new Error('Empty snapshot touched a native or codec capability');
    }
    const coldBuffer = new WorksheetNativeBuffer(
        {exports: unexpectedBufferAccess, encode: unexpectedBufferAccess, decode: unexpectedBufferAccess},
        WorksheetWorkspace.WORKSPACE_MODE_NAMES, {call: unexpectedBufferAccess});
    const coldReadBytes = coldBuffer.readBytes;
    coldBuffer.readBytes = unexpectedBufferAccess;
    const coldReadSnapshot = coldBuffer.readSnapshot;
    check(coldReadSnapshot(0) === null && coldAccesses === 0,
          'Detached empty snapshot returns null before exports, byte copying, codec or native validation');
    coldBuffer.readBytes = coldReadBytes;
    let negativeSnapshotError = '';
    try {
        coldReadSnapshot(-1);
    } catch (error) {
        if (error instanceof Error) negativeSnapshotError = error.message;
    }
    check(negativeSnapshotError === 'Invalid native worksheet operation' && coldAccesses === 0,
          'Negative snapshot length still fails before borrowing native exports');

    const buffer = new WorksheetNativeBuffer();
    const {readSnapshot, stageSnapshot, stage, stageText, readBytes, readText} = buffer;
    const stagedSnapshot: WorksheetSnapshot = {mode: 'expression', text: 'detached μ🙂'};
    const stagedLength = stageSnapshot(stagedSnapshot, 1);
    const bufferNative = labWire.exports();
    const stagedCopy = new Uint8Array(bufferNative.memory.buffer,
        bufferNative.lab_workspace_input(1), stagedLength).slice();
    const stagedRecord = labWire.decode(stagedCopy) as {state: WorksheetSnapshot};
    check(stagedRecord.state.text === stagedSnapshot.text,
          'Detached snapshot staging retains the buffer codec and selected native slot');
    check(stage(new Uint8Array([65, 66]), 1) === 2 &&
          new Uint8Array(bufferNative.memory.buffer, bufferNative.lab_workspace_input(1), 2).join(',') === '65,66',
          'Detached byte staging retains its native buffer owner');
    const decodeBefore = labWire.decode;
    try {
        for (const invalid of [null, false, 7, 'state', [], {}, {state: false}, {state: []},
                               {state: {mode: 'invalid', text: 'x'}}, {state: {mode: 'Expression', text: 'x'}},
                               {state: {mode: 'expression'}}, {state: {text: 'x'}},
                               {state: {mode: 'expression', text: 7}}]) {
            labWire.decode = () => invalid;
            let rejected = false;
            try {
                readSnapshot(1);
            } catch {
                rejected = true;
            }
            check(rejected, 'native snapshot boundary rejects structurally invalid records');
        }
        const snapshot: WorksheetSnapshot = {mode: 'expression', text: 'x', marker: 'retained'};
        labWire.decode = () => ({state: snapshot});
        check(readSnapshot(1) === snapshot, 'detached snapshot boundary preserves native object identity');
        labWire.decode = () => ({state: null});
        check(readSnapshot(1) === null, 'detached snapshot boundary retains native empty state');
        for (const mode of WorksheetWorkspace.WORKSPACE_MODE_NAMES) {
            const state: WorksheetSnapshot = {mode, text: '\ufeffμ🙂', extra: {retained: true}};
            check(labDOM.call('lab_workspace_snapshot_validate', {state}, WorksheetWorkspace.WORKSPACE_MODE_NAMES) === state,
                  'Native validation preserves every valid mode, Unicode and opaque fields');
        }
        check(labDOM.call('lab_workspace_snapshot_validate', {}, ['expression']) === -1 &&
              labDOM.call('lab_workspace_snapshot_validate', {state: snapshot}, ['matrix']) === -2 &&
              labDOM.call('lab_workspace_snapshot_validate', {state: snapshot}, ['expression']) === snapshot,
              'Native validation distinguishes envelope errors and honours the supplied mode catalogue');
        check(labDOM.call('lab_workspace_snapshot_validate', {state: undefined}, ['expression']) === -2 &&
              labDOM.call('lab_workspace_snapshot_validate', {state: snapshot}, []) === -2 &&
              labDOM.call('lab_workspace_snapshot_validate', {state: snapshot}, 'expression') === -2 &&
              labDOM.call('lab_workspace_snapshot_validate', {state: snapshot},
                          ['expression', 'equation', 'matrix', 'integrator', 'datetime', 'almanac', 'diffequation', 'extra']) === -2,
              'Native snapshot validation rejects undefined state and invalid or oversized catalogues');
        let exportReads = 0;
        const liveWire: WorksheetNativeExports = {
            exports() { ++exportReads; return labWire.exports(); }
        };
        const liveViews = new WorksheetNativeViews(liveWire, buffer, WorksheetWorkspace.WORKSPACE_MODE_NAMES);
        const {mode: modeView, sources: sourcesView} = liveViews;
        const editorView = modeView('editor');
        const precisionView = modeView('precision');
        const sourceView = sourcesView();
        // Compile-only checks retain distinct overload results and writable source fields.
        if (false) {
            const typedEditor: WorksheetModeEditors = modeView('editor');
            const typedPrecision: WorksheetModePrecisions = modeView('precision');
            typedEditor.expression = 'authored text';
            typedPrecision.expression = 256;
            sourceView.goalTarget = 'writable source';
            // @ts-expect-error Native precision fields accept numbers, not editor text.
            typedPrecision.expression = '256';
            // @ts-expect-error Native editor fields accept text, not precision values.
            typedEditor.expression = 256;
            // @ts-expect-error Source fields retain their text contract.
            sourceView.goalTarget = 256;
        }
        check(exportReads === 0, 'Detached view factories create descriptors without borrowing exports');
        // lab_workspace.h fixes the source ABI to these indices, independently of the TypeScript tuple.
        const sourceNames = ['fullText', 'displayText', 'lastInput', 'goalSource', 'goalTarget'] as const;
        check(WORKSHEET_SOURCE_NAMES.join(',') === sourceNames.join(',') &&
              Object.keys(sourceView).join(',') === sourceNames.join(','),
              'All five source keys retain the documented native index order');
        const oldEditor = editorView.expression;
        const oldPrecision = precisionView.expression;
        const oldSources = sourceNames.map(name => sourceView[name]);
        try {
            editorView.expression = 'μ🙂';
            precisionView.expression = 256;
            sourceView.goalSource = 'symbolic text';
            check(WorksheetContext.modeEditorText.expression === 'μ🙂' &&
                  WorksheetContext.modePrecisionBits.expression === 256 &&
                  liveViews.sources().goalSource === 'symbolic text',
                  'Independent live views read the same native fields without browser caches');
            check(Object.keys(editorView).length === 7 && !Object.isExtensible(editorView) &&
                  !Object.isExtensible(precisionView) && !Object.isExtensible(sourceView),
                  'Native views expose only the fixed enumerable field catalogue');
            for (const view of [editorView, precisionView, sourceView]) {
                check(Object.getPrototypeOf(view) === null && !Reflect.defineProperty(view, 'extra', {value: 1}),
                      'Native views retain a null prototype and reject additional fields');
                for (const key of Object.keys(view)) {
                    const descriptor = Object.getOwnPropertyDescriptor(view, key)!;
                    check(descriptor.enumerable && !descriptor.configurable &&
                          typeof descriptor.get === 'function' && typeof descriptor.set === 'function' &&
                          !Object.hasOwn(descriptor, 'value') && !Object.hasOwn(descriptor, 'writable') &&
                          !Reflect.deleteProperty(view, key),
                          'Live fields remain enumerable, non-configurable accessor descriptors: ' + key);
                }
            }
            for (let index = 0; index < sourceNames.length; ++index) {
                const name = sourceNames[index], authored = 'source-' + index + '-μ🙂';
                sourceView[name] = authored;
                const length = bufferNative.lab_workspace_source_get(index);
                check(readText(length) === authored && new TextDecoder().decode(readBytes(length)) === authored,
                      'Writable source accessor maps to documented native index: ' + name);
                const replacement = 'native-' + index;
                check(!!bufferNative.lab_workspace_source_set(index, stageText(replacement)),
                      'Native source fixture accepts replacement bytes');
                const descriptor = Object.getOwnPropertyDescriptor(sourceView, name)!;
                const before = exportReads;
                check(descriptor.get!.call(null) === replacement && descriptor.get!.call({}) === replacement &&
                      exportReads === before + 2, 'Detached source getter borrows exports on every read: ' + name);
                descriptor.set!.call(null, authored);
                check(exportReads === before + 3 && readText(bufferNative.lab_workspace_source_get(index)) === authored,
                      'Detached source setter borrows exports without relying on its receiver: ' + name);
            }
            for (const [view, first, second] of [
                [editorView, 'native editor', 'descriptor editor'], [precisionView, 384, 256]
            ] as const) {
                const descriptor = Object.getOwnPropertyDescriptor(view, 'expression')!;
                if (typeof first === 'string') bufferNative.lab_workspace_editor_set(0, stageText(first));
                else bufferNative.lab_workspace_precision_set(0, first);
                const before = exportReads;
                check(descriptor.get!.call(null) === first && descriptor.get!.call({}) === first &&
                      exportReads === before + 2, 'Editor and precision getters observe native writes without caching');
                descriptor.set!.call(null, second);
                check(exportReads === before + 3 && descriptor.get!.call(null) === second,
                      'Editor and precision setters retain their field identity when detached');
            }
        } finally {
            editorView.expression = oldEditor;
            precisionView.expression = oldPrecision;
            for (let index = 0; index < sourceNames.length; ++index)
                sourceView[sourceNames[index]] = oldSources[index];
        }
    } finally {
        labWire.decode = decodeBefore;
    }
    const native = labWire.exports();
    check(native.lab_jobs_count() === 0, 'worker queue starts idle');
    const capacity = native.lab_jobs_capacity();
    const tokens: number[] = [];
    for (let index = 0; index < capacity; ++index)
        tokens.push(native.lab_jobs_submit(1));
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
    const {Jobs, createJobs, workerEvents, workerActions} = await (import('/js/jobs.js' as string) as Promise<typeof import('../../typescript/jobs.js')>);
    const codec = createCodec();
    const wasmBytes = await (await fetch('/wasm/lab_codec.wasm')).arrayBuffer();
    await codec.start(wasmBytes);
    window.checkLabMemory(codec.exports() as unknown as LabMemoryTestNative, codec.exports().lab_browser_input_buffer());
    window.checkLabProtobufWire(codec);
    await checkLabCodecClasses(check, Codec, codec, wasmBytes);
    checkLabStoredCodec(check, codec);
    await checkLabWorkerValidation(check);
    const policy = codec.exports();
    const messageLimit = policy.lab_browser_message_limit();
    check(messageLimit === native.lab_browser_message_limit(), 'browser and worker codec bounds agree');
    check(policy.lab_browser_member_limit() === native.lab_browser_member_limit() &&
        policy.lab_browser_handle_capacity() === native.lab_browser_handle_capacity() &&
        policy.lab_browser_base64_limit() === native.lab_browser_base64_limit(),
        'browser and worker host admission policies agree');
    check(codec.bytes(new Uint8Array(messageLimit)).length === messageLimit,
        'host buffer admission accepts the exact native bound');
    let oversizedRejected = false;
    try {
        codec.bytes(new Uint8Array(messageLimit + 1));
    } catch (caught) {
        const error = caught instanceof Error ? caught : new Error(String(caught));
        oversizedRejected = error.message.includes('8 MiB');
    }
    check(oversizedRejected, 'host buffer admission rejects one byte above the native bound');
    checkLabWorkerPolicy(policy, workerEvents, workerActions, check);
    check(policy.memory.buffer instanceof ArrayBuffer, 'codec worker uses private memory');
    function url(path: string): string {
        return new URL(path, location.href).href;
    }
    const workerURL = url('/js/codec_worker.js'), wasmURL = url('/wasm/lab_codec.wasm');
    await checkLabCodecWorkerRequests(check, workerURL, wasmURL, codec);
    const pool = createJobs(policy, codec, workerURL, wasmURL);
    check(pool instanceof Jobs, 'jobs compatibility factory returns the worker lifecycle class');
    async function rejects(promise: Promise<unknown>, name?: string): Promise<boolean> {
        try {
            await promise;
            return false;
        } catch (caught) {
            const error = caught instanceof Error ? caught : new Error(String(caught));
            return !name || error.name === name;
        }
    }
    const value = {text: 'μ\u0000'.repeat(100000), values: [null, false, 0, '1/3']};
    const bytes = codec.encode(value);
    const failingCopy = bytes.slice();
    Object.defineProperty(failingCopy, 'slice', {
        value: function failCopy(): never { throw new RangeError('Controlled copy failure'); }
    });
    const copyFailurePool = new Jobs(policy, {
        bytes(): Uint8Array<ArrayBuffer> { return failingCopy; },
        decode: codec.decode
    }, workerURL, wasmURL);
    try {
        check(await rejects(copyFailurePool.decode(bytes), 'RangeError') &&
              copyFailurePool.status().count === 0 && copyFailurePool.status().bytes === 0 &&
              copyFailurePool.status().started === 0,
              'Failed input copying releases native admission before starting a worker');
    } finally {
        copyFailurePool.close();
    }
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
        const timer = new Promise<void>(function scheduleTimer(resolve) {
            function recordTimer(): void {
                timerRan = true;
                resolve();
            }
            setTimeout(recordTimer, 0);
        });
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
        const polluted: LabTestRecord = Object.create(null);
        polluted.__proto__ = {safe: true};
        polluted.text = value.text;
        const safe = await pool.decode(codec.encode(polluted)) as object;
        check(Object.hasOwn(safe, '__proto__') && !Object.hasOwn(Object.prototype, 'safe'), 'worker cloning preserves safe own keys');
        const tasks: Promise<boolean>[] = [];
        for (let index = 0; index < 9; ++index)
            tasks.push(pool.decode(bytes).then(() => true, () => false));
        const outcomes = await Promise.all(tasks);
        check(outcomes.slice(0, 8).every(Boolean) && !outcomes[8], 'host applies native queue backpressure');
        const finishedController = new AbortController();
        await pool.decode(bytes, finishedController.signal);
        const finishedStatus = pool.status();
        finishedController.abort(new Error('Late cancellation'));
        check(pool.status().completed === finishedStatus.completed && pool.status().active === 0 &&
              pool.status().count === 0 && pool.status().started === finishedStatus.started,
              'Cancellation after completion cannot retire another job or restart the worker');
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
            await LabTestBrowser.wait(10);
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
        const badUrl = URL.createObjectURL(new Blob([
            'self.onmessage = ({data}) => { if (data.type === "init") self.postMessage({type:"ready"});' +
            'else self.postMessage(' + reply + '); };'
        ], {type: 'text/javascript'}));
        const badPool = createJobs(policy, codec, badUrl, wasmURL);
        try {
            const outcomes = await Promise.all([rejects(badPool.decode(bytes)), rejects(badPool.decode(bytes))]);
            check(outcomes.every(Boolean), 'malformed worker reply rejects running and queued jobs: ' + reply);
            check(!badPool.status().count && !badPool.status().bytes && !badPool.status().active,
                'malformed reply releases all browser and native job ownership');
        } finally {
            badPool.close();
            URL.revokeObjectURL(badUrl);
        }
    }
    const restartUrl = URL.createObjectURL(new Blob([
        'self.onmessage = ({data}) => { if (data.type === "init") self.postMessage({type:"ready"});' +
        'else setTimeout(() => self.postMessage({type:"result",token:data.token,value:"restarted"}),100); };'
    ], {type: 'text/javascript'}));
    const restartPool = createJobs(policy, codec, restartUrl, wasmURL);
    try {
        const controller = new AbortController();
        const cancelled = rejects(restartPool.decode(bytes, controller.signal), 'AbortError');
        const waiting = restartPool.decode(bytes);
        for (let i = 0; i < 1000 && !restartPool.status().active; ++i)
            await LabTestBrowser.wait(1);
        check(restartPool.status().active !== 0, 'restart fixture dispatches a job');
        controller.abort();
        check(await cancelled && await waiting === 'restarted', 'queued work survives active cancellation');
        check(restartPool.status().started === 2 && restartPool.status().completed === 1,
            'native cancellation policy starts one replacement worker');
        check(!restartPool.status().count && !restartPool.status().bytes, 'replacement releases all budgets');
    } finally {
        restartPool.close();
        URL.revokeObjectURL(restartUrl);
    }

    const originalFetch = window.fetch;
    try {
        window.fetch = async function workerResponse() {
            return new Response(bytes.slice(), {headers: {'Content-Type': 'application/x-protobuf'}});
        };
        const before = labWire.background().status().completed;
        const response = await labHttp.fetch('/worker-fixture');
        check((await response.labData() as LabWorkerFixture).text === value.text, 'production transport uses background decoding');
        check(labWire.background().status().completed === before + 1, 'production path reached a worker');
    } finally {
        window.fetch = originalFetch;
    }
}
