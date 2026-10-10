/**
 * Workspace state adapters and asynchronous form/request coordination.
 * C owns workspace DOM projection, control policy and presentation snapshots.
 * Loaded last, after the definition scripts, to initialise shared browser references
 * and views of native state before subscriptions and the first evaluation.
 */

function nextIntegratorFormsRevision() {
    const revision = labWire.exports().lab_rows_revision_next();
    if (!revision)
        throw new Error('Integrator form revision limit reached; reload the Lab');
    return revision;
}
let integratorReferenceMetadata = null;

// This copies structured DOM fields only. Native /forms owns all text interpretation.
function sanitizeIntegratorRow(row, fallbackName = 'x') {
    return labDOM.call('lab_binding_row_normalise', row, String(fallbackName));
}

function integratorFormsExpression() {
    return currentExpressionText() || expr.value || '';
}

function installIntegratorReferenceMetadata(expression, result) {
    integratorReferenceMetadata = labDOM.call('lab_binding_rows_metadata', expression, result);
}

async function refreshIntegratorForms(rows = currentIntegratorRows(), expression = integratorFormsExpression()) {
    return labFlowContinue(73, {rows, expression});
}

const integratorCandidateNames = Object.freeze(labDOM.call('lab_binding_row_names'));

function integratorDefaultVariableName(rows = []) {
    return labDOM.call('lab_binding_rows_name', rows);
}

function integratorRowText(row) {
    return integratorBoundsTextFromRows([row]);
}

function integratorBoundsTextFromRows(rows) {
    const text = labDOM.call('lab_binding_rows_text', rows);
    if (text === null)
        throw new Error('Integrator rows exceed the native limits or contain invalid Unicode');
    return text;
}

async function parseIntegratorBoundsText(text) {
    return labFlowContinue(74, {text});
}

function integratorFallbackRows() {
    return labDOM.call('lab_binding_rows_default', 0);
}

function integratorBlankRows() {
    return labDOM.call('lab_binding_rows_default', 1);
}

function currentIntegratorRows() {
    return labDOM.call('lab_binding_rows_read', integratorBoundStack);
}

function activeIntegratorRowPlan(rows = currentIntegratorRows(), expressionText = '') {
    const plan = labDOM.call(
        'lab_binding_rows_plan', rows, integratorReferenceMetadata, expressionText || integratorFormsExpression());
    if (!plan)
        throw new Error('Invalid integrator row plan');
    return plan;
}

function activeIntegratorBoundRows(rows = currentIntegratorRows(), expressionText = '') {
    return activeIntegratorRowPlan(rows, expressionText).bounds;
}

function activeIntegratorRows(rows = currentIntegratorRows(), expressionText = '') {
    return activeIntegratorRowPlan(rows, expressionText).rows;
}

function currentIntegratorBoundRows() {
    return activeIntegratorBoundRows();
}

function currentIntegratorBoundNames() {
    return new Set(currentIntegratorBoundRows().map((row) => row.name));
}

function planIntegratorRowEdit(rows, index, operation) {
    const plan = labDOM.call('lab_binding_rows_edit', rows, index, operation);
    if (!plan)
        throw new Error('This edit would remove the last bound or exceed the 256-row form limit');
    return plan;
}

function applyIntegratorResultBound(data) {
    labStateSync(14, {data});
}

async function restoreIntegratorBoundsText(text, isCurrent = () => true) {
    return labFlowContinue(75, {text, isCurrent, defaultBounds: DEFAULT_INTEGRATOR_BOUNDS_TEXT});
}

function currentIntegratorBoundsText() {
    return integratorBoundsTextFromRows(activeIntegratorRows());
}

function resetIntegratorBoundsToDefault() {
    return restoreIntegratorBoundsText(DEFAULT_INTEGRATOR_BOUNDS_TEXT);
}

function resetIntegratorBoundsToBlank() {
    renderIntegratorRows(integratorBlankRows());
}

function requestedIntegratorIntervalCap() {
    return labWire.intervals(marsFormNumber(integratorIntervalCap?.value), DEFAULT_INTEGRATOR_INTERVAL_CAP);
}

const WORKSPACE_MODE_NAMES =
    Object.freeze(['expression', 'equation', 'diffequation', 'matrix', 'integrator', 'datetime', 'almanac']);
const workspaceEncoder = new TextEncoder();
const workspaceDecoder = new TextDecoder('utf-8', {fatal: true, ignoreBOM: true});

