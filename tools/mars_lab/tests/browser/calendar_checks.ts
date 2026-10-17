/**
 * @file calendar_checks.ts
 * @brief Calendar schema, capture, restoration and event-policy regressions.
 *
 * Uses native calendar metadata and browser control fixtures to verify state handling and persistence without reinterpreting calendar policy.
 */

/** Calendar schema, restoration, capture and event-policy checks against real C/WASM. */
window.checkLabCalendarState = async function checkLabCalendarState() {
    const native = labWire.exports();
    const check: LabTestCheck = (ok, message) => {
        if (!ok)
            throw new Error('Calendar state: ' + message);
    };
    const equal = (actual: unknown, expected: unknown, message: string) =>
        check(Object.is(actual, expected), message + ': ' + actual + ' != ' + expected);
    const savedMode = WorksheetWorkspace.mode.currentMode(), savedOffset = WorksheetContext.worksheetSession.calendar.datetimeAutoGmtOffset, savedTouched = WorksheetContext.worksheetSession.calendar.datetimeGmtOffsetTouched;
    const snapshots = (['datetime', 'almanac'] as const).map(mode => [mode, WorksheetLocations.calendar.readCalendarControls(mode)] as const);
    const savedTriggers = [WorksheetLocations.calendarActions.triggerDatetimeAutoEvaluation, WorksheetLocations.calendarActions.triggerAlmanacAutoEvaluation];
    const savedStore = WorksheetPersistence.persistence.publish, savedLocalStore = WorksheetPersistence.persistence.writeLocal;
    const savedEditors = [WorksheetContext.modeEditorText.datetime, WorksheetContext.modeEditorText.almanac];
    const events: [string, WorksheetAutoEvaluationOptions][] = [];
    const stored: ([string, Record<string, unknown>] | Record<string, unknown>)[] = [];
    try {
        const retired = await labHttp.fetch('/js/almanac.js');
        check(
            retired.status === 404 && !labApplication.definitionScripts.includes('almanac'),
            'retired almanac script remains served or loaded');
        equal(native.lab_browser_abi_version(), 37, 'current browser ABI is required');
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
        (['datetime', 'almanac'] as const).forEach((mode, index) => {
            equal(calendarSchema(mode).map(field => field.key).join(','), expectedKeys[index], 'native schema order');
            check(
                calendarSchema(mode).every((field: {key: string; element: string}) => !field.element || document.getElementById(field.element)!),
                'schema refers to missing controls');
        });
        for (let flags = 0; flags < 32; ++flags) {
            const [town, touched, present, automatic, suggested] = [1, 2, 4, 8, 16].map(bit => !!(flags & bit));
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
        const prefixed = Object.fromEntries(Object.entries(raw).map(([key, value]) => ['datetime_' + key, value]));
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
        const previousTown = WorksheetContext.datetimeTown.value, previousAlmanacJurisdiction = WorksheetContext.almanacJurisdiction.value;
        WorksheetLocations.calendar.applyCalendarState('datetime', {}, 3);
        equal(WorksheetContext.datetimeDate.value, WorksheetContext.DEFAULT_DATETIME_DATE, 'clear resets date');
        equal(WorksheetContext.datetimeJdn.value, '', 'clear resets JDN');
        equal(WorksheetContext.datetimeTown.value, previousTown, 'clear preserves selected town');
        check(!WorksheetContext.worksheetSession.calendar.datetimeGmtOffsetTouched, 'clear resets offset ownership');
        WorksheetLocations.calendar.applyCalendarState('almanac', {}, 3);
        equal(WorksheetContext.almanacJurisdiction.value, previousAlmanacJurisdiction, 'almanac clear preserves jurisdiction');
        equal(WorksheetContext.worksheetSession.calendar.almanacVisibilityMode, WorksheetContext.DEFAULT_ALMANAC_VISIBILITY, 'clear resets visibility');

        Reflect.set(WorksheetLocations.calendarActions, 'triggerDatetimeAutoEvaluation', (options: WorksheetAutoEvaluationOptions) => events.push(['datetime', options]));
        Reflect.set(WorksheetLocations.calendarActions, 'triggerAlmanacAutoEvaluation', (options: Record<string, unknown>) => events.push(['almanac', options]));
        for (const mode of ['datetime', 'almanac'] as const) {
            WorksheetWorkspace.mode.setMode(mode);
            const elements = WorksheetLocations.calendar.calendarElements(mode) as Record<string, HTMLInputElement>;
            elements.date.value = '2024-02-29';
            elements.date.dispatchEvent(new Event('change'));
            equal(events.at(-1)![0], mode, 'date dispatch uses owning mode');
            check(
                events.at(-1)![1].refreshJurisdiction && !events.at(-1)![1].refreshCoordinates,
                'date refresh preserves coordinates');
            elements.jurisdiction.dispatchEvent(new Event('change'));
            check(events.at(-1)![1].refreshCoordinates, 'jurisdiction change refreshes coordinates');
            if (mode === 'datetime') {
                equal(elements.year.value, '2024', 'date change updates year');
                equal(elements.jdn.value, '', 'date change clears JDN');
                elements.start.dispatchEvent(new Event('change'));
                check(!events.at(-1)![1].refreshJurisdiction, 'range change does not refresh jurisdiction');
            }
        }
        Reflect.set(WorksheetPersistence.persistence, 'publish', (payload: Record<string, unknown>) => stored.push(payload));
        Reflect.set(WorksheetPersistence.persistence, 'writeLocal', (key: string, payload: Record<string, unknown>) => stored.push([key, payload]));
        WorksheetPersistence.persistence.saveCalendar('datetime');
        WorksheetPersistence.persistence.saveCalendar('almanac');
        equal((stored[0] as [string, Record<string, unknown>])[0], 'mars.exprLab.lastDatetimeState', 'DateTime local key unchanged');
        equal((stored[2] as [string, Record<string, unknown>])[0], 'mars.exprLab.lastAlmanacState', 'almanac local key unchanged');
        for (const [index, mode] of [[0, 'datetime'], [2, 'almanac']] as const)
            for (const field of calendarSchema(mode))
                equal(
                    (stored[index + 1] as Record<string, unknown>)[mode + '_' + field.key],
                    (stored[index] as [string, Record<string, unknown>])[1][field.key],
                    'local and server state fields agree');
    } finally {
        Reflect.set(WorksheetLocations.calendarActions, 'triggerDatetimeAutoEvaluation', savedTriggers[0]);
        Reflect.set(WorksheetLocations.calendarActions, 'triggerAlmanacAutoEvaluation', savedTriggers[1]);
        Reflect.set(WorksheetPersistence.persistence, 'publish', savedStore);
        Reflect.set(WorksheetPersistence.persistence, 'writeLocal', savedLocalStore);
        [WorksheetContext.modeEditorText.datetime, WorksheetContext.modeEditorText.almanac] = savedEditors;
        for (const [mode, snapshot] of snapshots) {
            const elements = WorksheetLocations.calendar.calendarElements(mode) as Record<string, HTMLInputElement>;
            for (const [key, value] of Object.entries(snapshot))
                if (elements[key])
                    elements[key].value = value as string;
                else if (key === 'visibility')
                    WorksheetContext.worksheetSession.calendar.almanacVisibilityMode = value as string;
        }
        WorksheetContext.worksheetSession.calendar.datetimeAutoGmtOffset = savedOffset;
        WorksheetContext.worksheetSession.calendar.datetimeGmtOffsetTouched = savedTouched;
        WorksheetWorkspace.mode.setMode(savedMode);
    }
};
