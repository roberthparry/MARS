/**
 * @file state_flow_checks.ts
 * @brief Worksheet state continuation and resource-ownership regressions.
 *
 * Checks isolated continuation frames, recovery boundaries, awaited cleanup and namespace-owned persistence and history resources.
 */

/** Create an opaque deferred value without adding a timer or continuation. */
function labStateDeferred(): LabWorksheetTestDeferred {
    let resolve!: (value?: unknown) => void;
    let reject!: (reason?: unknown) => void;
    const promise = new Promise<unknown>(function captureSettlement(accept, fail) {
        resolve = accept;
        reject = fail;
    });
    return {promise, resolve, reject};
}

/** Native state continuations: isolated frames, recovery boundaries and awaited cleanup. */
window.checkLabStateFlow = async function checkLabStateFlow() {
    function check(condition: unknown, message: string): void {
        if (!condition)
            throw new Error('State flow: ' + message);
    }
    function equal(actual: unknown, expected: unknown, message: string): void {
        check(Object.is(actual, expected), message);
    }
    const saved = new Map<string, LabWorksheetTestReplacement>();
    function replace(target: string | readonly [object, string], callback: MarsTransportService): void {
        let owner: object;
        let key: string;
        let name: string;
        if (typeof target === 'string') {
            owner = labTestModuleOwner(target);
            key = target;
            name = target;
        } else {
            [owner, key] = target;
            name = 'owned:' + key;
        }
        if (!saved.has(name))
            saved.set(name, {owner, key, callback: Reflect.get(owner, key)});
        Reflect.set(owner, key, callback);
    }
    function step(kind: number, frame: object): LabWorksheetTestPlan {
        return labDOM.call<LabWorksheetTestPlan>('lab_flow_step', kind, frame, {});
    }
    function sync(kind: number, frame: object): LabWorksheetTestPlan {
        return labDOM.call<LabWorksheetTestPlan>('lab_state_sync', kind, frame);
    }
    function bootstrap(phase: number, context: unknown): LabWorksheetTestPlan {
        return labDOM.call<LabWorksheetTestPlan>('lab_bootstrap_workspace', phase, context);
    }
    function unread(): never {
        throw new Error('inactive bootstrap getter was read');
    }
    equal(
        bootstrap(0, {
            token: '',
            get search() {
                return unread();
            }
        }).calls.length,
        0, 'absent token does not inspect the URL');
    equal(
        bootstrap(0, {
            token: 'token',
            search: '?ordinary=1',
            prefix: 'control=',
            get pathname() {
                return unread();
            },
            get hash() {
                return unread();
            }
        }).calls.length,
        0, 'unmatched token prefix does not inspect the replacement location');
    const startupReads: string[] = [];
    const locationPlan = bootstrap(0, {
        token: 'token',
        get search() {
            startupReads.push('search');
            return '?other=1&control=opaque';
        },
        get prefix() {
            startupReads.push('prefix');
            return 'control=';
        },
        get pathname() {
            startupReads.push('pathname');
            return '/lab';
        },
        get hash() {
            startupReads.push('hash');
            return '#worksheet';
        }
    });
    equal(startupReads.join(','), 'search,prefix,pathname,hash', 'URL getters retain startup order');
    let replacement;
    labDOM.services(locationPlan, {replaceLocation: url => replacement = url});
    equal(replacement, '/lab#worksheet', 'token cleanup retains path/hash and removes the query');
    equal(
        bootstrap(1, {
            control: {value: 'authored'},
            get fallback() {
                return unread();
            }
        }).calls.length,
        0, 'authored jurisdiction neither changes nor reads the default');
    equal(
        bootstrap(1, {
            control: null,
            get fallback() {
                return unread();
            }
        }).calls.length,
        0, 'missing jurisdiction needs no default write');
    const startupOffset = {value: ''};
    let jurisdictionValue = '';
    const startupJurisdiction = {
        get value() {
            return jurisdictionValue;
        },
        set value(value) {
            jurisdictionValue = value;
            startupOffset.value = '0';
        }
    };
    const jurisdictionPlan = bootstrap(1, {control: startupJurisdiction, fallback: 'GB-ENG'});
    equal(jurisdictionValue, '', 'jurisdiction planning does not write before host effects');
    labDOM.services(jurisdictionPlan, {writeValue: (control: {value: string}, value: string) => control.value = value});
    equal(jurisdictionValue, 'GB-ENG', 'blank jurisdiction receives native-selected default');
    equal(
        bootstrap(2, {control: startupOffset, fallback: '1'}), '0',
        'GMT capture follows jurisdiction effects and preserves a zero-valued control');
    equal(bootstrap(2, {control: {value: ''}, fallback: '1'}), '1', 'blank GMT uses its default');
    equal(bootstrap(2, {control: null, fallback: -3.5}), '-3.5', 'absent GMT default receives string coercion');
    equal(
        bootstrap(99, {
            get token() {
                return unread();
            }
        }).calls.length,
        0, 'invalid bootstrap phase has no effects');
    const canonicalFrame = {index: 1, text: '  authored equation  ', options: {}};
    const canonical = sync(0, canonicalFrame);
    equal(canonical.calls[0].service, 'stateCanonical', 'synchronous save selects native canonical metadata');
    equal(canonical.calls[0].args[0], 'authored equation', 'save trims only outer whitespace before canonical lookup');
    equal(canonical.wait, false, 'synchronous save remains synchronous');
    let keepaliveReads = 0;
    const options = {
        get keepalive() {
            ++keepaliveReads;
            return true;
        }
    };
    const delayed = sync(1, {stage: 1, save: {index: 0, delay: 250, options, patch: {expression: 'queued'}}});
    equal(keepaliveReads, 0, 'deferred publication never reads keepalive');
    equal((delayed.calls[0].args[1] as WorksheetSaveOptions).keepalive, false, 'deferred saves cannot become unload saves');
    const immediate = sync(1, {stage: 1, save: {index: 0, delay: 0, options, patch: {expression: 'immediate'}}});
    equal(keepaliveReads, 1, 'immediate publication reads keepalive at publication time');
    equal((immediate.calls[0].args[1] as WorksheetSaveOptions).keepalive, true, 'immediate keepalive is retained');
    const staleSave = sync(1, {save: {index: 0, token: 0, options}});
    equal(staleSave.done, true, 'unowned timer publishes no effects');
    equal(keepaliveReads, 1, 'unowned timer cannot observe publish-time options');
    equal(
        sync(5, {stage: 2, result: null}).calls[0].service, 'stateClearResult', 'missing result clearing stays native');
    equal(
        sync(8, {stage: 3, index: 6}).calls[0].service, 'stateEvaluateExpression',
        'keyboard fallback retains previous almanac behaviour');
    const first: LabWorksheetTestEditorFrame = {read: key => key, local: false};
    const second: LabWorksheetTestEditorFrame = {read: key => key, local: true};
    const firstPlan = step(16, first);
    const secondPlan = step(16, second);
    equal(firstPlan.calls[0].service, 'stateRead', 'server editors begin with a lazy read');
    equal(firstPlan.calls[0].args[1], 'expression', 'server schema remains native');
    equal(secondPlan.calls[0].args[1], 'mars.exprLab.lastExpression', 'local schema remains native');
    equal(firstPlan.calls[0].args[0], first.read, 'saved callback survives another native scope');
    first.raw = ' server source ';
    const prepare = step(16, first);
    equal(first.text, 'server source', 'server source is trimmed before preparation');
    equal(second.stage, 2, 'one editor continuation does not advance another');
    equal(second.text, undefined, 'one editor continuation does not mutate another context');
    equal(prepare.calls[0].service, 'statePrepare', 'editor metadata is prepared before projection');
    equal(prepare.wait, true, 'preparation suspension is selected natively');

    const controls: {fields?: unknown} = {};
    const schema = step(17, controls);
    controls.fields = labDOM.call(...schema.calls[0].args as [string, ...unknown[]]);
    equal(step(17, controls).calls[0].args[1], 'equation_variable', 'control reads begin in server order');
    const boundsFrame = {stage: 3, index: 3, bounds: 'x = 0 .. 1'};
    const bounds = step(17, boundsFrame);
    equal(bounds.calls[0].service, 'stateBounds', 'bounds restore is a distinct capability');
    equal(bounds.wait, true, 'later control reads cannot cross the bounds await');
    equal(boundsFrame.index, 3, 'next control is not selected before bounds completion');

    const obsolete = {stage: 4, state: {mode: 'matrix'}, current: false};
    equal(step(21, obsolete).value, false, 'stale history cannot reach control projection');
    const obsoleteSelection = {stage: 11, current: false};
    equal(step(24, obsoleteSelection).done, true, 'stale selection stops before bounds/default projection');
    for (const [action, service] of [
             [1, 'statePrecisionGoal'], [2, 'statePrecisionExpression'], [3, 'stateEvaluate']]) {
        const frame = {stage: 4, plan: {action, source: 'native source', target: '1/3'}};
        const plan = step(25, frame);
        equal(plan.calls[0].service, service, 'precision dispatch is selected by the C continuation');
        equal(plan.wait, true, 'precision work must finish before history cleanup');
        equal(plan.calls[0].args[0], frame.plan, 'precision operands remain opaque browser-owned values');
    }

    const events: string[] = [];
    const getItem = Storage.prototype.getItem;
    const originalMode = WorksheetWorkspace.mode.currentMode();
    try {
        async function fetchFixture() {
            return {
                async labData() {
                    return {marker: 'server'};
                }
            };
        }
        replace([labHttp, 'fetch'], fetchFixture);
        replace([WorksheetPersistence.restoration, 'applySaved'], async (data: {marker: string}) => events.push('server:' + data.marker));
        replace([WorksheetPersistence.restoration, 'recoverLocalEditors'], async function rejectRecovery() {
            events.push('recovery');
            throw new Error('locked local storage');
        });
        replace([WorksheetPersistence.restoration, 'restoreEditors'], async (_read: (key: string) => unknown, local: boolean) => events.push('editors:' + local));
        replace([WorksheetPersistence.restoration, 'restoreControls'], async (_read: (key: string) => unknown, local: boolean) => events.push('controls:' + local));
        replace([WorksheetPersistence.persistence, 'readLocal'], function readLocal(key: string) {
            events.push(key);
            return null;
        });
        replace('syncTownSelectors', async () => events.push('towns'));
        replace('restoreTownSelection', async () => events.push('town'));
        replace('applyLabMode', async (mode: WorksheetMode) => events.push('mode:' + mode));
        Storage.prototype.getItem = function readFixtureStorage(key) {
            if (key === WorksheetContext.LAB_MODE_STORAGE_KEY) {
                events.push('local-mode');
                return null;
            }
            return getItem.call(this, key);
        };
        await WorksheetPersistence.restoration.load();
        equal(events.join(','), 'server:server,recovery', 'local recovery failure must not trigger fallback');

        events.length = 0;
        replace([WorksheetPersistence.restoration, 'applySaved'], async function rejectInstallation() {
            events.push('server-failed');
            throw new Error('cannot install server copy');
        });
        await WorksheetPersistence.restoration.load();
        equal(
            events.join(','),
            'server-failed,local-mode,editors:true,controls:true,mars.exprLab.lastDatetimeState,mars.exprLab.lastAlmanacState,towns,town,town',
            'server installation failure restores local controls and towns without inventing a saved mode');

        events.length = 0;
        replace([labHttp, 'fetch'], async function rejectFetch() {
            throw new Error('offline');
        });
        replace([WorksheetPersistence.restoration, 'restoreEditors'], async function rejectRestoration() {
            events.push('local-failed');
            throw new Error('storage denied');
        });
        await WorksheetPersistence.restoration.load();
        equal(events.join(','), 'local-mode,local-failed', 'local failure terminates fallback after the early mode read');

        const commit = labStateDeferred();
        const commitEntered = labStateDeferred();
        let owned = true;
        replace('commitVisibleBindingInputs', async function commitInputs(guard: WorksheetGuard) {
            check(guard(), 'capture supplies its ownership guard to binding commit');
            commitEntered.resolve();
            await commit.promise;
        });
        const capture = WorksheetWorkspace.mode.captureCurrentModeEditor(() => owned);
        await commitEntered.promise;
        owned = false;
        commit.resolve();
        equal(await capture, false, 'superseded capture stops after binding commit');

        // Exercise the real wrapper: finally must wait for and retain a rejected evaluation.
        events.length = 0;
        let appliedPrecision = 0;
        replace('setRequestedPrecisionBits', function recordPrecision(bits: number) {
            appliedPrecision = bits;
            events.push('precision');
        });
        replace([WorksheetPersistence.persistence, 'savePrecision'], () => events.push('save'));
        replace('setStatus', (text: string) => events.push(text));
        replace('currentMode', () => 'matrix');
        replace('currentGoalSeekSource', () => '');
        replace('updateHistoryButtons', () => events.push('buttons'));
        const pending = labStateDeferred();
        const entered = labStateDeferred();
        replace('evaluateCurrentMode', async function deferEvaluation() {
            events.push('evaluate');
            entered.resolve();
            await pending.promise;
        });
        const error = new Error('precision fixture');
        const completion = WorksheetWorkspace.precision.changeWorksheetPrecision(1).then(() => null, failure => failure);
        await entered.promise;
        equal(events.join(','), 'precision,save,Precision changed,evaluate', 'precision cleanup waits for evaluation');
        pending.reject(error);
        equal(await completion, error, 'finally preserves the original evaluation exception');
        equal(events[events.length - 1], 'buttons', 'precision rejection still refreshes history buttons');

        events.length = 0;
        const selectedPrecision = await WorksheetWorkspace.precision.setWorksheetPrecision(384).then(() => null, failure => failure);
        equal(appliedPrecision, 384, 'slider commits the selected bits rather than taking a relative step');
        equal(selectedPrecision, error, 'slider uses the same guarded evaluation workflow');
        equal(events.join(','), 'precision,save,Precision changed,evaluate,buttons', 'slider persists and evaluates exactly once');

        Reflect.set(WorksheetWorkspace.mode, 'evaluateCurrentMode', saved.get('evaluateCurrentMode')!.callback);
        const evaluated = labStateDeferred();
        const requestedOptions = {skipHistoryUpdate: true};
        replace('evaluateLabMode', function evaluateMode(mode: WorksheetMode, options: {skipHistoryUpdate?: boolean}) {
            equal(mode, 'matrix', 'current-mode wrapper forwards its selected mode to native evaluation');
            equal(options, requestedOptions, 'current-mode wrapper preserves the options object');
            return evaluated.promise;
        });
        let finished = false;
        const delegated = WorksheetWorkspace.mode.evaluateCurrentMode(requestedOptions).then(function markFinished(value) {
            finished = true;
            return value;
        });
        equal(finished, false, 'current-mode wrapper waits for native evaluation');
        evaluated.resolve('ignored result');
        equal(await delegated, undefined, 'current-mode wrapper preserves discarded return semantics');
        replace('evaluateLabMode', async function rejectEvaluation() {
            throw error;
        });
        equal(
            await WorksheetWorkspace.mode.evaluateCurrentMode().then(() => null, failure => failure), error,
            'current-mode wrapper propagates native evaluation errors');
    } finally {
        Storage.prototype.getItem = getItem;
        for (const {owner, key, callback} of saved.values()) {
            Reflect.set(owner, key, callback);
        }
    }
    equal(WorksheetWorkspace.mode.currentMode(), originalMode, 'flow fixtures leave the selected worksheet unchanged');
    await labStateFormsChecks();
    await labStateResourceChecks();
    return true;
};

