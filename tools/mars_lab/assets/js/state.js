/**
 * Worksheet storage adapters, asynchronous restoration and history coordination.
 * C owns saved-field normalisation, recovery records and guarded DOM projection.
 * Definition-only client script; app.js loads it before shared worksheet state.
 */

// localStorage accepts text only; base64 is a browser adapter around the C codec.
function saveLabLocalState(key, state) {
    const bytes = new Uint8Array(labWire.encode(state));
    const chunks = [];
    for (let offset = 0; offset < bytes.length; offset += 8192)
        chunks.push(String.fromCharCode(...bytes.subarray(offset, offset + 8192)));
    localStorage.setItem(`${key}.protobuf`, btoa(chunks.join('')));
}

function loadLabLocalState(key) {
    try {
        const text = localStorage.getItem(`${key}.protobuf`);
        if (!text || text.length > 5592408)
            return null;
        const bytes = Uint8Array.from(atob(text), character => character.charCodeAt(0));
        return labWire.decode(bytes.buffer);
    } catch (_) {
        // One damaged or inaccessible fallback must not prevent other state restoration.
        return null;
    }
}

function validPrecisionBits(bits, fallback) {
    return labDOM.call('lab_storage_precision', [bits, fallback]);
}

function validIntegratorIntervalCap(value) {
    return labDOM.call('lab_storage_intervals', [value], labConfig);
}

function validMatrixOperation(value) {
    return labDOM.call('lab_storage_operation', [value]);
}

function validLabMode(value) {
    return WORKSPACE_MODE_NAMES[workspaceModeId(value)];
}

async function restoreWorksheetEditors(read, local = false) {
    return labFlowContinue(16, {read, local});
}

async function restoreWorksheetControls(read, local = false) {
    return labFlowContinue(17, {read, local});
}

async function applySavedState(data) {
    return labFlowContinue(18, {data});
}

async function restoreNewerLocalEditors(data) {
    return labFlowContinue(19, {data});
}

async function loadLastState() {
    return labFlowContinue(20, {});
}

function saveLabState(patch, options = {}) {
    const payload = {...patch};
    // Snapshot only our trusted native getter view; the codec still rejects arbitrary accessors.
    if (payload.precision_bits === modePrecisionBits)
        payload.precision_bits = {...modePrecisionBits};
    labFetch('/state', {method: 'POST', body: labWire.encode(payload), keepalive: !!options.keepalive})
        .catch(
            () => {
                // Persistence is helpful, not essential.
            });
}

function savePrecisionState() {
    saveLabState({precision_bits: modePrecisionBits});
}

function saveLastLabMode(mode = currentMode()) {
    labStateSync(3, {mode});
}

const worksheetSaveTimers = new Map();
function worksheetSaveSchema(mode) {
    return labDOM.call('lab_persist_schema', mode);
}

// The host captures browser values and performs I/O; C supplies the schema and save ownership.
function saveWorksheetState(mode, text = currentExpressionText() || expr.value || '', options = {}) {
    labStateSync(0, {index: workspaceModeId(mode), text, options});
}

function saveCalendarState(mode) {
    labStateSync(2, {mode});
}

function saveLastDatetimeState() {
    saveCalendarState('datetime');
}

function saveLastAlmanacState() {
    saveCalendarState('almanac');
}

function modeHistoryStack(store, mode = currentMode()) {
    const index = workspaceModeId(mode);
    // Compatibility with DOM event wiring: every operation reaches the native owner.
    return {
        get length() {
            return labWire.exports().lab_workspace_history_count(index, store);
        },
        push(snapshot) {
            const length = workspaceStageSnapshot(snapshot);
            const count = labWire.exports().lab_workspace_history_push(index, store, length, !!snapshot?.text, 0);
            if (count < 0)
                throw new Error('Could not retain worksheet history');
            return count;
        },
        pop() {
            return workspaceReadSnapshot(labWire.exports().lab_workspace_history_pop(index, store));
        }
    };
}

