/** Shared DOM references and mutable worksheet state; loaded once by app.js. */

const expr = document.getElementById('expr');
const labTextareas = Array.from(document.querySelectorAll('textarea'));
const subtitle = document.getElementById('subtitle');
const leftPaneTitle = document.getElementById('leftPaneTitle');
const modeTabs = Array.from(document.querySelectorAll('.mode-tab'));
const matrixControls = document.getElementById('matrixControls');
const matrixOperation = document.getElementById('matrixOperation');
const matrixOperand = document.getElementById('matrixOperand');
const matrixOperandLabel = document.getElementById('matrixOperandLabel');
const equationControls = document.getElementById('equationControls');
const diffequationControls = document.getElementById('diffequationControls');
const equationVariable = null;
const integratorControls = document.getElementById('integratorControls');
const integratorBoundStack = document.getElementById('integratorBoundStack');
const integratorIntervalCap = document.getElementById('integratorIntervalCap');
const datetimeControls = document.getElementById('datetimeControls');
const datetimeDate = document.getElementById('datetimeDate');
const datetimeJdn = document.getElementById('datetimeJdn');
const datetimeStart = document.getElementById('datetimeStart');
const datetimeEnd = document.getElementById('datetimeEnd');
const datetimeYear = document.getElementById('datetimeYear');
const datetimeJurisdiction = document.getElementById('datetimeJurisdiction');
const datetimeTown = document.getElementById('datetimeTown');
const datetimeLatitude = document.getElementById('datetimeLatitude');
const datetimeLongitude = document.getElementById('datetimeLongitude');
const datetimeElevation = document.getElementById('datetimeElevation');
const datetimeGmtOffset = document.getElementById('datetimeGmtOffset');
const datetimeLocal = document.getElementById('datetimeLocal');
const datetimeLocalBody = document.getElementById('datetimeLocalBody');
const almanacControls = document.getElementById('almanacControls');
const almanacDate = document.getElementById('almanacDate');
const almanacTime = document.getElementById('almanacTime');
const almanacZone = document.getElementById('almanacZone');
const almanacJurisdiction = document.getElementById('almanacJurisdiction');
const almanacTown = document.getElementById('almanacTown');
const almanacLatitude = document.getElementById('almanacLatitude');
const almanacLongitude = document.getElementById('almanacLongitude');
const almanacElevation = document.getElementById('almanacElevation');
const marsDatePicker = document.getElementById('marsDatePicker');
const marsDatePickerTitle = document.getElementById('marsDatePickerTitle');
const marsDatePickerMonth = document.getElementById('marsDatePickerMonth');
const marsDatePickerYear = document.getElementById('marsDatePickerYear');
const marsDatePickerYearDown = document.getElementById('marsDatePickerYearDown');
const marsDatePickerYearUp = document.getElementById('marsDatePickerYearUp');
const marsDatePickerWeekdays = document.getElementById('marsDatePickerWeekdays');
const marsDatePickerGrid = document.getElementById('marsDatePickerGrid');
const marsDatePickerPrev = document.getElementById('marsDatePickerPrev');
const marsDatePickerNext = document.getElementById('marsDatePickerNext');
const marsDatePickerToday = document.getElementById('marsDatePickerToday');
const marsDatePickerClose = document.getElementById('marsDatePickerClose');
const marsDateButtons = Array.from(document.querySelectorAll('[data-date-target]'));
const helpCards = Array.from(document.querySelectorAll('#helpPane .help-card'));
const run = document.getElementById('run');
const back = document.getElementById('back');
const forward = document.getElementById('forward');
const help = document.getElementById('help');
const goalSeek = document.getElementById('goalSeek');
const clear = document.getElementById('clear');
const targetRow = document.getElementById('targetRow');
const goalTarget = document.getElementById('goalTarget');
const lessPrecision = document.getElementById('lessPrecision');
const morePrecision = document.getElementById('morePrecision');
const derivativeButtons = document.getElementById('derivativeButtons');
const variableValues = document.getElementById('variableValues');
const mobileAccess = document.getElementById('mobileAccess');
const mobileTitle = document.getElementById('mobileTitle');
const mobileHint = document.getElementById('mobileHint');
const mobileUrl = document.getElementById('mobileUrl');
const mobileQr = document.getElementById('mobileQr');
const controlToken = labConfig.CONTROL_TOKEN;
const buttonTooltip = document.createElement('div');
const BUTTON_TOOLTIP_TEXT = Object.freeze({
  back: 'Return to the previous input in this mode',
  clear: 'Clear the current input and its results',
  forward: 'Move to the next input in this mode',
  goalSeek: 'Find a variable value that reaches the requested target',
  help: 'Show or hide help for the current mode',
  inputCopy: 'Copy the current input',
  lessPrecision: 'Calculate and display fewer significant digits',
  marsDatePickerClose: 'Close the date picker',
  marsDatePickerToday: 'Use the current date',
  morePrecision: 'Calculate and display more significant digits',
  resultUseInput: 'Put this result into the current mode as a new input',
  run: 'Evaluate the current input'
});
let activeTooltipButton = null;
let activeTooltipDescribedBy = null;

