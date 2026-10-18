/**
 * @file result_flow_checks.ts
 * @brief Result-action continuation and clipboard-failure regressions.
 *
 * Checks native binding snapshots, bootstrap and result-action sequencing while controlling editor supersession and browser service failures.
 */

/** Native result-action continuations, edit supersession and clipboard failures. */
window.checkLabResultFlows = async function checkLabResultFlows() {
    function check(condition: unknown, message: string): void {
        if (!condition)
            throw new Error('Result flow: ' + message);
    }
    const saved = new Map<string, unknown>();
    const events: unknown[][] = [];
    const originalBindings = WorksheetWorkspace.presentation.state.resultInputBindings;
    const originalEditor = WorksheetContext.expr.value;
    const metadata = {name: '__proto__', value: 'π/7', extra: {native: true}};
    const sparse = [metadata, , null];
    const snapshot = labDOM.call<{name?: string; extra?: unknown}[]>('lab_result_binding_snapshot', sparse);
    check(snapshot.length === 3 && !(1 in snapshot), 'binding snapshot preserves sparse indices');
    check(snapshot[0] !== metadata && snapshot[0].extra === metadata.extra, 'metadata snapshot is a shallow copy');
    check(snapshot[0].name === '__proto__' && Object.keys(snapshot[2]).length === 0, 'safe metadata and null records');
    check(labDOM.call<unknown[]>('lab_result_binding_snapshot', {length: 2}).length === 0, 'non-array bindings are ignored');
    function boot(frame: object): ResultFlowFixturePlan {
        return labDOM.call<ResultFlowFixturePlan>('lab_flow_step', 64, frame, {});
    }
    const initial = boot({});
    check(initial.calls[0].args[0] === '/bootstrap' && initial.wait, 'bootstrap endpoint and await are native');
    for (const stage of [1, 5]) {
        const failure = boot({stage, response: {ok: false, status: 503}});
        check(
            failure.calls[0].service === 'fail' && (failure.calls[0].args[0] as string).endsWith('(503)'),
            'bootstrap rejects failed configuration and catalogue responses');
    }
    const wrongABI = boot({stage: 2, data: {BROWSER_ABI_VERSION: 'old'}});
    check(
        wrongABI.calls[0].service === 'fail' && (wrongABI.calls[0].args[0] as string).includes('make mars-lab-restart'),
        'ABI mismatch retains the restart diagnostic');
    const scripts = boot({stage: 8, valid: true}).calls[0].args[0] as string[];
    check(scripts.join(',') === labApplication.definitionScripts.join(','), 'loaded definitions come from the native catalogue');
    check(scripts.includes('bindings_controller') && scripts.includes('bindings'),
          'binding controller and controls are both loaded before worksheet initialisation');
    check(scripts.includes('workspace_controllers') && !scripts.includes('workspace'),
          'workspace controller definitions load before the separate workspace initialisation stage');
    check(scripts.indexOf('results') >= 0 && scripts.indexOf('results_services') > scripts.indexOf('results'),
          'result services follow their controllers in the startup catalogue');
    const widgets = labDOM.call<ResultFlowWidgetPlan>('lab_bootstrap_widgets');
    check(widgets.calls.length === 4 && widgets.calls.every(call => call.args[0].tagName === 'SELECT'),
          'only location dropdowns need native menu enhancement; plot settings are radio buttons');
    check(
        widgets.calls[0].args[1].searchPlaceholder === 'Search jurisdictions' &&
            widgets.calls[2].args[1].placeholder === 'Custom location',
        'native search-control labels');
    function replace(name: string, callback: MarsTransportService): void {
        if (!saved.has(name))
            saved.set(name, Reflect.get(labTestModuleOwner(name), name));
        Reflect.set(labTestModuleOwner(name), name, callback);
    }
    let mode = 'expression';
    let source = 'native result';
    let blocked = false;
    function record(name: string): (...args: unknown[]) => number {
        return function recordEvent(...args: unknown[]): number {
            return events.push([name, ...args]);
        };
    }
    try {
        replace('currentMode', () => mode);
        replace('currentExpressionText', () => 'authored source');
        replace('resultExpressionTextForInput', () => source);
        replace('prepareLabEditor', async (text: string) => events.push(['prepare', text]));
        replace('historyStateForMode', (_mode: string, text: string) => ({text: text || 'old source'}));
        replace('historyStatesEqual', (left: {text: string}, right: {text: string}) => left.text === right.text);
        for (const name
                 of ['pushExpressionHistory', 'clearGoalSeekRequest', 'hideTargetEntry', 'setExpressionEditor',
                     'saveCurrentModeEditorState', 'updateHistoryButtons', 'setStatus', 'syncMatrixControls',
                     'syncRoundedSelect'])
            replace(name, record(name));
        replace('applyMarsBindingExpression', async function applyFixtureBindings(...args: unknown[]) {
            events.push(['binding', ...args]);
            return !blocked;
        });
        replace('compactExpressionForEditor', () => ({bindings: [{name: 'x', value: 'α'}]}));
        replace('expressionWithBindings', async function assembleFixture(text: string, bindings: WorksheetBinding[]) {
            events.push(['assemble', text, bindings]);
            return 'native assembled';
        });
        replace('bindingsWithAuthoredValues', (bindings: WorksheetBinding[]) => bindings);
        WorksheetWorkspace.presentation.state.resultInputBindings = [];
        const oldOperation = WorksheetContext.matrixOperation.value;
        const oldOperand = WorksheetContext.matrixOperand.value;
        try {
            for (mode of ['expression', 'matrix', 'equation', 'diffequation']) {
                events.length = 0;
                await WorksheetResults.reuse.sendResultExpressionToInput();
                check(events.filter(event => event[0] === 'prepare').length === 2, 'both editor metadata requests');
                check(events.filter(event => event[0] === 'pushExpressionHistory').length === 1, 'one history push');
                check(
                    events.filter(event => event[0] === 'saveCurrentModeEditorState').length === 1, 'one editor save');
                check(events.at(-1)![1] === 'Result sent to input', 'completion status follows persistence');
                if (mode === 'matrix')
                    check(
                        events.find(event => event[0] === 'setExpressionEditor')![1] === 'native assembled',
                        'matrix uses native bound source without mathematical rewriting');
                else if (mode !== 'expression')
                    check(
                        events.find(event => event[0] === 'setExpressionEditor')![1] === source,
                        'equations retain native result text exactly');
            }
        } finally {
            WorksheetContext.matrixOperation.value = oldOperation;
            WorksheetContext.matrixOperand.value = oldOperand;
        }
        mode = 'expression';
        blocked = true;
        events.length = 0;
        await WorksheetResults.reuse.sendResultExpressionToInput();
        check(!events.some(event => event[0] === 'saveCurrentModeEditorState'), 'rejected binding cannot be persisted');
        blocked = false;
        source = '';
        events.length = 0;
        await WorksheetResults.reuse.sendResultExpressionToInput();
        check(events.length === 0, 'empty result is inert');
        source = 'native result';
        replace('prepareLabEditor', async function changeEditor() {
            WorksheetContext.expr.value = 'new edit';
        });
        events.length = 0;
        await WorksheetResults.reuse.sendResultExpressionToInput();
        check(events.length === 0, 'intervening edit cancels transfer before history or editor mutation');

        WorksheetContext.expr.value = originalEditor;
        replace('prepareLabEditor', async function changeMode() {
            WorksheetRequests.labRequests.modeChanged('equation');
            WorksheetRequests.labRequests.modeChanged('expression');
        });
        events.length = 0;
        await WorksheetResults.reuse.sendResultExpressionToInput();
        check(events.length === 0, 'mode round trip cancels transfer even when editor and mode match again');

        replace('commitVisibleBindingInputs', async () => events.push(['commit']));
        replace('datetimeSummaryText', () => 'calendar source');
        replace('copyTextForTarget', (target: string) => target === 'empty' ? '' : 'native card source');
        replace('writeClipboardText', async function writeClipboard(text: string) {
            events.push(['clipboard', text]);
            if (blocked)
                throw new Error('clipboard fixture');
        });
        replace('flashCopyButton', record('flash'));
        replace('setTimeout', function recordTimer(_callback: TimerHandler, delay: number) {
            events.push(['timer', delay]);
            return 0;
        });
        for (mode of ['expression', 'datetime']) {
            for (blocked of [false, true]) {
                events.length = 0;
                WorksheetContext.expr.value = '  authored source  ';
                await WorksheetResults.actions.copyInputFromEvent();
                check(events[0][0] === 'commit', 'copy waits for visible bindings');
                check(
                    events.find(event => event[0] === 'clipboard')![1] ===
                        (mode === 'datetime' ? 'calendar source' : 'authored source'),
                    'copy source policy');
                check(events.find(event => event[0] === 'flash')![2] === !blocked, 'copy failure feedback');
                check(events.some(event => event[0] === 'timer') === !blocked, 'only success schedules status reset');
            }
        }
        const button = document.createElement('button');
        button.dataset.copyTarget = 'expression';
        blocked = false;
        events.length = 0;
        await WorksheetResults.actions.copyResultFromEvent(button);
        check(events[0][1] === 'native card source', 'card copy retains exact native source');
        button.dataset.copyTarget = 'empty';
        events.length = 0;
        await WorksheetResults.actions.copyResultFromEvent(button);
        check(events.length === 0, 'empty card has no clipboard or feedback side effects');
    } finally {
        for (const [name, callback] of saved) Reflect.set(labTestModuleOwner(name), name, callback);
        WorksheetWorkspace.presentation.state.resultInputBindings = originalBindings;
        WorksheetContext.expr.value = originalEditor;
    }
};