/** Exercise the real wrappers with controlled browser-resource services and pending forms requests. */
async function labStateFormsChecks() {
    function check(condition: unknown, message: string): void {
        if (!condition)
            throw new Error('State forms: ' + message);
    }
    function equal(actual: unknown, expected: unknown, message: string): void {
        check(Object.is(actual, expected), message);
    }
    const saved = new Map<string, LabWorksheetTestReplacement>();
    function replace(target: string | readonly [object, string], callback: MarsTransportService): void {
        let owner: object;
        let key: string;
        let name: string;
        if (typeof target === 'string') {
            owner = labTestModuleOwner(target);
            key = target;
            name = target;
        } else {
            [owner, key] = target;
            name = 'owned:' + key;
        }
        if (!saved.has(name))
            saved.set(name, {owner, key, callback: Reflect.get(owner, key)});
        Reflect.set(owner, key, callback);
    }
    const events: string[] = [];
    const registry = WorksheetRequests.runtime.getRegistry();
    let reset = 1;
    let historicalText = 'previous';
    let capture: LabWorksheetTestDeferred;
    try {
        // Isolate browser cancellation/request acquisition while retaining the actual clear continuation and wrapper.
        function fixtureCapabilities(): NativeServiceRegistry {
            return {
                ...registry,
                stateClearPrelude: () => events.push('cancel'),
                stateClearOwned() {
                    events.push('request');
                    return WorksheetRequests.runtime.continue(72, {stage: 3, request: {fixture: true}});
                },
                stateClearProjection() {
                    events.push('reset');
                    return reset;
                },
                stateFocusEditor: () => events.push('focus')
            };
        }
        replace('labFlowCapabilities', fixtureCapabilities);
        replace([WorksheetPersistence.history, 'snapshot'], () => ({text: historicalText}));
        replace('pushExpressionHistory', () => events.push('history'));
        replace([WorksheetPersistence.history, 'clearForward'], () => events.push('forward'));
        replace('resetIntegratorBoundsToBlank', () => events.push('blank'));
        replace('applyCalendarState', (_mode: WorksheetMode, _data: unknown, phase: number) => events.push('calendar:' + phase));
        replace('captureCurrentModeEditor', function captureEditor() {
            events.push('capture');
            return capture.promise;
        });
        replace([WorksheetPersistence.history, 'commit'], () => events.push('commit'));
        for (const [name, label] of [
                 ['clearExpressionSource', 'source'], ['hideTargetEntry', 'target'], ['clearResultPane', 'result'],
                 ['saveCurrentModeResultState', 'save-result'],
                 ['updateHistoryButtons', 'buttons']])
            replace(name, () => events.push(label));
        replace('setStatus', (text: string) => events.push(text));

        capture = labStateDeferred();
        const stale = WorksheetWorkspace.mode.clearWorksheetFromEvent();
        equal(
            events.join(','), 'cancel,request,history,forward,reset,blank,capture',
            'clear cancellation/history/reset order');
        capture.resolve(false);
        await stale;
        equal(events.at(-1), 'capture', 'stale capture suppresses every clear completion effect');

        events.length = 0;
        capture = labStateDeferred();
        const failure = new Error('capture failed');
        const rejected = WorksheetWorkspace.mode.clearWorksheetFromEvent().then(() => null, error => error);
        capture.reject(failure);
        equal(await rejected, failure, 'clear capture failure propagates unchanged');
        equal(events.at(-1), 'capture', 'failed capture cannot clear results or focus');

        events.length = 0;
        historicalText = '';
        reset = 2;
        capture = labStateDeferred();
        const success = WorksheetWorkspace.mode.clearWorksheetFromEvent();
        capture.resolve(true);
        await success;
        equal(
            events.join(','),
            'cancel,request,forward,reset,calendar:3,capture,source,target,result,save-result,commit,buttons,Ready,focus',
            'calendar clear retains completion order and skips empty history');

        // Forms services resolve native-backed expression state only outside a DOM handle scope.
        events.length = 0;
        let expression = 'opaque expression';
        let reads = 0;
        let requests = 0;
        let pending: LabWorksheetTestDeferred;
        let payload!: {text: string; expression?: string};
        replace('integratorFormsExpression', function readSource() {
            ++reads;
            return expression;
        });
        replace('requestLabForms', function requestForms(input: {text: string; expression?: string}) {
            ++requests;
            payload = input;
            return pending.promise;
        });
        replace('installIntegratorReferenceMetadata', () => events.push('metadata'));
        replace('renderIntegratorRows', () => events.push('render'));
        const native = labWire.exports();
        const revision = WorksheetWorkspace.integrator.forms.nextRevision();
        equal(await WorksheetWorkspace.integrator.restoreIntegratorBoundsText('x = 0 .. 1', () => false), false, 'initial guard rejects bounds');
        equal(requests, 0, 'initially stale restoration sends no request');
        equal(reads, 0, 'initially stale restoration does not inspect expression state');
        equal(
            native.lab_rows_revision_accept(revision, 1), 1, 'initially stale restoration does not consume a revision');

        pending = labStateDeferred();
        let owned = true;
        const restored = WorksheetWorkspace.integrator.restoreIntegratorBoundsText('', () => owned);
        equal(payload.text, WorksheetContext.DEFAULT_INTEGRATOR_BOUNDS_TEXT, 'empty restoration selects configured bounds');
        equal(payload.expression, expression, 'restoration carries opaque source');
        owned = false;
        pending.resolve({
            get rows() {
                throw new Error('stale rows must not be read');
            }
        });
        equal(await restored, false, 'post-await guard rejects obsolete bounds');
        equal(reads, 1, 'failed post-await guard short-circuits the second expression read');
        equal(events.length, 0, 'stale bounds install neither metadata nor rows');

        pending = labStateDeferred();
        const refreshed = WorksheetWorkspace.integrator.refreshIntegratorForms([], expression);
        WorksheetWorkspace.integrator.forms.nextRevision();
        pending.resolve({
            get rows() {
                throw new Error('superseded rows must not be read');
            }
        });
        equal(await refreshed, null, 'superseded forms refresh returns null, not undefined');
        equal(events.length, 0, 'superseded refresh does not publish metadata');

        pending = labStateDeferred();
        const changed = WorksheetWorkspace.integrator.refreshIntegratorForms([], expression);
        expression = 'newer source';
        pending.resolve({rows: []});
        equal(await changed, null, 'changed source rejects otherwise current revision');

        events.length = 0;
        pending = labStateDeferred();
        const accepted = WorksheetWorkspace.integrator.restoreIntegratorBoundsText(' x = a+b .. a-b ');
        equal(payload.text, ' x = a+b .. a-b ', 'authored bounds are not trimmed or mathematically interpreted');
        pending.resolve({
            get rows() {
                events.push('rows');
                return [];
            }
        });
        equal(await accepted, true, 'current bounds restoration succeeds');
        equal(events.join(','), 'metadata,rows,render', 'metadata precedes one row read and rendering');

        function refreshForms() {
            return WorksheetWorkspace.integrator.refreshIntegratorForms([], expression);
        }
        function restoreBounds() {
            return WorksheetWorkspace.integrator.restoreIntegratorBoundsText('x = 0 .. 1');
        }
        function parseBounds() {
            return parseIntegratorBoundsText('x = 0 .. 1');
        }
        for (const operation of [refreshForms, restoreBounds, parseBounds]) {
            events.length = 0;
            pending = labStateDeferred();
            const error = new Error('forms transport failed');
            const completion = operation().then(() => null, (reason: unknown) => reason);
            pending.reject(error);
            equal(await completion, error, 'forms transport errors propagate unchanged');
            equal(events.length, 0, 'failed transport performs no metadata/DOM projection');
        }
        for (const value of [null, undefined, []]) {
            pending = labStateDeferred();
            const parsed = parseIntegratorBoundsText(null);
            equal(payload.text, '', 'parse-only empty input has no restoration default');
            check(!Object.hasOwn(payload, 'expression'), 'parse-only request does not acquire an expression');
            pending.resolve({rows: value});
            equal(await parsed, value, 'rows result retains null/undefined/array identity');
        }

        events.length = 0;
        pending = labStateDeferred();
        const metadataError = new Error('metadata projection failed');
        replace('installIntegratorReferenceMetadata', function rejectMetadata() {
            throw metadataError;
        });
        const broken = WorksheetWorkspace.integrator.restoreIntegratorBoundsText('x = 0 .. 1').then(() => null, error => error);
        pending.resolve({
            get rows() {
                events.push('rows');
                return [];
            }
        });
        equal(await broken, metadataError, 'metadata failure propagates unchanged');
        equal(events.length, 0, 'metadata failure prevents row read and rendering');
    } finally {
        for (const {owner, key, callback} of saved.values()) {
            Reflect.set(owner, key, callback);
        }
    }
}

