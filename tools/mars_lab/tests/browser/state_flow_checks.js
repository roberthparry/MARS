/** Native state continuations: isolated frames, recovery boundaries and awaited cleanup. */
window.checkLabStateFlow = async function checkLabStateFlow() {
    const check = (condition, message) => {
        if (!condition)
            throw new Error('State flow: ' + message);
    };
    const equal = (actual, expected, message) => check(Object.is(actual, expected), message);
    const saved = new Map();
    const replace = (name, callback) => {
        if (!saved.has(name))
            saved.set(name, window[name]);
        window[name] = callback;
    };
    const deferred = () => {
        let resolve, reject;
        const promise = new Promise((accept, fail) => {
            resolve = accept;
            reject = fail;
        });
        return {promise, resolve, reject};
    };
    const step = (kind, frame) => labDOM.call('lab_flow_step', kind, frame, {});
    const sync = (kind, frame) => labDOM.call('lab_state_sync', kind, frame);
    const bootstrap = (phase, context) => labDOM.call('lab_bootstrap_workspace', phase, context);
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
    const startupReads = [];
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
    labDOM.services(jurisdictionPlan, {writeValue: (control, value) => control.value = value});
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
    equal(delayed.calls[0].args[1].keepalive, false, 'deferred saves cannot become unload saves');
    const immediate = sync(1, {stage: 1, save: {index: 0, delay: 0, options, patch: {expression: 'immediate'}}});
    equal(keepaliveReads, 1, 'immediate publication reads keepalive at publication time');
    equal(immediate.calls[0].args[1].keepalive, true, 'immediate keepalive is retained');
    const staleSave = sync(1, {save: {index: 0, token: 0, options}});
    equal(staleSave.done, true, 'unowned timer publishes no effects');
    equal(keepaliveReads, 1, 'unowned timer cannot observe publish-time options');
    equal(
        sync(5, {stage: 2, result: null}).calls[0].service, 'stateClearResult', 'missing result clearing stays native');
    equal(
        sync(8, {stage: 3, index: 6}).calls[0].service, 'stateEvaluateExpression',
        'keyboard fallback retains previous almanac behaviour');
    const first = {read: key => key, local: false}, second = {read: key => key, local: true};
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

    const controls = {};
    const schema = step(17, controls);
    controls.fields = labDOM.call(...schema.calls[0].args);
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

    const events = [];
    const getItem = Storage.prototype.getItem;
    const originalMode = currentMode();
    try {
        replace('labFetch', async () => ({labData: async () => ({marker: 'server'})}));
        replace('applySavedState', async data => events.push('server:' + data.marker));
        replace('restoreNewerLocalEditors', async () => {
            events.push('recovery');
            throw new Error('locked local storage');
        });
        replace('restoreWorksheetEditors', async (_read, local) => events.push('editors:' + local));
        replace('restoreWorksheetControls', async (_read, local) => events.push('controls:' + local));
        replace('loadLabLocalState', key => {
            events.push(key);
            return null;
        });
        replace('syncTownSelectors', async () => events.push('towns'));
        replace('restoreTownSelection', async () => events.push('town'));
        replace('applyLabMode', async mode => events.push('mode:' + mode));
        Storage.prototype.getItem = function(key) {
            if (key === LAB_MODE_STORAGE_KEY) {
                events.push('local-mode');
                return null;
            }
            return getItem.call(this, key);
        };
        await loadLastState();
        equal(events.join(','), 'server:server,recovery', 'local recovery failure must not trigger fallback');

        events.length = 0;
        replace('applySavedState', async () => {
            events.push('server-failed');
            throw new Error('cannot install server copy');
        });
        await loadLastState();
        equal(
            events.join(','),
            'server-failed,editors:true,controls:true,mars.exprLab.lastDatetimeState,mars.exprLab.lastAlmanacState,local-mode,towns,town,town',
            'server installation failure restores local controls and towns without inventing a saved mode');

        events.length = 0;
        replace('labFetch', async () => {
            throw new Error('offline');
        });
        replace('restoreWorksheetEditors', async () => {
            events.push('local-failed');
            throw new Error('storage denied');
        });
        await loadLastState();
        equal(events.join(','), 'local-failed', 'local failure terminates fallback without later reads');

        const commit = deferred(), commitEntered = deferred();
        let owned = true;
        replace('commitVisibleBindingInputs', async guard => {
            check(guard(), 'capture supplies its ownership guard to binding commit');
            commitEntered.resolve();
            await commit.promise;
        });
        const capture = captureCurrentModeEditor(() => owned);
        await commitEntered.promise;
        owned = false;
        commit.resolve();
        equal(await capture, false, 'superseded capture stops after binding commit');

        // Exercise the real wrapper: finally must wait for and retain a rejected evaluation.
        events.length = 0;
        replace('setRequestedPrecisionBits', () => events.push('precision'));
        replace('savePrecisionState', () => events.push('save'));
        replace('setStatus', text => events.push(text));
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
        const completion = changeWorksheetPrecision(1).then(() => null, failure => failure);
        await entered.promise;
        equal(events.join(','), 'precision,save,Precision changed,evaluate', 'precision cleanup waits for evaluation');
        pending.reject(error);
        equal(await completion, error, 'finally preserves the original evaluation exception');
        equal(events[events.length - 1], 'buttons', 'precision rejection still refreshes history buttons');

        window.evaluateCurrentMode = saved.get('evaluateCurrentMode');
        const evaluated = deferred(), requestedOptions = {skipHistoryUpdate: true};
        replace('evaluateLabMode', (mode, options) => {
            equal(mode, 'matrix', 'current-mode wrapper forwards its selected mode to native evaluation');
            equal(options, requestedOptions, 'current-mode wrapper preserves the options object');
            return evaluated.promise;
        });
        let finished = false;
        const delegated = evaluateCurrentMode(requestedOptions).then(value => {
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
            await evaluateCurrentMode().then(() => null, failure => failure), error,
            'current-mode wrapper propagates native evaluation errors');
    } finally {
        Storage.prototype.getItem = getItem;
        for (const [name, callback] of saved) window[name] = callback;
    }
    equal(currentMode(), originalMode, 'flow fixtures leave the selected worksheet unchanged');
    await labStateFormsChecks();
    return true;
};

/** Exercise the real wrappers with controlled browser-resource services and pending forms requests. */
async function labStateFormsChecks() {
    const check = (condition, message) => {
        if (!condition)
            throw new Error('State forms: ' + message);
    };
    const equal = (actual, expected, message) => check(Object.is(actual, expected), message);
    const saved = new Map();
    const replace = (name, callback) => {
        if (!saved.has(name))
            saved.set(name, window[name]);
        window[name] = callback;
    };
    const deferred = () => {
        let resolve, reject;
        const promise = new Promise((accept, fail) => {
            resolve = accept;
            reject = fail;
        });
        return {promise, resolve, reject};
    };
    const events = [], registry = labFlowCapabilities();
    let reset = 1, historicalText = 'previous', capture;
    try {
        // Isolate browser cancellation/request acquisition while retaining the actual clear continuation and wrapper.
        replace('labFlowCapabilities', () => ({
                                           ...registry,
                                           stateClearPrelude: () => events.push('cancel'),
                                           stateClearOwned: () => {
                                               events.push('request');
                                               return labFlowContinue(72, {stage: 3, request: {fixture: true}});
                                           },
                                           stateClearProjection: () => {
                                               events.push('reset');
                                               return reset;
                                           },
                                           stateFocusEditor: () => events.push('focus')
                                       }));
        replace('historyStateForMode', () => ({text: historicalText}));
        replace('pushExpressionHistory', () => events.push('history'));
        replace('clearForwardHistory', () => events.push('forward'));
        replace('resetIntegratorBoundsToBlank', () => events.push('blank'));
        replace('applyCalendarState', (_mode, _data, phase) => events.push('calendar:' + phase));
        replace('captureCurrentModeEditor', () => {
            events.push('capture');
            return capture.promise;
        });
        for (const [name, label] of [
                 ['clearExpressionSource', 'source'], ['hideTargetEntry', 'target'], ['clearResultPane', 'result'],
                 ['saveCurrentModeResultState', 'save-result'], ['commitModeState', 'commit'],
                 ['updateHistoryButtons', 'buttons']])
            replace(name, () => events.push(label));
        replace('setStatus', text => events.push(text));

        capture = deferred();
        const stale = clearWorksheetFromEvent();
        equal(
            events.join(','), 'cancel,request,history,forward,reset,blank,capture',
            'clear cancellation/history/reset order');
        capture.resolve(false);
        await stale;
        equal(events.at(-1), 'capture', 'stale capture suppresses every clear completion effect');

        events.length = 0;
        capture = deferred();
        const failure = new Error('capture failed');
        const rejected = clearWorksheetFromEvent().then(() => null, error => error);
        capture.reject(failure);
        equal(await rejected, failure, 'clear capture failure propagates unchanged');
        equal(events.at(-1), 'capture', 'failed capture cannot clear results or focus');

        events.length = 0;
        historicalText = '';
        reset = 2;
        capture = deferred();
        const success = clearWorksheetFromEvent();
        capture.resolve(true);
        await success;
        equal(
            events.join(','),
            'cancel,request,forward,reset,calendar:3,capture,source,target,result,save-result,commit,buttons,Ready,focus',
            'calendar clear retains completion order and skips empty history');

        // Forms services resolve native-backed expression state only outside a DOM handle scope.
        events.length = 0;
        let expression = 'opaque expression', reads = 0, requests = 0, pending, payload;
        replace('integratorFormsExpression', () => {
            ++reads;
            return expression;
        });
        replace('requestLabForms', input => {
            ++requests;
            payload = input;
            return pending.promise;
        });
        replace('installIntegratorReferenceMetadata', () => events.push('metadata'));
        replace('renderIntegratorRows', () => events.push('render'));
        const native = labWire.exports(), revision = nextIntegratorFormsRevision();
        equal(await restoreIntegratorBoundsText('x = 0 .. 1', () => false), false, 'initial guard rejects bounds');
        equal(requests, 0, 'initially stale restoration sends no request');
        equal(reads, 0, 'initially stale restoration does not inspect expression state');
        equal(
            native.lab_rows_revision_accept(revision, 1), 1, 'initially stale restoration does not consume a revision');

        pending = deferred();
        let owned = true;
        const restored = restoreIntegratorBoundsText('', () => owned);
        equal(payload.text, DEFAULT_INTEGRATOR_BOUNDS_TEXT, 'empty restoration selects configured bounds');
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
        const refreshed = refreshIntegratorForms([], expression);
        nextIntegratorFormsRevision();
        pending.resolve({
            get rows() {
                throw new Error('superseded rows must not be read');
            }
        });
        equal(await refreshed, null, 'superseded forms refresh returns null, not undefined');
        equal(events.length, 0, 'superseded refresh does not publish metadata');

        pending = deferred();
        const changed = refreshIntegratorForms([], expression);
        expression = 'newer source';
        pending.resolve({rows: []});
        equal(await changed, null, 'changed source rejects otherwise current revision');

        events.length = 0;
        pending = deferred();
        const accepted = restoreIntegratorBoundsText(' x = a+b .. a-b ');
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
                 of [() => refreshIntegratorForms([], expression), () => restoreIntegratorBoundsText('x = 0 .. 1'),
                    () => parseIntegratorBoundsText('x = 0 .. 1')]) {
            events.length = 0;
            pending = deferred();
            const error = new Error('forms transport failed');
            const completion = operation().then(() => null, reason => reason);
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
        const broken = restoreIntegratorBoundsText('x = 0 .. 1').then(() => null, error => error);
        pending.resolve({
            get rows() {
                events.push('rows');
                return [];
            }
        });
        equal(await broken, metadataError, 'metadata failure propagates unchanged');
        equal(events.length, 0, 'metadata failure prevents row read and rendering');
    } finally {
        for (const [name, callback] of saved) window[name] = callback;
    }
}