function workspaceStage(bytes, index = 0) {
    const native = labWire.exports();
    if (bytes.length > native.lab_workspace_capacity())
        throw new Error('Worksheet snapshot exceeds the native 4 MiB limit');
    new Uint8Array(native.memory.buffer, native.lab_workspace_input(index), bytes.length).set(bytes);
    return bytes.length;
}

function workspaceStageText(text, index = 0) {
    const bytes = workspaceEncoder.encode(String(text));
    if (workspaceDecoder.decode(bytes) !== String(text))
        throw new Error('Worksheet text contains an unpaired Unicode surrogate');
    return workspaceStage(bytes, index);
}

function workspaceModeId(mode) {
    const length = workspaceStageText(String(mode || '').trim());
    return labWire.exports().lab_workspace_mode_id(length);
}

function workspaceStageSnapshot(state, index = 0) {
    return workspaceStage(labWire.encode({state: state || null}), index);
}

function workspaceReadBytes(length) {
    if (length < 0)
        throw new Error('Invalid native worksheet operation');
    const native = labWire.exports();
    return new Uint8Array(native.memory.buffer, native.lab_workspace_output(), length).slice();
}

function workspaceReadSnapshot(length) {
    const bytes = workspaceReadBytes(length);
    return length ? labWire.decode(bytes.buffer).state : null;
}

// Property access is a host view, never a second owner of worksheet state.
function createWorkspaceModeView(kind) {
    const view = Object.create(null);
    WORKSPACE_MODE_NAMES.forEach((mode, index) => {
        Object.defineProperty(view, mode, {
            enumerable: true,
            get() {
                const native = labWire.exports();
                if (kind === 'precision')
                    return native.lab_workspace_precision(index);
                return workspaceDecoder.decode(workspaceReadBytes(native.lab_workspace_editor_get(index)));
            },
            set(value) {
                const native = labWire.exports();
                if (kind === 'precision')
                    native.lab_workspace_precision_set(index, Number(value));
                else if (!native.lab_workspace_editor_set(index, workspaceStageText(String(value))))
                    throw new Error('Could not retain worksheet editor text');
            }
        });
    });
    return Object.preventExtensions(view);
}

function createWorkspaceSourceView() {
    const view = Object.create(null);
    ['fullText', 'displayText', 'lastInput', 'goalSource', 'goalTarget'].forEach((name, field) => {
        Object.defineProperty(view, name, {
            enumerable: true,
            get() {
                return workspaceDecoder.decode(workspaceReadBytes(labWire.exports().lab_workspace_source_get(field)));
            },
            set(value) {
                if (!labWire.exports().lab_workspace_source_set(field, workspaceStageText(String(value))))
                    throw new Error('Could not retain editor source text');
            }
        });
    });
    return Object.preventExtensions(view);
}

function precisionDigitsForBits(bits) {
    return labWire.exports().lab_workspace_digits(Number(bits));
}

function requestedPrecisionBits() {
    const native = labWire.exports();
    return native.lab_workspace_requested_precision(native.lab_workspace_mode());
}

function precisionStatusText() {
    return labDOM.call('lab_workspace_dom_precision');
}

function setStatus(text) {
    labDOM.call('lab_workspace_dom_status', String(text));
}

function currentMode() {
    return WORKSPACE_MODE_NAMES[labWire.exports().lab_workspace_mode()];
}

function syncModeTabs() {
    labDOM.call('lab_workspace_dom_tabs', modeTabs, currentMode());
}

function setMode(mode, options = {}) {
    return labStateSync(6, {index: workspaceModeId(mode), options});
}

// Defaults arrive from the native catalogue; the C controller decides when to use them.
function workspaceDefaultText(index) {
    return [
        DEFAULT_EXPRESSION_TEXT, DEFAULT_EQUATION_TEXT, DEFAULT_DIFFEQUATION_TEXT, DEFAULT_MATRIX_TEXT,
        DEFAULT_INTEGRATOR_TEXT, DEFAULT_DATETIME_TEXT, DEFAULT_ALMANAC_TEXT
    ][index];
}

async function captureCurrentModeEditor(isCurrent = () => true) {
    return labFlowContinue(23, {isCurrent});
}

function restoreModeEditor(mode) {
    labStateSync(4, {index: workspaceModeId(mode)});
}

function setValueCardVisible(visible) {
    labDOM.call('lab_workspace_dom_value', Number(!!visible));
}