buttonTooltip.className = 'mars-button-tooltip';
buttonTooltip.id = 'marsButtonTooltip';
buttonTooltip.setAttribute('role', 'tooltip');
document.body.appendChild(buttonTooltip);

document.addEventListener('pointerover', (event) => {
  const button = eventButton(event.target);
  if (button && (!event.relatedTarget || !button.contains(event.relatedTarget)))
    showButtonTooltip(button);
});
document.addEventListener('pointerout', (event) => {
  const button = eventButton(event.target);
  if (button && (!event.relatedTarget || !button.contains(event.relatedTarget)))
    hideButtonTooltip();
});
document.addEventListener('focusin', (event) => showButtonTooltip(eventButton(event.target)));
document.addEventListener('focusout', hideButtonTooltip);
document.addEventListener('click', hideButtonTooltip);
document.addEventListener('keydown', (event) => {
  if (event.key === 'Escape')
    hideButtonTooltip();
});
window.addEventListener('scroll', hideButtonTooltip, {passive: true});
window.addEventListener('resize', hideButtonTooltip);

enhanceRoundedSelect(datetimeJurisdiction, {
  searchable: true,
  searchPlaceholder: 'Search jurisdictions',
  emptyText: 'No matching jurisdiction'
});
enhanceRoundedSelect(datetimeTown, {
  searchable: true,
  searchPlaceholder: 'Search towns',
  emptyText: 'No towns for this jurisdiction',
  placeholder: 'Custom location',
  renderOption: townOptionDisplay
});
enhanceRoundedSelect(almanacJurisdiction, {
  searchable: true,
  searchPlaceholder: 'Search jurisdictions',
  emptyText: 'No matching jurisdiction'
});
enhanceRoundedSelect(almanacTown, {
  searchable: true,
  searchPlaceholder: 'Search towns',
  emptyText: 'No towns for this jurisdiction',
  placeholder: 'Custom location',
  renderOption: townOptionDisplay
});
let datetimeLocalRefreshSequence = 0;
let datetimeEvaluationSequence = 0;
let datetimeWeatherAbortController = null;
let almanacEvaluationSequence = 0;
let almanacLocationRefreshSequence = 0;
const statusEl = document.getElementById('status');
const inputCopy = document.getElementById('inputCopy');
const labWorkspace = document.getElementById('labWorkspace');
const workspacePanel = document.getElementById('workspacePanel');
const resultWorkspacePanel = document.getElementById('resultWorkspacePanel');
const rightPaneTitle = document.getElementById('rightPaneTitle');
const resultUseInput = document.getElementById('resultUseInput');
const resultPane = document.getElementById('resultPane');
const helpPane = document.getElementById('helpPane');
const rendered = document.getElementById('rendered');
const renderedTitle = document.getElementById('renderedTitle');
const renderedCopy = document.getElementById('renderedCopy');
const renderedMore = document.getElementById('renderedMore');
const parsed = document.getElementById('parsed');
const parsedTitle = document.getElementById('parsedTitle');
const parsedMore = document.getElementById('parsedMore');
const functionStyle = document.getElementById('functionStyle');
const functionTitle = document.getElementById('functionTitle');
const functionMore = document.getElementById('functionMore');
const functionRun = document.getElementById('functionRun');
const functionRunResult = document.getElementById('functionRunResult');
const functionRunOutput = document.getElementById('functionRunOutput');
let functionRunSequence = 0;
let functionRunController = null;
const valueCard = document.getElementById('valueCard');
const valueNoteCard = document.getElementById('valueNoteCard');
const valueNote = document.getElementById('valueNote');
const value = document.getElementById('value');
const valueTitle = document.getElementById('valueTitle');
const valueMore = document.getElementById('valueMore');
const copyButtons = Array.from(document.querySelectorAll('.copy-result'));
const moreDigitButtons = Array.from(document.querySelectorAll('.more-digits'));
const resultCards = Array.from(document.querySelectorAll('.result-card'));
const expandCardButtons = Array.from(document.querySelectorAll('[data-expand-card]'));
const zoomButtons = Array.from(document.querySelectorAll('[data-zoom-step], [data-zoom-reset]'));
const MARS_FUNCTION_KEYWORDS = new Set([
  'array', 'const', 'else', 'equation', 'expression', 'i', 'if', 'matrix', 'return'
]);
const MARS_FUNCTION_CONSTANTS = new Set(['NAN', 'e', 'pi', 'π']);
const RESULT_ZOOM_LEVELS = [0.5, 0.67, 0.8, 1, 1.25, 1.5, 2, 3, 4, 6, 8];
const RESULT_ZOOM_DEFAULT_INDEX = 3;
let lastTex = '';
let resultInputBindings = [];
let renderedTeXFitFrame = 0;
let solverFitFrame = 0;
let solverWrapRenderPending = false;
let lastDerivativeExpression = '';
let currentVariables = [];
let currentBindingKinds = new Map();
let currentDifferentiable = true;

