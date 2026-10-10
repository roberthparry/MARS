/** Calendar location projection and asynchronous restoration checks against the real C/WASM bridge. */
window.checkLabLocationDom = async function checkLabLocationDom() {
    const check = (ok, message) => {
        if (!ok)
            throw new Error('Location DOM: ' + message);
    };
    const equal = (actual, expected, message) =>
        check(Object.is(actual, expected), message + ': ' + actual + ' != ' + expected);
    const savedContext = calendarLocationContext();
    const savedControls = ['datetime', 'almanac'].map(mode => [mode, readCalendarControls(mode)]);
    const savedJurisdictions = [datetimeJurisdiction, almanacJurisdiction].map(
        select => ({select, children: Array.from(select.childNodes), disabled: select.disabled}));
    const savedNotices = new Map(Array.from(
        document.querySelectorAll('[data-jurisdiction-notice]'),
        node => [node, {text: node.textContent, classes: node.className}]));
    const savedTowns = townsForJurisdiction, savedRequest = requestLabForms;
    const input = value => {
        const node = document.createElement('input');
        node.value = value;
        return node;
    };
    const towns = [
        {value: 'A|0|0|0', name: 'A <literal>', latitude: '0', longitude: '0', elevation: '0', detail: 'μ & σ'}, {
            value: 'B|1|2|3',
            name: 'B',
            latitude: '1',
            longitude: '2',
            elevation: '3',
            timezone: 'Europe/London',
            default: true
        },
        {value: 'C|4|5|6', name: 'C', latitude: '4', longitude: '5', elevation: '6', default: true}
    ];
    try {
        equal(validDateText(' 2024-02-29 ', 'fallback'), '2024-02-29', 'leap date trimmed');
        equal(validDateText('2023-02-29', 'fallback'), 'fallback', 'impossible date rejected');
        equal(validDateText('0001-01-01', 'fallback'), '0001-01-01', 'lower supported year');
        equal(validDateText('9999-12-31', 'fallback'), '9999-12-31', 'upper supported year');
        equal(validDateText('10000-01-01', 'fallback'), 'fallback', 'extended year rejected');
        equal(validDateText(0, 'fallback'), 'fallback', 'numeric values are boxed, not handles');
        equal(validAlmanacVisibility(' VISIBLE ', 'all'), 'visible', 'visibility normalised');
        equal(validAlmanacVisibility('hidden', 'all'), 'all', 'unknown visibility falls back');
        equal(validDatetimeJurisdiction('invalid', 'GB'), 'GB', 'unknown jurisdiction falls back');
        const catalogue = {
            config: {DEFAULT_DATETIME_JURISDICTION: 'GB-WLS'},
            jurisdictions: new Set(['GB-WLS', 'GB-SCT']),
            towns: {'GB-WLS': [], GB: towns}
        };
        equal(
            labDOM.call('lab_location_towns', catalogue, {value: 'GB-WLS'}), catalogue.towns['GB-WLS'],
            'explicit empty catalogue does not fall through to country');
        equal(
            labDOM.call('lab_location_towns', catalogue, {value: 'GB-SCT'}), towns,
            'missing regional catalogue uses country');
        catalogue.towns.GB = {};
        equal(
            labDOM.call('lab_location_towns', catalogue, {value: 'GB-SCT'}).length, 0,
            'non-array country catalogue becomes an empty list');

        const select = document.createElement('select');
        labDOM.call('lab_location_populate', select, towns, 1);
        equal(select.value, towns[1].value, 'first default wins');
        equal(select.options[0].textContent, 'A <literal>', 'option labels remain literal text');
        equal(select.options[0].dataset.detail, 'μ & σ', 'native detail preserved');
        equal(select.options[0].dataset.latitude, '0', 'string zero coordinate preserved');
        equal(select.options[0].dataset.elevation, '0', 'string zero elevation preserved');
        equal(selectedTownOption(select), select.options[1], 'chosen option uses native selection');
        select.value = towns[2].value;
        labDOM.call('lab_location_populate', select, towns, 0);
        equal(select.value, towns[2].value, 'existing selection survives population');
        labDOM.call('lab_location_populate', select, towns.slice(0, 2), 0);
        equal(select.value, towns[0].value, 'removed previous selection uses first town');
        check(selectTownByCoordinates(select, 0, 0), 'numeric zero coordinates match');
        check(!selectTownByCoordinates(select, '', ''), 'missing coordinates do not match zero');
        check(!selectTownByCoordinates(select, 'bad', 'bad'), 'invalid coordinates do not match zero');
        check(!selectTownByCoordinates(select, Infinity, 0), 'non-finite coordinates rejected');
        check(selectTownByCoordinates(select, '1.0000005', '2'), 'coordinate tolerance preserved');
        const latitude = input('1'), longitude = input('2'), elevation = input('3.005');
        clearTownForCustomCoordinates(select, latitude, longitude, elevation);
        equal(select.value, towns[1].value, 'elevation within tolerance retains town');
        elevation.value = '3.02';
        clearTownForCustomCoordinates(select, latitude, longitude, elevation);
        equal(select.value, '', 'edited elevation clears town');
        equal(selectedTownOption(select), null, 'empty select has no town');
        check(!selectTownByCoordinates(null, 1, 2), 'absent select safely rejected');

        select.value = towns[1].value;
        const context = calendarLocationContext(), zone = input('9');
        const controls = {
            townSelect: select,
            latitudeInput: latitude,
            longitudeInput: longitude,
            elevationInput: elevation,
            zoneInput: zone,
            resetOffsetTouched: true
        };
        labDOM.call('lab_location_apply_town', controls, '0', context);
        equal(elevation.value, '3', 'selected town restores elevation');
        equal(zone.value, '0', 'zero timezone offset applied');
        equal(context.automaticOffset, '0', 'automatic offset recorded');
        equal(context.offsetTouched, false, 'offset ownership is a boolean');
        labDOM.call('lab_location_apply_town', controls, '', context);
        equal(zone.value, '0', 'unavailable timezone preserves current offset');

        const snapshot = labDOM.call('lab_location_snapshot', select);
        check(labDOM.call('lab_location_unchanged', select, snapshot), 'fresh snapshot accepted');
        select.options[0].replaceWith(select.options[0].cloneNode(true));
        check(
            !labDOM.call('lab_location_unchanged', select, snapshot),
            'identical-looking replacement invalidates snapshot');
        check(!labDOM.call('lab_location_choose', select, {value: 'missing'}), 'missing key rejected');
        equal(select.value, towns[1].value, 'failed key lookup preserves selection');
        check(!labDOM.call('lab_location_choose', select, {index: NaN}), 'invalid native match rejected');
        check(!labDOM.call('lab_location_choose', select, {index: -1}), 'negative native match rejected');
        check(labDOM.call('lab_location_choose', select, {index: 0}), 'native match index zero accepted');
        equal(select.value, towns[0].value, 'match index selects its option');

        const owned = document.createElement('select');
        const addListener = owned.addEventListener;
        let changeListeners = 0;
        owned.addEventListener = function(type, ...args) {
            if (type === 'change')
                ++changeListeners;
            return addListener.call(this, type, ...args);
        };
        labDOM.call('lab_location_populate', owned, towns, 0);
        const firstGeneration = labDOM.call('lab_location_snapshot', owned);
        labDOM.call('lab_location_populate', owned, towns, 0);
        delete owned.addEventListener;
        equal(changeListeners, 1, 'repopulation registers only one native change listener');
        const secondGeneration = labDOM.call('lab_location_snapshot', owned);
        check(firstGeneration.generation !== secondGeneration.generation, 'population replaces ownership token');
        // Restore the exact original DOM to isolate generation invalidation from option identity checks.
        owned.replaceChildren(...firstGeneration.options);
        owned.value = firstGeneration.value;
        check(!labDOM.call('lab_location_unchanged', owned, firstGeneration), 'old generation rejects original DOM');
        const beforeChange = labDOM.call('lab_location_snapshot', owned);
        const changeEvent = new Event('change', {cancelable: true});
        owned.dispatchEvent(changeEvent);
        check(!changeEvent.defaultPrevented, 'native ownership listener does not consume change');
        check(!labDOM.call('lab_location_unchanged', owned, beforeChange), 'unchanged value change invalidates token');
        const afterChange = labDOM.call('lab_location_snapshot', owned);
        check(labDOM.call('lab_location_unchanged', owned, afterChange), 'new snapshot survives scoped handle reuse');
        const ignored = labDOM.call('lab_location_dispatch', 99, {target: owned}, owned);
        equal(ignored.calls.length, 0, 'unknown ownership action returns an empty event plan');
        labDOM.call('lab_location_dispatch', 1, {target: select}, owned);
        check(labDOM.call('lab_location_unchanged', owned, afterChange), 'unrelated event target preserves generation');
        owned.options[1].value = 'edited-value';
        check(
            !labDOM.call('lab_location_unchanged', owned, afterChange), 'in-place candidate edit invalidates snapshot');
        const emptySelect = document.createElement('select');
        check(
            !labDOM.call(
                'lab_location_unchanged', emptySelect,
                labDOM.call('lab_location_snapshot', document.createElement('select'))),
            'empty snapshots cannot cross select identities');

        // More than 4096 cumulative handles exercises per-row release and escaped return values.
        const large = Array.from({length: 600}, (_, index) => ({
                                                    value: 'town-' + index,
                                                    name: 'Town ' + index,
                                                    latitude: String(index),
                                                    longitude: '0',
                                                    detail: 'detail'
                                                }));
        labDOM.call('lab_location_populate', select, large, 1);
        equal(select.options.length, large.length, 'large catalogue projects within scoped handle capacity');
        const largeSnapshot = labDOM.call('lab_location_snapshot', select);
        check(
            labDOM.call('lab_location_unchanged', select, largeSnapshot), 'returned nodes survive later bridge calls');
        equal(largeSnapshot.candidates[599], 'town-599', 'returned candidate array survives handle release');
        check(selectTownByCoordinates(select, 599, 0), 'large coordinate search releases temporary handles');

        const restored = calendarState('datetime', {date: '2024-02-29', start: 'bad', latitude: ' 1 '}, 0);
        equal(restored.start, '2024-02-29', 'restore fallback follows resolved date');
        equal(restored.latitude, ' 1 ', 'restoration preserves authored coordinate whitespace');
        const captured = calendarState('datetime', {date: ' 2024-02-29 ', start: 'bad', latitude: ' 1 '}, 1);
        equal(captured.start, ' 2024-02-29 ', 'capture range fallback follows the raw date');
        equal(captured.latitude, '1', 'capture trims coordinates');
        equal(captured.gmt_offset, '', 'capture does not force an automatic offset');
        const beforeInvalid = JSON.stringify(readCalendarControls('datetime'));
        let rejected = false;
        try {
            applyCalendarState('datetime', {date: '2024-02-29'}, 99);
        } catch (_) {
            rejected = true;
        }
        check(rejected, 'invalid state operation rejected');
        equal(JSON.stringify(readCalendarControls('datetime')), beforeInvalid, 'invalid operation cannot project');

        datetimeDate.value = '2024-02-29';
        datetimeGmtOffset.value = '6';
        datetimeAutoGmtOffset = '5';
        datetimeGmtOffsetTouched = true;
        applyCalendarEvaluationFields(
            'datetime', {fields: {date: 'invalid', julian_day_number: ' 42 ', gmt_offset: '2'}});
        equal(datetimeDate.value, '2024-02-29', 'invalid evaluation date keeps current date');
        equal(datetimeYear.value, '2024', 'evaluation year follows effective date');
        equal(datetimeJdn.value, ' 42 ', 'evaluation preserves JDN text');
        equal(datetimeGmtOffset.value, '6', 'evaluation preserves manually edited offset');
        equal(datetimeGmtOffsetTouched, true, 'manual offset ownership remains intact');
        datetimeGmtOffset.value = '5';
        applyCalendarEvaluationFields('datetime', {fields: {date: '2025-01-01', gmt_offset: ' 0 '}});
        equal(datetimeGmtOffset.value, '0', 'evaluation replaces unchanged automatic offset');
        equal(datetimeAutoGmtOffset, '0', 'evaluation records returned automatic offset');
        equal(datetimeGmtOffsetTouched, false, 'evaluation clears automatic offset touch flag');
        equal(datetimeJdn.value, '', 'missing JDN cleared when fields exist');
        datetimeJdn.value = 'kept';
        applyCalendarEvaluationFields('datetime', {});
        equal(datetimeJdn.value, 'kept', 'absent field record leaves controls untouched');
        applyCalendarEvaluationFields(
            'almanac',
            {fields: {zone: ' 0 ', latitude: ' 0 ', longitude: ' -2 ', elevation: ' 7 ', jurisdiction: 'invalid'}});
        equal(almanacZone.value, '0', 'almanac evaluation trims zero zone');
        equal(almanacLatitude.value, '0', 'almanac evaluation retains string zero latitude');
        equal(almanacJurisdiction.value, DEFAULT_DATETIME_JURISDICTION, 'evaluation uses jurisdiction fallback');
        applyCalendarEvaluationFields('almanac', {fields: {zone: 0, latitude: '', elevation: null}});
        equal(almanacZone.value, '0', 'falsey returned fields preserve current values');
        equal(almanacElevation.value, '7', 'missing returned elevation preserves current value');

        const responseContext = calendarLocationContext();
        datetimeLatitude.value = '12';
        datetimeGmtOffset.value = '6';
        responseContext.automaticOffset = '5';
        responseContext.offsetTouched = true;
        labDOM.call(
            'lab_location_response', 5, {latitude: '30', longitude: '40', gmt_offset: '2'}, responseContext, 1, 1);
        equal(datetimeLatitude.value, '12', 'location response preserves coordinates supplied by a selected town');
        equal(datetimeGmtOffset.value, '6', 'location response preserves town offset');
        labDOM.call(
            'lab_location_response', 5, {latitude: '30', longitude: '40', gmt_offset: '2'}, responseContext, 1, 0);
        equal(datetimeLatitude.value, '30', 'location response supplies unclaimed coordinates');
        equal(datetimeGmtOffset.value, '6', 'location response preserves manual offset');
        responseContext.offsetTouched = false;
        labDOM.call('lab_location_response', 5, {}, responseContext, 0, 0);
        equal(datetimeGmtOffset.value, '', 'missing suggested DateTime offset clears automatic value');

        const button = document.createElement('button');
        Object.assign(button.dataset, {
            date: 'invalid',
            time: ' 12:34 ',
            zone: ' 0 ',
            jurisdiction: 'invalid',
            town: ' saved town ',
            latitude: ' 0 ',
            longitude: ' -4 ',
            elevation: ' 20 '
        });
        almanacDate.value = '2024-02-29';
        const totality = labDOM.call('lab_location_totality_prepare', button, calendarLocationContext());
        equal(almanacDate.value, '2024-02-29', 'invalid eclipse date keeps current date');
        equal(almanacTime.value, '12:34', 'eclipse time is projected before restoration');
        equal(almanacZone.value, '0', 'eclipse zone retains zero');
        equal(totality.town, 'saved town', 'eclipse restoration key is trimmed');
        equal(totality.jurisdiction, DEFAULT_DATETIME_JURISDICTION, 'eclipse jurisdiction uses default');
        labDOM.call('lab_location_totality_finish', totality, 0);
        equal(almanacTown.value, '', 'unmatched eclipse location clears town');
        equal(almanacLatitude.value, '0', 'explicit eclipse coordinates are applied');
        equal(almanacElevation.value, '20', 'explicit eclipse elevation is applied');
        labDOM.call('lab_location_totality_finish', {latitude: '', longitude: '', elevation: ''}, 1);
        equal(almanacLatitude.value, '0', 'missing eclipse coordinates preserve current values');

        const summary = datetimeSummaryText({
            date: '2024-01-01',
            jdn: '42',
            start: 'a',
            end: 'b',
            year: '2024',
            jurisdiction: 'GB',
            latitude: '1',
            longitude: '2',
            gmt_offset: ''
        });
        equal(
            summary,
            'MARS datetime observation\nDate: 2024-01-01\nJulian Day Number: 42\nRange: a to b\nYear: 2024' +
                '\nHoliday jurisdiction: GB\nLocation: 1, 2\nGMT offset: local machine offset',
            'DateTime summary unchanged');
        check(
            almanacSummaryText({visibility: 'visible'}).endsWith('Show bodies: visible only'),
            'visibility summary policy');

        // Resolve each delayed request explicitly; no timing assumptions or live requests.
        townsForJurisdiction = () => towns;
        let finish;
        requestLabForms = () => new Promise(resolve => {
            finish = resolve;
        });
        const stale = restoreTownSelection(select, 'GB-WLS', 'old-town', '', '');
        await Promise.resolve();
        check(typeof finish === 'function', 'compatibility request started');
        select.dispatchEvent(new Event('change'));
        finish({match_index: 1});
        equal(await stale, null, 'change event invalidates a delayed compatibility match');
        const replaced = restoreTownSelection(select, 'GB-WLS', 'old-town', '', '');
        await Promise.resolve();
        select.options[0].replaceWith(select.options[0].cloneNode(true));
        finish({match_index: 1});
        equal(await replaced, null, 'option replacement invalidates a delayed compatibility match');
        let current = true;
        const cancelled = restoreTownSelection(select, 'GB-WLS', 'old-town', '', '', () => current);
        await Promise.resolve();
        current = false;
        finish({match_index: 1});
        equal(await cancelled, null, 'external cancellation prevents delayed projection');
        const accepted = restoreTownSelection(select, 'GB-WLS', 'old-town', '', '');
        await Promise.resolve();
        finish({match_index: 1});
        equal(await accepted, true, 'current compatibility result applies');
        equal(select.value, towns[1].value, 'compatible result selects the returned index');
        equal(
            await restoreTownSelection(select, 'GB-WLS', towns[0].value, '', ''), true,
            'exact saved key applies synchronously');
        equal(
            await restoreTownSelection(select, 'GB-WLS', '', '4', '5'), true,
            'coordinates restore without a saved key');
        equal(select.value, towns[2].value, 'coordinate restoration chooses matching option');
        equal(
            await restoreTownSelection(select, 'GB-WLS', '', '', ''), false,
            'missing key and coordinates clear selection');
        equal(select.value, '', 'unmatched restoration leaves no named town');

        let requested;
        requestLabForms = request => {
            requested = request;
            return Promise.resolve({match_index: -1});
        };
        equal(
            await restoreTownSelection(select, 'GB-WLS', '  legacy town  ', '4', '5'), true,
            'failed compatibility match falls back to authored coordinates');
        equal(requested.action, 'town', 'C chooses the server parsing action');
        equal(requested.value, 'legacy town', 'C trims the opaque restoration key');
        equal(
            requested.candidates.join(','), towns.map(town => town.value).join(','),
            'C supplies candidates in snapshotted order');
        equal(select.value, towns[2].value, 'coordinate fallback follows a failed saved-key match');
        for (const match_index of [NaN, Infinity, -1, 0.5, 99, '0', null]) {
            requestLabForms = () => Promise.resolve({match_index});
            equal(
                await restoreTownSelection(select, 'GB-WLS', 'old-town', '', ''), false,
                'invalid server match clears only after coordinate fallback: ' + match_index);
            equal(select.value, '', 'invalid server match leaves no named town');
        }
        requestLabForms = () => {
            throw new Error('Unexpected compatibility request');
        };
        equal(
            await restoreTownSelection(select, 'GB-WLS', '  ' + towns[0].value + '  ', '4', '5'), true,
            'trimmed exact match bypasses the server and takes precedence over coordinates');
        equal(select.value, towns[0].value, 'exact key wins over a different coordinate match');
        equal(await restoreTownSelection(null, 'GB-WLS', 'old-town', '', ''), false, 'absent select is unmatched');
        equal(
            await restoreTownSelection(select, 'GB-WLS', 'old-town', '', '', () => false), null,
            'already cancelled restoration does not populate or request');
        equal(select.value, towns[0].value, 'cancelled restoration preserves the selection');

        const pending = [];
        requestLabForms = () => new Promise(resolve => pending.push(resolve));
        const older = restoreTownSelection(select, 'GB-WLS', 'old-town', '', '');
        await Promise.resolve();
        const newer = restoreTownSelection(select, 'GB-WLS', 'new-town', '', '');
        await Promise.resolve();
        equal(pending.length, 2, 'overlapping restorations both reached the server');
        pending[1]({match_index: 2});
        equal(await newer, true, 'newer restoration owns the selection');
        pending[0]({match_index: 0});
        equal(await older, null, 'late older response loses its generation');
        equal(select.value, towns[2].value, 'late response cannot replace newer selection');

        for (const townSelect of [datetimeTown, almanacTown]) {
            const children = Array.from(townSelect.childNodes), selectedIndex = townSelect.selectedIndex;
            try {
                labDOM.call('lab_location_populate', townSelect, towns, 0);
                const townSnapshot = labDOM.call('lab_location_snapshot', townSelect);
                const exact = labDOM.call(
                    'lab_location_restore', 0, townSelect, townSnapshot,
                    {value: towns[0].value, latitude: '4', longitude: '5'}, null);
                equal(exact.restored, true, 'native exact restoration returns a boolean result');
                equal(exact.controls.townSelect, townSelect, 'C resolves the matched calendar controls');
                equal(
                    exact.controls.resetOffsetTouched, townSelect === datetimeTown,
                    'C preserves each calendar mode offset ownership policy');
                const fallback = labDOM.call(
                    'lab_location_restore', 0, townSelect, townSnapshot, {value: '', latitude: '4', longitude: '5'},
                    null);
                equal(fallback.restored, true, 'native coordinate restoration succeeds');
                equal(fallback.controls, undefined, 'coordinate fallback does not overwrite authored controls');
            } finally {
                townSelect.replaceChildren(...children);
                townSelect.selectedIndex = selectedIndex;
                syncRoundedSelect(townSelect);
            }
        }

        const previousOptions = datetimeJurisdiction.innerHTML;
        equal(labDOM.call('lab_location_catalogue', {options: {}, towns: {}}), 0, 'non-array catalogue rejected');
        equal(labDOM.call('lab_location_catalogue', {options: [], towns: null}), 0, 'missing town catalogue rejected');
        equal(
            labDOM.call('lab_location_catalogue', {options: [['GB', 'United Kingdom'], null], towns: {}}), 0,
            'malformed jurisdiction row rejected before projection');
        equal(datetimeJurisdiction.innerHTML, previousOptions, 'invalid catalogue leaves existing menus untouched');
        const bootstrapCatalogue = {
            available: true,
            options: [['GB', 'United <Kingdom>'], ['IE', 'Ireland']],
            towns: {}
        };
        equal(labDOM.call('lab_location_catalogue', bootstrapCatalogue), 1, 'valid bootstrap catalogue accepted');
        for (const jurisdiction of [datetimeJurisdiction, almanacJurisdiction]) {
            equal(jurisdiction.options.length, 2, 'bootstrap options projected to both modes');
            equal(jurisdiction.options[0].textContent, 'United <Kingdom>', 'jurisdiction labels remain plain text');
            equal(jurisdiction.value, 'GB', 'first jurisdiction selected by native select');
            equal(jurisdiction.disabled, false, 'available catalogue enables jurisdiction');
        }
        bootstrapCatalogue.available = false;
        bootstrapCatalogue.error = 'Fixture <unavailable>';
        equal(
            labDOM.call('lab_location_catalogue', bootstrapCatalogue), 1, 'unavailable database still permits startup');
        for (const jurisdiction of [datetimeJurisdiction, almanacJurisdiction]) {
            equal(jurisdiction.disabled, true, 'unavailable database disables jurisdiction');
            const panel = jurisdiction.closest('.mode-panel');
            const notice = panel.querySelector('[data-jurisdiction-notice]');
            equal(panel.firstElementChild, notice, 'unavailable notice prepended to its panel');
            equal(notice.textContent, 'Fixture <unavailable>', 'database error displayed as plain text');
            check(!notice.classList.contains('hidden'), 'unavailable notice visible');
        }
        delete bootstrapCatalogue.error;
        labDOM.call('lab_location_catalogue', bootstrapCatalogue);
        equal(
            datetimeJurisdiction.closest('.mode-panel').querySelector('[data-jurisdiction-notice]').textContent,
            'Jurisdiction database unavailable', 'missing database error uses native fallback');
        bootstrapCatalogue.available = true;
        labDOM.call('lab_location_catalogue', bootstrapCatalogue);
        const recoveredNotice = datetimeJurisdiction.closest('.mode-panel').querySelector('[data-jurisdiction-notice]');
        check(recoveredNotice.classList.contains('hidden'), 'recovered catalogue hides the old notice');
    } finally {
        townsForJurisdiction = savedTowns;
        requestLabForms = savedRequest;
        for (const {select, children, disabled} of savedJurisdictions) {
            select.replaceChildren(...children);
            select.disabled = disabled;
        }
        for (const notice of document.querySelectorAll('[data-jurisdiction-notice]')) {
            const saved = savedNotices.get(notice);
            if (saved) {
                notice.textContent = saved.text;
                notice.className = saved.classes;
            } else {
                notice.remove();
            }
        }
        for (const [mode, values] of savedControls) {
            const elements = calendarElements(mode);
            for (const [key, value] of Object.entries(values))
                if (elements[key])
                    elements[key].value = value;
            syncRoundedSelect(elements.jurisdiction);
            syncRoundedSelect(elements.town);
        }
        acceptCalendarLocationContext(savedContext);
    }
};