// Event handlers can use this atomic operation instead of paired stack pop/push calls.
function navigateWorksheetHistory(direction) {
    const native = labWire.exports();
    const current = historyStateForMode();
    const length = workspaceStageSnapshot(current);
    const restored = native.lab_workspace_navigate(native.lab_workspace_mode(), direction, length, !!current.text);
    return workspaceReadSnapshot(restored);
}

function currentHistoryLength() {
    return modeHistoryStack(expressionHistory).length;
}

function currentForwardHistoryLength() {
    return modeHistoryStack(forwardHistory).length;
}

function historyStateForMode(mode = currentMode(), textOverride = null) {
    return labStateSync(9, {index: workspaceModeId(mode), textOverride});
}

function historyStatesEqual(left, right) {
    const leftLength = workspaceStageSnapshot(left, 0);
    const rightLength = workspaceStageSnapshot(right, 1);
    return !!labWire.exports().lab_workspace_equal(leftLength, rightLength);
}

function previousModeStateForHistory(nextState) {
    const mode = workspaceModeId(nextState && nextState.mode || currentMode());
    const length = workspaceStageSnapshot(nextState);
    return workspaceReadSnapshot(labWire.exports().lab_workspace_previous(mode, length));
}

function commitModeState(mode = currentMode(), textOverride = null) {
    const index = workspaceModeId(mode);
    const length = workspaceStageSnapshot(historyStateForMode(mode, textOverride));
    if (!labWire.exports().lab_workspace_commit(index, length))
        throw new Error('Could not commit worksheet state');
}

async function restoreHistoryState(state, isCurrent = () => true) {
    return labFlowContinue(21, {state, isCurrent});
}

function clearForwardHistory(mode = currentMode()) {
    labWire.exports().lab_workspace_history_clear(workspaceModeId(mode), 1);
}

async function navigateHistoryFromEvent(direction) {
    return labRequests.runUI('evaluate', currentMode(), request => labFlowContinue(22, {direction, request}));
}

