/**
 * Binding controls, authored values and editor synchronisation.
 * Definition-only client script; app.js loads it before shared worksheet state.
 */

async function editIntegratorRow(item, index, operation) {
    return labFlowContinue(47, {item, index, operation});
}

const labBindingEventRoots = new WeakSet();

function installLabBindingEvents(root, nativeEvent, actions) {
    if (labBindingEventRoots.has(root))
        return;
    labBindingEventRoots.add(root);
    for (const type of ['keydown', 'change', 'input', 'pointerdown', 'click']) {
        root.addEventListener(type, event => {
            const action = labDOM.call(nativeEvent, root, event);
            if (action?.prevent)
                event.preventDefault();
            if (action)
                void actions[action.action]?.(action);
        }, true);
    }
}

async function prepareIntegratorBindingInput(input) {
    return labFlowContinue(48, {input});
}

const labIntegratorBindingActions = {
    invalidate: ({card}) => {
        nextIntegratorFormsRevision();
        integratorReferenceMetadata = null;
        card.input.setCustomValidity('');
    },
    prepare: ({card}) => prepareIntegratorBindingInput(card.input),
    edit: ({item, index, operation}) => editIntegratorRow(item, index, operation)
};

function renderIntegratorRows(rows) {
    nextIntegratorFormsRevision();
    const safeRows = labDOM.call('lab_binding_rows_prepare', rows);
    installLabBindingEvents(integratorBoundStack, 'lab_binding_integrator_event', labIntegratorBindingActions);
    labDOM.call('lab_binding_integrator_render', integratorBoundStack, safeRows);
}

function labEditorData(text) {
    return labDOM.call('lab_editor_metadata', 0, [text], labPresentationEditors);
}

function expressionWithSortedConstants(text) {
    return labDOM.call('lab_editor_metadata', 1, [text], labPresentationEditors);
}

function canGoalSeek() {
    return currentVariables.length > 0;
}
function expressionForEvaluation(text) {
    return String(text || '');
}
function expressionForEditor(text) {
    return String(text || '');
}

function restoreCompactBindingValues(text) {
    return labDOM.call('lab_editor_metadata', 2, [text], labPresentationEditors);
}

// UTF-8 validation and buffer transfer only; safe to call from a scoped native view getter.
function labEditorSnapshot() {
    const text = expr.value.trim();
    return {text, length: workspaceStageText(text)};
}

function currentExpressionText() {
    return labDOM.call('lab_editor_current', labEvaluationView);
}

function expressionBodyForEditor(text) {
    return labDOM.call('lab_editor_metadata', 3, [text], labPresentationEditors);
}

function clearExpressionSource() {
    labDOM.services(labDOM.call('lab_binding_projection_clear', 0), labBindingFlowServices());
}

function clearGoalSeekRequest() {
    labDOM.services(labDOM.call('lab_binding_projection_clear', 1), labBindingFlowServices());
}

function currentGoalSeekSource() {
    const length = labWire.exports().lab_workspace_source_resolve(workspaceStageText(expr.value.trim()), true);
    return workspaceDecoder.decode(workspaceReadBytes(length));
}

function expressionReadyToEvaluate() {
    return labDOM.call('lab_editor_ready', [currentMode()], labEvaluationView);
}

function bindingParts(text) {
    return labDOM.call('lab_editor_metadata', 4, [text], labPresentationEditors);
}

function compactExpressionForEditor(fullText) {
    return labDOM.call('lab_editor_metadata', 5, [fullText], labPresentationEditors);
}

async function expressionWithBindings(bodyText, bindings) {
    return labFlowContinue(40, {body: bodyText, bindings});
}

function visibleBindingValues() {
    return labDOM.call('lab_binding_visible', variableValues);
}

async function expressionWithVisibleBindings(sourceExpression, visibleBindings) {
    return await expressionWithBindings(sourceExpression, visibleBindings) || sourceExpression;
}

function bindingsWithAuthoredValues(bindings, sourceExpression, visibleBindings = []) {
    return labDOM.call(
        'lab_binding_authored', bindings, compactExpressionForEditor(sourceExpression).bindings, visibleBindings);
}

