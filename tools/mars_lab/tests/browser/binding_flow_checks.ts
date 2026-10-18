/**
 * @file binding_flow_checks.ts
 * @brief Binding continuation and browser-resource boundary regressions.
 *
 * Exercises typed event routing, detachable controller capabilities and native service plans.
 * Isolated hosts and controlled timers verify callback forwarding and asynchronous resource ownership.
 */

/** @brief Mutable refresh frame retained by the request-admission fixture. */
interface BindingRefreshFixture {
    editedBody: string;
    sourceExpression: string;
    request?: object | null;
}

/** @brief Minimal response used to exercise accepted and rejected projections. */
interface BindingProjectionResponse {
    response: {ok: boolean};
    data: {ok?: boolean; binding_values?: unknown[]; error?: string};
}

/** @brief Request fields inspected after a kind-change continuation. */
interface BindingKindPayload {
    action: string;
    operation: string;
    text: string;
    name: string;
    kind: string;
}

/** @brief Compile-time binding contracts; deliberately never invoked. */
function checkBindingTypeContracts(
    root: HTMLElement, router: WorksheetBindings.WorksheetBindingEventRouter,
    host: WorksheetBindings.ControllerDependencies, row: LabBindingRowAction
): void {
    // Shared capabilities keep native methods and mutable panel values strongly typed.
    const runtime: WorksheetBindings.BindingRuntime = host;
    const controls: WorksheetBindings.BindingControls = host.bindingPanel;
    const values: WorksheetBindings.BindingValues = host.bindingPanel;
    controls.renderVariableValues([]);
    values.currentVariables = ['x'];
    host.modeEditors.integrator = 'x';

    // @ts-expect-error Shared native capabilities remain read-only on dependencies.
    runtime.dom = labDOM;
    // @ts-expect-error Mode editors retain text, not precision numbers.
    host.modeEditors.integrator = 53;
    // @ts-expect-error Binding cards cannot install an integrator row handler.
    router.install(root, 'lab_binding_event', {edit() {}});
    // @ts-expect-error Integrator row actions cannot use a binding-card action name.
    row.action = 'copy';
    // @ts-expect-error Native row edit operation codes remain restricted to 1, 2 or 3.
    row.operation = 4;
    // @ts-expect-error Symbolic starting values remain strings.
    const goal: LabBindingGoalStart = {expression: 'x', start: {x: 3}};
    void goal;
}