function saveCurrentModeResultState(mode = currentMode()) {
    modeResultState[mode] = labDOM.call(
        'lab_workspace_dom_result_save', {lastTex, lastDerivativeExpression, currentVariables, currentDifferentiable});
}

function restoreModeResultState(mode = currentMode()) {
    labStateSync(5, {mode});
}

function syncMatrixControls() {
    syncRoundedSelect(matrixOperation);
    labDOM.call('lab_workspace_dom_matrix', workspaceModeId(currentMode()));
    scheduleEditorResizeGrip();
}

function syncModeUI() {
    labDOM.call('lab_workspace_dom_mode', workspaceModeId(currentMode()), ALMANAC_COVERAGE_TEXT);
    clearFunctionRun();
    syncMatrixControls();
    syncHelpCards();
    updateHistoryButtons();
    scheduleWorkspacePanelFit();
}

function resetEditorManualSize() {
    labDOM.call('lab_workspace_dom_editor_reset', labTextareas);
}

function syncEditorResizeGrip() {
    editorResizeFrame = 0;
    labDOM.call('lab_workspace_dom_editor_resize', labTextareas, window.innerHeight);
}

function scheduleEditorResizeGrip() {
    if (editorResizeFrame)
        cancelAnimationFrame(editorResizeFrame);
    editorResizeFrame = requestAnimationFrame(syncEditorResizeGrip);
}

function scheduleWorkspacePanelFit() {
    scheduleEditorResizeGrip();
}

function syncHelpCards() {
    labDOM.call('lab_workspace_dom_help_cards', helpCards, currentMode());
}

function applyLabMode(mode) {
    labStateSync(7, {mode});
}

function showResults() {
    labDOM.call('lab_workspace_dom_help', 0);
}

function showHelp() {
    labDOM.call('lab_workspace_dom_help', 1);
}

function toggleHelp() {
    labDOM.call('lab_workspace_dom_help', 2);
}

function variableNamesFromBindings(bindings) {
    return labDOM.call('lab_binding_variables', bindings);
}

function visibleBindingsForCurrentMode(bindings) {
    const mode = workspaceModeId(currentMode());
    return labDOM.call('lab_binding_select', mode, bindings, mode === 4 ? currentIntegratorBoundNames() : null);
}

function showTargetEntry() {
    labDOM.call('lab_workspace_dom_target', 1);
}

function hideTargetEntry() {
    labDOM.call('lab_workspace_dom_target', 0);
}

async function evaluateCurrentMode(options = {}) {
    await evaluateLabMode(currentMode(), options);
}

function syncWorksheetButtons(isBusy = false) {
    labDOM.call(
        'lab_workspace_dom_controls', workspaceModeId(currentMode()), Number(!!isBusy),
        Number(!!expressionReadyToEvaluate()), currentHistoryLength(), currentForwardHistoryLength(),
        Number(!!canGoalSeek()), Number(atMinimumPrecision()), Number(atMaximumPrecision()));
}

function setBusy(isBusy) {
    syncWorksheetButtons(isBusy);
    labDOM.call('lab_workspace_dom_busy', Number(!!isBusy), equationVariable, copyButtons, moreDigitButtons);
}

function updateHistoryButtons() {
    syncWorksheetButtons();
}

function pushExpressionHistory(entry) {
    labStateSync(10, {entry});
}

function renderDerivativeButtons(variables) {
    labDOM.call(
        'lab_workspace_dom_derivatives', variables, variables.map(bindingDisplayName), Number(!!currentDifferentiable));
    for (const button of derivativeButtons.querySelectorAll('button'))
        button.addEventListener('click', () => {
            [takeDerivative, takeIntegral][Number(button.dataset.calculusAction)](button.dataset.variable, button);
        });
}

function setActionRunning(button, running) {
    labDOM.call('lab_workspace_dom_running', button, Number(!!running));
}

function requestedValuePrecision() {
    return precisionDigitsForBits(requestedPrecisionBits());
}

function atMinimumPrecision() {
    const native = labWire.exports();
    return !native.lab_workspace_precision_can_step(native.lab_workspace_mode(), -1);
}

function atMaximumPrecision() {
    const native = labWire.exports();
    return !native.lab_workspace_precision_can_step(native.lab_workspace_mode(), 1);
}

function setRequestedPrecisionBits(bits) {
    const native = labWire.exports();
    native.lab_workspace_request_precision(native.lab_workspace_mode(), Number(bits));
}

