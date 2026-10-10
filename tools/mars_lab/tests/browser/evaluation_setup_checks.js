/** Native setup phases, lazy editor snapshots and asynchronous request ownership regressions. */
window.checkLabEvaluationSetup = async function checkLabEvaluationSetup() {
    const check = (condition, message) => {
        if (!condition)
            throw new Error('Evaluation setup: ' + message);
    };
    const equal = (actual, expected, message) => check(Object.is(actual, expected), message);
    const names = plan => plan.calls.map(call => call.service).join(',');
    const plan = (mode, phase, context, options = {}, view = {}) =>
        labDOM.call('lab_evaluation_setup', mode, phase, context, options, view);
    const forbidden = Object.freeze({
        get expressionText() {
            throw new Error('Unexpected expressionText read');
        },
        get bodyText() {
            throw new Error('Unexpected bodyText read');
        },
        get lastInput() {
            throw new Error('Unexpected lastInput read');
        }
    });
    const watched = values => {
        const reads = [];
        const view = {};
        for (const key of ['expressionText', 'bodyText', 'lastInput']) {
            Object.defineProperty(view, key, {
                enumerable: true,
                get() {
                    reads.push(key);
                    check(Object.hasOwn(values, key), 'skipped getter must remain unread: ' + key);
                    return values[key];
                }
            });
        }
        return {view: Object.freeze(view), reads};
    };
    const initialNames =
        ['captureBindings,commitBindings', 'commitBindings', '', 'commitBindings', 'commitBindings', '', ''];
    const beforeEditor = expr.value, beforeRendered = rendered.innerHTML;
    for (let mode = 0; mode < 7; ++mode) {
        const context = Object.freeze({sentinel: 'retained'});
        const result = plan(mode, 0, context, {}, forbidden);
        equal(names(result), initialNames[mode], 'phase-zero service order for mode ' + mode);
        equal(result.wait, [0, 1, 3, 4].includes(mode), 'phase-zero wait policy');
        equal(result.done, false, 'phase zero never finishes setup');
        equal(Object.keys(context).join(','), 'sentinel', 'phase zero cannot mutate captured context');
        if (mode === 0) {
            equal(result.calls[0].args[0], context, 'capture service receives actual context');
            equal(result.calls[1].args.length, 0, 'binding commit has no invented arguments');
        }
    }
    for (const [mode, phase] of [[-1, 0], [7, 0], [2147483647, 0], [0, -1], [0, 3]])
        equal(plan(mode, phase, Object.freeze({}), {}, forbidden), null, 'invalid phase or mode is inert');
    for (let mode = 1; mode < 7; ++mode)
        equal(plan(mode, 2, Object.freeze({}), {}, forbidden), null, 'only Expression has phase two');

    const enteredBindings = Object.freeze([{name: 'μ', value: 'π/7'}]);
    const source = '  ∫ opaque native mathematics { μ = π/7 }  ';
    const captured = {enteredBindings, unrelated: 'retained'};
    const editor = watched({expressionText: source, bodyText: '\u00a0 body \n'});
    const assembly = plan(0, 1, captured, {}, editor.view);
    equal(captured.editorText, source, 'Expression retains authored source bytes');
    equal(captured.editorBodyText, 'body', 'Expression trims only the body snapshot');
    equal(captured.enteredBindings, enteredBindings, 'captured binding identity survives setup');
    equal(captured.unrelated, 'retained', 'unrelated context is preserved');
    check(!Object.hasOwn(captured, 'text'), 'phase one cannot select final Expression text');
    equal(names(assembly), 'assemble', 'phase one requests assembly only');
    equal(assembly.calls[0].args[0], captured, 'assembler receives the same mutable context');
    equal(assembly.wait, true, 'assembly must finish before selecting input');
    equal(assembly.done, false, 'Expression requires final phase');
    equal(editor.reads.join(','), 'expressionText,bodyText', 'Expression reads only its current editor snapshot');
    for (const body of [undefined, null, false, 0, '', 42]) {
        const context = {};
        plan(0, 1, context, {}, watched({expressionText: source, bodyText: body}).view);
        equal(context.editorBodyText, String(body || '').trim(), 'body snapshot follows String/falsey semantics');
    }

    for (const mode of [1, 2, 3, 4]) {
        for (const expression of [source, 42]) {
            const context = {unrelated: 'retained'};
            const snapshot = watched({expressionText: expression});
            const result = plan(mode, 1, context, {}, snapshot.view);
            equal(
                context.text, mode <= 2 ? String(expression).trim() : expression,
                'mode-specific source trimming and coercion');
            equal(snapshot.reads.join(','), 'expressionText', 'truthy source skips body and last-input getters');
            equal(names(result), '', 'mathematical snapshot requires no host service');
            equal(result.wait, false, 'mathematical snapshot does not await');
            equal(result.done, true, 'mathematical snapshot completes setup');
            equal(context.unrelated, 'retained', 'mathematical snapshot preserves unrelated context');
        }
        for (const expression of [undefined, null, false, 0, '']) {
            const context = {};
            const snapshot = watched({expressionText: expression, bodyText: '  fallback μ \n'});
            plan(mode, 1, context, {}, snapshot.view);
            equal(context.text, 'fallback μ', 'falsey source falls back to trimmed body');
            equal(snapshot.reads.join(','), 'expressionText,bodyText', 'fallback getter read order');
        }
        const context = {};
        plan(mode, 1, context, {}, watched({expressionText: '   '}).view);
        equal(context.text, mode <= 2 ? '' : '   ', 'truthy whitespace never selects fallback body');
    }
    for (const mode of [5, 6]) {
        const context = Object.freeze({sentinel: 'calendar'});
        const result = plan(mode, 1, context, {}, forbidden);
        equal(names(result), mode === 5 ? 'captureDatetime' : 'captureAlmanac', 'calendar capture dispatch');
        equal(result.calls[0].args[0], context, 'calendar capture receives actual context');
        equal(result.wait, false, 'calendar capture is synchronous');
        equal(result.done, true, 'calendar capture finishes setup');
        equal(Object.keys(context).join(','), 'sentinel', 'calendar plan defers context mutation to its service');
    }
    for (const entered of ['', undefined, '  exact assembled source  ']) {
        const context = {editorText: source, entered};
        const result = plan(0, 2, context, {}, forbidden);
        equal(context.text, entered || source, 'final input uses assembled text then authored source');
        equal(names(result), 'saveWorksheetState', 'final phase requests one persistence operation');
        equal(result.calls[0].args[0], 'expression', 'save targets Expression');
        equal(result.calls[0].args[1], source, 'persistence retains authored editor even when assembled differs');
        equal(result.wait, false, 'persistence is not awaited by setup');
        equal(result.done, true, 'phase two completes Expression setup');
    }
    for (const lastInput of ['', undefined, '  previous native request  ']) {
        const context = {editorText: '', entered: 'assembled'};
        const snapshot = watched({lastInput});
        const result = plan(0, 2, context, {reuseLastInput: true}, snapshot.view);
        equal(context.text, lastInput || 'assembled', 'reuse requires a truthy previous input');
        equal(result.calls[0].args[1], context.text, 'empty editor saves selected final input');
        check(
            snapshot.reads.every(key => key === 'lastInput') && snapshot.reads.length > 0,
            'reuse reads only the last-input getter');
    }
    const lazyContext = {editorText: 'author'};
    Object.defineProperty(lazyContext, 'entered', {
        get() {
            throw new Error('Unexpected assembled-input read');
        }
    });
    plan(0, 2, lazyContext, {reuseLastInput: true}, watched({lastInput: 'reused'}).view);
    equal(lazyContext.text, 'reused', 'successful reuse skips assembled fallback');
    equal(expr.value, beforeEditor, 'setup plans never project editor DOM');
    equal(rendered.innerHTML, beforeRendered, 'setup plans never project result DOM');

    // Exercise the production getter inside a DOM scope: source resolution uses direct workspace exports,
    // not a nested labDOM.call. Matching compact text deliberately selects native-owned full source.
    const sourceFields = ['fullText', 'displayText', 'lastInput'];
    const savedSources = Object.fromEntries(sourceFields.map(field => [field, labEditorState[field]]));
    const savedBody = expr.value;
    try {
        const compact = 'setup fixture compact μ';
        const full = '{ opaque native source | μ = π/7 }';
        labEditorState.displayText = compact;
        labEditorState.fullText = full;
        labEditorState.lastInput = 'previous exact native request';
        expr.value = '  ' + compact + '  ';
        const context = {};
        equal(
            names(plan(0, 0, context, {}, labEvaluationView)), 'captureBindings,commitBindings',
            'real live view is accepted without eager setup reads');
        plan(0, 1, context, {}, labEvaluationView);
        equal(context.editorText, full, 'production expression getter resolves full text through native exports');
        equal(context.editorBodyText, compact, 'production body getter independently snapshots visible text');
        equal(expr.value, '  ' + compact + '  ', 'real getter does not rewrite the editor');
        labEditorState.fullText = 'updated native-owned source';
        const updated = {};
        plan(0, 1, updated, {}, labEvaluationView);
        equal(updated.editorText, 'updated native-owned source', 'production getter is live rather than cached');
        plan(0, 2, updated, {reuseLastInput: true}, labEvaluationView);
        equal(updated.text, 'previous exact native request', 'real last-input getter reads native source storage');
    } finally {
        for (const field of sourceFields) labEditorState[field] = savedSources[field];
        expr.value = savedBody;
    }

    // The generic executor is synchronous: it returns, but does not unwrap, the last service promise.
    let resolveCommit;
    const commitment = new Promise(resolve => {
        resolveCommit = resolve;
    });
    const sequence = [], context = {};
    const registry = {
        captureBindings: target => {
            sequence.push('capture');
            target.enteredBindings = enteredBindings;
            return 'not the final return value';
        },
        commitBindings: () => {
            sequence.push('commit');
            return commitment;
        }
    };
    const returned = labDOM.services(plan(0, 0, context, {}, forbidden), registry);
    equal(returned, commitment, 'executor returns exact last callback promise without awaiting');
    equal(sequence.join(','), 'capture,commit', 'capture precedes asynchronous binding commit');
    equal(context.enteredBindings, enteredBindings, 'service mutation reaches shared context');
    resolveCommit('committed');
    equal(await returned, 'committed', 'host can await the selected final promise');
    equal(labDOM.services({calls: []}, registry), undefined, 'empty service list returns undefined');
    equal(
        labDOM.services({calls: [{service: 'answer', args: []}]}, {answer: () => 42}), 42,
        'synchronous last callback result is retained');
    const failure = new Error('setup service failure');
    let caught, committed = false;
    try {
        labDOM.services(plan(0, 0, {}, {}, forbidden), {
            captureBindings: () => {
                throw failure;
            },
            commitBindings: () => {
                committed = true;
            }
        });
    } catch (error) {
        caught = error;
    }
    equal(caught, failure, 'synchronous setup failure propagates unchanged');
    equal(committed, false, 'capture failure stops before binding commit');

    // Real adapter setup failures and stale assembly must not advance to request/history/persistence.
    const originalMode = currentMode(), originals = new Map(), events = [];
    const record = name => (...args) => events.push({name, args});
    const replace = (name, callback) => {
        if (!originals.has(name))
            originals.set(name, window[name]);
        window[name] = callback;
    };
    const oldCall = labDOM.call;
    const adapterEditor = {
        body: expr.value,
        fullText: labEditorState.fullText,
        displayText: labEditorState.displayText
    };
    let pending = null, release = null, newer = null;
    try {
        replace('currentMode', () => 'expression');
        expr.value = 'evaluation setup compact fixture';
        labEditorState.displayText = expr.value;
        labEditorState.fullText = source;
        replace('visibleBindingValues', () => enteredBindings);
        replace('commitVisibleBindingInputs', async () => {});
        for (const name
                 of ['saveWorksheetState', 'pushExpressionHistory', 'previousModeStateForHistory', 'commitModeState',
                     'updateHistoryButtons', 'showResults', 'setStatus', 'clearResultDetails', 'setBusy',
                     'setActionRunning'])
            replace(name, record(name));
        replace('fetchEvaluation', async () => {
            events.push({name: 'fetch'});
            return {response: {ok: true}, data: {}};
        });
        labDOM.call = (name, ...args) => {
            if (name === 'lab_result_solver_invalidate')
                return;
            if (name === 'lab_layout_error' || name === 'lab_layout_more') {
                events.push({name, args});
                return;
            }
            return oldCall(name, ...args);
        };
        labRequests.modeChanged('expression');
        replace('expressionWithVisibleBindings', async () => {
            throw failure;
        });
        await evaluateExpression();
        equal(
            events.find(event => event.name === 'lab_layout_error')?.args[0], String(failure),
            'assembly rejection reaches the outer UI error guard');
        const downstream = new Set([
            'saveWorksheetState', 'pushExpressionHistory', 'previousModeStateForHistory', 'commitModeState',
            'updateHistoryButtons', 'fetch'
        ]);
        equal(
            events.filter(event => downstream.has(event.name)).length, 0,
            'assembly rejection cannot enter history, persistence or fetch');
        check(!labRequests.busy(), 'rejected assembly releases its busy owner');

        events.length = 0;
        let entered;
        const reached = new Promise(resolve => {
            entered = resolve;
        });
        const gate = new Promise(resolve => {
            release = resolve;
        });
        replace('expressionWithVisibleBindings', () => {
            entered();
            return gate;
        });
        pending = evaluateExpression();
        await Promise.race([
            reached, pending.then(() => {
                throw new Error('Evaluation setup: adapter completed before entering assembly');
            })
        ]);
        newer = labRequests.begin('evaluate', 'expression');
        check(newer, 'replacement evaluation owns the worksheet');
        events.length = 0;
        release('obsolete assembled input');
        await pending;
        equal(
            events
                .filter(
                    event =>
                        downstream.has(event.name) || event.name === 'setStatus' || event.name === 'lab_layout_error')
                .length,
            0, 'stale assembly completion cannot publish or persist');
        check(labRequests.busy(), 'stale finaliser preserves replacement busy state');
    } finally {
        if (release)
            release('fixture cleanup');
        if (pending) {
            try {
                await pending;
            } catch (_) { /* Preserve the original assertion while restoring the fixture. */
            }
        }
        if (newer)
            labRequests.finish(newer);
        labDOM.call = oldCall;
        expr.value = adapterEditor.body;
        labEditorState.fullText = adapterEditor.fullText;
        labEditorState.displayText = adapterEditor.displayText;
        for (const [name, callback] of originals) window[name] = callback;
        labRequests.modeChanged(originalMode);
    }
};
