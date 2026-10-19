/**
 * @file evaluation_checks.ts
 * @brief Evaluation lifecycle and request-ownership regressions.
 *
 * Checks native flow validation and browser completion paths across worksheet modes, including supersession and installer failures.
 */

/** Evaluation lifecycle regressions using real C/WASM policy and request ownership. */
window.checkLabEvaluation = async function checkLabEvaluation() {
    const native = labWire.exports();
    function check(condition: unknown, message: string): void {
        if (!condition)
            throw new Error('Evaluation: ' + message);
    }
    function equal(actual: unknown, expected: unknown, message: string): void {
        check(Object.is(actual, expected), message);
    }
    const modes = ['expression', 'equation', 'diffequation', 'matrix', 'integrator', 'datetime', 'almanac'];
    for (let mode = 0; mode < modes.length; ++mode) {
        const request = {token: 17};
        const context = {text: 'captured expression'};
        const frame = {mode, stage: 12, context, request};
        const plan = labDOM.call<EvaluationFixturePlan>('lab_flow_step', 0, frame, {});
        equal(plan.calls[0].service, 'fetch', 'all evaluation modes use the common fetch service');
        const fetchContext = plan.calls[0].args[0] as WorksheetRequests.MarsApiFetchContext;
        equal(fetchContext.kind, modes[mode], 'native fetch context identifies its mode');
        if (fetchContext.kind === 'expression')
            equal(fetchContext.text, context.text, 'expression fetch retains captured text');
        else
            equal(Object.hasOwn(fetchContext, 'text'), false, 'other modes do not copy expression text');
        equal(plan.calls[0].args[1], request, 'fetch preserves request identity');
        equal(plan.wait, true, 'fetch awaits transport');
        equal(plan.save, 'response', 'fetch saves the decoded response');
        equal(frame.context, context, 'fetch does not replace the evaluation context');
    }
    const holidayRequest = {token: 18};
    const holidayPlan = labDOM.call<EvaluationFixturePlan>('lab_flow_step', 10, {stage: 3, request: holidayRequest}, {});
    equal(holidayPlan.calls[0].service, 'fetch', 'holiday refresh uses the common fetch service');
    equal((holidayPlan.calls[0].args[0] as WorksheetRequests.MarsApiFetchContext).kind, 'datetime', 'holiday refresh selects calendar input');
    equal(holidayPlan.calls[0].args[1], holidayRequest, 'holiday refresh preserves request identity');
    for (const kind of [-1, 6, 0x7fffffff])
        equal(labDOM.call<EvaluationFixturePlan>('lab_flow_begin', kind, {mode: 0}), null, 'invalid flow kind has no request plan');
    for (const mode of [-1, -0.5, 0.5, 7, 0x7fffffff, Infinity]) {
        const frame = {mode, stage: 0, context: {text: 'untouched'}};
        const context = frame.context;
        equal(labDOM.call<EvaluationFixturePlan>('lab_flow_begin', 0, frame), null, 'invalid numeric mode has no request plan');
        equal(labDOM.call<EvaluationFixturePlan>('lab_flow_step', 0, frame, {}).done, true, 'invalid mode terminates progression');
        equal(frame.stage, 0, 'invalid mode does not advance its frame');
        equal(frame.context, context, 'invalid mode does not replace context');
    }
    for (let mode = 0; mode < modes.length; ++mode) {
        const plan = labDOM.call<EvaluationFixturePlan>('lab_flow_begin', 0, {mode, parent: 42});
        equal(plan.operation, 'evaluate', 'evaluation request operation');
        equal(plan.mode, modes[mode], 'numeric frame mode selects request mode');
        equal(plan.options.parent, 42, 'request parent retained');
        equal(plan.ui, true, 'evaluation owns visible request state');
    }
    for (const frame of [{}, {mode: undefined}, {mode: null}])
        equal(labDOM.call<EvaluationFixturePlan>('lab_flow_begin', 0, frame).mode, 'expression', 'absent mode defaults numerically to zero');
    for (const stage of [-1, -0.5, 0.5, 65536, Infinity]) {
        const frame = {mode: 0, stage, context: {text: 'untouched'}};
        const context = frame.context;
        equal(labDOM.call<EvaluationFixturePlan>('lab_flow_step', 0, frame, {}).done, true, 'invalid stage terminates progression');
        equal(frame.stage, stage, 'invalid stage is not overwritten');
        equal(frame.context, context, 'invalid stage preserves context');
    }

    // Every indexed workflow must reject absent entries without an indirect-call trap.
    for (const kind of [-1, 36, 37, 38, 39, 63, 68, 69, 70, 71, 76, 0x7fffffff]) {
        const frame = {mode: 0, stage: 0};
        const plan = labDOM.call<EvaluationFixturePlan>('lab_flow_step', kind, frame, {});
        equal(plan.done, true, 'reserved or invalid workflow selector finishes safely');
        equal(frame.stage, 0, 'reserved selector cannot enter another workflow');
        equal((plan.calls || []).length, 0, 'reserved selector schedules no browser operation');
        equal(Object.hasOwn(plan, 'value'), false, 'reserved selector has no result value');
    }
    for (let kind = 0; kind < 76; ++kind) {
        const frame = {mode: 0, stage: 65535};
        const plan = labDOM.call<EvaluationFixturePlan>('lab_flow_step', kind, frame, {});
        equal(plan.done, true, `unknown stage terminates workflow ${kind}`);
        equal(frame.stage, 65535, 'unknown stage leaves the frame unchanged');
        equal((plan.calls || []).length, 0, 'unknown stage schedules no browser operation');
    }
    for (const [kind, stage] of [[13, 7], [56, 19], [64, 4]]) {
        const plan = labDOM.call<EvaluationFixturePlan>('lab_flow_step', kind, {stage}, {});
        equal(plan.done, true, 'a gap within a stage table uses its completion handler');
    }
    const returned = {exact: 'calculus return'};
    equal(labDOM.call<EvaluationFixturePlan>('lab_flow_step', 7, {stage: 999, value: returned}, {}).value, returned,
          'sparse calculus return stage preserves the exact result');
    for (let kind = 0; kind < 17; ++kind) {
        const plan = labDOM.call<EvaluationFixturePlan>('lab_state_sync', kind, {stage: 65535});
        equal(plan.done, true, 'synchronous dispatch rejects unknown stages');
        equal((plan.calls || []).length, 0, 'unknown synchronous stage has no operations');
    }

    for (const svg of ['<svg data-impulse="true"></svg>', '']) {
        const frame = {stage: 14, action: 'plot', request: {}, result: {
            response: {ok: true}, data: {presentation: {calculus: {plot: {svg}}}}
        }};
        const plan = labDOM.call<EvaluationFixturePlan>('lab_flow_step', 7, frame, {});
        equal(plan.calls[0].service, 'requestOutcome', 'plot request reaches outcome validation');
        equal(plan.calls[0].args[3], svg, 'plot availability follows SVG, not an optional expression field');
    }

    const beforeFlowMarkup = WorksheetContext.rendered.innerHTML;
    const originalContext = {text: 'original context'};
    const firstFrame: EvaluationFixtureFrame = {mode: 0, stage: 0, context: originalContext, options: {skipHistoryUpdate: false}};
    const secondFrame: EvaluationFixtureFrame = {mode: 3, stage: 0, context: originalContext, options: {skipHistoryUpdate: true}};
    const firstView = {text: 'first view'};
    const secondView = {text: 'second view'};
    const firstPreparation = labDOM.call<EvaluationFixturePlan>('lab_flow_step', 0, firstFrame, firstView);
    equal(firstFrame.stage, 1, 'first frame advances to preparation');
    equal(firstFrame.phase, 0, 'first frame initialises setup phase');
    equal(secondFrame.stage, 0, 'first step leaves second frame stage alone');
    equal(secondFrame.context, originalContext, 'first step leaves second frame context alone');
    check(firstFrame.context !== originalContext, 'start allocates a fresh context');
    const firstContext = firstFrame.context;
    const secondPreparation = labDOM.call<EvaluationFixturePlan>('lab_flow_step', 0, secondFrame, secondView);
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
    firstFrame[firstPreparation.save] = labDOM.call(...(firstPreparation.calls[0].args as [string, ...unknown[]]));
    const firstSetup = labDOM.call<EvaluationFixturePlan>('lab_flow_step', 0, firstFrame, firstView);
    equal(firstFrame.stage, 3, 'prepared first frame progresses to setup application');
    equal(secondFrame.stage, 1, 'first setup leaves second frame awaiting preparation');
    equal(secondFrame.phase, 0, 'first setup leaves second setup phase unchanged');
    secondFrame[secondPreparation.save] = labDOM.call(...(secondPreparation.calls[0].args as [string, ...unknown[]]));
    const secondSetup = labDOM.call<EvaluationFixturePlan>('lab_flow_step', 0, secondFrame, secondView);
    equal(firstFrame.stage, 3, 'second setup does not advance first frame');
    equal(firstFrame.context, firstContext, 'second setup does not replace first context');
    secondFrame.context.text = 'second authored σ';
    equal(firstContext.text, 'first authored μ', 'second context mutation remains independent');
    for (const [plan, frame, view] of [[firstSetup, firstFrame, firstView], [secondSetup, secondFrame, secondView]] as const) {
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
    equal(WorksheetContext.rendered.innerHTML, beforeFlowMarkup, 'direct continuation planning has no DOM effects');
    const working = [
        'Evaluating...', 'Solving equation...', 'Solving differential equation...', 'Evaluating matrix...',
        'Integrating...', 'Calculating dates...', 'Working the almanac...'
    ];
    const failures = [
        'Evaluation failed', 'Equation solving failed', 'Differential-equation solving failed',
        'Matrix evaluation failed', 'Integration failed', 'Datetime calculation failed', 'Almanac calculation failed'
    ];
    const preparations = [7, 13, 12, 5, 5, 16, 32];
    const successes = [1, 13, 141, 13, 13, 269, 269];
    const rejected = [6, 6, 6, 38, 70, 22, 6];
    const preparationNames = ['bindings', 'expression', 'input', 'trim', 'datetime', 'almanac', 'history'];
    function planResponse(mode: number, outcome: number, data: unknown = {}, error: unknown = null): EvaluationFixturePlan {
        return labDOM.call<EvaluationFixturePlan>('lab_evaluation_response', mode, outcome, data, {text: 'authored μ'}, error);
    }
    function recoveryNames(flags: number): string {
        const names: string[] = [];
        if (flags & 32)
            names.push('clearMatrixScalar');
        if (flags & 2)
            names.push('setRenderedError', 'resetRenderedDigits');
        if (flags & 16)
            names.push('setDatetimeLocalText');
        if (flags & 4)
            names.push('clearResultDetails');
        if (flags & 8)
            names.push('clearRenderedError');
        if (flags & 64)
            names.push('applyIntegratorBindingState', 'applyIntegratorResultBound', 'saveWorksheetState');
        return names.join(',');
    }
    for (let mode = 0; mode < modes.length; ++mode) {
        for (const skip of [0, 1]) {
            const plan = labDOM.call<EvaluationFixturePlan>('lab_evaluation_prepare_plan', mode, skip);
            const bits = preparations[mode] | (skip ? 0 : 64);
            for (let bit = 0; bit < preparationNames.length; ++bit) {
                const name = preparationNames[bit];
                equal(plan[name], !!(bits & (1 << bit)), 'named preparation ' + name);
            }
            equal(plan.label, working[mode], 'preparation carries the native status');
        }
        for (const outcome of [0, 1, 2, 3, 4]) {
            const data = {error: ' specific μ ', raw_error: ' raw ', status: 'series'};
            const flags = native.lab_evaluation_actions(mode, outcome);
            const before = WorksheetContext.rendered.innerHTML;
            const plan = planResponse(mode, outcome, data);
            equal(WorksheetContext.rendered.innerHTML, before, 'plan creation has no DOM side effects');
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
                equal((clear.args[0] as {keepBindings: boolean}).keepBindings, true, 'recovery preserves authored bindings');
            const diagnostic = plan.calls.find(call => call.service === 'setRenderedError');
            if (diagnostic)
                equal(diagnostic.args[0], mode === 4 ? 'specific μ' : data.error, 'diagnostic trimming policy');
            if (flags & 64) {
                const restore = plan.calls.find(call => call.service === 'applyIntegratorBindingState')!;
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
        equal(labDOM.call<EvaluationFixturePlan>('lab_evaluation_prepare_plan', mode, 0), null, 'invalid mode has no preparation plan');
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

    function diagnostic(mode: number, data: unknown, error: unknown = null): unknown {
        const plan = planResponse(mode, 1, data, error);
        return plan.calls.find(call => call.service === 'setRenderedError')!.args[0];
    }
    equal(
        diagnostic(4, {error: ' Integration failed ', raw_error: ' raw μ '}), 'raw μ',
        'generic error uses raw diagnostic');
    equal(
        diagnostic(4, {error: ' ', raw_error: ' '}), 'Integration failed',
        'blank integration diagnostics use fallback');
    equal(diagnostic(0, {error: ''}), 'Evaluation failed', 'empty expression diagnostic uses fallback');
    equal(diagnostic(0, {error: 'ignored'}, ''), '', 'explicit empty exception overrides native diagnostic');
    equal(diagnostic(0, {error: 'ignored'}, 'exception'), 'exception', 'explicit exception takes precedence');
    const ordered: unknown[] = [];
    const registry = Object.freeze({
        first(value: unknown) {
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

    const savedMode = WorksheetWorkspace.mode.currentMode();
    const savedFunctions = new Map<string, unknown>();
    const savedInstall = WorksheetRequests.evaluation.installEvaluationResult;
    const savedInstallers: EvaluationFixtureInstaller[] = [];
    for (let index = 0; index < modes.length; ++index) {
        function installSaved(data: unknown, context: WorksheetRequests.MarsApiEvaluationContext, outcome: number,
                              request?: WorksheetRequests.MarsApiRequest | null): unknown {
            return savedInstall(index, data, context, outcome, request);
        }
        savedInstallers.push(installSaved);
    }
    const installers: EvaluationFixtureInstaller[] = [...savedInstallers];
    const savedScalar = WorksheetWorkspace.presentation.state.lastMatrixScalarExpression;
    const savedLastInput = WorksheetContext.labEditorState.lastInput;
    const savedFullText = WorksheetContext.labEditorState.fullText, savedDisplayText = WorksheetContext.labEditorState.displayText;
    const savedDerivative = WorksheetWorkspace.presentation.state.lastDerivativeExpression;
    const savedLastTex = WorksheetWorkspace.presentation.state.lastTex;
    const savedVariables = WorksheetBindings.panel.currentVariables;
    const savedDifferentiable = WorksheetBindings.panel.currentDifferentiable;
    const savedInput = WorksheetContext.expr.value;
    const savedFetch = window.fetch;
    const savedDOMCall = labDOM.call;
    const plotRange = document.getElementById('plotRange')!;
    const savedPlotNames: unknown = Reflect.get(plotRange, 'marsPlotNames');
    const nodes = [WorksheetContext.rendered, WorksheetContext.renderedTitle, WorksheetContext.renderedMore, WorksheetContext.functionStyle, WorksheetContext.valueNote, WorksheetContext.valueNoteCard, WorksheetContext.valueTitle];
    const savedNodes: EvaluationFixtureNodeSnapshot[] = [];
    for (const node of nodes) {
        const attributes: [string, string][] = [];
        for (const attribute of node.attributes)
            attributes.push([attribute.name, attribute.value]);
        savedNodes.push({children: Array.from(node.childNodes), attributes});
    }
    const events: [string, ...unknown[]][] = [];
    let mode = 'expression', text = 'authored input';
    let reply: EvaluationFixtureReply = {ok: false}, transportError: unknown = null;
    function setEditorSource(source: string): void {
        WorksheetContext.expr.value = source ? 'evaluation compact fixture' : '';
        WorksheetContext.labEditorState.displayText = WorksheetContext.expr.value;
        WorksheetContext.labEditorState.fullText = source;
    }
    function replace(name: string, callback: (...args: never[]) => unknown): void {
        if (!savedFunctions.has(name)) {
            check(typeof Reflect.get(labTestModuleOwner(name), name) === 'function', 'writable browser service ' + name);
            savedFunctions.set(name, Reflect.get(labTestModuleOwner(name), name));
        }
        Reflect.set(labTestModuleOwner(name), name, callback);
    }
    function record(name: string): (...args: unknown[]) => number {
        return function recordCall(...args: unknown[]): number {
            return events.push([name, ...args]);
        };
    }
    function calls(name: string): [string, ...unknown[]][] {
        return events.filter(event => event[0] === name);
    }
    // Retain an opaque result for fixtures that complete with different payload shapes.
    function createValueGate(): EvaluationFixtureGate {
        let resolve!: (value: unknown) => void;
        let reject!: (reason: unknown) => void;
        const promise = new Promise<unknown>(function retainCompletion(accept, fail) {
            resolve = accept;
            reject = fail;
        });
        return {promise, resolve, reject};
    }
    async function fetchResult() {
        events.push(['fetch', mode]);
        if (transportError)
            throw transportError;
        return {response: {ok: true}, data: reply};
    }
    async function commitBindings(): Promise<number> {
        return events.push(['bindings']);
    }
    async function retainSource(source: string): Promise<string> {
        return source;
    }
    function assertNoCompletion(): void {
        for (const name of ['install', 'setRenderedError', 'commitModeState', 'updateHistoryButtons', 'setStatus'])
            equal(calls(name).length, 0, 'stale request cannot perform ' + name);
    }
    try {
        // Observe presentation at the DOM boundary; its real projection has a dedicated card suite.
        const layoutServices: {[name: string]: (...args: unknown[]) => number} = {
            lab_layout_error(message: unknown, enabled: unknown) {
                return events.push([enabled ? 'setRenderedError' : 'clearRenderedError', message]);
            },
            lab_layout_more: record('resetMoreDigitsButton'),
            lab_layout_content: record('setRenderedContent'),
            lab_layout_solver_install: record('installSolverTexSvg'),
            lab_evaluation_cards_present: record('presentEvaluationCards')
        };
        // Preserve the production bridge's generic result contract at this fixture boundary.
        labDOM.call = function callFixture<T = unknown>(name: string, ...args: unknown[]): T {
            if (Object.hasOwn(layoutServices, name))
                return layoutServices[name](...args) as T;
            return savedDOMCall<T>(name, ...args);
        };
        replace('currentMode', () => mode);
        setEditorSource(text);
        replace('visibleBindingValues', () => ({x: '1/3'}));
        replace('commitVisibleBindingInputs', commitBindings);
        replace('expressionWithVisibleBindings', retainSource);
        replace('historyStateForMode', (selected: string, source: string) => ({mode: selected, text: source}));
        function previousState(next: {mode: string}) {
            events.push(['previous', next]);
            return {mode: next.mode, text: 'previous input'};
        }
        replace('previousModeStateForHistory', previousState);
        replace('currentDatetimeState', () => ({date: '2000-01-01', latitude: '1', longitude: '2'}));
        replace('datetimeSummaryText', () => 'date snapshot');
        replace('currentAlmanacState', () => ({}));
        replace('almanacSummaryText', () => 'almanac snapshot');
        for (const name
                 of ['setBusy', 'setActionRunning', 'showResults', 'setStatus', 'pushExpressionHistory',
                     'updateHistoryButtons', 'commitModeState', 'saveWorksheetState', 'clearResultDetails',
                     'setDatetimeLocalText', 'applyIntegratorBindingState', 'applyIntegratorResultBound',
                     'refreshDatetimeWeather', 'scheduleRenderedTeXFit', 'setExpandableText', 'setResultInputText',
                     'scheduleSolverTexFit', 'scheduleAutomaticPlot'])
            replace(name, record(name));
        replace('fetch', fetchResult);
        for (let index = 0; index < modes.length; ++index) {
            installers[index] = function recordInstall(data: unknown): number {
                return events.push(['install', modes[index], data]);
            };
        }
        function installResult(mode: number, data: unknown, context: WorksheetRequests.MarsApiEvaluationContext,
                               outcome: number, request?: WorksheetRequests.MarsApiRequest | null): unknown {
            return installers[mode](data, context, outcome, request);
        }
        replace('installEvaluationResult', installResult);

        // Each mode reaches the same native-directed adapter through its public entry point.
        Reflect.set(plotRange, 'marsPlotNames', ['x']);
        const evaluate = [
            WorksheetRequests.evaluation.evaluateExpression, WorksheetRequests.evaluation.evaluateEquation, WorksheetRequests.evaluation.evaluateDiffequation,
            WorksheetRequests.evaluation.evaluateMatrix, WorksheetRequests.evaluation.evaluateIntegrator,
            WorksheetRequests.evaluation.evaluateDatetime, WorksheetRequests.evaluation.evaluateAlmanac
        ];
        for (let index = 0; index < modes.length; ++index) {
            mode = modes[index];
            for (const ok of [false, true]) {
                events.length = 0;
                reply = {ok, status: 'solved'};
                WorksheetWorkspace.presentation.state.lastMatrixScalarExpression = 'previous scalar';
                await evaluate[index]();
                equal(calls('bindings').length, [0, 1, 3, 4].includes(index) ? 1 : 0, 'binding commit policy');
                equal(calls('fetch').length, 1, 'one evaluation fetch');
                equal(calls('install').length, ok ? 1 : 0, 'install only accepted results');
                equal(calls('scheduleAutomaticPlot').length, ok && (index < 3 || index === 4) ? 1 : 0,
                      'successful plotting modes schedule a separate plot');
                equal(calls('commitModeState').length, 1, 'one result commit');
                equal(calls('pushExpressionHistory').length, 1, 'previous state pushed once');
                equal(calls('updateHistoryButtons').length, 1, 'history finalised once');
                equal(calls('setStatus')[0][1], working[index], 'mode working status');
                equal(calls('setStatus').at(-1)![1], ok ? 'Ready' : 'Error', 'mode completion status');
                equal(calls('refreshDatetimeWeather').length, ok && (index === 5 || index === 6) ? 1 : 0,
                    'calendar weather after success only');
                if (ok && index === 5) {
                    equal(calls('refreshDatetimeWeather')[0][1], WorksheetRequests.labRequests.latestMain(), 'weather uses parent token');
                    equal((calls('refreshDatetimeWeather')[0][2] as {date: string}).date, '2000-01-01', 'weather retains captured date');
                }
                equal(calls('applyIntegratorBindingState').length, !ok && index === 4 ? 1 : 0, 'integration bindings');
                equal(
                    calls('setDatetimeLocalText').length, !ok && index === 5 ? 1 : 0, 'failed date clears local card');
                equal(WorksheetWorkspace.presentation.state.lastMatrixScalarExpression, !ok && index === 3 ? '' : 'previous scalar', 'matrix failure cache');
                if (!ok)
                    equal(calls('setRenderedError')[0][1], failures[index], 'mode fallback diagnostic');
                check(!WorksheetRequests.labRequests.busy(), 'completed evaluation releases native busy state');
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
            check(!WorksheetRequests.labRequests.busy(), 'exception releases native busy state');
        }

        // A successful HTTP response without native markup must not publish a blank, Ready worksheet.
        mode = 'almanac';
        const previousAlmanac = WorksheetContext.worksheetSession.calendar.almanacLastWorksheetData;
        installers[6] = savedInstallers[6];
        for (const invalid
                 of [{ok: true, visibility: 'all'},
                     {ok: true, visibility: 'visible', almanac_presentation: {visible: {html: ''}}}]) {
            events.length = 0;
            reply = invalid;
            await WorksheetRequests.evaluation.evaluateAlmanac();
            check(
                (calls('setRenderedError')[0] as [string, string])?.[1].includes('Almanac presentation'),
                'missing native presentation must show its diagnostic');
            equal(calls('setStatus').at(-1)![1], 'Error', 'missing native presentation cannot report Ready');
            equal(WorksheetContext.worksheetSession.calendar.almanacLastWorksheetData, previousAlmanac, 'invalid response cannot replace saved worksheet');
            check(!WorksheetRequests.labRequests.busy(), 'invalid presentation releases native busy state');
        }
        installers[6] = data => events.push(['install', mode, data]);

        text = '';
        setEditorSource(text);
        WorksheetContext.expr.value = '';
        for (let index = 0; index < modes.length; ++index) {
            mode = modes[index];
            events.length = 0;
            reply = {ok: false};
            await evaluate[index]();
            equal(calls('fetch').length, index >= 5 ? 1 : 0, 'calendar snapshots do not require editor text');
            equal(calls('pushExpressionHistory').length, index >= 5 ? 1 : 0, 'empty mathematical input keeps history');
            check(!WorksheetRequests.labRequests.busy(), 'empty input releases native busy state');
        }
        WorksheetContext.expr.value = savedInput;
        text = '  authored input  ';
        setEditorSource(text);
        for (const index of [0, 1, 2, 3, 4]) {
            mode = modes[index];
            events.length = 0;
            await evaluate[index]();
            equal(
                (calls('previous')[0][1] as {text: string}).text, index === 1 || index === 2 ? text.trim() : text,
                'history preserves mode-specific text preparation');
        }
        text = 'authored input';
        setEditorSource(text);
        mode = 'expression';
        WorksheetContext.labEditorState.lastInput = 'previous exact native input';
        events.length = 0;
        await WorksheetRequests.evaluation.evaluateExpression({reuseLastInput: true});
        equal((calls('previous')[0][1] as {text: string}).text, 'previous exact native input', 'precision evaluation reuses last input');
        equal(calls('saveWorksheetState')[0][2], text, 'reuse still saves authored editor text');

        mode = 'diffequation';
        for (const [status, expected] of [['series', 'Local series'], ['reduced', 'Not solved']]) {
            events.length = 0;
            reply = {ok: true, status};
            await WorksheetRequests.evaluation.evaluateDiffequation();
            equal(calls('setStatus').at(-1)![1], expected, 'solver completion label comes from C');
        }

        mode = 'integrator';
        for (const [error, raw_error, expected] of [
                 [' Integration failed ', ' precise native reason ', 'precise native reason'],
                 [' specific reason ', ' raw reason ', 'specific reason'], ['', ' raw reason ', 'raw reason'],
                 [' ', ' ', 'Integration failed']]) {
            events.length = 0;
            reply = {ok: false, error, raw_error};
            await WorksheetRequests.evaluation.evaluateIntegrator();
            equal(calls('setRenderedError')[0][1], expected, 'integration diagnostic fallback');
            equal(calls('applyIntegratorResultBound').length, 1, 'native failure restores integration bounds');
        }

        mode = 'matrix';
        events.length = 0;
        async function failedResponse() {
            return {response: {ok: false}, data: {ok: true, error: 'HTTP fixture failure'}};
        }
        replace('fetch', failedResponse);
        await WorksheetRequests.evaluation.evaluateMatrix();
        equal(calls('install').length, 0, 'HTTP failure cannot install a successful-looking payload');
        equal(calls('setRenderedError')[0][1], 'HTTP fixture failure', 'HTTP failure retains native diagnostic');
        replace('fetch', fetchResult);
        events.length = 0;
        reply = {ok: true};
        const matrixInstaller = installers[3];
        installers[3] = async function failInstall() {
            throw new Error('installer fixture failure');
        };
        await WorksheetRequests.evaluation.evaluateMatrix();
        installers[3] = matrixInstaller;
        equal(calls('setRenderedError')[0][1], 'Error: installer fixture failure', 'installer exception reaches catch');
        equal(calls('commitModeState').length, 1, 'installer exception commits failure once');
        equal(calls('updateHistoryButtons').length, 1, 'installer exception finalises history once');
        check(!WorksheetRequests.labRequests.busy(), 'installer exception releases native busy state');

        mode = 'expression';
        events.length = 0;
        async function failCommit(): Promise<never> {
            throw new Error('binding setup failed');
        }
        replace('commitVisibleBindingInputs', failCommit);
        await WorksheetRequests.evaluation.evaluateExpression();
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
        await WorksheetRequests.evaluation.evaluateExpression();
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
        equal(calls('setStatus').at(-1)![1], 'Error', 'partial cards retain error status');

        events.length = 0;
        reply = {...reply, partial_error: false, error: ''};
        await WorksheetRequests.evaluation.evaluateExpression();
        equal(calls('setRenderedResult')[0][1], reply, 'successful rendering receives the unchanged native response');
        equal(calls('setExpressionEditor')[0][1], text, 'successful evaluation retains the authored editor');
        equal(
            (calls('setExpressionEditor')[0][2] as WorksheetBinding[]).length, reply.binding_values!.length,
            'native binding selection retains every discovered parameter');
        equal(
            (calls('setExpressionEditor')[0][2] as WorksheetBinding[])[0], reply.binding_values![0],
            'unoverridden native binding record retains its exact identity');
        equal(
            calls('setExpandableText')[0][3], reply.display_expression, 'success preserves native symbolic expression');
        equal(calls('setExpandableText')[1][4], reply.full_display_function, 'native function declarations retained');
        equal(calls('saveWorksheetState').length, 2, 'successful expression saves preparation and installed state');
        equal(calls('setStatus').at(-1)![1], 'Ready', 'successful expression reaches ready status');
        installers[0] = data => events.push(['install', mode, data]);

        // Supersession at either awaited preparation boundary must prevent fetch and history writes.
        for (const boundary of ['commitVisibleBindingInputs', 'expressionWithVisibleBindings']) {
            const gate = createValueGate();
            const entered = LabTestBrowser.signal();
            function awaitPreparation(): Promise<unknown> {
                entered.resolve();
                return gate.promise;
            }
            replace(boundary, awaitPreparation);
            const pending = WorksheetRequests.evaluation.evaluateExpression();
            await entered.promise;
            const newer = WorksheetRequests.labRequests.begin('evaluate', mode);
            events.length = 0;
            gate.resolve(text);
            await pending;
            assertNoCompletion();
            equal(
                calls('fetch').length + calls('pushExpressionHistory').length, 0,
                'stale preparation performs no writes');
            check(WorksheetRequests.labRequests.busy(), 'stale preparation finaliser retains newer busy state');
            WorksheetRequests.labRequests.finish(newer);
            replace(boundary, boundary === 'commitVisibleBindingInputs' ? commitBindings : retainSource);
        }

        // A late success or rejection, including switching away and back, must be invisible.
        for (const scenario of ['success', 'rejection', 'mode change']) {
            const gate = createValueGate();
            const entered = LabTestBrowser.signal();
            function awaitFetch(): Promise<unknown> {
                entered.resolve();
                return gate.promise;
            }
            replace('fetch', awaitFetch);
            const pending = WorksheetRequests.evaluation.evaluateExpression();
            await entered.promise;
            if (scenario === 'mode change') {
                mode = 'matrix';
                WorksheetRequests.labRequests.modeChanged(mode);
                mode = 'expression';
                WorksheetRequests.labRequests.modeChanged(mode);
            }
            const newer = WorksheetRequests.labRequests.begin('evaluate', mode);
            events.length = 0;
            if (scenario === 'rejection')
                gate.reject(new Error('obsolete failure'));
            else
                gate.resolve({response: {ok: true}, data: {ok: true}});
            await pending;
            assertNoCompletion();
            check(WorksheetRequests.labRequests.busy(), 'stale fetch finaliser retains newer busy state');
            WorksheetRequests.labRequests.finish(newer);
        }

        // Exercise the real differential-equation installer and binary solver-render transport.
        replace('fetch', fetchResult);
        mode = 'diffequation';
        installers[2] = savedInstallers[2];
        reply = {ok: true, status: 'solved', steps: 'native derivation', steps_left_TeX: 'x = 1'};
        const solver = LabTestBrowser.responseGate();
        const solverEntered = LabTestBrowser.signal();
        window.fetch = function awaitRender(url, options) {
            equal(String(url), '/render_TeX', 'solver render uses native endpoint');
            equal((labWire.decode(options!.body as Uint8Array<ArrayBuffer>) as {tex: string}).tex, 'x = 1', 'solver mathematics passed through unchanged');
            solverEntered.resolve();
            return solver.promise;
        };
        const solving = WorksheetRequests.evaluation.evaluateDiffequation();
        await solverEntered.promise;
        const newer = WorksheetRequests.labRequests.begin('evaluate', mode);
        events.length = 0;
        solver.resolve(new Response(
            labWire.encode({ok: true, svg: '<svg/>'}), {headers: {'Content-Type': 'application/x-protobuf'}}));
        await solving;
        assertNoCompletion();
        equal(calls('installSolverTexSvg').length, 0, 'stale solver render cannot install an SVG');
        check(WorksheetRequests.labRequests.busy(), 'stale asynchronous installer retains newer busy state');
        WorksheetRequests.labRequests.finish(newer);
    } finally {
        WorksheetRequests.labRequests.cancel('evaluate');
        window.fetch = savedFetch;
        labDOM.call = savedDOMCall;
        if (savedPlotNames === undefined) Reflect.deleteProperty(plotRange, 'marsPlotNames');
        else Reflect.set(plotRange, 'marsPlotNames', savedPlotNames);
        for (const [name, callback] of savedFunctions) Reflect.set(labTestModuleOwner(name), name, callback);
        WorksheetWorkspace.presentation.state.lastMatrixScalarExpression = savedScalar;
        WorksheetContext.labEditorState.lastInput = savedLastInput;
        WorksheetContext.labEditorState.fullText = savedFullText;
        WorksheetContext.labEditorState.displayText = savedDisplayText;
        WorksheetWorkspace.presentation.state.lastDerivativeExpression = savedDerivative;
        WorksheetWorkspace.presentation.state.lastTex = savedLastTex;
        WorksheetBindings.panel.currentVariables = savedVariables;
        WorksheetBindings.panel.currentDifferentiable = savedDifferentiable;
        WorksheetContext.expr.value = savedInput;
        for (let index = 0; index < nodes.length; ++index) {
            const node = nodes[index];
            node.replaceChildren(...savedNodes[index].children);
            for (const attribute of Array.from(node.attributes)) node.removeAttribute(attribute.name);
            for (const [name, value] of savedNodes[index].attributes) node.setAttribute(name, value);
        }
        WorksheetRequests.labRequests.modeChanged(savedMode);
    }
};