async function replaceBindingKindInExpression(sourceExpression, targetName, nextKind) {
    return labFlowContinue(55, {source: sourceExpression, name: targetName, nextKind});
}

function applyUpdatedBindingExpression(updated) {
    labDOM.services(
        labDOM.call('lab_binding_projection_update', workspaceModeId(currentMode()), updated, labEditorData(updated)),
        labBindingFlowServices());
}

async function applyMarsBindingExpression(updated, editorBodyText = null) {
    const mode = currentMode(), editorSnapshot = expr.value;
    return labRequests.run(
        'bindingCommit', mode, request => labFlowContinue(52, {updated, editorBodyText, editorSnapshot, request}));
}

async function applyMarsBindingsToEditedExpression(editedBody, sourceExpression, data, request) {
    return labFlowContinue(53, {editedBody, sourceExpression, data, request});
}

async function refreshEditedExpressionBindings(editedBody, sourceExpression, request) {
    try {
        return await labFlowContinue(54, {editedBody, sourceExpression, request});
    } finally {
        labRequests.finish(request);
    }
}

function scheduleEditedExpressionBindingRefresh() {
    const context = {editedBody: expr.value, sourceExpression: labEditorState.fullText};
    for (let phase = 0; phase < 2; ++phase)
        labDOM.services(labDOM.call('lab_binding_projection_refresh', phase, context), labBindingFlowServices());
}

function saveCurrentModeEditorState() {
    saveWorksheetState(currentMode());
}

const labBindingCommit = Object.freeze({
    source: text => labEditorState.fullText = text,
    cache: (snapshot, editor) => labDOM.call('lab_binding_committed_cache', snapshot, editor, bindingValueCache),
    applyUpdatedBindingExpression: text => applyUpdatedBindingExpression(text),
    applyMarsBindingExpression: (...args) => applyMarsBindingExpression(...args),
    refreshVariableValuesFromEditor: (...args) => refreshVariableValuesFromEditor(...args),
    updateHistoryButtons: () => updateHistoryButtons(),
    saveCurrentModeEditorState: () => saveCurrentModeEditorState()
});

async function commitLabBindingValues(inputs, isCurrent = () => true) {
    return labFlowContinue(51, {inputs, isCurrent});
}

async function commitBindingInput(input) {
    return commitLabBindingValues([input]);
}

async function commitVisibleBindingInputs(isCurrent = () => true) {
    return labFlowContinue(41, {isCurrent});
}

async function toggleBindingKind(binding) {
    return labFlowContinue(43, {binding});
}

function clearVariableValues() {
    labDOM.call('lab_binding_clear', variableValues);
    currentBindingKinds = new Map();
}

async function refreshVariableValuesFromEditor(isCurrent = () => true) {
    return labFlowContinue(44, {isCurrent});
}

function queueBindingInputCommit(input) {
    const mode = currentMode();
    const pending = pendingExpressionBindingCommit.then(() => labFlowContinue(45, {mode, input}));
    pendingExpressionBindingCommit = pending.catch(err => setStatus(String(err)));
    return pendingExpressionBindingCommit;
}

function bindingDisplayName(name) {
    return labDOM.call('lab_binding_label', [name]);
}

const labBindingActions = {
    evaluate: ({card}) => labFlowContinue(49, {card, operation: 0}),
    commit: ({card}) => queueBindingInputCommit(card.input),
    history: () => updateHistoryButtons(),
    clear: ({card}) => labFlowContinue(49, {card, operation: 1}),
    copy: ({card}) => labFlowContinue(49, {card, operation: 2}),
    toggle: ({card}) => toggleBindingKind(card.binding)
};

function renderVariableValues(bindings) {
    installLabBindingEvents(variableValues, 'lab_binding_event', labBindingActions);
    const rendered = labDOM.call('lab_binding_render', variableValues, bindings);
    bindingValueCache = new Map(rendered.values);
    currentBindingKinds = new Map(rendered.kinds);
}

function solvedStartValuesForGoalSeek(sourceExpression, solvedExpression, providedStart = {}) {
    return labDOM.call(
        'lab_binding_goal_starts', labEditorData(sourceExpression), labEditorData(solvedExpression), providedStart,
        bindingValueCache);
}

