/** Calendar schema, restoration, capture and event-policy checks against real C/WASM. */
window.checkLabCalendarState = async function checkLabCalendarState() {
    const native = labWire.exports();
    const check = (ok, message) => {
        if (!ok)
            throw new Error('Calendar state: ' + message);
    };
    const equal = (actual, expected, message) =>
        check(Object.is(actual, expected), message + ': ' + actual + ' != ' + expected);
    const savedMode = currentMode(), savedOffset = datetimeAutoGmtOffset, savedTouched = datetimeGmtOffsetTouched;
    const snapshots = ['datetime', 'almanac'].map(mode => [mode, readCalendarControls(mode)]);
    const savedTriggers = [triggerDatetimeAutoEvaluation, triggerAlmanacAutoEvaluation];
    const savedStore = saveLabState, savedLocalStore = saveLabLocalState;
    const savedEditors = [modeEditorText.datetime, modeEditorText.almanac];
    const events = [], stored = [];
    try {
        const retired = await labFetch('/js/almanac.js');
        check(
            retired.status === 404 && !labDefinitionScripts.includes('almanac'),
            'retired almanac script remains served or loaded');
        equal(native.lab_browser_abi_version(), 30, 'current browser ABI is required');
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
        ['datetime', 'almanac'].forEach((mode, index) => {
            equal(calendarSchema(mode).map(field => field.key).join(','), expectedKeys[index], 'native schema order');
            check(
                calendarSchema(mode).every(field => !field.element || document.getElementById(field.element)),
                'schema refers to missing controls');
        });
        for (let flags = 0; flags < 32; ++flags) {
            const [town, touched, present, automatic, suggested] = [1, 2, 4, 8, 16].map(bit => !!(flags & bit));
            equal(
                native.lab_profile_accept_offset(5, town, touched, present, automatic, suggested),
                Number(!town && (!touched || !present || automatic)), 'DateTime offset ownership');
            equal(
                native.lab_profile_accept_offset(6, town, touched, present, automatic, suggested),
                Number(!town && suggested), 'almanac offset ownership');
        }
        const raw = {
            date: ' 2024-02-29 ',
            start: 'bad',
            end: '2024-03-02',
            year: ' 2028 ',
            jdn: ' 2460000 ',
            jurisdiction: DEFAULT_DATETIME_JURISDICTION,
            latitude: ' 51 ',
            longitude: ' -2 ',
            elevation: ' 40 ',
            gmt_offset: ' 5.5 ',
            town: ' μ|51|-2|40 '
        };
        const restored = calendarState('datetime', raw, 0);
        equal(restored.date, '2024-02-29', 'restore validates date');
        equal(restored.start, restored.date, 'range falls back to resolved date');
        equal(restored.year, ' 2028 ', 'restoration preserves authored year whitespace');
        equal(restored.latitude, ' 51 ', 'restoration preserves authored coordinate whitespace');
        const prefixed = Object.fromEntries(Object.entries(raw).map(([key, value]) => ['datetime_' + key, value]));
        equal(
            JSON.stringify(calendarState('datetime', prefixed, 0, 'datetime_')), JSON.stringify(restored),
            'server and history restoration share policy');
        const captured = calendarState('datetime', raw, 1);
        equal(captured.jdn, '2460000', 'capture trims JDN');
        equal(captured.latitude, '51', 'capture trims coordinate');
        equal(captured.gmt_offset, '5.5', 'explicit offset retained');
        equal(calendarState('datetime', {}, 1).gmt_offset, '', 'capture has no forced GMT offset');
        equal(calendarState('datetime', {date: '2024-02-29'}, 0).year, '2024', 'year fallback follows date');
        equal(
            calendarState('datetime', {date: '2023-02-29'}, 0).date, DEFAULT_DATETIME_DATE,
            'non-leap date uses configured fallback');
        const almanac = calendarState('almanac', {time: '   ', zone: ' 2 ', visibility: ' VISIBLE '}, 0);
        equal(almanac.time, DEFAULT_ALMANAC_TIME, 'blank restored time uses default');
        equal(almanac.zone, '2', 'restored almanac zone trimmed');
        equal(almanac.visibility, 'visible', 'visibility normalised');
        equal(calendarState('almanac', {time: '   '}, 1).time, '', 'capture retains incomplete authored time');

        applyCalendarState('datetime', raw);
        datetimeGmtOffsetTouched = true;
        equal(currentDatetimeState().gmt_offset, '', 'unchanged automatic offset omitted');
        datetimeGmtOffset.value = '6';
        equal(currentDatetimeState().gmt_offset, '6', 'manually changed offset included');
        datetimeStart.value = '';
        restoreDatetimeDefaultsIfBlank();
        equal(datetimeStart.value, datetimeDate.value, 'blank range control follows date');
        const previousTown = datetimeTown.value, previousAlmanacJurisdiction = almanacJurisdiction.value;
        applyCalendarState('datetime', {}, 3);
        equal(datetimeDate.value, DEFAULT_DATETIME_DATE, 'clear resets date');
        equal(datetimeJdn.value, '', 'clear resets JDN');
        equal(datetimeTown.value, previousTown, 'clear preserves selected town');
        check(!datetimeGmtOffsetTouched, 'clear resets offset ownership');
        applyCalendarState('almanac', {}, 3);
        equal(almanacJurisdiction.value, previousAlmanacJurisdiction, 'almanac clear preserves jurisdiction');
        equal(almanacVisibilityMode, DEFAULT_ALMANAC_VISIBILITY, 'clear resets visibility');

        triggerDatetimeAutoEvaluation = options => events.push(['datetime', options]);
        triggerAlmanacAutoEvaluation = options => events.push(['almanac', options]);
        for (const mode of ['datetime', 'almanac']) {
            setMode(mode);
            const elements = calendarElements(mode);
            elements.date.value = '2024-02-29';
            elements.date.dispatchEvent(new Event('change'));
            equal(events.at(-1)[0], mode, 'date dispatch uses owning mode');
            check(
                events.at(-1)[1].refreshJurisdiction && !events.at(-1)[1].refreshCoordinates,
                'date refresh preserves coordinates');
            elements.jurisdiction.dispatchEvent(new Event('change'));
            check(events.at(-1)[1].refreshCoordinates, 'jurisdiction change refreshes coordinates');
            if (mode === 'datetime') {
                equal(elements.year.value, '2024', 'date change updates year');
                equal(elements.jdn.value, '', 'date change clears JDN');
                elements.start.dispatchEvent(new Event('change'));
                check(!events.at(-1)[1].refreshJurisdiction, 'range change does not refresh jurisdiction');
            }
        }
        saveLabState = payload => stored.push(payload);
        saveLabLocalState = (key, payload) => stored.push([key, payload]);
        saveLastDatetimeState();
        saveLastAlmanacState();
        equal(stored[0][0], 'mars.exprLab.lastDatetimeState', 'DateTime local key unchanged');
        equal(stored[2][0], 'mars.exprLab.lastAlmanacState', 'almanac local key unchanged');
        for (const [index, mode] of [[0, 'datetime'], [2, 'almanac']])
            for (const field of calendarSchema(mode))
                equal(
                    stored[index + 1][mode + '_' + field.key], stored[index][1][field.key],
                    'local and server state fields agree');
    } finally {
        [triggerDatetimeAutoEvaluation, triggerAlmanacAutoEvaluation] = savedTriggers;
        saveLabState = savedStore;
        saveLabLocalState = savedLocalStore;
        [modeEditorText.datetime, modeEditorText.almanac] = savedEditors;
        for (const [mode, snapshot] of snapshots) {
            const elements = calendarElements(mode);
            for (const [key, value] of Object.entries(snapshot))
                if (elements[key])
                    elements[key].value = value;
                else if (key === 'visibility')
                    almanacVisibilityMode = value;
        }
        datetimeAutoGmtOffset = savedOffset;
        datetimeGmtOffsetTouched = savedTouched;
        setMode(savedMode);
    }
};
