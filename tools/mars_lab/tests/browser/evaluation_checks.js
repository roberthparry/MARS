/** Evaluation lifecycle regressions using real C/WASM policy and request ownership. */
window.checkLabEvaluation = async function checkLabEvaluation() {
    const native = labWire.exports();
    const check = (condition, message) => {
        if (!condition)
            throw new Error('Evaluation: ' + message);
    };
    const equal = (actual, expected, message) => check(Object.is(actual, expected), message);
    const modes = ['expression', 'equation', 'diffequation', 'matrix', 'integrator', 'datetime', 'almanac'];
    for (const kind of [-1, 6, 0x7fffffff])
        equal(labDOM.call('lab_flow_begin', kind, {mode: 0}), null, 'invalid flow kind has no request plan');
    for (const mode of [-1, -0.5, 0.5, 7, 0x7fffffff, Infinity]) {
        const frame = {mode, stage: 0, context: {text: 'untouched'}};
        const context = frame.context;
        equal(labDOM.call('lab_flow_begin', 0, frame), null, 'invalid numeric mode has no request plan');
        equal(labDOM.call('lab_flow_step', 0, frame, {}).done, true, 'invalid mode terminates progression');
        equal(frame.stage, 0, 'invalid mode does not advance its frame');
        equal(frame.context, context, 'invalid mode does not replace context');
    }
    for (let mode = 0; mode < modes.length; ++mode) {
        const plan = labDOM.call('lab_flow_begin', 0, {mode, parent: 42});
        equal(plan.operation, 'evaluate', 'evaluation request operation');
        equal(plan.mode, modes[mode], 'numeric frame mode selects request mode');
        equal(plan.options.parent, 42, 'request parent retained');
        equal(plan.ui, true, 'evaluation owns visible request state');
    }
    for (const frame of [{}, {mode: undefined}, {mode: null}])
        equal(labDOM.call('lab_flow_begin', 0, frame).mode, 'expression', 'absent mode defaults numerically to zero');
    for (const stage of [-1, -0.5, 0.5, 65536, Infinity]) {
        const frame = {mode: 0, stage, context: {text: 'untouched'}};
        const context = frame.context;
        equal(labDOM.call('lab_flow_step', 0, frame, {}).done, true, 'invalid stage terminates progression');
        equal(frame.stage, stage, 'invalid stage is not overwritten');
        equal(frame.context, context, 'invalid stage preserves context');
    }

    const beforeFlowMarkup = rendered.innerHTML;
    const originalContext = {text: 'original context'};
    const firstFrame = {mode: 0, stage: 0, context: originalContext, options: {skipHistoryUpdate: false}};
    const secondFrame = {mode: 3, stage: 0, context: originalContext, options: {skipHistoryUpdate: true}};
    const firstView = {text: 'first view'};
    const secondView = {text: 'second view'};
    const firstPreparation = labDOM.call('lab_flow_step', 0, firstFrame, firstView);
    equal(firstFrame.stage, 1, 'first frame advances to preparation');
    equal(firstFrame.phase, 0, 'first frame initialises setup phase');
    equal(secondFrame.stage, 0, 'first step leaves second frame stage alone');
    equal(secondFrame.context, originalContext, 'first step leaves second frame context alone');
    check(firstFrame.context !== originalContext, 'start allocates a fresh context');
    const firstContext = firstFrame.context;
    const secondPreparation = labDOM.call('lab_flow_step', 0, secondFrame, secondView);
    check(firstContext !== secondFrame.context, 'independent frames never share their new contexts');
    firstContext.text = 'first authored μ';
    equal(secondFrame.context.text, undefined, 'mutating one context leaves the other unchanged');
    equal(originalContext.text, 'original context', 'shared input context is not mutated');
    equal(firstPreparation.calls[0].service, 'native', 'retained preparation service survives scope reuse');
    equal(firstPreparation.calls[0].args.join(','), 'lab_evaluation_prepare_plan,0,0', 'first mode and skip retained');
    equal(
        secondPreparation.calls[0].args.join(','), 'lab_evaluation_prepare_plan,3,1', 'second mode and skip retained');
    equal(firstPreparation.save, 'preparation', 'preparation return destination retained');
    equal(firstPreparation.wait, false, 'preparation is synchronous');
    firstFrame[firstPreparation.save] = labDOM.call(...firstPreparation.calls[0].args);
    const firstSetup = labDOM.call('lab_flow_step', 0, firstFrame, firstView);
    equal(firstFrame.stage, 3, 'prepared first frame progresses to setup application');
    equal(secondFrame.stage, 1, 'first setup leaves second frame awaiting preparation');
    equal(secondFrame.phase, 0, 'first setup leaves second setup phase unchanged');
    secondFrame[secondPreparation.save] = labDOM.call(...secondPreparation.calls[0].args);
    const secondSetup = labDOM.call('lab_flow_step', 0, secondFrame, secondView);
    equal(firstFrame.stage, 3, 'second setup does not advance first frame');
    equal(firstFrame.context, firstContext, 'second setup does not replace first context');
    secondFrame.context.text = 'second authored σ';
    equal(firstContext.text, 'first authored μ', 'second context mutation remains independent');
    for (const [plan, frame, view] of [[firstSetup, firstFrame, firstView], [secondSetup, secondFrame, secondView]]) {
        equal(plan.save, 'plan', 'setup return destination survives other native calls');
        equal(plan.wait, false, 'setup planner call is synchronous');
        equal(plan.calls[0].service, 'native', 'setup service survives native scope reuse');
        const args = plan.calls[0].args;
        equal(args[0], 'lab_evaluation_setup', 'setup selects native policy');
        equal(args[1], frame.mode, 'setup retains numeric mode, not a mode-name string');
        equal(args[2], 0, 'setup begins at phase zero');
        equal(args[3], frame.context, 'returned plan retains its own ordinary browser context');
        equal(args[4], frame.options, 'returned plan retains its own options');
        equal(args[5], view, 'returned plan retains its own view');
    }
    equal(firstPreparation.calls[0].args[1], 0, 'earlier plan remains valid after both frames progress');
    equal(rendered.innerHTML, beforeFlowMarkup, 'direct continuation planning has no DOM effects');
    const working = [
        'Evaluating...', 'Solving equation...', 'Solving differential equation...', 'Evaluating matrix...',
        'Integrating...', 'Calculating dates...', 'Working the almanac...'
    ];
    const failures = [
        'Evaluation failed', 'Equation solving failed', 'Differential-equation solving failed',
        'Matrix evaluation failed', 'Integration failed', 'Datetime calculation failed', 'Almanac calculation failed'
    ];
    const preparations = [7, 13, 12, 5, 5, 16, 32];
    const successes = [1, 13, 141, 13, 13, 269, 13];
    const rejected = [6, 6, 6, 38, 70, 22, 6];
    const preparationNames = ['bindings', 'expression', 'input', 'trim', 'datetime', 'almanac', 'history'];
    const planResponse = (mode, outcome, data = {}, error = null) =>
        labDOM.call('lab_evaluation_response', mode, outcome, data, {text: 'authored μ'}, error);
    const recoveryNames =
        flags => [flags & 32 ? 'clearMatrixScalar' : '', flags & 2 ? 'setRenderedError,resetRenderedDigits' : '',
                  flags & 16 ? 'setDatetimeLocalText' : '', flags & 4 ? 'clearResultDetails' : '',
                  flags & 8 ? 'clearRenderedError' : '',
                  flags & 64 ? 'applyIntegratorBindingState,applyIntegratorResultBound,saveWorksheetState' : '']
                     .filter(Boolean)
                     .join(',');
    for (let mode = 0; mode < modes.length; ++mode) {
        for (const skip of [0, 1]) {
            const plan = labDOM.call('lab_evaluation_prepare_plan', mode, skip);
            const bits = preparations[mode] | (skip ? 0 : 64);
            preparationNames.forEach(
                (name, bit) => equal(plan[name], !!(bits & (1 << bit)), 'named preparation ' + name));
            equal(plan.label, working[mode], 'preparation carries the native status');
        }
        for (const outcome of [0, 1, 2, 3, 4]) {
            const data = {error: ' specific μ ', raw_error: ' raw ', status: 'series'};
            const flags = native.lab_evaluation_actions(mode, outcome);
            const before = rendered.innerHTML;
            const plan = planResponse(mode, outcome, data);
            equal(rendered.innerHTML, before, 'plan creation has no DOM side effects');
            if (!flags) {
                equal(plan, null, 'stale or invalid response has no plan');
                continue;
            }
            equal(plan.install, !!(flags & 1), 'installation flag');
            equal(plan.awaitInstall, !!(flags & 128), 'awaited installation flag');
            equal(plan.weather, !!(flags & 256), 'weather follow-up flag');
            equal(plan.calls.map(call => call.service).join(','), recoveryNames(flags), 'recovery order stays native');
            equal(
                plan.status, evaluationLabel(mode, native.lab_evaluation_status(mode, outcome, 1, 0)),
                'status in plan');
            const clear = plan.calls.find(call => call.service === 'clearResultDetails');
            if (clear)
                equal(clear.args[0].keepBindings, true, 'recovery preserves authored bindings');
            const diagnostic = plan.calls.find(call => call.service === 'setRenderedError');
            if (diagnostic)
                equal(diagnostic.args[0], mode === 4 ? 'specific μ' : data.error, 'diagnostic trimming policy');
            if (flags & 64) {
                const restore = plan.calls.find(call => call.service === 'applyIntegratorBindingState');
                equal(restore.args[0], data, 'recovery retains the actual native response');
                equal(restore.args[1], 'authored μ', 'recovery retains authored source');
            }
        }
        equal(native.lab_evaluation_prepare(mode, 0), preparations[mode] | 64, 'preparation and history flags');
        equal(native.lab_evaluation_prepare(mode, 1), preparations[mode], 'skip history does not skip preparation');
        equal(native.lab_evaluation_actions(mode, 0), 0, 'stale result has no actions');
        equal(native.lab_evaluation_actions(mode, 1), rejected[mode], 'native failure recovery');
        equal(native.lab_evaluation_actions(mode, 2), successes[mode], 'successful installation policy');
        equal(native.lab_evaluation_actions(mode, 3), mode === 0 ? 3 : 0, 'partial expression policy');
        equal(native.lab_evaluation_actions(mode, 4), mode === 5 ? 22 : 6, 'exception recovery policy');
        equal(evaluationLabel(mode, 0), working[mode], 'working label');
        equal(evaluationLabel(mode, 1), failures[mode], 'fallback error label');
        for (const outcome of [1, 3, 4]) {
            const expected = outcome === 3 && mode !== 0 ? 6 : 3;
            equal(native.lab_evaluation_status(mode, outcome, 1, 1), expected, 'failure overrides solver status');
        }
        equal(native.lab_evaluation_status(mode, 2, 0, 1), 2, 'successful status');
        equal(native.lab_evaluation_status(mode, 2, 1, 1), mode === 2 ? 4 : 2, 'local series status');
        equal(native.lab_evaluation_status(mode, 2, 0, 0), mode === 2 ? 5 : 2, 'unsolved status');
        for (const hasError of [0, 1]) {
            for (const generic of [0, 1]) {
                for (const hasRaw of [0, 1]) {
                    const expected = mode !== 4 ? hasError : hasError && !generic ? 2 : hasRaw ? 3 : hasError ? 2 : 0;
                    equal(
                        native.lab_evaluation_error_source(mode, hasError, generic, hasRaw), expected,
                        'integration raw diagnostic precedence');
                }
            }
        }
    }
    for (const mode of [-1, 7, 0x7fffffff]) {
        equal(labDOM.call('lab_evaluation_prepare_plan', mode, 0), null, 'invalid mode has no preparation plan');
        equal(planResponse(mode, 2), null, 'invalid mode has no response plan');
        equal(native.lab_evaluation_prepare(mode, 0), 0, 'invalid mode preparation');
        equal(native.lab_evaluation_actions(mode, 2), 0, 'invalid mode actions');
        equal(native.lab_evaluation_status(mode, 2, 0, 0), 6, 'invalid mode status');
        equal(evaluationLabel(mode, 0), '', 'invalid mode label');
        equal(native.lab_evaluation_error_source(mode, 1, 1, 1), 0, 'invalid mode diagnostic');
    }
    for (const field of [-1, 6, 0x7fffffff]) equal(evaluationLabel(0, field), '', 'invalid label field');
    for (const outcome of [-1, 0, 5]) {
        equal(planResponse(0, outcome), null, 'invalid outcome has no response plan');
        equal(native.lab_evaluation_actions(0, outcome), 0, 'invalid/stale outcome actions');
        equal(native.lab_evaluation_status(0, outcome, 1, 1), 6, 'invalid/stale outcome status');
    }

    const diagnostic = (mode, data, error = null) =>
        planResponse(mode, 1, data, error).calls.find(call => call.service === 'setRenderedError').args[0];
    equal(
        diagnostic(4, {error: ' Integration failed ', raw_error: ' raw μ '}), 'raw μ',
        'generic error uses raw diagnostic');
    equal(
        diagnostic(4, {error: ' ', raw_error: ' '}), 'Integration failed',
        'blank integration diagnostics use fallback');
    equal(diagnostic(0, {error: ''}), 'Evaluation failed', 'empty expression diagnostic uses fallback');
    equal(diagnostic(0, {error: 'ignored'}, ''), '', 'explicit empty exception overrides native diagnostic');
    equal(diagnostic(0, {error: 'ignored'}, 'exception'), 'exception', 'explicit exception takes precedence');
    const ordered = [];
    const registry = Object.freeze({
        first: value => {
            ordered.push(value);
            equal(evaluationLabel(0, 0), working[0], 'services re-enter WASM after scope release');
        },
        second: () => ordered.push('second')
    });
    labDOM.services({calls: [{service: 'first', args: ['first']}, {service: 'second', args: []}]}, registry);
    equal(ordered.join(','), 'first,second', 'generic service bridge preserves ordering');
    let unknownRejected = false;
    try {
        labDOM.services({calls: [{service: 'toString', args: []}]}, registry);
    } catch (_) {
        unknownRejected = true;
    }
    check(unknownRejected, 'inherited or unknown services cannot be dispatched');

    const savedMode = currentMode();
    const savedFunctions = new Map();
    const savedInstall = installEvaluationResult;
    const savedInstallers = modes.map(
        (_, index) => (data, context, outcome, request) => savedInstall(index, data, context, outcome, request));
    const installers = [...savedInstallers];
    const savedScalar = lastMatrixScalarExpression;
    const savedLastInput = labEditorState.lastInput;
    const savedFullText = labEditorState.fullText, savedDisplayText = labEditorState.displayText;
    const savedDerivative = lastDerivativeExpression;
    const savedLastTex = lastTex;
    const savedVariables = currentVariables;
    const savedDifferentiable = currentDifferentiable;
    const savedInput = expr.value;
    const savedFetch = window.fetch;
    const savedDOMCall = labDOM.call;
    const nodes = [rendered, renderedTitle, renderedMore, functionStyle, valueNote, valueNoteCard, valueTitle];
    const savedNodes =
        nodes.map(node => ({
                      children: Array.from(node.childNodes),
                      attributes: Array.from(node.attributes, attribute => [attribute.name, attribute.value])
                  }));
    const events = [];
    let mode = 'expression', text = 'authored input', reply = {ok: false}, transportError = null;
    const setEditorSource = source => {
        expr.value = source ? 'evaluation compact fixture' : '';
        labEditorState.displayText = expr.value;
        labEditorState.fullText = source;
    };
    const replace = (name, callback) => {
        if (!savedFunctions.has(name)) {
            check(typeof window[name] === 'function', 'writable browser service ' + name);
            savedFunctions.set(name, window[name]);
        }
        window[name] = callback;
    };
    const record = name => (...args) => events.push([name, ...args]);
    const calls = name => events.filter(event => event[0] === name);
    const deferred = () => {
        let resolve, reject;
        const promise = new Promise((accept, fail) => {
            resolve = accept;
            reject = fail;
        });
        return {promise, resolve, reject};
    };
    const fetchResult = async () => {
        events.push(['fetch', mode]);
        if (transportError)
            throw transportError;
        return {response: {ok: true}, data: reply};
    };
    const commitBindings = async () => events.push(['bindings']);
    const assertNoCompletion = () => {
        for (const name of ['install', 'setRenderedError', 'commitModeState', 'updateHistoryButtons', 'setStatus'])
            equal(calls(name).length, 0, 'stale request cannot perform ' + name);
    };
    try {
        // Observe presentation at the DOM boundary; its real projection has a dedicated card suite.
        const layoutServices = {
            lab_layout_error: (message, enabled) =>
                events.push([enabled ? 'setRenderedError' : 'clearRenderedError', message]),
            lab_layout_more: record('resetMoreDigitsButton'),
            lab_layout_content: record('setRenderedContent'),
            lab_layout_solver_install: record('installSolverTexSvg'),
            lab_evaluation_cards_present: record('presentEvaluationCards')
        };
        labDOM.call = (name, ...args) =>
            Object.hasOwn(layoutServices, name) ? layoutServices[name](...args) : savedDOMCall(name, ...args);
        replace('currentMode', () => mode);
        setEditorSource(text);
        replace('visibleBindingValues', () => ({x: '1/3'}));
        replace('commitVisibleBindingInputs', commitBindings);
        replace('expressionWithVisibleBindings', async source => source);
        replace('historyStateForMode', (selected, source) => ({mode: selected, text: source}));
        replace('previousModeStateForHistory', next => {
            events.push(['previous', next]);
            return {mode: next.mode, text: 'previous input'};
        });
        replace('currentDatetimeState', () => ({date: '2000-01-01', latitude: '1', longitude: '2'}));
        replace('datetimeSummaryText', () => 'date snapshot');
        replace('currentAlmanacState', () => ({}));
        replace('almanacSummaryText', () => 'almanac snapshot');
        for (const name
                 of ['setBusy', 'setActionRunning', 'showResults', 'setStatus', 'pushExpressionHistory',
                     'updateHistoryButtons', 'commitModeState', 'saveWorksheetState', 'clearResultDetails',
                     'setDatetimeLocalText', 'applyIntegratorBindingState', 'applyIntegratorResultBound',
                     'refreshDatetimeWeather', 'scheduleRenderedTeXFit', 'setExpandableText', 'setResultInputText',
                     'scheduleSolverTexFit'])
            replace(name, record(name));
        for (const suffix
                 of ['Expression', 'Equation', 'Diffequation', 'Matrix', 'Integrator', 'Datetime', 'Almanac']) {
            const name = suffix === 'Expression' ? 'fetchEvaluation' : 'fetch' + suffix + 'Evaluation';
            replace(name, fetchResult);
        }
        for (let index = 0; index < modes.length; ++index)
            installers[index] = data => events.push(['install', modes[index], data]);
        replace(
            'installEvaluationResult',
            (mode, data, context, outcome, request) => installers[mode](data, context, outcome, request));

        // Each mode reaches the same native-directed adapter through its public entry point.
        const evaluate = [
            evaluateExpression, evaluateEquation, evaluateDiffequation, evaluateMatrix, evaluateIntegrator,
            evaluateDatetime, evaluateAlmanac
        ];
        for (let index = 0; index < modes.length; ++index) {
            mode = modes[index];
            for (const ok of [false, true]) {
                events.length = 0;
                reply = {ok, status: 'solved'};
                lastMatrixScalarExpression = 'previous scalar';
                await evaluate[index]();
                equal(calls('bindings').length, [0, 1, 3, 4].includes(index) ? 1 : 0, 'binding commit policy');
                equal(calls('fetch').length, 1, 'one evaluation fetch');
                equal(calls('install').length, ok ? 1 : 0, 'install only accepted results');
                equal(calls('commitModeState').length, 1, 'one result commit');
                equal(calls('pushExpressionHistory').length, 1, 'previous state pushed once');
                equal(calls('updateHistoryButtons').length, 1, 'history finalised once');
                equal(calls('setStatus')[0][1], working[index], 'mode working status');
                equal(calls('setStatus').at(-1)[1], ok ? 'Ready' : 'Error', 'mode completion status');
                equal(calls('refreshDatetimeWeather').length, ok && index === 5 ? 1 : 0, 'weather after success only');
                if (ok && index === 5) {
                    equal(calls('refreshDatetimeWeather')[0][1], labRequests.latestMain(), 'weather uses parent token');
                    equal(calls('refreshDatetimeWeather')[0][2].date, '2000-01-01', 'weather retains captured date');
                }
                equal(calls('applyIntegratorBindingState').length, !ok && index === 4 ? 1 : 0, 'integration bindings');
                equal(
                    calls('setDatetimeLocalText').length, !ok && index === 5 ? 1 : 0, 'failed date clears local card');
                equal(lastMatrixScalarExpression, !ok && index === 3 ? '' : 'previous scalar', 'matrix failure cache');
                if (!ok)
                    equal(calls('setRenderedError')[0][1], failures[index], 'mode fallback diagnostic');
                check(!labRequests.busy(), 'completed evaluation releases native busy state');
            }
            events.length = 0;
            await evaluate[index]({skipHistoryUpdate: true});
            equal(
                calls('previous').length + calls('pushExpressionHistory').length + calls('updateHistoryButtons').length,
                0, 'skip history suppresses lookup, push and finalisation');
            events.length = 0;
            transportError = new Error('network fixture');
            await evaluate[index]();
            transportError = null;
            equal(calls('setRenderedError')[0][1], 'Error: network fixture', 'owned exception is displayed exactly');
            equal(calls('commitModeState').length, 1, 'owned exception commits cleared results');
            equal(calls('updateHistoryButtons').length, 1, 'owned exception finalises history');
            equal(
                calls('applyIntegratorBindingState').length, 0, 'transport failure has no integration binding payload');
            check(!labRequests.busy(), 'exception releases native busy state');
        }

        // A successful HTTP response without native markup must not publish a blank, Ready worksheet.
        mode = 'almanac';
        const previousAlmanac = almanacLastWorksheetData;
        installers[6] = savedInstallers[6];
        for (const invalid
                 of [{ok: true, visibility: 'all'},
                     {ok: true, visibility: 'visible', almanac_presentation: {visible: {html: ''}}}]) {
            events.length = 0;
            reply = invalid;
            await evaluateAlmanac();
            check(
                calls('setRenderedError')[0]?.[1].includes('Almanac presentation'),
                'missing native presentation must show its diagnostic');
            equal(calls('setStatus').at(-1)[1], 'Error', 'missing native presentation cannot report Ready');
            equal(almanacLastWorksheetData, previousAlmanac, 'invalid response cannot replace saved worksheet');
            check(!labRequests.busy(), 'invalid presentation releases native busy state');
        }
        installers[6] = data => events.push(['install', mode, data]);

        text = '';
        setEditorSource(text);
        expr.value = '';
        for (let index = 0; index < modes.length; ++index) {
            mode = modes[index];
            events.length = 0;
            reply = {ok: false};
            await evaluate[index]();
            equal(calls('fetch').length, index >= 5 ? 1 : 0, 'calendar snapshots do not require editor text');
            equal(calls('pushExpressionHistory').length, index >= 5 ? 1 : 0, 'empty mathematical input keeps history');
            check(!labRequests.busy(), 'empty input releases native busy state');
        }
        expr.value = savedInput;
        text = '  authored input  ';
        setEditorSource(text);
        for (const index of [0, 1, 2, 3, 4]) {
            mode = modes[index];
            events.length = 0;
            await evaluate[index]();
            equal(
                calls('previous')[0][1].text, index === 1 || index === 2 ? text.trim() : text,
                'history preserves mode-specific text preparation');
        }
        text = 'authored input';
        setEditorSource(text);
        mode = 'expression';
        labEditorState.lastInput = 'previous exact native input';
        events.length = 0;
        await evaluateExpression({reuseLastInput: true});
        equal(calls('previous')[0][1].text, 'previous exact native input', 'precision evaluation reuses last input');
        equal(calls('saveWorksheetState')[0][2], text, 'reuse still saves authored editor text');

        mode = 'diffequation';
        for (const [status, expected] of [['series', 'Local series'], ['reduced', 'Not solved']]) {
            events.length = 0;
            reply = {ok: true, status};
            await evaluateDiffequation();
            equal(calls('setStatus').at(-1)[1], expected, 'solver completion label comes from C');
        }

        mode = 'integrator';
        for (const [error, raw_error, expected] of [
                 [' Integration failed ', ' precise native reason ', 'precise native reason'],
                 [' specific reason ', ' raw reason ', 'specific reason'], ['', ' raw reason ', 'raw reason'],
                 [' ', ' ', 'Integration failed']]) {
            events.length = 0;
            reply = {ok: false, error, raw_error};
            await evaluateIntegrator();
            equal(calls('setRenderedError')[0][1], expected, 'integration diagnostic fallback');
            equal(calls('applyIntegratorResultBound').length, 1, 'native failure restores integration bounds');
        }

        mode = 'matrix';
        events.length = 0;
        replace(
            'fetchMatrixEvaluation',
            async () => ({response: {ok: false}, data: {ok: true, error: 'HTTP fixture failure'}}));
        await evaluateMatrix();
        equal(calls('install').length, 0, 'HTTP failure cannot install a successful-looking payload');
        equal(calls('setRenderedError')[0][1], 'HTTP fixture failure', 'HTTP failure retains native diagnostic');
        replace('fetchMatrixEvaluation', fetchResult);
        events.length = 0;
        reply = {ok: true};
        const matrixInstaller = installers[3];
        installers[3] = async () => {
            throw new Error('installer fixture failure');
        };
        await evaluateMatrix();
        installers[3] = matrixInstaller;
        equal(calls('setRenderedError')[0][1], 'Error: installer fixture failure', 'installer exception reaches catch');
        equal(calls('commitModeState').length, 1, 'installer exception commits failure once');
        equal(calls('updateHistoryButtons').length, 1, 'installer exception finalises history once');
        check(!labRequests.busy(), 'installer exception releases native busy state');

        mode = 'expression';
        events.length = 0;
        replace('commitVisibleBindingInputs', async () => {
            throw new Error('binding setup failed');
        });
        await evaluateExpression();
        equal(calls('setRenderedError')[0][1], 'Error: binding setup failed', 'setup failure reaches outer UI guard');
        equal(calls('commitModeState').length + calls('updateHistoryButtons').length, 0, 'setup failure keeps history');
        replace('commitVisibleBindingInputs', commitBindings);

        // A real expression installer must retain native card strings on partial errors.
        for (const name
                 of ['setRenderedResult', 'renderVariableValues', 'setExpressionEditor', 'setValueText',
                     'setValueCardVisible', 'renderDerivativeButtons'])
            replace(name, record(name));
        replace('variableNamesFromBindings', () => ['x']);
        installers[0] = savedInstallers[0];
        events.length = 0;
        reply = {
            ok: true,
            partial_error: true,
            error: 'native partial diagnostic',
            expression: 'native full expression',
            display_expression: '{ native symbolic result | x = 1/3 }',
            full_display_expression: '{ native full symbolic result | x = 1/3 }',
            display_function: 'native function; const c = ?;',
            full_display_function: 'native full function; const c = ?;',
            editor_expression: 'native editor source',
            binding_values: [{name: 'x', value: '1/3'}],
            value: '1'
        };
        await evaluateExpression();
        equal(calls('setRenderedError')[0][1], reply.error, 'partial diagnostic retained');
        equal(
            calls('setRenderedResult').length + calls('setExpressionEditor').length, 0,
            'partial result keeps rendered error and editor');
        equal(calls('clearResultDetails').length, 0, 'partial result does not clear available cards');
        equal(calls('setExpandableText')[0][3], reply.display_expression, 'expression presentation passed through');
        equal(calls('setExpandableText')[1][4], reply.full_display_function, 'function presentation passed through');
        equal(calls('setValueText')[0][1], '1', 'binding-independent numerical value retained');
        equal(calls('presentEvaluationCards').length, 1, 'partial expression projects card policy once');
        equal(calls('presentEvaluationCards')[0][1], 0, 'expression card presentation mode');
        equal(calls('presentEvaluationCards')[0][2], reply, 'card presentation retains the native payload');
        equal(calls('saveWorksheetState').length, 1, 'partial result retains preparation save only');
        equal(calls('setStatus').at(-1)[1], 'Error', 'partial cards retain error status');

        events.length = 0;
        reply = {...reply, partial_error: false, error: ''};
        await evaluateExpression();
        equal(calls('setRenderedResult')[0][1], reply, 'successful rendering receives the unchanged native response');
        equal(calls('setExpressionEditor')[0][1], text, 'successful evaluation retains the authored editor');
        equal(
            calls('setExpressionEditor')[0][2].length, reply.binding_values.length,
            'native binding selection retains every discovered parameter');
        equal(
            calls('setExpressionEditor')[0][2][0], reply.binding_values[0],
            'unoverridden native binding record retains its exact identity');
        equal(
            calls('setExpandableText')[0][3], reply.display_expression, 'success preserves native symbolic expression');
        equal(calls('setExpandableText')[1][4], reply.full_display_function, 'native function declarations retained');
        equal(calls('saveWorksheetState').length, 2, 'successful expression saves preparation and installed state');
        equal(calls('setStatus').at(-1)[1], 'Ready', 'successful expression reaches ready status');
        installers[0] = data => events.push(['install', mode, data]);

        // Supersession at either awaited preparation boundary must prevent fetch and history writes.
        for (const boundary of ['commitVisibleBindingInputs', 'expressionWithVisibleBindings']) {
            const gate = deferred(), entered = deferred();
            replace(boundary, () => {
                entered.resolve();
                return gate.promise;
            });
            const pending = evaluateExpression();
            await entered.promise;
            const newer = labRequests.begin('evaluate', mode);
            events.length = 0;
            gate.resolve(text);
            await pending;
            assertNoCompletion();
            equal(
                calls('fetch').length + calls('pushExpressionHistory').length, 0,
                'stale preparation performs no writes');
            check(labRequests.busy(), 'stale preparation finaliser retains newer busy state');
            labRequests.finish(newer);
            replace(boundary, boundary === 'commitVisibleBindingInputs' ? commitBindings : async source => source);
        }

        // A late success or rejection, including switching away and back, must be invisible.
        for (const scenario of ['success', 'rejection', 'mode change']) {
            const gate = deferred(), entered = deferred();
            replace('fetchEvaluation', () => {
                entered.resolve();
                return gate.promise;
            });
            const pending = evaluateExpression();
            await entered.promise;
            if (scenario === 'mode change') {
                mode = 'matrix';
                labRequests.modeChanged(mode);
                mode = 'expression';
                labRequests.modeChanged(mode);
            }
            const newer = labRequests.begin('evaluate', mode);
            events.length = 0;
            if (scenario === 'rejection')
                gate.reject(new Error('obsolete failure'));
            else
                gate.resolve({response: {ok: true}, data: {ok: true}});
            await pending;
            assertNoCompletion();
            check(labRequests.busy(), 'stale fetch finaliser retains newer busy state');
            labRequests.finish(newer);
        }

        // Exercise the real differential-equation installer and binary solver-render transport.
        mode = 'diffequation';
        installers[2] = savedInstallers[2];
        reply = {ok: true, status: 'solved', steps: 'native derivation', steps_left_TeX: 'x = 1'};
        const solver = deferred(), solverEntered = deferred();
        window.fetch = (url, options) => {
            equal(String(url), '/render_TeX', 'solver render uses native endpoint');
            equal(labWire.decode(options.body).tex, 'x = 1', 'solver mathematics passed through unchanged');
            solverEntered.resolve();
            return solver.promise;
        };
        const solving = evaluateDiffequation();
        await solverEntered.promise;
        const newer = labRequests.begin('evaluate', mode);
        events.length = 0;
        solver.resolve(new Response(
            labWire.encode({ok: true, svg: '<svg/>'}), {headers: {'Content-Type': 'application/x-protobuf'}}));
        await solving;
        assertNoCompletion();
        equal(calls('installSolverTexSvg').length, 0, 'stale solver render cannot install an SVG');
        check(labRequests.busy(), 'stale asynchronous installer retains newer busy state');
        labRequests.finish(newer);
    } finally {
        labRequests.cancel('evaluate');
        window.fetch = savedFetch;
        labDOM.call = savedDOMCall;
        for (const [name, callback] of savedFunctions) window[name] = callback;
        lastMatrixScalarExpression = savedScalar;
        labEditorState.lastInput = savedLastInput;
        labEditorState.fullText = savedFullText;
        labEditorState.displayText = savedDisplayText;
        lastDerivativeExpression = savedDerivative;
        lastTex = savedLastTex;
        currentVariables = savedVariables;
        currentDifferentiable = savedDifferentiable;
        expr.value = savedInput;
        nodes.forEach((node, index) => {
            node.replaceChildren(...savedNodes[index].children);
            for (const attribute of Array.from(node.attributes)) node.removeAttribute(attribute.name);
            for (const [name, value] of savedNodes[index].attributes) node.setAttribute(name, value);
        });
        labRequests.modeChanged(savedMode);
    }
};
