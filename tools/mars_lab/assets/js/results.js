/**
 * Browser scheduling, request and clipboard adapters for C-owned result presentation.
 * Definition-only client script; app.js loads it before shared worksheet state.
 */

function showButtonTooltip(button) {
    if (labDOM.call('lab_tooltip_show', button, activeTooltipButton, window.innerWidth, window.innerHeight))
        activeTooltipButton = button;
}

function hideButtonTooltip() {
    labDOM.call('lab_tooltip_hide', activeTooltipButton);
    activeTooltipButton = null;
}


// DOM capability adapters only; C owns layout and native card state.
const resetMoreDigitsButton = (button, enabled) => labDOM.call('lab_layout_more', button, Number(!!enabled));
const hasAbbreviatedValue = value => value === true;
const resultZoomIndex = card => labWire.exports().lab_view_card_zoom(resultCardIds.get(card) ?? -1);
const applyResultZoom = card => labDOM.call('lab_layout_zoom', card);
const setResultZoom = (card, index) => labDOM.call('lab_layout_set_zoom', card, index, 0);
const stepResultZoom = (card, direction) => labDOM.call('lab_layout_set_zoom', card, direction, 1);
const renderResultCardExpansion = () => labDOM.call('lab_layout_expand', null, 0);
const resultCardExpanded = card => labWire.exports().lab_view_card_expanded() === resultCardIds.get(card);
const collapseResultCards = () => labDOM.call('lab_layout_expand', null, 2);
const toggleResultCardExpansion = button => labDOM.call('lab_layout_expand', button.closest('.result-card'), 1);
const setRenderedContent = (svg, fallback = '') => labDOM.call('lab_layout_content', svg, fallback);
const svgMarkupIntrinsicWidth = markup => labDOM.call('lab_layout_markup_width', markup);
const installSolverTexSvg = (markup, variant) => labDOM.call('lab_layout_solver_install', markup, variant);
const clearRenderedError = () => labDOM.call('lab_layout_error', '', 0);
const setRenderedError = message => labDOM.call('lab_layout_error', String(message || ''), 1);

function fitRenderedTeXToCard() {
    renderedTeXFitFrame = 0;
    labDOM.call('lab_layout_fit');
}

function scheduleRenderedTeXFit() {
    cancelAnimationFrame(renderedTeXFitFrame);
    renderedTeXFitFrame = requestAnimationFrame(fitRenderedTeXToCard);
}

function scheduleSolverTexFit() {
    cancelAnimationFrame(solverFitFrame);
    solverFitFrame = requestAnimationFrame(() => {
        void fitSolverTexToCard();
    });
}

const labPresentationNumericLines = new Map();
const labPresentationSolverTeX = new Map();
const labPresentationCompact = new Map();
const labPresentationEditors = new Map();
const labPresentationExpansions = new Map();
const labFunctionSyntax = new Map();
const labMatrixHeadings = new Map();
const labResultCaches = {
    numeric: labPresentationNumericLines,
    solver: labPresentationSolverTeX,
    compact: labPresentationCompact,
    editors: labPresentationEditors,
    expansions: labPresentationExpansions,
    functions: labFunctionSyntax,
    headings: labMatrixHeadings
};

// Called by the transport bridge before exposing decoded native responses.
function installLabPresentationData(data) {
    labDOM.call('lab_result_presentation_install', labResultCaches, data);
}

// These maps retain native presentations only; the browser does not classify or format their text.
function installLabFunctionSyntaxData(data) {
    labDOM.call('lab_result_syntax_install', labResultCaches, data);
}

function setExpandableText(element, button, displayText, fullText) {
    if (labDOM.call(
            'lab_result_expandable', labResultCaches, element, button, String(displayText || ''),
            String(fullText || '')))
        clearFunctionRun();
}

function setValueText(fullText) {
    labDOM.call('lab_result_value', labResultCaches, String(fullText || ''));
}

function renderDatetimeSections(element, button, sections, fallbackText = '') {
    if (!labDOM.call('lab_result_datetime', element, button, sections, String(fallbackText || '')))
        throw new Error(
            'DateTime presentation is missing from the server response. ' +
            'Run make mars-lab-restart, reload the page and evaluate again.');
}