async function goalSeekExpressionAndStarts(sourceExpression, providedStart = {}) {
    return labFlowContinue(46, {source: sourceExpression, providedStart});
}

function setExpressionEditor(fullText, evaluatedBindings = null, editorBodyText = null, evaluationReady = null) {
    labDOM.services(
        labDOM.call(
            'lab_binding_projection_prepare',
            {mode: currentMode(), fullText, evaluatedBindings, editorBodyText, evaluationReady},
            labEditorData(fullText)),
        labBindingFlowServices());
    const mode = workspaceModeId(currentMode());
    const selected = labDOM.call(
        'lab_binding_editor', mode, {
            text: fullText,
            metadata: labEditorData(fullText),
            body: editorBodyText,
            bodyMetadata: labEditorData(editorBodyText),
            bindings: evaluatedBindings,
            ready: evaluationReady
        },
        mode === 4 ? currentIntegratorBoundNames() : null);
    labDOM.services(labDOM.call('lab_binding_projection_finish', selected), labBindingFlowServices());
}

function applyIntegratorBindingState(data, fallbackExpression) {
    labDOM.services(
        labDOM.call(
            'lab_binding_integrator_state', data, fallbackExpression, labPresentationEditors,
            currentIntegratorBoundNames()),
        {
            setExpressionEditor,
            clearVariableValues,
            renderVariableValues,
            setIntegratorBindingExpression: expression => modeEditorText.integrator = expression
        });
}

/** Apply native input decisions while retaining editor storage and refresh timers in the host. */
function refreshWorksheetFromInput() {
    labRequests.cancel('evaluate');
    labRequests.cancel('bindings');
    const mode = currentMode(), native = labWire.exports();
    const plan = labDOM.call(
        'lab_events_refresh', workspaceModeId(mode), Number(!!bindingParts(expr.value)),
        native.lab_workspace_source_matches(workspaceStageText(expr.value.trim())),
        {editor: expr, defaultDatetimeText: DEFAULT_DATETIME_TEXT});
    labDOM.services(plan, {
        setModeEditor: (mode, text) => {
            modeEditorText[mode] = text;
        },
        setSource: (field, text) => {
            labEditorState[field] = text;
        },
        refreshVariableValuesFromEditor,
        saveState: (mode, state) => saveWorksheetState(mode, state.text, state.options),
        cancelBindingRefresh: () => clearTimeout(expressionBindingRefreshTimer),
        markBindingRefresh: editor => labDOM.call('lab_events_refresh_mark', editor),
        clearGoalSeekRequest,
        scheduleEditedExpressionBindingRefresh,
        updateHistoryButtons
    });
}

