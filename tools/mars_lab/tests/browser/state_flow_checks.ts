/**
 * @file state_flow_checks.ts
 * @brief Worksheet state continuation and resource-ownership regressions.
 *
 * Checks isolated continuation frames, recovery boundaries, awaited cleanup and namespace-owned persistence and history resources.
 */

/** Native state continuations: isolated frames, recovery boundaries and awaited cleanup. */
window.checkLabStateFlow = async function checkLabStateFlow() {
    const check: LabTestCheck = (condition, message) => {
        if (!condition)
            throw new Error('State flow: ' + message);
    };
    const equal = (actual: unknown, expected: unknown, message: string) => check(Object.is(actual, expected), message);
    const saved = new Map<string, unknown>();
    const targets = new Map<string, readonly [object, string]>();
    const replace = (target: string | readonly [object, string], callback: (...args: never[]) => unknown) => {
        const [owner, key] = typeof target === 'string' ? [labTestModuleOwner(target), target] as const : target;
        const name = typeof target === 'string' ? target : 'owned:' + key;
        if (!saved.has(name)) {
            targets.set(name, [owner, key]);
            saved.set(name, Reflect.get(owner, key));
        }
        Reflect.set(owner, key, callback);
    };
    const deferred = (): LabWorksheetTestDeferred => {
        let resolve!: (value?: unknown) => void, reject!: (reason?: unknown) => void;
        const promise = new Promise((accept, fail) => {
            resolve = accept;
            reject = fail;
        });
        return {promise, resolve, reject};
    };
    const step = (kind: number, frame: object) => labDOM.call<LabWorksheetTestPlan>('lab_flow_step', kind, frame, {});
    const sync = (kind: number, frame: object) => labDOM.call<LabWorksheetTestPlan>('lab_state_sync', kind, frame);
    const bootstrap = (phase: number, context: unknown) => labDOM.call<LabWorksheetTestPlan>('lab_bootstrap_workspace', phase, context);
    const unread = () => {
        throw new Error('inactive bootstrap getter was read');
    };
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
    const first: LabWorksheetTestEditorFrame = {read: (key: string) => key, local: false}, second: LabWorksheetTestEditorFrame = {read: (key: string) => key, local: true};
    const firstPlan = step(16, first), secondPlan = step(16, second);
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
        replace([labHttp, 'fetch'], async () => ({labData: async () => ({marker: 'server'})}));
        replace([WorksheetPersistence.restoration, 'applySaved'], async (data: {marker: string}) => events.push('server:' + data.marker));
        replace([WorksheetPersistence.restoration, 'recoverLocalEditors'], async () => {
            events.push('recovery');
            throw new Error('locked local storage');
        });
        replace([WorksheetPersistence.restoration, 'restoreEditors'], async (_read: (key: string) => unknown, local: boolean) => events.push('editors:' + local));
        replace([WorksheetPersistence.restoration, 'restoreControls'], async (_read: (key: string) => unknown, local: boolean) => events.push('controls:' + local));
        replace([WorksheetPersistence.persistence, 'readLocal'], (key: string) => {
            events.push(key);
            return null;
        });
        replace('syncTownSelectors', async () => events.push('towns'));
        replace('restoreTownSelection', async () => events.push('town'));
        replace('applyLabMode', async (mode: WorksheetMode) => events.push('mode:' + mode));
        Storage.prototype.getItem = function(key) {
            if (key === WorksheetContext.LAB_MODE_STORAGE_KEY) {
                events.push('local-mode');
                return null;
            }
            return getItem.call(this, key);
        };
        await WorksheetPersistence.restoration.load();
        equal(events.join(','), 'server:server,recovery', 'local recovery failure must not trigger fallback');

        events.length = 0;
        replace([WorksheetPersistence.restoration, 'applySaved'], async () => {
            events.push('server-failed');
            throw new Error('cannot install server copy');
        });
        await WorksheetPersistence.restoration.load();
        equal(
            events.join(','),
            'server-failed,editors:true,controls:true,mars.exprLab.lastDatetimeState,mars.exprLab.lastAlmanacState,local-mode,towns,town,town',
            'server installation failure restores local controls and towns without inventing a saved mode');

        events.length = 0;
        replace([labHttp, 'fetch'], async () => {
            throw new Error('offline');
        });
        replace([WorksheetPersistence.restoration, 'restoreEditors'], async () => {
            events.push('local-failed');
            throw new Error('storage denied');
        });
        await WorksheetPersistence.restoration.load();
        equal(events.join(','), 'local-failed', 'local failure terminates fallback without later reads');

        const commit = deferred(), commitEntered = deferred();
        let owned = true;
        replace('commitVisibleBindingInputs', async (guard: WorksheetGuard) => {
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
        replace('setRequestedPrecisionBits', (bits: number) => {
            appliedPrecision = bits;
            events.push('precision');
        });
        replace([WorksheetPersistence.persistence, 'savePrecision'], () => events.push('save'));
        replace('setStatus', (text: string) => events.push(text));
        replace('currentMode', () => 'matrix');
        replace('currentGoalSeekSource', () => '');
        replace('updateHistoryButtons', () => events.push('buttons'));
        const pending = deferred(), entered = deferred();
        replace('evaluateCurrentMode', async () => {
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

        Reflect.set(WorksheetWorkspace.mode, 'evaluateCurrentMode', saved.get('evaluateCurrentMode'));
        const evaluated = deferred(), requestedOptions = {skipHistoryUpdate: true};
        replace('evaluateLabMode', (mode: WorksheetMode, options: Record<string, unknown>) => {
            equal(mode, 'matrix', 'current-mode wrapper forwards its selected mode to native evaluation');
            equal(options, requestedOptions, 'current-mode wrapper preserves the options object');
            return evaluated.promise;
        });
        let finished = false;
        const delegated = WorksheetWorkspace.mode.evaluateCurrentMode(requestedOptions).then(value => {
            finished = true;
            return value;
        });
        equal(finished, false, 'current-mode wrapper waits for native evaluation');
        evaluated.resolve('ignored result');
        equal(await delegated, undefined, 'current-mode wrapper preserves discarded return semantics');
        replace('evaluateLabMode', async () => {
            throw error;
        });
        equal(
            await WorksheetWorkspace.mode.evaluateCurrentMode().then(() => null, failure => failure), error,
            'current-mode wrapper propagates native evaluation errors');
    } finally {
        Storage.prototype.getItem = getItem;
        for (const [name, callback] of saved) {
            const [owner, key] = targets.get(name)!;
            Reflect.set(owner, key, callback);
        }
    }
    equal(WorksheetWorkspace.mode.currentMode(), originalMode, 'flow fixtures leave the selected worksheet unchanged');
    await labStateFormsChecks();
    labStateResourceChecks();
    return true;
};

/** Exercise the real wrappers with controlled browser-resource services and pending forms requests. */
async function labStateFormsChecks() {
    const check = (condition: unknown, message: string) => {
        if (!condition)
            throw new Error('State forms: ' + message);
    };
    const equal = (actual: unknown, expected: unknown, message: string) => check(Object.is(actual, expected), message);
    const saved = new Map<string, unknown>();
    const targets = new Map<string, readonly [object, string]>();
    const replace = (target: string | readonly [object, string], callback: (...args: never[]) => unknown) => {
        const [owner, key] = typeof target === 'string' ? [labTestModuleOwner(target), target] as const : target;
        const name = typeof target === 'string' ? target : 'owned:' + key;
        if (!saved.has(name)) {
            targets.set(name, [owner, key]);
            saved.set(name, Reflect.get(owner, key));
        }
        Reflect.set(owner, key, callback);
    };
    const deferred = (): LabWorksheetTestDeferred => {
        let resolve!: (value?: unknown) => void, reject!: (reason?: unknown) => void;
        const promise = new Promise((accept, fail) => {
            resolve = accept;
            reject = fail;
        });
        return {promise, resolve, reject};
    };
    const events: string[] = [], registry = WorksheetRequests.runtime.getRegistry();
    let reset = 1, historicalText = 'previous';
    let capture: LabWorksheetTestDeferred;
    try {
        // Isolate browser cancellation/request acquisition while retaining the actual clear continuation and wrapper.
        replace('labFlowCapabilities', () => ({
                                           ...registry,
                                           stateClearPrelude: () => events.push('cancel'),
                                           stateClearOwned: () => {
                                               events.push('request');
                                               return WorksheetRequests.runtime.continue(72, {stage: 3, request: {fixture: true}});
                                           },
                                           stateClearProjection: () => {
                                               events.push('reset');
                                               return reset;
                                           },
                                           stateFocusEditor: () => events.push('focus')
                                       }));
        replace([WorksheetPersistence.history, 'snapshot'], () => ({text: historicalText}));
        replace('pushExpressionHistory', () => events.push('history'));
        replace([WorksheetPersistence.history, 'clearForward'], () => events.push('forward'));
        replace('resetIntegratorBoundsToBlank', () => events.push('blank'));
        replace('applyCalendarState', (_mode: WorksheetMode, _data: unknown, phase: number) => events.push('calendar:' + phase));
        replace('captureCurrentModeEditor', () => {
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

        capture = deferred();
        const stale = WorksheetWorkspace.mode.clearWorksheetFromEvent();
        equal(
            events.join(','), 'cancel,request,history,forward,reset,blank,capture',
            'clear cancellation/history/reset order');
        capture.resolve(false);
        await stale;
        equal(events.at(-1), 'capture', 'stale capture suppresses every clear completion effect');

        events.length = 0;
        capture = deferred();
        const failure = new Error('capture failed');
        const rejected = WorksheetWorkspace.mode.clearWorksheetFromEvent().then(() => null, error => error);
        capture.reject(failure);
        equal(await rejected, failure, 'clear capture failure propagates unchanged');
        equal(events.at(-1), 'capture', 'failed capture cannot clear results or focus');

        events.length = 0;
        historicalText = '';
        reset = 2;
        capture = deferred();
        const success = WorksheetWorkspace.mode.clearWorksheetFromEvent();
        capture.resolve(true);
        await success;
        equal(
            events.join(','),
            'cancel,request,forward,reset,calendar:3,capture,source,target,result,save-result,commit,buttons,Ready,focus',
            'calendar clear retains completion order and skips empty history');

        // Forms services resolve native-backed expression state only outside a DOM handle scope.
        events.length = 0;
        let expression = 'opaque expression', reads = 0, requests = 0;
        let pending: LabWorksheetTestDeferred, payload!: {text: string; expression?: string};
        replace('integratorFormsExpression', () => {
            ++reads;
            return expression;
        });
        replace('requestLabForms', (input: {text: string; expression?: string}) => {
            ++requests;
            payload = input;
            return pending.promise;
        });
        replace('installIntegratorReferenceMetadata', () => events.push('metadata'));
        replace('renderIntegratorRows', () => events.push('render'));
        const native = labWire.exports(), revision = WorksheetWorkspace.integrator.forms.nextRevision();
        equal(await WorksheetWorkspace.integrator.restoreIntegratorBoundsText('x = 0 .. 1', () => false), false, 'initial guard rejects bounds');
        equal(requests, 0, 'initially stale restoration sends no request');
        equal(reads, 0, 'initially stale restoration does not inspect expression state');
        equal(
            native.lab_rows_revision_accept(revision, 1), 1, 'initially stale restoration does not consume a revision');

        pending = deferred();
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

        pending = deferred();
        const refreshed = WorksheetWorkspace.integrator.refreshIntegratorForms([], expression);
        WorksheetWorkspace.integrator.forms.nextRevision();
        pending.resolve({
            get rows() {
                throw new Error('superseded rows must not be read');
            }
        });
        equal(await refreshed, null, 'superseded forms refresh returns null, not undefined');
        equal(events.length, 0, 'superseded refresh does not publish metadata');

        pending = deferred();
        const changed = WorksheetWorkspace.integrator.refreshIntegratorForms([], expression);
        expression = 'newer source';
        pending.resolve({rows: []});
        equal(await changed, null, 'changed source rejects otherwise current revision');

        events.length = 0;
        pending = deferred();
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

        for (const operation
                 of [() => WorksheetWorkspace.integrator.refreshIntegratorForms([], expression), () => WorksheetWorkspace.integrator.restoreIntegratorBoundsText('x = 0 .. 1'),
                    () => parseIntegratorBoundsText('x = 0 .. 1')]) {
            events.length = 0;
            pending = deferred();
            const error = new Error('forms transport failed');
            const completion = operation().then(() => null, (reason: unknown) => reason);
            pending.reject(error);
            equal(await completion, error, 'forms transport errors propagate unchanged');
            equal(events.length, 0, 'failed transport performs no metadata/DOM projection');
        }
        for (const value of [null, undefined, []]) {
            pending = deferred();
            const parsed = parseIntegratorBoundsText(null);
            equal(payload.text, '', 'parse-only empty input has no restoration default');
            check(!Object.hasOwn(payload, 'expression'), 'parse-only request does not acquire an expression');
            pending.resolve({rows: value});
            equal(await parsed, value, 'rows result retains null/undefined/array identity');
        }

        events.length = 0;
        pending = deferred();
        const metadataError = new Error('metadata projection failed');
        replace('installIntegratorReferenceMetadata', () => {
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
        for (const [name, callback] of saved) {
            const [owner, key] = targets.get(name)!;
            Reflect.set(owner, key, callback);
        }
    }
}

/** Resource adapters retain callback ownership without moving native history policy into the browser. */
function labStateResourceChecks(): void {
    const equal = (actual: unknown, expected: unknown, message: string): void => {
        if (!Object.is(actual, expected))
            throw new Error('State resources: ' + message);
    };
    for (const name of ['saveLabState', 'loadLastState', 'restoreHistoryState', 'WorksheetHistoryCursor'])
        equal(Object.hasOwn(window, name), false, 'state API has no global alias: ' + name);

    const timers = new Map<number, () => void>(), published: WorksheetSavePlan[] = [];
    let timerId = 0;
    const clock: WorksheetSaveTimerPort = {
        schedule: callback => { timers.set(++timerId, callback); return timerId; },
        cancel: handle => { if (handle !== undefined) timers.delete(handle); }
    };
    const unusedRuntime: WorksheetStateRuntimePort = {
        sync: () => { throw new Error('Unexpected native sync operation'); },
        restore: () => { throw new Error('Unexpected native restore operation'); }
    };
    const local = new Map<string, string>(), precision = {...WorksheetContext.modePrecisionBits};
    const snapshot: WorksheetSnapshot = {mode: 'expression', text: 'x'};
    const io: WorksheetPersistenceIO = {
        read: key => local.get(key) ?? null,
        write: (key, value) => { local.set(key, value); },
        encode: () => new Uint8Array([1, 2, 3]),
        decode: () => snapshot,
        base64Limit: () => 64,
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

    const firstSave: WorksheetSavePlan = {index: 0, delay: 10};
    const secondSave: WorksheetSavePlan = {index: 0, delay: 20};
    first.scheduler.schedule(firstSave);
    const firstHandle = timerId;
    second.scheduler.schedule(secondSave);
    const secondHandle = timerId;
    first.dispose();
    equal(timers.has(firstHandle), false, 'disposing an owner releases its own timer');
    equal(timers.has(secondHandle), true, 'disposing an owner preserves another owner timer');
    timers.get(secondHandle)!();
    equal(published[0], secondSave, 'scheduler publishes the native save object unchanged');
    second.dispose();

    const calls: string[] = [];
    let count = 2;
    const port: Pick<WorksheetHistoryPort, 'count' | 'push' | 'pop' | 'stage' | 'read'> = {
        count: (index, store) => { calls.push(`count:${index}:${store}`); return count; },
        push: (index, store, length, text, flags) => {
            calls.push(`push:${index}:${store}:${length}:${text}:${flags}`);
            return count;
        },
        pop: (index, store) => { calls.push(`pop:${index}:${store}`); return 17; },
        stage: value => { equal(value, snapshot, 'push stages the original snapshot'); return 17; },
        read: length => { equal(length, 17, 'pop forwards native length'); return snapshot; }
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
    try { push(snapshot); } catch (error) { failure = error; }
    equal(failure instanceof Error && failure.message, 'Could not retain worksheet history', 'native failure propagates');
}