function displayMatrixResult(data) {
    clearFunctionRun();
    const state = labDOM.call('lab_result_display', labResultCaches, data, 1);
    lastTex = state.TeX;
    resultInputBindings = state.bindings;
}

function displayCalculusResult(result) {
    clearFunctionRun();
    const state = labDOM.call('lab_result_display', labResultCaches, result, 2);
    lastTex = state.TeX;
    resultInputBindings = state.bindings;
}

function copyTextForTarget(target) {
    return labDOM.call('lab_result_copy', target, lastTex);
}

function parsedExpressionText() {
    return labDOM.call('lab_result_copy', 'expression', lastTex);
}

function setResultInputText(text, bindings = null) {
    labDOM.call('lab_result_input_set', String(text || ''));
    resultInputBindings = labDOM.call('lab_result_binding_snapshot', bindings);
}

function resultExpressionTextForInput() {
    return labDOM.call('lab_result_input_get', currentMode());
}

function sendResultExpressionToInput() {
    return labFlowContinue(56, {});
}

function setMatrixPrettyResult(resultText, prettyText, element = functionStyle, moreButton = functionMore) {
    return labFlowContinue(
        57, {text: String(resultText || ''), pretty: String(prettyText || ''), element, button: moreButton});
}

function setRenderedResult(data) {
    lastTex = labDOM.call('lab_result_rendered', labResultCaches, data, 0);
}

function renderTexSvg(TeX) {
    return labFlowContinue(61, {TeX});
}

function toggleTextDigits(element, button) {
    labDOM.call('lab_result_text_digits', labResultCaches, element, button);
}

function toggleRenderedDigits() {
    return labFlowContinue(58, {});
}

async function writeClipboardText(text) {
    if (navigator.clipboard && window.isSecureContext) {
        await navigator.clipboard.writeText(text);
        return;
    }

    const area = document.createElement('textarea');
    area.value = text;
    area.setAttribute('readonly', '');
    area.style.position = 'fixed';
    area.style.left = '-9999px';
    area.style.top = '0';
    document.body.appendChild(area);
    area.select();
    const ok = document.execCommand('copy');
    document.body.removeChild(area);
    if (!ok)
        throw new Error('Copy was blocked by the browser');
}

function flashCopyButton(button, ok) {
    labDOM.call('lab_result_copy_flash', button, ok ? 1 : 2);
    clearTimeout(button.copyResetTimer);
    button.copyResetTimer = setTimeout(() => labDOM.call('lab_result_copy_flash', button, 0), 1200);
}

function clearResultPane() {
    labDOM.call('lab_result_pane_clear');
    setDatetimeLocalText('');
    clearResultDetails();
}

function clearResultDetails(options = {}) {
    clearFunctionRun();
    const state = labDOM.call('lab_result_reset', labResultCaches, currentMode(), options);
    ({resultInputBindings, lastTex, lastDerivativeExpression, currentVariables, currentDifferentiable} = state);
    renderDerivativeButtons(currentVariables);
    labDOM.services(state, {clearVariableValues: () => clearVariableValues()});
}

function almanacPresentationVariant(data, visibility) {
    const variant = labDOM.call('lab_result_almanac_variant', data, visibility);
    if (!variant)
        throw new Error(
            'Almanac presentation is missing from the server response. ' +
            'Run make mars-lab-restart, reload the page and evaluate again.');
    return variant;
}

// Browser-owned live state; C owns visibility selection and event service ordering.
const labAlmanacEventState = Object.freeze({
    get visibility() {
        return almanacVisibilityMode;
    },
    get worksheet() {
        return almanacLastWorksheetData;
    }
});

function setAlmanacVisibility(value) {
    almanacVisibilityMode = value;
}

function bindAlmanacTotalityActions(root) {
    labDOM.call('lab_almanac_events_install', root, null);
}

function almanacWorksheetCopyText(data, visibility) {
    return almanacPresentationVariant(data, visibility).copy_text;
}

function renderAlmanacWorksheet(target, data) {
    const visibility = labDOM.call('lab_almanac_events_render', target, data, labAlmanacEventState);
    if (visibility === null)
        throw new Error(
            'Almanac presentation is missing from the server response. ' +
            'Run make mars-lab-restart, reload the page and evaluate again.');
    setAlmanacVisibility(visibility);
}