/** @brief Check lazy record readers and callbacks that resume the registered runtime after browser work. */
async function labStateCallbackChecks(): Promise<void> {
    function equal(actual: unknown, expected: unknown, message: string): void {
        if (!Object.is(actual, expected))
            throw new Error('State callbacks: ' + message);
    }
    const restoration = WorksheetPersistence.restoration;
    const runtime = WorksheetPersistence.stateRuntime;
    const calendar = WorksheetLocations.calendarActions;
    const savedEditors = restoration.restoreEditors;
    const savedControls = restoration.restoreControls;
    const savedRefresh = calendar.refreshDatetimeJurisdictionLocation;
    const savedSync = runtime.sync;
    const savedTimeout = window.setTimeout;
    const readers: WorksheetStateReader[] = [];
    let reads = 0;
    let value: unknown = 'initial';
    const record: WorksheetStateRecord = {};
    Object.defineProperty(record, 'fixture', {
        enumerable: true,
        get() {
            ++reads;
            return value;
        }
    });
    function retainReader(read: WorksheetStateReader): Promise<void> {
        readers.push(read);
        return Promise.resolve();
    }
    let finishRefresh!: () => void;
    const pending = new Promise<void>(resolve => {
        finishRefresh = resolve;
    });
    const continuations: {kind: number; frame: {save?: WorksheetSavePlan}}[] = [];
    function resumed(this: WorksheetStateRuntimePort, kind: number, frame: {save?: WorksheetSavePlan}): void {
        equal(this, runtime, 'callbacks preserve the registered runtime receiver');
        continuations.push({kind, frame});
    }
    let timerCallback: (() => void) | undefined;
    const save: WorksheetSavePlan = {index: 2147483647, delay: 27, token: 41, patch: {expression: 'queued'}};
    try {
        Reflect.set(restoration, 'restoreEditors', retainReader);
        Reflect.set(restoration, 'restoreControls', retainReader);
        const {stateServerEditors, stateServerControls, stateRead} = WorksheetPersistence.restorationCapabilities;
        await stateServerEditors(record);
        await stateServerControls(record);
        equal(reads, 0, 'registering server readers does not inspect record fields');
        equal(readers.length, 2, 'editor and control restoration each receive a reader');
        for (const read of readers) {
            value = {reader: reads};
            equal(stateRead(read, 'fixture'), value, 'detached reader observes the current field without copying');
            value = 'changed';
            equal(stateRead(read, 'fixture'), value, 'reader does not cache a prior field value');
            equal(stateRead(read, 'absent'), undefined, 'missing record fields remain undefined');
        }
        equal(reads, 4, 'only explicit field reads invoke the record getter');

        Reflect.set(calendar, 'refreshDatetimeJurisdictionLocation', function refresh() { return pending; });
        Reflect.set(runtime, 'sync', function premature() { throw new Error('Refresh resumed before completion'); });
        const refresh = WorksheetPersistence.workspaceCapabilities.stateRefreshLocation;
        equal(refresh(), undefined, 'location refresh starts without returning a continuation promise');
        await Promise.resolve();
        equal(continuations.length, 0, 'unresolved location refresh does not resume native projection');
        Reflect.set(runtime, 'sync', resumed);
        finishRefresh();
        await Promise.resolve();
        equal(continuations.length, 1, 'location refresh resumes once after settlement');
        equal(continuations[0].kind, 13, 'refresh completion selects the native projection operation');
        equal(Object.keys(continuations[0].frame).length, 0, 'refresh completion supplies an empty projection frame');

        Reflect.set(window, 'setTimeout', function captureTimer(callback: () => void, delay: number): number {
            equal(delay, save.delay, 'application save callback retains the native delay');
            timerCallback = callback;
            return 2147483647;
        });
        WorksheetPersistence.persistence.scheduler.schedule(save);
        Reflect.set(window, 'setTimeout', savedTimeout);
        equal(continuations.length, 1, 'save registration does not publish before its timer');
        if (!timerCallback) throw new Error('State callbacks: missing save timer');
        timerCallback();
        equal(continuations.length, 2, 'save timer resumes the runtime once');
        equal(continuations[1].kind, 1, 'save callback selects native publication');
        equal(continuations[1].frame.save, save, 'application callback retains the exact native save plan');
    } finally {
        Reflect.set(restoration, 'restoreEditors', savedEditors);
        Reflect.set(restoration, 'restoreControls', savedControls);
        Reflect.set(calendar, 'refreshDatetimeJurisdictionLocation', savedRefresh);
        Reflect.set(runtime, 'sync', savedSync);
        Reflect.set(window, 'setTimeout', savedTimeout);
        WorksheetPersistence.persistence.scheduler.forget(save.index);
    }
}

