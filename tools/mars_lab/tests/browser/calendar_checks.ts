/**
 * @file calendar_checks.ts
 * @brief Calendar schema, capture, restoration and event-policy regressions.
 *
 * Uses native calendar metadata and browser control fixtures to verify state handling and persistence without reinterpreting calendar policy.
 */

/** @brief Exercise calendar projection with a host containing no editor or result session state. */
function checkCalendarHostPort(): void {
    const host = {
        HOLIDAY_JURISDICTION_SET: WorksheetContext.HOLIDAY_JURISDICTION_SET,
        JURISDICTION_TOWN_OPTIONS: WorksheetContext.JURISDICTION_TOWN_OPTIONS,
        ALMANAC_WORKSHEET_TITLE: WorksheetContext.ALMANAC_WORKSHEET_TITLE,
        worksheetSession: {
            calendar: {almanacVisibilityMode: 'visible', datetimeAutoGmtOffset: '1', datetimeGmtOffsetTouched: false}
        }
    };
    const controller = new WorksheetLocations.CalendarStateController({
        context() { return host; },
        mode() { return WorksheetWorkspace.mode; },
        configuration() { return labApplication.configuration; },
        native() { return labDOM; }
    });
    const capture = controller.calendarLocationContext;
    const accept = controller.acceptCalendarLocationContext;
    const mark = controller.markDatetimeOffsetTouched;
    const snapshot = capture();
    if (snapshot.visibility !== 'visible' || snapshot.automaticOffset !== '1' || snapshot.offsetTouched)
        throw new Error('Calendar port: capture must read the supplied calendar-only state');
    host.worksheetSession.calendar = {
        almanacVisibilityMode: 'all', datetimeAutoGmtOffset: '-3.5', datetimeGmtOffsetTouched: false
    };
    mark();
    const current = capture();
    if (current.visibility !== 'all' || current.automaticOffset !== '-3.5' || !current.offsetTouched)
        throw new Error('Calendar port: detached callbacks must resolve the current calendar state');
    accept(snapshot);
    const restored = host.worksheetSession.calendar;
    if (restored.almanacVisibilityMode !== 'visible' || restored.datetimeAutoGmtOffset !== '1' ||
        restored.datetimeGmtOffsetTouched)
        throw new Error('Calendar port: accepting context must update all three flags');
}

/** @brief Calendar controls retained for restoration after the fixture. */
interface LabCalendarSnapshot {
    mode: 'datetime' | 'almanac';
    state: WorksheetCalendarState;
}

/** @brief One local or server publication in the order it was observed. */
interface LabCalendarStoredEntry {
    key?: string;
    payload: WorksheetCalendarState;
}

/** @brief Value-bearing controls addressed by the native calendar schema. */
interface LabCalendarInputElements {
    [name: string]: HTMLInputElement;
}