function evaluateFromKeyboard() {
    labStateSync(8, {});
}

/** Await a guarded clear operation without disturbing a newer request. */
async function clearWorksheetFromEvent() {
    await labFlowContinue(72, {});
}

/** Restore a selected worksheet across asynchronous editor and result preparation. */
async function selectWorksheetMode(mode) {
    return labRequests.runUI('evaluate', currentMode(), request => labFlowContinue(24, {mode, request}));
}

async function changeWorksheetPrecision(direction) {
    return labFlowContinue(25, {direction});
}

function flushWorksheetState() {
    labStateSync(12, {});
}

/** Convert browser control values using their existing native validators. */
function normaliseWorksheetControl(kind) {
    labStateSync(
        11, {index: kind, controls: [matrixOperation, equationVariable, integratorIntervalCap], config: labConfig});
}


// Browser references and views of C-owned worksheet state, initialised after the definition scripts.
const {
    expr,
    subtitle,
    leftPaneTitle,
    matrixControls,
    matrixOperation,
    matrixOperand,
    matrixOperandLabel,
    equationControls,
    diffequationControls,
    integratorControls,
    integratorBoundStack,
    integratorIntervalCap,
    datetimeControls,
    datetimeDate,
    datetimeJdn,
    datetimeStart,
    datetimeYear,
    datetimeJurisdiction,
    datetimeTown,
    datetimeLatitude,
    datetimeLongitude,
    datetimeGmtOffset,
    datetimeLocal,
    datetimeLocalBody,
    almanacControls,
    almanacDate,
    almanacTime,
    almanacZone,
    almanacJurisdiction,
    almanacTown,
    almanacLatitude,
    almanacLongitude,
    almanacElevation,
    marsDatePicker,
    marsDatePickerMonth,
    marsDatePickerYear,
    marsDatePickerWeekdays,
    marsDatePickerGrid,
    marsDatePickerToday,
    marsDatePickerClose,
    run,
    back,
    forward,
    help,
    goalSeek,
    clear,
    targetRow,
    goalTarget,
    lessPrecision,
    morePrecision,
    derivativeButtons,
    variableValues,
    mobileAccess,
    mobileTitle,
    mobileHint,
    mobileUrl,
    mobileQr,
    statusEl,
    inputCopy,
    labWorkspace,
    rightPaneTitle,
    resultUseInput,
    resultPane,
    helpPane,
    rendered,
    renderedTitle,
    renderedMore,
    parsed,
    parsedMore,
    functionStyle,
    functionTitle,
    functionMore,
    functionRun,
    functionRunResult,
    functionRunOutput,
    valueCard,
    valueNoteCard,
    valueNote,
    value,
    valueTitle,
    valueMore,
    labTextareas,
    modeTabs,
    helpCards,
    copyButtons,
    moreDigitButtons,
    resultCards,
} = labDOM.call('lab_workspace_dom_references');
const equationVariable = null;
const controlToken = labConfig.CONTROL_TOKEN;
let activeTooltipButton = null;

const resultCardIds = new WeakMap(resultCards.map((card, index) => [card, index]));
if (!labWire.exports().lab_view_cards_reset(resultCards.length))
    throw new Error('Too many result cards for the native view controller');
let lastTex = '';
let resultInputBindings = [];
let renderedTeXFitFrame = 0;
let solverFitFrame = 0;
let lastDerivativeExpression = '';
let currentVariables = [];
let currentBindingKinds = new Map();
let currentDifferentiable = true;

labWire.exports().lab_workspace_reset();
const expressionHistory = 0;
const forwardHistory = 1;
const labEditorState = createWorkspaceSourceView();
let expressionBindingRefreshTimer = 0;
let pendingExpressionBindingCommit = Promise.resolve();

labDOM.services(
    labDOM.call('lab_bootstrap_workspace', 0, {
        token: controlToken,
        get search() {
            return window.location.search;
        },
        get prefix() {
            return String(labConfig.CONTROL_QUERY_PREFIX);
        },
        get pathname() {
            return window.location.pathname;
        },
        get hash() {
            return window.location.hash;
        }
    }),
    {replaceLocation: url => window.history.replaceState(null, '', url)});