/** Binding continuation ordering, asynchronous ownership and browser-resource boundaries. */
window.checkLabBindingFlows = async function checkLabBindingFlows() {
    function check(condition: unknown, message: string): void {
        if (!condition)
            throw new Error('Binding flow: ' + message);
    }

    function equal(actual: unknown, expected: unknown, message: string): void {
        check(Object.is(actual, expected), message);
    }
    // Exercise the typed router with real C-created payloads, without application side effects.
    {
        const router = new WorksheetBindings.WorksheetBindingEventRouter();
        const root = document.createElement('div'), rows = document.createElement('div');
        const binding = {name: 'router fixture', kind: 'variable', value: '1/3'};
        labDOM.call('lab_binding_render', root, [binding]);
        const input = root.querySelector<HTMLInputElement>('.binding-value-input')!;
        let commits = 0, preparations = 0, edits = 0;
        const handlers: LabBindingCardHandlers = {
            commit(action) {
                equal(this, handlers, 'router preserves handler-map receiver');
                equal(action.card.binding, binding, 'card action retains binding identity');
                equal(action.card.input, input, 'card action retains input identity');
                ++commits;
            }
        };
        router.install(root, 'lab_binding_event', handlers);
        router.install(root, 'lab_binding_event', {
            commit() {
                throw new Error('Duplicate router subscription');
            }
        });
        input.dispatchEvent(new Event('change'));
        equal(commits, 1, 'one capture subscription handles non-bubbling events');
        handlers.commit = function recordReplacementCommit(action) {
            equal(action.action, 'commit', 'replacement receives correlated native action');
            commits += 10;
        };
        input.dispatchEvent(new Event('change'));
        equal(commits, 11, 'router resolves replacement handlers at dispatch time');
        const escape = new KeyboardEvent('keydown', {key: 'Escape', cancelable: true});
        equal(input.dispatchEvent(escape), false, 'empty native action still prevents the default');
        equal(commits, 11, 'prevention-only action does not invoke a handler');
        root.dispatchEvent(new Event('change'));
        equal(commits, 11, 'null native action does not invoke a handler');

        labDOM.call('lab_binding_integrator_render', rows, [{kind: 'bound', name: 'x', lo: '0', hi: '1'}]);
        const lower = rows.querySelector<HTMLInputElement>('[data-integrator-lower]')!;
        const rowHandlers: LabBindingIntegratorHandlers = {
            prepare(action) {
                equal(this, rowHandlers, 'integrator handler-map receiver');
                equal(action.card.input, lower, 'input-only integrator payload');
                equal(Object.keys(action.card).join(','), 'input', 'integrator card does not require variable metadata');
                ++preparations;
            },
            edit(action) {
                equal(action.item, rows.firstElementChild, 'edit payload retains row identity');
                equal(action.index, 0, 'edit payload retains row index');
                equal(action.operation, 2, 'edit payload retains native operation');
                equal(action.card, null, 'row edit has no input card');
                ++edits;
            }
        };
        router.install(rows, 'lab_binding_integrator_event', rowHandlers);
        lower.dispatchEvent(new Event('change'));
        rows.querySelector<HTMLButtonElement>('.integrator-bound-add')!.dispatchEvent(new MouseEvent('click'));
        equal(preparations, 1, 'input action reaches its typed handler');
        equal(edits, 1, 'row action reaches its typed handler');
        router.dispose();
    }
    {
        const root = document.createElement('div');
        labDOM.call('lab_binding_render', root, [{name: 'x', kind: 'variable', value: '1'}]);
        const input = root.querySelector<HTMLInputElement>('.binding-value-input')!;
        const router = new WorksheetBindings.WorksheetBindingEventRouter();
        const other = new WorksheetBindings.WorksheetBindingEventRouter();
        const install = router.install, dispose = router.dispose;
        let owned = 0, unrelated = 0;
        try {
            install(root, 'lab_binding_event', {commit: () => ++owned});
            other.install(root, 'lab_binding_event', {commit: () => ++unrelated});
            input.dispatchEvent(new Event('change'));
            equal(owned, 1, 'detached router installation retains its receiver');
            dispose();
            dispose();
            check(!router.roots.has(root), 'disposal releases the router root marker');
            input.dispatchEvent(new Event('change'));
            equal(owned, 1, 'detached disposal removes owned capture listeners');
            equal(unrelated, 2, 'disposal preserves another router on the same root');
            install(root, 'lab_binding_event', {commit: () => owned += 10});
            input.dispatchEvent(new Event('change'));
            equal(owned, 11, 'released roots accept one fresh subscription');
        } finally {
            dispose();
            other.dispose();
        }
    }
    await checkBindingControllerCallbacks(check);

    async function run(kind: number, frame: LabTestRecord, services: NativeServiceRegistry) {
        const registry = {
            native(entry: string, ...args: unknown[]) {
                return labDOM.call(entry, ...args);
            },
            fail(message: string) {
                throw new Error(message);
            },
            ...services
        };
        for (let step = 0; step < 80; ++step) {
            try {
                const plan = labDOM.call<BindingTestPlan>('lab_flow_step', kind, frame, {});
                if (plan.done)
                    return plan.value;
                const pending = labDOM.services(plan, registry);
                const value = plan.wait ? await pending : pending;
                if (plan.save)
                    frame[plan.save] = value;
            } catch (error) {
                if (!Object.hasOwn(frame, 'failed'))
                    throw error;
                frame.stage = frame.failed;
                delete frame.failed;
                frame.exception = error;
                frame.error = String(error);
            }
        }
        throw new Error('Binding flow: continuation failed to terminate');
    }
    const events: string[] = [];
    function services(plan: BindingTestPlan): string {
        return plan.calls.map(call => call.service).join(',');
    }
    const clear = labDOM.call<BindingTestPlan>('lab_binding_projection_clear', 0);
    equal(services(clear), 'bindingSource,bindingSource,bindingSource,bindingResetCache,bindingClearFlags,bindingClearGoal',
          'full source clearing order');
    equal(clear.calls.slice(0, 3).map(call => call.args[0]).join(','), 'fullText,displayText,lastInput',
          'source clearing selects only authored fields');
    const goalOnly = labDOM.call<BindingTestPlan>('lab_binding_projection_clear', 1);
    equal(services(goalOnly), 'bindingSource,bindingSource', 'goal clearing cannot clear controls or source');
    equal(goalOnly.calls.map(call => call.args[0]).join(','), 'goalSource,goalTarget', 'goal field selection');
    for (let mode = 0; mode < 7; ++mode) {
        const plain = labDOM.call<BindingTestPlan>('lab_binding_projection_update', mode, '  opaque source  ', null);
        equal(services(plain), mode < 3 ? 'bindingSetEditor' : 'bindingSetText,bindingClearSource,bindingClearValues',
              'synchronous mode-dependent projection');
        equal(services(labDOM.call<BindingTestPlan>('lab_binding_projection_update', mode, 'opaque source', {wrapped: true})),
              'bindingSetEditor', 'wrapped metadata always selects complete editor projection');
    }
    const context = {fullText: 'source'};
    const deferred = labDOM.call<BindingTestPlan>('lab_binding_projection_prepare', context, null);
    equal(services(deferred), 'bindingDeferEditor', 'missing metadata requires asynchronous preparation');
    equal(deferred.calls[0].args[0], context, 'deferred preparation preserves actual context');
    equal(services(labDOM.call<BindingTestPlan>('lab_binding_projection_prepare', context, {})), '', 'cached metadata skips preparation');
    const selected = {fullText: 'full', displayText: 'body', bindings: [], refresh: true};
    equal(services(labDOM.call<BindingTestPlan>('lab_binding_projection_finish', selected)),
          'bindingSource,bindingSource,bindingResize,bindingRenderValues,bindingSetVariables,bindingRenderDerivatives,' +
              'bindingScheduleRefresh', 'synchronous installation precedes scheduled refresh');
    selected.refresh = false;
    check(!services(labDOM.call<BindingTestPlan>('lab_binding_projection_finish', selected)).includes('bindingScheduleRefresh'),
          'explicit readiness avoids refresh scheduling');

    const refreshContext: BindingRefreshFixture = {
        editedBody: '  \u00a0opaque body\n ',
        sourceExpression: ' exact native source '
    };
    const admission = labDOM.call<BindingTestPlan>('lab_binding_projection_refresh', 0, refreshContext);
    equal(services(admission), 'bindingCancelRefresh,bindingBeginRefresh', 'timer cancellation precedes request admission');
    equal(refreshContext.editedBody, 'opaque body', 'refresh body uses browser whitespace trimming');
    equal(refreshContext.sourceExpression, ' exact native source ', 'refresh retains exact native source');
    equal(admission.calls[1].args[0], refreshContext, 'request callback retains the frame by identity');
    equal(admission.calls[1].args[1], 'bindings', 'native refresh request operation');
    equal(admission.calls[1].args[2], 'expression', 'native refresh request mode');
    equal(Reflect.get(Object(admission.calls[1].args[3]), 'input'), true, 'nonblank body admits input subject to existing request guards');
    equal(services(labDOM.call<BindingTestPlan>('lab_binding_projection_refresh', 1, refreshContext)), '',
          'absent request cannot mark controls or schedule a timer');
    refreshContext.request = null;
    equal(services(labDOM.call<BindingTestPlan>('lab_binding_projection_refresh', 1, refreshContext)), '',
          'rejected request cannot mark controls or schedule a timer');
    const owner = {};
    labDOM.services(admission, {
        bindingCancelRefresh: () => events.push('cancel'),
        bindingBeginRefresh(frame: BindingRefreshFixture, operation: string, mode: string, options: {input: boolean}) {
            events.push('begin');
            equal(operation, 'bindings', 'host receives native operation without reinterpretation');
            equal(mode, 'expression', 'host preserves request current-mode guard selection');
            equal(options.input, true, 'host preserves native input admission flag');
            frame.request = owner;
        }
    });
    equal(events.join(','), 'cancel,begin', 'admission services execute outside native scope in order');
    const scheduled = labDOM.call<BindingTestPlan>('lab_binding_projection_refresh', 1, refreshContext);
    equal(services(scheduled), 'bindingRefreshPending,bindingRefreshHistory,bindingRefreshTimer',
          'accepted refresh marks and updates history before starting timer');
    equal(scheduled.calls[2].args[0], 'opaque body', 'timer captures edited body');
    equal(scheduled.calls[2].args[1], ' exact native source ', 'timer captures native source');
    equal(scheduled.calls[2].args[2], owner, 'timer preserves request identity');
    equal(scheduled.calls[2].args[3], 300, 'native refresh debounce remains 300 ms');
    refreshContext.editedBody = 'new body';
    refreshContext.sourceExpression = 'new source';
    refreshContext.request = {};
    equal(scheduled.calls[2].args[0], 'opaque body', 'later frame writes cannot alter scheduled body');
    equal(scheduled.calls[2].args[2], owner, 'later frame writes cannot change scheduled owner');
    const blank = {editedBody: '\n \u00a0', sourceExpression: ''};
    equal(Reflect.get(Object(labDOM.call<BindingTestPlan>('lab_binding_projection_refresh', 0, blank).calls[1].args[3]), 'input'), false,
          'blank refresh retains input rejection through native request admission');
    equal(services(labDOM.call<BindingTestPlan>('lab_binding_projection_refresh', 2, refreshContext)), '', 'invalid phase is inert');
    events.length = 0;
    let live = false;
    const visible = {
        bindingCurrent: (callback: () => boolean) => callback(),
        async bindingPending(): Promise<number | void> {
            events.push('pending');
            live = false;
        },
        bindingCommitVisible() {
            events.push('commit');
            return true;
        }
    };
    equal(await run(41, {isCurrent: () => live}, visible), false, 'obsolete visible commit returns false');
    equal(events.length, 0, 'obsolete visible commit does not touch pending work');
    live = true;
    equal(await run(41, {isCurrent: () => live}, visible), false, 'ownership is checked again after pending work');
    equal(events.join(','), 'pending', 'stale pending completion cannot commit controls');
    events.length = 0;
    visible.bindingPending = async () => events.push('pending');
    live = true;
    equal(await run(41, {isCurrent: () => live}, visible), true, 'current visible commit returns child result');
    equal(events.join(','), 'pending,commit', 'pending commit precedes visible snapshot');

    const before = {mode: 'matrix', modeId: 3, source: 'opaque source', text: 'visible body'};
    let after = before, captures = 0;
    const refresh = {
        bindingCurrent: (callback: () => boolean) => callback(),
        bindingCapture() {
            const captured = captures === 0 ? before : after;
            ++captures;
            return captured;
        },
        bindingPrepare: async (source: string) => equal(source, before.source, 'preparation receives exact authored source'),
        bindingProjectVisible: () => events.push('project'),
        bindingScheduleRefresh: () => events.push('schedule'),
        setStatus: (message: string) => events.push(message)
    };
    for (const field of ['mode', 'text', 'source']) {
        events.length = 0;
        captures = 0;
        after = {...before, [field]: 'changed'};
        await run(44, {isCurrent: () => true}, refresh);
        equal(events.length, 0, 'stale ' + field + ' blocks refreshed controls');
    }
    captures = 0;
    after = before;
    await run(44, {isCurrent: () => true}, refresh);
    equal(events.join(','), 'project', 'accepted refresh publishes controls');
    events.length = 0;
    captures = 0;
    refresh.bindingPrepare = async function rejectPreparation() {
        throw new Error('prepare unavailable');
    };
    await run(44, {isCurrent: () => true}, refresh);
    equal(events.join(','), 'Error: prepare unavailable', 'current preparation failure publishes diagnostic');
    events.length = 0;
    captures = 0;
    after = {...before, text: 'newer body'};
    await run(44, {isCurrent: () => true}, refresh);
    equal(events.length, 0, 'stale preparation failure cannot replace status');

    const input = {isConnected: false};
    const queued = {
        currentMode: () => 'matrix',
        bindingCommitInput(value: unknown) {
            equal(value, input, 'queue retains actual browser input');
            events.push('commit');
            return true;
        }
    };
    equal(await run(45, {mode: 'matrix', input}, queued), false, 'detached queued input is rejected');
    input.isConnected = true;
    equal(await run(45, {mode: 'equation', input}, queued), false, 'queued input cannot cross worksheet mode');
    equal(events.length, 0, 'rejected queue work never commits');
    equal(await run(45, {mode: 'matrix', input}, queued), true, 'current queue returns accepted commit value');
    equal(events.join(','), 'commit', 'queue starts one commit');

    const item = {isConnected: true}, rows = ['native row'];
    const rowServices = {
        commitBindings: async () => events.push('commit'),
        async bindingPrepareRows(index: number, operation: string) {
            equal(index, 2, 'row edit index');
            equal(operation, 'remove', 'row edit operation remains opaque');
            item.isConnected = false;
            return rows;
        },
        bindingRenderRows: () => events.push('render')
    };
    events.length = 0;
    await run(47, {item, index: 2, operation: 'remove'}, rowServices);
    equal(events.join(','), 'commit', 'detached row cannot publish prepared rows');
    item.isConnected = true;
    rowServices.bindingPrepareRows = async () => rows;
    Object.assign(rowServices, {
        bindingRefresh: () => events.push('refresh'),
        updateHistoryButtons: () => events.push('history'),
        currentMode: () => 'integrator',
        saveWorksheetState: (mode: string) => events.push('save:' + mode)
    });
    events.length = 0;
    await run(47, {item, index: 2, operation: 'remove'}, rowServices);
    equal(events.join(','), 'commit,render,refresh,history,save:integrator', 'row publication order');

    const control = document.createElement('input');
    // Plain fixture provides connectivity without adding controls to the real worksheet.
    const value = {isConnected: true, value: 'authored'};
    const prepare = {
        async bindingRefreshForms() {
            value.value = 'newer';
            return rows;
        },
        bindingUpdateForms: () => events.push('update')
    };
    events.length = 0;
    await run(48, {input: value}, prepare);
    equal(events.length, 0, 'changed authored input cannot publish old forms');
    value.value = 'authored';
    prepare.bindingRefreshForms = async () => rows;
    Object.assign(prepare, {currentMode: () => 'matrix'});
    await run(48, {input: value}, prepare);
    equal(events.join(','), 'update', 'non-Integrator mode does not persist Integrator state');

    const card = {input: control, copy: {}, message: 'Copied binding'};
    control.value = 'opaque π/7';
    const clipboard = {
        async bindingClipboard(text: string): Promise<void> {
            equal(text, control.value, 'clipboard receives exact authored text');
            throw new Error('clipboard unavailable');
        },
        bindingFlash(button: object, success: boolean) {
            equal(button, card.copy, 'clipboard retains browser button identity');
            events.push('flash:' + success);
        },
        setStatus: (message: string) => events.push(message),
        bindingReadyTimer: () => events.push('timer')
    };
    events.length = 0;
    await run(49, {card, operation: 2}, clipboard);
    equal(events.join(','), 'flash:false,Error: clipboard unavailable', 'clipboard failure never schedules Ready');
    events.length = 0;
    clipboard.bindingClipboard = async () => {};
    await run(49, {card, operation: 2}, clipboard);
    equal(events.join(','), 'flash:true,Copied binding,timer', 'clipboard success order');

    const updated = 'native updated source';
    const projection = {
        setStatus: (status: string) => events.push(status),
        async bindingFetch(): Promise<BindingProjectionResponse> {
            return {response: {ok: true}, data: {ok: true, binding_values: []}};
        },
        bindingPrepare: async (source: string) => equal(source, updated, 'binding projection prepares exact updated source'),
        requestCurrent: () => false
    };
    events.length = 0;
    equal(
        await run(52, {updated, editorSnapshot: 'visible', request: {}}, projection), false,
        'stale binding projection reports false');
    equal(events.join(','), 'Updating bindings...', 'stale projection does not publish completion or Ready');
    events.length = 0;
    projection.bindingFetch = async function rejectProjection() {
        return {response: {ok: false}, data: {error: 'binding failure'}};
    };
    projection.requestCurrent = () => true;
    equal(await run(52, {updated, request: {}}, projection), false, 'failed binding response reports false');
    equal(events.join(','), 'Updating bindings...,Error: binding failure', 'failed binding response diagnostic');

    const exactSource = '  { x + c | c = π/7 }  ';
    let kindPayload!: BindingKindPayload;
    equal(await run(55, {source: exactSource, name: '__proto__', nextKind: 'constant'}, {
        async bindingPresentation(payload: BindingKindPayload) {
            kindPayload = payload;
            return {editor: {expression: 'native replacement'}};
        },
        bindingExpression: (data: BindingEditorResponse) => data.editor.expression
    }), 'native replacement', 'native kind request returns the server expression');
    equal(kindPayload.action, 'editor', 'kind payload action');
    equal(kindPayload.operation, 'kind', 'kind payload operation');
    equal(kindPayload.text, exactSource, 'kind payload keeps exact source without parsing');
    equal(kindPayload.name, '__proto__', 'kind payload keeps opaque binding name');
    equal(kindPayload.kind, 'constant', 'kind payload retains selected binding category');
};