/** @brief Check calendar schema, restoration, capture and event policy against real C/WASM. */
window.checkLabCalendarState = async function checkLabCalendarState() {
    checkCalendarHostPort();
    const native = labWire.exports();
    function check(ok: unknown, message: string): void {
        if (!ok)
            throw new Error('Calendar state: ' + message);
    }
    function equal(actual: unknown, expected: unknown, message: string): void {
        check(Object.is(actual, expected), message + ': ' + actual + ' != ' + expected);
    }
    const savedMode = WorksheetWorkspace.mode.currentMode();
    const savedOffset = WorksheetContext.worksheetSession.calendar.datetimeAutoGmtOffset;
    const savedTouched = WorksheetContext.worksheetSession.calendar.datetimeGmtOffsetTouched;
    const calendarModes = ['datetime', 'almanac'] as const;
    const snapshots: LabCalendarSnapshot[] = [];
    for (const mode of calendarModes) {
        snapshots.push({mode, state: WorksheetLocations.calendar.readCalendarControls(mode)});
    }
    const savedTriggers = {
        datetime: WorksheetLocations.calendarActions.triggerDatetimeAutoEvaluation,
        almanac: WorksheetLocations.calendarActions.triggerAlmanacAutoEvaluation
    };
    const savedStore = WorksheetPersistence.persistence.publish;
    const savedLocalStore = WorksheetPersistence.persistence.writeLocal;
    const savedEditors = {
        datetime: WorksheetContext.modeEditorText.datetime,
        almanac: WorksheetContext.modeEditorText.almanac
    };
    const events: [string, WorksheetAutoEvaluationOptions][] = [];
    const stored: LabCalendarStoredEntry[] = [];
    try {
        const services = WorksheetLocations.locationServices;
        check(Object.isFrozen(services), 'location service catalogue stays immutable');
        for (const service of ['locCurrent', 'locTimeApply', 'locState', 'locRenderPicker']) {
            check(Object.hasOwn(services, service), 'native dispatch can resolve own service: ' + service);
        }
        const timeInput = document.createElement('input');
        timeInput.type = 'text';
        const applyTime = services.locTimeApply;
        applyTime(timeInput, '03:04:05');
        equal(timeInput.value, '03:04:05', 'detached time service writes its supplied input');
        equal(timeInput.selectionStart, 8, 'time service places the caret after the complete value');
        equal(timeInput.selectionEnd, 8, 'time service collapses the selection');
        const retired = await labHttp.fetch('/js/almanac.js');
        check(
            retired.status === 404 && !labApplication.definitionScripts.includes('almanac'),
            'retired almanac script remains served or loaded');
        equal(native.lab_browser_abi_version(), 47, 'current browser ABI is required');
        equal(native.lab_browser_message_limit(), 8388608, 'native message limit is 8 MiB');
        equal(native.lab_browser_member_limit(), 65536, 'native member bound preserves container admission');
        equal(native.lab_browser_handle_capacity(), 131073, 'native handle capacity preserves host admission');
        equal(native.lab_browser_base64_limit(), 11184812, 'native base64 bound includes final padding');
        equal(native.lab_profile_count(5), 11, 'DateTime field count');
        equal(native.lab_profile_count(6), 9, 'almanac field count');
        for (const mode of [0, 4, 7, -1]) {
            equal(native.lab_profile_count(mode), 0, 'invalid mode has no schema');
            equal(native.lab_profile_choose(mode, 0, 0, 1, 1, 1), -1, 'invalid mode cannot select text');
            equal(native.lab_profile_write(mode, 0, 0), 0, 'invalid mode cannot write');
            equal(native.lab_profile_event(mode, 0), 0, 'invalid mode has no event actions');
        }
        for (const mode of [5, 6]) {
            for (const field of [native.lab_profile_count(mode), 999, -1]) {
                equal(native.lab_profile_text_length(mode, field, 0), 0, 'invalid field has no key');
                equal(native.lab_profile_choose(mode, field, 0, 1, 1, 1), -1, 'invalid field rejected');
                equal(native.lab_profile_write(mode, field, 3), 0, 'invalid field not writable');
                equal(native.lab_profile_event(mode, field), 0, 'invalid field has no actions');
            }
            equal(native.lab_profile_choose(mode, 0, 4, 1, 1, 1), -1, 'invalid operation rejected');
        }
        const expectedKeys = [
            'date,jdn,start,end,year,jurisdiction,town,latitude,longitude,elevation,gmt_offset',
            'date,time,zone,jurisdiction,town,latitude,longitude,elevation,visibility'
        ];
        for (let index = 0; index < calendarModes.length; ++index) {
            const mode = calendarModes[index];
            equal(calendarSchema(mode).map(field => field.key).join(','), expectedKeys[index], 'native schema order');
            check(
                calendarSchema(mode).every((field: {key: string; element: string}) => !field.element || document.getElementById(field.element)!),
                'schema refers to missing controls');
        }
        for (let flags = 0; flags < 32; ++flags) {
            const town = (flags & 1) !== 0;
            const touched = (flags & 2) !== 0;
            const present = (flags & 4) !== 0;
            const automatic = (flags & 8) !== 0;
            const suggested = (flags & 16) !== 0;
            equal(
                native.lab_profile_accept_offset(5, Number(town), Number(touched), Number(present), Number(automatic), Number(suggested)),
                Number(!town && (!touched || !present || automatic)), 'DateTime offset ownership');
            equal(
                native.lab_profile_accept_offset(6, Number(town), Number(touched), Number(present), Number(automatic), Number(suggested)),
                Number(!town && suggested), 'almanac offset ownership');
        }
        const raw = {
            date: ' 2024-02-29 ',
            start: 'bad',
            end: '2024-03-02',
            year: ' 2028 ',
            jdn: ' 2460000 ',
            jurisdiction: WorksheetContext.DEFAULT_DATETIME_JURISDICTION,
            latitude: ' 51 ',
            longitude: ' -2 ',
            elevation: ' 40 ',
            gmt_offset: ' 5.5 ',
            town: ' μ|51|-2|40 '
        };
        const restored = WorksheetLocations.calendar.calendarState('datetime', raw, 0);
        equal(restored.date, '2024-02-29', 'restore validates date');
        equal(restored.start, restored.date, 'range falls back to resolved date');
        equal(restored.year, ' 2028 ', 'restoration preserves authored year whitespace');
        equal(restored.latitude, ' 51 ', 'restoration preserves authored coordinate whitespace');
        const prefixed: WorksheetCalendarState = {};
        for (const [key, value] of Object.entries(raw)) {
            prefixed['datetime_' + key] = value;
        }
        equal(
            JSON.stringify(WorksheetLocations.calendar.calendarState('datetime', prefixed, 0, 'datetime_')), JSON.stringify(restored),
            'server and history restoration share policy');
        const captured = WorksheetLocations.calendar.calendarState('datetime', raw, 1);
        equal(captured.jdn, '2460000', 'capture trims JDN');
        equal(captured.latitude, '51', 'capture trims coordinate');
        equal(captured.gmt_offset, '5.5', 'explicit offset retained');
        equal(WorksheetLocations.calendar.calendarState('datetime', {}, 1).gmt_offset, '', 'capture has no forced GMT offset');
        equal(WorksheetLocations.calendar.calendarState('datetime', {date: '2024-02-29'}, 0).year, '2024', 'year fallback follows date');
        equal(
            WorksheetLocations.calendar.calendarState('datetime', {date: '2023-02-29'}, 0).date, WorksheetContext.DEFAULT_DATETIME_DATE,
            'non-leap date uses configured fallback');
        const almanac = WorksheetLocations.calendar.calendarState('almanac', {time: '   ', zone: ' 2 ', visibility: ' VISIBLE '}, 0);
        equal(almanac.time, WorksheetContext.DEFAULT_ALMANAC_TIME, 'blank restored time uses default');
        equal(almanac.zone, '2', 'restored almanac zone trimmed');
        equal(almanac.visibility, 'visible', 'visibility normalised');
        equal(WorksheetLocations.calendar.calendarState('almanac', {time: '   '}, 1).time, '', 'capture retains incomplete authored time');

        WorksheetLocations.calendar.applyCalendarState('datetime', raw);
        WorksheetContext.worksheetSession.calendar.datetimeGmtOffsetTouched = true;
        equal(WorksheetLocations.calendar.currentDatetimeState().gmt_offset, '', 'unchanged automatic offset omitted');
        WorksheetContext.datetimeGmtOffset.value = '6';
        equal(WorksheetLocations.calendar.currentDatetimeState().gmt_offset, '6', 'manually changed offset included');
        WorksheetContext.datetimeStart.value = '';
        WorksheetLocations.calendar.restoreDatetimeDefaultsIfBlank();
        equal(WorksheetContext.datetimeStart.value, WorksheetContext.datetimeDate.value, 'blank range control follows date');
        const previousTown = WorksheetContext.datetimeTown.value;
        const previousAlmanacJurisdiction = WorksheetContext.almanacJurisdiction.value;
        WorksheetLocations.calendar.applyCalendarState('datetime', {}, 3);
        equal(WorksheetContext.datetimeDate.value, WorksheetContext.DEFAULT_DATETIME_DATE, 'clear resets date');
        equal(WorksheetContext.datetimeJdn.value, '', 'clear resets JDN');
        equal(WorksheetContext.datetimeTown.value, previousTown, 'clear preserves selected town');
        check(!WorksheetContext.worksheetSession.calendar.datetimeGmtOffsetTouched, 'clear resets offset ownership');
        WorksheetLocations.calendar.applyCalendarState('almanac', {}, 3);
        equal(WorksheetContext.almanacJurisdiction.value, previousAlmanacJurisdiction, 'almanac clear preserves jurisdiction');
        equal(WorksheetContext.worksheetSession.calendar.almanacVisibilityMode, WorksheetContext.DEFAULT_ALMANAC_VISIBILITY, 'clear resets visibility');

        Reflect.set(WorksheetLocations.calendarActions, 'triggerDatetimeAutoEvaluation', (options: WorksheetAutoEvaluationOptions) => events.push(['datetime', options]));
        Reflect.set(WorksheetLocations.calendarActions, 'triggerAlmanacAutoEvaluation', (options: LabTestRecord) => events.push(['almanac', options]));
        for (const mode of ['datetime', 'almanac'] as const) {
            WorksheetWorkspace.mode.setMode(mode);
            const elements = WorksheetLocations.calendar.calendarElements(mode) as LabCalendarInputElements;
            elements.date.value = '2024-02-29';
            elements.date.dispatchEvent(new Event('change'));
            equal(events.at(-1)![0], mode, 'date dispatch uses owning mode');
            check(
                events.at(-1)![1].refreshJurisdiction && !events.at(-1)![1].refreshCoordinates,
                'date refresh preserves coordinates');
            elements.jurisdiction.dispatchEvent(new Event('change'));
            check(events.at(-1)![1].refreshCoordinates, 'jurisdiction change refreshes coordinates');
            if (mode === 'almanac') {
                for (const field of ['time', 'zone', 'latitude', 'longitude', 'elevation']) {
                    const before = events.length;
                    elements[field].dispatchEvent(new Event('change'));
                    equal(events.length, before + 1, `${field} change initiates one automatic evaluation`);
                    equal(events.at(-1)![0], 'almanac', `${field} change evaluates Almanac`);
                    check(!events.at(-1)![1].refreshCoordinates, `${field} change preserves observer coordinates`);
                }
            }
            if (mode === 'datetime') {
                equal(elements.year.value, '2024', 'date change updates year');
                equal(elements.jdn.value, '', 'date change clears JDN');
                elements.start.dispatchEvent(new Event('change'));
                check(!events.at(-1)![1].refreshJurisdiction, 'range change does not refresh jurisdiction');
            }
        }
        /** @brief Record a server publication in the common publication sequence. */
        function recordPublished(payload: WorksheetCalendarState): number {
            return stored.push({payload});
        }

        /** @brief Record a local publication together with its storage key. */
        function recordLocal(key: string, payload: WorksheetCalendarState): number {
            return stored.push({key, payload});
        }

        Reflect.set(WorksheetPersistence.persistence, 'publish', recordPublished);
        Reflect.set(WorksheetPersistence.persistence, 'writeLocal', recordLocal);
        WorksheetPersistence.persistence.saveCalendar('datetime');
        WorksheetPersistence.persistence.saveCalendar('almanac');
        equal(stored[0].key, 'mars.exprLab.lastDatetimeState', 'DateTime local key unchanged');
        equal(stored[2].key, 'mars.exprLab.lastAlmanacState', 'almanac local key unchanged');
        for (const [index, mode] of [[0, 'datetime'], [2, 'almanac']] as const) {
            for (const field of calendarSchema(mode)) {
                equal(
                    stored[index + 1].payload[mode + '_' + field.key], stored[index].payload[field.key],
                    'local and server state fields agree');
            }
        }
    } finally {
        Reflect.set(WorksheetLocations.calendarActions, 'triggerDatetimeAutoEvaluation', savedTriggers.datetime);
        Reflect.set(WorksheetLocations.calendarActions, 'triggerAlmanacAutoEvaluation', savedTriggers.almanac);
        Reflect.set(WorksheetPersistence.persistence, 'publish', savedStore);
        Reflect.set(WorksheetPersistence.persistence, 'writeLocal', savedLocalStore);
        WorksheetContext.modeEditorText.datetime = savedEditors.datetime;
        WorksheetContext.modeEditorText.almanac = savedEditors.almanac;
        for (const snapshot of snapshots) {
            const elements = WorksheetLocations.calendar.calendarElements(snapshot.mode) as LabCalendarInputElements;
            for (const [key, value] of Object.entries(snapshot.state)) {
                if (elements[key]) {
                    elements[key].value = value as string;
                } else if (key === 'visibility') {
                    WorksheetContext.worksheetSession.calendar.almanacVisibilityMode = value as string;
                }
            }
        }
        WorksheetContext.worksheetSession.calendar.datetimeAutoGmtOffset = savedOffset;
        WorksheetContext.worksheetSession.calendar.datetimeGmtOffsetTouched = savedTouched;
        WorksheetWorkspace.mode.setMode(savedMode);
    }
};
