/**
 * @file binding_flow_checks.ts
 * @brief Binding continuation and browser-resource boundary regressions.
 *
 * Exercises typed event routing and native service plans with controlled asynchronous ownership and binding fixtures.
 */

/** Binding continuation ordering, asynchronous ownership and browser-resource boundaries. */
window.checkLabBindingFlows = async function checkLabBindingFlows() {
    const check: LabTestCheck = (condition, message) => {
        if (!condition)
            throw new Error('Binding flow: ' + message);
    };
    const equal = (actual: unknown, expected: unknown, message: string) => check(Object.is(actual, expected), message);
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
        router.install(root, 'lab_binding_event', {commit: () => { throw new Error('Duplicate router subscription'); }});
        input.dispatchEvent(new Event('change'));
        equal(commits, 1, 'one capture subscription handles non-bubbling events');
        handlers.commit = action => {
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
    }
    const run = async (kind: number, frame: Record<string, unknown>, services: NativeServiceRegistry) => {
        const registry = {
            native: (entry: string, ...args: unknown[]) => labDOM.call(entry, ...args),
            fail: (message: string) => {
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
    };
    const events: string[] = [];
    const services = (plan: BindingTestPlan) => plan.calls.map(call => call.service).join(',');
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

    const refreshContext: {editedBody: string; sourceExpression: string; request?: object | null} = {editedBody: '  \u00a0opaque body\n ', sourceExpression: ' exact native source '};
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
        bindingBeginRefresh: (frame: typeof refreshContext, operation: string, mode: string, options: {input: boolean}) => {
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
        bindingPending: async (): Promise<number | void> => {
            events.push('pending');
            live = false;
        },
        bindingCommitVisible: () => {
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
        bindingCapture: () => captures++ ? after : before,
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
    refresh.bindingPrepare = async () => {
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
        bindingCommitInput: (value: unknown) => {
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
        bindingPrepareRows: async (index: number, operation: string) => {
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
    const prepare: NativeServiceRegistry & {bindingRefreshForms: () => Promise<string[]>; currentMode?: () => string} = {
        bindingRefreshForms: async () => {
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
    prepare.currentMode = () => 'matrix';
    await run(48, {input: value}, prepare);
    equal(events.join(','), 'update', 'non-Integrator mode does not persist Integrator state');

    const card = {input: control, copy: {}, message: 'Copied binding'};
    control.value = 'opaque π/7';
    const clipboard = {
        bindingClipboard: async (text: string): Promise<void> => {
            equal(text, control.value, 'clipboard receives exact authored text');
            throw new Error('clipboard unavailable');
        },
        bindingFlash: (button: object, success: boolean) => {
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
        bindingFetch: async (): Promise<{response: {ok: boolean}; data: {ok?: boolean; binding_values?: unknown[]; error?: string}}> => ({response: {ok: true}, data: {ok: true, binding_values: []}}),
        bindingPrepare: async (source: string) => equal(source, updated, 'binding projection prepares exact updated source'),
        requestCurrent: () => false
    };
    events.length = 0;
    equal(
        await run(52, {updated, editorSnapshot: 'visible', request: {}}, projection), false,
        'stale binding projection reports false');
    equal(events.join(','), 'Updating bindings...', 'stale projection does not publish completion or Ready');
    events.length = 0;
    projection.bindingFetch = async () => ({response: {ok: false}, data: {error: 'binding failure'}});
    projection.requestCurrent = () => true;
    equal(await run(52, {updated, request: {}}, projection), false, 'failed binding response reports false');
    equal(events.join(','), 'Updating bindings...,Error: binding failure', 'failed binding response diagnostic');

    const exactSource = '  { x + c | c = π/7 }  ';
    let kindPayload!: Record<string, unknown>;
    equal(await run(55, {source: exactSource, name: '__proto__', nextKind: 'constant'}, {
        bindingPresentation: async (payload: Record<string, unknown>) => {
            kindPayload = payload;
            return {editor: {expression: 'native replacement'}};
        },
        bindingExpression: (data: {editor: {expression: string}}) => data.editor.expression
    }), 'native replacement', 'native kind request returns the server expression');
    equal(kindPayload.action, 'editor', 'kind payload action');
    equal(kindPayload.operation, 'kind', 'kind payload operation');
    equal(kindPayload.text, exactSource, 'kind payload keeps exact source without parsing');
    equal(kindPayload.name, '__proto__', 'kind payload keeps opaque binding name');
    equal(kindPayload.kind, 'constant', 'kind payload retains selected binding category');
};
