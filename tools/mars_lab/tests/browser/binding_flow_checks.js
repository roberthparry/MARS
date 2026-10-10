/** Binding continuation ordering, asynchronous ownership and browser-resource boundaries. */
window.checkLabBindingFlows = async function checkLabBindingFlows() {
    const check = (condition, message) => {
        if (!condition)
            throw new Error('Binding flow: ' + message);
    };
    const equal = (actual, expected, message) => check(Object.is(actual, expected), message);
    const run = async (kind, frame, services) => {
        const registry = {
            native: (entry, ...args) => labDOM.call(entry, ...args),
            fail: message => {
                throw new Error(message);
            },
            ...services
        };
        for (let step = 0; step < 80; ++step) {
            try {
                const plan = labDOM.call('lab_flow_step', kind, frame, {});
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
    const events = [];
    const services = plan => plan.calls.map(call => call.service).join(',');
    const clear = labDOM.call('lab_binding_projection_clear', 0);
    equal(services(clear), 'bindingSource,bindingSource,bindingSource,bindingResetCache,bindingClearFlags,bindingClearGoal',
          'full source clearing order');
    equal(clear.calls.slice(0, 3).map(call => call.args[0]).join(','), 'fullText,displayText,lastInput',
          'source clearing selects only authored fields');
    const goalOnly = labDOM.call('lab_binding_projection_clear', 1);
    equal(services(goalOnly), 'bindingSource,bindingSource', 'goal clearing cannot clear controls or source');
    equal(goalOnly.calls.map(call => call.args[0]).join(','), 'goalSource,goalTarget', 'goal field selection');
    for (let mode = 0; mode < 7; ++mode) {
        const plain = labDOM.call('lab_binding_projection_update', mode, '  opaque source  ', null);
        equal(services(plain), mode < 3 ? 'bindingSetEditor' : 'bindingSetText,bindingClearSource,bindingClearValues',
              'synchronous mode-dependent projection');
        equal(services(labDOM.call('lab_binding_projection_update', mode, 'opaque source', {wrapped: true})),
              'bindingSetEditor', 'wrapped metadata always selects complete editor projection');
    }
    const context = {fullText: 'source'};
    const deferred = labDOM.call('lab_binding_projection_prepare', context, null);
    equal(services(deferred), 'bindingDeferEditor', 'missing metadata requires asynchronous preparation');
    equal(deferred.calls[0].args[0], context, 'deferred preparation preserves actual context');
    equal(services(labDOM.call('lab_binding_projection_prepare', context, {})), '', 'cached metadata skips preparation');
    const selected = {fullText: 'full', displayText: 'body', bindings: [], refresh: true};
    equal(services(labDOM.call('lab_binding_projection_finish', selected)),
          'bindingSource,bindingSource,bindingResize,bindingRenderValues,bindingSetVariables,bindingRenderDerivatives,' +
              'bindingScheduleRefresh', 'synchronous installation precedes scheduled refresh');
    selected.refresh = false;
    check(!services(labDOM.call('lab_binding_projection_finish', selected)).includes('bindingScheduleRefresh'),
          'explicit readiness avoids refresh scheduling');

    const refreshContext = {editedBody: '  \u00a0opaque body\n ', sourceExpression: ' exact native source '};
    const admission = labDOM.call('lab_binding_projection_refresh', 0, refreshContext);
    equal(services(admission), 'bindingCancelRefresh,bindingBeginRefresh', 'timer cancellation precedes request admission');
    equal(refreshContext.editedBody, 'opaque body', 'refresh body uses browser whitespace trimming');
    equal(refreshContext.sourceExpression, ' exact native source ', 'refresh retains exact native source');
    equal(admission.calls[1].args[0], refreshContext, 'request callback retains the frame by identity');
    equal(admission.calls[1].args[1], 'bindings', 'native refresh request operation');
    equal(admission.calls[1].args[2], 'expression', 'native refresh request mode');
    equal(admission.calls[1].args[3].input, true, 'nonblank body admits input subject to existing request guards');
    equal(services(labDOM.call('lab_binding_projection_refresh', 1, refreshContext)), '',
          'absent request cannot mark controls or schedule a timer');
    refreshContext.request = null;
    equal(services(labDOM.call('lab_binding_projection_refresh', 1, refreshContext)), '',
          'rejected request cannot mark controls or schedule a timer');
    const owner = {};
    labDOM.services(admission, {
        bindingCancelRefresh: () => events.push('cancel'),
        bindingBeginRefresh: (frame, operation, mode, options) => {
            events.push('begin');
            equal(operation, 'bindings', 'host receives native operation without reinterpretation');
            equal(mode, 'expression', 'host preserves request current-mode guard selection');
            equal(options.input, true, 'host preserves native input admission flag');
            frame.request = owner;
        }
    });
    equal(events.join(','), 'cancel,begin', 'admission services execute outside native scope in order');
    const scheduled = labDOM.call('lab_binding_projection_refresh', 1, refreshContext);
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
    equal(labDOM.call('lab_binding_projection_refresh', 0, blank).calls[1].args[3].input, false,
          'blank refresh retains input rejection through native request admission');
    equal(services(labDOM.call('lab_binding_projection_refresh', 2, refreshContext)), '', 'invalid phase is inert');
    events.length = 0;
    let live = false;
    const visible = {
        bindingCurrent: callback => callback(),
        bindingPending: async () => {
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
        bindingCurrent: callback => callback(),
        bindingCapture: () => captures++ ? after : before,
        bindingPrepare: async source => equal(source, before.source, 'preparation receives exact authored source'),
        bindingProjectVisible: () => events.push('project'),
        bindingScheduleRefresh: () => events.push('schedule'),
        setStatus: message => events.push(message)
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
        bindingCommitInput: value => {
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
        bindingPrepareRows: async (index, operation) => {
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
        saveWorksheetState: mode => events.push('save:' + mode)
    });
    events.length = 0;
    await run(47, {item, index: 2, operation: 'remove'}, rowServices);
    equal(events.join(','), 'commit,render,refresh,history,save:integrator', 'row publication order');

    const control = document.createElement('input');
    // Plain fixture provides connectivity without adding controls to the real worksheet.
    const value = {isConnected: true, value: 'authored'};
    const prepare = {
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
        bindingClipboard: async text => {
            equal(text, control.value, 'clipboard receives exact authored text');
            throw new Error('clipboard unavailable');
        },
        bindingFlash: (button, success) => {
            equal(button, card.copy, 'clipboard retains browser button identity');
            events.push('flash:' + success);
        },
        setStatus: message => events.push(message),
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
        setStatus: status => events.push(status),
        bindingFetch: async () => ({response: {ok: true}, data: {ok: true, binding_values: []}}),
        bindingPrepare: async source => equal(source, updated, 'binding projection prepares exact updated source'),
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
    let kindPayload;
    equal(await run(55, {source: exactSource, name: '__proto__', nextKind: 'constant'}, {
        bindingPresentation: async payload => {
            kindPayload = payload;
            return {editor: {expression: 'native replacement'}};
        },
        bindingExpression: data => data.editor.expression
    }), 'native replacement', 'native kind request returns the server expression');
    equal(kindPayload.action, 'editor', 'kind payload action');
    equal(kindPayload.operation, 'kind', 'kind payload operation');
    equal(kindPayload.text, exactSource, 'kind payload keeps exact source without parsing');
    equal(kindPayload.name, '__proto__', 'kind payload keeps opaque binding name');
    equal(kindPayload.kind, 'constant', 'kind payload retains selected binding category');
};
