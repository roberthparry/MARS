/**
 * Browser timing and Intl adapters for C/WASM calendar location and control projection.
 * Definition-only client script; app.js loads it before shared worksheet state.
 */

function validDateText(value, fallback = DEFAULT_DATETIME_DATE) {
    return labDOM.call('lab_location_validate', 1, {value}, fallback, HOLIDAY_JURISDICTION_SET);
}

function formatAlmanacTimeInput(value) {
    return labFlowContinue(26, {action: 0, value});
}

function validDatetimeJurisdiction(value, fallback = DEFAULT_DATETIME_JURISDICTION) {
    return labDOM.call('lab_location_validate', 2, {value}, fallback, HOLIDAY_JURISDICTION_SET);
}

function townsForJurisdiction(jurisdiction) {
    return labDOM.call('lab_location_towns', calendarLocationContext(), {value: jurisdiction});
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

function syncRoundedSelect(select) {
    const capabilities = {
        get rebuild() { return typeof select.__marsRebuildRoundedSelect === 'function'; },
        get sync() { return typeof select.__marsSyncRoundedSelect === 'function'; }
    };
    labDOM.services(labDOM.call('lab_flow_location_select', select, capabilities), labLocationFlowServices());
}

function selectTownByCoordinates(select, latitude, longitude) {
    return Boolean(labDOM.call('lab_location_coordinates', select, {latitude, longitude}));
}

function restoreTownSelection(select, jurisdiction, townValue, latitude, longitude, isCurrent = () => true) {
    return labFlowContinue(27, {select, jurisdiction, selection: {value: townValue, latitude, longitude}, isCurrent});
}

function populateTownSelect(select, jurisdiction, {selectDefault = true, isCurrent = () => true} = {}) {
    return labFlowContinue(28, {select, jurisdiction, selectDefault, isCurrent});
}

function selectedTownOption(select) {
    return labDOM.call('lab_location_selected', select);
}

function clearTownForCustomCoordinates(townSelect, latitudeInput, longitudeInput, elevationInput) {
    labDOM.call('lab_location_clear_custom', townSelect, latitudeInput, longitudeInput, elevationInput);
}

function timeZoneOffsetHours(timeZone, dateText) {
    if (!timeZone)
        return null;
    const parsed = parseMarsIsoDate(dateText);
    if (!parsed)
        return null;
    const forms = labWire.exports();
    const probe = new Date(forms.lab_forms_utc_milliseconds(parsed.year, parsed.month, parsed.day, 12, 0, 0));
    if (Number.isNaN(probe.getTime()))
        return null;
    try {
        const formatter = new Intl.DateTimeFormat('en-GB', {
            timeZone,
            hour12: false,
            year: 'numeric',
            month: '2-digit',
            day: '2-digit',
            hour: '2-digit',
            minute: '2-digit',
            second: '2-digit'
        });
        const parts = Object.fromEntries(formatter.formatToParts(probe).map((part) => [part.type, part.value]));
        const localAsUtc = forms.lab_forms_utc_milliseconds(
            marsFormNumber(parts.year), marsFormNumber(parts.month), marsFormNumber(parts.day),
            marsFormNumber(parts.hour), marsFormNumber(parts.minute), marsFormNumber(parts.second));
        const offset = forms.lab_forms_offset_hours(localAsUtc, probe.getTime());
        return Number.isFinite(offset) ? offset : null;
    } catch (_) {
        return null;
    }
}

function formatOffsetHours(offset) {
    if (offset === null || !Number.isFinite(offset))
        return '';
    const rounded = labWire.exports().lab_forms_round_offset(offset);
    return Number.isFinite(rounded) ? String(rounded) : '';
}

function applySelectedTown(controls = {}) {
    const option = selectedTownOption(controls.townSelect);
    const offset = formatOffsetHours(timeZoneOffsetHours(option?.dataset.timezone, controls.dateInput?.value));
    const context = calendarLocationContext();
    const applied = labDOM.call('lab_location_apply_town', controls, offset, context);
    acceptCalendarLocationContext(context);
    return Boolean(applied);
}

function syncTownSelectors({selectDefault = false} = {}) {
    return labFlowContinue(29, {selectDefault});
}

const calendarSchemas = new Map();

function calendarLocationContext() {
    return {
        config: labConfig,
        jurisdictions: HOLIDAY_JURISDICTION_SET,
        towns: JURISDICTION_TOWN_OPTIONS,
        visibility: almanacVisibilityMode,
        automaticOffset: datetimeAutoGmtOffset,
        offsetTouched: datetimeGmtOffsetTouched
    };
}

function acceptCalendarLocationContext(context) {
    almanacVisibilityMode = context.visibility;
    datetimeAutoGmtOffset = context.automaticOffset;
    datetimeGmtOffsetTouched = context.offsetTouched;
}

function calendarSchema(mode) {
    if (!calendarSchemas.has(mode))
        calendarSchemas.set(mode, labDOM.call('lab_location_schema', workspaceModeId(mode)));
    return calendarSchemas.get(mode);
}

function readCalendarControls(mode) {
    return labDOM.call('lab_location_read', workspaceModeId(mode), calendarLocationContext());
}

function calendarState(mode, source, operation, prefix = '') {
    const state = labDOM.call(
        'lab_location_state', workspaceModeId(mode), operation, source, prefix, calendarLocationContext(), 0);
    if (!state)
        throw new Error('Invalid calendar state operation');
    return state;
}

function applyCalendarState(mode, source, operation = 0, prefix = '') {
    const context = calendarLocationContext();
    const state = labDOM.call('lab_location_state', workspaceModeId(mode), operation, source, prefix, context, 1);
    if (!state)
        throw new Error('Invalid calendar state operation');
    acceptCalendarLocationContext(context);
    return state;
}

function restoreCalendarHistory(mode, source, isCurrent = () => true) {
    return labFlowContinue(30, {mode, source, isCurrent});
}

function calendarElements(mode) {
    return labDOM.call('lab_location_elements', workspaceModeId(mode));
}

function calendarTownControls(mode, coordinates = true) {
    return labDOM.call('lab_location_controls', workspaceModeId(mode), Number(coordinates));
}

function restoreDatetimeDefaultsIfBlank() {
    applyCalendarState('datetime', readCalendarControls('datetime'), 2);
}

function restoreAlmanacDefaultsIfBlank() {
    applyCalendarState('almanac', readCalendarControls('almanac'), 2);
}

function currentDatetimeState() {
    return currentCalendarState('datetime');
}

function currentAlmanacState() {
    return currentCalendarState('almanac');
}

function currentCalendarState(mode) {
    const context = calendarLocationContext();
    const state = labDOM.call('lab_location_current', workspaceModeId(mode), context);
    acceptCalendarLocationContext(context);
    return state;
}

function validAlmanacVisibility(value, fallback = DEFAULT_ALMANAC_VISIBILITY) {
    return labDOM.call('lab_location_validate', 3, {value}, fallback, HOLIDAY_JURISDICTION_SET);
}

function almanacSummaryText(state = currentAlmanacState()) {
    return labDOM.call('lab_location_summary', 6, state, ALMANAC_WORKSHEET_TITLE);
}

function datetimeSummaryText(state = currentDatetimeState()) {
    return labDOM.call('lab_location_summary', 5, state, '');
}

function applyCalendarEvaluationFields(mode, data) {
    const context = calendarLocationContext();
    labDOM.call('lab_location_evaluation', workspaceModeId(mode), data, context);
    acceptCalendarLocationContext(context);
}

function setDatetimeLocalText(text, sections = null) {
    labDOM.services(
        labDOM.call('lab_flow_location_local', {value: text}, sections, datetimeLocalBody, datetimeLocal),
        labLocationFlowServices());
}

function applyAlmanacTotalityAction(button) {
    return labFlowContinue(31, {button});
}

function refreshCalendarJurisdictionLocation(mode, {updateCoordinates = true} = {}) {
    return labFlowContinue(32, {mode, updateCoordinates});
}

function refreshDatetimeJurisdictionLocation(options) {
    return refreshCalendarJurisdictionLocation('datetime', options);
}

function refreshAlmanacJurisdictionLocation(options) {
    return refreshCalendarJurisdictionLocation('almanac', options);
}

function triggerDatetimeAutoEvaluation({refreshJurisdiction = false, refreshCoordinates = false} = {}) {
    return labFlowContinue(33, {mode: 'datetime', options: {refreshJurisdiction, refreshCoordinates}});
}

function triggerAlmanacAutoEvaluation({refreshJurisdiction = false, refreshCoordinates = false} = {}) {
    return labFlowContinue(33, {mode: 'almanac', options: {refreshJurisdiction, refreshCoordinates}});
}

/** Format time through native services, retaining request and authored-input ownership. */
function formatAlmanacTimeFromEvent() {
    return labFlowContinue(34, {});
}

/** Deliver native calendar actions to asynchronous location and evaluation services. */
function applyCalendarControlEvent(mode, actions) {
    void labFlowContinue(35, {mode, actions});
}

// Resolve browser-owned controls and application callbacks only when a native continuation executes them.
let labLocationRegistry;
function labLocationFlowServices() {
    return labLocationRegistry ||= Object.freeze({
        locForms: request => requestLabForms(request),
        locCurrent: callback => callback(),
        locAwait: promise => promise,
        locTowns: jurisdiction => townsForJurisdiction(jurisdiction),
        locPopulate: (...args) => populateTownSelect(...args),
        locRestore: (...args) => restoreTownSelection(...args),
        locApplyTown: controls => applySelectedTown(controls),
        locElements: mode => calendarElements(mode),
        locControls: (...args) => calendarTownControls(...args),
        locApplyState: (...args) => applyCalendarState(...args),
        locState: (...args) => calendarState(...args),
        locRead: mode => readCalendarControls(mode),
        locContext: () => calendarLocationContext(),
        locAcceptContext: context => acceptCalendarLocationContext(context),
        locBegin: (...args) => labRequests.begin(...args),
        locFinish: request => labRequests.finish(request),
        locRestoreTotality: (state, request) => restoreTownSelection(
            almanacTown, state.jurisdiction, state.town, state.latitude, state.longitude,
            () => labRequests.current(request)),
        locSaveAlmanac: () => saveLastAlmanacState(),
        locSaveDatetime: () => saveLastDatetimeState(),
        locEvaluateCurrent: () => evaluateCurrentMode(),
        locStatus: status => setStatus(status),
        locError: message => setRenderedError(message),
        locCoordinates: (...args) => selectTownByCoordinates(...args),
        locResponse: (mode, data, context, update, applied) => labDOM.call(
            'lab_location_response', workspaceModeId(mode), data, context, Number(update), Number(!!applied)),
        locClearAlmanac: () => almanacLastWorksheetData = null,
        locRefreshAlmanac: options => refreshAlmanacJurisdictionLocation(options),
        locRefreshDatetime: options => refreshDatetimeJurisdictionLocation(options),
        locEvaluateAlmanac: options => evaluateAlmanac(options),
        locEvaluateDatetime: options => evaluateDatetime(options),
        locTriggerAlmanac: options => triggerAlmanacAutoEvaluation(options),
        locTriggerDatetime: options => triggerDatetimeAutoEvaluation(options),
        locTimeSnapshot: () => ({authored: almanacTime.value, context: labRequests.context(),
                                main: labRequests.latestMain(), element: almanacTime}),
        locLatestMain: () => labRequests.latestMain(),
        locFormatTime: text => formatAlmanacTimeInput(text),
        locTimeApply: (input, text) => {
            input.value = text;
            input.setSelectionRange(text.length, text.length);
        },
        locClearCustom: (...args) => clearTownForCustomCoordinates(...args),
        locRenderLocal: (node, sections, text) => renderDatetimeSections(node, null, sections, text),
        locLocalVisible: card => labDOM.call('lab_flow_location_visibility', card, currentMode()),
        locLocalHidden: card => card.classList.add('hidden'),
        locSelectRebuild: select => select.__marsRebuildRoundedSelect(),
        locSelectSync: select => select.__marsSyncRoundedSelect(),
        locRenderPicker: () => renderMarsDatePicker()
    });
}


/** DOM event delivery for menus whose projection and keyboard policy live in C/WASM. */
function enhanceRoundedSelect(select, options = {}) {
    if (!labDOM.call('lab_select_enhance', select, options, document))
        return null;
    const sync = () => labDOM.call('lab_select_sync', select);
    const close = () => labDOM.call('lab_select_event', select, 3, 0, null);
    select.__marsSyncRoundedSelect = sync;
    select.__marsRebuildRoundedSelect = () => labDOM.call('lab_select_refresh', select, options);
    return {sync, close};
}

// Browser date conversion and clock access; C owns picker policy and DOM projection.
// Browser form controls perform text conversion; C receives structured numbers only.
function marsFormNumber(value) {
    if (typeof value === 'number')
        return value;
    const input = document.createElement('input');
    input.type = 'number';
    input.value = String(value ?? '').trim();
    return input.valueAsNumber;
}

function parseMarsIsoDate(text) {
    const input = document.createElement('input');
    input.type = 'date';
    input.value = String(text || '').trim();
    const date = input.valueAsDate;
    if (!date)
        return null;
    const year = date.getUTCFullYear(), month = date.getUTCMonth() + 1, day = date.getUTCDate();
    return labWire.exports().lab_forms_date_valid(year, month, day) ? {year, month, day} : null;
}

function marsIsoDate(year, month, day) {
    return `${String(year).padStart(4, '0')}-${String(month).padStart(2, '0')}-${String(day).padStart(2, '0')}`;
}

function marsClockTime(hours, minutes, seconds) {
    return `${String(hours).padStart(2, '0')}:${String(minutes).padStart(2, '0')}:${String(seconds).padStart(2, '0')}`;
}

function marsTodayIsoDate() {
    const now = new Date();
    return marsIsoDate(now.getFullYear(), now.getMonth() + 1, now.getDate());
}

function marsCurrentGmtMoment(now = new Date()) {
    return {
        date: marsIsoDate(now.getUTCFullYear(), now.getUTCMonth() + 1, now.getUTCDate()),
        time: marsClockTime(now.getUTCHours(), now.getUTCMinutes(), now.getUTCSeconds())
    };
}

function marsTodayIsoDateForInput(input) {
    return labDOM.call('lab_widgets_picker_today_date', input, marsTodayIsoDate(), marsCurrentGmtMoment().date);
}


function closeMarsDatePicker({restoreFocus = false} = {}) {
    labDOM.call('lab_widgets_picker_close', marsDatePickerState, Number(restoreFocus));
}

function marsDatePickerAnchorRect(shell) {
    return labDOM.call('lab_widgets_picker_anchor', shell);
}

function placeMarsDatePicker(shell) {
    labDOM.call('lab_widgets_picker_place', marsDatePickerState, shell, window.innerWidth, window.innerHeight);
}

function commitMarsDateValue(input, value) {
    return !!labDOM.call('lab_widgets_date_commit', input, String(value));
}

function commitMarsTodayValue(input) {
    labDOM.call('lab_widgets_picker_today', input, marsTodayIsoDate(), marsCurrentGmtMoment());
}

function renderMarsDatePicker() {
    const input = marsDatePickerState.input;
    labDOM.call(
        'lab_widgets_picker_render', marsDatePickerState, parseMarsIsoDate(input?.value),
        parseMarsIsoDate(marsTodayIsoDateForInput(input)), window.innerWidth, window.innerHeight);
}

function openMarsDatePicker(input, button) {
    labDOM.call(
        'lab_widgets_picker_open', marsDatePickerState, input, button, parseMarsIsoDate(input?.value),
        parseMarsIsoDate(marsTodayIsoDateForInput(input)), new Date().getFullYear(), window.innerWidth,
        window.innerHeight);
}

function setMarsDatePickerMonthYear(year, month, {commit = false} = {}) {
    const moved = labDOM.call(
        'lab_widgets_picker_move', marsDatePickerState, 0, marsFormNumber(year), marsFormNumber(month),
        parseMarsIsoDate(marsDatePickerState.input?.value), new Date().getFullYear(), Number(!!commit));
    labDOM.services(labDOM.call('lab_flow_location_picker', Number(!!moved)), labLocationFlowServices());
}

function shiftMarsDatePicker(delta, years) {
    const moved = labDOM.call(
        'lab_widgets_picker_move', marsDatePickerState, 1 + Number(!!years), delta, 0,
        parseMarsIsoDate(marsDatePickerState.input?.value), new Date().getFullYear(), 1);
    labDOM.services(labDOM.call('lab_flow_location_picker', Number(!!moved)), labLocationFlowServices());
}

function shiftMarsDatePickerYear(delta) {
    shiftMarsDatePicker(delta, true);
}

function shiftMarsDatePickerMonth(delta) {
    shiftMarsDatePicker(delta, false);
}