/** @brief Check detached capabilities and resource ownership with an isolated controller host. */
async function checkBindingControllerCallbacks(check: LabTestCheck): Promise<void> {
    function equal(actual: unknown, expected: unknown, message: string): void {
        check(Object.is(actual, expected), message);
    }

    const timers = new Map<number, {callback: () => void; delay: number}>();
    const cleared: number[] = [];
    const finished: WorksheetRequest[] = [];
    const statuses: string[] = [];
    let nextTimer = 0;
    const request: WorksheetRequest = {
        channel: 0, token: 1, controller: new AbortController(), timer: undefined, button: null, disableButton: false
    };
    let admitted: WorksheetRequest | null = request;
    function unexpected(): never {
        throw new Error('Unexpected binding fixture capability');
    }

    const host: WorksheetBindings.ControllerDependencies = {
        input: document.createElement('textarea'),
        // Unused dependencies fail immediately if a regression reaches outside this fixture.
        get sources() { return unexpected(); },
        get modeEditors() { return unexpected(); },
        get container() { return unexpected(); },
        get integratorContainer() { return unexpected(); },
        get defaultDatetimeText() { return unexpected(); },
        get wire() { return unexpected(); },
        get evaluationTransport() { return unexpected(); },
        get persistence() { return unexpected(); },
        get results() { return unexpected(); },
        get bindingEditor() { return unexpected(); },
        get workspaceBindingSelection() { return unexpected(); },
        get workspaceIntegrator() { return unexpected(); },
        get workspaceMode() { return unexpected(); },
        get workspaceLayout() { return unexpected(); },
        get forms() { return unexpected(); },
        get bindingPanel() { return unexpected(); },
        get bindingIntegratorPanel() { return unexpected(); },
        get bindingGoal() { return unexpected(); },
        dom: labDOM,
        buffer: {stageText: (text: string) => text.length},
        setTimer(callback: () => void, delay: number) {
            ++nextTimer;
            timers.set(nextTimer, {callback, delay});
            return nextTimer;
        },
        clearTimer(timer: number) {
            cleared.push(timer);
            timers.delete(timer);
        },
        requests: {
            begin: () => admitted,
            finish(owner: WorksheetRequest | null | undefined) {
                if (owner)
                    finished.push(owner);
            },
            run: unexpected,
            cancel: unexpected
        },
        flow: unexpected,
        requestPresentation: {prepareLabEditor: unexpected, requestLabPresentation: unexpected},
        workspacePresentation: {
            renderDerivativeButtons: unexpected,
            updateHistoryButtons: unexpected,
            setStatus(text: string) {
                statuses.push(text);
            }
        }
    };
    const controller = new WorksheetBindings.WorksheetBindingController(host);
    const snapshot = controller.labEditorSnapshot;
    const capabilities = controller.labBindingFlowServices;
    const dispose = controller.dispose;
    const services = capabilities();
    function dispatch(service: string, ...args: unknown[]): unknown {
        return labDOM.services({calls: [{service, args}]}, services);
    }
    const calls: unknown[][] = [];
    const guard = () => true;
    let completeRefresh = function completeIdleRefresh() {};
    let completion = Promise.resolve();
    try {
        host.input.value = '  fixture  ';
        equal(snapshot().text, 'fixture', 'detached controller snapshot retains its input');
        equal(snapshot().length, 7, 'detached controller snapshot stages trimmed text');
        equal(capabilities(), services, 'detached capability provider retains its cached registry');
        check(Object.isFrozen(services), 'binding capabilities remain immutable');

        // Replace methods after retaining their service callbacks, then check exact argument tuples.
        controller.setExpressionEditor = function recordEditor(this: WorksheetBindings.WorksheetBindingController, ...args) {
            equal(this, controller, 'late editor replacement receives its controller');
            calls.push(args);
        };
        dispatch('bindingSetEditor', 'source');
        dispatch('bindingSetEditor', 'source', undefined);
        dispatch('bindingSetEditor', 'source', null, null, false);
        equal(calls[0].length, 1, 'editor forwarding preserves omitted optional arguments');
        equal(calls[1].length, 2, 'editor forwarding preserves explicit undefined');
        equal(calls[1][1], undefined, 'editor forwarding does not normalise undefined');
        equal(calls[2].length, 4, 'editor forwarding preserves all supplied arguments');
        equal(calls[2][1], null, 'editor forwarding preserves explicit null');
        equal(calls[2][3], false, 'editor forwarding preserves false readiness');
        calls.length = 0;
        controller.setExpressionEditor = function recordSecondEditor(...args) {
            calls.push(['second', ...args]);
        };
        dispatch('bindingSetEditor', 'replacement');
        equal(calls[0][0], 'second', 'retained service resolves a second method replacement');
        equal(calls[0][1], 'replacement', 'replacement still receives the forwarded source');

        calls.length = 0;
        const presentation = host.requestPresentation;
        presentation.requestLabPresentation = async function recordPresentation(this: typeof presentation, ...args) {
            equal(this, presentation, 'late presentation replacement receives its host');
            calls.push(args);
            return {};
        };
        const payload = {text: 'opaque source'};
        await dispatch('bindingPresentation', payload);
        await dispatch('bindingPresentation', payload, undefined);
        await dispatch('bindingPresentation', payload, null);
        await dispatch('bindingPresentation', payload, request);
        equal(calls[0].length, 1, 'presentation forwarding preserves omitted request');
        equal(calls[1].length, 2, 'presentation forwarding preserves explicit undefined request');
        equal(calls[1][1], undefined, 'presentation request remains undefined');
        equal(calls[2][1], null, 'presentation request remains null');
        equal(calls[3][0], payload, 'presentation forwarding preserves payload identity');
        equal(calls[3][1], request, 'presentation forwarding preserves request identity');

        calls.length = 0;
        controller.applyMarsBindingExpression = async function recordProjection(
            this: WorksheetBindings.WorksheetBindingController, ...args
        ) {
            equal(this, controller, 'commit effect resolves its controller method late');
            calls.push(args);
            return true;
        };
        controller.refreshVariableValuesFromEditor = async function recordRefresh(...args) {
            calls.push(args);
        };
        for (const args of [['source'], ['source', undefined], ['source', null]])
            await dispatch('bindingEffects', {calls: [{service: 'applyMarsBindingExpression', args}]});
        for (const args of [[], [undefined], [guard]])
            await dispatch('bindingEffects', {calls: [{service: 'refreshVariableValuesFromEditor', args}]});
        equal(calls.map(args => args.length).join(','), '1,2,2,0,1,1',
            'commit effects preserve omitted and supplied optional argument counts');
        equal(calls[1][1], undefined, 'commit effect preserves explicit undefined body');
        equal(calls[2][1], null, 'commit effect preserves explicit null body');
        equal(calls[4][0], undefined, 'refresh effect preserves explicit undefined guard');
        equal(calls[5][0], guard, 'refresh effect preserves guard identity');

        // Timer callbacks and request completion are driven explicitly; no real timers or requests run.
        const context: {request?: WorksheetRequest | null} = {};
        equal(dispatch('bindingBeginRefresh', context, 'bindings', 'expression', {input: true}), request,
            'dispatched refresh admission returns the owned request');
        equal(context.request, request, 'refresh admission publishes the same owner');
        admitted = null;
        equal(dispatch('bindingBeginRefresh', {}, 'bindings', 'expression', {input: false}), null,
            'rejected refresh admission has no owner');
        const ready = dispatch('bindingReadyTimer');
        if (typeof ready !== 'number')
            throw new Error('Binding feedback service must return a timer handle');
        equal(timers.get(ready)!.delay, 1000, 'feedback timer retains its delay');
        const readyCallback = timers.get(ready)!.callback;
        timers.delete(ready);
        readyCallback();
        equal(statuses.join(','), 'Ready', 'dispatched feedback capability schedules status restoration');
        const pendingReady = dispatch('bindingReadyTimer');
        if (typeof pendingReady !== 'number')
            throw new Error('Binding feedback service must return a timer handle');
        dispatch('bindingRefreshTimer', 'body', 'source', request, 300);
        const refreshTimer = controller.expressionBindingRefreshTimer;
        equal(timers.get(refreshTimer)!.delay, 300, 'refresh timer preserves its requested delay');
        const unrelatedTimer = host.setTimer(unexpected, 17);
        dispose();
        equal(controller.expressionBindingRefreshTimer, 0, 'disposal resets the refresh timer handle');
        equal(finished.length, 1, 'disposal finishes only admitted refresh requests');
        equal(finished[0], request, 'disposal retains refresh request identity');
        check(cleared.includes(refreshTimer) && cleared.includes(pendingReady), 'disposal cancels both owned timers');
        check(!cleared.includes(ready), 'completed feedback timer is no longer disposal-owned');
        equal(timers.size, 1, 'disposal leaves only the unrelated timer');
        check(timers.has(unrelatedTimer), 'disposal preserves unrelated timer ownership');
        dispose();
        equal(finished.length, 1, 'repeated disposal does not finish a released request again');

        admitted = request;
        dispatch('bindingBeginRefresh', {}, 'bindings', 'expression', {input: true});
        completion = new Promise<void>(function captureRefreshCompletion(resolve) {
            completeRefresh = resolve;
        });
        let captured: object = {};
        let capturedKind = 0;
        host.flow = function captureFlow(kind, frame): Promise<unknown> {
            capturedKind = kind;
            captured = frame;
            return completion;
        };
        dispatch('bindingRefreshTimer', 'edited body', 'exact source', request, 300);
        const runningTimer = controller.expressionBindingRefreshTimer;
        const refreshCallback = timers.get(runningTimer)!.callback;
        timers.delete(runningTimer);
        refreshCallback();
        equal(capturedKind, 54, 'refresh timer enters the native refresh continuation');
        equal(Reflect.get(captured, 'editedBody'), 'edited body', 'timer retains the edited body');
        equal(Reflect.get(captured, 'sourceExpression'), 'exact source', 'timer retains the source expression');
        equal(Reflect.get(captured, 'request'), request, 'timer retains the admitted owner');
        equal(finished.length, 1, 'running refresh retains ownership until completion');
        completeRefresh();
        await completion;
        await Promise.resolve();
        equal(finished.length, 2, 'completed refresh finishes its request');
        dispose();
        equal(finished.length, 2, 'completed refresh is removed from disposal ownership');
    } finally {
        completeRefresh();
        await completion;
        await Promise.resolve();
        dispose();
        timers.clear();
    }
}