/** Resource adapters retain callback ownership without moving native history policy into the browser. */
async function labStateResourceChecks(): Promise<void> {
    function equal(actual: unknown, expected: unknown, message: string): void {
        if (!Object.is(actual, expected))
            throw new Error('State resources: ' + message);
    }
    await labStateCallbackChecks();
    for (const name of ['saveLabState', 'loadLastState', 'restoreHistoryState', 'WorksheetHistoryCursor'])
        equal(Object.hasOwn(window, name), false, 'state API has no global alias: ' + name);

    const timers = new Map<number, () => void>();
    const published: WorksheetSavePlan[] = [];
    let timerId = 0;
    const clock: WorksheetSaveTimerPort = {
        schedule(callback) {
            timers.set(++timerId, callback);
            return timerId;
        },
        cancel(handle) {
            if (handle !== undefined)
                timers.delete(handle);
        }
    };
    const unusedRuntime: WorksheetStateRuntimePort = {
        sync() {
            throw new Error('Unexpected native sync operation');
        },
        restore() {
            throw new Error('Unexpected native restore operation');
        }
    };
    const local = new Map<string, string>();
    const precision = {...WorksheetContext.modePrecisionBits};
    const snapshot: WorksheetSnapshot = {mode: 'expression', text: 'x'};
    const io: WorksheetPersistenceIO = {
        read: key => local.get(key) ?? null,
        write(key, value) {
            local.set(key, value);
        },
        encode: () => new Uint8Array([1, 2, 3]),
        encodeStored(value) {
            equal(value, snapshot, 'local encoding forwards the record unchanged');
            return 'AQID';
        },
        decodeStored(text) {
            if (text !== 'AQID') throw new Error('Invalid storage record');
            return snapshot;
        },
        post: async () => undefined
    };
    const inputs: WorksheetPersistenceInputs = {
        mode: () => 'expression',
        modeIndex: () => 0,
        source: () => 'x',
        precision: () => precision
    };
    const first = new WorksheetPersistence.WorksheetPersistenceController(io, inputs, unusedRuntime,
        new WorksheetPersistence.WorksheetSaveScheduler(clock, save => published.push(save)));
    const second = new WorksheetPersistence.WorksheetPersistenceController(io, inputs, unusedRuntime,
        new WorksheetPersistence.WorksheetSaveScheduler(clock, save => published.push(save)));
    equal(first.scheduler.timers.size, 0, 'new persistence owners start without pending saves');
    equal(first.scheduler === second.scheduler, false, 'persistence owners have distinct schedulers');
    equal(first.scheduler.timers === WorksheetPersistence.persistence.scheduler.timers, false,
          'new owner cannot share the application timer registry');
    const {writeLocal, readLocal} = first;
    writeLocal('fixture', snapshot);
    equal(local.get('fixture.protobuf'), 'AQID', 'detached write uses the injected codec and storage');
    equal(readLocal('fixture'), snapshot, 'detached read preserves the decoded payload identity');
    equal(readLocal('absent'), null, 'missing fallback remains null');
    local.set('fixture.protobuf', '*');
    equal(readLocal('fixture'), null, 'malformed base64 cannot escape the fallback boundary');
    local.set('fixture.protobuf', 'A'.repeat(65));
    equal(readLocal('fixture'), null, 'native codec rejection remains a recoverable fallback failure');

    // Binary publication stays separate from the text-only local storage seam.
    const binary = new Uint8Array(16387);
    for (let index = 0; index < binary.length; ++index)
        binary[index] = index % 256;
    writeLocal('binary', snapshot);
    equal(readLocal('binary'), snapshot, 'native fallback preserves decoded identity');
    const storageFailure = new Error('storage unavailable');
    io.read = function inaccessibleStorage() { throw storageFailure; };
    equal(readLocal('binary'), null, 'inaccessible local storage is a recoverable fallback failure');
    io.write = function fullStorage() { throw storageFailure; };
    let writeFailure: unknown;
    try {
        writeLocal('binary', snapshot);
    } catch (error) {
        writeFailure = error;
    }
    equal(writeFailure, storageFailure, 'local write errors still propagate to the caller');

    let captured: unknown;
    let keepalive = false;
    io.encode = function capturePublication(value: unknown) {
        captured = value;
        return binary;
    };
    io.post = async function rejectPublication(bytes: Uint8Array<ArrayBuffer>, keep: boolean) {
        equal(bytes, binary, 'publication forwards the encoded bytes unchanged');
        keepalive = keep;
        throw new Error('offline');
    };
    const patch = {precision_bits: precision, text: 'authored'};
    const publish = first.publish;
    publish(patch, {keepalive: true});
    await Promise.resolve();
    const payload = captured as {precision_bits: typeof precision; text: string};
    equal(payload === patch, false, 'publication snapshots the supplied record');
    equal(payload.precision_bits === precision, false, 'publication snapshots the live precision view');
    equal(payload.precision_bits.expression, precision.expression, 'precision snapshot retains its value');
    equal(patch.precision_bits, precision, 'publication leaves the caller record unchanged');
    equal(keepalive, true, 'publication preserves unload-safe delivery');
    const explicitPrecision = {...precision};
    publish({precision_bits: explicitPrecision});
    await Promise.resolve();
    equal((captured as {precision_bits: typeof precision}).precision_bits, explicitPrecision,
          'an explicit precision record retains its identity');
    io.encode = function failedEncoding() { throw storageFailure; };
    let encodeFailure: unknown;
    try {
        publish({text: 'authored'});
    } catch (error) {
        encodeFailure = error;
    }
    equal(encodeFailure, storageFailure, 'synchronous publication encoding errors are not swallowed');

    const firstSave: WorksheetSavePlan = {index: 0, delay: 10};
    const secondSave: WorksheetSavePlan = {index: 0, delay: 20};
    const scheduleFirst = first.scheduler.schedule;
    scheduleFirst(firstSave);
    const firstHandle = timerId;
    second.scheduler.schedule(secondSave);
    const secondHandle = timerId;
    first.dispose();
    equal(timers.has(firstHandle), false, 'disposing an owner releases its own timer');
    equal(timers.has(secondHandle), true, 'disposing an owner preserves another owner timer');
    timers.get(secondHandle)!();
    equal(published[0], secondSave, 'scheduler publishes the native save object unchanged');
    second.dispose();

    const scheduleSecond = second.scheduler.schedule;
    const cancelSecond = second.scheduler.cancel;
    const forgetSecond = second.scheduler.forget;
    scheduleSecond(secondSave);
    const cancelledHandle = timerId;
    cancelSecond(secondSave.index);
    equal(timers.has(cancelledHandle), false, 'detached cancellation releases its timer');
    equal(second.scheduler.timers.size, 0, 'detached cancellation releases its registry entry');
    scheduleSecond(secondSave);
    const completedHandle = timerId;
    timers.delete(completedHandle);
    equal(forgetSecond(secondSave.index), true, 'detached forget retires a completed registry entry');
    equal(forgetSecond(secondSave.index), false, 'forget remains idempotent');

    const calls: string[] = [];
    let count = 2;
    const port: WorksheetHistoryCursorPort = {
        count(index, store) {
            calls.push(`count:${index}:${store}`);
            return count;
        },
        push(index, store, length, text, flags) {
            calls.push(`push:${index}:${store}:${length}:${text}:${flags}`);
            return count;
        },
        pop(index, store) {
            calls.push(`pop:${index}:${store}`);
            return 17;
        },
        stage(value) {
            equal(value, snapshot, 'push stages the original snapshot');
            return 17;
        },
        read(length) {
            equal(length, 17, 'pop forwards native length');
            return snapshot;
        }
    };
    const cursor = new WorksheetPersistence.WorksheetHistoryCursor(3, 1, port);
    const {push, pop} = cursor;
    equal(cursor.length, 2, 'length reads native history');
    count = 4;
    equal(cursor.length, 4, 'length is not cached');
    equal(push(snapshot), 4, 'detached push retains its cursor');
    equal(pop(), snapshot, 'detached pop retains its cursor');
    equal(calls.join(','), 'count:3:1,count:3:1,push:3:1:17:1:0,pop:3:1', 'cursor preserves native ABI');
    count = -1;
    let failure: unknown;
    try {
        push(snapshot);
    } catch (error) {
        failure = error;
    }
    equal(failure instanceof Error && failure.message, 'Could not retain worksheet history', 'native failure propagates');

    const savedMode = WorksheetWorkspace.mode.currentMode;
    const savedContext = WorksheetRequests.labRequests.context;
    const savedMain = WorksheetRequests.labRequests.latestMain;
    let mode: WorksheetMode = 'expression';
    let context = 17;
    let main = 23;
    let owned = true;
    function currentMode(): WorksheetMode { return mode; }
    function currentContext(): number { return context; }
    function currentMain(): number { return main; }
    function currentOwner(): boolean { return owned; }
    try {
        Reflect.set(WorksheetWorkspace.mode, 'currentMode', currentMode);
        Reflect.set(WorksheetRequests.labRequests, 'context', currentContext);
        Reflect.set(WorksheetRequests.labRequests, 'latestMain', currentMain);
        const capture = WorksheetPersistence.editorCapabilities.stateCaptureGuard(currentOwner);
        const selection = WorksheetPersistence.workspaceCapabilities.stateSelectionGuard();
        equal(capture(), true, 'capture guard initially owns its context');
        equal(selection(), true, 'selection guard initially owns its context');
        mode = 'matrix';
        equal(capture(), false, 'capture guard rejects a changed mode');
        mode = 'expression';
        owned = false;
        equal(capture(), false, 'capture guard preserves its supplied ownership callback');
        owned = true;
        ++main;
        equal(selection(), false, 'selection guard rejects a newer main request');
        --main;
        ++context;
        equal(capture(), false, 'capture guard rejects a newer context');
        equal(selection(), false, 'selection guard rejects a newer context');
    } finally {
        Reflect.set(WorksheetWorkspace.mode, 'currentMode', savedMode);
        Reflect.set(WorksheetRequests.labRequests, 'context', savedContext);
        Reflect.set(WorksheetRequests.labRequests, 'latestMain', savedMain);
    }
}