let expressionHistory = createEmptyModeHistory();
let forwardHistory = createEmptyModeHistory();
const modeCommittedState = {
  expression: null,
  equation: null,
  diffequation: null,
  matrix: null,
  integrator: null,
  datetime: null,
  almanac: null
};
let workingPrecisionBits = 256;
let fullExpressionText = '';
let displayedExpressionText = '';
let expressionBindingRefreshTimer = 0;
let expressionBindingRefreshSequence = 0;
let pendingExpressionBindingCommit = Promise.resolve();

if (controlToken && window.location.search.includes(labConfig.CONTROL_QUERY_PREFIX)) {
  window.history.replaceState(null, '', window.location.pathname + window.location.hash);
}
let lastEvaluationInputText = '';
let lastMatrixScalarExpression = '';
let bindingValueCache = new Map();
const DOUBLE_PRECISION_BITS = 53;
const DOUBLE_PRECISION_DIGITS = 17;
const QFLOAT_PRECISION_BITS = 106;
const MAX_PRECISION_BITS = 1048576;
const MODE_DEFAULT_PRECISION_BITS = {
  expression: 256,
  equation: 256,
  diffequation: 256,
  matrix: 256,
  integrator: DOUBLE_PRECISION_BITS,
  datetime: DOUBLE_PRECISION_BITS,
  almanac: DOUBLE_PRECISION_BITS
};
const modePrecisionBits = {
  expression: MODE_DEFAULT_PRECISION_BITS.expression,
  equation: MODE_DEFAULT_PRECISION_BITS.equation,
  diffequation: MODE_DEFAULT_PRECISION_BITS.diffequation,
  matrix: MODE_DEFAULT_PRECISION_BITS.matrix,
  integrator: MODE_DEFAULT_PRECISION_BITS.integrator,
  datetime: MODE_DEFAULT_PRECISION_BITS.datetime,
  almanac: MODE_DEFAULT_PRECISION_BITS.almanac
};
const START_FORBIDDEN_PATTERN = /[=,;|{}]/;
const COMPACT_BINDING_VALUE_LIMIT = 20;
const COMPACT_BINDING_VALUE_KEEP = 16;
const COMPACT_INTEGER_DIGITS_KEEP = 23;
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
const DEFAULT_DATETIME_LATITUDE = labConfig.DEFAULT_DATETIME_LATITUDE;
const DEFAULT_DATETIME_LONGITUDE = labConfig.DEFAULT_DATETIME_LONGITUDE;
const DEFAULT_DATETIME_ELEVATION = labConfig.DEFAULT_DATETIME_ELEVATION;
const DEFAULT_DATETIME_GMT_OFFSET = labConfig.DEFAULT_DATETIME_GMT_OFFSET;
const DEFAULT_ALMANAC_TEXT = labConfig.DEFAULT_ALMANAC_TEXT;
const DEFAULT_ALMANAC_DATE = labConfig.DEFAULT_ALMANAC_DATE;
const DEFAULT_ALMANAC_TIME = labConfig.DEFAULT_ALMANAC_TIME;
const DEFAULT_ALMANAC_ZONE = labConfig.DEFAULT_ALMANAC_ZONE;
const DEFAULT_ALMANAC_LATITUDE = labConfig.DEFAULT_ALMANAC_LATITUDE;
const DEFAULT_ALMANAC_LONGITUDE = labConfig.DEFAULT_ALMANAC_LONGITUDE;
const DEFAULT_ALMANAC_ELEVATION = labConfig.DEFAULT_ALMANAC_ELEVATION;
const DEFAULT_ALMANAC_VISIBILITY = labConfig.DEFAULT_ALMANAC_VISIBILITY;
const ALMANAC_LAND_TOTALITY_SEARCH_TIMEOUT_MS = labConfig.ALMANAC_LAND_TOTALITY_SEARCH_TIMEOUT_MS;
const ALMANAC_WORKSHEET_TITLE = labConfig.ALMANAC_WORKSHEET_TITLE;
const ALMANAC_COVERAGE_TEXT = labConfig.ALMANAC_COVERAGE_TEXT_JS;
const ALMANAC_ACCURACY_NOTE = labConfig.ALMANAC_ACCURACY_NOTE_JS;
if (datetimeJurisdiction && !datetimeJurisdiction.value)
  datetimeJurisdiction.value = DEFAULT_DATETIME_JURISDICTION;