/** Browser capabilities for native binding continuations; no request acceptance decisions live here. */
let labBindingRegistry;
function labBindingFlowServices() {
    return labBindingRegistry ||= Object.freeze({
        bindingCancelRefresh: () => clearTimeout(expressionBindingRefreshTimer),
        bindingBeginRefresh: (context, operation, mode, options) => context.request =
            labRequests.begin(operation, mode, options),
        bindingRefreshPending: () => expr.dataset.bindingRefreshValid = 'pending',
        bindingRefreshHistory: () => updateHistoryButtons(),
        bindingRefreshTimer: (editedBody, sourceExpression, request, delay) => {
            expressionBindingRefreshTimer = setTimeout(() => {
                void refreshEditedExpressionBindings(editedBody, sourceExpression, request);
            }, delay);
        },
        bindingSource: (field, value) => labEditorState[field] = value,
        bindingResetCache: () => bindingValueCache = new Map(),
        bindingClearFlags: () => {
            delete expr.dataset.bindingRefreshValid;
            delete expr.dataset.evaluationReady;
        },
        bindingClearGoal: () => clearGoalSeekRequest(),
        bindingClearSource: () => clearExpressionSource(),
        bindingClearValues: () => clearVariableValues(),
        bindingSetText: text => expr.value = text,
        bindingDeferEditor: context => {
            void labFlowContinue(50, context);
        },
        bindingResize: () => scheduleEditorResizeGrip(),
        bindingRenderValues: bindings => renderVariableValues(bindings),
        bindingSetVariables: bindings => currentVariables = variableNamesFromBindings(bindings),
        bindingRenderDerivatives: () => renderDerivativeButtons(currentVariables),
        bindingCapture: () => ({
            mode: currentMode(),
            modeId: workspaceModeId(currentMode()),
            source: currentExpressionText(),
            text: expr.value,
            editor: expr
        }),
        bindingCurrent: callback => callback(),
        bindingPending: () => pendingExpressionBindingCommit,
        bindingCommitVisible: isCurrent =>
            commitLabBindingValues(Array.from(variableValues.querySelectorAll('.binding-value-input')), isCurrent),
        bindingRunCommit: frame => labRequests.run('bindingCommit', frame.before.mode, request => labFlowContinue(42, {
                                                                                           request,
                                                                                           source: frame.before.source,
                                                                                           modeId: frame.before.modeId,
                                                                                           inputs: frame.inputs,
                                                                                           isCurrent: frame.isCurrent
                                                                                       })),
        bindingCommitInput: input => commitBindingInput(input),
        bindingPrepare: text => prepareLabEditor(text),
        bindingPresentation: (...args) => requestLabPresentation(...args),
        bindingExpression: data => data.editor.expression,
        bindingInvalid: () => {
            throw new TypeError('Cannot read properties of null or undefined binding');
        },
        bindingEffects: plan => labDOM.services(plan, labBindingCommit),
        bindingReplaceKind: (...args) => replaceBindingKindInExpression(...args),
        bindingScheduleRefresh: () => scheduleEditedExpressionBindingRefresh(),
        bindingProjectVisible: source => {
            const bindings = visibleBindingsForCurrentMode(compactExpressionForEditor(source).bindings);
            renderVariableValues(bindings);
            currentVariables = variableNamesFromBindings(bindings);
            renderDerivativeButtons(currentVariables);
        },
        bindingPrepareRows: (index, operation) =>
            refreshIntegratorForms(planIntegratorRowEdit(currentIntegratorRows(), index, operation)),
        bindingRenderRows: rows => renderIntegratorRows(rows),
        bindingRowError: (item, error) => item.title = error.message,
        bindingRefresh: () => refreshVariableValuesFromEditor(),
        bindingRefreshForms: () => refreshIntegratorForms(),
        bindingUpdateForms: prepared => labDOM.call('lab_binding_integrator_update', integratorBoundStack, prepared),
        bindingValidity: (input, error) => {
            input.setCustomValidity(error.message);
            input.reportValidity();
        },
        bindingQueue: input => queueBindingInputCommit(input),
        bindingRefocus: card => labDOM.call('lab_binding_refocus', variableValues, card, expr),
        bindingEvaluate: () => evaluateFromKeyboard(),
        bindingClipboard: text => writeClipboardText(text),
        bindingFlash: (button, success) => flashCopyButton(button, success),
        bindingReadyTimer: () => setTimeout(() => setStatus('Ready'), 1000),
        bindingSetEditor: (...args) => setExpressionEditor(...args),
        bindingFetch: (text, source, request) => fetchEvaluation(text, '', 'bindings', source, '', request),
        bindingAuthored: (bindings, source) => bindingsWithAuthoredValues(bindings, source),
        bindingBody: text => expressionBodyForEditor(text),
        bindingSaveMode: () => saveCurrentModeEditorState(),
        bindingInspectText: text => ({parts: bindingParts(text), body: expressionBodyForEditor(text)}),
        bindingProjectEdited: (assembled, body, bindings, data) => {
            labEditorState.fullText = assembled;
            labEditorState.displayText = body;
            const projected = labDOM.call('lab_binding_sync_apply', expr, {displayText: body, bindings, data});
            renderVariableValues(bindings);
            currentVariables = projected.variables;
            currentDifferentiable = projected.differentiable;
            renderDerivativeButtons(currentVariables);
            saveWorksheetState('expression', labEditorState.fullText, {debounce: true});
        },
        bindingApplyEdited: (...args) => applyMarsBindingsToEditedExpression(...args),
        bindingMarkInvalid: () => expr.dataset.bindingRefreshValid = 'false'
    });
}