// Resolve browser capabilities lazily, after definition-only scripts have loaded.
function labStateFlowServices() {
    return {
        stateClearPrelude: () => {
            clearTimeout(expressionBindingRefreshTimer);
            labRequests.cancel('evaluate');
            labRequests.cancel('bindings');
            labRequests.cancel('function');
        },
        stateClearOwned: () =>
            labRequests.runUI('evaluate', currentMode(), request => labFlowContinue(72, {stage: 3, request})),
        stateClearProjection: () => labDOM.call('lab_events_clear', workspaceModeId(currentMode()), labConfig),
        stateBlankBounds: () => resetIntegratorBoundsToBlank(),
        stateResetCalendar: () => applyCalendarState(currentMode(), {}, 3),
        stateFormsRequest: payload => requestLabForms(payload),
        stateFormsExpression: () => integratorFormsExpression(),
        stateFormsMetadata: (expression, result) => installIntegratorReferenceMetadata(expression, result),
        // Box the property once: null and undefined survive native handle scopes distinctly.
        stateFormsRows: result => ({value: result.rows}),
        stateFormsRender: rows => renderIntegratorRows(rows.value),
        stateRead: (read, key) => read(key),
        stateNow: () => Date.now(),
        statePrepare: text => prepareLabEditor(text),
        stateCanonical: text => expressionWithSortedConstants(text),
        stateEditor: (index, text) => modeEditorText[WORKSPACE_MODE_NAMES[index]] = text,
        stateExpression: (text, timestamp) => {
            lastExpressionUpdatedAt = timestamp;
            setExpressionEditor(text);
        },
        stateTimestamp: timestamp => lastExpressionUpdatedAt = timestamp,
        stateSetExpression: text => setExpressionEditor(text),
        stateIntegratorSource: text => expr.dataset.savedIntegratorExpression = text,
        stateControl: (field, value, local) => labDOM.call('lab_storage_control', field, [value], local, labConfig),
        stateBounds: (...args) => restoreIntegratorBoundsText(...args),
        stateServerEditors: data => restoreWorksheetEditors(key => data[key]),
        stateServerControls: data => restoreWorksheetControls(key => data[key]),
        stateCalendar: (...args) => applyCalendarState(...args),
        stateSyncTowns: () => syncTownSelectors({selectDefault: false}),
        stateTown: (index, town) => {
            const [select, jurisdiction, latitude, longitude] = [
                [datetimeTown, datetimeJurisdiction, datetimeLatitude, datetimeLongitude],
                [almanacTown, almanacJurisdiction, almanacLatitude, almanacLongitude]
            ][index];
            return restoreTownSelection(
                select, jurisdiction && jurisdiction.value, town, latitude && latitude.value,
                longitude && longitude.value);
        },
        stateMode: value => validLabMode(value),
        statePrepareMode: mode => prepareLabEditor(modeEditorText[mode]),
        stateApplyMode: mode => applyLabMode(mode),
        stateRecovery: (index, data) => {
            const fields = worksheetSaveSchema(index);
            return labDOM.call(
                'lab_storage_recovery', index, data,
                [localStorage.getItem(fields[0].local), localStorage.getItem(fields[1].local)], labConfig);
        },
        stateRecovered: (index, recovery, text, now) =>
            labDOM.call('lab_storage_recovered', index, recovery, [text], now),
        stateModeIndex: () => workspaceModeId(currentMode()),
        stateRestoreEditor: mode => restoreModeEditor(mode),
        stateSave: (...args) => saveLabState(...args),
        stateFetch: () => labFetch('/state'),
        stateDecode: response => response.labData(),
        stateApplySaved: data => applySavedState(data),
        stateRecover: data => restoreNewerLocalEditors(data),
        stateLocalEditors: () => restoreWorksheetEditors(key => localStorage.getItem(key), true),
        stateLocalControls: () => restoreWorksheetControls(key => localStorage.getItem(key), true),
        stateLocalCalendar: key => loadLabLocalState(key),
        stateLocalMode: () => localStorage.getItem(LAB_MODE_STORAGE_KEY),
        stateGuard: guard => guard(),
        stateHistoryPlan: (phase, state) => labDOM.call('lab_storage_history', phase, state, labConfig),
        stateCalendarHistory: (...args) => restoreCalendarHistory(...args),
        stateUpdated: text => applyUpdatedBindingExpression(text),
        stateClearSource: () => clearExpressionSource(),
        stateClearBindings: () => clearVariableValues(),
        stateCommitBindings: (...args) => commitVisibleBindingInputs(...args),
        stateNavigate: direction => navigateWorksheetHistory(direction),
        stateHistoryButtons: () => updateHistoryButtons(),
        stateCancelBindings: () => {
            clearTimeout(expressionBindingRefreshTimer);
            labRequests.cancel('bindings');
        },
        stateRestoreHistory: (state, request) => restoreHistoryState(state, () => labRequests.current(request)),
        stateEvaluateHistory: () => evaluateCurrentMode({skipHistoryUpdate: true}),
        stateCaptureGuard: isCurrent => {
            const mode = currentMode(), context = labRequests.context();
            return () => isCurrent() && currentMode() === mode && labRequests.context() === context;
        },
        stateCaptureEditor: () => {
            const native = labWire.exports(), index = native.lab_workspace_mode();
            const length = workspaceStageText(currentExpressionText() || expr.value.trim());
            const defaultLength = workspaceStageText(workspaceDefaultText(index), 1);
            if (!native.lab_workspace_editor_capture(index, length, defaultLength))
                throw new Error('Could not retain worksheet editor text');
            return index;
        },
        stateSaveEditor: index => labStateSync(15, {index}),
        stateSaveCalendar: index => saveCalendarState(WORKSPACE_MODE_NAMES[index]),
        stateCaptureRequest: request => captureCurrentModeEditor(() => labRequests.current(request)),
        stateSaveResult: () => saveCurrentModeResultState(),
        stateSetMode: mode => setMode(mode),
        stateSelectionGuard: () => {
            const context = labRequests.context(), main = labRequests.latestMain();
            return () => context === labRequests.context() && main === labRequests.latestMain();
        },
        stateSaveMode: () => saveLastLabMode(currentMode()),
        stateHideTarget: () => hideTargetEntry(),
        stateRestoreCurrentEditor: () => restoreModeEditor(currentMode()),
        stateSyncUI: () => syncModeUI(),
        stateRestoreResult: () => restoreModeResultState(currentMode()),
        stateNeedsBounds: () => labStateSync(16, {}),
        stateResetBounds: () => resetIntegratorBoundsToDefault(),
        stateFinishIntegrator: () => {
            if (integratorIntervalCap)
                integratorIntervalCap.value = String(validIntegratorIntervalCap(integratorIntervalCap.value));
            expr.focus();
        },
        stateFinishDatetime: () => {
            restoreDatetimeDefaultsIfBlank();
            datetimeDate?.focus();
        },
        stateFinishAlmanac: () => {
            restoreAlmanacDefaultsIfBlank();
            almanacDate?.focus();
        },
        stateFocusEditor: () => expr.focus(),
        stateStatus: text => setStatus(text),
        statePrecisionStep: direction => setRequestedPrecisionBits(
            labWire.exports().lab_workspace_precision_step(requestedPrecisionBits(), direction)),
        stateSavePrecision: () => savePrecisionState(),
        statePrecisionPlan: () => {
            const source = currentGoalSeekSource(), target = labEditorState.goalTarget || '';
            return {
                source,
                target,
                action: labWire.exports().lab_events_precision(workspaceModeId(currentMode()), !!source, !!target)
            };
        },
        statePrecisionGoal: plan => runGoalSeek(
            plan.source, plan.target,
            solvedStartValuesForGoalSeek(plan.source, labEditorState.fullText || currentExpressionText()),
            {skipHistoryUpdate: true}),
        statePrecisionExpression: () => evaluateExpression({skipHistoryUpdate: true, reuseLastInput: true}),
        stateEvaluate: () => evaluateCurrentMode(),
        ...labStateSyncServices()
    };
}