let datetimeAutoGmtOffset = String(datetimeGmtOffset && datetimeGmtOffset.value || DEFAULT_DATETIME_GMT_OFFSET);
let datetimeGmtOffsetTouched = false;
let almanacVisibilityMode = DEFAULT_ALMANAC_VISIBILITY;
let almanacLastWorksheetData = null;
let almanacLandTotalitySequence = 0;
const HOLIDAY_JURISDICTION_SET = new Set(labCatalogue.options.map(row => row[0]));
const JURISDICTION_TOWN_OPTIONS = labCatalogue.towns;
const LAB_MODE_STORAGE_KEY = 'mars.exprLab.lastMode';
const EXPRESSION_TIMESTAMP_STORAGE_KEY = 'mars.exprLab.lastExpressionUpdatedAt';
const EQUATION_TIMESTAMP_STORAGE_KEY = 'mars.exprLab.lastEquationUpdatedAt';
let currentLabMode = 'expression';
let expressionStateSaveTimer = null;
let equationStateSaveTimer = null;
let lastExpressionUpdatedAt = 0;
const modeEditorText = {
  expression: DEFAULT_EXPRESSION_TEXT,
  equation: DEFAULT_EQUATION_TEXT,
  diffequation: DEFAULT_DIFFEQUATION_TEXT,
  matrix: DEFAULT_MATRIX_TEXT,
  integrator: DEFAULT_INTEGRATOR_TEXT,
  datetime: DEFAULT_DATETIME_TEXT,
  almanac: DEFAULT_ALMANAC_TEXT
};
const modeResultState = {
  expression: null,
  equation: null,
  diffequation: null,
  matrix: null,
  integrator: null,
  datetime: null,
  almanac: null
};