/** Copy after outstanding binding commits, using the browser clipboard. */
function copyInputFromEvent() {
    return labFlowContinue(59, {button: inputCopy});
}

// Browser projection capabilities consumed by native continuation plans.
function evaluationLabel(mode, field) {
    return labDOM.call('lab_evaluation_label', mode, field);
}

// Only browser-side effects live here; C selects their arguments and execution order.
const labEvaluationRecovery = Object.freeze({
    clearMatrixScalar: () => {
        lastMatrixScalarExpression = '';
    },
    setRenderedError: text => setRenderedError(text),
    resetRenderedDigits: () => resetMoreDigitsButton(renderedMore, false),
    setDatetimeLocalText: text => setDatetimeLocalText(text),
    clearResultDetails: options => clearResultDetails(options),
    clearRenderedError: () => clearRenderedError(),
    applyIntegratorBindingState: (data, text) => applyIntegratorBindingState(data, text),
    applyIntegratorResultBound: data => applyIntegratorResultBound(data),
    saveWorksheetState: mode => saveWorksheetState(mode)
});

function installEvaluationTextCards(cards) {
    setExpandableText(parsed, parsedMore, cards.expression, cards.full_expression);
    setResultInputText(cards.input);
    setExpandableText(functionStyle, functionMore, cards.function, cards.full_function);
    setValueText(cards.value);
}

// Host mutations are deliberately deferred: native plans never redefine live workspace getters.
const labEvaluationInstall = Object.freeze({
    setRenderedResult: data => setRenderedResult(data),
    setExpressionEditor: (...args) => setExpressionEditor(...args),
    renderVariableValues: bindings => renderVariableValues(bindings),
    clearVariableValues: () => clearVariableValues(),
    installEvaluationTextCards: cards => installEvaluationTextCards(cards),
    present: (mode, data) => labDOM.call('lab_evaluation_cards_present', mode, data),
    render: (mode, data, expandable) => lastTex = labDOM.call('lab_evaluation_render', mode, data, Number(expandable)),
    source: (key, text) => labEditorState[key] = text,
    modeSource: (mode, text) => modeEditorText[mode] = text,
    derivative: text => lastDerivativeExpression = text,
    scalar: text => lastMatrixScalarExpression = text,
    variables: (variables, differentiable) => {
        currentVariables = variables;
        currentDifferentiable = differentiable;
        renderDerivativeButtons(currentVariables);
    },
    saveWorksheetState: (...args) => saveWorksheetState(...args),
    displayMatrixResult: data => displayMatrixResult(data),
    scheduleRenderedTeXFit: () => scheduleRenderedTeXFit(),
    solverTextCards: cards => {
        setExpandableText(parsed, parsedMore, cards.expression, cards.full_expression);
        setResultInputText(cards.input);
        setExpandableText(functionStyle, functionMore, cards.function, cards.full_function);
    },
    setValueText: value => setValueText(value),
    setValueCardVisible: visible => setValueCardVisible(visible),
    applyIntegratorBindingState: (data, text) => applyIntegratorBindingState(data, text),
    applyIntegratorResultBound: data => applyIntegratorResultBound(data),
    calendarCard: card => renderDatetimeSections(card.element, card.button, card.sections, card.text),
    setDatetimeLocalText: (...args) => setDatetimeLocalText(...args),
    applyCalendarEvaluationFields: (mode, data) => applyCalendarEvaluationFields(mode, data),
    setResultInputText: text => setResultInputText(text),
    saveLastDatetimeState: () => saveLastDatetimeState(),
    saveLastAlmanacState: () => saveLastAlmanacState(),
    almanacRender: data => renderAlmanacWorksheet(rendered, data),
    almanacAccept: data => almanacLastWorksheetData = data,
    refreshAlmanacLandTotality: data => refreshAlmanacLandTotality(data)
});