// Synchronous counterpart of the promise interpreter: plans contain no awaited operations.
function labStateSync(kind, frame) {
    return labDOM.sync('lab_state_sync', kind, frame, labFlowCapabilities());
}

function labStateSyncServices() {
    return {
        stateRetainText: (index, text) => {
            if (!labWire.exports().lab_workspace_editor_capture(index, workspaceStageText(text), 0))
                throw new Error('Could not retain worksheet text');
        },
        stateMatrixOperation: () => validMatrixOperation(matrixOperation && matrixOperation.value),
        stateMatrixOperand: () => String(matrixOperand && matrixOperand.value || '').trim(),
        stateCurrentBounds: () => currentIntegratorBoundsText(),
        stateIntervalCap: () => requestedIntegratorIntervalCap(),
        stateRecords: (index, values) => labDOM.call('lab_persist_records', index, values, modePrecisionBits),
        stateLocalRecords: records => {
            for (const [key, value] of Object.entries(records)) localStorage.setItem(key, String(value));
        },
        stateCancelSaveTimer: index => {
            clearTimeout(worksheetSaveTimers.get(index));
            worksheetSaveTimers.delete(index);
        },
        stateForgetSaveTimer: index => worksheetSaveTimers.delete(index),
        stateScheduleSave: save =>
            worksheetSaveTimers.set(save.index, setTimeout(() => labStateSync(1, {save}), save.delay)),
        statePublishSave: save => labStateSync(1, {save}),
        stateDatetime: () => currentDatetimeState(),
        stateAlmanac: () => currentAlmanacState(),
        stateDefaultEditor: (mode, index) => modeEditorText[mode] = workspaceDefaultText(index),
        stateWriteCalendar: (key, state) => saveLabLocalState(key, state),
        stateCalendarPatch: (mode, state) =>
            labDOM.call('lab_persist_calendar', workspaceModeId(mode), state, modePrecisionBits),
        stateWriteMode: mode => localStorage.setItem(LAB_MODE_STORAGE_KEY, mode),
        stateRestoredText: index => {
            const length = labWire.exports().lab_workspace_editor_restore(
                index, workspaceStageText(workspaceDefaultText(index), 1));
            return workspaceDecoder.decode(workspaceReadBytes(length));
        },
        stateBindingParts: text => bindingParts(text),
        stateClearFunction: () => clearFunctionRun(),
        stateResultProjection: mode => labDOM.call('lab_workspace_dom_result_restore', modeResultState[mode]),
        stateClearResult: () => clearResultPane(),
        stateResultValues: state => {
            ({lastTex, lastDerivativeExpression, currentVariables, currentDifferentiable, resultInputBindings} = state);
        },
        stateDerivativeButtons: () => renderDerivativeButtons(currentVariables),
        stateRenderedFit: () => scheduleRenderedTeXFit(),
        stateSolverFit: () => scheduleSolverTexFit(),
        stateNotifyMode: () => labRequests.modeChanged(currentMode()),
        stateResetEditorSize: () => resetEditorManualSize(),
        stateSyncTabs: () => syncModeTabs(),
        stateForceMode: mode => setMode(validLabMode(mode), {force: true}),
        stateRenderActiveRows: () => renderIntegratorRows(activeIntegratorRows()),
        stateDatetimeDefaults: () => restoreDatetimeDefaultsIfBlank(),
        stateAlmanacDefaults: () => restoreAlmanacDefaultsIfBlank(),
        stateRefreshLocation: () => {
            refreshDatetimeJurisdictionLocation().then(() => labStateSync(13, {}));
        },
        stateSaveDatetime: () => saveLastDatetimeState(),
        stateSaveAlmanac: () => saveLastAlmanacState(),
        stateBoundCount: () => currentIntegratorBoundRows().length,
        stateNormaliseCap: () => {
            if (integratorIntervalCap)
                integratorIntervalCap.value = String(validIntegratorIntervalCap(integratorIntervalCap.value));
        },
        stateReady: () => expressionReadyToEvaluate(),
        stateClearForward: () => clearForwardHistory(),
        stateEvaluateExpression: () => evaluateExpression(),
        stateEvaluateEquation: () => evaluateEquation(),
        stateEvaluateDiffequation: () => evaluateDiffequation(),
        stateEvaluateMatrix: () => evaluateMatrix(),
        stateEvaluateIntegrator: () => evaluateIntegrator(),
        stateEvaluateDatetime: () => evaluateDatetime(),
        stateDatetimeSummary: state => datetimeSummaryText(state),
        stateAlmanacSummary: state => almanacSummaryText(state),
        stateEditorText: () => currentExpressionText() || expr.value || '',
        stateHistoryCap: () => String(validIntegratorIntervalCap(integratorIntervalCap?.value)),
        stateHistoryRecord: (...args) => labDOM.call('lab_persist_history', ...args, labConfig),
        stateCurrentHistory: (...args) => historyStateForMode(currentMode(), ...args),
        statePushSnapshot: snapshot => {
            const mode = workspaceModeId(snapshot.mode), length = workspaceStageSnapshot(snapshot);
            if (labWire.exports().lab_workspace_history_push(mode, 0, length, !!snapshot.text, 3) < 0)
                throw new Error('Could not retain worksheet history');
        },
        stateValidOperation: value => validMatrixOperation(value),
        stateValidCap: value => String(validIntegratorIntervalCap(value)),
        stateSyncMatrix: () => syncMatrixControls(),
        stateSaveWorksheet: (...args) => saveWorksheetState(...args),
        stateSaveNamedEditor: index => saveWorksheetState(WORKSPACE_MODE_NAMES[index]),
        stateExpressionSavedText: () => modeEditorText.expression,
        stateNamedIndex: mode => workspaceModeId(mode),
        stateFlushSave: mode => saveWorksheetState(mode, undefined, {keepalive: true}),
        stateMergeRows: data => labDOM.call('lab_binding_rows_merge', currentIntegratorRows(), data),
        stateRenderRows: rows => renderIntegratorRows(rows)
    };
}