let lastMatrixScalarExpression = '';
let bindingValueCache = new Map();
const modePrecisionBits = createWorkspaceModeView('precision');
const DEFAULT_EXPRESSION_TEXT = labConfig.DEFAULT_EXPRESSION;
const DEFAULT_EQUATION_TEXT = labConfig.DEFAULT_EQUATION;
const DEFAULT_DIFFEQUATION_TEXT = labConfig.DEFAULT_DIFFEQUATION;
const DEFAULT_EQUATION_VARIABLE_TEXT = labConfig.DEFAULT_EQUATION_VARIABLE;
const DEFAULT_MATRIX_TEXT = labConfig.DEFAULT_MATRIX;
const DEFAULT_INTEGRATOR_TEXT = labConfig.DEFAULT_INTEGRATOR;
const DEFAULT_INTEGRATOR_BOUNDS_TEXT = labConfig.DEFAULT_INTEGRATOR_BOUNDS;
const DEFAULT_INTEGRATOR_INTERVAL_CAP = labConfig.DEFAULT_INTEGRATOR_INTERVAL_CAP;
const DEFAULT_DATETIME_TEXT = labConfig.DEFAULT_DATETIME_TEXT;
const DEFAULT_DATETIME_DATE = labConfig.DEFAULT_DATETIME_DATE;
const DEFAULT_DATETIME_JURISDICTION = labConfig.DEFAULT_DATETIME_JURISDICTION;
const DEFAULT_DATETIME_GMT_OFFSET = labConfig.DEFAULT_DATETIME_GMT_OFFSET;
const DEFAULT_ALMANAC_TEXT = labConfig.DEFAULT_ALMANAC_TEXT;
const DEFAULT_ALMANAC_TIME = labConfig.DEFAULT_ALMANAC_TIME;
const DEFAULT_ALMANAC_ZONE = labConfig.DEFAULT_ALMANAC_ZONE;
const DEFAULT_ALMANAC_LATITUDE = labConfig.DEFAULT_ALMANAC_LATITUDE;
const DEFAULT_ALMANAC_LONGITUDE = labConfig.DEFAULT_ALMANAC_LONGITUDE;
const DEFAULT_ALMANAC_VISIBILITY = labConfig.DEFAULT_ALMANAC_VISIBILITY;
const ALMANAC_WORKSHEET_TITLE = labConfig.ALMANAC_WORKSHEET_TITLE;
const ALMANAC_COVERAGE_TEXT = labConfig.ALMANAC_COVERAGE_TEXT_JS;
labDOM.services(
    labDOM.call('lab_bootstrap_workspace', 1, {control: datetimeJurisdiction, fallback: DEFAULT_DATETIME_JURISDICTION}),
    {writeValue: (control, value) => control.value = value});
let datetimeAutoGmtOffset =
    labDOM.call('lab_bootstrap_workspace', 2, {control: datetimeGmtOffset, fallback: DEFAULT_DATETIME_GMT_OFFSET});
let datetimeGmtOffsetTouched = false;
let almanacVisibilityMode = DEFAULT_ALMANAC_VISIBILITY;
let almanacLastWorksheetData = null;
const HOLIDAY_JURISDICTION_SET = new Set(labCatalogue.options.map(row => row[0]));
const JURISDICTION_TOWN_OPTIONS = labCatalogue.towns;
const LAB_MODE_STORAGE_KEY = 'mars.exprLab.lastMode';
const EXPRESSION_TIMESTAMP_STORAGE_KEY = 'mars.exprLab.lastExpressionUpdatedAt';
let lastExpressionUpdatedAt = 0;
const modeEditorText = createWorkspaceModeView('editor');
Object.assign(modeEditorText, {
    expression: DEFAULT_EXPRESSION_TEXT,
    equation: DEFAULT_EQUATION_TEXT,
    diffequation: DEFAULT_DIFFEQUATION_TEXT,
    matrix: DEFAULT_MATRIX_TEXT,
    integrator: DEFAULT_INTEGRATOR_TEXT,
    datetime: DEFAULT_DATETIME_TEXT,
    almanac: DEFAULT_ALMANAC_TEXT
});
const modeResultState = labDOM.call('lab_workspace_dom_result_states');

let editorResizeFrame = 0;

const marsDatePickerState = {
    input: null,
    button: null,
    shell: null,
    get year() {
        const native = labWire.exports();
        return native.lab_forms_date_year(native.lab_forms_picker_date());
    },
    get month() {
        const native = labWire.exports();
        return native.lab_forms_date_month(native.lab_forms_picker_date());
    }
};
