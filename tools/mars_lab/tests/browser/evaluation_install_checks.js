/** Ordered native installer plans and browser completion boundaries; run after Lab readiness. */
window.checkLabEvaluationInstall = async function checkLabEvaluationInstall() {
    const check = (condition, message) => {
        if (!condition)
            throw new Error('Evaluation install: ' + message);
    };
    const equal = (actual, expected, message) => check(Object.is(actual, expected), message);
    const names = plan => plan.calls.map(call => call.service).join(',');
    const service = (plan, name) => plan.calls.find(call => call.service === name);
    const modes = ['expression', 'equation', 'diffequation', 'matrix', 'integrator', 'datetime', 'almanac'];
    const bindings = Object.freeze([
        Object.freeze({name: ' x ', value: 'π/7'}), Object.freeze({name: 'c', kind: 'constant', value: '?'}),
        Object.freeze({name: '__proto__', value: 'opaque native value'})
    ]);
    const data = Object.freeze({expression: 'native expression', binding_values: bindings, evaluation_ready: false});
    const context = Object.freeze({text: 'authored source', editorText: 'authored source', editorBodyText: 'body'});
    const cards = Object.freeze({
        expression: 'expression card',
        function: 'function card',
        value: 'value card',
        derivative: 'native derivative',
        differentiable: true,
        expandable: true,
        scalar: 'native scalar',
        calendar: Object.freeze(Array.from({length: 4}, (_, index) => Object.freeze({text: 'calendar ' + index})))
    });
    let fullText = 'before editor', expressionText = 'before matrix', fullReads = 0, expressionReads = 0;
    const view = Object.freeze({
        editors: new Map(),
        get fullText() {
            ++fullReads;
            return fullText;
        },
        get expressionText() {
            ++expressionReads;
            return expressionText;
        }
    });
    const plan = (mode, phase, response = data, captured = context, outcome = 2) =>
        labDOM.call('lab_evaluation_install', mode, phase, outcome, response, captured, cards, view);
    const expected = [
        [
            'setRenderedResult,setExpressionEditor',
            'installEvaluationTextCards,present,source,saveWorksheetState,derivative,variables'
        ],
        ['render,installEvaluationTextCards,renderVariableValues', 'modeSource,saveWorksheetState,variables'],
        [
            'render,scheduleRenderedTeXFit,solverTextCards,present',
            'setValueText,setValueCardVisible,clearVariableValues,modeSource,saveWorksheetState,variables'
        ],
        ['displayMatrixResult,scalar,present,setExpressionEditor', 'modeSource,saveWorksheetState,variables'],
        [
            'render,installEvaluationTextCards,applyIntegratorBindingState,applyIntegratorResultBound',
            'saveWorksheetState,variables'
        ],
        [
            'calendarCard,calendarCard,calendarCard,calendarCard,present,setDatetimeLocalText,' +
                'applyCalendarEvaluationFields,setResultInputText',
            'clearVariableValues,variables,saveLastDatetimeState'
        ],
        [
            'almanacRender,almanacAccept,refreshAlmanacLandTotality,present,installEvaluationTextCards,' +
                'applyCalendarEvaluationFields',
            'clearVariableValues,variables,saveLastAlmanacState'
        ]
    ];
    const beforeDOM = rendered.innerHTML, beforeEditor = expr.value;
    for (let mode = 0; mode < modes.length; ++mode) {
        for (let phase = 0; phase < 2; ++phase)
            equal(names(plan(mode, phase)), expected[mode][phase], modes[mode] + ' phase ' + phase + ' service order');
    }
    equal(rendered.innerHTML, beforeDOM, 'creating plans cannot project result DOM');
    equal(expr.value, beforeEditor, 'creating plans cannot project editor DOM');
    equal(bindings[0].value, 'π/7', 'frozen native bindings remain untouched');
    equal(view.fullText, 'before editor', 'live view property was not overwritten');
    check(
        typeof Object.getOwnPropertyDescriptor(view, 'fullText').get === 'function', 'live getter remains an accessor');
    equal(service(plan(0, 0), 'setRenderedResult').args[0], data, 'native payload identity retained');
    equal(service(plan(0, 0), 'setExpressionEditor').args[3], false, 'false readiness is not defaulted');
    equal(service(plan(0, 1), 'installEvaluationTextCards').args[0], cards, 'card record identity retained');
    equal(service(plan(0, 1), 'variables').args[0].join(','), 'x,__proto__', 'constants excluded without special keys');
    equal(service(plan(1, 0), 'render').args[2], true, 'equation expansion is retained');
    equal(service(plan(2, 1), 'setValueCardVisible').args[0], true, 'solver value card is explicitly visible');
    equal(plan(5, 0).calls[3].args[0], cards.calendar[3], 'calendar card records retain identity');

    const authored = {name: 'x', value: 'authored π/5', display: 'authored display'};
    const visible = {name: 'x', value: 'visible α + β', display: 'visible display'};
    const editorView = {editors: new Map([[context.text, {bindings: [authored]}]])};
    const overlay = captured =>
        service(
            labDOM.call('lab_evaluation_install', 0, 0, 2, data, captured, cards, editorView), 'setExpressionEditor')
            .args[1];
    equal(overlay(context)[0].value, authored.value, 'cached authored value overrides native discovery value');
    const visibleOverlay = overlay({...context, enteredBindings: [visible]});
    equal(visibleOverlay[0].value, visible.value, 'visible authored value has final precedence');
    equal(visibleOverlay[0].display, visible.display, 'visible display follows the same exact-name overlay');
    equal(visibleOverlay[0].name, bindings[0].name, 'overlay retains native binding identity text');
    equal(visibleOverlay[1], bindings[1], 'unmatched binding record retains identity');

    equal(names(plan(0, 0, data, context, 3)), 'renderVariableValues', 'partial result preserves editor and rendering');
    const partial = plan(0, 1, data, context, 3);
    equal(
        names(partial), 'installEvaluationTextCards,present,source,derivative,variables', 'partial result skips save');
    equal(service(partial, 'source').args[1], context.text, 'partial result still retains evaluated source');
    for (const mode of [-1, 7, 2147483647]) equal(plan(mode, 0), null, 'invalid mode is inert');
    for (const phase of [-1, 2]) equal(plan(0, phase), null, 'invalid phase is inert');
    for (const outcome of [0, 1, 4]) equal(plan(0, 0, data, context, outcome), null, 'unaccepted outcome is inert');
    for (let mode = 1; mode < 7; ++mode) equal(plan(mode, 0, data, context, 3), null, 'partial is Expression-only');

    for (const values of [undefined, null, false, '', {}, {0: bindings[0], length: 1}, [], bindings]) {
        const response = Object.freeze({binding_values: values});
        equal(
            names(plan(0, 0, response, context, 3)), values ? 'renderVariableValues' : '',
            'partial Expression uses truthiness, not an array check');
        equal(
            plan(1, 0, response).calls.at(-1).service,
            Array.isArray(values) ? 'renderVariableValues' : 'clearVariableValues',
            'Equation accepts an explicit empty array');
        equal(
            plan(3, 0, response).calls.at(-1).service,
            Array.isArray(values) && values.length ? 'setExpressionEditor' : 'clearVariableValues',
            'Matrix requires a non-empty actual array');
    }
    fullReads = expressionReads = 0;
    plan(0, 0);
    plan(3, 0);
    equal(fullReads + expressionReads, 0, 'phase zero cannot read post-editor state');
    fullText = 'new full editor source';
    expressionText = 'new matrix editor source';
    equal(
        service(plan(0, 1, data, {text: context.text}), 'saveWorksheetState').args[1], fullText,
        'expression completion reads current full text');
    equal(fullReads, 1, 'expression completion reads full text once');
    plan(0, 1);
    equal(fullReads, 1, 'authored editor text short-circuits fallback getter');
    equal(service(plan(3, 1), 'modeSource').args[1], expressionText, 'matrix completion reads current editor text');
    equal(expressionReads, 1, 'matrix completion reads editor text once');

    const large =
        Object.freeze(Array.from({length: 1600}, (_, index) => Object.freeze({name: 'p' + index, value: '?'})));
    const largeResponse = Object.freeze({expression: 'native', binding_values: large});
    const selected = service(plan(0, 0, largeResponse), 'setExpressionEditor').args[1];
    const variables = service(plan(0, 1, largeResponse), 'variables').args[0];
    equal(selected.length, 1600, 'large binding overlay releases temporary handles');
    equal(variables.length, 1600, 'large variable selection releases temporary handles');
    plan(6, 0);
    equal(selected[1599], large[1599], 'returned bindings survive subsequent handle-table reuse');
    equal(variables[1599], 'p1599', 'returned variable names are browser values, not handles');
    const sentinel = new Error('installer service fixture');
    const executed = [];
    let thrown;
    try {
        labDOM.services(plan(6, 0), {
            almanacRender: () => {
                executed.push('render');
                throw sentinel;
            },
            almanacAccept: () => executed.push('accept')
        });
    } catch (error) {
        thrown = error;
    }
    equal(thrown, sentinel, 'service exception propagates unchanged');
    equal(executed.join(','), 'render', 'service failure prevents later effects');

    // Real installer adapters, with browser effects observed rather than committed to the active worksheet.
    const saved = {
        call: labDOM.call,
        fetch: window.fetch,
        mode: currentMode(),
        fullText: labEditorState.fullText,
        displayText: labEditorState.displayText,
        editorBody: expr.value,
        lastInput: labEditorState.lastInput,
        lastTex,
        lastDerivativeExpression,
        lastMatrixScalarExpression,
        currentVariables,
        currentDifferentiable,
        almanacLastWorksheetData,
        modeText: Object.fromEntries(modes.map(mode => [mode, modeEditorText[mode]]))
    };
    const originals = new Map();
    const events = [];
    const replace = (name, callback) => {
        if (!originals.has(name))
            originals.set(name, window[name]);
        window[name] = callback;
    };
    const record = name => (...args) => events.push({name, args});
    const calls = name => events.filter(event => event.name === name);
    const deferred = () => {
        let resolve;
        const promise = new Promise(accept => {
            resolve = accept;
        });
        return {promise, resolve};
    };
    let mode = 'expression', liveText = 'old live text';
    const requests = [];
    const pending = [];
    try {
        for (const name
                 of ['setRenderedResult', 'setExpressionEditor', 'renderVariableValues', 'clearVariableValues',
                     'setExpandableText', 'setResultInputText', 'setValueText', 'setValueCardVisible',
                     'renderDerivativeButtons', 'saveWorksheetState', 'displayMatrixResult', 'scheduleRenderedTeXFit',
                     'scheduleSolverTexFit', 'applyIntegratorBindingState', 'applyIntegratorResultBound',
                     'renderDatetimeSections', 'setDatetimeLocalText', 'applyCalendarEvaluationFields',
                     'saveLastDatetimeState', 'saveLastAlmanacState', 'renderAlmanacWorksheet',
                     'refreshAlmanacLandTotality', 'setBusy', 'setActionRunning'])
            replace(name, record(name));
        replace('currentMode', () => mode);
        expr.value = liveText;
        labEditorState.displayText = '';
        labDOM.call = (name, ...args) => {
            if (['lab_evaluation_render', 'lab_evaluation_cards_present', 'lab_evaluation_solver'].includes(name)) {
                events.push({name, args});
                return name === 'lab_evaluation_render' ? 'native TeX' : undefined;
            }
            return saved.call(name, ...args);
        };
        replace('setExpressionEditor', (...args) => {
            record('setExpressionEditor')(...args);
            labEditorState.fullText = 'installed full text';
            liveText = 'installed matrix text';
            expr.value = liveText;
            labEditorState.displayText = '';
        });
        await installEvaluationResult(0, data, {text: 'authored source'}, 2);
        equal(calls('saveWorksheetState')[0].args[1], 'installed full text', 'real wrapper defers full-text getter');
        equal(labEditorState.lastInput, 'authored source', 'source setter updates native owner');
        mode = 'matrix';
        events.length = 0;
        await installEvaluationResult(3, data, context, 2);
        equal(modeEditorText.matrix, 'installed matrix text', 'real wrapper defers matrix getter');
        equal(calls('saveWorksheetState')[0].args.length, 1, 'matrix save retains execution-time default argument');
        check(
            typeof Object.getOwnPropertyDescriptor(modeEditorText, 'matrix').set === 'function',
            'real wrapper preserves the native workspace accessor');

        mode = 'almanac';
        events.length = 0;
        const previousAlmanac = almanacLastWorksheetData;
        replace('renderAlmanacWorksheet', () => {
            throw sentinel;
        });
        thrown = null;
        try {
            await installEvaluationResult(6, data, context, 2);
        } catch (error) {
            thrown = error;
        }
        equal(thrown, sentinel, 'real wrapper propagates presentation failure');
        equal(almanacLastWorksheetData, previousAlmanac, 'failed render cannot replace saved Almanac');
        equal(
            calls('refreshAlmanacLandTotality').length + calls('saveLastAlmanacState').length, 0,
            'failed first phase cannot start background work or persistence');

        mode = 'diffequation';
        labRequests.modeChanged(mode);
        const response = payload =>
            new Response(labWire.encode(payload), {headers: {'Content-Type': 'application/x-protobuf'}});
        for (const scenario of ['failure', 'no SVG', 'SVG', 'stale success', 'stale failure']) {
            const gate = deferred(), entered = deferred();
            const request = labRequests.begin('evaluate', mode);
            requests.push(request);
            check(request, 'solver fixture owns a main request');
            events.length = 0;
            window.fetch = (url, options) => {
                entered.resolve();
                equal(String(url), '/render_TeX', 'solver request uses the native renderer');
                equal(labWire.decode(options.body).tex, 'native solver source', 'opaque solver source is unchanged');
                return gate.promise;
            };
            const payload = {
                ok: true,
                status: 'solved',
                steps: 'plain derivation',
                steps_left_TeX: 'native solver source',
                solutions: 'native roots'
            };
            const task = installEvaluationResult(2, payload, context, 2, request);
            pending.push({task, gate});
            await Promise.race([
                entered.promise, task.then(() => {
                    throw new Error('Evaluation install: solver installer completed without requesting a render');
                })
            ]);
            equal(calls('setExpandableText').length, 2, 'problem and derivation installed before render wait');
            equal(
                calls('setValueText').length + calls('saveWorksheetState').length, 0,
                'completion waits for solver render');
            let newer = null;
            if (scenario.startsWith('stale')) {
                newer = labRequests.begin('evaluate', mode);
                requests.push(newer);
            }
            events.length = 0;
            const failed = scenario.endsWith('failure');
            gate.resolve(response(
                failed                    ? {ok: false, error: 'fixture render failure'} :
                    scenario === 'no SVG' ? {ok: true} :
                                            {ok: true, svg: '<svg/>'}));
            await task;
            if (newer) {
                equal(
                    events.filter(event => !['setBusy', 'setActionRunning'].includes(event.name)).length, 0,
                    'stale solver continuation performs no installer effects');
                check(labRequests.busy(), 'stale completion cannot release newer busy state');
                labRequests.finish(newer);
            } else {
                equal(calls('setValueText')[0]?.args[0], 'native roots', 'current continuation installs solutions');
                equal(calls('saveWorksheetState').length, 1, 'current continuation persists exactly once');
                equal(calls('setValueCardVisible')[0]?.args[0], true, 'current continuation shows solution card');
                equal(
                    calls('lab_evaluation_solver').length, scenario === 'SVG' ? 1 : 0,
                    'only a current non-empty SVG replaces plain derivation');
                equal(
                    calls('scheduleSolverTexFit').length, scenario === 'SVG' ? 1 : 0,
                    'only an installed SVG schedules solver fitting');
            }
            labRequests.finish(request);
        }
    } finally {
        // Release deferred fetches even if an assertion failed before the usual response boundary.
        for (const item of pending) {
            item.gate.resolve(new Response(
                labWire.encode({ok: false, error: 'fixture cleanup'}),
                {headers: {'Content-Type': 'application/x-protobuf'}}));
            try {
                await item.task;
            } catch (_) {
                // Preserve the original test failure while still restoring browser state.
            }
        }
        for (const request of requests)
            if (request)
                labRequests.finish(request);
        labDOM.call = saved.call;
        window.fetch = saved.fetch;
        for (const [name, callback] of originals) window[name] = callback;
        labEditorState.fullText = saved.fullText;
        labEditorState.displayText = saved.displayText;
        expr.value = saved.editorBody;
        labEditorState.lastInput = saved.lastInput;
        for (const mode of modes) modeEditorText[mode] = saved.modeText[mode];
        lastTex = saved.lastTex;
        lastDerivativeExpression = saved.lastDerivativeExpression;
        lastMatrixScalarExpression = saved.lastMatrixScalarExpression;
        currentVariables = saved.currentVariables;
        currentDifferentiable = saved.currentDifferentiable;
        almanacLastWorksheetData = saved.almanacLastWorksheetData;
        labRequests.modeChanged(saved.mode);
    }
};
