/**
 * Test-only adapters for native browser policies, injected into the private browser fixture.
 * Production scripts call these C policies directly. Keep convenience wrappers here so
 * regression checks can exercise them without shipping unused adapters to users.
 */

let integratorCandidateNames;
let DEFAULT_EQUATION_VARIABLE_TEXT;

function initialise_lab_test_adapters() {
    integratorCandidateNames = Object.freeze(labDOM.call('lab_binding_row_names'));
    DEFAULT_EQUATION_VARIABLE_TEXT = labConfig.DEFAULT_EQUATION_VARIABLE;
}

// This copies structured DOM fields only. Native /forms owns all text interpretation.
function sanitizeIntegratorRow(row, fallbackName = 'x') {
    return labDOM.call('lab_binding_row_normalise', row, String(fallbackName));
}

function integratorDefaultVariableName(rows = []) {
    return labDOM.call('lab_binding_rows_name', rows);
}

function integratorRowText(row) {
    return integratorBoundsTextFromRows([row]);
}

async function parseIntegratorBoundsText(text) {
    return labFlowContinue(74, {text});
}

function integratorFallbackRows() {
    return labDOM.call('lab_binding_rows_default', 0);
}

function precisionStatusText() {
    return labDOM.call('lab_workspace_dom_precision');
}

function showHelp() {
    labDOM.call('lab_workspace_dom_help', 1);
}

function showTargetEntry() {
    labDOM.call('lab_workspace_dom_target', 1);
}

const EXPRESSION_TIMESTAMP_STORAGE_KEY = 'mars.exprLab.lastExpressionUpdatedAt';

const hasAbbreviatedValue = value => value === true;

const resultZoomIndex = card => labWire.exports().lab_view_card_zoom(resultCardIds.get(card) ?? -1);

const applyResultZoom = card => labDOM.call('lab_layout_zoom', card);

const setResultZoom = (card, index) => labDOM.call('lab_layout_set_zoom', card, index, 0);

const stepResultZoom = (card, direction) => labDOM.call('lab_layout_set_zoom', card, direction, 1);

const renderResultCardExpansion = () => labDOM.call('lab_layout_expand', null, 0);

const resultCardExpanded = card => labWire.exports().lab_view_card_expanded() === resultCardIds.get(card);

const collapseResultCards = () => labDOM.call('lab_layout_expand', null, 2);

const toggleResultCardExpansion = button => labDOM.call('lab_layout_expand', button.closest('.result-card'), 1);

const svgMarkupIntrinsicWidth = markup => labDOM.call('lab_layout_markup_width', markup);

const installSolverTexSvg = (markup, variant) => labDOM.call('lab_layout_solver_install', markup, variant);

function parsedExpressionText() {
    return labDOM.call('lab_result_copy', 'expression', lastTex);
}

function almanacWorksheetCopyText(data, visibility) {
    return almanacPresentationVariant(data, visibility).copy_text;
}

function evaluationLabel(mode, field) {
    return labDOM.call('lab_evaluation_label', mode, field);
}

function validDateText(value, fallback = DEFAULT_DATETIME_DATE) {
    return labDOM.call('lab_location_validate', 1, {value}, fallback, HOLIDAY_JURISDICTION_SET);
}

function validDatetimeJurisdiction(value, fallback = DEFAULT_DATETIME_JURISDICTION) {
    return labDOM.call('lab_location_validate', 2, {value}, fallback, HOLIDAY_JURISDICTION_SET);
}

function townValueParts(value) {
    return labFlowContinue(26, {action: 1, value});
}

function townOptionMatchesValue(option, value) {
    return labFlowContinue(26, {action: 2, option, value});
}

function numbersNearlyEqual(left, right, tolerance = 0.000001) {
    return Boolean(labWire.exports().lab_forms_nearly_equal(marsFormNumber(left), marsFormNumber(right), tolerance));
}

function calendarSchema(mode) {
    if (!calendarSchemas.has(mode))
        calendarSchemas.set(mode, labDOM.call('lab_location_schema', workspaceModeId(mode)));
    return calendarSchemas.get(mode);
}

function validAlmanacVisibility(value, fallback = DEFAULT_ALMANAC_VISIBILITY) {
    return labDOM.call('lab_location_validate', 3, {value}, fallback, HOLIDAY_JURISDICTION_SET);
}

function marsDatePickerAnchorRect(shell) {
    return labDOM.call('lab_widgets_picker_anchor', shell);
}

const calendarSchemas = new Map();

function validPrecisionBits(bits, fallback) {
    return labDOM.call('lab_storage_precision', [bits, fallback]);
}

function restoreCompactBindingValues(text) {
    return labDOM.call('lab_editor_metadata', 2, [text], labPresentationEditors);
}

function almanacPresentationVariant(data, visibility) {
    const variant = labDOM.call('lab_result_almanac_variant', data, visibility);
    if (!variant)
        throw new Error(
            'Almanac presentation is missing from the server response. ' +
            'Run make mars-lab-restart, reload the page and evaluate again.');
    return variant;
}