marsDateButtons.forEach((button) => {
  button.addEventListener('click', () => {
    const targetId = String(button.dataset.dateTarget || '').trim();
    const input = targetId ? document.getElementById(targetId) : null;
    if (!input)
      return;
    if (marsDatePickerState.input === input && marsDatePicker && !marsDatePicker.classList.contains('hidden')) {
      closeMarsDatePicker({restoreFocus: true});
      return;
    }
    openMarsDatePicker(input, button);
  });
});

if (datetimeGmtOffset) {
  datetimeGmtOffset.addEventListener('input', () => {
    datetimeGmtOffsetTouched = true;
  });
}

if (marsDatePickerPrev)
  marsDatePickerPrev.addEventListener('click', () => shiftMarsDatePickerMonth(-1));
if (marsDatePickerNext)
  marsDatePickerNext.addEventListener('click', () => shiftMarsDatePickerMonth(1));
if (marsDatePickerYearDown)
  marsDatePickerYearDown.addEventListener('click', (event) => shiftMarsDatePickerYear(-marsDatePickerYearStep(event)));
if (marsDatePickerYearUp)
  marsDatePickerYearUp.addEventListener('click', (event) => shiftMarsDatePickerYear(marsDatePickerYearStep(event)));
if (marsDatePickerMonth)
  marsDatePickerMonth.addEventListener('change', () => {
    setMarsDatePickerMonthYear(marsDatePickerState.year, marsDatePickerMonth.value, {commit: true});
  });
if (marsDatePickerYear) {
  marsDatePickerYear.addEventListener('change', () => {
    setMarsDatePickerMonthYear(marsDatePickerYear.value, marsDatePickerState.month, {commit: true});
  });
  marsDatePickerYear.addEventListener('keydown', (event) => {
    if (event.key === 'Enter') {
      event.preventDefault();
      setMarsDatePickerMonthYear(marsDatePickerYear.value, marsDatePickerState.month, {commit: true});
    }
  });
}
if (marsDatePickerToday)
  marsDatePickerToday.addEventListener('click', () => {
    if (!marsDatePickerState.input)
      return;
    commitMarsTodayValue(marsDatePickerState.input);
    closeMarsDatePicker({restoreFocus: true});
  });
if (marsDatePickerClose)
  marsDatePickerClose.addEventListener('click', () => closeMarsDatePicker({restoreFocus: true}));

document.addEventListener('click', (event) => {
  if (!marsDatePicker || marsDatePicker.classList.contains('hidden'))
    return;
  if (marsDatePicker.contains(event.target))
    return;
  if (event.target.closest && event.target.closest('.mars-date-shell'))
    return;
  closeMarsDatePicker();
});

window.addEventListener('resize', () => {
  if (marsDatePicker && !marsDatePicker.classList.contains('hidden'))
    placeMarsDatePicker(marsDatePickerState.shell);
});

let editorResizeFrame = 0;

const MARS_DATE_WEEKDAYS = ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun'];
const MARS_DATE_MONTHS = [
  'January', 'February', 'March', 'April', 'May', 'June',
  'July', 'August', 'September', 'October', 'November', 'December'
];
const MARS_DATE_MIN_YEAR = 1;
const MARS_DATE_MAX_YEAR = 9999;
const marsDatePickerState = {
  input: null,
  button: null,
  shell: null,
  year: 0,
  month: 0
};

if (typeof ResizeObserver === 'function') {
  const renderedTeXResizeObserver = new ResizeObserver(
    scheduleRenderedTeXFit
  );
  renderedTeXResizeObserver.observe(rendered);
  const solverResizeObserver = new ResizeObserver(scheduleSolverTexFit);
  solverResizeObserver.observe(functionStyle);
} else {
  window.addEventListener('resize', scheduleRenderedTeXFit);
  window.addEventListener('resize', scheduleSolverTexFit);
}