// Getters use direct native exports only; they must not open another DOM scope.
const labEvaluationView = Object.freeze({
    get caches() {
        return labResultCaches;
    },
    get editors() {
        return labPresentationEditors;
    },
    get fullText() {
        return labEditorState.fullText;
    },
    get rawEditor() {
        return labEditorSnapshot();
    },
    get bodyText() {
        return expr.value;
    },
    get lastInput() {
        return labEditorState.lastInput;
    }
});

const labWeatherServices = Object.freeze({
    setStatus: status => setStatus(status),
    installWeather: (overview, data) => {
        const cards = labDOM.call('lab_evaluation_weather_cards', overview, data);
        renderDatetimeSections(rendered, null, cards.sections, cards.text);
    }
});

const labGoalServices = Object.freeze({
    setRenderedError: message => setRenderedError(message),
    resetRenderedDigits: () => resetMoreDigitsButton(renderedMore, false),
    clearResultDetails: options => clearResultDetails(options),
    setStatus: status => setStatus(status),
    captureHistory: () => pushExpressionHistory(currentExpressionText()),
    setRenderedResult: data => setRenderedResult(data),
    setEditor: (data, context) => setExpressionEditor(
        context.editorExpression, data.binding_values || null, context.editorBody, data.evaluation_ready),
    installCards: context => installEvaluationTextCards(context.cards),
    setLastInput: context => {
        labEditorState.lastInput = context.editorExpression;
    },
    clearDerivative: () => {
        lastDerivativeExpression = '';
    },
    setVariables: data => {
        currentVariables = variableNamesFromBindings(data.binding_values || []);
    },
    setDifferentiable: context => {
        currentDifferentiable = context.cards.differentiable;
    },
    renderDerivatives: () => renderDerivativeButtons(currentVariables),
    setSource: context => {
        labEditorState.goalSource = expressionForEditor(context.seek.expression).trim();
    },
    setTarget: context => {
        labEditorState.goalTarget = context.target;
    },
    hideTargetEntry: () => hideTargetEntry(),
    setSuccessStatus: context => setStatus(labDOM.call('lab_goal_status', context.cards))
});


/** Copy an exact native card source through the browser clipboard. */
function copyResultFromEvent(button) {
    return labFlowContinue(60, {button});
}

/** Browser capabilities for native result-action continuations. */
function labResultFlowServices() {
    return {
        resultInput: () => resultExpressionTextForInput(),
        resultSnapshot: () => ({mode: currentMode(), editor: expr.value, context: labRequests.context()}),
        resultPrepare: text => prepareLabEditor(text),
        resultHistoryEqual: (left, right) => historyStatesEqual(left, right),
        resultClearGoal: () => {
            clearGoalSeekRequest();
            hideTargetEntry();
        },
        resultSourceBindings: () => compactExpressionForEditor(currentExpressionText()),
        resultBindings: () => resultInputBindings,
        resultAuthoredBindings: (...args) => bindingsWithAuthoredValues(...args),
        resultApplyBinding: (...args) => applyMarsBindingExpression(...args),
        resultMatrixControls: () => {
            matrixOperation.value = 'eval';
            matrixOperand.value = '';
            syncRoundedSelect(matrixOperation);
            syncMatrixControls();
        },
        resultSaveEditor: () => saveCurrentModeEditorState(),
        resultPresentation: payload => requestLabPresentation(payload),
        resultLastTeX: () => lastTex,
        resultRenderTeX: TeX => renderTexSvg(TeX),
        resultCopyText: button => copyTextForTarget(button.dataset.copyTarget),
        resultDatetimeSummary: () => datetimeSummaryText(),
        resultClipboard: text => writeClipboardText(text),
        resultFlash: (...args) => flashCopyButton(...args),
        resultStatusLater: (text, delay) => setTimeout(() => setStatus(text), delay),
        resultFetch: (url, payload) => labFetch(url, {method: 'POST', body: labWire.encode(payload)}),
        resultDecode: response => response.labData(),
        resultStartupSnapshot: () => ({context: labRequests.context(), main: labRequests.latestMain()}),
        resultRestoreBounds: () => restoreIntegratorBoundsText(DEFAULT_INTEGRATOR_BOUNDS_TEXT),
        resultLoadState: () => loadLastState(),
        resultInitialEvaluation: () => evaluateActiveModeOnLoad()
    };
}
